/**
 * ROTOR RESCUE - game rules, plain portable C (by Paul Ross Seibert).
 * (c) 2026 Desert Palms Software.
 *
 * Nothing in logic.c may include ACE or Amiga headers: it is compiled for the
 * Amiga *and* for the host so `agk unit` can test it in milliseconds.
 * Deterministic (a seeded LFSR, no timers) and 16-bit maths in per-frame code:
 * the 68000 has no 32-bit multiply/divide instruction.
 *
 * Coordinates: x is world pixels (0..WORLD_W-1), y is screen pixels
 * (0..255, the HUD takes 0..PLAY_TOP-1). The camera (cam) is the world x
 * at the screen's left edge.
 */
#ifndef _LOGIC_H_
#define _LOGIC_H_

#include <stdint.h>
#include "hiscore.h"   // the kit's hall of fame (the table, the initials editor)

// ------------------------------------------------------------- the world ---
#define SCREEN_W 320
#define SCREEN_H 256
#define PLAY_TOP 24          // the HUD is above
#define WORLD_W 3072
#define GROUND_Y 236         // the chopper's skids when landed
#define FEET_Y 240           // where people and tanks stand
#define CEIL_Y (PLAY_TOP + 20) // highest the skids can go

#define BASE_PAD_X0 2800     // landing here unloads
#define BASE_PAD_X1 2940
#define HANGAR_X 2960        // hangar's left edge: rescued people run in here
#define BORDER_X 2560        // the fence: enemy ground units stay left of it
#define START_X 2880

#define BARRACKS_COUNT 4
#define HOSTAGES_PER_BARRACKS 16
#define HOSTAGE_COUNT (BARRACKS_COUNT * HOSTAGES_PER_BARRACKS)
#define CABIN_MAX 16
#define LIVES_START 3
#define MISSION_COUNT 3      // missions a world (world 1: missions 1-3, world 2: 4-6)

// The worlds (SPEC 4): g->world 0 is the Border Desert (missions 1-3, WORLD_W),
// 1 the Coral Islands (missions 4-6, W2_W). g->mission is the mission within the
// world (0-2), so every per-mission table keeps three rows. World 2 has its
// islands, pads, barracks, water and boats (SPEC 4.4, 6; docs/STYLE-world2.md);
// the mission after world 1's third is its first. MISSIONS_TOTAL: missions 1-6.
#define WORLD_COUNT 2
#define MISSIONS_TOTAL (WORLD_COUNT * MISSION_COUNT)
#define W2_W 4608            // 288 tile columns of 16 px
#define W2_START_X 4136      // on the base island's pad (art/world2/tilemap.txt: pad column 256, 80 px)
#define W2_MAP_COLS (W2_W / 16)

// World 2's islands (art/world2/tilemap.txt, "island" lines): the span of the land (a
// whole number of 16 px tile columns; everything else is water), the barracks'
// centre (the four flat tiles it stands on), the pad's centre and the widest pad its
// bed takes (px). The last is the base island: pad, then the hangar; no barracks.
#define W2_ISLANDS 5
#define W2_BASE_ISLAND 4
#define W2_HANGAR_X 4212     // the hangar's left edge on the base island, just past the widest pad's zone (the people run in here)
typedef struct { int16_t x0, x1, barracksX, padX; uint8_t bedPx; } tIsland;
extern const tIsland g_pIslands[W2_ISLANDS];
// Pads (SPEC 4.4): the drawn pad is 32 + 16 n px (n 1-4: two 8 px end caps, the 16 px H,
// n 8 px units a side), the widest the skill and the island's bed allow: 96 / 80 / 64 /
// 56 / 48 by skill, so 96 / 96 / 80 / 64 / 48 on a wide bed. A landing counts within
// PAD_TOLERANCE px outside its edge; on skill 1-3 the last PAD_ASSIST_PX of a descent
// nudges the chopper 0.5 px a frame toward the centre
#define PAD_TOLERANCE 6
#define PAD_ASSIST_PX 12
#define PAD_ASSIST_REACH 20  // px past the tolerance the nudge still works
// How far the people hear a landed chopper in world 2 (their pad is up to 192 px from the barracks)
#define W2_CALL_RANGE 160
// How many of a barracks' people are out in the open at once in world 2: the rest wait inside and come
// out as those boarded or killed leave the list. Every person on screen costs about 1.5 % of an A500
// frame (draw, erase, update, the radar dot), and 16 on a beach beside a pad dropped frames.
#define W2_OUT_MAX 6

#define TANK_MAX 5
#define JET_MAX 2
#define MINE_MAX 3
#define SHOT_MAX 3           // the player's
#define ESHOT_MAX 6          // enemy shells, missiles, bombs
#define TANK_QUOTA 10       // tank kills that score, per mission: later ones pay nothing (no farming)
#define QUOTA_MSG_FRAMES 200 // the HUD's QUOTA line, after the last scoring kill
#define BOOM_MAX 6

