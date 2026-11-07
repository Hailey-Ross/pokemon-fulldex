// Cloud saves: which save wins, between two devices sharing one cloud (a folder here; WebDAV
// too when FULLDEX_TEST_WEBDAV names a server). Build:
//   g++ -std=c++17 -Ifrontend tests/test_cloud_saves.cpp frontend/cloud_saves.cpp frontend/achievements_http.cpp -ldl
#include "cloud_saves.h"
#include "net_http.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace CloudSaves;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static std::vector<uint8_t> Save(char fill, size_t n = 524288) { return std::vector<uint8_t>(n, (uint8_t)fill); }
static void Write(const std::string& p, const std::vector<uint8_t>& d)
{
    FILE* f = fopen(p.c_str(), "wb");
    fwrite(d.data(), 1, d.size(), f);
    fclose(f);
}
static std::vector<uint8_t> Read(const std::string& p)
{
    std::vector<uint8_t> d;
    FILE* f = fopen(p.c_str(), "rb");
    if (!f) return d;
    int c;
    while ((c = fgetc(f)) != EOF) d.push_back((uint8_t)c);
    fclose(f);
    return d;
}
static size_t Backups(const std::string& dir)
{
    size_t n = 0;
    std::error_code ec;
    for (fs::directory_iterator it(dir + "/backups", ec), end; !ec && it != end; it.increment(ec)) n++;
    return n;
}

// A backend that can be switched off, as a device out of reach of the network.
struct Flaky : Backend
{
    std::unique_ptr<Backend> real;
    bool* online;
    Flaky(std::unique_ptr<Backend> r, bool* o) : real(std::move(r)), online(o) {}
    const char* Name() const override { return real->Name(); }
    bool Stat(const std::string& n, bool& e, Meta& m, std::string& err) override { if (!*online) { err = "offline"; return false; } return real->Stat(n, e, m, err); }
    bool Get(const std::string& n, std::vector<uint8_t>& d, std::string& err) override { if (!*online) { err = "offline"; return false; } return real->Get(n, d, err); }
    bool Put(const std::string& n, const std::vector<uint8_t>& d, const Meta& m, std::string& err) override { if (!*online) { err = "offline"; return false; } return real->Put(n, d, m, err); }
};

