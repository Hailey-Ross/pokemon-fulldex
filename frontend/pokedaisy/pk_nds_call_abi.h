// What pk_nds_call.cpp (the host) and stub/pk_nds_hook.c (inside the game) agree on: where the hook and its job sit in
// the ARM9's ITCM and the job's layout. Plain C, both sides.
#pragma once

/** The hook's code: the top 16 KB of the ITCM, which Platinum never uses (its ITCM ends at 0x01FF8660; checked empty on
 * the field, in menus and in battle). */
#define PK_NDS_HOOK_ADDR 0x01FFC000u
#define PK_NDS_JOB 0x01FFC400u
/** A script of ours the game's script engine reads straight from here (ScriptContext_Jump). */
#define PK_NDS_SCRIPT_ADDR 0x01FFD000u
#define PK_NDS_SCRIPT_MAX 0x1000u

#define PK_NDS_MAX_OPS 24
#define PK_NDS_MAX_RESULTS 32
/** An argument of PK_NDS_RESULT_REF + i is op i's result. */
#define PK_NDS_RESULT_REF 0xFFFFFFE0u

/** Op kinds; anything else is a function's address (Thumb with bit 0 set), called with the four arguments. */
#define PK_NDS_OP_END 0u
#define PK_NDS_OP_STORE8 1u    // *(u8*) (a0 + a1) = a2
#define PK_NDS_OP_STORE16 2u
#define PK_NDS_OP_STORE32 3u
#define PK_NDS_OP_LOAD32 4u    // result = *(u32*) (a0 + a1)

typedef struct PkNdsOp {
    unsigned int kind;
    unsigned int a[4];
} PkNdsOp;

/** Gen 5's field-skill helper (stub/pk_nds_skill.c): its code, then its PkNdsSkill, in the spare ITCM (Black has no
 * scripts of ours to put there). */
#define PK_NDS_HELPER_ADDR 0x01FFD000u
#define PK_NDS_SKILL_ADDR 0x01FFD400u

/** The helper's arguments: the game's three field-skill functions, the skill, its use parameters (FLDSKILL's: the party
 * slot, the skill, a word - FLY's zone) and the check work the game fills (0x14 bytes in Black). */
typedef struct PkNdsSkill {
    unsigned int initCheck;   // (fieldMap, check*)
    unsigned int canUse;      // (skill, check*): 0 = may
    unsigned int use;         // (skill, param*, check*): the skill's event
    unsigned int skill;
    unsigned short paramSlot, paramSkill;
    unsigned int paramArg;
    unsigned int check[8];
} PkNdsSkill;

typedef struct PkNdsJob {
    unsigned int seq;    // the host's number for this job
    unsigned int done;   // the hook sets it to seq once the ops ran
    unsigned int slot;   // the function pointer the hook replaced
    unsigned int orig;   // its value, put back first thing
    unsigned int res[PK_NDS_MAX_RESULTS];
    PkNdsOp ops[PK_NDS_MAX_OPS];
} PkNdsJob;
