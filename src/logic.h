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
#define BOOM_MAX 6

// --------------------------------------------------------------- the BOBs ---
// Sizes must match the art (main.c checks at startup).
enum {
	BOB_HANGAR, BOB_FENCE, BOB_BARRACKS, BOB_TANK, BOB_HOSTAGE, BOB_JET,
	BOB_MINE, BOB_SHOT, BOB_BOMB, BOB_SHELL, BOB_BOOM, BOB_FLAG, BOB_SHADOW,
	BOB_TYPES
};
// The chopper's shadow on the track: frame = size * 2 + dither phase (x & 1),
// size 0 (on the ground) .. SHADOW_SIZES - 1 (high up)
#define SHADOW_Y (GROUND_Y + 1)
#define SHADOW_SIZES 4
extern const uint8_t g_pBobW[BOB_TYPES], g_pBobH[BOB_TYPES], g_pBobFrames[BOB_TYPES];

// Frames inside a BOB
enum { BARRACKS_OK, BARRACKS_HIT, BARRACKS_OPEN0, BARRACKS_OPEN1 };
enum { TANK_L0, TANK_L1, TANK_R0, TANK_R1, TANK_FIRE_L, TANK_FIRE_R };
enum { MAN_STAND, MAN_RUN0, MAN_RUN1, MAN_WAVE0, MAN_WAVE1, MAN_DEAD,
	MAN_RUNL0, MAN_RUNL1 };
enum { JET_L, JET_R };

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
} tTank;

typedef struct {
	int16_t x, y;
	int8_t dir;
	uint8_t active, cool, hp;
	uint8_t pass;        // passes made
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

enum { GS_TITLE, GS_INTRO, GS_PLAY, GS_LOST, GS_CLEAR, GS_OVER, GS_VICTORY };

typedef struct {
	uint8_t state;
	uint16_t stateTimer;
	uint16_t frame;      // frames since boot
	uint16_t lfsr;
	uint8_t mission;     // 0..MISSION_COUNT-1
	uint8_t lives;
	uint8_t trip;        // completed trips home this mission
	uint8_t aboard, saved, killed;  // this mission
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
	uint16_t wxFall;     // the weather's slide, 1/4 px (wraps)
	int16_t wxDrift;     // its wind, 1/4 px, 0..4 * SCREEN_W - 1
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
	// events of the latest frame (sound, serial): bit flags
	uint16_t events;
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

// --------------------------------------------------------------- tuning ---
#define VX_MAX 48            // 3 px/frame
#define VX_ACCEL 2
#define VX_DRAG 1
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

// The enemy schedule for the current difficulty, mission and trip
uint8_t logicTankLimit(const tGame *g);
uint8_t logicJetsOn(const tGame *g);
uint8_t logicMinesOn(const tGame *g);

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

// Radar: 0..RADAR_W-1 from a world x
#define RADAR_W 192
#define RADAR_X0 64
static inline uint8_t logicRadarX(int16_t x) { return (uint8_t)(x >> 4); }

// Mission palettes: sky gradient and band colours (copper)
#define SKY_LINES 172
uint16_t logicSkyColor(uint8_t ubMission, uint8_t ubLine);   // line 0 = PLAY_TOP
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

#endif // _LOGIC_H_