// --------------------------------------------------------------- the BOBs ---
// Sizes must match the art (main.c checks at startup).
// (the order is the art block's and the blit templates' (main.c): the BOBs both worlds
// use first, then world 1's own, whose chip and templates world 2's set takes over,
// then world 2's)
enum {
	BOB_HOSTAGE, BOB_SHOT, BOB_BOMB, BOB_SHELL, BOB_BOOM, BOB_SHADOW, BOB_SMOKE, BOB_HANGAR,
	BOB_FENCE, BOB_BARRACKS, BOB_TANK, BOB_JET, BOB_MINE, BOB_FLAG,
	BOB_W2_BARRACKS, BOB_PATROL, BOB_GUNBOAT, BOB_PAD48, BOB_PAD64, BOB_PAD80, BOB_PAD96,
	BOB_PAD_LAMP, BOB_SPLASH,
	BOB_TYPES
};
#define BOB_W1_FIRST BOB_FENCE       // world 1's own BOBs: BOB_FENCE .. BOB_FLAG
#define BOB_W2_FIRST BOB_W2_BARRACKS // world 2's: BOB_W2_BARRACKS .. BOB_SPLASH
// The chopper's shadow on the track: frame = size * 2 + dither phase (x & 1),
// size 0 (on the ground) .. SHADOW_SIZES - 1 (high up)
#define SHADOW_Y (GROUND_Y + 1)
#define SHADOW_SIZES 4
extern const uint8_t g_pBobW[BOB_TYPES], g_pBobH[BOB_TYPES], g_pBobFrames[BOB_TYPES];

// Frames inside a BOB
enum { BARRACKS_OK, BARRACKS_HIT, BARRACKS_OPEN0, BARRACKS_OPEN1 };
enum { TANK_L0, TANK_L1, TANK_R0, TANK_R1, TANK_FIRE_L, TANK_FIRE_R,
	TANK_AIM_L, TANK_AIM_R, TANK_FLASH_L, TANK_FLASH_R };   // the tell: barrel up, a blinking flash
enum { MAN_STAND, MAN_RUN0, MAN_RUN1, MAN_WAVE0, MAN_WAVE1, MAN_DEAD,
	MAN_RUNL0, MAN_RUNL1 };
enum { SMOKE_SMALL, SMOKE_BIG, SMOKE_THIN };   // a puff's life, in thirds
enum { JET_L, JET_R, JET_FLASH_L, JET_FLASH_R };   // the tell: nose and engine glowing
// World 2's BOB frames: a boat faces left or right (drawn, not mirrored); the gunboat's tell
// is its barrel rising (TELL, then FIRE's flash, blinking), FIRE again for the shot's smoke; the
// patrol boat's (missions 5 and 6) is its bow gun up and its lamp lit, held until the shot
enum { PATROL_L, PATROL_R, PATROL_TELL_L, PATROL_TELL_R };
enum { GUNBOAT_L, GUNBOAT_TELL_L, GUNBOAT_FIRE_L, GUNBOAT_R, GUNBOAT_TELL_R, GUNBOAT_FIRE_R };
enum { PAD_LAMP_RED, PAD_LAMP_YELLOW };                  // off the pad, over it (no green in PF1)
// A pad is one BOB a width (BOB_PAD48 + (px - 48) / 16: PAD_ROWS tall, an end cap with its pylon, the
// units with their chevrons, the H, the other cap: main.c puts it together at the card, padCompose); its
// slab's top row is PAD_Y, the cap's top PAD_SLAB_ROW rows above
#define PAD_ROWS 46
#define PAD_SLAB_ROW 30

// The chopper's sprite poses (art/chopper.txt), right-facing. Art frame =
// pose * 2 + rotor phase; main.c adds ART_CHOPPER_MIRROR for left-facing.
// CHOP_LEAN1 / CHOP_LEAN2: the front view banked a little / fully to the right
// (the right side low); with CHOP_LEFT, to the left.
enum { CHOP_SIDE0, CHOP_SIDE1, CHOP_SIDE2, CHOP_TURN, CHOP_FRONT, CHOP_WRECK,
	CHOP_LEAN1, CHOP_LEAN2, CHOP_POSES };
#define CHOP_LEFT 0x80       // flag in logicChopperFrame()'s result

// ------------------------------------------------------------- the input ---
typedef struct {
	int8_t dx, dy;       // -1, 0, 1
	uint8_t fire, fire2; // held
	uint8_t key;         // a raw keycode pressed this frame (the initials), 0: none
} tInput;

// --------------------------------------------------------------- objects ---
enum { FACE_LEFT, FACE_FRONT, FACE_RIGHT };

typedef struct {
	int16_t x, y;        // x = centre, y = skids
	int16_t vx, vy;      // 1/16 px per frame
	uint8_t fx, fy;      // sub-pixel accumulators (1/16)
	uint8_t face;        // FACE_*
	uint8_t faceTarget;  // where a turn is heading
	uint8_t turnTimer;   // frames left in the current turn step
	uint8_t turnHold;    // frames the stick has pushed against the facing
	int8_t holdDx;       // the stick direction that count is of
	uint8_t lastSide;    // FACE_LEFT or FACE_RIGHT: for fire2's cycle
	uint8_t landed;
	uint8_t alive;
	uint8_t crashTimer;  // falling / burning
	uint8_t damaged;     // the first hit: smoke, slower, until the home pad mends it
	uint8_t invuln;      // frames left in which nothing hurts it (and it blinks)
	uint8_t fireTimer;   // auto-fire
	uint8_t prevFire, prevFire2;
	int8_t dyHold;       // frames the stick has been held down (+) / up (-) with no fire (saturates)
	uint8_t stance;      // frames left of the stick's bomb stance (0: not in one)
	uint8_t leanTimer;   // frames left of the front-view lean after / before a turn
	uint8_t leanDir;     // FACE_LEFT or FACE_RIGHT: which way that lean banks
	uint8_t swayTimer;   // the front-facing hover's sway clock
	int8_t lean;         // the head-on strafe's lean: -2..2, one step at a time (negative: left)
	uint8_t leanStep;    // frames to the next step of it
} tChopper;

enum { HS_INSIDE, HS_FREE, HS_TO_CHOPPER, HS_ABOARD, HS_TO_HANGAR, HS_SAVED, HS_DEAD };