static void Scenario(const char* label, std::unique_ptr<Backend> (*make)(), const std::string& root)
{
    fprintf(stderr, "== %s\n", label);
    fs::remove_all(root);
    const std::string pcDir = root + "/pc", swDir = root + "/switch";
    fs::create_directories(pcDir);
    fs::create_directories(swDir);
    const std::string pcSave = pcDir + "/black2.sav", swSave = swDir + "/black2.sav";
    bool pcOnline = true, swOnline = true;
    Sync pc(std::unique_ptr<Backend>(new Flaky(make(), &pcOnline)), pcDir + "/cloud-state.txt", "Windows PC");
    Sync sw(std::unique_ptr<Backend>(new Flaky(make(), &swOnline)), swDir + "/cloud-state.txt", "Nintendo Switch");

    // 1. Nothing anywhere yet.
    CHECK(pc.BeforePlay(pcSave).outcome == Outcome::NothingYet);
    // 2. The PC saves; it goes up when the game ends.
    Write(pcSave, Save('A'));
    CHECK(pc.AfterPlay(pcSave).outcome == Outcome::Pushed);
    CHECK(pc.AfterPlay(pcSave).outcome == Outcome::InSync);             // nothing new: no upload
    // 3. In bed: the Switch has no save, and gets the PC's.
    Result r = sw.BeforePlay(swSave);
    CHECK(r.outcome == Outcome::Pulled);
    CHECK(r.message.find("WINDOWS PC") != std::string::npos);
    CHECK(Read(swSave) == Save('A'));
    // 4. The Switch plays on; it goes up.
    Write(swSave, Save('B'));
    CHECK(sw.AfterPlay(swSave).outcome == Outcome::Pushed);
    // 5. Back on the PC: its save is the old one, so the Switch's comes down, the old one kept.
    r = pc.BeforePlay(pcSave);
    CHECK(r.outcome == Outcome::Pulled);
    CHECK(Read(pcSave) == Save('B'));
    CHECK(!r.backup.empty() && Read(r.backup) == Save('A'));
    CHECK(Backups(pcDir) == 1);
    // 6. The Switch away from Wi-Fi: its save cannot go up, and goes up next time.
    Write(swSave, Save('C'));
    swOnline = false;
    CHECK(sw.AfterPlay(swSave).outcome == Outcome::Failed);
    CHECK(sw.BeforePlay(swSave).outcome == Outcome::Failed);             // plays its own save
    CHECK(Read(swSave) == Save('C'));
    swOnline = true;
    CHECK(sw.BeforePlay(swSave).outcome == Outcome::Pushed);
    CHECK(pc.BeforePlay(pcSave).outcome == Outcome::Pulled);
    CHECK(Read(pcSave) == Save('C'));
    // 7. Both play on without syncing between: neither is lost, and the player is asked.
    Write(pcSave, Save('D'));
    Write(swSave, Save('E'));
    CHECK(pc.AfterPlay(pcSave).outcome == Outcome::Pushed);
    CHECK(sw.AfterPlay(swSave).outcome == Outcome::Conflict);            // nothing goes up over the PC's
    r = sw.BeforePlay(swSave);
    CHECK(r.outcome == Outcome::Conflict);
    CHECK(r.cloud.device == "Windows PC" && r.cloud.hash == Hash(Save('D')));
    CHECK(r.local.hash == Hash(Save('E')));
    CHECK(Read(swSave) == Save('E'));                                     // unchanged until chosen
    // 7a. Keep the Switch's: it goes up, the PC's is kept in the Switch's backups.
    const size_t before = Backups(swDir);
    r = sw.Resolve(swSave, false);
    CHECK(r.outcome == Outcome::Pushed);
    CHECK(!r.backup.empty() && Read(r.backup) == Save('D'));
    CHECK(Backups(swDir) == before + 1);
    CHECK(pc.BeforePlay(pcSave).outcome == Outcome::Pulled);
    CHECK(Read(pcSave) == Save('E'));
    // 7b. Again, keeping the cloud's this time.
    Write(pcSave, Save('F'));
    Write(swSave, Save('G'));
    CHECK(pc.AfterPlay(pcSave).outcome == Outcome::Pushed);
    CHECK(sw.BeforePlay(swSave).outcome == Outcome::Conflict);
    r = sw.Resolve(swSave, true);
    CHECK(r.outcome == Outcome::Pulled);
    CHECK(Read(swSave) == Save('F'));
    CHECK(!r.backup.empty() && Read(r.backup) == Save('G'));
    CHECK(sw.AfterPlay(swSave).outcome == Outcome::InSync);
    // 8. A device that synced already but lost its memory of it (state file gone): same saves
    //    match; different ones ask.
    fs::remove(swDir + "/cloud-state.txt");
    {
        Sync fresh(std::unique_ptr<Backend>(new Flaky(make(), &swOnline)), swDir + "/cloud-state.txt", "Nintendo Switch");
        CHECK(fresh.BeforePlay(swSave).outcome == Outcome::InSync);
        Write(swSave, Save('H'));
        fs::remove(swDir + "/cloud-state.txt");
    }
    {
        Sync fresh(std::unique_ptr<Backend>(new Flaky(make(), &swOnline)), swDir + "/cloud-state.txt", "Nintendo Switch");
        CHECK(fresh.BeforePlay(swSave).outcome == Outcome::Conflict);
    }
    // 9. A second game is its own save.
    Write(pcDir + "/heartgold.sav", Save('h', 262144));
    CHECK(pc.AfterPlay(pcDir + "/heartgold.sav").outcome == Outcome::Pushed);
    CHECK(sw.BeforePlay(swDir + "/heartgold.sav").outcome == Outcome::Pulled);
    CHECK(Read(swDir + "/heartgold.sav") == Save('h', 262144));
}

static std::string g_folder;
static std::unique_ptr<Backend> MakeFolder() { return FolderBackend(g_folder); }
#ifdef FULLDEX_TEST_DRIVE
static std::string g_refresh;
static GoogleClient TestClient() { GoogleClient c; c.id = "test-client.apps.googleusercontent.com"; c.secret = "test-secret"; return c; }
static std::unique_ptr<Backend> MakeDrive() { return GoogleDriveBackend(TestClient(), g_refresh); }
#endif
static std::string g_dav;
static std::unique_ptr<Backend> MakeDav() { return WebDavBackend(g_dav, "fulldex", "test-password"); }

