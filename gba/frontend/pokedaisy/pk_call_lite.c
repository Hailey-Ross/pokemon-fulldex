// FAST FORWARD MUSIC's hidden copy of the game (ff_music_gba.h): parking its main loop and calling the game's own
// m4aSongNumStart on it. Adapted from PokeDaisy by Lidor Itzhari - https://github.com/lidor30/pokedaisy (pk_call.c,
// pokedaisy_jni.c's pkRenderPark; GPL 3). C, as mGBA's ARM headers are.
#include "pk_call_lite.h"

#include <string.h>

#include <mgba/core/core.h>
#include <mgba/internal/arm/arm.h>
#include <mgba/internal/arm/isa-inlines.h>
#include <mgba/internal/gba/gba.h>

int pk_lite_park(struct mCore* core, uint32_t spin) {
    struct ARMCore* cpu = (struct ARMCore*) core->cpu;
    for (int i = 0; i < 5000000; i++) {
        if ((cpu->privilegeMode == MODE_SYSTEM || cpu->privilegeMode == MODE_USER) && !cpu->cpsr.i && !cpu->halted
            && (core->busRead16(core, 0x04000208) & 1) && (core->busRead16(core, 0x04000200) & 1)
            && (core->busRead16(core, 0x04000004) & 8)) {
            cpu->gprs[ARM_PC] = (int32_t) (spin & ~1u);
            _ARMSetMode(cpu, MODE_THUMB);
            ThumbWritePC(cpu);
            return 1;
        }
        core->step(core);
    }
    return 0;
}

#define PK_CALL_SENTINEL 0x080000C0u

static int callRaw(struct mCore* core, uint32_t fn, const uint32_t args[4], uint32_t* r0);

static int parked(struct mCore* core) {
    struct ARMCore* cpu = (struct ARMCore*) core->cpu;
    uint32_t pc = (uint32_t) cpu->gprs[ARM_PC];
    // (Right after the branch the PC is the `b .`'s own address + 2, mid-way through it + 4.)
    return cpu->executionMode == MODE_THUMB && (core->rawRead16(core, pc - WORD_SIZE_THUMB * 2, -1) == 0xE7FE
                                                || core->rawRead16(core, pc - WORD_SIZE_THUMB, -1) == 0xE7FE);
}

int pk_lite_call(struct mCore* core, uint32_t fn, const uint32_t args[4]) {
    if (!parked(core)) return 0;   // only on the parked `b .`
    return callRaw(core, fn, args, NULL);
}

// The running game's own functions (GBA field actions: FLY), between frames. A frame often ends part-way through the
// game's own code (its VBlank handler, a lag frame's logic); a call there lands in the middle of the game's own
// read-modify-writes. So calls go ahead only when the game waits for VBlank with nothing of its own in progress: not
// in an interrupt, its System stack at the main loop's depth (the depth most frames end at, counted per game).
#define PK_REST_SLOTS 8
static uint32_t restSp[PK_REST_SLOTS], restCount[PK_REST_SLOTS];

static uint32_t sysSp(struct ARMCore* cpu) {
    return cpu->privilegeMode == MODE_SYSTEM || cpu->privilegeMode == MODE_USER
        ? (uint32_t) cpu->gprs[ARM_SP] : (uint32_t) cpu->bankedRegisters[BANK_NONE][0];
}

void pk_game_rest_reset(void) {
    memset(restSp, 0, sizeof(restSp));
    memset(restCount, 0, sizeof(restCount));
}

void pk_game_rest_note(struct mCore* core) {
    struct ARMCore* cpu = (struct ARMCore*) core->cpu;
    if (cpu->privilegeMode == MODE_IRQ) return;
    uint32_t sp = sysSp(cpu);
    int low = 0;
    for (int i = 0; i < PK_REST_SLOTS; i++) {
        if (restSp[i] == sp) {
            if (restCount[i] < 100000) restCount[i]++;
            return;
        }
        if (restCount[i] < restCount[low]) low = i;
    }
    restSp[low] = sp;
    restCount[low] = 1;
}

