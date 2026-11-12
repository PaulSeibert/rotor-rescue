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
#define MISSION_COUNT 3

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
enum {
	BOB_HANGAR, BOB_FENCE, BOB_BARRACKS, BOB_TANK, BOB_HOSTAGE, BOB_JET,
	BOB_MINE, BOB_SHOT, BOB_BOMB, BOB_SHELL, BOB_BOOM, BOB_FLAG, BOB_SHADOW,
	BOB_SMOKE,
	BOB_TYPES
};
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

// The chopper's sprite poses (art/chopper.txt), right-facing. Art frame =
// pose * 2 + rotor phase; main.c adds ART_CHOPPER_MIRROR for left-facing.
enum { CHOP_SIDE0, CHOP_SIDE1, CHOP_SIDE2, CHOP_TURN, CHOP_FRONT, CHOP_WRECK,
	CHOP_POSES };
#define CHOP_LEFT 0x80       // flag in logicChopperFrame()'s result

// ------------------------------------------------------------- the input ---
typedef struct {
	int8_t dx, dy;       // -1, 0, 1
	uint8_t fire, fire2; // held
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
	uint8_t lastSide;    // FACE_LEFT or FACE_RIGHT: for fire2's cycle
	uint8_t landed;
	uint8_t alive;
	uint8_t crashTimer;  // falling / burning
	uint8_t damaged;     // the first hit: smoke, slower, until the home pad mends it
	uint8_t invuln;      // frames left in which nothing hurts it (and it blinks)
	uint8_t fireTimer;   // auto-fire
	uint8_t prevFire, prevFire2;
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
	uint8_t hp;          // 0 = open
	uint8_t released;    // how many have come out
	uint8_t timer;
} tBarracks;

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

// The hall of fame: SCORE_COUNT entries, best first, saved to the floppy
#define SCORE_COUNT 5
#define SCORE_NAME 3         // initials
typedef struct {
	char name[SCORE_NAME];
	uint8_t mission;     // reached: 1..MISSION_COUNT, MISSION_COUNT + 1 = all done
	uint16_t saved;      // hostages saved over the game (the rank comes from it)
	uint32_t score;
} tScore;
#define SCORES_BYTES (4 + SCORE_COUNT * 10 + 2)   // what's on the disk: magic, entries, checksum

typedef struct {
	uint8_t state;
	uint16_t stateTimer;
	uint16_t frame;      // frames since boot
	uint16_t lfsr;
	uint8_t mission;     // 0..MISSION_COUNT-1
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
	tScore pScores[SCORE_COUNT];
	uint8_t newRank;     // the entry being or just made (SCORE_COUNT: none)
	uint8_t entryPos;    // its letter being chosen (0..SCORE_NAME-1)
	uint8_t entryHold;   // the stick held up/down: frames to the next letter
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
#define TURN_STEP_FRAMES 6   // side -> front, front -> side
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
void logicUpdate(tGame *g, const tInput *pIn);

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

// Radar: 0..RADAR_W-1 from a world x
#define RADAR_W 192
#define RADAR_X0 64
static inline uint8_t logicRadarX(int16_t x) { return (uint8_t)(x >> 4); }

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
#define ENTRY_FRAMES 1500    // initials: 30 s, then what's chosen stands
#define DEMO_LETTERS " ABCDEFGHIJKLMNOPQRSTUVWXYZ."   // a name's letters, in stick order
void logicScoresDefault(tGame *g);
uint8_t logicScoreRank(const tGame *g, uint32_t ulScore);   // its place, or SCORE_COUNT
void logicScoresPack(const tGame *g, uint8_t *pOut);        // SCORES_BYTES
uint8_t logicScoresUnpack(tGame *g, const uint8_t *pIn);    // 0: not a valid table (g unchanged)
void logicStartDemo(tGame *g);
uint8_t logicDemoInput(tGame *g, tInput *pOut);             // 0: the stream is over
uint8_t logicIsMenu(const tGame *g);                        // the title or the hall of fame

// The preview card before each mission (GS_INTRO, fire skips it): its one-line
// brief - what's out there this time - and a strip map of the battlefield,
// world x / ROUTE_SCALE, with a "you are here" marker at the chopper.
const char *logicBrief(const tGame *g);
#define ROUTE_SCALE 12
#define ROUTE_W (WORLD_W / ROUTE_SCALE)   // 256 px
enum { ROUTE_ENEMY, ROUTE_HOME, ROUTE_FENCE, ROUTE_BARRACKS, ROUTE_PAD, ROUTE_HANGAR, ROUTE_HERE };
typedef struct { uint8_t kind, x, w; } tRouteMark;   // x, w in map px
#define ROUTE_MARKS (6 + BARRACKS_COUNT)
uint8_t logicRouteMap(const tGame *g, tRouteMark *pOut);   // back to front

#endif // _LOGIC_H_
