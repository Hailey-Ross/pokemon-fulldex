// Music and sound effects at their own volumes, for DS games made with Nintendo's sound library
// (all the Pokemon games).
//
// The game's main processor does not play sound itself: it writes a list of sound commands to
// main memory and sends the list's address to the sound processor through the IPC FIFO, tag 7
// (NitroSDK SND_FlushCommand). The list is read here, as it is sent, before the sound processor
// sees it:
//   - START_SEQ / PREPARE_SEQ (player, sequence data, offset, bank): a sequence starts on one of
//     the 16 sequence players. A sequence that opens 4 or more tracks is music (HeartGold's songs
//     use 7 to 16; its sound effects and cries 1).
//   - PLAYER_PARAM at offset 6, 2 bytes: the player's volume (SNDPlayer.extFader) as an
//     attenuation in tenths of a decibel (0 full, -723 silent). The sound library sends it only when
//     it changes; Music's setting is added to it for a music player, Sound Effects' for the others.
// So that a change of setting is heard at once (not at the game's next change of volume), the
// sound processor's own players (SNDWork.players, 16 of 0x24 bytes, playerId at +1, extFader at
// +6) are found in its memory and their volume is kept at ours every frame (Apply).
// Layouts: NitroSDK, as in pret/pokediamond include/nitro/SND_command_shared.h, SND_seq_shared.h,
// SND_work_shared.h and SND_SetPlayerVolume in arm9/lib/NitroSDK/src/SND_interface.c.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "NDS.h"

struct GameMusic
{
    melonDS::NDS* Nds = nullptr;
    int Music = 100, Effects = 100; // percent, as the menu shows them
    bool External = false; // the player's own music is playing and QUIET GAME MUSIC is on: no music
    bool Log = false;

    bool isMusic[16] = {};
    // (Streamed music, as Black 2's opening: the main processor locks two hardware channels at
    // once (LOCK_CHANNEL, a mask of 2 or more), sets each up as PCM (SETUP_CHANNEL_PCM, its volume
    // in the third word's top byte) and fades them (CHANNEL_VOLUME: mask, volume). Those channels'
    // volumes scaled by Music until UNLOCK_CHANNEL; a single locked channel (a voice, a sample
    // played by the game itself) by Sound Effects.)
    uint32_t streamChannels = 0, sampleChannels = 0;
    int16_t gameVolume[16] = {}; // the volume the game last asked for, before ours

    uint8_t* players = nullptr; // the sound processor's 16 players, once found
    unsigned searchWait = 0;
    bool active = false; // our volumes are in the players

    void Attach(melonDS::NDS& nds)
    {
        Nds = &nds;
        Log = getenv("FULLDEX_SOUND_LOG") != nullptr;
        nds.IPCSendHook = &GameMusic::OnSend;
        nds.IPCSendUser = this;
    }

    // The attenuation added for a setting: 20 log10(percent / 100) decibels, in tenths.
    static int Attenuation(int percent)
    {
        if (percent >= 100) return 0;
        if (percent <= 0) return -32768;
        return (int)std::lround(200.0 * std::log10(percent / 100.0));
    }

    // A stream's channel volume (0 to 127) at Music's setting: amplitude, as the hardware's volume is.
    uint32_t StreamVolume(uint32_t v) const
    {
        const int percent = External ? 0 : Music;
        if (percent >= 100) return v;
        return (uint32_t)std::lround(v * std::max(0, percent) / 100.0);
    }

    uint32_t SampleVolume(uint32_t v) const
    {
        if (Effects >= 100) return v;
        return (uint32_t)std::lround(v * std::max(0, Effects) / 100.0);
    }

    int16_t Ours(int player, int16_t game) const
    {
        const int add = Attenuation(isMusic[player] ? (External ? 0 : Music) : Effects);
        if (add <= -32768) return -32768;
        return (int16_t)std::max(-32768, (int)game + add);
    }

    uint8_t* Ram(uint32_t addr) { return Nds->MainRAM + (addr & Nds->MainRAMMask); }
    uint32_t Read32(uint32_t addr) { uint32_t v; memcpy(&v, Ram(addr), 4); return v; }
    static bool InMain(uint32_t addr) { return addr >= 0x02000000 && addr < 0x03000000; }

    // How many tracks a sequence opens: its first command, 0xFE, and a mask of the tracks.
    int Tracks(uint32_t base, uint32_t offset)
    {
        const uint32_t at = base + offset;
        if (!InMain(at)) return 0;
        const uint8_t* p = Ram(at);
        if (p[0] != 0xFE) return 1;
        const unsigned mask = p[1] | p[2] << 8;
        int n = 0;
        for (unsigned m = mask; m; m &= m - 1) n++;
        return n;
    }

    static void OnSend(void* user, uint32_t word)
    {
        auto* self = static_cast<GameMusic*>(user);
        if ((word & 0x1F) == 7) self->Commands(word >> 6);
    }

