#pragma once
// TRADES OVER WI-FI (the Hub's Trade page): two devices on the same network find each other and
// send Pokémon, as a link cable would. One opens a room; it says so on the network every second
// (UDP broadcast, port 47261), and the other sees it and joins (TCP, port 47260). Then each side
// can offer a Pokémon (a trade: one each way, both accept) or send one (a gift: the other
// accepts). Only the bytes of the Pokémon and a few words go across: nothing else of a save.
//
// The network runs on a thread of its own; the page asks Snapshot() for how things stand, and
// Version() changes whenever they change. Nothing here reads or writes a save: the page does,
// with pokemon_hub.h, once both sides have said yes.
#include <cstdint>
#include <string>
#include <vector>

namespace HubLink
{
enum class State { Off, Hosting, Browsing, Linked, Online }; // (Online: waiting at the relay for the other)

struct Room { std::string name, game, address; int code = 0; };

// What the other side offered: its generation, its bytes, and how to show it.
struct Offer
{
    bool any = false, gift = false;
    int generation = 0;
    std::vector<uint8_t> data;
    std::string name, species, game; // (as sent, for showing before it is read)
    int speciesId = 0, level = 0;
    bool shiny = false;
};

struct Status
{
    State state = State::Off;
    int code = 0;                 // this room's number (Hosting, Linked as host)
    std::string onlineCode;       // the code typed or made (Online, Linked online)
    std::vector<Room> rooms;      // rooms seen (Browsing)
    std::string peerName, peerGame, myAddress;
    Offer mine, theirs;
    bool theyCanTake = false;     // their side's check of my offer
    std::string theirReason;      // why not
    bool iAccepted = false, theyAccepted = false;
    bool finish = false;          // both accepted: this side should now write its save (FinishDone after)
    bool theyFinished = false;
    std::string note;             // the last thing to tell the player
};

// me: this player's trainer and game, as the other side is told.
void Host(const std::string& me, const std::string& game);
void Browse(const std::string& me, const std::string& game);
void Join(const Room& room);
void Leave();

// ONLINE (tools/relay: a Cloudflare Worker): both devices type the same code; the relay passes the
// same messages between them. relay: its address, e.g. https://fulldex-relay.example.workers.dev.
std::string NewCode();
void Online(const std::string& relay, const std::string& code, const std::string& me, const std::string& game);

void SetOffer(const Offer& offer);       // this side's offer (any false: none)
void TellCheck(bool ok, const std::string& reason); // this side's check of their offer
void Accept(bool yes);
void FinishDone(bool ok, const std::string& note);  // this side wrote its save (or could not)

Status Snapshot();
int Version();
}
