// Fast forward's music at its own speed and pitch, for the DS games: what the game is playing,
// heard at 1x with no delay while the game runs fast.
//
// A DS game's music is played by its sound processor (the ARM7, Nintendo's sound driver), from
// data the game has already put in main memory: the main processor only sends it commands. So a
// second console given the game's state at that moment, with its main processor stopped for
// good, goes on playing exactly what the game was playing, by itself: its loops, its tempo,
// nothing recorded or cut. That copy has no cartridge (the state's cartridge part is passed
// over, so the save is never touched), no JIT, and nothing of it is drawn. What its sound
// processor sends the main one is read off the FIFO every frame, as the main processor would (a
// full FIFO stalls the driver), and the shared flag the main processor releases is released
// (else the driver waits on it for good).
// A new song in the game: a new copy (main.cpp, from GameMusic's count of songs started and
// stopped). Streamed music (Black 2's opening) is fed by the main processor: not for this.
//
// Adapted from PokeDaisy's pk_nds_music (app/src/main/cpp/nds/pk_nds_music.cpp),
// Copyright (C) 2026 Lidor Itzhari, GNU GPL version 3: https://github.com/lidor30/pokedaisy
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include "NDS.h"
#include "Savestate.h"

extern bool rc_frame_hidden, rc_wide_render_skip; // melonDS/src/GPU.cpp

struct FfMusic
{
    std::unique_ptr<melonDS::NDS> copy;
    bool playing = false;
    bool gaveUp = false; // this song could not be played this way: the older way until the next
    uint32_t cnt[16] = {};
    int still = 0; // frames the copy's channels have all stood still while one sounds
    unsigned frames = 0;
    bool log = getenv("FULLDEX_FFMUSIC_LOG") != nullptr;

    // Windows, Linux and Android, where it has been run. A second console is memory the Switch is
    // short of, and on Apple's processors making one changes the thread's JIT write mode: those
    // keep FastForwardAudio's stretches until it has been tried there.
    static bool Supported()
    {
#if defined(__SWITCH__) || defined(__APPLE__)
        return false;
#else
        static const bool off = getenv("FULLDEX_FFMUSIC") && atoi(getenv("FULLDEX_FFMUSIC")) == 0;
        return !off;
#endif
    }

    // The copy takes the game's state as it is now and plays on from here. Between the game's
    // frames, on the thread that runs them.
    bool Begin(melonDS::NDS& live)
    {
        using namespace melonDS;
        playing = false;
        Savestate save;
        if (!live.DoSavestate(&save) || save.Error) return false;
        save.Finish();
        if (!copy || copy->LargeMainRAM != live.LargeMainRAM)
        {
            NDSArgs args;
            args.JIT = std::nullopt; // (its sound processor idles most of the time: the interpreter is enough)
            copy = std::make_unique<NDS>(std::move(args));
            copy->LargeMainRAM = live.LargeMainRAM;
            copy->Reset();
            copy->Start();
        }
        NDS& n = *copy;
        // (Which processor has the shared work memory, WRAMCNT: a console just made gives it all
        // to the main one, a game to the sound processor, whose program is in it. Loading a state
        // into a console that already says what the state says leaves the old arrangement in
        // place, so it is set as the game has it first, through the register, and again after.)
        const u8 wramCnt = live.ARM9Read8(0x04000247);
        n.ARM9Write8(0x04000247, wramCnt ^ 3);
        n.ARM9Write8(0x04000247, wramCnt);
        Savestate load(save.Buffer(), save.Length(), false);
        const bool loaded = !load.Error && n.DoSavestate(&load) && !load.Error;
        if (loaded)
        {
            n.ARM9Write8(0x04000247, wramCnt ^ 3);
            n.ARM9Write8(0x04000247, wramCnt);
        }
        if (!loaded)
        {
            if (log) fprintf(stderr, "[ffmusic] the state did not load\n");
            return false;
        }
#ifdef JIT_ENABLED
        // (A game run by the JIT, Black 2: its state has no instructions waiting in the sound
        // processor's pipeline, which the interpreter here runs from. Read again from where it is.)
        n.ARM7.FillPipeline();
#endif
        // The main processor stopped for good: halted, with no interrupt that could wake it.
        n.IME[0] = 0;
        n.IE[0] = 0;
        n.ARM9.Halt(1);
        n.GPU.GPU2D_A.CaptureCnt &= ~(1u << 31); // (no display capture: it would draw)
        // (What the sound chip had mixed but not handed out belongs to the game's own stream.)
        int16_t drop[2048];
        while (n.SPU.GetOutputSize() > 0 && n.SPU.ReadOutput(drop, 1024) > 0) {}
        for (uint32_t& c : cnt) c = 0;
        still = 0;
        frames = 0;
        playing = true;
        if (log) fprintf(stderr, "[ffmusic] begin (%u bytes of state)\n", (unsigned)save.Length());
        return true;
    }

