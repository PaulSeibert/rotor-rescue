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
#include "world3_art.h"   // world 3's palettes (art/tools/make_world3_disk.py)

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
// World 3, the Highlands (SPEC 13): the hardware pilot, reached only by the debug entry (Ctrl+L, W, 7), so WORLD_COUNT
// (the worlds the game walks through) stays 2. g->world is 2 there; every world-2-only rule asks logicIsW2().
#define W3_WORLD 2
#define WORLD_DEBUG_COUNT 3
#define W3_W 4096
#define W3_COLS (W3_W / 16)      // 256 columns of 16 px: the height map has W3_COLS + 1 joints
#define W3_PEAK 100              // the highest surface, px above GROUND_Y
#define W3_WALL_RISE 28          // a flank rising this much (or more) in 16 px (60 degrees) is a wall
#define W3_SKID_HALF 10          // the skids are x - 10 and x + 10
#define W3_BODY_HALF 16          // the body's half width: a wall stops it
#define W3_DECK_CLEAR 20         // the chopper's height: its skids stop this far under a deck's base
#define W3_MIN_GAP 70            // the least air between a deck and the crest under it on the route (SPEC 13.2)
#define W3_FLAG_PAD 1            // a column flag: a pad bed (flat, landable)
#define W3_FLAG_MASSIF 2         // a column flag: a mass the route goes round (exempt from the air-gap rule)
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
// World 2's jets (SPEC 6, v1.16): none appear while the chopper is right of this x (the base island's approach: 336 px before its pad),
// as world 1's stop at BORDER_X + 120
#define W2_JET_SAFE_X 3800

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
	BOB_PAD_LAMP, BOB_SPLASH, BOB_W2_JET, BOB_RADAR, BOB_SOCK, BOB_ROPE, BOB_HOOK, BOB_CLIMB,   // (slice 2's common set: both missions)
	BOB_DECK,                                                                                   // (mission 5's part, loaded at its card)
	BOB_FRTR, BOB_SPRAY, BOB_DHULL, BOB_DHULL0, BOB_DHULL1, BOB_DHULL2, BOB_DHULL3, BOB_TURRET, BOB_THIT, BOB_AAGUN, BOB_BARRAGE, BOB_CAP,   // (mission 6's part)
	BOB_W3_LODGE, BOB_W3_NEST,   // world 3's own (its pad is BOB_PAD64 again, in its own art), after the missions' parts so world 2's ranges stay whole
	BOB_TYPES
};
#define BOB_M5_FIRST BOB_DECK        // mission 5's own BOBs: BOB_DECK only
#define BOB_M6_FIRST BOB_FRTR        // mission 6's: BOB_FRTR .. BOB_CAP (the freighter, the strait destroyer, the whitecaps)
#define BOB_W1_FIRST BOB_FENCE       // world 1's own BOBs: BOB_FENCE .. BOB_FLAG
#define BOB_W2_FIRST BOB_W2_BARRACKS // world 2's: BOB_W2_BARRACKS .. BOB_CAP (the jet is world 1's, a copy in the set; from BOB_M5_FIRST a mission's own)
#define BOB_W3_FIRST BOB_W3_LODGE    // world 3's: BOB_W3_LODGE, BOB_W3_NEST (and BOB_PAD64)
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
// The radar site's dish turns: wide, half, thin, half (the red lamp blinks on the even frames)
enum { RADAR_F0, RADAR_F1, RADAR_F2, RADAR_F3 };
enum { DECK_F0, DECK_F1 };                       // the cargo ship: idle, bow wake
enum { ROPE_HANG, ROPE_LADDER_L, ROPE_LADDER_R };   // the winch rope, the ladder swaying left, right
enum { CLIMB_F0, CLIMB_F1 };                     // a person climbing
enum { SOCK_LIMP, SOCK_FULL };   // the windsock: it fills (and whips) for a gust
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

// World 2's radar sites (missions 5 and 6; SPEC 11, "World 2, slice 2"): static, on an island, never shooting. While one
// is alive the 1200 px stretch around it (its zone) is where jets may start; destroying it ends jets there.
#define RADAR_MAX 3
#define RADAR_HP 4               // a cannon hit is 1, a bomb kills
#define RADAR_ZONE_HALF 600      // the zone is x - 600 .. x + 600 (1200 px), cut at W2_JET_SAFE_X
#define RADAR_SCORE 150
#define RADAR_W_PX 34            // its hit box: 34 x 38, standing on FEET_Y
#define RADAR_H_PX 38
typedef struct {
	int16_t x;               // centre
	uint8_t alive, hp;
} tRadar;
typedef struct { int16_t lo, hi; } tZone;

