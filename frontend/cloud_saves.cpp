// Cloud saves (cloud_saves.h): which save wins, and the three places saves can be kept.
#include "cloud_saves.h"
#include "net_http.h"

#include <sys/stat.h>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <map>
#include <mutex>

// (Tests point these at a stand-in server: tests/fake_drive.py.)
#ifndef FULLDEX_GOOGLE_OAUTH
#define FULLDEX_GOOGLE_OAUTH "https://oauth2.googleapis.com"
#endif
#ifndef FULLDEX_GOOGLE_API
#define FULLDEX_GOOGLE_API "https://www.googleapis.com"
#endif
#ifndef FULLDEX_GOOGLE_CLIENT_ID
#define FULLDEX_GOOGLE_CLIENT_ID ""
#endif
#ifndef FULLDEX_GOOGLE_CLIENT_SECRET
#define FULLDEX_GOOGLE_CLIENT_SECRET ""
#endif

namespace CloudSaves
{
namespace fs = std::filesystem;

// ---------------------------------------------------------------------------------------------
// Small helpers: hashing, files, JSON, addresses

std::string Hash(const std::vector<uint8_t>& data)
{
    // FNV-1a over the bytes, with the length: enough to tell two saves apart, and the same on
    // every platform.
    uint64_t h = 1469598103934665603ull;
    for (uint8_t b : data) { h ^= b; h *= 1099511628211ull; }
    char out[64];
    snprintf(out, sizeof out, "fnv64-%016llx-%llu", (unsigned long long)h, (unsigned long long)data.size());
    return out;
}

static bool ReadAll(const std::string& path, std::vector<uint8_t>& data)
{
    data.clear();
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    uint8_t buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) data.insert(data.end(), buf, buf + n);
    const bool ok = !ferror(f);
    fclose(f);
    return ok;
}

static int64_t ModifiedTime(const std::string& path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0 ? (int64_t)st.st_mtime : 0;
}

// Written beside, then put in place, so a save is never left half written.
static bool WriteAtomic(const std::string& path, const std::vector<uint8_t>& data, std::string& error)
{
    const std::string tmp = path + ".cloud-tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) { error = "CANNOT WRITE IN THE SAVE FOLDER"; return false; }
    bool ok = data.empty() || fwrite(data.data(), 1, data.size(), f) == data.size();
    ok = fflush(f) == 0 && ok;
    ok = fclose(f) == 0 && ok;
    if (!ok) { remove(tmp.c_str()); error = "CANNOT FINISH WRITING THE SAVE"; return false; }
#ifdef __SWITCH__
    // (The Switch's file system will not rename onto an existing file: the old one steps aside.)
    const std::string old = path + ".cloud-old";
    remove(old.c_str());
    const bool hadOld = rename(path.c_str(), old.c_str()) == 0;
    if (rename(tmp.c_str(), path.c_str()) != 0)
    {
        if (hadOld) rename(old.c_str(), path.c_str());
        remove(tmp.c_str());
        error = "CANNOT REPLACE THE SAVE";
        return false;
    }
    if (hadOld) remove(old.c_str());
#else
    std::error_code ec;
    fs::rename(fs::u8path(tmp), fs::u8path(path), ec);
    if (ec) { remove(tmp.c_str()); error = "CANNOT REPLACE THE SAVE"; return false; }
#endif
    return true;
}

// Into the backups folder beside the save, named as the game's own backups are
// (black2-20261104-231500.sav), so RESTORE BACKUP lists it.
static std::string BackUp(const std::string& savePath, const std::vector<uint8_t>& data)
{
    if (data.empty()) return std::string();
    const fs::path p = fs::u8path(savePath);
    const fs::path dir = (p.has_parent_path() ? p.parent_path() : fs::path(".")) / "backups";
    std::error_code ec;
    fs::create_directories(dir, ec);
    time_t now = time(nullptr);
    for (int tries = 0; tries < 10; tries++, now++)
    {
        char when[32];
        strftime(when, sizeof when, "%Y%m%d-%H%M%S", localtime(&now));
        const fs::path out = dir / (p.stem().u8string() + "-" + when + ".sav");
        if (fs::exists(out, ec)) continue;
        std::string error;
        if (WriteAtomic(out.u8string(), data, error)) return out.u8string();
        return std::string();
    }
    return std::string();
}