int pk_game_call_safe(struct mCore* core) {
    struct ARMCore* cpu = (struct ARMCore*) core->cpu;
    if (parked(core)) return 1;
    if (cpu->privilegeMode == MODE_IRQ) return 0;
    int best = 0;
    for (int i = 1; i < PK_REST_SLOTS; i++) if (restCount[i] > restCount[best]) best = i;
    uint32_t rest = restCount[best] >= 30 ? restSp[best] : 0;
    return !rest || sysSp(cpu) == rest;
}

int pk_game_call(struct mCore* core, uint32_t fn, const uint32_t args[4], uint32_t* r0) {
    if (!pk_game_call_safe(core)) return 0;
    return callRaw(core, fn, args, r0);
}

static int callRaw(struct mCore* core, uint32_t fn, const uint32_t args[4], uint32_t* r0) {
    struct ARMCore* cpu = (struct ARMCore*) core->cpu;
    struct ARMRegisterFile saved = cpu->regs;
    uint32_t prefetch0 = cpu->prefetch[0], prefetch1 = cpu->prefetch[1];
    struct ARMMemory mem = cpu->memory;
    struct GBA* gba = (struct GBA*) core->board;
    int region = gba->memory.activeRegion;
    uint32_t biosPrefetch = gba->memory.biosPrefetch;
    uint32_t idleLoop = gba->idleLoop, lastJump = gba->lastJump;
    bool haltPending = gba->haltPending;
    int idleStep = gba->idleDetectionStep, idleFailures = gba->idleDetectionFailures;
    int32_t cached[16];
    memcpy(cached, gba->cachedRegisters, sizeof(cached));
    enum ExecutionMode exec = cpu->executionMode;
    int halted = cpu->halted;
    enum PrivilegeMode priv = cpu->privilegeMode;
    cpu->halted = 0;
    cpu->cpsr.i = 1;
    if (priv != MODE_SYSTEM && priv != MODE_USER) {
        ARMSetPrivilegeMode(cpu, MODE_SYSTEM);
        cpu->cpsr.priv = MODE_SYSTEM;
    }
    for (int i = 0; i < 4; i++) cpu->gprs[i] = (int32_t) args[i];
    cpu->gprs[ARM_LR] = (int32_t) (PK_CALL_SENTINEL | 1);
    cpu->gprs[ARM_PC] = (int32_t) (fn & ~1u);
    _ARMSetMode(cpu, MODE_THUMB);
    ThumbWritePC(cpu);
    int ok = 0;
    for (long i = 0; i < 2000000; i++) {
        if (cpu->executionMode == MODE_THUMB && (uint32_t) cpu->gprs[ARM_PC] - WORD_SIZE_THUMB == PK_CALL_SENTINEL) {
            ok = 1;
            break;
        }
        core->step(core);
    }
    if (r0) *r0 = (uint32_t) cpu->gprs[0];
    cpu->regs = saved;
    cpu->privilegeMode = priv;
    cpu->executionMode = exec == MODE_ARM ? MODE_THUMB : MODE_ARM;
    _ARMSetMode(cpu, exec);
    cpu->prefetch[0] = prefetch0;
    cpu->prefetch[1] = prefetch1;
    cpu->memory = mem;
    gba->memory.activeRegion = region;
    gba->memory.biosPrefetch = biosPrefetch;
    gba->idleLoop = idleLoop;
    gba->lastJump = lastJump;
    gba->haltPending = haltPending;
    gba->idleDetectionStep = idleStep;
    gba->idleDetectionFailures = idleFailures;
    memcpy(gba->cachedRegisters, cached, sizeof(cached));
    cpu->halted = halted;
    return ok;
}

uint32_t pk_lite_where(struct mCore* core, int* thumb, int* mode, int* halted) {
    struct ARMCore* cpu = (struct ARMCore*) core->cpu;
    *thumb = cpu->executionMode == MODE_THUMB;
    *mode = (int) cpu->privilegeMode;
    *halted = cpu->halted;
    return (uint32_t) cpu->gprs[ARM_PC];
}