// World 2's wind gusts (missions 5 and 6; SPEC 11, "World 2, slice 2", slice B): a gust pushes the chopper toward +x, 0.25 px a frame
// for 3 s, after a 50 frame tell (the windsocks whip); one at a time; never while landed, just after take-off or in a mission's
// first 18 s; none on skill 1; skill 2-3 not near a barracks. Its own sub-pixel accumulator and random seed.
#define GUST_PUSH 4              // 1/16 px a frame
#define GUST_FRAMES 150          // 3 s of push
#define GUST_TELL_FRAMES 50      // the windsocks whip this long first
#define GUST_FIRST 900           // frames of a mission before its first tell may begin (18 s), plus up to the gap's spread
#define GUST_GAP_MIN 600         // frames from one gust's end to the next one's tell: 12-20 s
#define GUST_GAP_MAX 1000
#define GUST_BARRACKS_CLEAR 100  // skill 2-3: no gust within this many px of a barracks
// World 2's ship deck (mission 5; SPEC 11, "World 2, slice 2", slice C): a moored cargo ship replaces the third island's barracks. 16
// people wait on it; the chopper hovers in a 24 x 20 px box above its flat deck and a rope brings one up every DECK_PERSON_FRAMES
#define DECK_X 1730              // the hull's left edge, world px (124 x 30, art/world2/deck_ship.txt); its top is DECK_TOP
#define DECK_TOP 219             // (the waterline row is BOAT_BOTTOM_Y 248 at the hull's last row, 29 below it)
#define DECK_Y 227               // the flat deck edge: the skids stop here over the hull (not a landing)
#define DECK_BARRACKS 2          // the barracks slot (and the people's slots) it takes in mission 5
#define DECK_PEOPLE HOSTAGES_PER_BARRACKS
#define DECK_BOX_X0 (DECK_X + 47)   // the hover box: hull x 47-70, the skids' y 207 .. 227
#define DECK_BOX_X1 (DECK_X + 70)
#define DECK_BOX_Y0 (DECK_Y - 20)
#define DECK_HOVER_SPEED 8       // the most the chopper may move (1/16 px a frame, each axis) and still be hovering
#define DECK_ROPE_SET 12         // frames of hover before the rope is down and a person starts to climb (the hook falls 2 px a frame)
#define DECK_PERSON_FRAMES 50    // a person's climb: 8 people take 8 s
#define DECK_WALK_FRAMES 22      // of those 50, the run from the group to the rope's foot (the rest is the climb)
#define DECK_GROUP_MAX 5         // people drawn waving on deck, for all those waiting
// World 2's sinking freighter (mission 6; SPEC 11, "World 2, slice 2", slice D): in the same sea gap, in place of the third barracks. A timer
// (SINK_SECONDS by skill, in four stages: the BOB's frames) runs from the start of play; the winch box is the ship's hold, 20 px above the
// coaming, and follows the tilt; when it sinks the people still aboard are lost
#define FRTR_X 1728              // the hull's left edge (128 x 36, art/world2/freighter.txt); its top is FRTR_TOP
#define FRTR_TOP 213             // (the waterline row, 33, is y 246)
#define FRTR_BOX_DX 52           // the hover box: hull x 52-75
#define FRTR_STAGES 4
#define SINK_BAR_PX 192          // the timer bar's length (HUD row 22-23, from RADAR_X0): the whole timer
enum { SPRAY_F0, SPRAY_F1, SPRAY_F2 };
// World 2's strait destroyer (mission 6; SPEC 11, "World 2, slice 2", slice E): a set piece in the sea between the third and the fourth island,
// one facing (bow left), not moving: two turrets of 6 HP, four AA guns without HP, a barrage at 40 s and its turrets open at 90 s
#define DEST_X 2560              // the hull's left edge (154 x 44, art/world2/hull_list.txt, cut into 4 BOBs of at most 48 px); its top is DEST_TOP
#define DEST_W 154
// The BOB blitter does not clip: a BOB is drawn only whole inside the buffer's margins (48 px left of the screen, 48 px right of it). The whole hull
// (BOB_DHULL, 154 px) is drawn while its left edge is on the screen between these; beyond them its four 48 px pieces (BOB_DHULL0-3, views of the
// same bitmap, drawn right to left) are, which appear and vanish off screen. main.c asserts the numbers against its buffer.
#define DEST_WHOLE_MIN -48
#define DEST_WHOLE_MAX 214
#define DEST_TOP 205             // (the last row, 43, is the waterline row BOAT_BOTTOM_Y 248)
#define DEST_TURRETS 2
#define DEST_HP 6
#define DEST_OPEN_HP 2           // a turret at this many HP or fewer smokes; the 90 s opening cuts them to it
#define DEST_TURRET_SCORE 150
#define DEST_DISABLED_SCORE 100  // both turrets down: 150 + 150 + 100 = the plan's 400
#define DEST_ENGAGE 360          // the barrage's clock counts while the chopper is within this of the hull's centre
#define DEST_BARRAGE_AT 2000     // frames of that clock to the first barrage (40 s) ...
#define DEST_BARRAGE_EVERY 1500  // ... and the next ones (30 s)
#define DEST_OPEN_AT 4500        // frames of it to the turrets opening (90 s)
#define BARRAGE_TELL_FRAMES 30
#define BARRAGE_MARKERS 3
#define BARRAGE_STAGGER 4        // the markers burst this many frames apart (a salvo: three explosions at once on one picture cost more than a frame has)
#define BARRAGE_SALVO (BARRAGE_STAGGER * (BARRAGE_MARKERS - 1))   // frames of the salvo after the tell: barTell counts BARRAGE_TELL_FRAMES + this
#define BARRAGE_SPREAD 36        // the markers' distance apart (the chopper's x, +- this)
#define BARRAGE_HALF 14          // a burst's half width, px
#define BARRAGE_TOP 232          // a marker's top y (the BOB is 24 x 12)
#define BARRAGE_LOW_Y 150        // a burst reaches a chopper whose skids are below this
#define AA_COUNT 4
#define AA_RANGE 180             // less than the cannon's reach (216): a turret can be shot from outside the guns
#define AA_FLOOR_Y 212           // a gun takes only a chopper above this: more than 24 px above the sea
#define AA_MIN_DIST 56
#define AA_PUFF_MAX 14           // the ranging puff shows this many frames before the burst (or the whole tell when it is shorter)
#define AA_FLASH 5               // the muzzle flash when the puff appears
#define AA_PAIR_GAP 40           // no two guns start a tell closer than this
#define FLAK_MAX 2
#define FLAK_RADIUS 18
#define ZONE_CLEAR_SCORE 100
#define BONUS_MSG_FRAMES 150     // the HUD's left slot says ZONE CLR or DEST DWN this long ...
#define BONUS_DELAY 8            // ... starting this many frames after the event (the picture of a kill is already a heavy one: the score's digits, the boom)
enum { BONUS_NONE, BONUS_ZONE, BONUS_DEST };
// BOB_TURRET (the left facing): idle, the tell's two blinking frames, the shot; BOB_THIT: smoking, dead; BOB_AAGUN; BOB_DHULL0-3 frame: as played, listing
enum { TURRET_IDLE, TURRET_TELL1, TURRET_TELL2, TURRET_FIRE };
enum { THIT_SMOKING, THIT_DEAD };
enum { AA_IDLE, AA_TELL, AA_PUFF };
enum { HULL_OK, HULL_LIST };
extern const int16_t g_pTurretDx[DEST_TURRETS];   // the turrets' top-left x on the hull: forward -7, aft 122 (y 11)
typedef struct { int16_t x, y; uint8_t life, active; } tFlak;   // a flak shot's ranging puff: where it will burst, frames to go
extern const uint8_t g_pFrtrFloor[FRTR_STAGES];   // the skids' floor over the hold by stage (the coaming's top): 230 230 230 229
#define SOCK_DX 60               // a windsock stands this far right of each island's pad centre

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
	// the first-use bomb prompt (hintUpdate): what shows now (HINT_*), its frames left, the spacing, and
	// the shows and bombs dropped so far (over the game: a continue keeps them, a new game resets them)
	uint8_t hint, hintT, hintHold;
	uint16_t hintCoolB, hintCoolS;
	uint8_t hintShowsB, hintShowsS, bombUses;
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
	tRadar pRadars[RADAR_MAX];   // world 2 missions 5 and 6: the radar sites (radarCount placed, alive or not)
	uint8_t radarCount;
	uint8_t evRadar;     // a radar site went down this frame (index + 1)
	uint16_t gustTimer;  // frames to the next gust's tell (counts always; at 0 it waits until the chopper is where a gust may begin)
	uint8_t gustTell, gustOn;   // frames left of the tell, of the push (never both)
	uint8_t gustFx;      // the push's sub-pixel accumulator (1/16)
	uint16_t gustSeed;   // its own random sequence: the game's LFSR is not disturbed
	uint16_t events3;    // EV3_* and EV4_* (one word: clearing it each frame costs what the byte did)
	uint8_t deckOn;      // mission 5: the ship deck stands
	uint8_t deckLeft;    // people still on it
	uint8_t deckTimer;   // frames of the current person's climb (kept when the chopper drifts out)
	uint8_t winch;       // frames the chopper has hovered in the box (0: not winching): a landed-class state
	uint8_t winchSlip;   // frames the chopper has been out of the box with the rope still down (the skill's grace)
	uint16_t sinkTimer, sinkTotal;   // the freighter: frames of play so far, and the whole timer (fixed at the mission's start)
	uint8_t sinkStage;   // 0-3, a quarter of the timer each
	uint8_t sunk;        // it has gone down
	uint8_t deckLost;    // the people lost with it
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
	uint8_t events5;     // EV5_* (world 3: slice 2 already owns events3/events4)
	uint8_t evBarracks;  // which barracks opened (index + 1)
	// the strait destroyer (mission 6, slice E)
	uint8_t destOn;      // mission 6: it lies in the strait
	uint8_t turHp[DEST_TURRETS];     // forward, aft: 6 .. 0
	uint8_t turTell[DEST_TURRETS];   // frames left of the wind-up before a shell (0: none)
	uint8_t turFire[DEST_TURRETS];   // frames of muzzle flash
	uint8_t turCool[DEST_TURRETS];   // frames to the next tell
	uint8_t destOpen;    // the 90 s opening happened
	uint8_t destDown;    // both turrets are dead: the ship is disabled, listing, silent
	uint16_t destClock;  // frames the chopper has spent alive within DEST_ENGAGE
	uint16_t barNext;    // the clock value at which the next barrage starts
	uint8_t barTell;     // frames left of the barrage's tell (0: none)
	int16_t barX[BARRAGE_MARKERS];   // the markers' centres
	uint8_t barN;        // markers placed (the others were over land)
	uint8_t aaTell[AA_COUNT], aaCool[AA_COUNT], aaFlash[AA_COUNT];   // a gun's frames left: the tell, the rest, the muzzle flash
	uint8_t aaWait;      // frames until another gun may start a tell
	tFlak pFlak[FLAK_MAX];
	uint8_t zoneDone;    // the radar zone bonus was paid this mission
	uint8_t quotaKind;   // what the HUD's quota line says while quotaMsg runs: 0 QUOTA 10, BONUS_ZONE, BONUS_DEST
	uint8_t bonusMsg, bonusKind;     // the HUD line for a bonus: frames left, BONUS_*
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