static std::string JsonEscape(const std::string& s)
{
    std::string o;
    for (char c : s)
    {
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if ((unsigned char)c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
        else o += c;
    }
    return o;
}

// The string (or bare number) after "key": in a JSON text. Enough for the few small replies read
// here, whose keys are known.
static bool JsonValue(const std::string& json, const std::string& key, std::string& out)
{
    const std::string k = "\"" + key + "\"";
    size_t at = 0;
    while ((at = json.find(k, at)) != std::string::npos)
    {
        size_t i = at + k.size();
        while (i < json.size() && (json[i] == ' ' || json[i] == '\n' || json[i] == '\r' || json[i] == '\t')) i++;
        if (i >= json.size() || json[i] != ':') { at = i; continue; }
        i++;
        while (i < json.size() && (json[i] == ' ' || json[i] == '\n' || json[i] == '\r' || json[i] == '\t')) i++;
        out.clear();
        if (i < json.size() && json[i] == '"')
        {
            for (i++; i < json.size() && json[i] != '"'; i++)
            {
                if (json[i] == '\\' && i + 1 < json.size())
                {
                    i++;
                    const char e = json[i];
                    if (e == 'n') out += '\n';
                    else if (e == 't') out += '\t';
                    else if (e == 'u' && i + 4 < json.size())
                    {
                        const unsigned v = (unsigned)strtoul(json.substr(i + 1, 4).c_str(), nullptr, 16);
                        if (v < 0x80) out += (char)v;
                        else if (v < 0x800) { out += (char)(0xC0 | (v >> 6)); out += (char)(0x80 | (v & 0x3F)); }
                        else { out += (char)(0xE0 | (v >> 12)); out += (char)(0x80 | ((v >> 6) & 0x3F)); out += (char)(0x80 | (v & 0x3F)); }
                        i += 4;
                    }
                    else out += e;
                }
                else out += json[i];
            }
            return true;
        }
        while (i < json.size() && json[i] != ',' && json[i] != '}' && json[i] != ']' && json[i] != ' ' && json[i] != '\n')
            out += json[i++];
        return !out.empty();
    }
    return false;
}

std::string MetaToJson(const Meta& m)
{
    return "{\"hash\":\"" + JsonEscape(m.hash) + "\",\"device\":\"" + JsonEscape(m.device) + "\",\"savedAt\":\"" +
           std::to_string(m.savedAt) + "\",\"size\":\"" + std::to_string(m.size) + "\"}";
}

bool MetaFromJson(const std::string& json, Meta& m)
{
    m = Meta();
    if (!JsonValue(json, "hash", m.hash) || m.hash.empty()) return false;
    std::string v;
    JsonValue(json, "device", m.device);
    if (JsonValue(json, "savedAt", v)) m.savedAt = strtoll(v.c_str(), nullptr, 10);
    if (JsonValue(json, "size", v)) m.size = strtoull(v.c_str(), nullptr, 10);
    return true;
}

static Meta MetaOf(const std::vector<uint8_t>& data, const std::string& device, int64_t when)
{
    Meta m;
    m.hash = Hash(data);
    m.device = device;
    m.savedAt = when;
    m.size = data.size();
    return m;
}

static std::string UrlEncode(const std::string& s)
{
    std::string o;
    for (unsigned char c : s)
    {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') o += (char)c;
        else { char b[4]; snprintf(b, sizeof b, "%%%02X", c); o += b; }
    }
    return o;
}

static std::string Base64(const std::string& in)
{
    static const char* t = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string o;
    size_t i = 0;
    for (; i + 2 < in.size(); i += 3)
    {
        const uint32_t v = ((uint8_t)in[i] << 16) | ((uint8_t)in[i + 1] << 8) | (uint8_t)in[i + 2];
        o += t[v >> 18]; o += t[(v >> 12) & 63]; o += t[(v >> 6) & 63]; o += t[v & 63];
    }
    if (i + 1 == in.size())
    {
        const uint32_t v = (uint8_t)in[i] << 16;
        o += t[v >> 18]; o += t[(v >> 12) & 63]; o += "==";
    }
    else if (i + 2 == in.size())
    {
        const uint32_t v = ((uint8_t)in[i] << 16) | ((uint8_t)in[i + 1] << 8);
        o += t[v >> 18]; o += t[(v >> 12) & 63]; o += t[(v >> 6) & 63]; o += '=';
    }
    return o;
}

static std::string Bytes(const std::vector<uint8_t>& d) { return std::string(d.begin(), d.end()); }
static std::vector<uint8_t> Bytes(const std::string& s) { return std::vector<uint8_t>(s.begin(), s.end()); }

static std::string StatusError(const char* what, int status, const std::string& body)
{
    std::string e = std::string(what) + " (" + std::to_string(status) + ")";
    std::string msg;
    if (JsonValue(body, "message", msg) || JsonValue(body, "error_description", msg) || JsonValue(body, "error", msg))
        e += ": " + msg;
    return e;
}

// ---------------------------------------------------------------------------------------------
// A folder something else syncs: <folder>/black2.sav, and <folder>/black2.sav.json beside it

class Folder : public Backend
{
public:
    explicit Folder(const std::string& folder) : dir(folder) {}
    const char* Name() const override { return "FOLDER"; }

    bool Stat(const std::string& name, bool& exists, Meta& meta, std::string& error) override
    {
        std::error_code ec;
        if (!fs::is_directory(fs::u8path(dir), ec)) { error = "THE SYNC FOLDER IS MISSING"; return false; }
        std::vector<uint8_t> data, json;
        exists = ReadAll(Path(name), data);
        if (!exists) return true;
        // (A save put in the folder by hand has no .json: it is described from itself.)
        if (!ReadAll(Path(name) + ".json", json) || !MetaFromJson(Bytes(json), meta) || meta.hash != Hash(data))
            meta = MetaOf(data, "", ModifiedTime(Path(name)));
        return true;
    }
    bool Get(const std::string& name, std::vector<uint8_t>& data, std::string& error) override
    {
        if (!ReadAll(Path(name), data)) { error = "CANNOT READ THE SYNC FOLDER"; return false; }
        return true;
    }
    bool Put(const std::string& name, const std::vector<uint8_t>& data, const Meta& meta, std::string& error) override
    {
        std::error_code ec;
        fs::create_directories(fs::u8path(dir), ec);
        return WriteAtomic(Path(name), data, error) && WriteAtomic(Path(name) + ".json", Bytes(MetaToJson(meta)), error);
    }

private:
    std::string Path(const std::string& name) const { return (fs::u8path(dir) / fs::u8path(name)).u8string(); }
    std::string dir;
};

std::unique_ptr<Backend> FolderBackend(const std::string& folder) { return std::unique_ptr<Backend>(new Folder(folder)); }

// ---------------------------------------------------------------------------------------------
// WebDAV: the same two files, by GET and PUT only (Android's HttpURLConnection has no PROPFIND
// or MKCOL), in the folder the address names, which must already exist.

class WebDav : public Backend
{
public:
    WebDav(const std::string& url, const std::string& user, const std::string& password)
        : base(url), auth(user.empty() ? std::string() : "Authorization: Basic " + Base64(user + ":" + password))
    {
        if (!base.empty() && base.back() != '/') base += '/';
    }
    const char* Name() const override { return "WEBDAV"; }

    bool Stat(const std::string& name, bool& exists, Meta& meta, std::string& error) override
    {
        int status = 0;
        std::string body;
        if (!Call("GET", name + ".json", "", status, body, error)) return false;
        if (status == 200 && MetaFromJson(body, meta)) { exists = true; return true; }
        if (status != 404 && status != 200) { error = Problem(status, body); return false; }
        // (No description: a save put there by hand, or none at all.)
        if (!Call("GET", name, "", status, body, error)) return false;
        if (status == 404) { exists = false; return true; }
        if (status != 200) { error = Problem(status, body); return false; }
        exists = true;
        meta = MetaOf(Bytes(body), "", 0);
        return true;
    }
    bool Get(const std::string& name, std::vector<uint8_t>& data, std::string& error) override
    {
        int status = 0;
        std::string body;
        if (!Call("GET", name, "", status, body, error)) return false;
        if (status != 200) { error = Problem(status, body); return false; }
        data = Bytes(body);
        return true;
    }
    bool Put(const std::string& name, const std::vector<uint8_t>& data, const Meta& meta, std::string& error) override
    {
        // (The save first, then its description: a description never names a save not there.)
        int status = 0;
        std::string body;
        if (!Call("PUT", name, Bytes(data), status, body, error)) return false;
        if (status < 200 || status > 299) { error = Problem(status, body); return false; }
        if (!Call("PUT", name + ".json", MetaToJson(meta), status, body, error)) return false;
        if (status < 200 || status > 299) { error = Problem(status, body); return false; }
        return true;
    }

private:
    bool Call(const char* method, const std::string& name, const std::string& data, int& status, std::string& body, std::string& error)
    {
        Achievements::HttpCall c;
        c.method = method;
        c.url = base + UrlEncode(name);
        if (!auth.empty()) c.headers.push_back(auth);
        if (c.method == "PUT") c.headers.push_back("Content-Type: application/octet-stream");
        c.body = data;
        if (!Achievements::HttpRequest(c, status, body, error)) { error = "NO CONNECTION (" + error + ")"; return false; }
        return true;
    }
    static std::string Problem(int status, const std::string&)
    {
        if (status == 401 || status == 403) return "WEBDAV NAME OR PASSWORD NOT ACCEPTED";
        if (status == 409) return "THE WEBDAV FOLDER DOES NOT EXIST - MAKE IT FIRST";
        return "WEBDAV ERROR " + std::to_string(status);
    }
    std::string base, auth;
};

std::unique_ptr<Backend> WebDavBackend(const std::string& url, const std::string& user, const std::string& password)
{
    return std::unique_ptr<Backend>(new WebDav(url, user, password));
}

// ---------------------------------------------------------------------------------------------
// Google Drive: each save one file in a "Fulldex Cloud Saves" folder in the player's Drive, its
// description in the file's appProperties. (drive.file: Fulldex sees only the files it made
// itself, nothing else in the Drive. The hidden app folder, drive.appdata, is what Google's
// documents offer, but its sign-in with a code, the only kind the Switch can do, refuses it.)

static const char* kDriveScope = "https://www.googleapis.com/auth/drive.file";
static const char* kDriveFolder = "Fulldex Cloud Saves";

static bool PostForm(const std::string& url, const std::string& form, int& status, std::string& body, std::string& error)
{
    Achievements::HttpCall c;
    c.method = "POST";
    c.url = url;
    c.headers.push_back("Content-Type: application/x-www-form-urlencoded");
    c.body = form;
    if (!Achievements::HttpRequest(c, status, body, error)) { error = "NO CONNECTION (" + error + ")"; return false; }
    return true;
}

class Drive : public Backend
{
public:
    Drive(const GoogleClient& c, const std::string& token) : client(c), refresh(token) {}
    const char* Name() const override { return "GOOGLE DRIVE"; }

    bool Stat(const std::string& name, bool& exists, Meta& meta, std::string& error) override
    {
        if (!Folder(error)) return false;
        Achievements::HttpCall c;
        c.url = FULLDEX_GOOGLE_API "/drive/v3/files?pageSize=10&fields=" + UrlEncode("files(id,appProperties)") + "&q=" +
                UrlEncode("name = '" + name + "' and '" + folder + "' in parents and trashed = false");
        int status = 0;
        std::string body;
        if (!Call(c, status, body, error)) return false;
        if (status != 200) { error = StatusError("GOOGLE DRIVE", status, body); return false; }
        std::string id;
        exists = JsonValue(body, "id", id) && !id.empty();
        if (!exists) { ids.erase(name); return true; }
        ids[name] = id;
        if (!MetaFromJson(body, meta))
        {
            // (No description: read the save itself.)
            std::vector<uint8_t> data;
            if (!Get(name, data, error)) return false;
            meta = MetaOf(data, "", 0);
        }
        return true;
    }
    bool Get(const std::string& name, std::vector<uint8_t>& data, std::string& error) override
    {
        std::string id;
        if (!Id(name, id, error)) return false;
        if (id.empty()) { error = "NOT IN GOOGLE DRIVE"; return false; }
        Achievements::HttpCall c;
        c.url = FULLDEX_GOOGLE_API "/drive/v3/files/" + id + "?alt=media";
        int status = 0;
        std::string body;
        if (!Call(c, status, body, error)) return false;
        if (status != 200) { error = StatusError("GOOGLE DRIVE", status, body); return false; }
        data = Bytes(body);
        return true;
    }
    bool Put(const std::string& name, const std::vector<uint8_t>& data, const Meta& meta, std::string& error) override
    {
        std::string id;
        if (!Id(name, id, error)) return false;
        if (!Folder(error)) return false;
        const std::string props = "\"appProperties\":" + MetaToJson(meta);
        for (int attempt = 0; attempt < 2; attempt++)
        {
            const std::string boundary = "fulldex-cloud-save-boundary-7d3f";
            const std::string metadata = id.empty()
                ? "{\"name\":\"" + JsonEscape(name) + "\",\"parents\":[\"" + folder + "\"]," + props + "}"
                : "{" + props + "}";
            Achievements::HttpCall c;
            c.method = id.empty() ? "POST" : "PATCH";
            c.url = FULLDEX_GOOGLE_API "/upload/drive/v3/files" + (id.empty() ? std::string() : "/" + id) +
                    "?uploadType=multipart&fields=id";
            c.headers.push_back("Content-Type: multipart/related; boundary=" + boundary);
            c.body = "--" + boundary + "\r\nContent-Type: application/json; charset=UTF-8\r\n\r\n" + metadata + "\r\n--" +
                     boundary + "\r\nContent-Type: application/octet-stream\r\n\r\n" + Bytes(data) + "\r\n--" + boundary + "--";
            int status = 0;
            std::string body;
            if (!Call(c, status, body, error)) return false;
            if (status == 404 && !id.empty()) { id.clear(); ids.erase(name); continue; } // (gone meanwhile: made anew)
            if (status != 200) { error = StatusError("GOOGLE DRIVE", status, body); return false; }
            std::string got;
            if (JsonValue(body, "id", got)) ids[name] = got;
            return true;
        }
        error = "GOOGLE DRIVE: CANNOT SAVE";
        return false;
    }

private:
    // The saves' folder: found by name among the files Fulldex made, or made now.
    bool Folder(std::string& error)
    {
        if (!folder.empty()) return true;
        Achievements::HttpCall find;
        find.url = FULLDEX_GOOGLE_API "/drive/v3/files?pageSize=10&fields=" + UrlEncode("files(id)") + "&q=" +
                   UrlEncode(std::string("name = '") + kDriveFolder +
                             "' and mimeType = 'application/vnd.google-apps.folder' and trashed = false");
        int status = 0;
        std::string body;
        if (!Call(find, status, body, error)) return false;
        if (status != 200) { error = StatusError("GOOGLE DRIVE", status, body); return false; }
        if (JsonValue(body, "id", folder) && !folder.empty()) return true;
        Achievements::HttpCall make;
        make.method = "POST";
        make.url = FULLDEX_GOOGLE_API "/drive/v3/files?fields=id";
        make.headers.push_back("Content-Type: application/json; charset=UTF-8");
        make.body = std::string("{\"name\":\"") + kDriveFolder + "\",\"mimeType\":\"application/vnd.google-apps.folder\"}";
        if (!Call(make, status, body, error)) return false;
        if (status != 200 || !JsonValue(body, "id", folder)) { folder.clear(); error = StatusError("GOOGLE DRIVE", status, body); return false; }
        return true;
    }
    bool Id(const std::string& name, std::string& id, std::string& error)
    {
        auto it = ids.find(name);
        if (it != ids.end()) { id = it->second; return true; }
        bool exists = false;
        Meta m;
        if (!Stat(name, exists, m, error)) return false;
        id = exists ? ids[name] : std::string();
        return true;
    }
    bool Token(bool renew, std::string& error)
    {
        if (!renew && !access.empty() && time(nullptr) < accessUntil - 60) return true;
        int status = 0;
        std::string body;
        if (!PostForm(FULLDEX_GOOGLE_OAUTH "/token",
                      "client_id=" + UrlEncode(client.id) + "&client_secret=" + UrlEncode(client.secret) +
                          "&refresh_token=" + UrlEncode(refresh) + "&grant_type=refresh_token",
                      status, body, error))
            return false;
        std::string expires;
        if (status != 200 || !JsonValue(body, "access_token", access))
        {
            std::string e;
            JsonValue(body, "error", e);
            error = e == "invalid_grant" ? "GOOGLE SIGN-IN EXPIRED - SIGN IN AGAIN" : StatusError("GOOGLE SIGN-IN", status, body);
            access.clear();
            return false;
        }
        accessUntil = time(nullptr) + (JsonValue(body, "expires_in", expires) ? atol(expires.c_str()) : 3000);
        return true;
    }
    bool Call(Achievements::HttpCall c, int& status, std::string& body, std::string& error)
    {
        for (int attempt = 0; attempt < 2; attempt++)
        {
            if (!Token(attempt > 0, error)) return false;
            Achievements::HttpCall call = c;
            call.headers.push_back("Authorization: Bearer " + access);
            if (!Achievements::HttpRequest(call, status, body, error)) { error = "NO CONNECTION (" + error + ")"; return false; }
            if (status != 401) return true;
        }
        return true;
    }
    GoogleClient client;
    std::string refresh, access, folder;
    time_t accessUntil = 0;
    std::map<std::string, std::string> ids;
};

std::unique_ptr<Backend> GoogleDriveBackend(const GoogleClient& client, const std::string& refreshToken)
{
    return std::unique_ptr<Backend>(new Drive(client, refreshToken));
}

bool GoogleSignInStart(const GoogleClient& client, SignIn& out, std::string& error)
{
    if (client.id.empty()) { error = "THIS VERSION HAS NO GOOGLE SIGN-IN"; return false; }
    int status = 0;
    std::string body, v;
    if (!PostForm(FULLDEX_GOOGLE_OAUTH "/device/code", "client_id=" + UrlEncode(client.id) + "&scope=" + UrlEncode(kDriveScope),
                  status, body, error))
        return false;
    if (status != 200 || !JsonValue(body, "device_code", out.deviceCode) || !JsonValue(body, "user_code", out.userCode))
    {
        error = StatusError("GOOGLE SIGN-IN", status, body);
        return false;
    }
    if (!JsonValue(body, "verification_url", out.verificationUrl)) JsonValue(body, "verification_uri", out.verificationUrl);
    if (JsonValue(body, "interval", v)) out.interval = atoi(v.c_str()) > 0 ? atoi(v.c_str()) : 5;
    if (JsonValue(body, "expires_in", v)) out.expiresIn = atoi(v.c_str());
    return true;
}

SignInState GoogleSignInPoll(const GoogleClient& client, const SignIn& signIn, std::string& refreshToken, std::string& error)
{
    int status = 0;
    std::string body;
    if (!PostForm(FULLDEX_GOOGLE_OAUTH "/token",
                  "client_id=" + UrlEncode(client.id) + "&client_secret=" + UrlEncode(client.secret) + "&device_code=" +
                      UrlEncode(signIn.deviceCode) + "&grant_type=" + UrlEncode("urn:ietf:params:oauth:grant-type:device_code"),
                  status, body, error))
        return SignInState::Failed;
    if (status == 200 && JsonValue(body, "refresh_token", refreshToken)) return SignInState::Done;
    std::string e;
    JsonValue(body, "error", e);
    if (e == "authorization_pending") return SignInState::Waiting;
    if (e == "slow_down") return SignInState::SlowDown;
    if (e == "access_denied") return SignInState::Denied;
    if (e == "expired_token") return SignInState::Expired;
    error = StatusError("GOOGLE SIGN-IN", status, body);
    return SignInState::Failed;
}

// ---------------------------------------------------------------------------------------------
// Which save wins. This device remembers, for each save, the one last known to match the cloud
// ("base"). Then: only the cloud changed since, it comes down; only this one, it goes up; both,
// the player chooses.

struct Sync::Impl
{
    std::unique_ptr<Backend> backend;
    std::string statePath, device;
    std::map<std::string, std::string> base;
    std::mutex mutex;

    void Load()
    {
        std::vector<uint8_t> raw;
        if (!ReadAll(statePath, raw)) return;
        const std::string text = Bytes(raw);
        size_t at = 0;
        while (at < text.size())
        {
            size_t end = text.find('\n', at);
            if (end == std::string::npos) end = text.size();
            const std::string line = text.substr(at, end - at);
            const size_t tab = line.find('\t');
            if (tab != std::string::npos) base[line.substr(0, tab)] = line.substr(tab + 1);
            at = end + 1;
        }
    }
    void Store()
    {
        std::string text;
        for (const auto& b : base) text += b.first + "\t" + b.second + "\n";
        std::string error;
        if (!WriteAtomic(statePath, Bytes(text), error)) fprintf(stderr, "[cloud] cannot keep %s\n", statePath.c_str());
    }
    void Matched(const std::string& name, const std::string& hash)
    {
        base[name] = hash;
        Store();
    }

    static Result Say(Outcome o, const std::string& message)
    {
        Result r;
        r.outcome = o;
        r.message = message;
        return r;
    }
    static std::string Upper(std::string s)
    {
        for (char& c : s) c = (char)toupper((unsigned char)c);
        return s;
    }

    Result Pull(const std::string& savePath, const std::string& name, const std::vector<uint8_t>& local, const Meta& cloud)
    {
        std::vector<uint8_t> data;
        std::string error;
        if (!backend->Get(name, data, error)) return Say(Outcome::Failed, "CLOUD SAVE NOT DOWNLOADED - " + error);
        if (Hash(data) != cloud.hash)
            return Say(Outcome::Failed, "THE CLOUD SAVE WAS STILL UPLOADING - TRY AGAIN"); // (a save and description from two uploads)
        Result r;
        r.backup = BackUp(savePath, local);
        if (!local.empty() && r.backup.empty()) return Say(Outcome::Failed, "CANNOT BACK UP THIS SAVE - NOTHING CHANGED");
        if (!WriteAtomic(savePath, data, error)) return Say(Outcome::Failed, error);
        Matched(name, cloud.hash);
        r.outcome = Outcome::Pulled;
        r.message = cloud.device.empty() ? "NEWER SAVE BROUGHT DOWN FROM THE CLOUD" : "NEWER SAVE FROM " + Upper(cloud.device) + " BROUGHT DOWN";
        r.cloud = cloud;
        return r;
    }
    Result Push(const std::string& name, const std::vector<uint8_t>& local)
    {
        const Meta m = MetaOf(local, device, (int64_t)time(nullptr));
        std::string error;
        if (!backend->Put(name, local, m, error)) return Say(Outcome::Failed, "SAVE NOT UPLOADED - " + error);
        Matched(name, m.hash);
        Result r = Say(Outcome::Pushed, "SAVE UPLOADED TO " + std::string(backend->Name()));
        r.local = m;
        return r;
    }
    Result Conflict(const std::string& savePath, const std::vector<uint8_t>& local, const Meta& cloud)
    {
        Result r = Say(Outcome::Conflict, "THIS SAVE AND THE CLOUD SAVE ARE DIFFERENT");
        r.local = MetaOf(local, device, ModifiedTime(savePath));
        r.cloud = cloud;
        return r;
    }
};

Sync::Sync(std::unique_ptr<Backend> backend, const std::string& statePath, const std::string& deviceName) : d(new Impl)
{
    d->backend = std::move(backend);
    d->statePath = statePath;
    d->device = deviceName;
    d->Load();
}

Sync::~Sync() {}

const char* Sync::BackendName() const { return d->backend->Name(); }

static std::string FileName(const std::string& savePath) { return fs::u8path(savePath).filename().u8string(); }

Result Sync::BeforePlay(const std::string& savePath)
{
    std::lock_guard<std::mutex> lock(d->mutex);
    const std::string name = FileName(savePath);
    std::vector<uint8_t> local;
    ReadAll(savePath, local);
    const std::string here = local.empty() ? std::string() : Hash(local);
    const std::string last = d->base.count(name) ? d->base[name] : std::string();

    bool exists = false;
    Meta cloud;
    std::string error;
    if (!d->backend->Stat(name, exists, cloud, error))
        return Impl::Say(Outcome::Failed, "CLOUD NOT REACHED - PLAYING THIS DEVICE'S SAVE (" + error + ")");
    if (!exists)
        return here.empty() ? Impl::Say(Outcome::NothingYet, "NO SAVE YET") : d->Push(name, local);
    if (cloud.hash == here)
    {
        if (last != here) d->Matched(name, here);
        return Impl::Say(Outcome::InSync, "SAVE UP TO DATE");
    }
    if (here.empty() || here == last) return d->Pull(savePath, name, local, cloud);  // only the cloud changed
    if (cloud.hash == last) return d->Push(name, local);                              // only this one changed
    return d->Conflict(savePath, local, cloud);
}

Result Sync::AfterPlay(const std::string& savePath)
{
    std::lock_guard<std::mutex> lock(d->mutex);
    const std::string name = FileName(savePath);
    std::vector<uint8_t> local;
    ReadAll(savePath, local);
    if (local.empty()) return Impl::Say(Outcome::NothingYet, "NO SAVE YET");
    const std::string here = Hash(local);
    const std::string last = d->base.count(name) ? d->base[name] : std::string();
    if (here == last) return Impl::Say(Outcome::InSync, "SAVE UP TO DATE");

    bool exists = false;
    Meta cloud;
    std::string error;
    if (!d->backend->Stat(name, exists, cloud, error))
        return Impl::Say(Outcome::Failed, "CLOUD NOT REACHED - THE SAVE GOES UP NEXT TIME (" + error + ")");
    if (exists && cloud.hash == here)
    {
        d->Matched(name, here);
        return Impl::Say(Outcome::InSync, "SAVE UP TO DATE");
    }
    if (!exists || cloud.hash == last) return d->Push(name, local);
    // (Another device saved meanwhile: nothing goes up over it; the player chooses next start.)
    return d->Conflict(savePath, local, cloud);
}

Result Sync::Resolve(const std::string& savePath, bool keepCloud)
{
    std::lock_guard<std::mutex> lock(d->mutex);
    const std::string name = FileName(savePath);
    std::vector<uint8_t> local;
    ReadAll(savePath, local);
    bool exists = false;
    Meta cloud;
    std::string error;
    if (!d->backend->Stat(name, exists, cloud, error))
        return Impl::Say(Outcome::Failed, "CLOUD NOT REACHED - NOTHING CHANGED (" + error + ")");
    if (keepCloud)
    {
        if (!exists) return Impl::Say(Outcome::Failed, "THERE IS NO CLOUD SAVE");
        return d->Pull(savePath, name, local, cloud);
    }
    if (local.empty()) return Impl::Say(Outcome::Failed, "THERE IS NO SAVE ON THIS DEVICE");
    Result kept;
    if (exists && cloud.hash != Hash(local))
    {
        // (The cloud's save, about to be replaced, kept in the backups here.)
        std::vector<uint8_t> theirs;
        if (!d->backend->Get(name, theirs, error)) return Impl::Say(Outcome::Failed, "CLOUD SAVE NOT DOWNLOADED - " + error);
        kept.backup = BackUp(savePath, theirs);
        if (kept.backup.empty()) return Impl::Say(Outcome::Failed, "CANNOT BACK UP THE CLOUD SAVE - NOTHING CHANGED");
    }
    Result r = d->Push(name, local);
    r.backup = kept.backup;
    return r;
}

// ---------------------------------------------------------------------------------------------
// cloud.ini

static std::string Trim(const std::string& s)
{
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) a++;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) b--;
    return s.substr(a, b - a);
}

