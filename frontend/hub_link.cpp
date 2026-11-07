#include "hub_link.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <random>
#include <thread>
#ifdef __SWITCH__
namespace SwitchPlatform { void ExitTrace(const char* step); }
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET Sock;
static const Sock kBad = INVALID_SOCKET;
static void CloseSock(Sock s) { closesocket(s); }
static void NonBlocking(Sock s) { u_long on = 1; ioctlsocket(s, FIONBIO, &on); }
static bool WouldBlock() { const int e = WSAGetLastError(); return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS; }
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int Sock;
static const Sock kBad = -1;
static void CloseSock(Sock s) { close(s); }
static void NonBlocking(Sock s) { fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK); }
static bool WouldBlock() { return errno == EWOULDBLOCK || errno == EAGAIN || errno == EINPROGRESS; }
#endif
#ifdef __SWITCH__
#include <switch.h>
#endif
#include "net_http.h"

namespace HubLink
{
namespace
{
const int kTcpPort = 47260, kUdpPort = 47261;
enum Frame : uint8_t { F_HELLO = 1, F_OFFER, F_NOOFFER, F_CHECK, F_ACCEPT, F_DONE, F_BYE };

std::mutex g_m;
std::atomic<int> g_version{ 0 };
std::thread g_thread;
std::atomic<bool> g_stop{ false };
Status g_st;
std::string g_me, g_game;
Room g_joinTo;
bool g_wantJoin = false;
std::vector<std::vector<uint8_t>> g_out; // (frames to send, from the page)
// (Online: the relay's address, the code, this device's id in the room; a goodbye still to say.)
bool g_online = false;
std::string g_relay, g_code, g_myId;
std::string g_bye;

void Changed() { g_version++; }
// (A short log of the link, for finding faults: hub-link-log.txt, only with SSR_HUB_LOG set.)
void Log(const char* fmt, ...)
{
#ifdef __SWITCH__
    // (On the Switch: with hub-link-log.on beside the launcher.)
    static const bool on = [] { FILE* t = fopen("sdmc:/switch/pokemon-ds-wide/hub-link-log.on", "r"); if (t) fclose(t); return t != nullptr; }();
#else
    static const bool on = getenv("SSR_HUB_LOG") != nullptr;
#endif
    if (!on) return;
#ifdef __SWITCH__
    static FILE* f = fopen("sdmc:/switch/pokemon-ds-wide/hub-link-log.txt", "w");
#else
    static FILE* f = fopen("hub-link-log.txt", "w");
#endif
    if (!f) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fflush(f);
}

// (This device's address on the home network: the one a connection out would use. Nothing is sent.)
std::string LocalAddress()
{
    Sock s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == kBad) return "";
    sockaddr_in a{};
    a.sin_family = AF_INET; a.sin_port = htons(53); a.sin_addr.s_addr = inet_addr("8.8.8.8");
    std::string out;
    if (connect(s, (sockaddr*)&a, sizeof a) == 0)
    {
        sockaddr_in me{};
        socklen_t l = sizeof me;
        if (getsockname(s, (sockaddr*)&me, &l) == 0) out = inet_ntoa(me.sin_addr);
    }
    CloseSock(s);
    return out == "0.0.0.0" ? std::string() : out;
}

void StartNet()
{
#ifdef _WIN32
    static bool once = false;
    if (!once) { WSADATA w; WSAStartup(MAKEWORD(2, 2), &w); once = true; }
#elif defined(__SWITCH__)
    static bool once = false;
    if (!once) { Achievements::EnsureNetwork(); once = true; } // (shared: closed as the program ends)
#endif
}

std::vector<uint8_t> MakeFrame(uint8_t type, const std::vector<uint8_t>& body)
{
    std::vector<uint8_t> f;
    const uint32_t n = (uint32_t)body.size() + 1;
    for (int i = 0; i < 4; i++) f.push_back((uint8_t)(n >> (8 * i)));
    f.push_back(type);
    f.insert(f.end(), body.begin(), body.end());
    return f;
}
void Queue(uint8_t type, const std::vector<uint8_t>& body = {}) { g_out.push_back(MakeFrame(type, body)); }
void PutText(std::vector<uint8_t>& b, const std::string& s) { b.insert(b.end(), s.begin(), s.end()); b.push_back(0); }
std::string GetText(const std::vector<uint8_t>& b, size_t& at)
{
    std::string s;
    while (at < b.size() && b[at]) s += (char)b[at++];
    if (at < b.size()) at++;
    return s;
}

// (Both said yes: what this side should do now. Lock held.)
void CheckFinish()
{
    Status& s = g_st;
    if (s.finish) return;
    const bool trade = s.mine.any && s.theirs.any && !s.mine.gift && !s.theirs.gift;
    const bool theirGift = s.theirs.any && s.theirs.gift && !s.mine.any;
    const bool myGift = s.mine.any && s.mine.gift && !s.theirs.any;
    if ((trade && s.iAccepted && s.theyAccepted) || (theirGift && s.iAccepted) || (myGift && s.theyAccepted && s.theyFinished))
    {
        s.finish = true;
        Changed();
    }
}

void Handle(uint8_t type, const std::vector<uint8_t>& b)
{
    Log("in: frame %d, %d bytes", (int)type, (int)b.size());
    std::lock_guard<std::mutex> l(g_m);
    Status& s = g_st;
    size_t at = 0;
    switch (type)
    {
    case F_HELLO:
        s.peerName = GetText(b, at);
        s.peerGame = GetText(b, at);
        s.note = "Linked with " + s.peerName + ".";
        break;
    case F_OFFER:
    {
        if (b.size() < 6) break;
        Offer o;
        o.any = true;
        o.gift = b[0] != 0;
        o.generation = b[1];
        o.level = b[2];
        o.shiny = b[3] != 0;
        o.speciesId = b[4] | b[5] << 8;
        at = 6;
        o.name = GetText(b, at);
        o.species = GetText(b, at);
        o.game = GetText(b, at);
        o.data.assign(b.begin() + std::min(at, b.size()), b.end());
        s.theirs = o;
        s.iAccepted = s.theyAccepted = false;
        s.theyFinished = false;
        s.note = s.peerName + (o.gift ? " wants to send you " : " offers ") + o.name + ".";
        break;
    }
    case F_NOOFFER:
        s.theirs = Offer();
        s.iAccepted = s.theyAccepted = false;
        break;
    case F_CHECK:
        s.theyCanTake = !b.empty() && b[0];
        at = 1;
        s.theirReason = GetText(b, at);
        break;
    case F_ACCEPT:
        s.theyAccepted = !b.empty() && b[0];
        if (s.theyAccepted) s.note = s.peerName + " said yes.";
        break;
    case F_DONE:
    {
        const bool ok = !b.empty() && b[0];
        at = 1;
        const std::string note = GetText(b, at);
        if (!ok) { s.note = s.peerName + "'s game could not take it: " + note; s.iAccepted = s.theyAccepted = false; break; }
        s.theyFinished = true;
        // (Their side is written. A trade: both offers are done with on each side as it finishes.)
        if (s.theirs.any && !s.mine.any) s.theirs = Offer(); // (they sent a gift we already took)
        break;
    }
    case F_BYE:
        s.state = State::Off;
        s.note = s.peerName + " left.";
        break;
    }
    CheckFinish();
    Changed();
}

void Run()
{
    StartNet();
    const std::string myAddress = LocalAddress();
    { std::lock_guard<std::mutex> l(g_m); g_st.myAddress = myAddress; Changed(); }
    Sock udp = kBad, listener = kBad, conn = kBad;
    std::vector<uint8_t> inbuf;
    auto lastAnnounce = std::chrono::steady_clock::now() - std::chrono::seconds(5);
    std::vector<std::pair<Room, std::chrono::steady_clock::time_point>> seen;
    State was = State::Off;
    auto closeAll = [&]() {
        if (udp != kBad) CloseSock(udp);
        if (listener != kBad) CloseSock(listener);
        if (conn != kBad) CloseSock(conn);
        udp = listener = conn = kBad;
    };
    while (!g_stop)
    {
        State st;
        int code;
        bool wantJoin;
        Room joinTo;
        {
            std::lock_guard<std::mutex> l(g_m);
            st = g_st.state; code = g_st.code; wantJoin = g_wantJoin; joinTo = g_joinTo; g_wantJoin = false;
        }
        if (st != was && st != State::Linked) { closeAll(); seen.clear(); inbuf.clear(); }
        was = st;
        // (A goodbye to the relay, after Leave.)
        std::string bye, relayUrl;
        { std::lock_guard<std::mutex> l(g_m); bye.swap(g_bye); relayUrl = g_relay; }
        if (!bye.empty())
        {
            Achievements::HttpCall c;
            c.method = "POST"; c.url = bye; c.timeout = 5;
            c.headers.push_back("Content-Type: application/octet-stream");
            const auto f = MakeFrame(F_BYE, {});
            c.body.assign(f.begin(), f.end());
            int status = 0; std::string reply, err;
            Achievements::HttpRequest(c, status, reply, err);
        }
        if (st == State::Off) { closeAll(); std::this_thread::sleep_for(std::chrono::milliseconds(100)); continue; }
        // Online: everything through the relay, a little under once a second.
        bool online;
        { std::lock_guard<std::mutex> l(g_m); online = g_online; }
        if (online)
        {
            std::vector<std::vector<uint8_t>> out;
            std::string code, id;
            { std::lock_guard<std::mutex> l(g_m); out.swap(g_out); code = g_code; id = g_myId; }
            Achievements::HttpCall c;
            c.method = "POST";
            c.url = relayUrl + "/v1/" + code + "/" + id;
            c.timeout = 15;
            c.headers.push_back("Content-Type: application/octet-stream");
            for (const auto& f : out) c.body.append(f.begin(), f.end());
            int status = 0;
            std::string reply, err;
            const bool ok = Achievements::HttpRequest(c, status, reply, err);
            if (!ok || status != 200 || reply.empty())
            {
                std::lock_guard<std::mutex> l(g_m);
                if (status == 409) { g_st.state = State::Off; g_online = false; g_st.note = "That code already has two devices on it. Make a new one."; }
                else
                {
                    g_out.insert(g_out.begin(), out.begin(), out.end()); // (sent again next time)
                    g_st.note = "Could not reach the trade relay. Trying again...";
                }
                Changed();
                std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                continue;
            }
            const int peers = (unsigned char)reply[0];
            if (peers < 1 || peers > 2)
            {
                // (Not the relay's answer: a page in the way, or the wrong address.)
                Log("relay: strange reply %d, %d bytes: %.80s", status, (int)reply.size(), reply.c_str());
                std::lock_guard<std::mutex> l(g_m);
                g_out.insert(g_out.begin(), out.begin(), out.end());
                g_st.note = "The trade relay gave an answer it should not (" + std::to_string(status) + "). Trying again...";
                Changed();
                std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                continue;
            }
            inbuf.insert(inbuf.end(), reply.begin() + 1, reply.end());
            Log("relay: %d in the room, %d bytes", peers, (int)reply.size() - 1);
            {
                std::lock_guard<std::mutex> l(g_m);
                if (peers >= 2 && g_st.state == State::Online) { g_st.state = State::Linked; g_st.note = "Linked through the relay."; Changed(); }
            }
            while (inbuf.size() >= 5)
            {
                const uint32_t len = inbuf[0] | inbuf[1] << 8 | inbuf[2] << 16 | (uint32_t)inbuf[3] << 24;
                if (len == 0 || len > 65536) { inbuf.clear(); break; }
                if (inbuf.size() < 4 + (size_t)len) break;
                const uint8_t type = inbuf[4];
                std::vector<uint8_t> body(inbuf.begin() + 5, inbuf.begin() + 4 + len);
                inbuf.erase(inbuf.begin(), inbuf.begin() + 4 + len);
                Handle(type, body);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(700));
            continue;
        }
        // Hosting: say so on the network; wait for the other to come.
        if (st == State::Hosting)
        {
            if (udp == kBad)
            {
                udp = socket(AF_INET, SOCK_DGRAM, 0);
                int on = 1;
                setsockopt(udp, SOL_SOCKET, SO_BROADCAST, (const char*)&on, sizeof on);
            }
            if (listener == kBad)
            {
                listener = socket(AF_INET, SOCK_STREAM, 0);
                int on = 1;
                setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof on);
                sockaddr_in a{};
                a.sin_family = AF_INET; a.sin_port = htons(kTcpPort); a.sin_addr.s_addr = htonl(INADDR_ANY);
                if (bind(listener, (sockaddr*)&a, sizeof a) != 0 || listen(listener, 1) != 0)
                {
                    CloseSock(listener); listener = kBad;
                    std::lock_guard<std::mutex> l(g_m);
                    g_st.state = State::Off; g_st.note = "Could not open a room on this network."; Changed();
                    continue;
                }
                NonBlocking(listener);
            }
            if (std::chrono::steady_clock::now() - lastAnnounce > std::chrono::seconds(1))
            {
                lastAnnounce = std::chrono::steady_clock::now();
                const std::string msg = "FDX1|" + std::to_string(code) + "|" + g_me + "|" + g_game;
                // (To every device on the network, and to this network's own broadcast address, a.b.c.255:
                // some systems send the first through the wrong adapter, or not at all.)
                sockaddr_in a{};
                a.sin_family = AF_INET; a.sin_port = htons(kUdpPort); a.sin_addr.s_addr = htonl(INADDR_BROADCAST);
                sendto(udp, msg.c_str(), (int)msg.size(), 0, (sockaddr*)&a, sizeof a);
                if (!myAddress.empty())
                {
                    const std::string net = myAddress.substr(0, myAddress.find_last_of('.')) + ".255";
                    a.sin_addr.s_addr = inet_addr(net.c_str());
                    sendto(udp, msg.c_str(), (int)msg.size(), 0, (sockaddr*)&a, sizeof a);
                }
            }
            const Sock c = accept(listener, nullptr, nullptr);
            if (c != kBad)
            {
                conn = c;
                NonBlocking(conn);
                CloseSock(listener); listener = kBad;
                if (udp != kBad) { CloseSock(udp); udp = kBad; }
                std::lock_guard<std::mutex> l(g_m);
                g_st.state = State::Linked;
                std::vector<uint8_t> b; PutText(b, g_me); PutText(b, g_game);
                Queue(F_HELLO, b);
                Changed();
                was = State::Linked;
            }
        }
        // Browsing: the rooms on the network; joining one.
        if (st == State::Browsing)
        {
            if (udp == kBad)
            {
                udp = socket(AF_INET, SOCK_DGRAM, 0);
                int on = 1;
                setsockopt(udp, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof on);
                sockaddr_in a{};
                a.sin_family = AF_INET; a.sin_port = htons(kUdpPort); a.sin_addr.s_addr = htonl(INADDR_ANY);
                bind(udp, (sockaddr*)&a, sizeof a);
                NonBlocking(udp);
            }
            char buf[512];
            sockaddr_in from{};
            socklen_t fl = sizeof from;
            int n;
            bool changed = false;
            while ((n = (int)recvfrom(udp, buf, sizeof buf - 1, 0, (sockaddr*)&from, &fl)) > 0)
            {
                buf[n] = 0;
                std::string m = buf;
                if (m.rfind("FDX1|", 0) != 0) continue;
                Room r;
                size_t p1 = m.find('|', 5), p2 = p1 == std::string::npos ? p1 : m.find('|', p1 + 1);
                if (p2 == std::string::npos) continue;
                r.code = atoi(m.substr(5, p1 - 5).c_str());
                r.name = m.substr(p1 + 1, p2 - p1 - 1);
                r.game = m.substr(p2 + 1);
                r.address = inet_ntoa(from.sin_addr);
                bool found = false;
                for (auto& s : seen) if (s.first.address == r.address && s.first.code == r.code) { s.second = std::chrono::steady_clock::now(); found = true; }
                if (!found) { seen.push_back({ r, std::chrono::steady_clock::now() }); changed = true; }
            }
            const auto now = std::chrono::steady_clock::now();
            const size_t before = seen.size();
            seen.erase(std::remove_if(seen.begin(), seen.end(), [&](const auto& s) { return now - s.second > std::chrono::seconds(4); }), seen.end());
            if (changed || seen.size() != before)
            {
                std::lock_guard<std::mutex> l(g_m);
                g_st.rooms.clear();
                for (auto& s : seen) g_st.rooms.push_back(s.first);
                Changed();
            }
            if (wantJoin)
            {
                Sock c = socket(AF_INET, SOCK_STREAM, 0);
                sockaddr_in a{};
                a.sin_family = AF_INET; a.sin_port = htons(kTcpPort);
                a.sin_addr.s_addr = inet_addr(joinTo.address.c_str());
                NonBlocking(c);
                connect(c, (sockaddr*)&a, sizeof a);
                fd_set w; FD_ZERO(&w); FD_SET(c, &w);
                timeval tv{ 4, 0 };
                int err = 0; socklen_t el = sizeof err;
                const bool ok = select((int)c + 1, nullptr, &w, nullptr, &tv) > 0 && getsockopt(c, SOL_SOCKET, SO_ERROR, (char*)&err, &el) == 0 && err == 0;
                std::lock_guard<std::mutex> l(g_m);
                if (!ok) { CloseSock(c); g_st.note = "Could not reach " + joinTo.name + "'s room."; Changed(); }
                else
                {
                    if (udp != kBad) { CloseSock(udp); udp = kBad; }
                    conn = c;
                    g_st.state = State::Linked;
                    g_st.code = joinTo.code;
                    std::vector<uint8_t> b; PutText(b, g_me); PutText(b, g_game);
                    Queue(F_HELLO, b);
                    Changed();
                    was = State::Linked;
                }
            }
        }
        // Linked: frames both ways.
        if (st == State::Linked || was == State::Linked)
        {
            if (conn == kBad) { std::lock_guard<std::mutex> l(g_m); g_st.state = State::Off; Changed(); continue; }
            std::vector<std::vector<uint8_t>> out;
            { std::lock_guard<std::mutex> l(g_m); out.swap(g_out); }
            bool dead = false;
            for (const auto& f : out)
            {
                size_t sent = 0;
                const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                while (sent < f.size() && std::chrono::steady_clock::now() < end)
                {
                    const int n = (int)send(conn, (const char*)f.data() + sent, (int)(f.size() - sent), 0);
                    if (n > 0) sent += n;
                    else if (n < 0 && WouldBlock()) std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    else break;
                }
                Log("out: frame %d, %d of %d bytes", f.size() > 4 ? (int)f[4] : -1, (int)sent, (int)f.size());
                if (sent < f.size()) dead = true;
            }
            char buf[4096];
            int n;
            while ((n = (int)recv(conn, buf, sizeof buf, 0)) > 0) { inbuf.insert(inbuf.end(), buf, buf + n); Log("recv %d", n); }
            if (n == 0 || (n < 0 && !WouldBlock())) dead = true;
            while (inbuf.size() >= 5)
            {
                const uint32_t len = inbuf[0] | inbuf[1] << 8 | inbuf[2] << 16 | (uint32_t)inbuf[3] << 24;
                if (len == 0 || len > 65536) { dead = true; break; }
                if (inbuf.size() < 4 + (size_t)len) break;
                const uint8_t type = inbuf[4];
                std::vector<uint8_t> body(inbuf.begin() + 5, inbuf.begin() + 4 + len);
                inbuf.erase(inbuf.begin(), inbuf.begin() + 4 + len);
                Handle(type, body);
            }
            if (dead)
            {
                closeAll();
                std::lock_guard<std::mutex> l(g_m);
                if (g_st.state == State::Linked) { g_st.state = State::Off; g_st.note = "The link was lost."; }
                Changed();
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    if (conn != kBad) { const auto f = MakeFrame(F_BYE, {}); send(conn, (const char*)f.data(), (int)f.size(), 0); }
    closeAll();
}

// (Random numbers seeded from the clock too: std::random_device gives the same ones on every
// start on the Switch, which made the same room code each time.)
static uint32_t Rand()
{
    static std::mt19937 g = [] {
        std::random_device rd;
        std::seed_seq seq{ (uint32_t)rd(), (uint32_t)std::chrono::steady_clock::now().time_since_epoch().count(),
                           (uint32_t)std::chrono::system_clock::now().time_since_epoch().count(),
                           (uint32_t)(std::chrono::system_clock::now().time_since_epoch().count() >> 32) };
        return std::mt19937(seq);
    }();
    return g();
}

void EnsureThread()
{
    if (g_thread.joinable()) return;
    StartNet(); // (first, so the thread is stopped below before the network closes)
    g_stop = false;
    g_thread = std::thread(Run);
#ifdef __SWITCH__
    // (Stopped as the program ends, before the network closes: atexit runs newest first.)
    static bool once = false;
    if (!once) { once = true; atexit([] { ::SwitchPlatform::ExitTrace("hub link: stopping"); g_stop = true; if (g_thread.joinable()) g_thread.join(); ::SwitchPlatform::ExitTrace("hub link: stopped"); }); }
#endif
}
struct Ender { ~Ender() { g_stop = true; if (g_thread.joinable()) g_thread.join(); } } g_ender;

void Reset(State st)
{
    Status keep;
    keep.state = st;
    keep.myAddress = g_st.myAddress;
    keep.onlineCode = st == State::Linked || st == State::Online ? g_st.onlineCode : std::string();
    g_st = keep;
    g_out.clear();
}
}

void Host(const std::string& me, const std::string& game)
{
    {
        std::lock_guard<std::mutex> l(g_m);
        g_me = me; g_game = game;
        g_online = false;
        Reset(State::Hosting);
        g_st.code = 1000 + (int)(Rand() % 9000);
        g_st.note = "Room " + std::to_string(g_st.code) + " is open. On the other device, choose Join A Room.";
        Changed();
    }
    EnsureThread();
}

void Browse(const std::string& me, const std::string& game)
{
    {
        std::lock_guard<std::mutex> l(g_m);
        g_me = me; g_game = game;
        g_online = false;
        Reset(State::Browsing);
        g_st.note = "Looking for rooms on this network...";
        Changed();
    }
    EnsureThread();
}

void Join(const Room& room)
{
    std::lock_guard<std::mutex> l(g_m);
    g_joinTo = room;
    g_wantJoin = true;
    g_st.note = "Joining " + room.name + "'s room...";
    Changed();
}

void Leave()
{
    std::lock_guard<std::mutex> l(g_m);
    if (g_online) g_bye = g_relay + "/v1/" + g_code + "/" + g_myId;
    else if (g_st.state == State::Linked) Queue(F_BYE);
    g_online = false;
    Reset(State::Off);
    Changed();
}

void SetOffer(const Offer& o)
{
    std::lock_guard<std::mutex> l(g_m);
    g_st.mine = o;
    g_st.iAccepted = g_st.theyAccepted = false;
    g_st.theyCanTake = false;
    g_st.theirReason.clear();
    g_st.theyFinished = false;
    if (!o.any) Queue(F_NOOFFER);
    else
    {
        std::vector<uint8_t> b = { (uint8_t)o.gift, (uint8_t)o.generation, (uint8_t)o.level, (uint8_t)o.shiny, (uint8_t)(o.speciesId & 255), (uint8_t)(o.speciesId >> 8) };
        PutText(b, o.name); PutText(b, o.species); PutText(b, o.game);
        b.insert(b.end(), o.data.begin(), o.data.end());
        Queue(F_OFFER, b);
    }
    Changed();
}

void TellCheck(bool ok, const std::string& reason)
{
    std::lock_guard<std::mutex> l(g_m);
    std::vector<uint8_t> b = { (uint8_t)ok };
    PutText(b, reason);
    Queue(F_CHECK, b);
}

void Accept(bool yes)
{
    std::lock_guard<std::mutex> l(g_m);
    g_st.iAccepted = yes;
    Queue(F_ACCEPT, { (uint8_t)yes });
    CheckFinish();
    Changed();
}

void FinishDone(bool ok, const std::string& note)
{
    std::lock_guard<std::mutex> l(g_m);
    std::vector<uint8_t> b = { (uint8_t)ok };
    PutText(b, note);
    Queue(F_DONE, b);
    g_st.finish = false;
    g_st.note = note;
    if (ok) { g_st.mine = Offer(); g_st.theirs = Offer(); }
    g_st.iAccepted = g_st.theyAccepted = false;
    g_st.theyFinished = false;
    Changed();
}

std::string NewCode()
{
    static const char kLetters[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"; // (none that look alike: no I, O, 0, 1)
    std::string c;
    for (int i = 0; i < 6; i++) c += kLetters[Rand() % 32];
    return c;
}

void Online(const std::string& relay, const std::string& code, const std::string& me, const std::string& game)
{
    {
        std::lock_guard<std::mutex> l(g_m);
        g_me = me; g_game = game;
        Reset(State::Online);
        std::string cl;
        for (char ch : code) if (isalnum((unsigned char)ch)) cl += (char)toupper((unsigned char)ch);
        g_code = cl;
        g_relay = relay;
        while (!g_relay.empty() && g_relay.back() == '/') g_relay.pop_back();
        static const char hex[] = "0123456789abcdef";
        g_myId.clear();
        for (int i = 0; i < 16; i++) g_myId += hex[Rand() % 16];
        g_online = true;
        g_st.onlineCode = cl;
        std::vector<uint8_t> b; PutText(b, g_me); PutText(b, g_game);
        Queue(F_HELLO, b);
        g_st.note = "Waiting for the other player to enter " + cl + "...";
        Changed();
    }
    EnsureThread();
}

Status Snapshot() { std::lock_guard<std::mutex> l(g_m); return g_st; }
int Version() { return g_version.load(); }
}