    // One more frame (a 60th of a second) of the copy: its sound, stereo, into out (at most
    // maxFrames). Returns the frames written. The same thread as Begin.
    int Frame(int16_t* out, int maxFrames)
    {
        using namespace melonDS;
        if (!playing || !copy) return 0;
        NDS& n = *copy;
        for (int i = 0; i < 16 && !(n.ARM9Read16(0x04000184) & 0x100); i++) n.ARM9Read32(0x04100000);
        // (The flag in the shared work area: bit 0 of 0x027FFF96, which Black and its sequels
        // address as 0x02FFFF96. The same byte in a DS's 4 MB; not with the larger main memory
        // Black 2 and White 2 are given here (bw2map.h), where it is the second.)
        const u32 flagAt = n.MainRAMMask + 1 > 0x400000 ? 0x02FFFF96 : 0x027FFF96;
        const u16 flag = n.ARM7Read16(flagAt);
        if (flag & 1) n.ARM7Write16(flagAt, flag & ~1);
        const bool hidden = rc_frame_hidden, skip = rc_wide_render_skip;
        rc_frame_hidden = rc_wide_render_skip = true; // (nobody sees its screens)
        n.RunFrame();
        rc_frame_hidden = hidden;
        rc_wide_render_skip = skip;
        int got = 0;
        const int avail = n.SPU.GetOutputSize();
        if (avail > 0) got = n.SPU.ReadOutput(out, avail < maxFrames ? avail : maxFrames);
        // (A driver that is sequencing writes its channels every few milliseconds: their control
        // words standing still while one plays means it has stopped, and the channels loop what
        // they hold, a buzz.)
        bool changed = false, sounding = false;
        for (int ch = 0; ch < 16; ch++)
        {
            const uint32_t c = n.ARM7Read32(0x04000400 + ch * 16);
            changed |= c != cnt[ch];
            sounding |= (c >> 31) != 0;
            cnt[ch] = c;
        }
        still = changed || !sounding ? 0 : still + 1;
        frames++;
        if (log && frames % 120 == 0)
        {
            double sq = 0;
            for (int i = 0; i < got * 2; i++) sq += (double)out[i] * out[i];
            fprintf(stderr, "[ffmusic] frame %u: %d samples, level %.0f, still %d\n", frames, got, got ? sq / (got * 2) : 0.0, still);
            fprintf(stderr, "[ffmusic] arm7 pc %08X lr %08X halted %d ime %X ie %08X if %08X | arm9 halted %d stop %X | ipcsync %04X fifocnt %04X flag %04X | ts9 %llu ts7 %llu\n",
                n.ARM7.R[15], n.ARM7.R[14], (int)n.ARM7.Halted, n.IME[1], n.IE[1], n.IF[1], (int)n.ARM9.Halted, n.CPUStop,
                n.ARM7Read16(0x04000180), n.ARM7Read16(0x04000184), n.ARM7Read16(flagAt),
                (unsigned long long)n.ARM9Timestamp, (unsigned long long)n.ARM7Timestamp);
            if (frames == 120)
            {
                fprintf(stderr, "[ffmusic] mask %08X flags %04X %04X cpsr %08X r:", n.MainRAMMask, n.ARM7Read16(0x027FFF96), n.ARM7Read16(0x02FFFF96), n.ARM7.CPSR);
                for (int r = 0; r < 14; r++) fprintf(stderr, " %08X", n.ARM7.R[r]);
                fprintf(stderr, "\n[ffmusic] code before lr:");
                for (u32 a = (n.ARM7.R[14] & ~1u) - 40; a < (n.ARM7.R[14] & ~1u) + 8; a += 2) fprintf(stderr, " %04X", n.ARM7Read16(a));
                fprintf(stderr, "\n[ffmusic] bios at pc:");
                for (u32 a = (n.ARM7.R[15] & ~3u) - 24; a < (n.ARM7.R[15] & ~3u) + 12; a += 4) fprintf(stderr, " %08X", n.ARM7Read32(a));
                fprintf(stderr, "\n");
            }
        }
        return got;
    }

    // The game's volume for each song's player (SNDPlayer.extFader, 2 bytes at +6 of the sound
    // processor's player, game_music.h) given to the copy's: the game fades a song in and out
    // from its main processor, which the copy has not got. livePlayers: the game's 16 players.
    void MirrorVolumes(melonDS::NDS& live, const uint8_t* livePlayers, uint32_t musicPlayers)
    {
        using namespace melonDS;
        if (!playing || !copy || !livePlayers) return;
        NDS& n = *copy;
        const struct { const u8* from; u8* to; size_t size; } where[] = {
            { live.ARM7WRAM, n.ARM7WRAM, ARM7WRAMSize }, { live.SharedWRAM, n.SharedWRAM, SharedWRAMSize },
            { live.MainRAM, n.MainRAM, (size_t)live.MainRAMMask + 1 } };
        for (const auto& w : where)
        {
            if (!w.from || !w.to || livePlayers < w.from || livePlayers + 16 * 0x24 > w.from + w.size) continue;
            u8* mine = w.to + (livePlayers - w.from);
            for (int p = 0; p < 16; p++)
                if (musicPlayers >> p & 1) memcpy(mine + p * 0x24 + 6, livePlayers + p * 0x24 + 6, 2);
            return;
        }
    }

    void End()
    {
        if (playing && log) fprintf(stderr, "[ffmusic] end after %u frames\n", frames);
        playing = false;
    }
};
