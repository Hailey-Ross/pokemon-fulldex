// FAST FORWARD MUSIC's hidden copy of the game: park its main loop on a Thumb `b .` at spin, and call a Thumb function
// of the game with r0-r3 = args on it (pk_call_lite.c). 1 on success.
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct mCore;
int pk_lite_park(struct mCore* core, uint32_t spin);
int pk_lite_call(struct mCore* core, uint32_t fn, const uint32_t args[4]);
// (Where the CPU is: its PC, whether in Thumb, its mode and whether halted; for the log.)
/** The running game's function [fn] (Thumb) with r0-r3 = [args], between frames, only when the game waits for VBlank
 * with nothing in progress (pk_game_call_safe; after pk_game_rest_note has seen a second of frames). 1 = called. */
int pk_game_call(struct mCore* core, uint32_t fn, const uint32_t args[4], uint32_t* r0);
int pk_game_call_safe(struct mCore* core);
/** After every frame: the main loop's waiting depth. Reset when a game loads. */
void pk_game_rest_note(struct mCore* core);
void pk_game_rest_reset(void);
uint32_t pk_lite_where(struct mCore* core, int* thumb, int* mode, int* halted);
#ifdef __cplusplus
}
#endif