typedef struct {
	int16_t x;
	int8_t home;         // barracks index
	uint8_t state;
	uint8_t anim;
	uint8_t timer;       // dead: frames the body shows; free: wander time
	int8_t dir;          // wandering direction
} tHostage;

typedef struct {
	int16_t x;
	int16_t wx;          // where its people wander about: x, or in world 2 halfway to the pad
	uint8_t hp;          // 0 = open
	uint8_t released;    // how many have come out
	uint8_t timer;
} tBarracks;

// World 2's boats (SPEC 6): placed on a stretch of sea, they patrol it. A gunboat shells a low or
// landed chopper in range, after a tell, under the shared ground-threat rules; a patrol boat does
// the same from mission 5 (logicPatrolArmed), a little slower, and is unarmed in mission 4.
#define BOAT_MAX 6
enum { BOAT_PATROL, BOAT_GUN };
typedef struct {
	int16_t x, x0, x1;   // centre, and the stretch it patrols
	uint8_t kind, active, hp;
	int8_t dir;
	uint8_t tell;        // frames left in the wind-up before a shell (0: none)
	uint8_t fireAnim, cool, phase;
} tBoat;

typedef struct {
	int16_t x;
	uint8_t active, hp;
	int8_t dir;
	uint8_t cool, anim, fireAnim;
	uint8_t tell;        // frames left in the wind-up before a shell (0: none)
} tTank;

typedef struct {
	int16_t x, y;
	int8_t dir;
	uint8_t active, hp;
	uint16_t cool;       // frames to its next shot, or (inactive) to its next appearance
	uint8_t pass;        // passes made
	uint8_t seen;        // frames on screen this pass: no shot before JET_AIM frames
	uint8_t tell;        // frames left of the flash before a missile or bomb (0: none)
	uint8_t warn;        // frames left of its warning: no attack run before it is 0 (radar blip flashes)
} tJet;

typedef struct {
	int16_t x, y;
	uint8_t active, anim;
	uint8_t fx, fy;
} tMine;

enum { SHOT_CANNON, SHOT_BOMB };
enum { ESHOT_SHELL, ESHOT_MISSILE, ESHOT_BOMB };

typedef struct {
	int16_t x, y;
	int16_t vx, vy;      // 1/16 px per frame
	uint8_t fx, fy;
	uint8_t active, kind, life;
} tShot;

typedef struct {
	int16_t x, y;        // centre
	uint8_t active, frame, timer;
} tBoom;

// The damaged chopper's smoke trail: puffs that rise and thin out
#define SMOKE_MAX 8
#define SMOKE_LIFE 24        // frames
typedef struct {
	int16_t x, y;        // centre
	uint8_t active, age;
} tSmoke;

enum { GS_TITLE, GS_INTRO, GS_PLAY, GS_LOST, GS_CLEAR, GS_OVER, GS_VICTORY,
	GS_SCORES,   // the hall of fame: in the attract loop, or after a new entry
	GS_ENTRY };  // a new high score: the player's initials

// The hall of fame: HI_COUNT entries (the kit's hiscore technique), best first,
// saved to the floppy. In an entry, `reached` is the mission (1..MISSION_COUNT,
// MISSION_COUNT + 1 = all done) and `extra` the hostages saved over the game
// (the rank comes from it).

typedef struct {
	uint8_t state;
	uint16_t stateTimer;
	uint16_t frame;      // frames since boot
	uint16_t lfsr;
	uint8_t mission;     // 0..MISSION_COUNT-1, within the world
	uint8_t world;       // 0..WORLD_COUNT-1 (missions 1-3, 4-6)
	uint8_t lives;
	uint8_t trip;        // completed trips home this mission
	uint8_t aboard, saved, killed;  // this mission
	uint8_t tankKills;   // tanks destroyed this mission (the first TANK_QUOTA score)
	uint8_t quotaMsg;    // frames left of the HUD's QUOTA line
	uint16_t savedTotal;            // over the game
	uint32_t score;
	uint32_t hiScore;
	int16_t cam;
	int16_t camLead;     // look-ahead, 1/16 px (the camera is the chopper + this)
	int16_t camLeadV;    // its speed, 1/16 px per frame
	int16_t camStep;     // the camera's last step (px): steps change by 1 at most
	uint8_t camPan;      // a long pan (a respawn): brakes to arrive, no overshoot
	int16_t unloadTimer;
	uint16_t spawnTimer;
	uint8_t titleArmed;  // fire must be released before the title accepts it
	uint8_t difficulty;  // 0 (invincible) .. DIFFICULTY_MAX; kept from game to game
	uint8_t ghost;       // testing (Ctrl+L, G): nothing destroys the chopper, whatever the difficulty
	uint16_t jetAttacks; // jet attack runs started (a flash begun), for the pacing report
	uint16_t landFrames; // frames the chopper has been landed without a break (0: airborne)
	uint8_t airGrace;    // frames left of the grace after a take-off: no jet attack run starts
	uint8_t jetWait;     // frames until another jet may start its attack run (pairs)
	uint8_t jetLast;     // the jet that started the latest attack run
	uint8_t jetsTrip;    // jets that have appeared since the chopper last sat on the home pad (the trip's jet budget)
	uint16_t wxFall;     // the weather's slide, 1/4 px (wraps)
	int16_t wxDrift;     // its wind, 1/4 px, 0..4 * SCREEN_W - 1
	// the attract loop: title -> hall of fame -> a self-playing demo -> title
	uint16_t attract;    // frames on the title without a game
	uint8_t demo;        // the demo is playing (no scores kept, fire ends it)
	uint8_t demoRun, demoLeft;   // where it is in its input stream
	uint8_t demoDifficulty;      // the player's, put back after it
	uint8_t scoresNext;  // after the hall of fame: GS_INTRO (the demo) or GS_TITLE
	// the hall of fame, and a new entry
	tHiTable hi;
	tHiEdit edit;        // the initials being typed (edit.ubPos: the letter being chosen)
	uint8_t newRank;     // the entry being or just made (HI_COUNT: none)
	uint8_t scoresChanged;   // set when an entry is done: main.c saves the table
	tChopper chop;
	tHostage pHostages[HOSTAGE_COUNT];
	// The hostages out in the open (free, running, walking home, a body still
	// showing): the only ones per-frame code needs to look at
	uint8_t pOut[HOSTAGE_COUNT];
	uint8_t outCount;
	tBarracks pBarracks[BARRACKS_COUNT];
	tTank pTanks[TANK_MAX];
	tJet pJets[JET_MAX];
	tMine pMines[MINE_MAX];
	uint8_t padPx[W2_ISLANDS];   // each island's pad width by the skill (logicPadPx)
	tBoat pBoats[BOAT_MAX];
	uint8_t atkWait;     // frames until another boat may start a tell (one attack at a time, 3 s apart)
	uint8_t atkLast;     // the boat that started the latest one (it may go on by its own gap)
	uint8_t wet;         // the skids are in the water (a splash made; rises again when they lift)
	uint8_t splashTimer; // frames left of the splash drawn at splashX
	int16_t splashX;
	tShot pShots[SHOT_MAX];
	tShot pEShots[ESHOT_MAX];
	tBoom pBooms[BOOM_MAX];
	tSmoke pSmoke[SMOKE_MAX];
	uint8_t smokeTimer;  // frames to the next puff
	// the rotor's sound (logicRotorTick): Paula period, volume, and the take-off spin-up left
	uint16_t rotorPer;
	uint8_t rotorVol;
	uint8_t rotorBoost;  // frames of spin-up left after lift-off
	// events of the latest frame (sound, serial): bit flags
	uint16_t events;
	uint8_t events2;     // more event bits (EV2_*)
	uint8_t evBarracks;  // which barracks opened (index + 1)
} tGame;