#define EV3_GUSTTELL 0x01     // a gust's tell began
#define EV3_GUST     0x02     // its push began
#define EV3_WINCHSTART 0x04   // the chopper began a winch hover
#define EV3_WINCHBOARD 0x08   // a person came aboard up the rope
#define EV3_SINKSTAGE 0x10    // the freighter reached the next stage
#define EV3_SUNK     0x20     // it sank

#define EV4_TURTELL 0x0040   // a turret started its wind-up
#define EV4_TURFIRE 0x0080   // a turret fired
#define EV4_TURDOWN 0x0100   // a turret was destroyed
#define EV4_DESTDOWN 0x0200   // both are: the ship is disabled
#define EV4_DESTOPEN 0x0400   // the 90 s opening
#define EV4_BARTELL 0x0800   // a barrage's tell began
#define EV4_BARFIRE 0x1000   // its markers burst
#define EV4_AATELL 0x2000   // a gun started its tell
#define EV4_AABURST 0x4000   // a flak shot burst
#define EV4_ZONE 0x8000   // the last radar site fell: the zone bonus

#define EV2_JETTELL  0x01     // a jet started its flash (a missile or bomb follows)
#define EV2_JETFIRE  0x02     // a jet launched one
#define EV2_DAMAGE   0x04     // the chopper took its first hit
#define EV2_REPAIR   0x08     // landing on the pad mended it
#define EV2_JETWARN  0x10     // a jet appeared: the incoming-jet warning (a radar flash, an alert)
#define EV2_SPLASH   0x20     // the skids touched the water (world 2): a soft stop
#define EV2_BOATTELL 0x40     // a gunboat started its wind-up (a shell follows)
#define EV2_BOATFIRE 0x80     // a gunboat fired
#define EV5_BUMP     0x01     // the chopper's body met a wall (world 3): speed lost, no damage

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
#define W2_JET_FIRST 500     // world 2: a mission's first jet comes after 10 s of flying (world 1: 100 frames), so the first trip has its quiet start and then something to dodge
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
// The first-use bomb prompt (SPEC 3: Paul could not find how to bomb with the keyboard). A timed line
// while a bombable target (a tank, a closed barracks) is on screen ahead of the cannon-stance chopper,
// and one while it is in the bomb stance. Like F-35's: HINT_FRAMES long, at most HINT_SHOWS a kind a game,
// HINT_COOL frames from one's start to the next's, off for good after HINT_USES bombs dropped. The
// counts are kept through a continue and the next mission, reset by a new game.
#define HINT_NONE 0
#define HINT_BOMB 1          // a target ahead, the cannon: how to get to the bombs
#define HINT_STANCE 2        // in the bomb stance: fire bombs, up + fire is the cannon
// logicHintKey: what the line says (0: nothing). The first two alternate every HINT_ALT_FRAMES
#define HINT_KEY_NONE 0
#define HINT_KEY_DOWN 1      // HOLD DOWN, THEN FIRE: BOMB   (the stick and the keys)
#define HINT_KEY_BUTTON 2    // BTN 2, THEN FIRE: BOMB       (the second button)
#define HINT_KEY_STANCE 3
#define HINT_USES 3          // bombs dropped in a game after which the prompts stop
#define HINT_SHOWS 4         // timed prompts of each kind in a game
#define HINT_FRAMES 150      // how long one lasts
#define HINT_COOL 500        // frames from one prompt's start to the next of its kind
#define HINT_HOLD 60         // frames from the start of play (a card, a continue) before a prompt may begin: the card's letters are typed away by then
#define HINT_ALT_FRAMES 50   // the two ways to the bombs swap this often
_Static_assert(HINT_FRAMES <= 3 * HINT_ALT_FRAMES, "logicHintKey: down, button, down");
#define HINT_RANGE 160       // px ahead of the chopper a target counts (and it must be on screen)
#define HINT_BEHIND 24       // ... and this far behind it (still under it)
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
static inline int16_t logicWorldW(const tGame *g) { return g->world ? (g->world == W3_WORLD ? W3_W : W2_W) : WORLD_W; }   // (world 1 first: the hot paths pay one test)
static inline uint8_t logicIsW2(const tGame *g) { return g->world == 1; }
static inline uint8_t logicIsW3(const tGame *g) { return g->world == W3_WORLD; }

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
uint8_t logicJetsPossible(const tGame *g);   // world 2: jets can appear in this mission at this skill, on some trip (the card's brief says so)
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
#define ATK_JET 0xFE         // atkLast when a jet started the attack (world 2: jets and boats take turns)
#define ATK_TURRET 0xF0      // atkLast when a destroyer's turret did (+ its index), and ATK_BARRAGE for its barrage
#define ATK_BARRAGE 0xF2
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
// The strait destroyer's tables by skill (SPEC 11, slice E): the turret's tell 28 / 24 / 20 / 18 / 16 and the rest between its shells (tell included);
// a gun's tell 26 / 22 / 18 / 15 / 12 and its rest after a burst 120 / 100 / 85 / 70 / 60, and the flak shots live at once
uint8_t logicDestTell(const tGame *g);
uint8_t logicDestGap(const tGame *g);
uint8_t logicAaTell(const tGame *g);
uint8_t logicAaGap(const tGame *g);
uint8_t logicAaLive(const tGame *g);
void logicKillRadars(tGame *g);   // (the test hook Ctrl+L, R) every standing radar site down at once: the zone bonus
// A turret's housing centre (its hit box and its target point), world x
static inline int16_t logicTurretX(uint8_t i) { return (int16_t)(DEST_X + g_pTurretDx[i] + 24); }
// The radar sites' jet zones (SPEC 11, "World 2, slice 2"): may a jet START with the chopper at x? 1 in world 1 and in a
// mission with no radar sites (mission 4); otherwise only inside the zone of a live site
uint8_t logicJetZone(const tGame *g, int16_t x);
// Gusts may happen in this mission at this skill (missions 5 and 6, not skill 1)
uint8_t logicGustsOn(const tGame *g);
// The freighter's timer bar: 0 .. SINK_BAR_PX px for the time left (0 when there is no freighter or nobody aboard); the timer in seconds by skill
uint8_t logicSinkBar(const tGame *g);
uint8_t logicSinkSeconds(uint8_t ubDifficulty);
// The winch window by skill (SPEC 11, "Winch and loading polish"; entry 0 = GODMODE = entry 3): px added to each side of slice C's 24 px box, px the
// box's top is above its floor (20 on skill 5), the hover speed limit (1/16 px a frame), the frames the rope is kept out of the box, the capture
// ring (px; 0 = no pull and no settle)
extern const uint8_t g_pWinchGrow[6], g_pWinchUp[6], g_pWinchTol[6], g_pWinchGrace[6], g_pWinchCatch[6];
void logicWinchBox(const tGame *g, int16_t *pX0, int16_t *pX1, int16_t *pTopY);   // the box at this skill: x0 .. x1 (the chopper's x) and its top y (its floor is the site's)
uint8_t logicDeckGroup(const tGame *g);   // the people drawn waving on the deck: ceil(waiting * DECK_GROUP_MAX / DECK_PEOPLE)
// The zones of the live sites (ubAll: of every site, for the card), cut to 0 .. W2_JET_SAFE_X: how many, in pOut (RADAR_MAX)
uint8_t logicRadarZones(const tGame *g, tZone *pOut, uint8_t ubAll);
// A mission's sites (0-2 within world 2: the x of each, up to RADAR_MAX; how many) and the zone of a site at x: for the card's
// map, which is made for every mission of the world at its load
uint8_t logicRadarSites(uint8_t ubMission, int16_t *pX);
void logicRadarZoneOf(int16_t x, tZone *pZ);

