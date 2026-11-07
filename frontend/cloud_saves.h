#pragma once
// Cloud saves: a game's save follows the player between their devices (PC, Switch, Android,
// Mac, iPhone). Before a game starts its save is brought down from the cloud if another device
// saved later; when it ends, the save goes up. Nothing is ever lost: a save about to be replaced
// is first copied into the backups folder beside it (the same one the RESTORE BACKUP menu lists),
// and when two devices both played on without syncing, the player is asked which to keep.
//
// Where the saves are kept (cloud.ini, beside the settings):
//   google   the player's own Google Drive, in Fulldex's private app folder (signed in once
//            with a code typed on a phone or computer, so it works on the Switch too)
//   webdav   any WebDAV server (Nextcloud, a NAS, many hosts): address, name and password
//   folder   a folder on this device that something else syncs (Dropbox, OneDrive, Syncthing)
//
// Everything here blocks on the network: call it from a thread of its own, or with a
// SYNCING message up.
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace CloudSaves
{

// What is known of one save without fetching it.
struct Meta
{
    std::string hash;        // of the save's bytes (Hash()); empty: none
    std::string device;      // the device that saved it ("Windows PC", "Nintendo Switch")
    int64_t savedAt = 0;     // seconds since 1970, UTC
    uint64_t size = 0;
};

std::string Hash(const std::vector<uint8_t>& data);
std::string MetaToJson(const Meta& m);
bool MetaFromJson(const std::string& json, Meta& m);

// A place saves are kept. Names are bare file names ("black2.sav").
class Backend
{
public:
    virtual ~Backend() {}
    virtual const char* Name() const = 0;
    // exists false: no such save there yet. False return: could not tell (error says why).
    virtual bool Stat(const std::string& name, bool& exists, Meta& meta, std::string& error) = 0;
    virtual bool Get(const std::string& name, std::vector<uint8_t>& data, std::string& error) = 0;
    virtual bool Put(const std::string& name, const std::vector<uint8_t>& data, const Meta& meta, std::string& error) = 0;
};

std::unique_ptr<Backend> FolderBackend(const std::string& folder);
std::unique_ptr<Backend> WebDavBackend(const std::string& url, const std::string& user, const std::string& password);

// Google Drive. The refresh token is the lasting sign-in; it comes from SignIn below.
struct GoogleClient
{
    std::string id, secret;
};
std::unique_ptr<Backend> GoogleDriveBackend(const GoogleClient& client, const std::string& refreshToken);

// Signing in to Google on a device with no proper browser: show the player the address and code
// (Start), then ask every few seconds (Poll) until they have said yes on their phone.
struct SignIn
{
    std::string deviceCode, userCode, verificationUrl;
    int interval = 5, expiresIn = 1800;
};
bool GoogleSignInStart(const GoogleClient& client, SignIn& out, std::string& error);
enum class SignInState { Waiting, SlowDown, Done, Denied, Expired, Failed };
SignInState GoogleSignInPoll(const GoogleClient& client, const SignIn& signIn, std::string& refreshToken, std::string& error);

enum class Outcome
{
    Off,         // cloud saves are not set up
    InSync,      // the same save here and in the cloud
    Pulled,      // the cloud's newer save was brought down (the one here kept in backups)
    Pushed,      // this save went up
    NothingYet,  // no save here or in the cloud
    Conflict,    // both changed since they last matched: ask the player (Resolve)
    Failed,      // could not reach the cloud; the save here is played, and synced next time
};

struct Result
{
    Outcome outcome = Outcome::Off;
    std::string message;     // for the player, in capitals like the rest of the launcher
    Meta local, cloud;       // filled for Conflict, for the choice
    std::string backup;      // a save that was set aside, if any
};

class Sync
{
public:
    // statePath: a small file of this device's own, remembering each save as last synced.
    Sync(std::unique_ptr<Backend> backend, const std::string& statePath, const std::string& deviceName);
    ~Sync();
    const char* BackendName() const;

    // Before the game reads its save.
    Result BeforePlay(const std::string& savePath);
    // After the game wrote its save for the last time (or any time it saved: it is cheap when
    // nothing changed).
    Result AfterPlay(const std::string& savePath);
    // After a Conflict: keepCloud brings the cloud's down, otherwise this one goes up. The one
    // not kept is copied into the backups folder either way.
    Result Resolve(const std::string& savePath, bool keepCloud);

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

// cloud.ini: provider=off|google|webdav|folder, then webdav_url, webdav_user, webdav_password,
// folder, google_refresh_token, google_client_id, google_client_secret, device_name.
struct Settings
{
    std::string provider = "off";
    std::string webdavUrl, webdavUser, webdavPassword;
    std::string folder;
    std::string googleRefreshToken;
    GoogleClient google;
    std::string deviceName;
};
Settings LoadSettings(const std::string& path);
bool SaveSettings(const std::string& path, const Settings& s);
// The client built in (FULLDEX_GOOGLE_CLIENT_ID/SECRET at build time), unless cloud.ini names one.
GoogleClient BuiltInGoogleClient();
// Null when off or not fully set up.
std::unique_ptr<Sync> FromSettings(const Settings& s, const std::string& statePath, const std::string& defaultDevice);

}