// events bits
#define EV_CANNON    0x0001
#define EV_BOMB      0x0002
#define EV_BOOM      0x0004
#define EV_OPEN      0x0008
#define EV_BOARD     0x0010
#define EV_UNLOAD    0x0020
#define EV_CRASH     0x0040
#define EV_TANKFIRE  0x0080
#define EV_JET       0x0100
#define EV_LAND      0x0200
#define EV_SAVED     0x0400
#define EV_DIED      0x0800   // a hostage died
#define EV_STATE     0x1000   // game state changed
#define EV_TAKEOFF   0x2000
#define EV_SHRUG     0x4000   // invincible: a hit that would have destroyed the chopper
#define EV_TANKTELL  0x8000   // a tank started its wind-up (a shell follows)

#define EV2_JETTELL  0x01     // a jet started its flash (a missile or bomb follows)
#define EV2_JETFIRE  0x02     // a jet launched one
#define EV2_DAMAGE   0x04     // the chopper took its first hit
#define EV2_REPAIR   0x08     // landing on the pad mended it
#define EV2_JETWARN  0x10     // a jet appeared: the incoming-jet warning (a radar flash, an alert)
#define EV2_SPLASH   0x20     // the skids touched the water (world 2): a soft stop
#define EV2_BOATTELL 0x40     // a gunboat started its wind-up (a shell follows)
#define EV2_BOATFIRE 0x80     // a gunboat fired