// Drawing: what's on screen, back to front, in screen coordinates (top-left)
typedef struct {
	int16_t x, y;
	uint8_t bob, frame;
} tDraw;
#define DRAW_MAX 96
extern uint8_t g_ubDrawPeople0, g_ubDrawPeople1;   // (set by logicDraw: where its people are in the list, for main.c's merged erases)
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
#define ROTOR_OS 2               // v4: the loop is made at this many times the logical rate (period / ROTOR_OS)
#define ROTOR_LOOP_PASS (ROTOR_PASS * ROTOR_OS)   // samples a blade pass as stored
#define ROTOR_PASSES 2
#define ROTOR_SAMPLES (ROTOR_LOOP_PASS * ROTOR_PASSES)   // bytes of chip RAM the loop takes
#define ROTOR_CLOCK 3546895L     // Paula's PAL clock: rate = clock / period
uint16_t logicRotorPeriod(const tGame *g);   // Paula period
uint8_t logicRotorVolume(const tGame *g);    // 0-64 (0 when the chopper is gone)
int logicRotorThumpsX10(uint16_t uwPeriod);  // thumps a second times ten
uint16_t logicRotorHwPeriod(uint16_t uwLogical);   // what main.c writes to AUD2PER (the logical period / ROTOR_OS)
void logicRotorSynth(int8_t *pOut);          // the loop (ROTOR_SAMPLES bytes): main.c at startup, the unit test, make_rotor.py's mirror

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
// The animated sea (SPEC 11, "Sea animation"): a swell runs toward the player, a
// per-line horizontal drift of the water, deterministic from the game frame counter
// alone. Phase = SEA_PHASES steps of 2^SEA_PHASE_SHIFT frames (a cycle of 128 frames,
// a power of two so the 16-bit counter wraps seamlessly); line j drifts
// A_j sin(2 pi (phase - j) / 32) quarter pixels, A_j = (28 + 3 j) / 7 (the camera's own
// 1/z ratio: the near water swings most, 3.8 px at line 26) times a ramp: lines 1-11, the
// far water, are still (it is calm at a distance, and each line moved costs a copper
// write), line 12 moves 1/6 as far, line 17 and nearer all of it; rows 0 and 27 (bare)
// none. ubPhase is logicSeaPhase(frame).
#define SEA_PHASES 32
#define SEA_PHASE_SHIFT 2
uint8_t logicSeaPhase(uint16_t uwFrame);
int8_t logicSeaDrift(uint8_t ubPhase, uint8_t ubLine);   // quarter pixels, signed
// The sea's glints shimmer: pens 4-7 of the sea palette lighten 0-2 steps a channel on a
// triangle ramp, a step every 2^SEA_PEN_SHIFT frames (a cycle of 256), each pen a
// quarter of the cycle after the one before (a swell of light through the depth).
// uwBase is the mission's pen colour (0xRGB), ubPen 1-7; pens 1-3 never change.
#define SEA_PEN_SHIFT 3
uint16_t logicSeaPen(uint16_t uwBase, uint8_t ubPen, uint16_t uwFrame);
// ...as its two halves: the pen's level (0-2) at a frame, and a colour lightened by a level;
// the game builds its table of the three colours a pen has (startup) and looks the level up
uint8_t logicSeaPenLevel(uint8_t ubPen, uint16_t uwFrame);
uint16_t logicSeaLighten(uint16_t uwBase, uint8_t ubLevel);
// The base ocean (SPEC 11, "Base ocean animation"): the terrain strip's water, lines 208-255, in colour
// cycling. The art draws each water stroke in one of the island palette's pens 1, 6 or 7 (class 0, 1, 2).
// A pen's colour is the sum of three slow movements, all powers of two of the 16-bit frame counter so it
// wraps with no jump: the swell, a crest of light rolling toward the player (32 steps of 2 frames, a cycle
// of 64; the classes sit 0, 11, 21 steps apart); a counter-wave running the other way, a third as strong
// (32 steps of 8 frames, 256); and the deep water's breathing (the dark's depth, 32 steps of 16 frames, 512).
// uwDark, uwLight, uwWhite are the mission's island pens 1, 6, 7 (0xRGB); the colour runs from the dark
// (pen 1, 3 to 7 sixteenths of the way to pen 6) through the light to near-white (13/16 of the way to pen 7, never above it).
#define BASE_STEPS 32
#define BASE_STEP_SHIFT 1
uint8_t logicBaseStep(uint16_t uwFrame);
uint16_t logicBaseWave(uint16_t uwDark, uint16_t uwLight, uint16_t uwWhite, uint16_t uwFrame, uint8_t ubClass);
// ...as the game runs it: the table of one mission's colours (BASE_TAB_N words, built once from logicBaseColor) and a lookup that
// costs a few shifts; logicBaseWave is the same thing computed in full (the host's reference)
#define BASE_TAB_N (4 * 17)
uint16_t logicBaseColor(uint16_t uwDark, uint16_t uwLight, uint16_t uwWhite, uint8_t ubDw, uint8_t ubS);
uint16_t logicBaseWaveTab(const uint16_t *pTab, uint16_t uwFrame, uint8_t ubClass);
uint8_t logicBaseIndex(uint16_t uwFrame, uint8_t ubClass);   // the table index a class has at a frame
uint16_t logicBaseColorAt(uint16_t uwDark, uint16_t uwLight, uint16_t uwWhite, uint8_t ubIdx);   // the colour an index stands for
// Whether a picture writes the base ocean pens into its (back) list: ubHold is the list's first pictures (s_ubSeaHold), uwBlits
// the blits the picture queued so far (a busy picture is the frame budget's worst, it writes nothing)
#define BASE_BLIT_MAX 14   // = main.c's SEA_BLIT_MAX: the pens' writes cost 2 points of the worst picture on the a500 (w2play 85 -> 87 when every picture wrote)
uint8_t logicBasePenWrite(uint8_t ubHold, uint16_t uwBlits);
// The step (key, a frame / 2) a list shows when it is written. The two copper lists show the pens of two pictures in turn, so when
// a busy picture skipped its write the lists once showed different steps, and the water flipped between them every picture, or
// jumped many steps when a quiet picture caught up (tests/unit testBaseFlicker: up to 9 levels of a pen channel in a picture, the
// wave moves 3 at most). The rule: a list never leaves the other list more than ONE step ahead, and a list behind goes only to the
// other's step: so the shown pictures never differ by more than the wave's own step, and the wave runs slower while pictures are
// busy (never faster than its own speed, so it falls behind the frame counter for good: invisible, it is a slow cycle).
// uwTarget is the frame's step, uwMine and uwOther the lists' current ones (BASE_KEY_NONE: the list was just written)
#define BASE_KEY_NONE 0xFFFF
uint16_t logicBasePenKey(uint16_t uwTarget, uint16_t uwMine, uint16_t uwOther);
// pBase (SEA_LINES words) takes the undrifted left edge of line SEA_SWELL_FROM - 1 and nearer, for logicSeaSwell: the swell
// alone, same camera, lines SEA_SWELL_FROM and nearer (the far water is still)
#define SEA_SWELL_FROM 12
#define SEA_NO_SWELL 0xFF   // the game's phase for 'no swell' (a mission's first pictures hold it: they are the frame budget's worst)
int16_t logicSeaScroll(int16_t wCam, uint16_t uwX4Mask, uint16_t uwModBase, uint16_t *pOut, uint8_t ubStride);   // the camera alone
int16_t logicSeaScrollSwell(int16_t wCam, uint8_t ubPhase, uint16_t uwX4Mask, uint16_t uwModBase, uint16_t *pOut, uint8_t ubStride, uint16_t *pBase);   // ...and the swell
void logicSeaSwell(uint8_t ubPhase, uint16_t uwX4Mask, uint16_t uwModBase, const uint16_t *pBase, uint16_t *pOut, uint8_t ubStride);
uint16_t logicSeaX4(int16_t wCam, uint8_t j);   // (host reference) row j's left edge, 1/4 px, no drift
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

