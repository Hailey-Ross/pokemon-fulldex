// Running the DS game's own functions (the GBA's pk_call, for a DS game): a job of ops - calls, loads, stores - that
// the hook (stub/pk_nds_hook.c, in the ITCM) runs from the game's main loop the next frame, between two of the game's
// own calls. melonDS has no way to call into the game mid-frame, and the field's state machines run several steps per
// call, so a job is a whole recipe that ends with the game in a new state (a script started, a warp set). Only while
// the field is up and free (no task, no menu, no battle), like a press of A on a PC.
#pragma once

#include <stdint.h>

#include "pk_nds_call_abi.h"
#include "pk_nds_field.h"

namespace melonDS { class NDS; }

/** What a job is for; each builds its own ops (pk_nds_call.cpp). */
enum PkNdsAction {
    PK_NDS_ACTION_PC = 1,   // the Pokémon Storage System's menu, as a Pokémon Center PC opens it
    /** FLY's checks, the party menu's own: result 1 = FieldMoves_CheckFly's error (0 = may fly from here), results
     * 2 .. 1 + arg0 = whether fly spot 1 .. arg0 is unlocked (CheckFlyLocationUnlocked). */
    PK_NDS_ACTION_FLY_CHECK = 2,
    /** FLY: arg0 = the party slot of the Pokémon that flies, arg1 = the destination's map header, arg2 / arg3 = its
     * x / z (the spawn's fly tile: GetSpawnIdByMapAndCoords finds it again). The game's own FieldMoves_FlyTask from
     * there: the cut-in, the flight, the landing; and the journal's "flew to" line. */
    PK_NDS_ACTION_FLY = 3,
};

/** Posts [action] for the next frame; 0 when the field isn't free (or this cart has no recipe), else the job's number. */
int pk_nds_call_action(melonDS::NDS* nds, const PkNdsFieldGame* game, int action, const uint32_t args[4]);

/** Any ops (headless tools: nds_dump's call), on the same terms as an action. */
int pk_nds_call_ops(melonDS::NDS* nds, const PkNdsFieldGame* game, const PkNdsOp* ops, int n);

/** A field script of a tool's (assembled like stub/platinum_pc.s) started as the PC action starts its own. */
int pk_nds_call_script(melonDS::NDS* nds, const PkNdsFieldGame* game, const uint8_t* bytes, uint32_t len);

/** Bytes into the hook's spare ITCM (from PK_NDS_SCRIPT_ADDR up): what a tool's call points the game at. */
void pk_nds_call_itcm_write(melonDS::NDS* nds, uint32_t addr, const void* src, uint32_t len);

/** 1 once job [seq] ran, 0 while it waits, -1 when it never will (the field changed under it: the hook was taken out). */
int pk_nds_call_status(int seq);

/** Job [seq]'s results once it ran (PK_NDS_MAX_RESULTS words: op i's into [i]); false if it hasn't or isn't the last. */
bool pk_nds_call_results(int seq, uint32_t* out);

/** After every frame: notices a job that ran and takes out a hook the game hasn't called within a few frames. */
void pk_nds_call_frame(melonDS::NDS* nds, const PkNdsFieldGame* game);

/** Forget any job (a reset, a state load: the hook went with the old RAM, or is in the new RAM and will run). */
void pk_nds_call_reset(void);