// --------------------------------------------------------------- tuning ---
#define VX_MAX 48            // 3 px/frame
#define VX_ACCEL 2
#define VX_DRAG 1
// Head-on (facing the screen to bomb): a quick sidestep, Choplifter's jink -
// full speed in ~5 frames, a reversal in ~9, a stop in ~8 (side flight takes
// 24 / 48 / 48: a heavy machine cruising)
#define HO_VX_MAX 40         // 2.5 px/frame
#define HO_ACCEL 8
#define HO_REVERSE 12        // pushing against the drift
#define HO_DRAG 5
// Jets: heard and seen on the radar before they're on screen, and a beat to react
#define JET_MISSILE_SPEED 40 // 2.5 px/frame (80 = 5 before 2026-11-11; 6 before e46d2a1)
#define JET_MISSILE_LIFE 80  // frames: 200 px at 2.5
#define JET_GAP 40           // frames between missiles, the tell included (was 50 without a tell)
#define TANK_SHELL_SPEED 24  // 1.5 px/frame on the larger axis (2-4 before 2026-11-11)
#define SHELL_LIFE 140       // frames: 210 px at 1.5 (a tank fires from up to 200 px)
#define TANK_GAP_BASE 85     // frames between a tank's shells at mission 1, trip 0, normal (110 before)
#define JET_AHEAD_MIN 56     // no shot from closer than this ahead
// Jet fairness (Paul's playtest, 2026-11-11 - studio tuning, pending Paul's playtest; SPEC 5-7)
#define JET_WARN_FRAMES 100  // 2 s: from the jet's appearance (its warning) to the first moment it may start an attack run
#define JET_LEAD 384         // px off screen it appears: 100 frames at 4 px/frame later it is whole on screen
#define JET_LEAD_BEHIND 576  // px off screen a jet appears when it comes up BEHIND the chopper (flying the way it flies): 178 frames to a hovering chopper
#define JET_WARN_BEHIND 150  // 3 s: its warning, frames before it may start an attack run (time to turn and face it)
#define JET_TAKEOFF_GRACE 100  // 2 s after take-off before any jet may start an attack run
#define JET_PAIR_GAP 150     // 3 s between one jet's attack run starting and the next jet's
#define GROUND_NEVER 0xFFFF
#define VY_UP_MAX 28
#define VY_DOWN_MAX 32
#define VY_ACCEL 3
#define TURN_STEP_FRAMES 5   // the three-quarter view between a side and the front
#define LEAN_FRAMES 6        // the lean frame at the front, before / after a turn
#define LEAN_STEP_FRAMES 3   // the strafe's lean moves one pose (front, slight, full) this often
#define SWAY_PHASE 24        // front-facing hover: front, lean, front, lean, each this long
#define BOMB_ARM_FRAMES 20   // stick down this long with no fire: the next fire press is the bomb stance
#define BOMB_STANCE_FRAMES 75 // the stick's bomb stance turns back to the cannon this long after the last bomb
#define TURN_HOLD_FRAMES 5   // stick against the facing this long starts a turn
#define FIRE_REPEAT 10
#define CANNON_SPEED 96      // 6 px/frame
#define CANNON_LIFE 36
#define BOMB_GRAVITY 3
#define HOSTAGE_CALL_RANGE 112  // landed chopper calls people this close
#define UNLOAD_FRAMES 10
#define CRASH_FRAMES 70
// Damage (the hero device): the first hit costs smoke and a fifth of the speed,
// then INVULN_FRAMES (2 s) of blinking in which nothing hurts; a second hit while
// damaged destroys the chopper; landing on the pad mends it
#define INVULN_FRAMES 100
#define BLINK_FRAMES 4       // visible for this many frames, hidden for this many
#define DMG_VX_MAX 38        // 2.4 px/frame (VX_MAX 48 less 20 %)
#define DMG_HO_VX_MAX 32     // (HO_VX_MAX 40)
#define DMG_VY_UP_MAX 22     // (VY_UP_MAX 28)
#define DMG_VY_DOWN_MAX 26   // (VY_DOWN_MAX 32)
#define INTRO_FRAMES 120
#define LOST_FRAMES 150
#define CLEAR_FRAMES 250
#define OVER_FRAMES 300

// ------------------------------------------------------------ functions ---
void logicInit(tGame *g);
void logicStartGame(tGame *g);
void logicStartMission(tGame *g, uint8_t ubMission);
// A mission of a world at once (the debug keys: Ctrl+L, W, 1-6): from a menu it
// starts a game there; from play it moves to it (lives and score kept)
void logicStartAt(tGame *g, uint8_t ubWorld, uint8_t ubMission);
void logicUpdate(tGame *g, const tInput *pIn);
// The battlefield's width: WORLD_W, or W2_W in world 2
static inline int16_t logicWorldW(const tGame *g) { return g->world ? W2_W : WORLD_W; }

// The chopper on its own (exposed for unit tests)
void logicChopperUpdate(tGame *g, const tInput *pIn);
void logicChopperRespawn(tGame *g);

uint16_t logicRandom(tGame *g);

// Put hostage i out in the open, in state ubState at x (tests, and the game)
void logicHostageOut(tGame *g, uint8_t i, uint8_t ubState, int16_t x);

// Difficulty: 1 easiest .. 5 hardest, 3 normal (the default). 0 plays like
// 3, but nothing destroys the chopper and the hi-score isn't kept: for testing.
#define DIFFICULTY_INVINCIBLE 0
#define DIFFICULTY_NORMAL 3
#define DIFFICULTY_MAX 5
void logicSetDifficulty(tGame *g, uint8_t ubLevel);   // above DIFFICULTY_MAX: ignored
// The per-difficulty tables (DIFFICULTY_MAX + 1 entries of elem bytes each):
// for the unit test that GODMODE (0) plays like normal (3), table by table
typedef struct { const char *name; const void *p; uint8_t elem; } tDiffTable;
const tDiffTable *logicDiffTables(uint8_t *pCount);