// T3 (SPEC 13.3): the bands of each world's display, one table the copper builder reads (main.c) instead of
// constants: the band's first game line, its rows, its camera speed as a shift (0 = 1:1, 1 = /2, 2 = /4, 3 = /8) and
// the PF2 palette slot it loads (0 clouds or ceiling, 1 far, 2 land / sea / strip, 3 islands). rows 0: no such band.
// World 0 and 1 are the lines the builder had as constants; world 2 (the Highlands) is STYLE-world3 section 2.
enum { BK_CLOUDS, BK_FAR, BK_FLOOR, BK_HILLS, BK_GROUND, BK_COUNT };   // (the ground band is world 2's and world 3's terrain strip)
typedef struct { uint8_t y, rows, shift, pal; } tBand;
typedef struct { uint8_t skyEnd; tBand b[BK_COUNT]; } tBandLayout;   // skyEnd: the game line the sky's COLOR00 MOVEs stop at
const tBandLayout *logicBands(uint8_t ubWorld);

// World 3's terrain (SPEC 13.5): a map is the surface height at each 16 px joint (px above GROUND_Y), a flag a column and
// the deck's base line a joint (0: no deck). logicW3UseMap(0, 0, 0): the built-in pilot map (world3_map.h, no deck).
void logicW3UseMap(const uint8_t *pHeight, const uint8_t *pFlags, const uint8_t *pCeil);
uint8_t logicW3Joint(uint16_t uwJoint);                // the map's height at a joint
int16_t logicW3SurfaceY(int16_t x);                    // the surface's y at x (the line between its joints)
int16_t logicW3FloorAt(int16_t x);                     // the highest surface under the skids, as a y
uint8_t logicW3IsPad(int16_t x);                       // x is over a pad bed
int16_t logicW3CeilAt(int16_t x);                      // the highest y the skids can reach at x
uint8_t logicW3WallAt(int16_t xEdge, int16_t y, int8_t bDir);   // the body's leading edge at xEdge, skids at y, heading bDir: in a wall?
uint8_t logicW3GapOk(const uint8_t *pHeight, const uint8_t *pFlags, const uint8_t *pCeil);   // SPEC 13.2 (0, 0, 0: the built-in map)