int main()
{
    const std::string root = (fs::temp_directory_path() / "fulldex-cloud-test").string();

    // Pieces
    {
        Meta m;
        m.hash = Hash(Save('A'));
        m.device = "James's \"PC\"\tnew";
        m.savedAt = 1793836540;
        m.size = 524288;
        Meta back;
        CHECK(MetaFromJson(MetaToJson(m), back));
        CHECK(back.hash == m.hash && back.device == m.device && back.savedAt == m.savedAt && back.size == m.size);
        CHECK(Hash(Save('A')) != Hash(Save('B')));
        CHECK(Hash(Save('A', 10)) != Hash(Save('A', 11)));
        Settings s;
        s.provider = "webdav";
        s.webdavUrl = "https://example.com/dav/Fulldex/";
        s.webdavUser = "me";
        s.webdavPassword = "p=ss word";
        s.deviceName = "Bedroom Switch";
        fs::create_directories(root);
        CHECK(SaveSettings(root + "/cloud.ini", s));
        Settings t = LoadSettings(root + "/cloud.ini");
        CHECK(t.provider == "webdav" && t.webdavUrl == s.webdavUrl && t.webdavPassword == s.webdavPassword && t.deviceName == s.deviceName);
        CHECK(LoadSettings(root + "/missing.ini").provider == "off");
        CHECK(FromSettings(LoadSettings(root + "/missing.ini"), root + "/state.txt", "PC") == nullptr);
    }

    g_folder = root + "-cloud";
    fs::remove_all(g_folder);
    {
        // (A sync folder that is not there is an error, not an empty cloud: a typo, or a drive
        // not plugged in, must not look like "no saves yet".)
        bool exists;
        Meta m;
        std::string error;
        CHECK(!FolderBackend(g_folder)->Stat("black2.sav", exists, m, error));
    }
    fs::create_directories(g_folder);
    Scenario("folder", MakeFolder, root + "-folder");

    if (const char* dav = getenv("FULLDEX_TEST_WEBDAV"))
    {
        g_dav = dav;
        Scenario("webdav", MakeDav, root + "-webdav");
        // A wrong password says so.
        bool exists;
        Meta m;
        std::string error;
        CHECK(!WebDavBackend(g_dav, "fulldex", "wrong")->Stat("black2.sav", exists, m, error));
        CHECK(error.find("PASSWORD") != std::string::npos);
    }

#ifdef FULLDEX_TEST_DRIVE
    {
        // Signing in: the code to type, then waiting, then signed in.
        SignIn in;
        std::string error;
        CHECK(GoogleSignInStart(TestClient(), in, error));
        CHECK(in.userCode == "ABCD-EFGH" && in.verificationUrl == "https://www.google.com/device" && in.interval == 5);
        CHECK(GoogleSignInPoll(TestClient(), in, g_refresh, error) == SignInState::Waiting);
        CHECK(GoogleSignInPoll(TestClient(), in, g_refresh, error) == SignInState::Done);
        CHECK(!g_refresh.empty());
        GoogleClient none;
        CHECK(!GoogleSignInStart(none, in, error) && error.find("NO GOOGLE") != std::string::npos);

        Scenario("google drive", MakeDrive, root + "-drive");

        // The hour-long access running out between two calls: renewed, unnoticed.
        auto drive = MakeDrive();
        bool exists = false;
        Meta m;
        CHECK(drive->Stat("black2.sav", exists, m, error) && exists);
        Achievements::HttpCall expire;
        expire.url = FULLDEX_GOOGLE_API "/_expire";
        int status = 0;
        std::string body;
        CHECK(Achievements::HttpRequest(expire, status, body, error) && status == 200);
        CHECK(drive->Stat("black2.sav", exists, m, error) && exists && m.device == "Windows PC");
        // Signed out from the Google account's settings: asks to sign in again.
        CHECK(!GoogleDriveBackend(TestClient(), "revoked")->Stat("black2.sav", exists, m, error));
        CHECK(error.find("SIGN IN AGAIN") != std::string::npos);
    }
#endif

    fprintf(stderr, failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