// The enemy schedule for the current difficulty, mission and trip
uint8_t logicTankLimit(const tGame *g);
uint8_t logicJetsOn(const tGame *g);
uint8_t logicMinesOn(const tGame *g);
// Tanks: every shell comes after a tell - the tank stops, raises its barrel
// and its flash blinks for this many frames (16 on normal, 12 on the hardest)
uint8_t logicTankTell(const tGame *g);
// Jets: every missile or bomb comes after a flash of this many frames (the same
// table as the tank's: 16 on normal, 12 on the hardest), and only while the
// whole jet is on screen
uint8_t logicJetTell(const tGame *g);
// Gunboats (world 2): the barrel rises and the lamp flashes this many frames before every
// shell: 22 / 18 / 16 / 14 / 12 by skill (16 for GODMODE), longer than a tank's on the low skills
uint8_t logicBoatTell(const tGame *g);
// Patrol boats (mission 5 on): the same attack as a gunboat's, weaker: PATROL_TELL_EXTRA more frames
// of wind-up, PATROL_COOL_EXTRA more of rest, its bow gun a little lower; mission 4's are unarmed
uint8_t logicPatrolArmed(const tGame *g);   // mission 5 on, and mission 4 from skill 3
uint8_t logicBoatGap(const tGame *g);        // frames between a boat's shells (tell included): the tank's table, faster from skill 3
uint8_t logicPatrolTell(const tGame *g);
#define PATROL_TELL_EXTRA 4
#define PATROL_COOL_EXTRA 30
#define PATROL_MUZZLE_Y 233  // where a patrol boat's shell leaves (its bow gun), and PATROL_MUZZLE_DX from its centre
#define PATROL_MUZZLE_DX 19
#define BOAT_RANGE 220       // a gunboat's sight: no longer than its shell flies (SHELL_LIFE at 1.5 px a frame)
#define BOAT_MIN_RANGE 56    // and no shot from closer than this
#define BOAT_ATTACK_GAP 150  // 3 s between one boat's tell starting and the next boat's
#define BOAT_BOTTOM_Y 248    // the waterline: a boat's last row
#define BOAT_MUZZLE_Y 224    // where a gunboat's shell leaves
#define SPLASH_FRAMES 20     // the splash's life (5 frames of 4)
#define PAD_Y (GROUND_Y - 2) // a pad's slab, its top row (the skids rest on the second row below)
// Water (world 2): x is over open sea, not an island
uint8_t logicOnWater(int16_t x);
// The island whose pad x is over (the pad's width by skill, TOLERANCE px outside its edge), or
// 0xFF: the base island is W2_BASE_ISLAND
uint8_t logicPadAt(const tGame *g, int16_t x);
// The pad's drawn width, px: by the skill and the island's bed, worked out when a mission starts and
// when the skill changes (it is asked every frame, by the landing, the unloading and the drawing)
static inline uint8_t logicPadPx(const tGame *g, uint8_t ubIsland) { return g->padPx[ubIsland]; }
// World 2's hangar and base pad, or world 1's: where the people run to, and the unloading
static inline int16_t logicHangarX(const tGame *g) { return g->world ? W2_HANGAR_X : HANGAR_X; }
uint8_t logicOnBasePad(const tGame *g, int16_t x);
// Jets and a chopper on the ground: frames it must have sat landed (without a
// break) before a jet may bomb it; GROUND_NEVER on skill 1-2 (they overfly it)
uint16_t logicGroundDwell(const tGame *g);
// How many jets may be out at once: 1 on skill 1-2, and on 3 until mission 3; 2 above
uint8_t logicJetsMax(const tGame *g);
uint8_t logicJetsTripMax(const tGame *g);   // most jets that may appear in one trip (base to base)

// Drawing: what's on screen, back to front, in screen coordinates (top-left)
typedef struct {
	int16_t x, y;
	uint8_t bob, frame;
} tDraw;
#define DRAW_MAX 96
uint8_t logicDraw(const tGame *g, tDraw *pOut);
uint8_t logicChopperFrame(const tGame *g);   // CHOP_*, + 0x80 if facing left
uint8_t logicChopperVisible(const tGame *g);
uint8_t logicRotorPhase(const tGame *g);

// The rotor's sound: a loop of ROTOR_PASS-sample blade passes, one deep thump
// each, so the Paula period sets the thump rate: 5 a second at hover, 3.5 idling
// on the skids, 8-9 at full speed or climbing, and a louder, faster spin-up for
// two seconds after lift-off. logicUpdate slews both toward their targets
// (the pitch never jumps); main.c writes them to channel 2.
#define ROTOR_PASS 1200          // samples a blade pass in the loop
#define ROTOR_CLOCK 3546895L     // Paula's PAL clock: rate = clock / period
uint16_t logicRotorPeriod(const tGame *g);   // Paula period
uint8_t logicRotorVolume(const tGame *g);    // 0-64 (0 when the chopper is gone)
int logicRotorThumpsX10(uint16_t uwPeriod);  // thumps a second times ten

// Effects that can repeat (SPEC 9, rotor v3): main.c asks logicSfxAllow before it plays one, and
// the same effect is not played again inside its gap (logicSfxGap frames). pLast: SFXK_COUNT
// frame stamps (0 = never played), kept by the caller; uwNow: a frame count from 1.
enum { SFXK_BOOM, SFXK_JETWARN, SFXK_JET, SFXK_TANKFIRE, SFXK_SAVED, SFXK_BOARD, SFXK_LAND, SFXK_COUNT };
uint8_t logicSfxGap(uint8_t ubKind);
uint8_t logicSfxAllow(uint16_t *pLast, uint16_t uwNow, uint8_t ubKind);

// The camera follows the chopper with a look-ahead. It is locked to the
// chopper's pixel, so the chopper only moves on screen when the look-ahead
// does, and that glides (accelerates, cruises, slows) - never jumps.
#define CAM_LEAD 56          // px ahead of the facing
#define CAM_LEAD_VMAX 32     // look-ahead speed limit, 1/16 px a frame (2 px)
#define CAM_LEAD_ACCEL 2     // 1/16 px a frame per frame
#define CAM_PAN_FROM 32      // further than this from where it should be: a pan
#define CAM_PAN_VMAX 12      // px a frame
void logicCameraUpdate(tGame *g);
void logicCameraSnap(tGame *g);   // to the look-ahead's target at once (mission start)

// Scrolling maths for the copper (from examples/sidescroller):
// a band that moves at cam >> shift, looping every uwLoopW px
uint16_t logicBandOffset(int16_t wCam, uint8_t ubShift, uint16_t uwLoopW);
uint16_t logicScrollDelay(uint16_t uwX);      // BPLCON1 nibble for pixel uwX at the left edge
int16_t logicScrollByteOffset(uint16_t uwX);  // bitplane pointer offset, bytes
// The same in quarter pixels, for AGA (A1200): its BPLCON1 has 35 ns steps, a
// quarter of a lores pixel, so a band at camera / 4 or / 2 moves 0.75 or 1.5 px
// a frame instead of 0,1,1,1 or 1,2,1,2 (judder). x4 = the band's left edge in
// 1/4 px (0 .. 4 * uwLoopW - 1). The BPLCON1 value is PF2's: bits 7-4 the
// whole pixels, 13-12 the quarters (FMODE 0: a 16-px range, bits 15-14 unused).
uint16_t logicBandOffset4(int16_t wCam, uint8_t ubShift, uint16_t uwLoopW);
uint16_t logicScrollCon1Pf2Aga(uint16_t uwX4);
int16_t logicScrollByteOffset4(uint16_t uwX4);