// The terrain strip's ring (worlds 2 and 3) keeps ubSpan columns drawn, wLo .. wLo + ubSpan - 1: when wLo moves from wOldLo, the columns
// that come into it are [*pFrom, *pTo) (one range; empty when *pFrom >= *pTo; a jump draws the whole span)
void logicStripNew(int16_t wLo, int16_t wOldLo, uint8_t ubSpan, int16_t *pFrom, int16_t *pTo);

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

/** The bomb prompt now: HINT_NONE, HINT_BOMB or HINT_STANCE. */
uint8_t logicHint(const tGame *g);
/** The line to show, HINT_KEY_*: HINT_BOMB alternates its two wordings every HINT_ALT_FRAMES. */
uint8_t logicHintKey(const tGame *g);
/** Its text (at most 40 letters; "" for HINT_KEY_NONE). */
const char *logicHintText(uint8_t ubKey);

// The preview card before each mission (GS_INTRO, fire skips it): its one-line
// brief - what's out there this time - and a strip map of the battlefield,
// world x / ROUTE_SCALE, with a "you are here" marker at the chopper.
const char *logicBrief(const tGame *g);
#define ROUTE_SCALE 12
#define ROUTE_W (WORLD_W / ROUTE_SCALE)   // 256 px
// (World 2's map is world x / 18: ROUTE_ISLAND the land)
enum { ROUTE_ENEMY, ROUTE_HOME, ROUTE_FENCE, ROUTE_BARRACKS, ROUTE_PAD, ROUTE_HANGAR, ROUTE_HERE,
	ROUTE_ISLAND, ROUTE_ZONE, ROUTE_RADAR, ROUTE_DECK };   // (world 2 missions 5-6: a jet zone's red line, a radar site's tick)
typedef struct { uint8_t kind, x, w; } tRouteMark;   // x, w in map px
#define ROUTE_MARKS (7 + 4 * BARRACKS_COUNT + 2 * RADAR_MAX)
uint8_t logicRouteMap(const tGame *g, tRouteMark *pOut);   // back to front
// World 2's map of mission ubMission (0-2) of the world, whatever mission g is in: the card's maps are made for all three at the world's load
uint8_t logicRouteMapFor(const tGame *g, uint8_t ubMission, tRouteMark *pOut);
uint8_t logicRouteX2(int16_t x);   // world 2's map x for a world x (x / 18): the card's jet zones and radar sites

#endif // _LOGIC_H_