Settings LoadSettings(const std::string& path)
{
    Settings s;
    std::vector<uint8_t> raw;
    if (!ReadAll(path, raw)) return s;
    const std::string text = Bytes(raw);
    size_t at = 0;
    while (at < text.size())
    {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        const std::string line = Trim(text.substr(at, end - at));
        at = end + 1;
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = Trim(line.substr(0, eq)), v = Trim(line.substr(eq + 1));
        if (k == "provider") s.provider = v;
        else if (k == "webdav_url") s.webdavUrl = v;
        else if (k == "webdav_user") s.webdavUser = v;
        else if (k == "webdav_password") s.webdavPassword = v;
        else if (k == "folder") s.folder = v;
        else if (k == "google_refresh_token") s.googleRefreshToken = v;
        else if (k == "google_client_id") s.google.id = v;
        else if (k == "google_client_secret") s.google.secret = v;
        else if (k == "device_name") s.deviceName = v;
    }
    return s;
}

bool SaveSettings(const std::string& path, const Settings& s)
{
    const std::string text =
        "# Pokemon Fulldex cloud saves. provider: off, google, webdav or folder.\n"
        "provider=" + s.provider + "\n"
        "# WebDAV: the address of a folder that already exists, e.g. https://cloud.example.com/remote.php/dav/files/me/Fulldex/\n"
        "webdav_url=" + s.webdavUrl + "\nwebdav_user=" + s.webdavUser + "\nwebdav_password=" + s.webdavPassword + "\n"
        "# Folder: one that Dropbox, OneDrive, Syncthing or similar keeps in sync\n"
        "folder=" + s.folder + "\n"
        "# Google Drive: filled in by signing in from the menu\n"
        "google_refresh_token=" + s.googleRefreshToken + "\n"
        + (s.google.id.empty() ? std::string() : "google_client_id=" + s.google.id + "\ngoogle_client_secret=" + s.google.secret + "\n") +
        "# How this device is named when another asks which save to keep (blank: its kind)\n"
        "device_name=" + s.deviceName + "\n";
    std::string error;
    return WriteAtomic(path, Bytes(text), error);
}

GoogleClient BuiltInGoogleClient()
{
    GoogleClient c;
    c.id = FULLDEX_GOOGLE_CLIENT_ID;
    c.secret = FULLDEX_GOOGLE_CLIENT_SECRET;
    return c;
}

std::unique_ptr<Sync> FromSettings(const Settings& s, const std::string& statePath, const std::string& defaultDevice)
{
    std::unique_ptr<Backend> b;
    if (s.provider == "google" && !s.googleRefreshToken.empty())
        b = GoogleDriveBackend(s.google.id.empty() ? BuiltInGoogleClient() : s.google, s.googleRefreshToken);
    else if (s.provider == "webdav" && !s.webdavUrl.empty())
        b = WebDavBackend(s.webdavUrl, s.webdavUser, s.webdavPassword);
    else if (s.provider == "folder" && !s.folder.empty())
        b = FolderBackend(s.folder);
    if (!b) return nullptr;
    return std::unique_ptr<Sync>(new Sync(std::move(b), statePath, s.deviceName.empty() ? defaultDevice : s.deviceName));
}

}