// The plain (SPEC 8, decided by Paul, 2026-11-12): a true perspective floor
// between the mountains and the hills. Its far strip is the mountains band's
// last PLAIN_FAR_ROWS rows (lines FLOOR_Y - 10 .. FLOOR_Y - 1, camera / 4, one
// block: mesas may stand there). Below it FLOOR_LINES lines, each scrolled on
// its own by the copper: line j at (18 + j) / 72 of the camera, linear down the
// screen (on a flat plain 1/z is linear in screen y), from the mountains' 1/4
// to the hills' 1/2. Row j of the floor art loops every 8 (18 + j) px, its
// speed times FLOOR_PERIOD, so every row repeats after the same FLOOR_PERIOD px
// of camera and needs no wrap of its own (docs/plain-spec.md: the art contract).
#define PLAIN_FAR_ROWS 10
#define FLOOR_Y 180          // the floor's first game line (row 0: transparent, the haze)
#define FLOOR_LINES 19       // .. FLOOR_Y + 18 (row 18: transparent, C), then the hills
#define FLOOR_PERIOD 576     // camera px: 32 x (FLOOR_LINES - 1), so every row's loop is whole px
uint16_t logicFloorRowW(uint8_t j);                // row j's loop, px: 144 .. 288
uint16_t logicFloorX4(int16_t wCam, uint8_t j);    // row j's left edge, 1/4 px (0 .. 4 W - 1)
// Each floor line's copper values for this camera, written straight into the
// list: BPLCON1 (PF2's fine scroll, the quarters kept where uwX4Mask is
// 0xFFFF, AGA; 0xFFFC drops them, OCS) at pOut[j * ubStride], and the modulo
// that takes line j's PF2 fetch to row j + 1's start at pOut[j * ubStride + 2]
// (uwModBase = the bitmap's row bytes less the fetch). Returns row 0's byte
// offset (its bitplane pointers). Per frame: one 16 x 16 multiply, no divide.
int16_t logicFloorScroll(int16_t wCam, uint16_t uwX4Mask, uint16_t uwModBase, uint16_t *pOut, uint8_t ubStride);
// The plain's ground (COLOR00 under the floor), per line: from the sky's own
// colour at FLOOR_Y (the haze) to C, the land palette's pen 6 (line-dithered)
void logicFloorFillGround(uint8_t ubMission, uint16_t *pOut);
uint32_t logicFloorGround24(uint8_t ubMission, uint8_t j);   // the same in 24 bits, for AGA

// World 2's sea (docs/STYLE-world2.md section 2): the perspective floor re-cut to
// SEA_LINES lines, 180-207, line j at (28 + 3 j) / 112 of the camera (1/4 at the
// horizon's side to 0.97 at the strip), its row loops 4 (28 + 3 j) px, all
// repeating after SEA_PERIOD px of camera. Rows 0 and 27 are empty (the palette
// switches at lines 179 and 207). The same copper values as logicFloorScroll
// (pOut, ubStride, the BPLCON1 mask, the modulo base): per frame one 16 x 16
// multiply and no divide (line j's left edge in quarters, c + 3 c j / 28 with
// c = camera mod 448, is kept as a whole part and a remainder in 28ths).
#define SEA_Y 180
#define SEA_LINES 28
#define SEA_PERIOD 448
#define STRIP_Y (SEA_Y + SEA_LINES)   // 208: the terrain strip, 48 rows, 1:1
#define STRIP_ROWS (SCREEN_H - STRIP_Y)
int16_t logicSeaScroll(int16_t wCam, uint16_t uwX4Mask, uint16_t uwModBase, uint16_t *pOut, uint8_t ubStride);
uint16_t logicSeaX4(int16_t wCam, uint8_t j);   // (host reference) row j's left edge, 1/4 px
// A line-dithered COLOR00 gradient from uwA (line 0) to uwB (line ubLines - 1),
// as the sky's (startup), and its 24-bit form (0xRRGGBB) for AGA
void logicGradFill(uint16_t uwA, uint16_t uwB, uint8_t ubLines, uint16_t *pOut);
uint32_t logicGrad24(uint32_t ulA, uint32_t ulB, uint8_t ubLine, uint8_t ubLines);

// World 2's terrain (the hook for the gameplay slice): the floor under world x
// from the tile map (W2_MAP_COLS bytes, a tile index a 16 px column). Island tiles
// (flat, shore, palms, bush, rock, pad bed) are ground at GROUND_Y; water and reef
// (tiles 0-7, 32-33) are not landable: TERRAIN_WATER. Off the map: water.
#define TERRAIN_WATER 0xFF
uint8_t logicTerrainFloorY(const uint8_t *pMap, int16_t x);

// Radar: 0..RADAR_W-1 from a world x
#define RADAR_W 192
#define RADAR_X0 64
static inline uint8_t logicRadarX(int16_t x) { return (uint8_t)(x >> 4); }
// The same for world 2's 4608 px (x / 24, as a 16 x 16 multiply): x in 0..W2_W-1
static inline uint8_t logicRadarX2(int16_t x) { return (uint8_t)(((uint32_t)(uint16_t)x * 2731u) >> 16); }