    void Commands(uint32_t cmd)
    {
        for (int guard = 0; InMain(cmd) && guard < 256; guard++)
        {
            const uint32_t id = Read32(cmd + 4);
            const uint32_t a0 = Read32(cmd + 8), a1 = Read32(cmd + 12), a2 = Read32(cmd + 16), a3 = Read32(cmd + 20);
            if ((id == 0x00 || id == 0x02) && a0 < 16) // START_SEQ, PREPARE_SEQ
            {
                const int tracks = Tracks(a1, a2);
                isMusic[a0] = tracks >= 4;
                gameVolume[a0] = 0; // (the sound processor starts a player at full: PlayerInit)
                if (Log) fprintf(stderr, "[sound] player %u starts a sequence of %d tracks: %s\n", a0, tracks, isMusic[a0] ? "music" : "effect");
            }
            else if (id == 0x06 && a0 < 16 && a1 == 6 && a3 == 2) // PLAYER_PARAM: volume
            {
                const int16_t game = (int16_t)a2;
                gameVolume[a0] = game;
                const int16_t ours = Ours(a0, game);
                if (ours != game)
                {
                    const uint32_t v = (uint32_t)(uint16_t)ours;
                    memcpy(Ram(cmd + 16), &v, 4);
                }
                if (Log) fprintf(stderr, "[sound] player %u volume %d -> %d\n", a0, game, ours);
            }
            else if (id == 26) (__builtin_popcount(a0 & 0xFFFF) >= 2 ? streamChannels : sampleChannels) |= a0 & 0xFFFF; // LOCK_CHANNEL
            else if (id == 27) { streamChannels &= ~a0; sampleChannels &= ~a0; } // UNLOCK_CHANNEL
            else if (id == 14 && ((streamChannels | sampleChannels) >> (a0 & 15) & 1)) // SETUP_CHANNEL_PCM
            {
                const bool music = streamChannels >> (a0 & 15) & 1;
                const uint32_t v = a2 >> 24, scaled = music ? StreamVolume(v) : SampleVolume(v);
                if (scaled != v) { const uint32_t w = (a2 & 0x00FFFFFF) | scaled << 24; memcpy(Ram(cmd + 16), &w, 4); }
                if (Log) fprintf(stderr, "[sound] %s channel %u volume %u -> %u\n", music ? "stream" : "sample", a0 & 15, v, scaled);
            }
            else if (id == 20 && (a0 & (streamChannels | sampleChannels))) // CHANNEL_VOLUME
            {
                const uint32_t scaled = (a0 & streamChannels) ? StreamVolume(a1 & 0x7F) : SampleVolume(a1 & 0x7F);
                if (scaled != (a1 & 0x7F)) { const uint32_t w = (a1 & ~0x7Fu) | scaled; memcpy(Ram(cmd + 12), &w, 4); }
            }
            if (Log && id >= 12 && id <= 28) // (the channel commands, as streams use them)
                fprintf(stderr, "[sound] command %u: %08x %08x %08x %08x\n", id, a0, a1, a2, a3);
            cmd = Read32(cmd);
        }
    }

    // Whether 16 players start here: player p's id (byte 1) is p, its flags (byte 0) at most 7.
    static bool PlayersAt(const uint8_t* m)
    {
        for (int p = 0; p < 16; p++)
            if (m[p * 0x24 + 1] != p || m[p * 0x24] > 7) return false;
        return true;
    }

    uint8_t* FindPlayers()
    {
        const struct { uint8_t* mem; uint32_t size; } where[] = {
            { Nds->ARM7WRAM, Nds->ARM7WRAMSize }, { Nds->SharedWRAM, Nds->SharedWRAMSize }, { Nds->MainRAM, Nds->MainRAMMask + 1 } };
        for (const auto& w : where)
        {
            if (!w.mem || w.size < 16 * 0x24) continue;
            for (uint32_t o = 0; o + 16 * 0x24 <= w.size; o += 4)
                if (PlayersAt(w.mem + o)) return w.mem + o;
        }
        return nullptr;
    }

    // Every frame: the players' volume as ours (a change of setting heard at once).
    void Apply()
    {
        if (!Nds) return;
        if (players && !PlayersAt(players)) players = nullptr;
        if (Music >= 100 && Effects >= 100 && !External)
        {
            // (Both at full: the game's own volumes put back once, then left alone.)
            if (active && players)
                for (int p = 0; p < 16; p++) memcpy(players + p * 0x24 + 6, &gameVolume[p], 2);
            active = false;
            return;
        }
        active = true;
        if (!players)
        {
            if (searchWait) { searchWait--; return; }
            players = FindPlayers();
            if (!players) { searchWait = 120; return; }
            if (Log) fprintf(stderr, "[sound] the sound processor's players found\n");
        }
        for (int p = 0; p < 16; p++)
        {
            const int16_t want = Ours(p, gameVolume[p]);
            int16_t now;
            memcpy(&now, players + p * 0x24 + 6, 2);
            if (now != want) memcpy(players + p * 0x24 + 6, &want, 2);
        }
    }
};