// Mission palettes: sky gradient and band colours (copper)
#define SKY_LINES 172
uint16_t logicSkyColor(uint8_t ubMission, uint8_t ubLine);   // line 0 = PLAY_TOP
// The OCS sky, line-dithered (Fire & Ice): where the gradient sits between two
// 12-bit steps, a channel alternates line by line between them (A B A B, then
// B, then B C B C...), so the eye sees half steps: ~twice the shades for one
// MOVE a line. Within 1/4 step of a whole value a line is that value.
uint16_t logicSkyColorDither(uint8_t ubMission, uint8_t ubLine);
// All SKY_LINES of it at once, with no division (startup on a 68000: fast)
void logicSkyFillDither(uint8_t ubMission, uint16_t *pOut);
uint32_t logicSkyColor24(uint8_t ubMission, uint8_t ubLine); // the same in 24 bits (0xRRGGBB), for AGA
extern const uint16_t g_pMissionMtnPal[MISSION_COUNT][8];
extern const uint16_t g_pMissionLandPal[MISSION_COUNT][8];
extern const char *const g_pMissionName[MISSION_COUNT];

// The far sky: a PF2 cloud band (camera / 8, its palette per mission) and, in
// front of it, wisps (dusk, dawn) or stars (night) on sprite channel 6
// (colours 29-31), behind PF1, one under the other down its DMA list.
#define CLOUDS_Y 72          // the cloud band's first game line (art: 40 rows): low, and
                             // short - from here down it's 6-plane DMA on the A500
#define SKYDOT_COUNT 6
#define SKY_LOOP 512         // the dots repeat every 512 px
#define SKY_HIDDEN 0x7FFF    // off screen: park the sprite past the right edge
typedef struct { uint8_t y, h, shape; int16_t x; } tSkyItem;
extern const tSkyItem g_pSkyDots[SKYDOT_COUNT];   // y ascending, gaps >= 2 lines
extern const uint16_t g_pMissionCloudPal[MISSION_COUNT][8];
int16_t logicSkyDotX(const tGame *g, uint8_t i);  // screen x of the left edge, or SKY_HIDDEN
extern const uint16_t g_pMissionSkyDotPal[MISSION_COUNT][3];   // colours 29-31
uint8_t logicSkyDotsAreStars(uint8_t ubMission);
uint16_t logicStarTwinkle(const tGame *g);        // colour 31 at night this frame

// Weather, close to the camera (it scrolls 1:1 with the ground) on sprite
// channels 4-5, in front of everything: dust motes at dusk, rain at night, sea
// spray at dawn. Colours 25-27 (the chopper's pale blue, orange and yellow:
// a pair's colours are shared). Each channel's DMA list holds WX_SLOTS items
// one under the other, WX_PITCH lines apart (channel 1 half a pitch lower);
// the whole stack slides by off lines (falls, or rises), and every 16 lines
// (a wrap) each particle moves on to the next slot, so slot k shows particle
// (k - wraps) & (WX_PARTICLES - 1), at x = g_pWxX[channel][particle] + shift
// (less SCREEN_W past the right edge). Played only in GS_PLAY and GS_LOST.
enum { WX_DUST, WX_RAIN, WX_SPRAY };
#define WX_PARTICLES 16
#define WX_SLOTS 13
#define WX_PITCH 16
#define WX_H 15              // item height: a line free before the next one
#define WX_TOP PLAY_TOP
typedef struct {
	uint8_t off;         // 0..WX_PITCH-1: the stack's slide
	uint8_t wraps;       // (mod 256)
	int16_t shift;       // 0..SCREEN_W-1: wind and camera
} tWeatherFrame;
extern const int16_t g_pWxX[2][WX_PARTICLES];
uint8_t logicWeatherKind(uint8_t ubMission);
uint8_t logicWeather(const tGame *g, tWeatherFrame *pW);   // 0: none now

// Rank text for the end screen
const char *logicRank(uint16_t uwSaved);

// The attract loop and the hall of fame (scores are kept only on difficulties
// 1-5, and never from the demo)
#define ATTRACT_FRAMES 600   // idle on the title, then the hall of fame
#define SCORES_FRAMES 400    // the hall of fame shows this long
#define ENTRY_FRAMES HI_EDIT_FRAMES   // initials: 30 s (a key restarts it), then what's chosen stands
#define DEMO_LETTERS HI_LETTERS       // a name's letters, in stick order
void logicScoresDefault(tGame *g);
extern const tHiConfig g_sHiConfig;   // the file: "RRH1" (as before the kit), "rrscores"
void logicStartDemo(tGame *g);
uint8_t logicDemoInput(tGame *g, tInput *pOut);             // 0: the stream is over
uint8_t logicIsMenu(const tGame *g);                        // the title or the hall of fame

// The preview card before each mission (GS_INTRO, fire skips it): its one-line
// brief - what's out there this time - and a strip map of the battlefield,
// world x / ROUTE_SCALE, with a "you are here" marker at the chopper.
const char *logicBrief(const tGame *g);
#define ROUTE_SCALE 12
#define ROUTE_W (WORLD_W / ROUTE_SCALE)   // 256 px
// (World 2's map is world x / 18: ROUTE_ISLAND the land)
enum { ROUTE_ENEMY, ROUTE_HOME, ROUTE_FENCE, ROUTE_BARRACKS, ROUTE_PAD, ROUTE_HANGAR, ROUTE_HERE,
	ROUTE_ISLAND };
typedef struct { uint8_t kind, x, w; } tRouteMark;   // x, w in map px
#define ROUTE_MARKS (6 + 4 * BARRACKS_COUNT)
uint8_t logicRouteMap(const tGame *g, tRouteMark *pOut);   // back to front

#endif // _LOGIC_H_
