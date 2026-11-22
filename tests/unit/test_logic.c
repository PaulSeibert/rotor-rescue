// Host-side tests for src/logic.c (ROTOR RESCUE). Run with: agk unit
#include <stdio.h>
#include <string.h>
#include "logic.h"

static int s_failures;

#define CHECK(cond) do { \
	if(!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++s_failures; } \
} while(0)

static tGame g;
static const tInput NONE = {0};

static int absi(int v) { return v < 0 ? -v : v; }

static void step(tInput in, int frames) {
	for(int i = 0; i < frames; ++i) logicUpdate(&g, &in);
}

// A game in GS_PLAY, the chopper landed at the base
static void newGame(void) {
	logicInit(&g);
	logicStartGame(&g);
	step(NONE, INTRO_FRAMES + 1);
}

// Airborne at (x, y), facing left, still, no enemies
static void flyAt(int16_t x, int16_t y) {
	g.chop.x = x;
	g.chop.y = y;
	g.chop.landed = 0;
	g.chop.vx = g.chop.vy = 0;
	g.landFrames = 0;      // (it took off a while ago)
	g.airGrace = 0;
	g.cam = (int16_t)(x - SCREEN_W / 2);
	g.spawnTimer = 60000;
}

static uint8_t freeCount(void) {
	uint8_t n = 0;
	for(int i = 0; i < HOSTAGE_COUNT; ++i) n += g.pHostages[i].state == HS_FREE || g.pHostages[i].state == HS_TO_CHOPPER;
	return n;
}

static void testTitleNeedsFire(void) {
	logicInit(&g);
	CHECK(g.state == GS_TITLE);
	step(NONE, 100);
	CHECK(g.state == GS_TITLE);
	step((tInput){.fire = 1}, 1);
	CHECK(g.state == GS_INTRO);
	step(NONE, INTRO_FRAMES + 1);
	CHECK(g.state == GS_PLAY);
}

// The preview card: the brief follows the schedule, the map has the base right
// of the fence, the barracks left of it, the marker on the chopper; fire held
// from the title doesn't skip it, a fresh press does
static void testPreviewCard(void) {
	logicInit(&g);
	step(NONE, 1);
	step((tInput){.fire = 1}, 1);
	CHECK(g.state == GS_INTRO);
	step((tInput){.fire = 1}, 30);
	CHECK(g.state == GS_INTRO);            // still held from the title
	step(NONE, 1);
	step((tInput){.fire = 1}, 1);
	CHECK(g.state == GS_PLAY);             // a new press skips
	step(NONE, 20);
	CHECK(g.pShots[0].active == 0);        // and doesn't shoot
	tRouteMark pM[ROUTE_MARKS];
	uint8_t n = logicRouteMap(&g, pM), ubFence = 0, ubHere = 0, ubHangar = 0, ubBarracks = 0;
	CHECK(n <= ROUTE_MARKS);
	for(uint8_t i = 0; i < n; ++i) {
		CHECK(pM[i].x + pM[i].w <= ROUTE_W);
		if(pM[i].kind == ROUTE_FENCE) ubFence = pM[i].x;
		if(pM[i].kind == ROUTE_HERE) ubHere = pM[i].x;
		if(pM[i].kind == ROUTE_HANGAR) ubHangar = pM[i].x;
	}
	for(uint8_t i = 0; i < n; ++i) {
		if(pM[i].kind == ROUTE_BARRACKS) { CHECK(pM[i].x + 4 < ubFence); ++ubBarracks; }
	}
	CHECK(ubBarracks == BARRACKS_COUNT && ubHere == START_X / ROUTE_SCALE);
	CHECK(ubFence < ubHere && ubHere < ubHangar && pM[n - 1].kind == ROUTE_HERE);
	// the brief: tanks only at first (normal), jets from mission 2, mines from 3
	logicStartGame(&g);
	CHECK(logicBrief(&g)[6] == 'O');       // "TANKS ON PATROL"
	logicStartMission(&g, 1);
	CHECK(logicBrief(&g)[10] == 'J');      // "TANKS AND JETS..."
	logicStartMission(&g, 2);
	CHECK(logicBrief(&g)[7] == 'J');       // "TANKS, JETS AND AIR MINES"
}

// The hall of fame: defaults best first; ranks; on the disk and back; a
// damaged file is refused and leaves the table as it was
static void testScoresTable(void) {
	logicInit(&g);
	for(uint8_t i = 1; i < HI_COUNT; ++i) CHECK(g.hi.pEntries[i].score < g.hi.pEntries[i - 1].score);
	CHECK(g.hiScore == g.hi.pEntries[0].score);
	CHECK(hiRank(&g.hi, g.hi.pEntries[0].score + 1) == 0);
	CHECK(hiRank(&g.hi, g.hi.pEntries[1].score) == 2);            // an equal score goes below
	CHECK(hiRank(&g.hi, 1) == HI_COUNT - 1 || g.hi.pEntries[HI_COUNT - 1].score >= 1);
	CHECK(hiRank(&g.hi, 0) == HI_COUNT);
	// A table the game wrote before the kit's hiscore ("rrscores", 56 bytes): it
	// reads as it is, and writes back byte for byte
	static const uint8_t pOld[HI_BYTES] = {
		0x52, 0x52, 0x48, 0x31, 0x44, 0x50, 0x53, 0x03, 0x00, 0x96, 0x00, 0x00, 0x23, 0x28, 0x5A, 0x45,
		0x44, 0x02, 0x00, 0x64, 0x00, 0x00, 0x0F, 0xA0, 0x51, 0x31, 0x20, 0x01, 0x00, 0x2D, 0x00, 0x00,
		0x0D, 0x05, 0x41, 0x47, 0x4B, 0x01, 0x00, 0x3C, 0x00, 0x00, 0x07, 0xD0, 0x52, 0x54, 0x52, 0x01,
		0x00, 0x1E, 0x00, 0x00, 0x03, 0xE8, 0x81, 0x7F,
	};
	CHECK(HI_BYTES == 56 && !strcmp(g_sHiConfig.szMagic, "RRH1") && !strcmp(g_sHiConfig.szFile, "rrscores"));
	CHECK(hiUnpack(&g.hi, &g_sHiConfig, pOld));
	CHECK(!strncmp(g.hi.pEntries[0].name, "DPS", 3) && g.hi.pEntries[0].score == 9000 &&
		g.hi.pEntries[0].reached == 3 && g.hi.pEntries[0].extra == 150);
	CHECK(!strncmp(g.hi.pEntries[2].name, "Q1 ", 3) && g.hi.pEntries[2].score == 3333 &&
		g.hi.pEntries[2].reached == 1 && g.hi.pEntries[2].extra == 45);
	uint8_t pDisk[HI_BYTES];
	hiPack(&g.hi, &g_sHiConfig, pDisk);
	CHECK(!memcmp(pDisk, pOld, HI_BYTES));
	// "all done" (MISSION_COUNT + 1) is a valid mission to have reached; a later one is not
	g.hi.pEntries[1].reached = MISSION_COUNT + 1;
	hiPack(&g.hi, &g_sHiConfig, pDisk);
	logicScoresDefault(&g);
	CHECK(hiUnpack(&g.hi, &g_sHiConfig, pDisk) && g.hi.pEntries[1].reached == MISSION_COUNT + 1);
	uint8_t pBad[HI_BYTES];
	memcpy(pBad, pOld, HI_BYTES);
	pBad[9] ^= 1;                                                  // a damaged byte: refused, nothing changes
	logicScoresDefault(&g);
	CHECK(!hiUnpack(&g.hi, &g_sHiConfig, pBad) && g.hi.pEntries[2].score == 3000);
	memset(pBad, 0, HI_BYTES);                                     // an empty file
	CHECK(!hiUnpack(&g.hi, &g_sHiConfig, pBad));
	CHECK(g.hiScore == g.hi.pEntries[0].score);
}

static void endGameWith(uint32_t ulScore) {
	newGame();
	g.score = ulScore;
	g.state = GS_OVER;
	g.stateTimer = 1;
	step(NONE, 1);
}

// A score that makes the table: initials with the stick (up/down letters,
// fire the next one, left back one), then the hall of fame with it in place
static void testHighScoreEntry(void) {
	uint32_t ulOld1 = 4000, ulOld4 = 1000;
	endGameWith(4500);
	CHECK(g.state == GS_ENTRY && g.newRank == 1);
	CHECK(g.hi.pEntries[2].score == ulOld1 && g.hi.pEntries[4].score == 2000 && ulOld4 == 1000);   // shifted down
	step((tInput){.fire = 1}, 3);                 // fire held from the end screen: nothing
	CHECK(g.edit.ubPos == 0);
	step(NONE, 1);
	step((tInput){.dy = -1}, 1);                  // A -> B
	step(NONE, 1);
	step((tInput){.dy = -1}, 1);                  // -> C
	step(NONE, 1);
	CHECK(g.hi.pEntries[1].name[0] == 'C');
	step((tInput){.fire = 1}, 1);                 // next letter (starts as C)
	step(NONE, 1);
	step((tInput){.dy = 1}, 1);                   // C -> B
	step(NONE, 1);
	step((tInput){.dx = -1}, 1);                  // back to the first
	step(NONE, 1);
	CHECK(g.edit.ubPos == 0 && g.hi.pEntries[1].name[1] == 'B');
	step((tInput){.fire = 1}, 1);
	step(NONE, 1);
	step((tInput){.fire = 1}, 1);
	step(NONE, 1);
	CHECK(g.state == GS_ENTRY && g.edit.ubPos == 2);
	step((tInput){.dy = -1}, 20);                 // held: repeats every 8 frames (3 steps)
	step(NONE, 1);
	CHECK(g.hi.pEntries[1].name[2] == 'E');
	step((tInput){.fire = 1}, 1);
	CHECK(g.state == GS_SCORES && g.scoresChanged && g.scoresNext == GS_TITLE);
	CHECK(g.hi.pEntries[1].name[0] == 'C' && g.hi.pEntries[1].name[1] == 'B' && g.hi.pEntries[1].name[2] == 'E');
	CHECK(g.hi.pEntries[1].score == 4500 && g.hi.pEntries[1].reached == 1 && g.hiScore == 5000);
	step(NONE, SCORES_FRAMES + 1);
	CHECK(g.state == GS_TITLE);
	// no input at all: after ENTRY_FRAMES the initials stand as they are
	endGameWith(9000);
	CHECK(g.state == GS_ENTRY && g.newRank == 0);
	step(NONE, ENTRY_FRAMES + 1);
	CHECK(g.state == GS_SCORES && g.hi.pEntries[0].name[0] == 'A' && g.hiScore == 9000);
	// too low: straight to the title
	endGameWith(10);
	CHECK(g.state == GS_TITLE);
}

// The same entry typed on the keyboard (raw keycodes: Q 0x10, K 0x27, E 0x12,
// 1 0x01, keypad 7 0x3D, Space 0x40, Backspace 0x41, Return 0x44)
static void key(uint8_t ubRaw) {
	step((tInput){.key = ubRaw}, 1);
	step(NONE, 1);
}

static void testHighScoreKeys(void) {
	endGameWith(4500);
	CHECK(g.state == GS_ENTRY && g.newRank == 1);
	step((tInput){.fire = 1}, 3);                 // (Space is a space here, not fire: main.c reads only the stick's)
	key(0x27);                                    // K
	CHECK(g.hi.pEntries[1].name[0] == 'K' && g.edit.ubPos == 1);
	key(0x12);                                    // E
	CHECK(g.hi.pEntries[1].name[1] == 'E' && g.edit.ubPos == 2);
	key(0x41);                                    // Backspace: the E goes
	CHECK(g.hi.pEntries[1].name[1] == ' ' && g.edit.ubPos == 1);
	key(0x41);
	key(0x41);                                    // (at the first place: nothing)
	CHECK(g.edit.ubPos == 0 && g.state == GS_ENTRY);
	key(0x4F);                                    // the left arrow types nothing
	key(0x3D);                                    // keypad 7
	CHECK(g.hi.pEntries[1].name[0] == '7' && g.edit.ubPos == 1);
	step((tInput){.dy = -1}, 1);                  // the stick still works, from the cursor: ' ' -> A
	step(NONE, 1);
	CHECK(g.hi.pEntries[1].name[1] == 'A');
	key(0x40);                                    // Space types a space
	CHECK(g.hi.pEntries[1].name[1] == ' ' && g.edit.ubPos == 2);
	CHECK(g.state == GS_ENTRY);
	key(0x44);                                    // Return: done, the third place as it was
	CHECK(g.state == GS_SCORES && g.scoresChanged && g.edit.ubPos == HI_NAME);
	CHECK(!strncmp(g.hi.pEntries[1].name, "7  ", 3) && g.hi.pEntries[1].score == 4500);
	step(NONE, SCORES_FRAMES + 1);
	// the third letter ends it, as fire on the last place does
	endGameWith(9000);
	key(0x10); key(0x01);
	CHECK(g.state == GS_ENTRY);
	key(0x31);                                    // Z
	CHECK(g.state == GS_SCORES && !strncmp(g.hi.pEntries[0].name, "Q1Z", 3) && g.hiScore == 9000);
	step(NONE, SCORES_FRAMES + 1);
	// a key restarts the 30 s; none and it times out with what is typed
	endGameWith(9500);
	step(NONE, ENTRY_FRAMES - 10);
	key(0x27);
	step(NONE, ENTRY_FRAMES - 10);
	CHECK(g.state == GS_ENTRY && g.hi.pEntries[0].name[0] == 'K');
	step(NONE, 20);
	CHECK(g.state == GS_SCORES && g.hi.pEntries[0].name[0] == 'K' && g.hi.pEntries[0].name[1] == ' ');
}

// Esc (raw 0x45) in the initials ends the entry at once and keeps the score, with the initials
// so far: what the 30 s timeout does. It is not discarded, the table is shown (and saved).
static void testHighScoreEsc(void) {
	endGameWith(4500);
	CHECK(g.state == GS_ENTRY && g.newRank == 1);
	step(NONE, 3);
	key(0x27);                                    // K
	CHECK(g.state == GS_ENTRY);
	key(0x45);                                    // Esc
	CHECK(g.state == GS_SCORES && g.scoresChanged && g.scoresNext == GS_TITLE);
	CHECK(!strncmp(g.hi.pEntries[1].name, "K  ", 3) && g.hi.pEntries[1].score == 4500 && g.hi.pEntries[1].reached == 1);
	CHECK(g.hi.pEntries[0].score == 5000 && g.hiScore == 5000);
	step(NONE, SCORES_FRAMES + 1);
	CHECK(g.state == GS_TITLE);
	// at once: the default initials stand and the score is on the table
	endGameWith(9000);
	CHECK(g.state == GS_ENTRY);
	step(NONE, 3);
	key(0x45);
	CHECK(g.state == GS_SCORES && g.hi.pEntries[0].score == 9000 && g.hi.pEntries[0].name[0] == 'A' && g.hiScore == 9000);
	step(NONE, SCORES_FRAMES + 1);
	CHECK(g.state == GS_TITLE);
}

static void testNoEntryWhenInvincible(void) {
	newGame();
	logicSetDifficulty(&g, DIFFICULTY_INVINCIBLE);
	g.score = 99999;
	g.state = GS_OVER;
	g.stateTimer = 1;
	step(NONE, 1);
	CHECK(g.state == GS_TITLE && g.hi.pEntries[0].score == 5000);
}

// Title -> hall of fame -> the demo (the recorded trip: it rescues 16 and
// gets them home, scoring nothing for the table) -> the title, the player's
// difficulty back
static void testAttractLoop(void) {
	logicInit(&g);
	logicSetDifficulty(&g, 5);
	step(NONE, ATTRACT_FRAMES - 1);
	CHECK(g.state == GS_TITLE);
	step(NONE, 1);
	CHECK(g.state == GS_SCORES && g.newRank == HI_COUNT);
	step(NONE, SCORES_FRAMES);
	CHECK(g.demo && g.state == GS_INTRO && g.difficulty == DIFFICULTY_NORMAL);
	uint8_t ubMaxAboard = 0, ubSaved = 0, isLost = 0;
	uint16_t n = 0;
	while(g.demo && n < 3000) {
		step(NONE, 1);
		++n;
		if(g.aboard > ubMaxAboard) ubMaxAboard = g.aboard;
		if(g.saved > ubSaved) ubSaved = g.saved;
		if(g.state == GS_LOST) isLost = 1;
	}
	printf("demo: %u frames, %u aboard at most, %u saved, lost %u\n", n, ubMaxAboard, ubSaved, isLost);
	CHECK(!g.demo && g.state == GS_TITLE && n > 1200);
	CHECK(ubMaxAboard == 16 && ubSaved == 16 && !isLost);
	CHECK(g.difficulty == 5 && g.hiScore == 5000 && g.score == 0);
	CHECK(g.pBarracks[3].hp == 2);   // a fresh battlefield behind the title
}

static void testDemoEndsOnFire(void) {
	logicInit(&g);
	step(NONE, ATTRACT_FRAMES + SCORES_FRAMES + 1);
	CHECK(g.demo);
	step((tInput){.fire = 1}, 1);
	CHECK(!g.demo && g.state == GS_TITLE);
	step((tInput){.fire = 1}, 30);     // still held: no game
	CHECK(g.state == GS_TITLE);
	step(NONE, 1);
	step((tInput){.fire = 1}, 1);
	CHECK(g.state == GS_INTRO && !g.demo);   // released and pressed again: a game
}

static void testStartsLandedAtBase(void) {
	newGame();
	CHECK(g.chop.landed && g.chop.alive);
	CHECK(g.chop.x == START_X && g.chop.y == GROUND_Y);
	CHECK(g.chop.face == FACE_LEFT);
	CHECK(g.lives == LIVES_START && g.aboard == 0 && g.saved == 0 && g.killed == 0);
}

static void testTakeOffAndClimb(void) {
	newGame();
	step((tInput){.dy = -1}, 30);
	CHECK(!g.chop.landed);
	CHECK(g.chop.y < GROUND_Y - 40);
	step((tInput){.dy = -1}, 300);
	CHECK(g.chop.y == CEIL_Y);
}

static void testAcceleratesToMaxSpeed(void) {
	newGame();
	flyAt(2000, 120);
	step((tInput){.dx = -1}, 60);
	CHECK(g.chop.vx == -VX_MAX);
	int16_t x0 = g.chop.x;
	step((tInput){.dx = -1}, 10);
	CHECK(x0 - g.chop.x == 30);   // 3 px a frame
	step(NONE, 100);
	CHECK(g.chop.vx == 0);        // coasts to a stop
}

static void testTurnThroughFront(void) {
	newGame();
	flyAt(2000, 120);
	CHECK(g.chop.face == FACE_LEFT);
	step((tInput){.dx = 1}, TURN_HOLD_FRAMES + TURN_STEP_FRAMES);
	CHECK(g.chop.face == FACE_FRONT);
	CHECK(logicChopperFrame(&g) == CHOP_LEAN2);   // arrived: leaning right, the way it swings
	step((tInput){.dx = 1}, LEAN_FRAMES + TURN_STEP_FRAMES);
	CHECK(g.chop.face == FACE_RIGHT);
}

static void testFrontNeedsLongPush(void) {
	newGame();
	flyAt(2000, 120);
	g.chop.face = g.chop.faceTarget = FACE_FRONT;
	step((tInput){.dx = -1}, TURN_HOLD_FRAMES * 2);
	CHECK(g.chop.faceTarget == FACE_FRONT);   // strafing, still facing front
	step((tInput){.dx = -1}, TURN_HOLD_FRAMES * 2 + LEAN_FRAMES + TURN_STEP_FRAMES + 2);
	CHECK(g.chop.face == FACE_LEFT);
}

// ---- Paul's playtest, 2026-11-12: volleys never turn into bombs

// Run an input pattern for some frames: 1 if a bomb left or the chopper began to turn away from the cannon
static int volleyTurnedToBomb(const tInput *pPattern, int patLen, int frames) {
	for(int i = 0; i < frames; ++i) {
		tInput in = pPattern[i % patLen];
		logicUpdate(&g, &in);
		g.chop.y = 120; g.chop.vy = 0;   // (the dive is the stick's, not the height)
		if(g.events & EV_BOMB) return 1;
		if(g.chop.faceTarget != FACE_LEFT || g.chop.turnTimer) return 1;
	}
	return 0;
}

static void testVolleysNeverBomb(void) {
	// tapped volleys with the stick pushed down (diving at a jet), at every rhythm shorter than the arm time
	for(int gap = 1; gap < BOMB_ARM_FRAMES; ++gap) {
		newGame();
		flyAt(2000, 120);
		tInput pat[BOMB_ARM_FRAMES + 2];
		pat[0] = (tInput){.dy = 1, .fire = 1};
		for(int i = 1; i <= gap; ++i) pat[i] = (tInput){.dy = 1};
		CHECK(!volleyTurnedToBomb(pat, gap + 1, 600));
	}
	// diagonals: down and forward, down and back
	for(int dx = -1; dx <= 0; ++dx) {
		newGame();
		flyAt(2000, 120);
		tInput pat[4] = {
			{.dx = (int8_t)dx, .dy = 1, .fire = 1}, {.dx = (int8_t)dx, .dy = 1},
			{.dx = (int8_t)dx, .dy = 1}, {.dx = (int8_t)dx, .dy = 1}};
		CHECK(!volleyTurnedToBomb(pat, 4, 600));
	}
	// down and against the facing: the stick turns it round (to the cannon on the other side), no bomb
	newGame();
	flyAt(2000, 120);
	for(int i = 0; i < 600; ++i) {
		tInput in = {.dx = 1, .dy = 1, .fire = (uint8_t)((i & 3) == 0)};
		logicUpdate(&g, &in);
		g.chop.y = 120; g.chop.vy = 0;
		CHECK(!(g.events & EV_BOMB));
	}
	CHECK(g.chop.face == FACE_RIGHT);
	// a held fire with the stick down, for ever
	newGame();
	flyAt(2000, 120);
	tInput held = {.dy = 1, .fire = 1};
	CHECK(!volleyTurnedToBomb(&held, 1, 1000));
	// one frame short of deliberate: still a cannon shot
	newGame();
	flyAt(2000, 120);
	step((tInput){.dy = 1, .fire = 1}, 3);
	step((tInput){.dy = 1}, BOMB_ARM_FRAMES - 1);
	step((tInput){.dy = 1, .fire = 1}, 1);
	CHECK(g.chop.faceTarget == FACE_LEFT);
	CHECK(g.events & EV_CANNON);
}

static void testTapsWithoutTheStickNeverBomb(void) {
	newGame();
	flyAt(2000, 120);
	tInput pat[5] = {{.fire = 1}, {0}, {0}, {0}, {0}};
	CHECK(!volleyTurnedToBomb(pat, 5, 500));
	tInput pat2[2] = {{.dx = -1, .fire = 1}, {.dx = -1}};
	CHECK(!volleyTurnedToBomb(pat2, 2, 500));
}

static void testDeliberateBomb(void) {
	// down for BOMB_ARM_FRAMES, then fire: the stance. The press itself turns, it doesn't shoot
	newGame();
	flyAt(2000, 120);
	step((tInput){.dy = 1}, BOMB_ARM_FRAMES);
	step((tInput){.dy = 1, .fire = 1}, 1);
	CHECK(g.chop.faceTarget == FACE_FRONT);
	CHECK(!g.pShots[0].active);
	step(NONE, TURN_STEP_FRAMES + 1);
	g.chop.y = 120; g.chop.vy = 0;
	CHECK(g.chop.face == FACE_FRONT);
	step((tInput){.fire = 1}, 1);
	CHECK(g.events & EV_BOMB);
	// ...and every further fire is a bomb, not the cannon
	step(NONE, 5);
	step((tInput){.fire = 1}, 1);
	CHECK(g.events & EV_BOMB);
	// up + fire: back to the cannon, at once
	step(NONE, 3);
	step((tInput){.dy = -1, .fire = 1}, 1);
	CHECK(g.chop.faceTarget == FACE_LEFT);
	step(NONE, LEAN_FRAMES + TURN_STEP_FRAMES + 2);
	CHECK(g.chop.face == FACE_LEFT);
	g.chop.y = 120; g.chop.vy = 0;
	step((tInput){.fire = 1}, 1);
	CHECK(g.events & EV_CANNON);
	CHECK(!(g.events & EV_BOMB));
	// the second button also gets there, with no hold at all
	newGame();
	flyAt(2000, 120);
	step((tInput){.fire2 = 1}, 1);
	step(NONE, TURN_STEP_FRAMES + 2);
	step((tInput){.fire = 1}, 1);
	CHECK(g.events & EV_BOMB);
}

static void testBombStanceTimesOut(void) {
	newGame();
	flyAt(2000, 120);
	step((tInput){.dy = 1}, BOMB_ARM_FRAMES);
	step((tInput){.dy = 1, .fire = 1}, 1);
	step(NONE, TURN_STEP_FRAMES + 1);
	g.chop.y = 120; g.chop.vy = 0;
	step((tInput){.fire = 1}, 1);
	step(NONE, 20);
	CHECK(g.chop.faceTarget == FACE_FRONT);   // still bombing 20 frames after the last bomb
	step(NONE, BOMB_STANCE_FRAMES);
	CHECK(g.chop.faceTarget == FACE_LEFT);    // back to the cannon on its own
	// a stance made with the second button stays
	newGame();
	flyAt(2000, 120);
	step((tInput){.fire2 = 1}, 1);
	step(NONE, BOMB_STANCE_FRAMES * 3);
	CHECK(g.chop.faceTarget == FACE_FRONT && g.chop.face == FACE_FRONT);
}

static int rankOf(uint8_t ubFrame, const uint8_t *pOrder, int count) {
	for(int i = 0; i < count; ++i) if(pOrder[i] == ubFrame) return i;
	return -1;
}

// Left side through the front to the right side by pushing the stick (and the mirror): the poses in order,
// none skipped, each held long enough to be seen, the progress never going back
static void testTurnShowsLeanFramesInOrder(void) {
	for(int dir = 0; dir < 2; ++dir) {
		newGame();
		flyAt(2000, 120);
		uint8_t o[5];
		if(dir == 0) {
			const uint8_t a[5] = {CHOP_SIDE0 | CHOP_LEFT, CHOP_TURN | CHOP_LEFT, CHOP_LEAN2, CHOP_TURN, CHOP_SIDE0};
			memcpy(o, a, 5);
		}
		else {
			const uint8_t a[5] = {CHOP_SIDE0, CHOP_TURN, CHOP_LEAN2 | CHOP_LEFT, CHOP_TURN | CHOP_LEFT, CHOP_SIDE0 | CHOP_LEFT};
			memcpy(o, a, 5);
			g.chop.face = g.chop.faceTarget = g.chop.lastSide = FACE_RIGHT;
		}
		tInput push = {.dx = dir == 0 ? 1 : -1};
		int dwell[5] = {0}, prev = 0, jump = 0, total = 0, other = 0;
		for(int i = 0; i < 80 && prev < 4; ++i) {
			logicUpdate(&g, &push);
			g.chop.y = 120; g.chop.vy = 0;
			g.chop.vx = 0;      // (a pure turn: no lean from the speed)
			int r = rankOf(logicChopperFrame(&g), o, 5);
			if(r < 0) { ++other; continue; }
			if(r < prev) CHECK(!"the turn went backwards");
			if(r - prev > jump) jump = r - prev;
			prev = r;
			++dwell[r];
			++total;
		}
		CHECK(prev == 4);                   // reached the far side
		CHECK(other == 0);                  // no other pose slipped in
		CHECK(jump <= 1);                   // no pose skipped
		CHECK(dwell[1] == TURN_STEP_FRAMES && dwell[3] >= TURN_STEP_FRAMES - 1);
		CHECK(dwell[2] >= LEAN_FRAMES - 1); // the lean is held, not a flash
		for(int r = 1; r <= 3; ++r) CHECK(dwell[r] >= 4);   // no snaps
		CHECK(total <= TURN_HOLD_FRAMES + 2 * TURN_STEP_FRAMES + LEAN_FRAMES + dwell[0] + 1);
	}
}

// The turn the other way leans the other way
static void testLeanFollowsTheTurn(void) {
	newGame();
	flyAt(2000, 120);
	step((tInput){.fire2 = 1}, 1);
	step(NONE, TURN_STEP_FRAMES);
	CHECK(g.chop.face == FACE_FRONT);
	CHECK(logicChopperFrame(&g) == CHOP_LEAN2);               // from the left side: leans right
	step((tInput){.fire2 = 1}, 1);                            // on to the right...
	step(NONE, LEAN_FRAMES + TURN_STEP_FRAMES + 2);
	CHECK(g.chop.face == FACE_RIGHT);
	step((tInput){.fire2 = 1}, 1);
	step(NONE, TURN_STEP_FRAMES);
	CHECK(g.chop.face == FACE_FRONT);
	CHECK(logicChopperFrame(&g) == (CHOP_LEAN2 | CHOP_LEFT)); // from the right side: leans left
}

// Hovering front-facing sways: front, a slight lean, front, the slight lean the other way; never a jump
static void testFrontHoverSways(void) {
	newGame();
	flyAt(2000, 120);
	step((tInput){.fire2 = 1}, 1);
	step(NONE, LEAN_FRAMES + TURN_STEP_FRAMES + 2);
	int sawFront = 0, sawR = 0, sawL = 0, bad = 0;
	uint8_t last = logicChopperFrame(&g);
	for(int i = 0; i < SWAY_PHASE * 8; ++i) {
		step(NONE, 1);
		g.chop.y = 120; g.chop.vy = 0;
		uint8_t fr = logicChopperFrame(&g);
		if(fr == CHOP_FRONT) ++sawFront;
		else if(fr == CHOP_LEAN1) ++sawR;
		else if(fr == (CHOP_LEAN1 | CHOP_LEFT)) ++sawL;
		else ++bad;
		// a lean never jumps to the other lean: it passes the front pose
		if((last == CHOP_LEAN1 && fr == (CHOP_LEAN1 | CHOP_LEFT)) ||
			(last == (CHOP_LEAN1 | CHOP_LEFT) && fr == CHOP_LEAN1)) ++bad;
		last = fr;
	}
	CHECK(sawFront > 0 && sawR > 0 && sawL > 0 && bad == 0);
	CHECK(sawFront >= SWAY_PHASE * 3);   // gentle: mostly the front pose
}

// A sidestep leans into the move
static void testSidestepLeansIntoTheMove(void) {
	newGame();
	flyAt(2000, 120);
	step((tInput){.fire2 = 1}, 1);
	step(NONE, LEAN_FRAMES + TURN_STEP_FRAMES + 2);
	step((tInput){.dx = 1}, 12);
	g.chop.y = 120; g.chop.vy = 0;
	CHECK(g.chop.face == FACE_FRONT);
	uint8_t fr = logicChopperFrame(&g);
	CHECK(fr == CHOP_LEAN2 || fr == CHOP_LEAN1);        // right
	step((tInput){.dx = -1}, 16);
	g.chop.y = 120; g.chop.vy = 0;
	fr = logicChopperFrame(&g);
	CHECK((fr & CHOP_LEFT) && ((fr & 0x7F) == CHOP_LEAN2 || (fr & 0x7F) == CHOP_LEAN1));   // left
}

// The head-on strafe, eased: the lean steps front, slight, full (and back) a pose at a time, each held
// at least LEAN_STEP_FRAMES, banking the way it travels; the speed itself is still the snappy one
static void testStrafeLeanIsEasedAndMatchesTheDirection(void) {
	for(int dir = -1; dir <= 1; dir += 2) {
		newGame();
		flyAt(2000, 120);
		step((tInput){.fire2 = 1}, 1);
		step(NONE, LEAN_FRAMES + TURN_STEP_FRAMES + 2);
		int rank = 0, peak = 0, jump = 0, wrong = 0, run = 0, shortRun = 0;
		uint8_t last = logicChopperFrame(&g);
		for(int i = 0; i < 90; ++i) {
			tInput in = {.dx = (int8_t)(i < 24 ? dir : 0)};   // 24 frames: under the 20+ turn push? no: 24 > 20, see below
			if(i >= 16) in.dx = 0;                             // (16 frames strafing, then let go)
			logicUpdate(&g, &in);
			g.chop.y = 120; g.chop.vy = 0;
			uint8_t fr = logicChopperFrame(&g);
			uint8_t pose = fr & 0x7F;
			int left = (fr & CHOP_LEFT) != 0;
			rank = pose == CHOP_LEAN2 ? 2 : pose == CHOP_LEAN1 ? 1 : 0;
			if(rank && ((dir < 0) != left) && g.chop.vx * dir > 0) ++wrong;   // leaning against its travel
			if(rank > peak) peak = rank;
			if(fr == last) ++run;
			else {
				if(run < LEAN_STEP_FRAMES && i > 1) ++shortRun;
				run = 1;
				int lr = (last & 0x7F) == CHOP_LEAN2 ? 2 : (last & 0x7F) == CHOP_LEAN1 ? 1 : 0;
				int d = rank > lr ? rank - lr : lr - rank;
				if(d > jump) jump = d;
				if(lr && rank && ((last & CHOP_LEFT) != 0) != left) jump = 9;   // lean to lean the other way: a snap
			}
			last = fr;
			CHECK(g.chop.face == FACE_FRONT);
		}
		CHECK(peak == 2);
		CHECK(jump <= 1);
		CHECK(wrong == 0);
		CHECK(shortRun == 0);
		CHECK(logicChopperFrame(&g) == CHOP_FRONT || (logicChopperFrame(&g) & 0x7F) == CHOP_LEAN1);   // settled back toward upright
	}
	// reversing: the lean passes the upright pose
	newGame();
	flyAt(2000, 120);
	step((tInput){.fire2 = 1}, 1);
	step(NONE, LEAN_FRAMES + TURN_STEP_FRAMES + 2);
	uint8_t last = logicChopperFrame(&g);
	int snaps = 0;
	for(int i = 0; i < 60; ++i) {
		tInput in = {.dx = (int8_t)((i / 12) & 1 ? -1 : 1)};
		logicUpdate(&g, &in);
		g.chop.y = 120; g.chop.vy = 0;
		uint8_t fr = logicChopperFrame(&g);
		if((last & 0x7F) >= CHOP_LEAN1 && (fr & 0x7F) >= CHOP_LEAN1 && (last & CHOP_LEFT) != (fr & CHOP_LEFT)) ++snaps;
		last = fr;
	}
	CHECK(snaps == 0);
}

static void testFire2Cycles(void) {
	newGame();
	flyAt(2000, 120);
	step((tInput){.fire2 = 1}, 1);
	step(NONE, TURN_STEP_FRAMES);
	CHECK(g.chop.face == FACE_FRONT);
	step((tInput){.fire2 = 1}, 1);
	step(NONE, LEAN_FRAMES + TURN_STEP_FRAMES + 1);
	CHECK(g.chop.face == FACE_RIGHT);   // on through to the other side
	step((tInput){.fire2 = 1}, 1);
	step(NONE, TURN_STEP_FRAMES + 1);
	CHECK(g.chop.face == FACE_FRONT);
	step((tInput){.fire2 = 1}, 1);
	step(NONE, LEAN_FRAMES + TURN_STEP_FRAMES + 1);
	CHECK(g.chop.face == FACE_LEFT);
}

// the deliberate way to the bombs: the stick down BOMB_ARM_FRAMES with no fire, then fire
static void armBombStance(void) {
	step((tInput){.dy = 1}, BOMB_ARM_FRAMES);
	step((tInput){.dy = 1, .fire = 1}, 1);
}

static void testDownFireFacesFrontThenBombs(void) {
	newGame();
	flyAt(2000, 120);
	armBombStance();
	CHECK(g.chop.faceTarget == FACE_FRONT);
	CHECK(!g.pShots[0].active);           // the press turned, it didn't shoot
	step(NONE, TURN_STEP_FRAMES + 1);
	g.chop.y = 120; g.chop.vy = 0;
	step((tInput){.fire = 1}, 1);
	CHECK(g.pShots[0].active && g.pShots[0].kind == SHOT_BOMB);
	CHECK(g.events & EV_BOMB);
}

static void testCannonFiresForwardAndRepeats(void) {
	newGame();
	flyAt(2000, 120);
	step((tInput){.fire = 1}, 1);
	CHECK(g.pShots[0].active && g.pShots[0].kind == SHOT_CANNON);
	CHECK(g.pShots[0].vx < 0);            // facing left
	step((tInput){.fire = 1}, FIRE_REPEAT);
	CHECK(g.pShots[1].active);            // auto-fire
}

static void testCannonOpensBarracks(void) {
	newGame();
	int16_t bx = g.pBarracks[3].x;
	flyAt((int16_t)(bx + 80), FEET_Y - 8);
	g.chop.y = GROUND_Y - 4;   // low, still airborne... land to shoot level
	g.chop.landed = 1; g.chop.y = GROUND_Y;
	// shot y = GROUND_Y - 6 = 230, inside the barracks (212..240)
	step((tInput){.fire = 1}, 1);
	step(NONE, 30);
	CHECK(g.pBarracks[3].hp == 1);
	step((tInput){.fire = 1}, 1);
	int opened = 0;
	for(int i = 0; i < 30; ++i) { step(NONE, 1); if(g.events & EV_OPEN) opened = 1; }
	CHECK(opened);
	CHECK(g.pBarracks[3].hp == 0);
	step(NONE, 20 * 12 + 40);
	CHECK(g.pBarracks[3].released == HOSTAGES_PER_BARRACKS);
}

static void testBombOpensBarracks(void) {
	newGame();
	int16_t bx = g.pBarracks[2].x;
	flyAt(bx, 100);
	g.chop.face = g.chop.faceTarget = FACE_FRONT;
	step((tInput){.fire = 1}, 1);
	step(NONE, 60);
	CHECK(g.pBarracks[2].hp == 0);
}

static void openBarracks(int b) {
	g.pBarracks[b].hp = 0;
	g.pBarracks[b].timer = 0;
	for(int i = 0; i < 16 * 13 + 2; ++i) step(NONE, 1);
}

static void testHostagesBoardLandedChopperUpToSixteen(void) {
	newGame();
	g.spawnTimer = 60000;
	flyAt((int16_t)(g.pBarracks[3].x + 200), 100);  // away from them
	openBarracks(3);
	openBarracks(2);
	// land between the two barracks' people? no: next to barracks 3 only
	g.chop.x = (int16_t)(g.pBarracks[3].x + 60);
	g.chop.y = GROUND_Y - 1;
	g.chop.vy = 32;
	step(NONE, 1);
	CHECK(g.chop.landed);
	step(NONE, 300);
	CHECK(g.aboard == 16 - (g.killed));
	// all boarded from barracks 3 (minus any squashed on landing)
	CHECK(g.aboard + g.killed == 16);
	CHECK(g.aboard <= CABIN_MAX);
}

static void testCabinFullStopsBoarding(void) {
	newGame();
	g.spawnTimer = 60000;
	flyAt(1400, 100);
	openBarracks(1);
	openBarracks(2);
	// gather everyone near x = 1400 (between the barracks)
	for(int i = 16; i < 48; ++i) { if(g.pHostages[i].state != HS_DEAD) logicHostageOut(&g, (uint8_t)i, HS_FREE, (int16_t)(1340 + i)); }
	g.chop.x = 1300; g.chop.y = GROUND_Y - 1; g.chop.vy = 32;
	step(NONE, 1);
	CHECK(g.chop.landed);
	step(NONE, 400);
	CHECK(g.aboard == CABIN_MAX);
	CHECK(freeCount() == 32 - CABIN_MAX);
}

static void testLandingOnPeopleKillsThem(void) {
	newGame();
	g.spawnTimer = 60000;
	logicHostageOut(&g, 0, HS_FREE, 800);
	flyAt(800, GROUND_Y - 1);
	g.chop.vy = 32;
	step(NONE, 1);
	CHECK(g.chop.landed);
	CHECK(g.pHostages[0].state == HS_DEAD);
	CHECK(g.killed == 1);
}

static void testUnloadAtBaseSavesAndCountsTrip(void) {
	newGame();
	g.spawnTimer = 60000;
	for(int i = 0; i < 5; ++i) g.pHostages[i].state = HS_ABOARD;
	g.aboard = 5;
	step(NONE, 5 * UNLOAD_FRAMES + 1);
	CHECK(g.aboard == 0);
	CHECK(g.trip == 1);
	step(NONE, 200);
	CHECK(g.saved == 5);
	CHECK(g.score >= 500);
}

static void testNoUnloadAwayFromBase(void) {
	newGame();
	g.spawnTimer = 60000;
	g.pHostages[0].state = HS_ABOARD;
	g.aboard = 1;
	flyAt(1500, GROUND_Y - 1);
	g.chop.vy = 32;
	step(NONE, 100);
	CHECK(g.chop.landed && g.aboard == 1);
}

static void testShotDownLosesPassengers(void) {
	newGame();
	g.spawnTimer = 60000;
	for(int i = 0; i < 7; ++i) g.pHostages[i].state = HS_ABOARD;
	g.aboard = 7;
	flyAt(1500, 150);
	g.chop.damaged = 1;   // the second hit: the first only damages (testFirstHitDamages)
	// an enemy shell right on the chopper
	g.pEShots[0] = (tShot){.x = 1500, .y = 144, .vx = 0, .vy = 0, .active = 1, .kind = ESHOT_SHELL, .life = 50};
	step(NONE, 1);
	CHECK(!g.chop.alive);
	CHECK(g.killed == 7 && g.aboard == 0);
	CHECK(g.lives == LIVES_START - 1);
	CHECK(g.state == GS_LOST);
	step(NONE, LOST_FRAMES + 1);
	CHECK(g.state == GS_PLAY);
	CHECK(g.chop.alive && g.chop.landed && g.chop.x == START_X);
}

static void testGameOverAfterLastChopper(void) {
	newGame();
	for(int life = 0; life < LIVES_START; ++life) {
		g.spawnTimer = 60000;
		flyAt(1500, 150);
		g.chop.damaged = 1;
		g.pEShots[0] = (tShot){.x = 1500, .y = 144, .active = 1, .kind = ESHOT_SHELL, .life = 50};
		step(NONE, 1);
		step(NONE, LOST_FRAMES + 1);
	}
	CHECK(g.lives == 0);
	CHECK(g.state == GS_OVER);
	step(NONE, OVER_FRAMES + 1);
	CHECK(g.state == GS_TITLE);
}

static void testMissionClearWhenAllAccountedFor(void) {
	newGame();
	g.spawnTimer = 60000;
	for(int i = 0; i < HOSTAGE_COUNT; ++i) g.pHostages[i].state = HS_SAVED;
	g.saved = 60;
	g.killed = 4;
	step(NONE, 1);
	CHECK(g.state == GS_CLEAR);
	step(NONE, CLEAR_FRAMES + 1);
	CHECK(g.state == GS_INTRO && g.mission == 1);
	CHECK(g.saved == 0 && g.trip == 0);
	CHECK(g.lives == LIVES_START);   // lives carry over
}

// The game ends after the last mission built: world 2's third (world 1's third goes on to world 2)
static void testVictoryAfterLastMission(void) {
	newGame();
	g.world = WORLD_COUNT - 1;
	g.mission = MISSION_COUNT - 1;
	g.saved = HOSTAGE_COUNT;
	step(NONE, 1);
	CHECK(g.state == GS_CLEAR);
	step(NONE, CLEAR_FRAMES + 1);
	CHECK(g.state == GS_VICTORY);
}

static void testDifficultySchedule(void) {
	newGame();
	CHECK(logicTankLimit(&g) == 1);
	CHECK(!logicJetsOn(&g) && !logicMinesOn(&g));
	g.trip = 1;
	CHECK(logicTankLimit(&g) == 2 && logicJetsOn(&g) && !logicMinesOn(&g));
	g.trip = 2;
	CHECK(logicMinesOn(&g));
	g.trip = 9;
	CHECK(logicTankLimit(&g) == TANK_MAX);
}

static void testTankShootsLowChopper(void) {
	newGame();
	flyAt(1500, GROUND_Y - 10);
	g.pTanks[0] = (tTank){.x = 1400, .active = 1, .hp = 2, .dir = 1, .cool = 0};
	step(NONE, 1);
	// first the tell: the tank stops, barrel up, no shell yet
	CHECK(g.events & EV_TANKTELL);
	CHECK(g.pTanks[0].tell == logicTankTell(&g) && !g.pEShots[0].active);
	tDraw pDraw[DRAW_MAX];
	uint8_t n = logicDraw(&g, pDraw), isAim = 0;
	for(uint8_t i = 0; i < n; ++i) isAim |= pDraw[i].bob == BOB_TANK && pDraw[i].frame >= TANK_AIM_L;
	CHECK(isAim);
	int16_t x0 = g.pTanks[0].x;
	step(NONE, logicTankTell(&g) - 1);
	CHECK(!g.pEShots[0].active && g.pTanks[0].x == x0);   // braced, still no shell
	step(NONE, 1);
	CHECK(g.events & EV_TANKFIRE);
	CHECK(g.pEShots[0].active && g.pEShots[0].vx > 0);
}

// The fairness rule: no shell without a tell. Play long stretches with tanks
// round a landed chopper on every difficulty; every shell must come from a tank
// that spent at least logicTankTell() frames, unbroken, in its wind-up, and that
// tell is at least 12 frames on every difficulty, 12-18 on normal.
static void testNoShellWithoutTell(void) {
	CHECK(logicTankTell(&g) >= 12);
	uint16_t uwShells = 0;
	for(uint8_t d = 0; d <= DIFFICULTY_MAX; ++d) {
		newGame();
		logicSetDifficulty(&g, d);
		CHECK(logicTankTell(&g) >= 12);
		if(d == DIFFICULTY_NORMAL) CHECK(logicTankTell(&g) >= 12 && logicTankTell(&g) <= 18);
		flyAt(1500, GROUND_Y);
		g.chop.landed = 1;
		uint8_t ubTell = logicTankTell(&g);
		for(uint8_t i = 0; i < 4; ++i) {
			g.pTanks[i] = (tTank){.x = (int16_t)(1300 + 60 * i), .active = 1, .hp = 2, .dir = 1,
				.cool = (uint8_t)(7 * i)};
		}
		uint8_t pRun[TANK_MAX] = {0};
		for(uint16_t f = 0; f < 3000; ++f) {
			uint8_t pBefore[TANK_MAX];
			for(uint8_t i = 0; i < TANK_MAX; ++i) pBefore[i] = pRun[i];
			step(NONE, 1);
			if(g.state != GS_PLAY) break;
			// new shells this frame
			uint8_t ubNew = 0;
			for(uint8_t s = 0; s < ESHOT_MAX; ++s) {
				const tShot *p = &g.pEShots[s];
				ubNew += p->active && p->kind == ESHOT_SHELL && p->life == SHELL_LIFE;
			}
			uint8_t ubFired = 0;
			for(uint8_t i = 0; i < TANK_MAX; ++i) {
				const tTank *t = &g.pTanks[i];
				if(t->active && t->fireAnim == 8) {
					++ubFired;
					// its tell ran out this frame, after ubTell frames in a row
					CHECK(pBefore[i] >= ubTell && !t->tell);
				}
				pRun[i] = t->active && t->tell ? (uint8_t)(pRun[i] + 1) : 0;
			}
			CHECK(ubNew == ubFired);
			uwShells += ubNew;
			// (taken away before they arrive: the chopper stays put)
			for(uint8_t s = 0; s < ESHOT_MAX; ++s) g.pEShots[s].active = 0;
		}
	}
	CHECK(uwShells > 50);   // (it did shoot)
}

static void testTankIgnoresHighChopper(void) {
	newGame();
	flyAt(1500, 80);
	g.pTanks[0] = (tTank){.x = 1400, .active = 1, .hp = 2, .dir = 1, .cool = 0};
	step(NONE, 5);
	CHECK(!g.pEShots[0].active);
}

static void testTanksStayInEnemyGround(void) {
	newGame();
	flyAt(START_X - 40, 200);
	g.pTanks[0] = (tTank){.x = BORDER_X - 30, .active = 1, .hp = 2, .dir = 1, .cool = 255};
	step(NONE, 100);
	CHECK(g.pTanks[0].x <= BORDER_X - 24);
}

static void testBombKillsTank(void) {
	newGame();
	flyAt(1500, 150);
	g.pTanks[0] = (tTank){.x = 1500, .active = 1, .hp = 2, .dir = 1, .cool = 255};
	g.chop.face = g.chop.faceTarget = FACE_FRONT;
	step((tInput){.fire = 1}, 1);
	step(NONE, 40);
	CHECK(!g.pTanks[0].active);
	CHECK(g.score >= 50);
}

static void testCannonKillsJet(void) {
	newGame();
	flyAt(1500, 120);
	g.trip = 0; g.mission = 0;   // jets off: place one by hand
	g.pJets[0] = (tJet){.x = 1440, .y = 114, .dir = 1, .active = 1, .hp = 1, .cool = 200};
	step((tInput){.fire = 1}, 3);
	CHECK(!g.pJets[0].active);
	CHECK(g.chop.alive);
}

static void testMineContactDestroysChopper(void) {
	newGame();
	flyAt(1500, 120);
	g.chop.damaged = 1;
	g.pMines[0] = (tMine){.x = 1500, .y = 112, .active = 1};
	step(NONE, 1);
	CHECK(!g.chop.alive);
}

static void testCameraFollowsWithLead(void) {
	newGame();
	flyAt(1500, 120);
	step(NONE, 100);
	CHECK(g.cam == 1500 - SCREEN_W / 2 - 56);   // facing left: more room ahead
}

// Smooth scrolling: through take-off, flying both ways, letting go and turning
// (including the world's right edge by the base), the camera's step changes by
// at most 1 px a frame and the chopper never jumps on screen: it is locked to
// the camera and only drifts with the gliding look-ahead (<= 2 px a frame).
static void testCameraIsSmooth(void) {
	newGame();
	logicSetDifficulty(&g, DIFFICULTY_INVINCIBLE);
	g.spawnTimer = 60000;
	CHECK(g.state == GS_PLAY);
	tInput pIn[] = {{0, -1, 0, 0}, {-1, 0, 0, 0}, {0, 0, 0, 0}, {1, 0, 0, 0}, {0, 0, 0, 0}, {-1, 0, 0, 0}};
	int pFrames[] = {40, 120, 70, 200, 60, 40};
	int16_t wCam = g.cam, wScr = (int16_t)(g.chop.x - g.cam), wStep = 0;
	int worstChange = 0, worstScr = 0, n = 0;
	for(int s = 0; s < 6; ++s) {
		for(int f = 0; f < pFrames[s]; ++f, ++n) {
			logicUpdate(&g, &pIn[s]);
			int16_t st = (int16_t)(g.cam - wCam), sc = (int16_t)(g.chop.x - g.cam - wScr);
			int ch = st > wStep ? st - wStep : wStep - st;
			if(ch > worstChange) worstChange = ch;
			// at the world's edge the camera stops and the chopper flies on: real motion
			uint8_t isFree = g.cam > 0 && g.cam < WORLD_W - SCREEN_W;
			if(isFree && (sc < 0 ? -sc : sc) > worstScr) worstScr = sc < 0 ? -sc : sc;
			wCam = g.cam; wScr = (int16_t)(g.chop.x - g.cam); wStep = st;
		}
	}
	CHECK(worstChange <= 1);
	CHECK(worstScr <= 2);
	CHECK(g.cam == WORLD_W - SCREEN_W || g.cam < WORLD_W - SCREEN_W);
	printf("smooth camera: %d frames, step changes <= %d px, chopper drifts <= %d px a frame\n",
		n, worstChange, worstScr);
}

// After a crash far from the base, the camera pans to the new chopper: speeds
// up and slows by 1 px a frame and never reverses (it used to overshoot the
// base and swing back and forth).
static void testRespawnPan(void) {
	newGame();
	g.spawnTimer = 60000;
	step((tInput){.dy = -1}, 40);
	step((tInput){.dx = -1}, 300);
	g.chop.damaged = 1;
	g.pMines[0] = (tMine){.x = g.chop.x, .y = (int16_t)(g.chop.y - 8), .active = 1};
	int16_t wCam = g.cam, wStep = 0;
	int reversals = 0, worstChange = 0, frames = 0;
	for(int f = 0; f < 400; ++f) {
		logicUpdate(&g, &NONE);
		int16_t st = (int16_t)(g.cam - wCam);
		if(g.state == GS_PLAY) {
			++frames;
			if((st > 0 && wStep < 0) || (st < 0 && wStep > 0)) ++reversals;
			int ch = st > wStep ? st - wStep : wStep - st;
			if(ch > worstChange) worstChange = ch;
		}
		wCam = g.cam; wStep = st;
	}
	CHECK(g.state == GS_PLAY && g.chop.x == START_X);
	CHECK(reversals == 0);
	CHECK(worstChange <= 1);
	CHECK(g.chop.x - g.cam == SCREEN_W / 2 - (g.camLead >> 4) || g.cam == WORLD_W - SCREEN_W);
	printf("respawn pan: %d frames, no reversals, step changes <= %d px\n", frames, worstChange);
}

// The far sky: the cloud band sits in the open sky above the mountains; the
// sprite dots fit the sky lines with a line between them (DMA lists), their x
// stays in range and moves at 1/16 of the camera.
static void testSkyLayer(void) {
	CHECK(CLOUDS_Y >= PLAY_TOP + 8);
	for(int i = 0; i < SKYDOT_COUNT; ++i) {
		CHECK(g_pSkyDots[i].y >= PLAY_TOP && g_pSkyDots[i].y + g_pSkyDots[i].h < 120);
		if(i) CHECK(g_pSkyDots[i].y >= g_pSkyDots[i - 1].y + g_pSkyDots[i - 1].h + 2);
	}
	newGame();
	int shown = 0;
	for(int c = 0; c < WORLD_W - SCREEN_W; c += 7) {
		g.cam = (int16_t)c;
		for(int i = 0; i < SKYDOT_COUNT; ++i) {
			int16_t x = logicSkyDotX(&g, (uint8_t)i);
			CHECK(x == SKY_HIDDEN || (x >= -32 && x < SCREEN_W));
			shown += x != SKY_HIDDEN;
		}
	}
	CHECK(shown > 0);
	g.cam = 800;
	int16_t x0 = logicSkyDotX(&g, 2);
	g.cam = 816;
	int16_t x1 = logicSkyDotX(&g, 2);
	CHECK(x0 == SKY_HIDDEN || x1 == SKY_HIDDEN || x0 - x1 == 1);   // 16 px of camera = 1 px
	CHECK(logicSkyDotsAreStars(1) && !logicSkyDotsAreStars(0));
	printf("sky: %d dot placements on screen over the world\n", shown);
}

static void testScrollMaths(void) {
	CHECK(logicScrollDelay(0) == 0 && logicScrollByteOffset(0) == -2);
	CHECK(logicScrollDelay(1) == 15 && logicScrollByteOffset(1) == 0);
	CHECK(logicScrollDelay(16) == 0 && logicScrollByteOffset(16) == 0);
	CHECK(logicScrollDelay(17) == 15 && logicScrollByteOffset(17) == 2);
	CHECK(logicBandOffset(1000, 2, 320) == 250 - 0);
	CHECK(logicBandOffset(2000, 2, 320) == 500 - 320);
	CHECK(logicBandOffset(2000, 1, 336) == 1000 - 672);
}

// AGA's quarter-pixel scroll: whole pixels give exactly the OCS values; a band
// at camera / 4 steps 0.75 px a frame at a 3 px camera, every frame
static void testScrollMathsAga(void) {
	for(uint16_t x = 0; x < 320; ++x) {
		CHECK(logicScrollCon1Pf2Aga((uint16_t)(x * 4)) == (uint16_t)(logicScrollDelay(x) << 4));
		CHECK(logicScrollByteOffset4((uint16_t)(x * 4)) == logicScrollByteOffset(x));
	}
	for(int16_t c = 0; c < WORLD_W - SCREEN_W; ++c) {
		for(uint8_t s = 0; s < 3; ++s) {
			uint16_t x4 = logicBandOffset4(c, s, 320);
			CHECK(x4 < 4 * 320);
			CHECK((x4 >> 2) == logicBandOffset(c, s, 320));   // OCS: the quarters dropped
		}
	}
	// 0.25 px: one quarter more delay than the whole pixel above it, in bits 13-12
	CHECK(logicScrollCon1Pf2Aga(5) == ((14 << 4) | (3 << 12)));    // x 1.25: delay 14.75 px
	CHECK(logicScrollCon1Pf2Aga(6) == ((14 << 4) | (2 << 12)));    // x 1.50: 14.50
	CHECK(logicScrollByteOffset4(5) == 0);
	// the mountains (camera / 4) at 3 px a frame: 3 quarters a frame, every frame
	uint16_t uwPrev = logicBandOffset4(1000, 2, 320);
	for(int16_t c = 1003; c < 1300; c += 3) {
		uint16_t x4 = logicBandOffset4(c, 2, 320);
		CHECK((uint16_t)((x4 - uwPrev + 1280) % 1280) == 3);
		uwPrev = x4;
	}
	// the hills (camera / 2): 6 quarters (1.5 px)
	CHECK(logicBandOffset4(1003, 1, 320) - logicBandOffset4(1000, 1, 320) == 6);
}

// The plain's perspective floor (Paul, 2026-11-12): floor line j moves at
// (18 + j) / 72 of the camera, linear down the screen (1/z on a flat plain):
// line 0 at the mountains' / 4, the last at the hills' / 2; the far strip
// above is the mountains band itself (one speed); every row repeats after the
// same FLOOR_PERIOD of camera
static void testFloorSpeeds(void) {
	CHECK(FLOOR_LINES == 19 && FLOOR_PERIOD == 576 && PLAIN_FAR_ROWS == 10);
	CHECK(logicFloorRowW(0) == 144 && logicFloorRowW(FLOOR_LINES - 1) == 288);
	for(uint8_t j = 0; j < FLOOR_LINES; ++j) {
		uint16_t uwW4 = (uint16_t)(4 * logicFloorRowW(j));
		CHECK(logicFloorRowW(j) + 42 * 8 <= 640);   // the fetch (336 px) never leaves the 640 px bitmap row
		for(int16_t c = 0; c < WORLD_W - SCREEN_W; ++c) {
			uint16_t x4 = logicFloorX4(c, j);
			CHECK(x4 < uwW4);
			if(c + FLOOR_PERIOD < WORLD_W - SCREEN_W) CHECK(logicFloorX4((int16_t)(c + FLOOR_PERIOD), j) == x4);
		}
		// over 72 px of camera: 72 + 4 j quarters, exactly
		for(int16_t c = 0; c < WORLD_W - SCREEN_W - 72; c = (int16_t)(c + 13)) {
			uint16_t a = logicFloorX4(c, j), b = logicFloorX4((int16_t)(c + 72), j);
			uint16_t d = (uint16_t)((b + uwW4 - a) % uwW4);
			CHECK(d == 72 + 4 * j);
		}
	}
	// the far edge = the mountains (and so the far strip, their last rows): 1/4
	// px a camera px; the near edge = the hills: 1/2
	CHECK((logicBandOffset4(1072, 2, 320) - logicBandOffset4(1000, 2, 320)) == 72 + 4 * 0);
	CHECK((logicBandOffset4(1072, 1, 320) - logicBandOffset4(1000, 1, 320)) == 72 + 4 * (FLOOR_LINES - 1));
	// at 3 px a frame line 6 moves 1 px a frame, line 12 1.25 (5 px in 4 frames)
	CHECK((logicFloorX4(1003, 6) - logicFloorX4(1000, 6) + 4 * logicFloorRowW(6)) % (4 * logicFloorRowW(6)) == 4);
	CHECK((logicFloorX4(1012, 12) - logicFloorX4(1000, 12) + 4 * logicFloorRowW(12)) % (4 * logicFloorRowW(12)) == 20);
}

// What logicFloorScroll writes into the copper list, per floor line: BPLCON1
// (OCS: whole pixels; AGA: with the quarters) and the modulo to the next row's
// start, from the same maths as the bands'
static void testFloorCopper(void) {
	enum { STRIDE = 4, ROW = 240, BASE = ROW - 42 };
	uint16_t pOut[FLOOR_LINES * STRIDE];
	for(uint8_t isAga = 0; isAga < 2; ++isAga) {
		uint16_t uwMask = isAga ? 0xFFFF : 0xFFFC;
		for(int16_t c = 0; c < WORLD_W - SCREEN_W; c = (int16_t)(c + 7)) {
			for(uint16_t i = 0; i < FLOOR_LINES * STRIDE; ++i) pOut[i] = 0xDEAD;
			int16_t wOff0 = logicFloorScroll(c, uwMask, BASE, pOut, STRIDE);
			int16_t pOff[FLOOR_LINES];
			for(uint8_t j = 0; j < FLOOR_LINES; ++j) {
				uint16_t x4 = (uint16_t)(logicFloorX4(c, j) & uwMask);
				pOff[j] = logicScrollByteOffset4(x4);
				CHECK(pOut[j * STRIDE] == logicScrollCon1Pf2Aga(x4));
				if(!isAga) CHECK(pOut[j * STRIDE] == (uint16_t)(logicScrollDelay((uint16_t)(x4 >> 2)) << 4));
				CHECK(pOut[j * STRIDE + 1] == 0xDEAD && pOut[j * STRIDE + 3] == 0xDEAD);   // only its two words
			}
			CHECK(wOff0 == pOff[0]);
			for(uint8_t j = 0; j + 1 < FLOOR_LINES; ++j) CHECK(pOut[j * STRIDE + 2] == (uint16_t)(BASE + pOff[j + 1] - pOff[j]));
		}
	}
}

// World 2's sea (STYLE-world2 section 2): line j at (28 + 3 j) / 112 of the
// camera, so line 180 moves at 1/4 and line 207 at 109/112; each row loops in
// its own 4 (28 + 3 j) px; logicSeaScroll's copper values equal the exact
// reference at every camera of the period (and past it), OCS and AGA
static void testSeaCopper(void) {
	enum { STRIDE = 7, ROW = 288, BASE = ROW - 42 };
	uint16_t pOut[SEA_LINES * STRIDE];
	CHECK(logicSeaX4(4, 0) == 4);                       // 1/4: 1 px for 4 of camera (4 quarters)
	CHECK(logicSeaX4(112, SEA_LINES - 1) == 436);       // 109/112: 109 px for 112
	for(uint8_t j = 0; j < SEA_LINES; ++j) {
		CHECK(logicSeaX4(SEA_PERIOD - 1, j) < 16u * (28u + 3u * j));   // inside its loop
		CHECK(logicSeaX4(SEA_PERIOD, j) == 0);                          // the period
	}
	for(uint8_t isAga = 0; isAga < 2; ++isAga) {
		uint16_t uwMask = isAga ? 0xFFFF : 0xFFFC;
		for(int16_t c = 0; c < W2_W - SCREEN_W; c = (int16_t)(c + (c < SEA_PERIOD ? 1 : 13))) {
			for(uint16_t i = 0; i < SEA_LINES * STRIDE; ++i) pOut[i] = 0xDEAD;
			int16_t wOff0 = logicSeaScroll(c, uwMask, BASE, pOut, STRIDE);
			int16_t pOff[SEA_LINES];
			for(uint8_t j = 0; j < SEA_LINES; ++j) {
				uint16_t x4 = (uint16_t)(logicSeaX4(c, j) & uwMask);
				pOff[j] = logicScrollByteOffset4(x4);
				CHECK(pOut[j * STRIDE] == logicScrollCon1Pf2Aga(x4));
				CHECK(pOut[j * STRIDE + 1] == 0xDEAD && pOut[j * STRIDE + 3] == 0xDEAD);
				// the fetch (21 words from the row's offset) stays inside a 768 px row
				CHECK(pOff[j] >= -2 && pOff[j] + 42 <= 96);
			}
			CHECK(wOff0 == pOff[0]);
			for(uint8_t j = 0; j + 1 < SEA_LINES; ++j) CHECK(pOut[j * STRIDE + 2] == (uint16_t)(BASE + pOff[j + 1] - pOff[j]));
			CHECK(pOut[(SEA_LINES - 1) * STRIDE + 2] == 0xDEAD);   // the last line's modulo: the list's own
		}
	}
}

// A 24-bit gradient: its ends exact, every channel monotonic between them
static void testGrad24(void) {
	CHECK(logicGrad24(0x2488CC, 0xCCFFEE, 0, 156) == 0x2488CC);
	CHECK(logicGrad24(0x2488CC, 0xCCFFEE, 155, 156) == 0xCCFFEE);
	CHECK(logicGrad24(0xCCFFEE, 0x1A7C9A, 27, 28) == 0x1A7C9A);
	uint32_t ulPrev = logicGrad24(0xCCFFEE, 0x1A7C9A, 0, 28);
	for(uint8_t i = 1; i < 28; ++i) {
		uint32_t ul = logicGrad24(0xCCFFEE, 0x1A7C9A, i, 28);
		for(uint8_t s = 0; s < 24; s = (uint8_t)(s + 8)) CHECK(((ul >> s) & 0xFF) <= ((ulPrev >> s) & 0xFF));
		ulPrev = ul;
	}
	uint16_t pSea[SEA_LINES];
	logicGradFill(0xCFE, 0x179, SEA_LINES, pSea);
	CHECK(pSea[0] == 0xCFE && pSea[SEA_LINES - 1] == 0x179);
}

// World 2's display slice: the debug start puts a game on world 2's map, at its
// base pad, with no barracks, enemies or weather, the camera free over all
// 4608 px; a new game is world 1 again, unchanged
static void testWorld2Start(void) {
	logicInit(&g);
	logicStartAt(&g, 1, 0);
	logicSetDifficulty(&g, 2);   // (nothing shoots here: skill 3 has jets from the first trip, 2026-11-17)
	CHECK(g.world == 1 && g.mission == 0 && g.state == GS_INTRO && g.lives == LIVES_START);
	CHECK(g.chop.x == W2_START_X && logicWorldW(&g) == W2_W);
	step(NONE, INTRO_FRAMES + 1);
	CHECK(g.state == GS_PLAY);
	g.spawnTimer = 1;
	tInput left = {0};
	left.dx = -1;
	tInput up = {0};
	up.dy = -1;
	step(up, 40);
	for(int i = 0; i < 2000; ++i) {
		step(left, 1);
		for(uint8_t t = 0; t < TANK_MAX; ++t) CHECK(!g.pTanks[t].active);
		for(uint8_t t = 0; t < JET_MAX; ++t) CHECK(!g.pJets[t].active);
		for(uint8_t t = 0; t < MINE_MAX; ++t) CHECK(!g.pMines[t].active);
	}
	CHECK(g.cam == 0 && g.chop.x < 100);
	tWeatherFrame w;
	CHECK(!logicWeather(&g, &w));
	tDraw pDraw[DRAW_MAX];
	uint8_t n = logicDraw(&g, pDraw);
	for(uint8_t i = 0; i < n; ++i) CHECK(pDraw[i].bob != BOB_HANGAR && pDraw[i].bob != BOB_BARRACKS && pDraw[i].bob != BOB_FENCE);
	tInput right = {0};
	right.dx = 1;
	step(right, 1800);
	CHECK(g.cam == W2_W - SCREEN_W && g.chop.x <= W2_W - 24 && g.chop.x > W2_W - 200);
	CHECK(logicRadarX2(0) == 0 && logicRadarX2(W2_W - 1) == RADAR_W - 1 && logicRadarX2(2400) == 100);
	logicStartAt(&g, 1, 2);   // mission 6, from play: lives and score kept
	CHECK(g.world == 1 && g.mission == 2 && g.chop.x == W2_START_X);
	logicStartGame(&g);
	CHECK(g.world == 0 && g.chop.x == START_X && logicWorldW(&g) == WORLD_W);
	CHECK(logicTankLimit(&g) >= 1);
	logicStartAt(&g, 1, 0);
	g.state = GS_OVER;
	g.stateTimer = 1;
	step(NONE, 2);
	CHECK(g.world == 0);   // the title / hall of fame are world 1's
}

// World 2's floor hook: island tiles are ground, water and reef are not
static void testTerrainFloor(void) {
	uint8_t pMap[W2_MAP_COLS];
	for(uint16_t i = 0; i < W2_MAP_COLS; ++i) pMap[i] = (uint8_t)(i % 36);
	CHECK(logicTerrainFloorY(pMap, 0) == TERRAIN_WATER);            // water0
	CHECK(logicTerrainFloorY(pMap, 7 * 16 + 15) == TERRAIN_WATER);  // water7
	CHECK(logicTerrainFloorY(pMap, 8 * 16) == GROUND_Y);            // flat0
	CHECK(logicTerrainFloorY(pMap, 32 * 16 + 3) == TERRAIN_WATER);  // reef0
	CHECK(logicTerrainFloorY(pMap, 34 * 16) == GROUND_Y);           // pad bed
	CHECK(logicTerrainFloorY(pMap, -1) == TERRAIN_WATER && logicTerrainFloorY(pMap, W2_W) == TERRAIN_WATER);
}

// The plain's ground under the floor (COLOR00): from the sky's own colour at
// the floor's first line (no seam with the haze above) to C, the land
// palette's pen 6, at its last (the hills stand on it), line-dithered
static void testFloorGround(void) {
	for(uint8_t m = 0; m < MISSION_COUNT; ++m) {
		uint16_t pBg[FLOOR_LINES];
		logicFloorFillGround(m, pBg);
		CHECK(pBg[0] == logicSkyColor(m, FLOOR_Y - PLAY_TOP));
		CHECK(pBg[FLOOR_LINES - 1] == g_pMissionLandPal[m][6]);
		for(uint8_t s = 0; s < 12; s = (uint8_t)(s + 4)) {
			int16_t a = (int16_t)((pBg[0] >> s) & 15), b = (int16_t)((pBg[FLOOR_LINES - 1] >> s) & 15);
			for(uint8_t j = 1; j < FLOOR_LINES; ++j) {
				int16_t p = (int16_t)((pBg[j - 1] >> s) & 15), q = (int16_t)((pBg[j] >> s) & 15);
				CHECK(q - p <= 1 && p - q <= 1);                          // one step at most
				CHECK((q >= (a < b ? a : b)) && (q <= (a > b ? a : b)));  // inside the ramp
			}
		}
	}
}

// The OCS sky's line dither: each line within 3/4 step of the true gradient
// per channel, neighbours at most one step apart, two lines together within
// 1/4 step (the half steps show), and twice the shades of the plain sky
static uint16_t pairKey(uint8_t m, uint8_t y) {
	// what the eye sees: two lines together, per channel (5 bits each)
	uint16_t a = logicSkyColorDither(m, (uint8_t)(y - 1)), b = logicSkyColorDither(m, y), k = 0;
	for(uint8_t s = 0; s < 3; ++s) k |= (uint16_t)((((a >> (4 * s)) & 15) + ((b >> (4 * s)) & 15)) << (5 * s));
	return k;
}

static void testSkyDither(void) {
	for(uint8_t m = 0; m < MISSION_COUNT; ++m) {
		uint16_t pFill[SKY_LINES];
		logicSkyFillDither(m, pFill);
		for(uint8_t y = 0; y < SKY_LINES; ++y) CHECK(pFill[y] == logicSkyColorDither(m, y));
		uint8_t ubPlainSteps = 0, ubDitherShades = 0;
		for(uint8_t y = 0; y < SKY_LINES; ++y) {
			uint16_t c = logicSkyColorDither(m, y);
			uint32_t ul24 = logicSkyColor24(m, y);   // the true gradient, x17 per channel
			for(uint8_t s = 0; s < 3; ++s) {
				int16_t v = (int16_t)((c >> (4 * s)) & 15) * 68;          // in 1/4 of 1/17 steps
				int16_t t = (int16_t)((ul24 >> (8 * s)) & 255) * 4;
				CHECK(v - t <= 54 && t - v <= 54);                        // 3/4 step (+ the 8-bit rounding)
				if(y) {
					int16_t p = (int16_t)((logicSkyColorDither(m, (uint8_t)(y - 1)) >> (4 * s)) & 15);
					int16_t d = (int16_t)(((c >> (4 * s)) & 15) - p);
					CHECK(d >= -1 && d <= 1);
					// a pair of lines averages to within 1/4 step of the gradient there
					int16_t t2 = (int16_t)(((logicSkyColor24(m, (uint8_t)(y - 1)) >> (8 * s)) & 255) * 4 + t);
					CHECK(absi(v + p * 68 - t2) <= 2 * 17 + 8);
				}
			}
			if(y && logicSkyColor(m, y) != logicSkyColor(m, (uint8_t)(y - 1))) ++ubPlainSteps;
			if(y >= 2 && pairKey(m, y) != pairKey(m, (uint8_t)(y - 1))) ++ubDitherShades;
		}
		printf("sky %d: %d plain steps, %d dithered shades\n", m, ubPlainSteps, ubDitherShades);
		CHECK(ubDitherShades >= 2 * ubPlainSteps - 3);
	}
}

static void testSkyGradient(void) {
	CHECK(logicSkyColor(0, 0) == 0x214);
	CHECK(logicSkyColor(0, SKY_LINES - 1) == 0xE75);
	// the A1200's: the same ends in 24 bits, a step of at most 2 a channel a
	// line (smooth), and within a 12-bit step of the OCS gradient everywhere
	for(uint8_t m = 0; m < MISSION_COUNT; ++m) {
		CHECK(logicSkyColor24(m, 0) == ((logicSkyColor(m, 0) & 0xF00) * 0x1100 | (logicSkyColor(m, 0) & 0x0F0) * 0x110 | (logicSkyColor(m, 0) & 0x00F) * 0x11));
		uint32_t ulPrev = logicSkyColor24(m, 0);
		for(uint8_t y = 1; y < SKY_LINES; ++y) {
			uint32_t c = logicSkyColor24(m, y);
			uint16_t o = logicSkyColor(m, y);
			for(uint8_t s = 0; s < 3; ++s) {
				int d = (int)((c >> (8 * s)) & 0xFF) - (int)((ulPrev >> (8 * s)) & 0xFF);
				CHECK(d >= -2 && d <= 2);
				int e = (int)((c >> (8 * s)) & 0xFF) - (int)((o >> (4 * s)) & 15) * 17;
				CHECK(e >= -9 && e <= 9);
			}
			ulPrev = c;
		}
	}
	CHECK(logicSkyColor24(0, SKY_LINES - 1) == 0xEE7755);
}

static void testDrawListShowsBaseAtStart(void) {
	newGame();
	tDraw d[DRAW_MAX];
	uint8_t n = logicDraw(&g, d);
	int isHangar = 0;
	for(int i = 0; i < n; ++i) isHangar |= d[i].bob == BOB_HANGAR;
	CHECK(isHangar);
	CHECK(n < DRAW_MAX);
}

static const tDraw *findBob(const tDraw *d, uint8_t n, uint8_t ubBob) {
	for(int i = 0; i < n; ++i) if(d[i].bob == ubBob) return &d[i];
	return 0;
}

static void testChopperShadow(void) {
	tDraw d[DRAW_MAX];
	logicInit(&g);
	CHECK(!findBob(d, logicDraw(&g, d), BOB_SHADOW));      // the title: no chopper
	newGame();
	uint8_t n = logicDraw(&g, d);
	const tDraw *s = findBob(d, n, BOB_SHADOW);
	CHECK(s != 0);
	if(!s) return;
	CHECK(s->y == SHADOW_Y);
	CHECK(s->frame >> 1 == 0);                              // landed: the biggest, darkest
	CHECK(s->x + 16 == g.chop.x - g.cam);                   // right under it
	CHECK((s->frame & 1) == (s->x & 1));                    // the dither phase follows x
	CHECK(s > findBob(d, n, BOB_HANGAR));                   // on the ground, under people
	// High up: the smallest, shifted away from the sun
	flyAt(1500, CEIL_Y);
	n = logicDraw(&g, d);
	s = findBob(d, n, BOB_SHADOW);
	CHECK(s && s->frame >> 1 == SHADOW_SIZES - 1);
	CHECK(s && s->x + 16 == g.chop.x - g.cam + ((GROUND_Y - CEIL_Y) >> 4));
	CHECK(s && (s->frame & 1) == (s->x & 1));
	// Getting smaller as it climbs, never bigger
	uint8_t ubPrev = 0;
	for(int16_t y = GROUND_Y; y >= CEIL_Y; y = (int16_t)(y - 4)) {
		flyAt(1500, y);
		s = findBob(d, logicDraw(&g, d), BOB_SHADOW);
		CHECK(s && s->frame >> 1 >= ubPrev);
		if(s) ubPrev = (uint8_t)(s->frame >> 1);
	}
}

// Where particle p of channel c is this frame: 1 if it's on a slot
static int wxFind(int c, int p, int16_t *pX, int16_t *pY) {
	tWeatherFrame w;
	if(!logicWeather(&g, &w)) return 0;
	for(int k = 0; k < WX_SLOTS; ++k) {
		if(((k - w.wraps) & (WX_PARTICLES - 1)) == p) {
			*pY = (int16_t)(WX_TOP + c * (WX_PITCH / 2) + k * WX_PITCH + w.off);
			*pX = (int16_t)(g_pWxX[c][p] + w.shift);
			if(*pX >= SCREEN_W) *pX = (int16_t)(*pX - SCREEN_W);
			return 1;
		}
	}
	return 0;
}

static void testWeather(void) {
	tWeatherFrame w;
	logicInit(&g);
	CHECK(!logicWeather(&g, &w));                    // not on the title
	CHECK(logicWeatherKind(0) == WX_DUST && logicWeatherKind(1) == WX_RAIN && logicWeatherKind(2) == WX_SPRAY);
	CHECK(WX_TOP >= PLAY_TOP);
	CHECK(WX_PITCH / 2 + (WX_SLOTS - 1) * WX_PITCH + WX_PITCH - 1 + WX_TOP + WX_H <= 300 - 44);  // in the frame
	CHECK(WX_H < WX_PITCH);                          // a line between items (sprite DMA lists)
	for(int c = 0; c < 2; ++c)
		for(int p = 0; p < WX_PARTICLES; ++p) CHECK(g_pWxX[c][p] >= 0 && g_pWxX[c][p] < SCREEN_W);
	for(uint8_t m = 0; m < MISSION_COUNT; ++m) {
		newGame();
		logicStartMission(&g, m);
		step(NONE, INTRO_FRAMES + 1);
		CHECK(g.state == GS_PLAY);
		// a particle keeps its own path: its fall and drift per frame
		static const int16_t pFallQ[3] = {1, 16, -1}, pDriftQ[3] = {2, -4, 3};
		int checked = 0;
		for(int t = 0; t < 64; ++t) {
			int16_t x0, y0, x1, y1;
			uint16_t uwFall0 = g.wxFall, uwDrift0 = g.wxDrift;
			int had = wxFind(1, 5, &x0, &y0);
			CHECK(logicWeather(&g, &w) && w.off < WX_PITCH && w.shift >= 0 && w.shift < SCREEN_W);
			step(NONE, 1);
			if(had && wxFind(1, 5, &x1, &y1)) {
				// (wxFall starts mid-range at a mission: no 16-bit wrap in a test)
				CHECK(y1 - y0 == ((int)(uint16_t)(uwFall0 + pFallQ[m]) >> 2) - ((int)uwFall0 >> 2));
				int16_t dx = (int16_t)(((uwDrift0 + pDriftQ[m] + 4 * SCREEN_W) % (4 * SCREEN_W)) >> 2) - (int16_t)(uwDrift0 >> 2);
				int16_t ddx = (int16_t)(x1 - x0);
				CHECK(ddx == dx || ddx == dx + SCREEN_W || ddx == dx - SCREEN_W);
				++checked;
			}
		}
		CHECK(checked > 40);
	}
	// rain falls 4 px a frame
	newGame();
	logicStartMission(&g, 1);
	step(NONE, INTRO_FRAMES + 1);
	int16_t x0, y0, x1, y1;
	int ok = 0;
	for(int t = 0; t < 8 && !ok; ++t) {
		if(wxFind(0, 3, &x0, &y0)) {
			step(NONE, 1);
			if(wxFind(0, 3, &x1, &y1)) ok = (y1 - y0 == 4);
		}
		else step(NONE, 1);
	}
	CHECK(ok);
	// the camera carries them like the ground
	g.cam = 1000;
	CHECK(wxFind(0, 3, &x0, &y0));
	g.cam = 1003;
	CHECK(wxFind(0, 3, &x1, &y1));
	CHECK(x1 - x0 == -3 || x1 - x0 == SCREEN_W - 3);
}

static void testRank(void) {
	CHECK(logicRank(0)[0] == 'R');
	CHECK(logicRank(192)[0] == 'L');
}

static void testDeterministic(void) {
	// The same inputs give the same game
	tInput in = {.dx = -1, .dy = -1, .fire = 1};
	newGame();
	step(in, 500);
	uint32_t s1 = g.score; int16_t x1 = g.chop.x; uint16_t r1 = g.lfsr;
	newGame();
	step(in, 500);
	CHECK(g.score == s1 && g.chop.x == x1 && g.lfsr == r1);
}

static void testDifficultyDefaultsToNormalAndPersists(void) {
	logicInit(&g);
	CHECK(g.difficulty == DIFFICULTY_NORMAL);
	logicSetDifficulty(&g, 5);
	logicStartGame(&g);
	CHECK(g.difficulty == 5);              // a new game keeps it
	logicSetDifficulty(&g, DIFFICULTY_MAX + 1);
	CHECK(g.difficulty == 5);              // out of range: ignored
}

static void testInvincibleSurvivesHits(void) {
	newGame();
	logicSetDifficulty(&g, DIFFICULTY_INVINCIBLE);
	for(int i = 0; i < 5; ++i) g.pHostages[i].state = HS_ABOARD;
	g.aboard = 5;
	flyAt(1500, 150);
	g.pEShots[0] = (tShot){.x = 1500, .y = 144, .active = 1, .kind = ESHOT_SHELL, .life = 50};
	step(NONE, 1);
	CHECK(g.chop.alive && g.state == GS_PLAY);
	CHECK(g.lives == LIVES_START && g.aboard == 5 && g.killed == 0);
	CHECK(!g.pEShots[0].active);           // the shell still bursts on it
	CHECK(g.events & EV_SHRUG);
	g.pMines[0] = (tMine){.x = 1500, .y = 142, .active = 1};
	step(NONE, 1);
	CHECK(g.chop.alive && !g.pMines[0].active);
	CHECK(!(g.events & EV_CRASH));
}

static void testInvincibleKeepsHiScore(void) {
	newGame();
	logicSetDifficulty(&g, DIFFICULTY_INVINCIBLE);
	uint32_t ulHi = g.hiScore;
	flyAt(1500, 150);
	g.pTanks[0] = (tTank){.x = 1500, .active = 1, .hp = 2, .dir = 1, .cool = 255};
	g.chop.face = g.chop.faceTarget = FACE_FRONT;
	g.score = ulHi;                        // the next points would beat it
	step((tInput){.fire = 1}, 1);
	step(NONE, 40);
	CHECK(!g.pTanks[0].active && g.score > ulHi);
	CHECK(g.hiScore == ulHi);
}

static void testDifficultyScalesEnemies(void) {
	newGame();
	g.trip = 1;
	logicSetDifficulty(&g, 1);
	uint8_t ubEasyTanks = logicTankLimit(&g);
	CHECK(ubEasyTanks >= 1 && !logicJetsOn(&g));   // easy: no jets after one trip
	logicSetDifficulty(&g, 3);
	CHECK(logicTankLimit(&g) == 2 && logicJetsOn(&g) && !logicMinesOn(&g));   // normal: as before
	logicSetDifficulty(&g, 5);
	CHECK(logicTankLimit(&g) > 2 && logicMinesOn(&g));
	g.trip = 0;
	CHECK(logicJetsOn(&g));                // hardest: jets from the start
	logicSetDifficulty(&g, 0);
	CHECK(logicTankLimit(&g) == 1 && !logicJetsOn(&g));   // invincible plays like normal
}

static void testHarderTanksFireSooner(void) {
	// the same tank and target: frames until its second shell, per difficulty
	int pFrames[DIFFICULTY_MAX + 1];
	for(uint8_t d = 1; d <= DIFFICULTY_MAX; ++d) {
		newGame();
		logicSetDifficulty(&g, d);
		flyAt(1500, GROUND_Y - 10);
		g.pTanks[0] = (tTank){.x = 1400, .active = 1, .hp = 2, .dir = 1, .cool = 0};
		g.chop.landed = 1;
		int n = 0;
		while(!(g.events & EV_TANKFIRE) && n < 1000) { step(NONE, 1); ++n; }
		g.pEShots[0].active = g.pEShots[1].active = 0;
		g.chop.y = GROUND_Y;
		n = 0;
		do { step(NONE, 1); ++n; } while(!(g.events & EV_TANKFIRE) && n < 1000);
		pFrames[d] = n;
	}
	CHECK(pFrames[1] > pFrames[3] && pFrames[3] > pFrames[5]);
}

// Head-on: the sidestep is quick both ways (Choplifter's jink), side flight stays heavy
static void testHeadOnStrafeIsSnappy(void) {
	newGame();
	flyAt(2000, 120);
	armBombStance();
	step(NONE, TURN_STEP_FRAMES + 1);
	CHECK(g.chop.face == FACE_FRONT);
	g.chop.y = 120; g.chop.vy = 0;
	step((tInput){.dx = 1}, 8);
	CHECK(g.chop.vx == HO_VX_MAX && g.chop.face == FACE_FRONT);   // full speed in 8 frames, still facing front
	step((tInput){.dx = -1}, 10);
	CHECK(g.chop.vx == -HO_VX_MAX && g.chop.face == FACE_FRONT);  // turned round in 10
	step(NONE, 8);
	CHECK(g.chop.vx == 0);                                         // stopped in 8
	// facing a side, the same push is the heavy cruise
	newGame();
	flyAt(2000, 120);
	step((tInput){.dx = -1}, 8);
	CHECK(g.chop.vx == -8 * VX_ACCEL);
}

// Jets: they start well off screen (heard and on the radar first) and don't
// fire until they've been on screen a while
static void testJetsGiveWarning(void) {
	newGame();
	flyAt(2400, 120);
	g.trip = 10;                                   // jets on at any difficulty
	for(int i = 0; i < JET_MAX; ++i) g.pJets[i].cool = 1;
	g.chop.vx = -20;                               // flying left: the jet comes from the left, ahead
	step(NONE, 2);                                 // (the cool-down's last frame, then it comes)
	g.chop.vx = 0;
	int k = -1;
	for(int i = 0; i < JET_MAX; ++i) if(g.pJets[i].active) k = i;
	CHECK(k >= 0);
	if(k < 0) return;
	tJet *j = &g.pJets[k];
	CHECK(j->x < g.cam - 100 || j->x > g.cam + SCREEN_W + 100);   // well off screen
	CHECK(g.events & EV_JET);                                     // its sound
	// (the roll may send it from behind, where it can't get a shot: put it ahead of the left-facing chopper)
	j->dir = 1;
	j->x = (int16_t)(g.cam - JET_LEAD);
	j->warn = JET_WARN_FRAMES;                    // (and the from-ahead warning, not the longer one from behind)
	// no missile while it's off screen, nor in its first frames on screen
	int firstShotSeen = -1;
	for(int f = 0; f < 450 && firstShotSeen < 0; ++f) {
		g.chop.x = 2400; g.chop.y = 120; g.chop.vx = g.chop.vy = 0;
		g.chop.y = (int16_t)(j->y + 8);            // in its line of fire
		step(NONE, 1);
		for(int s = 0; s < ESHOT_MAX; ++s)
			if(g.pEShots[s].active && g.pEShots[s].kind == ESHOT_MISSILE) firstShotSeen = j->seen;
		if(!g.chop.alive) break;
	}
	CHECK(firstShotSeen >= 0);                  // it does come, ...
	CHECK(j->warn == 0);                        // ... but only once its 2 s warning is over
	printf("jets: first missile after %d frames on screen\n", firstShotSeen);
}

// GODMODE plays like normal: in every per-difficulty table, entry 0 is entry 3
// (decided by Paul, 2026-11-11; before, the jets' lead and delay differed)
static void testGodmodeTablesEqualNormal(void) {
	uint8_t n = 0;
	const tDiffTable *p = logicDiffTables(&n);
	CHECK(n >= 11);
	for(uint8_t t = 0; t < n; ++t) {
		const uint8_t *pB = (const uint8_t *)p[t].p;
		uint8_t e = p[t].elem;
		int isSame = memcmp(pB + 0 * e, pB + DIFFICULTY_NORMAL * e, e) == 0;
		if(!isSame) printf("table %s: index 0 differs from index 3\n", p[t].name);
		CHECK(isSame);
	}
}

// Tank farming is closed (decided by Paul, 2026-11-11): the first TANK_QUOTA tank
// kills of a mission pay 50 each, later ones nothing; a new mission starts the
// count again
static void killTankByBomb(void) {
	flyAt(1500, 150);
	g.chop.face = g.chop.faceTarget = FACE_FRONT;
	g.pTanks[0] = (tTank){.x = 1500, .active = 1, .hp = 2, .dir = 1, .cool = 255};
	step(NONE, 1);
	step((tInput){.fire = 1}, 1);
	step(NONE, 45);
	CHECK(!g.pTanks[0].active);
}

static void testTankQuotaStopsFarming(void) {
	newGame();
	uint32_t ulStart = g.score;
	for(int i = 0; i < TANK_QUOTA; ++i) killTankByBomb();
	CHECK(g.score == ulStart + 50u * TANK_QUOTA);
	CHECK(g.quotaMsg > 0);                       // the HUD says so
	uint32_t ulAtQuota = g.score;
	for(int i = 0; i < 3; ++i) killTankByBomb();   // more tanks die ...
	CHECK(g.score == ulAtQuota);                   // ... for nothing
	// the cannon too
	flyAt(1500, GROUND_Y);
	g.chop.landed = 1;
	g.chop.face = g.chop.faceTarget = FACE_LEFT;
	g.pTanks[0] = (tTank){.x = 1440, .active = 1, .hp = 1, .dir = 1, .cool = 255};
	step((tInput){.fire = 1}, 20);
	CHECK(!g.pTanks[0].active);
	CHECK(g.score == ulAtQuota);
	// the next mission counts again
	logicStartMission(&g, 1);
	step(NONE, INTRO_FRAMES + 1);
	CHECK(g.tankKills == 0);
	uint32_t ulM2 = g.score;
	killTankByBomb();
	CHECK(g.score == ulM2 + 50);
}

// Slower enemy shots (decided by Paul, 2026-11-11): a shell leaves a tank at about
// 1.5 px/frame and reaches as far as a tank can aim; a jet missile at 2.5. The
// slower shells are paid for in fire timing: the gap between a tank's shells
// is 85-116 frames at mission 1, trip 0, normal (it was 110-141).
static void testTankShellSpeedAndGap(void) {
	newGame();
	flyAt(1500, GROUND_Y);
	g.chop.landed = 1;
	g.pTanks[0] = (tTank){.x = 1320, .active = 1, .hp = 2, .dir = 1, .cool = 1};
	int iLast = -1, iMaxGap = 0, iMinGap = 9999, iShells = 0;
	for(int f = 0; f < 1500 && iShells < 8; ++f) {
		g.chop.x = 1500; g.chop.y = GROUND_Y; g.chop.landed = 1; g.chop.vx = 0;
		g.pTanks[0].x = 1320;
		g.spawnTimer = 60000;
		step(NONE, 1);
		for(int s = 0; s < ESHOT_MAX; ++s) {
			tShot *p = &g.pEShots[s];
			if(p->active && p->kind == ESHOT_SHELL && p->life == SHELL_LIFE) {
				int vx = absi(p->vx), vy = absi(p->vy);
				int m = vx > vy ? vx : vy;
				CHECK(m >= 22 && m <= 26);              // 1.5 px/frame on the larger axis
				CHECK(vx * vx + vy * vy <= 36 * 36);    // never above 2.25 px/frame
				CHECK(p->life * m >= 200 * 16 * 9 / 10);   // it flies to the chopper: 200 px of range
				if(iLast >= 0 && g.frame - iLast > iMaxGap) iMaxGap = g.frame - iLast;
				if(iLast >= 0 && g.frame - iLast < iMinGap) iMinGap = g.frame - iLast;
				if(iLast >= 0 && g.frame > iLast) {}
				iLast = g.frame;
				++iShells;
				p->active = 0;
			}
		}
	}
	CHECK(iShells == 8);
	printf("tank shells: gaps %d..%d frames\n", iMinGap, iMaxGap);
	CHECK(iMaxGap <= 116);
	CHECK(iMinGap >= 85);
}

// Jets: a flash (at least 12 frames, 16 on normal) before every missile; the
// missile flies at 2.5 px/frame; nothing is fired unless the whole jet is on screen
static uint8_t jetOnScreenFully(const tJet *j) {
	return j->x - 16 >= g.cam && j->x + 16 <= g.cam + SCREEN_W;
}

static void testNoMissileWithoutTell(void) {
	for(uint8_t d = 0; d <= DIFFICULTY_MAX; ++d) {
		newGame();
		logicSetDifficulty(&g, d);
		uint8_t ubTell = logicJetTell(&g);
		CHECK(ubTell >= 12);
		if(d == DIFFICULTY_NORMAL) CHECK(ubTell >= 12 && ubTell <= 18);
		int iMissiles = 0;
		for(int pass = 0; pass < 6; ++pass) {
			flyAt(1500, 120);
			g.trip = 10;
			g.pJets[0] = (tJet){.x = (int16_t)(g.cam + 20), .y = 112, .dir = 1, .active = 1, .hp = 1, .seen = 100};
			uint8_t ubRun = 0;
			for(int f = 0; f < 200; ++f) {
				g.chop.x = 1530; g.chop.y = 120; g.chop.vx = g.chop.vy = 0; g.chop.landed = 0;
				g.cam = 1340;
				g.pJets[1].active = 0; g.pJets[1].cool = 255;
				uint8_t ubBefore = ubRun;
				step(NONE, 1);
				if(!g.pJets[0].active) break;
				ubRun = g.pJets[0].tell ? (uint8_t)(ubRun + 1) : 0;
				int isNew = 0;
				for(int s = 0; s < ESHOT_MAX; ++s) {
					tShot *p = &g.pEShots[s];
					if(p->active && p->kind == ESHOT_MISSILE && p->life == JET_MISSILE_LIFE) {
						isNew = 1;
						CHECK(absi(p->vx) == 2 * 20);          // 2.5 px/frame
						p->active = 0;
					}
				}
				if(isNew) {
					++iMissiles;
					CHECK(ubBefore >= ubTell);                 // the whole flash came first
					g.pJets[0].active = 0;
				}
				g.chop.alive = 1;
			}
		}
		CHECK(iMissiles >= 1);
	}
}

// A jet that is only partly on screen (or beyond the edge) never fires (nor flashes more than 3/4 of a tell early): it used
// to, up to ~40 px past the edge, once it had been seen for a few frames
static void testJetNeverFiresOffScreen(void) {
	for(int side = 0; side < 2; ++side) {
		for(int off = -40; off <= 20; off += 4) {
			newGame();
			flyAt(1500, 120);
			g.trip = 10;
			g.cam = 1340;
			// coming from the left, centre `off` px from the left edge (-40: off screen, 15: partly on)
			int16_t x = (int16_t)(side == 0 ? g.cam + off : g.cam + SCREEN_W - off);
			if(off >= 12) continue;           // it flies 4 px before it looks: from here it is whole on screen
			g.pJets[0] = (tJet){.x = x, .y = 112, .dir = (int8_t)(side == 0 ? 1 : -1), .active = 1, .hp = 1, .seen = 100};
			g.pJets[1].cool = 255;
			g.chop.x = 1500;   // 148-200 px ahead of it
			g.chop.y = 120;
			// one frame: it must not fire, nor start a flash
			step(NONE, 1);
			for(int s = 0; s < ESHOT_MAX; ++s) CHECK(!(g.pEShots[s].active && g.pEShots[s].kind == ESHOT_MISSILE));
			// (the flash may begin up to three quarters of its length early, but never further out, and no shot)
				{ int w = logicJetTell(&g) * 3 + 4; /* (+4: the camera moves a little after the jet decided) */ const tJet *p = &g.pJets[0];
				CHECK(p->tell == 0 || (p->x - 16 >= g.cam - w && p->x + 16 <= g.cam + SCREEN_W + w)); }
		}
	}
}

// The flash is drawn: a jet in its tell blinks between its plain and glowing frames
static void testJetTellShowsFlash(void) {
	newGame();
	flyAt(1500, 120);
	g.cam = 1340;
	tDraw d[DRAW_MAX];
	int iFlash = 0, iPlain = 0;
	for(uint8_t tell = 1; tell <= 16; ++tell) {
		g.pJets[0] = (tJet){.x = 1400, .y = 112, .dir = -1, .active = 1, .hp = 1, .tell = tell};
		uint8_t n = logicDraw(&g, d);
		for(uint8_t i = 0; i < n; ++i) {
			if(d[i].bob != BOB_JET) continue;
			if(d[i].frame == JET_FLASH_L) ++iFlash;
			else if(d[i].frame == JET_L) ++iPlain;
		}
	}
	CHECK(iFlash >= 6 && iPlain >= 6);       // it blinks, half and half
	g.pJets[0].tell = 0;
	uint8_t n = logicDraw(&g, d);
	for(uint8_t i = 0; i < n; ++i) if(d[i].bob == BOB_JET) CHECK(d[i].frame == JET_L);
}

// ---- damage instead of instant loss (decided by Paul, 2026-11-11) ----

static void shellOnChopper(void) {
	for(int i = 0; i < ESHOT_MAX; ++i) {
		if(!g.pEShots[i].active) {
			g.pEShots[i] = (tShot){.x = g.chop.x, .y = (int16_t)(g.chop.y - 6), .active = 1, .kind = ESHOT_SHELL, .life = 50};
			return;
		}
	}
}

static void testFirstHitDamages(void) {
	newGame();
	g.spawnTimer = 60000;
	for(int i = 0; i < 7; ++i) g.pHostages[i].state = HS_ABOARD;
	g.aboard = 7;
	flyAt(1500, 150);
	shellOnChopper();
	step(NONE, 1);
	CHECK(g.chop.alive && g.chop.damaged);
	CHECK(g.state == GS_PLAY);
	CHECK(g.aboard == 7 && g.killed == 0 && g.lives == LIVES_START);   // everyone is kept
	CHECK(g.chop.invuln >= INVULN_FRAMES - 1);
	CHECK(g.events2 & EV2_DAMAGE);
	CHECK(!(g.events & EV_CRASH));
}

static void testDamagedFliesSlower(void) {
	newGame();
	flyAt(1000, 120);
	step((tInput){.dx = 1}, 120);
	int iHealthy = g.chop.vx;
	CHECK(iHealthy == VX_MAX);
	newGame();
	flyAt(1000, 120);
	g.chop.damaged = 1;
	step((tInput){.dx = 1}, 120);
	CHECK(g.chop.vx <= DMG_VX_MAX && g.chop.vx >= DMG_VX_MAX - 2);   // 38 of 48: about 20 % slower
	CHECK(g.chop.vx * 100 / iHealthy >= 75 && g.chop.vx * 100 / iHealthy <= 85);
	step((tInput){.dy = -1}, 60);
	CHECK(g.chop.vy >= -DMG_VY_UP_MAX);
	step((tInput){.dy = 1}, 60);
	CHECK(g.chop.vy <= DMG_VY_DOWN_MAX);
	// the sidestep too
	newGame();
	flyAt(1000, 120);
	g.chop.damaged = 1;
	g.chop.face = g.chop.faceTarget = FACE_FRONT;
	step((tInput){.dx = 1}, 12);   // (a longer push turns it to face right)
	CHECK(g.chop.vx == DMG_HO_VX_MAX && g.chop.face == FACE_FRONT);
}

static void testInvulnerableAfterHitAndBlinks(void) {
	newGame();
	g.spawnTimer = 60000;
	flyAt(1500, 150);
	shellOnChopper();
	step(NONE, 1);
	CHECK(g.chop.damaged);
	int iHidden = 0, iShown = 0;
	for(int f = 0; f < INVULN_FRAMES - 2; ++f) {
		// shells and mines pass through it
		shellOnChopper();
		g.pMines[0] = (tMine){.x = g.chop.x, .y = (int16_t)(g.chop.y - 6), .active = 1};
		g.chop.x = 1500; g.chop.y = 150; g.chop.vx = g.chop.vy = 0;
		step(NONE, 1);
		CHECK(g.chop.alive);
		if(logicChopperVisible(&g)) ++iShown; else ++iHidden;
	}
	CHECK(g.chop.alive && g.state == GS_PLAY && g.killed == 0);
	CHECK(iShown >= 40 && iHidden >= 40);       // it blinks, half and half
	// after the blink it's visible, and vulnerable again
	for(int i = 0; i < ESHOT_MAX; ++i) g.pEShots[i].active = 0;
	g.pMines[0].active = 0;
	step(NONE, 5);
	CHECK(g.chop.invuln == 0 && logicChopperVisible(&g));
}

static void testSecondHitDestroys(void) {
	newGame();
	g.spawnTimer = 60000;
	for(int i = 0; i < 7; ++i) g.pHostages[i].state = HS_ABOARD;
	g.aboard = 7;
	flyAt(1500, 150);
	shellOnChopper();
	step(NONE, 1);
	CHECK(g.chop.damaged && g.chop.alive);
	step(NONE, INVULN_FRAMES + 2);
	CHECK(g.chop.alive && g.chop.damaged);
	g.chop.x = 1500; g.chop.y = 150; g.chop.vx = g.chop.vy = 0;
	shellOnChopper();
	step(NONE, 1);
	CHECK(!g.chop.alive);                       // the old loss rules
	CHECK(g.killed == 7 && g.aboard == 0);
	CHECK(g.lives == LIVES_START - 1);
	CHECK(g.state == GS_LOST);
	step(NONE, LOST_FRAMES + 1);
	CHECK(g.state == GS_PLAY && g.chop.alive && !g.chop.damaged);   // a new chopper is whole
}

static void testPadRepairs(void) {
	newGame();
	g.spawnTimer = 60000;
	flyAt(1500, 150);
	g.chop.damaged = 1;
	// landing away from the pad does not mend it
	g.chop.y = GROUND_Y - 4;
	step((tInput){.dy = 1}, 10);
	CHECK(g.chop.landed && g.chop.damaged);
	// landing on the pad does
	flyAt(2860, 150);
	g.chop.damaged = 1;
	step((tInput){.dy = 1}, 60);
	CHECK(g.chop.landed && g.chop.x >= BASE_PAD_X0 && g.chop.x <= BASE_PAD_X1);
	CHECK(!g.chop.damaged);
}

static void testSmokeTrailOnlyWhenDamaged(void) {
	newGame();
	flyAt(1500, 120);
	tDraw d[DRAW_MAX];
	int iPuffs = 0;
	for(int f = 0; f < 40; ++f) {
		g.chop.x = 1500; g.chop.y = 120; g.chop.vx = 0;
		step(NONE, 1);
		uint8_t n = logicDraw(&g, d);
		for(uint8_t i = 0; i < n; ++i) iPuffs += d[i].bob == BOB_SMOKE;
	}
	CHECK(iPuffs == 0);                         // none while whole
	g.chop.damaged = 1;
	int iMax = 0;
	for(int f = 0; f < 60; ++f) {
		g.chop.x = 1500; g.chop.y = 120; g.chop.vx = 0;
		step(NONE, 1);
		uint8_t n = logicDraw(&g, d);
		int c = 0;
		for(uint8_t i = 0; i < n; ++i) c += d[i].bob == BOB_SMOKE;
		if(c > iMax) iMax = c;
	}
	CHECK(iMax >= 4 && iMax <= SMOKE_MAX);      // a trail of puffs
	g.chop.damaged = 0;                         // mended: the trail thins out and ends
	step(NONE, SMOKE_LIFE + 2);
	uint8_t n = logicDraw(&g, d);
	for(uint8_t i = 0; i < n; ++i) CHECK(d[i].bob != BOB_SMOKE);
}

// Hovering damaged, the puffs rise from one spot and pile up on each other: one
// every 7 frames there (as on the ground), not 4 - the column looks the same and
// the blitter draws half as many (the skill-5 loss among explosions overran the
// A500's frame). Flying, the trail is as before.
static void testSmokeSparserWhenHovering(void) {
	newGame();
	flyAt(1500, 120);
	g.chop.damaged = 1;
	tDraw d[DRAW_MAX];
	int iMax = 0;
	for(int f = 0; f < 60; ++f) {
		g.chop.x = 1500; g.chop.y = 120; g.chop.vx = 0;
		step(NONE, 1);
		uint8_t n = logicDraw(&g, d);
		int c = 0;
		for(uint8_t i = 0; i < n; ++i) c += d[i].bob == BOB_SMOKE;
		if(c > iMax) iMax = c;
	}
	CHECK(iMax >= 3 && iMax <= 4);              // hovering: a sparser column
	iMax = 0;
	for(int f = 0; f < 60; ++f) {
		g.chop.x = (int16_t)(1000 + 2 * f); g.chop.y = 120; g.chop.vx = 32;   // 2 px a frame
		step(NONE, 1);
		uint8_t n = logicDraw(&g, d);
		int c = 0;
		for(uint8_t i = 0; i < n; ++i) c += d[i].bob == BOB_SMOKE;
		if(c > iMax) iMax = c;
	}
	CHECK(iMax >= 6);                           // flying: the full trail
}

// The second hit: the chopper goes up in the explosion, and its smoke goes with
// it (the puffs were drawn on through the crash's heaviest pictures, inside the
// boom: the A500's frame overran there)
static void testSmokeGoesWithTheCrash(void) {
	newGame();
	logicSetDifficulty(&g, DIFFICULTY_MAX);
	g.spawnTimer = 60000;
	flyAt(1500, 150);
	g.chop.damaged = 1;
	tDraw d[DRAW_MAX];
	step(NONE, 20);
	uint8_t n = logicDraw(&g, d);
	int iPuffs = 0;
	for(uint8_t i = 0; i < n; ++i) iPuffs += d[i].bob == BOB_SMOKE;
	CHECK(iPuffs >= 2);                         // a damaged chopper trails smoke
	shellOnChopper();
	step(NONE, 1);
	CHECK(!g.chop.alive);
	n = logicDraw(&g, d);
	for(uint8_t i = 0; i < n; ++i) CHECK(d[i].bob != BOB_SMOKE);
}

static void testInvincibleIsNeverDamaged(void) {
	newGame();
	logicSetDifficulty(&g, DIFFICULTY_INVINCIBLE);
	g.spawnTimer = 60000;
	flyAt(1500, 150);
	shellOnChopper();
	step(NONE, 1);
	CHECK(g.chop.alive && !g.chop.damaged && !g.chop.invuln);
	CHECK(g.events & EV_SHRUG);
}

// ---- the rotor: a deep blade thump whose rate follows speed and climb ----
static int thumpsX10(void) { return logicRotorThumpsX10(logicRotorPeriod(&g)); }

static void testRotorRateFollowsSpeedAndClimb(void) {
	newGame();
	flyAt(1500, 120);
	step(NONE, 150);
	int iHover = thumpsX10();
	int iHoverVol = logicRotorVolume(&g);
	CHECK(iHover >= 45 && iHover <= 55);                  // 4.5-5.5 thumps a second at hover
	step((tInput){.dx = 1}, 120);
	int iFast = thumpsX10();
	CHECK(iFast >= 60 && iFast <= 65);                    // 6-6.5 at full speed (Paul, 2026-11-12: 8-9 was irritating)
	CHECK(logicRotorVolume(&g) >= iHoverVol);             // a touch louder at most: the hover level, not 56 (+16)
	CHECK(logicRotorVolume(&g) <= iHoverVol + 5);
	// cruise (2 px/frame): 5.5-6 a second
	newGame();
	flyAt(1500, 120);
	for(int i = 0; i < 200; ++i) { g.chop.vx = 32; step(NONE, 1); }
	CHECK(thumpsX10() >= 55 && thumpsX10() <= 60);
	// the hardest case, flat out and climbing: still no more than 6.5
	newGame();
	flyAt(1500, 150);
	for(int i = 0; i < 120; ++i) step((tInput){.dx = 1, .dy = -1}, 1);
	CHECK(thumpsX10() <= 65);
	newGame();
	flyAt(1500, 150);
	step(NONE, 150);
	step((tInput){.dy = -1}, 20);
	CHECK(thumpsX10() > iHover + 8);                      // climbing quickens it
	newGame();
	flyAt(1500, 60);
	step(NONE, 150);
	step((tInput){.dy = 1}, 20);
	CHECK(thumpsX10() <= iHover);                         // descending doesn't
}

static void testRotorIdlesSlowAndSoftOnTheGround(void) {
	newGame();
	step(NONE, 100);
	CHECK(g.chop.landed);
	CHECK(thumpsX10() >= 30 && thumpsX10() <= 40);        // a slow idle, 3-4 a second
	int iIdleVol = logicRotorVolume(&g);
	newGame();
	flyAt(1500, 120);
	step(NONE, 150);
	CHECK(iIdleVol < logicRotorVolume(&g));
}

static void testRotorSpinsUpAtTakeoff(void) {
	newGame();
	flyAt(1500, 120);
	step(NONE, 300);
	int iHover = thumpsX10(), iHoverVol = logicRotorVolume(&g);
	newGame();
	step(NONE, 100);
	int iPrev = thumpsX10();
	step((tInput){.dy = -1}, 1);
	CHECK(!g.chop.landed);
	int iPeak = iPrev, iPeakVol = 0;
	for(int i = 0; i < 90; ++i) {
		step(NONE, 1);
		int n = thumpsX10();
		CHECK(n >= iPrev - 1);                            // spinning up, never a dip, until it peaks
		if(n < iPrev) break;
		iPrev = n;
		if(logicRotorVolume(&g) > iPeakVol) iPeakVol = logicRotorVolume(&g);
		if(n > iPeak) iPeak = n;
	}
	CHECK(iPeak >= 70);                                   // clearly faster than the hover's 5
	CHECK(iPeak > iHover + 20);
	CHECK(iPeakVol >= iHoverVol + 6);                     // louder at take-off
	// it settles to the hover on its own
	g.chop.vx = g.chop.vy = 0;
	g.chop.y = 120;
	step(NONE, 400);
	CHECK(thumpsX10() >= iHover - 2 && thumpsX10() <= iHover + 2);
	CHECK(logicRotorVolume(&g) <= iHoverVol + 1);
}

static void testRotorSilentWhenDead(void) {
	newGame();
	g.chop.alive = 0;
	step(NONE, 5);
	CHECK(logicRotorVolume(&g) == 0);
}

// ---- rotor v3 (Paul's playtest, 2026-11-12: damaged and loaded were irritating) ----
// A hurt or loaded chopper sounds lower and heavier, never faster: its rotor period is the
// undamaged-empty one at the same speed plus something, and the take-off spin-up is
// tamed. The volume only ever steps a little. No effect repeats like a machine gun.

// Cruise at vx (forced) for 200 frames in the given state: the settled period and volume
static void rotorSettle(int16_t vx, uint8_t ubAboard, uint8_t isDamaged, int *pPer, int *pVol) {
	newGame();
	flyAt(1500, 120);
	g.chop.damaged = isDamaged;
	g.aboard = ubAboard;
	for(int i = 0; i < 200; ++i) { g.chop.vx = vx; step(NONE, 1); }
	*pPer = logicRotorPeriod(&g);
	*pVol = logicRotorVolume(&g);
}

static void testRotorApprovedEmptyStaysExactly(void) {
	int per, vol;
	rotorSettle(0, 0, 0, &per, &vol);
	CHECK(per == 591 && vol == 40);      // the approved v2 hover
	rotorSettle(32, 0, 0, &per, &vol);
	CHECK(per == 501 && vol == 42);      // the approved v2 cruise (2 px/frame)
}

static void testRotorLoadedAndDamagedNeverFasterThanEmpty(void) {
	static const int16_t pSpeed[] = {0, 16, 32, 38, 48};
	static const struct { uint8_t aboard, damaged; } pState[] = {{8, 0}, {16, 0}, {0, 1}, {8, 1}, {16, 1}};
	for(int s = 0; s < 5; ++s) {
		for(int k = 0; k < 5; ++k) {
			int perE, volE, per, vol;
			rotorSettle(pSpeed[s], 0, 0, &perE, &volE);
			rotorSettle(pSpeed[s], pState[k].aboard, pState[k].damaged, &per, &vol);
			CHECK(per > perE);                       // lower (a heavier thump), never the same or higher
			CHECK(per <= perE + 90);                 // ... but not a different engine
			CHECK(vol >= volE && vol <= volE + 5);   // a touch heavier, not louder
		}
	}
	// more aboard is never faster than fewer
	int p8, p16, v;
	rotorSettle(32, 8, 0, &p8, &v);
	rotorSettle(32, 16, 0, &p16, &v);
	CHECK(p16 > p8);
}

// The take-off spin-up (11.8 thumps a second at its peak) is for the empty chopper: a loaded
// or hurt one lifts off with a calm 6.5 at the most
static void testRotorHeavyLiftoffIsCalm(void) {
	static const struct { uint8_t aboard, damaged; } pState[] = {{8, 0}, {16, 0}, {0, 1}, {16, 1}};
	for(int k = 0; k < 4; ++k) {
		newGame();
		g.chop.x = 1500;                             // (away from the home pad: it would unload and mend)
		g.cam = (int16_t)(1500 - SCREEN_W / 2);
		g.spawnTimer = 60000;
		g.aboard = pState[k].aboard;
		g.chop.damaged = pState[k].damaged;
		step(NONE, 100);
		CHECK(g.chop.landed && g.aboard == pState[k].aboard && g.chop.damaged == pState[k].damaged);
		step((tInput){.dy = -1}, 1);
		int iPeak = 0, iPrevVol = logicRotorVolume(&g);
		for(int i = 0; i < 200; ++i) {
			step((tInput){.dx = 1, .dy = i < 20 ? -1 : 0}, 1);
			if(thumpsX10() > iPeak) iPeak = thumpsX10();
			int vol = logicRotorVolume(&g);
			CHECK(absi(vol - iPrevVol) <= 2);        // the volume steps are bounded
			CHECK(vol <= 62);
			iPrevVol = vol;
		}
		CHECK(iPeak <= 65);
	}
}

// Damaged: a slow, laboured wobble in the pitch (never a faster beat, never a jump)
static void testRotorDamagedWobbleIsGentle(void) {
	newGame();
	flyAt(1500, 120);
	g.chop.damaged = 1;
	int iMin = 9999, iMax = 0, iPrev = 0;
	for(int i = 0; i < 600; ++i) {
		g.chop.vx = 32;
		step(NONE, 1);
		int per = logicRotorPeriod(&g);
		if(i > 150) {
			if(per < iMin) iMin = per;
			if(per > iMax) iMax = per;
			CHECK(absi(per - iPrev) <= 3);           // no jump from one frame to the next
		}
		iPrev = per;
	}
	CHECK(iMax - iMin >= 6);                         // you can hear it is hurt ...
	CHECK(iMax - iMin <= 24);                        // ... without a warble
	CHECK(iMin >= 501 + 32);                         // always lower than the empty cruise
}

// ---- rotor v4 (Paul's playtest, 2026-11-14: "tinny or harsh in flight, like an alarm bell") ----
// What WinUAE and vAmiga showed: the rotor is the approved loop and it is never stolen or
// restarted, but at 6 kHz (period 591) Paula's sample-and-hold puts bright, gliding lines at the
// sample rate (4.2 kHz idling, 6 kHz hovering, 7.8 flat out; they sweep during the spin-up and
// wobble when damaged) over the thump, and the loop's slap sat at 250 Hz-1.2 kHz where ears and small
// speakers are keenest. v4: the loop is made at twice the rate (ROTOR_OS) so Paula's period is half
// the logical one (every approved period and thump rate stays as it was) and the lines move to 12 kHz,
// the slap is softer and lower.

static double cosT(double w) {            // (no libm on the host build) w in 0..pi
	double t = 1, s = 1, w2 = w * w;
	for(int i = 1; i < 14; ++i) { t *= -w2 / ((2 * i - 1) * (2 * i)); s += t; }
	return s;
}

static double aWeightPower(double f) {    // IEC 61672 A-weighting as a power ratio (linear, 1 kHz = 1)
	double f2 = f * f, a = 12194.0 * 12194.0;
	double r = (a * a * f2 * f2 * f2 * f2) / ((f2 + 20.6 * 20.6) * (f2 + 20.6 * 20.6) * (f2 + 107.7 * 107.7) * (f2 + 737.9 * 737.9) * (f2 + a) * (f2 + a));
	double f1 = 1000.0 * 1000.0;
	double r1 = (a * a * f1 * f1 * f1 * f1) / ((f1 + 20.6 * 20.6) * (f1 + 20.6 * 20.6) * (f1 + 107.7 * 107.7) * (f1 + 737.9 * 737.9) * (f1 + a) * (f1 + a));
	return r / r1;
}

// shares of the loop's power (percent) as played at Paula period uwHwPeriod: plain below 120 Hz and
// above 300 Hz, and A-weighted (what the ear counts) for 250 Hz-1 kHz and above 1 kHz
static void rotorLoopShares(const int8_t *pLoop, uint16_t uwHwPeriod, double *pLow, double *pOver300, double *pAmid, double *pAhigh) {
	double dRate = (double)ROTOR_CLOCK / uwHwPeriod, dMean = 0;
	for(int i = 0; i < ROTOR_SAMPLES; ++i) dMean += pLoop[i];
	dMean /= ROTOR_SAMPLES;
	double dAll = 0, dLow = 0, dOver = 0, dAtot = 0, dAmid = 0, dAhigh = 0;
	for(int k = 1; k < ROTOR_SAMPLES / 2; ++k) {        // Goertzel for bin k (the loop repeats, so bins are exact)
		double w = 2 * 3.14159265358979 * k / ROTOR_SAMPLES;
		double c = (w <= 3.14159265358979) ? cosT(w) : -cosT(6.28318530717959 - w), q0, q1 = 0, q2 = 0;
		for(int i = 0; i < ROTOR_SAMPLES; ++i) { q0 = 2 * c * q1 - q2 + (pLoop[i] - dMean); q2 = q1; q1 = q0; }
		double p = q1 * q1 + q2 * q2 - 2 * c * q1 * q2;
		double f = dRate * k / ROTOR_SAMPLES, a = p * aWeightPower(f);
		if(f >= 20) { dAll += p; dAtot += a; }
		if(f >= 40 && f < 120) dLow += p;
		if(f >= 300) dOver += p;
		if(f >= 250 && f < 1000) dAmid += a;
		if(f >= 1000) dAhigh += a;
	}
	*pLow = 100 * dLow / dAll;
	*pOver300 = 100 * dOver / dAll;
	*pAmid = 100 * dAmid / dAtot;
	*pAhigh = 100 * dAhigh / dAtot;
}

static void testRotorHardwarePeriodIsOversampled(void) {
	CHECK(ROTOR_OS >= 2);
	uint16_t uwPrev = 0;
	for(uint16_t per = 290; per <= 900; ++per) {
		uint16_t hw = logicRotorHwPeriod(per);
		CHECK(hw >= 124);                                          // Paula's shortest period
		CHECK(hw >= uwPrev);                                       // monotonic: a lower pitch never jumps up
		uwPrev = hw;
		// the thump rate (loop pass * period) is what the logic decided, to 1 %
		long lLogic = ROTOR_CLOCK * 100L / ((long)per * ROTOR_PASS);
		long lHw = ROTOR_CLOCK * 100L / ((long)hw * ROTOR_LOOP_PASS);
		CHECK(lHw >= lLogic * 99 / 100 && lHw <= lLogic * 101 / 100);
	}
	// the sample rate the images sit at: >= 11.5 kHz hovering, >= 8 kHz idling (was 6.0 and 4.2)
	CHECK(ROTOR_CLOCK / logicRotorHwPeriod(591) >= 11500);
	CHECK(ROTOR_CLOCK / logicRotorHwPeriod(845) >= 8000);
	// what the game writes to Paula follows the logic: hover 5 a second, idle 3.5
	newGame();
	flyAt(1500, 120);
	step(NONE, 150);
	CHECK(logicRotorPeriod(&g) == 591);
	int iHw = logicRotorThumpsX10(logicRotorPeriod(&g));
	CHECK(iHw >= 49 && iHw <= 51);
}

static void testRotorLoopIsDeepAndSoft(void) {
	static int8_t s_pLoop[ROTOR_SAMPLES];
	logicRotorSynth(s_pLoop);
	int iPeak = 0;
	for(int i = 0; i < ROTOR_SAMPLES; ++i) iPeak = s_pLoop[i] < 0 ? (-s_pLoop[i] > iPeak ? -s_pLoop[i] : iPeak) : (s_pLoop[i] > iPeak ? s_pLoop[i] : iPeak);
	CHECK(iPeak >= 110 && iPeak <= 127);                           // uses the 8 bits, never wraps
	CHECK(absi(s_pLoop[0] - s_pLoop[ROTOR_SAMPLES - 1]) <= 16);    // the loop closes without a click
	double dLow, dOver, dMid, dHigh;
	rotorLoopShares(s_pLoop, logicRotorHwPeriod(591), &dLow, &dOver, &dMid, &dHigh);
	CHECK(dLow >= 80);       // the body: 40-120 Hz (v3 loop: 84 %)
	CHECK(dOver <= 0.3);     // under 0.3 % of the power above 300 Hz (v3 loop: 0.51 %)
	CHECK(dMid <= 14);       // what the ear counts at 250 Hz-1 kHz (v3 loop: 27 %)
	CHECK(dHigh <= 3);       // ... and above 1 kHz (v3 loop: 11 %)
	// the same holds at the idle's and the flat-out rate (the same samples, another pitch)
	rotorLoopShares(s_pLoop, logicRotorHwPeriod(845), &dLow, &dOver, &dMid, &dHigh);
	CHECK(dMid <= 22 && dHigh <= 5);     // idling: everything a third lower in pitch (v3 loop: 35 % and 12 %)
	rotorLoopShares(s_pLoop, logicRotorHwPeriod(455), &dLow, &dOver, &dMid, &dHigh);
	CHECK(dMid <= 22 && dHigh <= 3);     // flat out (v3 loop: 33 % and 11 %)
}

// States (Paul: "slight pitch and tempo adjustments for different states"): descending settles the
// rotor a little lower and slower than the hover (never above it), the empty take-off spin-up peaks at
// 8 a second (it was 11.8, a buzz), and everything else stays as approved
static void testRotorDescendSettlesLower(void) {
	newGame();
	flyAt(1500, 60);
	step(NONE, 150);
	int iHoverPer = logicRotorPeriod(&g), iHoverVol = logicRotorVolume(&g);
	CHECK(iHoverPer == 591 && iHoverVol == 40);
	int iMaxPer = 0;
	for(int i = 0; i < 30; ++i) {
		step((tInput){.dy = 1}, 1);
		if(logicRotorPeriod(&g) > iMaxPer) iMaxPer = logicRotorPeriod(&g);
		CHECK(logicRotorVolume(&g) <= iHoverVol);
	}
	CHECK(iMaxPer >= iHoverPer + 8);                               // you can hear it settle ...
	CHECK(iMaxPer <= iHoverPer + 30);                              // ... a few percent, not another engine
	// and climbing is still quicker, never lower
	newGame();
	flyAt(1500, 150);
	step(NONE, 150);
	step((tInput){.dy = -1}, 20);
	CHECK(logicRotorPeriod(&g) < iHoverPer - 20);
}

static void testRotorEmptyTakeoffIsNoBuzz(void) {
	newGame();
	step(NONE, 100);
	int iPeak = 0;
	for(int i = 0; i < 200; ++i) {
		step((tInput){.dy = i < 25 ? -1 : 0}, 1);             // a second of up, as a player lifts off (the climb quickens it too)
		if(thumpsX10() > iPeak) iPeak = thumpsX10();
	}
	CHECK(iPeak >= 70);                                            // still a lively spin-up ...
	CHECK(iPeak <= 80);                                            // ... at 8 a second at the most (v3 with a climb: 9.8)
}

// Effects: none plays twice inside its gap, while damaged or not (the gate main.c uses)
static void testSfxGapsWhileDamaged(void) {
	for(int isDmg = 0; isDmg < 2; ++isDmg) {
		for(int k = 0; k < SFXK_COUNT; ++k) CHECK(logicSfxGap((uint8_t)k) >= 6);
		// every event every frame: what gets through is spaced by the gap
		uint16_t pLast[SFXK_COUNT] = {0};
		uint16_t pPlayed[SFXK_COUNT] = {0};
		for(uint16_t f = 1; f < 600; ++f) {
			for(int k = 0; k < SFXK_COUNT; ++k) {
				if(!logicSfxAllow(pLast, f, (uint8_t)k)) continue;
				if(pPlayed[k]) CHECK(f - pPlayed[k] >= logicSfxGap((uint8_t)k));
				pPlayed[k] = f;
			}
		}
		for(int k = 0; k < SFXK_COUNT; ++k) CHECK(pPlayed[k] != 0);
	}
	// the real thing: a damaged chopper flown down the field with tanks and jets about
	newGame();
	g.chop.damaged = 1;
	g.aboard = 16;
	uint16_t pLast[SFXK_COUNT] = {0}, pPlayed[SFXK_COUNT] = {0};
	for(int f = 0; f < 4000; ++f) {
		tInput in = {.dx = (f / 700) & 1 ? -1 : 1, .dy = (f & 255) < 40 ? -1 : 0};
		if(f < 40) in.dy = -1;
		step(in, 1);
		g.chop.invuln = 100;   // (stay damaged, not dead)
		uint8_t pHit[SFXK_COUNT] = {0};
		pHit[SFXK_BOOM] = !!(g.events & EV_BOOM);
		pHit[SFXK_JETWARN] = !!(g.events2 & EV2_JETWARN);
		pHit[SFXK_JET] = !!(g.events & EV_JET);
		pHit[SFXK_TANKFIRE] = !!(g.events & EV_TANKFIRE);
		pHit[SFXK_SAVED] = !!(g.events & EV_SAVED);
		pHit[SFXK_BOARD] = !!(g.events & (EV_BOARD | EV_UNLOAD));
		pHit[SFXK_LAND] = !!(g.events & EV_LAND);
		for(int k = 0; k < SFXK_COUNT; ++k) {
			if(!pHit[k] || !logicSfxAllow(pLast, (uint16_t)(f + 1), (uint8_t)k)) continue;
			if(pPlayed[k]) CHECK((f + 1) - pPlayed[k] >= logicSfxGap((uint8_t)k));
			pPlayed[k] = (uint16_t)(f + 1);
		}
	}
}

// ---- jet fairness (Paul's playtest, 2026-11-11; studio tuning, pending Paul's playtest) ----
// No jet bombs a chopper that has just landed (never on skill 1-2), none starts an
// attack run in the 2 s after a take-off, one jet at a time on skill 1-3, a pair's
// second attack 3 s after the first, a warning 2 s before every attack run.

// A jet placed to start an attack run on the chopper this frame, if the rules let it:
// a missile from some way ahead (airborne), or a bomb straight below (landed)
static void aimJet(int i, int dir) {
	tChopper *c = &g.chop;
	int16_t wRun = (int16_t)(logicJetTell(&g) * 4);
	int16_t wAhead = c->landed ? (int16_t)(wRun + 8) : (int16_t)(JET_AHEAD_MIN + wRun + 30);
	g.cam = (int16_t)(c->x - (dir > 0 ? 250 : 70));
	g.pJets[i] = (tJet){.x = (int16_t)(c->x - dir * wAhead), .y = (int16_t)(c->y - 8), .dir = (int8_t)dir,
		.active = 1, .hp = 1, .seen = 100};
}

static int bombsUp(void) {
	int n = 0;
	for(int s = 0; s < ESHOT_MAX; ++s) n += g.pEShots[s].active && g.pEShots[s].kind == ESHOT_BOMB;
	return n;
}

// Landed, with a jet always lined up to bomb it: no attack run on skill 1-2, ever;
// on 3-5 none before the chopper has sat there the dwell time, and one soon after
static void testNoBombOnLandedChopper(void) {
	static const uint16_t pDwell[DIFFICULTY_MAX + 1] = {300, GROUND_NEVER, GROUND_NEVER, 300, 200, 150};
	for(uint8_t d = 0; d <= DIFFICULTY_MAX; ++d) {
		newGame();
		logicSetDifficulty(&g, d);
		g.ghost = 1;
		g.trip = 10;
		flyAt(1500, GROUND_Y);
		g.chop.landed = 1;
		CHECK(logicGroundDwell(&g) == pDwell[d]);
		int iFirst = -1, iBombs = 0;
		for(int f = 0; f < 1000; ++f) {
			g.chop.x = 1500; g.chop.y = GROUND_Y; g.chop.landed = 1; g.chop.vx = g.chop.vy = 0;
			g.spawnTimer = 60000;
			g.pJets[1].active = 0; g.pJets[1].cool = 255;
			if(!g.pJets[0].tell) aimJet(0, 1);
			uint16_t uwBefore = g.jetAttacks;
			step(NONE, 1);
			iBombs += bombsUp() > 0;
			if(g.jetAttacks != uwBefore && iFirst < 0) {
				iFirst = f;
				if(pDwell[d] != GROUND_NEVER) CHECK(g.landFrames >= pDwell[d]);   // never before the dwell time
			}
		}
		if(pDwell[d] == GROUND_NEVER) CHECK(iFirst < 0 && iBombs == 0);
		else CHECK(iFirst >= 0 && iFirst < pDwell[d] + 40);   // and soon after it
	}
}

// After a take-off nothing starts an attack run for JET_TAKEOFF_GRACE frames; a
// chopper that has been airborne a while is attacked at once
static void testTakeoffGrace(void) {
	for(uint8_t d = 1; d <= DIFFICULTY_MAX; ++d) {
		int iAt[2] = {-1, -1};
		for(int takeoff = 0; takeoff < 2; ++takeoff) {
			newGame();
			logicSetDifficulty(&g, d);
			g.ghost = 1;
			g.trip = 10;
			flyAt(1500, 120);
			if(takeoff) {                                  // sat landed, then up
				g.chop.landed = 1;
				g.chop.y = GROUND_Y;
				for(int f = 0; f < 400; ++f) {
					g.chop.x = 1500; g.chop.y = GROUND_Y; g.chop.vx = g.chop.vy = 0;
					g.spawnTimer = 60000;
					step(NONE, 1);
				}
				g.chop.landed = 0;
			}
			for(int f = 0; f < 300 && iAt[takeoff] < 0; ++f) {
				g.chop.x = 1500; g.chop.y = 120; g.chop.landed = 0; g.chop.vx = g.chop.vy = 0;
				g.spawnTimer = 60000;
				g.pJets[1].active = 0; g.pJets[1].cool = 255;
				if(!g.pJets[0].tell) aimJet(0, 1);
				uint16_t uwBefore = g.jetAttacks;
				step(NONE, 1);
				if(g.jetAttacks != uwBefore) iAt[takeoff] = f;
			}
		}
		CHECK(iAt[0] >= 0 && iAt[0] <= 2);
		CHECK(iAt[1] >= JET_TAKEOFF_GRACE - 2 && iAt[1] <= JET_TAKEOFF_GRACE + 4);
	}
}

// Skill 1-3 (until mission 3 on skill 3): one jet out at a time, over a long game;
// two jets lined up start one attack run, the other waits until the first has gone
static void testOneJetAtATime(void) {
	for(uint8_t d = 1; d <= DIFFICULTY_NORMAL; ++d) {
		newGame();
		logicSetDifficulty(&g, d);
		CHECK(logicJetsMax(&g) == 1);
		g.ghost = 1;
		g.trip = 10;
		flyAt(1500, 120);
		int iSpawned = 0, iMax = 0, iWas = 0;
		for(int f = 0; f < 9000; ++f) {
			g.chop.x = 1500; g.chop.y = 120; g.chop.landed = 0; g.chop.vx = g.chop.vy = 0;
			g.spawnTimer = 60000;
			g.jetsTrip = 0;                      // (no trip budget here: testJetTripBudget)
			step(NONE, 1);
			int n = g.pJets[0].active + g.pJets[1].active;
			if(n > iMax) iMax = n;
			if(n && !iWas) ++iSpawned;
			iWas = n;
		}
		CHECK(iSpawned >= 3);
		CHECK(iMax == 1);
		// two lined up
		newGame();
		logicSetDifficulty(&g, d);
		g.ghost = 1;
		g.trip = 10;
		flyAt(1500, 120);
		aimJet(0, 1);
		aimJet(1, 1);
		g.pJets[1].x = (int16_t)(g.pJets[1].x - 8);
		for(int f = 0; f < 60; ++f) {
			g.chop.x = 1500; g.chop.y = 120; g.chop.vx = g.chop.vy = 0;
			g.cam = 1250;
			g.spawnTimer = 60000;
			step(NONE, 1);
			CHECK(g.jetAttacks <= 1);
		}
		printf("one at a time: skill %d, %d attack run(s)\n", d, g.jetAttacks);
		CHECK(g.jetAttacks == 1);
	}
}

// Skill 4-5 (and 3 in mission 3): pairs, but the second jet's attack run starts at
// least JET_PAIR_GAP frames (3 s) after the first's (one jet may follow a missile
// with another on the same pass: that is the same run)
static void testPairOffset(void) {
	for(uint8_t d = 3; d <= DIFFICULTY_MAX; ++d) {
		newGame();
		logicSetDifficulty(&g, d);
		g.mission = d == 3 ? 2 : 0;
		g.ghost = 1;
		g.trip = 10;
		flyAt(1500, 120);
		CHECK(logicJetsMax(&g) == 2);
		int iAt[JET_MAX] = {-1, -1}, iBoth = 0;
		for(int f = 0; f < 400; ++f) {
			g.chop.x = 1500; g.chop.y = 120; g.chop.landed = 0; g.chop.vx = g.chop.vy = 0;
			g.spawnTimer = 60000;
			for(int i = 0; i < JET_MAX; ++i) if(iAt[i] < 0 && !g.pJets[i].tell) aimJet(i, 1);
			uint8_t pTell[JET_MAX] = {g.pJets[0].tell, g.pJets[1].tell};
			step(NONE, 1);
			iBoth |= g.pJets[0].active && g.pJets[1].active;
			for(int i = 0; i < JET_MAX; ++i) if(iAt[i] < 0 && g.pJets[i].tell && !pTell[i]) iAt[i] = f;
		}
		CHECK(iBoth);
		CHECK(iAt[0] >= 0 && iAt[1] >= 0);
		int iGap = iAt[1] > iAt[0] ? iAt[1] - iAt[0] : iAt[0] - iAt[1];
		CHECK(iGap >= JET_PAIR_GAP && iGap <= JET_PAIR_GAP + 6);   // the second one as soon as it may
	}
}

// Every attack run comes at least 2 s after the jet's warning (its appearance)
static void testWarningBeforeAttack(void) {
	int iAttacks = 0;
	for(uint8_t d = 1; d <= DIFFICULTY_MAX; ++d) {
		newGame();
		logicSetDifficulty(&g, d);
		g.ghost = 1;
		g.trip = 10;
		g.pJets[0].cool = 1;
		g.pJets[1].cool = 1;
		flyAt(1500, 120);
		int iBorn[JET_MAX] = {-1, -1}, iWarns = 0, iSpawns = 0;
		for(int f = 0; f < 12000; ++f) {
			g.chop.x = 1500; g.chop.y = 120; g.chop.landed = 0; g.chop.vx = g.chop.vy = 0;
			g.spawnTimer = 60000;
			g.jetsTrip = 0;                      // (no trip budget here: testJetTripBudget)
			uint8_t pWas[JET_MAX] = {g.pJets[0].active, g.pJets[1].active};
			uint8_t pTell[JET_MAX] = {g.pJets[0].tell, g.pJets[1].tell};
			step(NONE, 1);
			int isBorn = 0;
			for(int i = 0; i < JET_MAX; ++i) {
				if(g.pJets[i].active && !pWas[i]) { iBorn[i] = f; isBorn = 1; }
				if(g.pJets[i].active && g.pJets[i].tell && !pTell[i]) {
					++iAttacks;
					CHECK(iBorn[i] >= 0 && f - iBorn[i] >= JET_WARN_FRAMES);
				}
			}
			iSpawns += isBorn;
			iWarns += (g.events2 & EV2_JETWARN) != 0;
		}
		CHECK(iSpawns >= 3 && iWarns == iSpawns);          // each one warned
	}
	CHECK(iAttacks >= 5);
}

// A jet that comes up BEHIND the chopper (flying the way the chopper faces) gets time to be
// turned on: no attack run and no ram within JET_WARN_BEHIND frames (3 s) of its appearing,
// whether the chopper hovers, stops dead, turns round to face it or cruises on (Paul's
// playtest, 2026-11-12: "the jets still come in from behind too quickly")
static void testJetFromBehindHasTime(void) {
	int iTested = 0;
	for(uint8_t d = 1; d <= DIFFICULTY_MAX; d += 2) {
		for(int beh = 0; beh < 4; ++beh) {               // hover, cruise, stop at 1.2 s, turn round at 1.2 s
			for(int seed = 1; seed <= 60; ++seed) {
				newGame();
				logicSetDifficulty(&g, d);
				g.ghost = 1;
				g.trip = 10;
				g.lfsr = (uint16_t)(seed * 40503u + 7);
				flyAt(1200, 120);
				g.chop.face = g.chop.faceTarget = FACE_RIGHT;
				g.chop.vx = beh == 0 ? 0 : 48;
				g.cam = 1200 - 160;
				g.pJets[0].cool = 1; g.pJets[1].active = 0; g.pJets[1].cool = 255;
				int iBorn = -1;
				uint16_t uwAtk0 = g.jetAttacks;
				for(int f = 0; f < 700; ++f) {
					g.spawnTimer = 60000;
					tInput in = {0};
					if(beh == 1 || (beh >= 2 && f < 60)) in.dx = 1;
					if(beh == 3 && f >= 60) in.dx = -1;
					uint8_t isWas = g.pJets[0].active;
					step(in, 1);
					if(beh == 0) g.chop.vx = 0;
					if(iBorn < 0 && g.pJets[0].active && !isWas) {
						iBorn = f;
						if(g.pJets[0].dir < 0) break;      // from ahead: not this test
					}
					if(iBorn < 0) continue;
					if(g.jetAttacks != uwAtk0 || (g.events & EV_SHRUG)) {
						CHECK(f - iBorn >= 150);                // 3 s, not JET_WARN_FRAMES' 2 s
						++iTested;
						break;
					}
					if(!g.pJets[0].active) break;
				}
			}
		}
	}
	CHECK(iTested >= 40);
	CHECK(JET_WARN_BEHIND >= 150);
}

// The trip's jet budget: only so many jets appear between one visit to the home pad and the
// next, and landing on the pad gives the next trip its own (Paul's playtest, 2026-11-12:
// the far barracks, the longest flight, was the worst; section 5, "Last barracks")
static void testJetTripBudget(void) {
	for(uint8_t d = 1; d <= DIFFICULTY_MAX; ++d) {
		newGame();
		logicSetDifficulty(&g, d);
		g.ghost = 1;
		g.trip = 10;
		flyAt(1500, 120);
		int iSpawns = 0;
		for(int f = 0; f < 9000; ++f) {
			g.chop.x = 1500; g.chop.y = 120; g.chop.landed = 0; g.chop.vx = g.chop.vy = 0;
			g.spawnTimer = 60000;
			step(NONE, 1);
			iSpawns += (g.events & EV_JET) != 0;
		}
		CHECK(iSpawns == logicJetsTripMax(&g));        // the budget is used up, and not more
		CHECK(logicJetsTripMax(&g) >= 2);
		// back on the pad: a new trip, a new budget
		g.chop.x = 2900; g.chop.y = GROUND_Y; g.chop.landed = 1;
		step(NONE, 2);
		CHECK(g.jetsTrip == 0);
	}
	// the budgets: skill 1 smallest, none above 1.5 x the smallest trip's own jets is a measured
	// property (tools/jetsim.c); here only the order and GODMODE = normal
	newGame();
	uint8_t pMax[DIFFICULTY_MAX + 1];
	for(uint8_t d = 0; d <= DIFFICULTY_MAX; ++d) { logicSetDifficulty(&g, d); pMax[d] = logicJetsTripMax(&g); }
	CHECK(pMax[0] == pMax[3]);
	for(uint8_t d = 2; d <= DIFFICULTY_MAX; ++d) CHECK(pMax[d] >= pMax[d - 1]);
	CHECK(pMax[1] < pMax[3]);
}

// The jet's tell: longer on the low skills, 12 frames at least everywhere
static void testJetTellTable(void) {
	static const uint8_t pTell[DIFFICULTY_MAX + 1] = {16, 24, 20, 16, 14, 12};
	newGame();
	for(uint8_t d = 0; d <= DIFFICULTY_MAX; ++d) {
		logicSetDifficulty(&g, d);
		CHECK(logicJetTell(&g) >= 12);
		CHECK(logicJetTell(&g) == pTell[d]);
	}
}

// Skill 1 is noticeably easier than normal: fewer tanks, rarer jets
static void testSkillOneEasier(void) {
	newGame();
	g.trip = 3;
	logicSetDifficulty(&g, 1);
	uint8_t ubT1 = logicTankLimit(&g);
	logicSetDifficulty(&g, 3);
	uint8_t ubT3 = logicTankLimit(&g);
	CHECK(ubT1 + 2 <= ubT3);
	// the time between a jet's passes (leaving to the next one's appearance)
	int iRest[2];
	for(int k = 0; k < 2; ++k) {
		newGame();
		logicSetDifficulty(&g, k ? 3 : 1);
		g.trip = 10;
		g.ghost = 1;
		flyAt(1500, 120);
		g.pJets[0].cool = 1;
		int iLeft = -1, iSum = 0, iN = 0;
		uint8_t ubWas = 0;
		for(int f = 0; f < 12000; ++f) {
			g.chop.x = 1500; g.chop.y = GROUND_Y; g.chop.landed = 1; g.chop.vx = g.chop.vy = 0;
			g.spawnTimer = 60000;
			int16_t wX = g.pJets[0].x;
			step(NONE, 1);
			// (it left past the screen's edge, not into the chopper)
			if(ubWas && !g.pJets[0].active && (wX < g.cam - 40 || wX > g.cam + SCREEN_W + 40)) iLeft = f;
			if(!ubWas && g.pJets[0].active && iLeft >= 0) { iSum += f - iLeft; ++iN; }
			ubWas = g.pJets[0].active;
		}
		iRest[k] = iN ? iSum / iN : 0;
	}
	printf("jet rest: skill 1 %d frames, normal %d\n", iRest[0], iRest[1]);
	CHECK(iRest[0] >= iRest[1] + 80);
}

// ------------------------------------------------------------------------------
// World 2's gameplay (SPEC 4.4, 6; docs/CONTENT-PLAN.md section 4): islands and pads,
// water as a soft stop, the barracks and the rescue, gunboats under the ground-threat
// rules, and the way from world 1's last mission to world 2's first.

// A game on world 2's mission ubM (0-2), in play at the base pad
static void newWorld2(uint8_t ubM) {
	logicInit(&g);
	logicStartAt(&g, 1, ubM);
	step(NONE, INTRO_FRAMES + 1);
	g.spawnTimer = 60000;
}

// Hovering at (x, y), facing left, no airborne grace
static void hoverAt(int16_t x, int16_t y) {
	flyAt(x, y);
	g.chop.face = g.chop.faceTarget = FACE_LEFT;
}

static uint8_t boatsActive(void) {
	uint8_t n = 0;
	for(uint8_t i = 0; i < BOAT_MAX; ++i) n += g.pBoats[i].active;
	return n;
}

// The island table is the tile map's: a column is water exactly when no island has it,
// each barracks and pad stands on its island, the pad bed fits its tiles
static void testW2IslandsMatchTheMap(void) {
	uint8_t pMap[W2_MAP_COLS];
	FILE *f = fopen("art/world2/tilemap.bin", "rb");
	if(!f) f = fopen("../art/world2/tilemap.bin", "rb");
	CHECK(f && fread(pMap, 1, W2_MAP_COLS, f) == W2_MAP_COLS);
	if(f) fclose(f);
	for(uint16_t c = 0; c < W2_MAP_COLS; ++c) {
		int16_t x = (int16_t)(c * 16 + 8);
		CHECK((logicTerrainFloorY(pMap, x) == TERRAIN_WATER) == (logicOnWater(x) != 0));
	}
	for(uint8_t i = 0; i < W2_ISLANDS; ++i) {
		const tIsland *p = &g_pIslands[i];
		CHECK(!logicOnWater((int16_t)((p->x0 + p->x1) / 2)) && logicOnWater((int16_t)(p->x0 - 8)));
		CHECK(p->padX - p->bedPx / 2 >= p->x0 && p->padX + p->bedPx / 2 <= p->x1);
		if(i != W2_BASE_ISLAND) CHECK(p->barracksX - 36 >= p->x0 && p->barracksX + 36 <= p->x1 && p->padX > p->barracksX + 40);
	}
	CHECK(g_pIslands[W2_BASE_ISLAND].padX == W2_START_X && W2_HANGAR_X > g_pIslands[W2_BASE_ISLAND].padX + 40);
	CHECK(logicOnWater(-5) && logicOnWater(W2_W + 5));
}

// The drawn pad is the widest on the 16 px grid (32 + 16 n) that the skill and the bed allow
static void testW2PadWidthBySkill(void) {
	newWorld2(0);
	static const uint8_t pWant3[6] = {64, 96, 80, 64, 48, 48};   // island 3 (96 px bed): GODMODE plays as 3
	static const uint8_t pWant2[6] = {48, 48, 48, 48, 48, 48};   // island 2 (56 px bed)
	static const uint8_t pWant0[6] = {64, 64, 64, 64, 48, 48};   // island 0 (64 px bed)
	for(uint8_t d = 0; d <= DIFFICULTY_MAX; ++d) {
		logicSetDifficulty(&g, d);
		CHECK(logicPadPx(&g, 3) == pWant3[d]);
		CHECK(logicPadPx(&g, 2) == pWant2[d]);
		CHECK(logicPadPx(&g, 0) == pWant0[d]);
		CHECK((logicPadPx(&g, 1) & 15) == 0 && logicPadPx(&g, 1) >= 48 && logicPadPx(&g, 1) <= 96);
	}
	logicSetDifficulty(&g, DIFFICULTY_NORMAL);
	// the zone: the pad and PAD_TOLERANCE px beyond its edge
	int16_t px = g_pIslands[3].padX;
	CHECK(logicPadAt(&g, px) == 3 && logicPadAt(&g, (int16_t)(px + 32 + PAD_TOLERANCE)) == 3);
	CHECK(logicPadAt(&g, (int16_t)(px + 32 + PAD_TOLERANCE + 1)) == 0xFF && logicPadAt(&g, 100) == 0xFF);
	CHECK(logicPadAt(&g, (int16_t)(px - 32 - PAD_TOLERANCE)) == 3 && logicPadAt(&g, (int16_t)(px - 32 - PAD_TOLERANCE - 1)) == 0xFF);
	CHECK(logicPadAt(&g, W2_START_X) == W2_BASE_ISLAND);
}

// Water is a soft stop with a splash: the skids stop at the surface, nothing is damaged, no
// life is lost; one splash a touch (a new one after lifting off the water again)
static void testW2WaterIsASoftStop(void) {
	newWorld2(0);
	logicSetDifficulty(&g, 2);                            // (skill 3 arms mission 4's patrol boat, which patrols this sea)
	hoverAt(800, 150);                                    // the open sea between islands 0 and 1
	uint8_t nSplash = 0;
	tInput down = {.dy = 1};
	for(int i = 0; i < 120; ++i) { step(down, 1); nSplash += (g.events2 & EV2_SPLASH) != 0; }
	CHECK(g.chop.y == GROUND_Y && !g.chop.landed && g.chop.alive && !g.chop.damaged);
	CHECK(g.state == GS_PLAY && g.lives == LIVES_START && nSplash == 1 && g.splashTimer + 20 > 0 && g.splashX == g.chop.x);
	tInput up = {.dy = -1};
	step(up, 12);
	CHECK(g.chop.y < GROUND_Y - 6);
	nSplash = 0;
	for(int i = 0; i < 80; ++i) { step(down, 1); nSplash += (g.events2 & EV2_SPLASH) != 0; }
	CHECK(g.chop.y == GROUND_Y && !g.chop.landed && nSplash == 1);
	// the splash is drawn, the shadow is not (no ground to fall on)
	g.splashTimer = 10;
	g.splashX = g.chop.x;
	tDraw pDraw[DRAW_MAX];
	uint8_t n = logicDraw(&g, pDraw), isSplash = 0, isShadow = 0;
	for(uint8_t i = 0; i < n; ++i) { isSplash |= pDraw[i].bob == BOB_SPLASH; isShadow |= pDraw[i].bob == BOB_SHADOW; }
	CHECK(isSplash && !isShadow);
}

// Landing happens on a pad only: the beach, a barracks roof and the water are floors to stop on
static void testW2LandOnlyOnPads(void) {
	newWorld2(0);
	tInput down = {.dy = 1};
	int16_t xSand = (int16_t)((g_pIslands[3].barracksX + g_pIslands[3].padX) / 2 + 8);   // between barracks 3 and its pad
	hoverAt(xSand, 150);
	step(down, 100);
	CHECK(g.chop.y == GROUND_Y && !g.chop.landed && !logicOnWater(g.chop.x));          // sand: not a landing
	hoverAt(g_pIslands[3].barracksX, 150);
	step(down, 100);
	CHECK(!g.chop.landed && g.chop.y < GROUND_Y);                                      // the roof (as world 1's)
	hoverAt(g_pIslands[3].padX, 150);
	step(down, 100);
	CHECK(g.chop.landed && g.chop.y == GROUND_Y && g.chop.x == g_pIslands[3].padX);
}

// "Pads are forgiving": a touch-down PAD_TOLERANCE outside the edge counts; on skill 1-3 the last
// 12 px of the descent nudge the chopper in, so a few px more still land; on skill 4-5 there is no nudge
static void testW2PadTolerance(void) {
	tInput down = {.dy = 1};
	for(uint8_t d = 1; d <= DIFFICULTY_MAX; ++d) {
		newWorld2(0);
		logicSetDifficulty(&g, d);
		int16_t wHalf = (int16_t)(logicPadPx(&g, 3) / 2 + PAD_TOLERANCE);
		int16_t px = g_pIslands[3].padX;
		hoverAt((int16_t)(px + wHalf), 150);
		step(down, 100);
		CHECK(g.chop.landed);                                      // on the edge of the tolerance
		hoverAt((int16_t)(px + wHalf + 2), 150);
		step(down, 100);
		if(d <= DIFFICULTY_NORMAL) CHECK(g.chop.landed && g.chop.x < px + wHalf + 2);   // nudged in
		else CHECK(!g.chop.landed && g.chop.x == px + wHalf + 2);                       // not helped
		hoverAt((int16_t)(px + wHalf + PAD_ASSIST_REACH + 8), 150);
		step(down, 100);
		CHECK(!g.chop.landed);                                     // too far for any help
	}
}

// The rescue as in world 1: a barracks opens, its people walk toward the pad and board a chopper
// landed there (all 16 of them: the pad is up to 192 px from the barracks), and unload at the base pad
static void testW2RescueFromAnIsland(void) {
	for(uint8_t isl = 0; isl < BARRACKS_COUNT; ++isl) {
		newWorld2(0);
		CHECK(g.pBarracks[isl].x == g_pIslands[isl].barracksX);
		hoverAt(g_pIslands[isl].padX, GROUND_Y - 30);
		g.jetsTrip = 255;   // (no jets in this rescue test: the trip's jets are out; skill 3 has them from the first trip)
		tInput down = {.dy = 1};
		step(down, 40);
		CHECK(g.chop.landed);
		g.pBarracks[isl].hp = 0;                                  // (shot open)
		uint8_t ubMostOut = 0;
		for(int i = 0; i < 1500; ++i) { step(NONE, 1); if(g.outCount > ubMostOut) ubMostOut = g.outCount; }
		CHECK(g.aboard == CABIN_MAX);
		CHECK(ubMostOut > 2 && ubMostOut <= W2_OUT_MAX);          // never more than W2_OUT_MAX on the beach at once
	}
	newWorld2(0);
	g.aboard = 16;
	for(uint8_t i = 0; i < 16; ++i) g.pHostages[i].state = HS_ABOARD;
	step(NONE, 400);
	CHECK(g.chop.landed && g.aboard == 0 && g.saved == 16 && g.trip == 1);
	CHECK(logicHangarX(&g) == W2_HANGAR_X);          // they ran in at the hangar on the base island
}

// A barracks shot open from the sea side: the cannon works on the island barracks as on world 1's
static void testW2CannonOpensABarracks(void) {
	newWorld2(0);
	hoverAt((int16_t)(g_pIslands[3].barracksX + 150), GROUND_Y - 10);
	for(int i = 0; i < 120 && g.pBarracks[3].hp; ++i) step((tInput){.fire = (uint8_t)(i & 1)}, 1);
	CHECK(g.pBarracks[3].hp == 0 && g.score >= 25);
}

// Boats are placed, not spawned: reef run has its patrol boats and gunboats from the start;
// the lower skills meet fewer gunboats; boats stay on their stretch of water
static void testW2BoatsArePlaced(void) {
	newWorld2(0);
	uint8_t nGun = 0, nPatrol = 0;
	for(uint8_t i = 0; i < BOAT_MAX; ++i) {
		const tBoat *b = &g.pBoats[i];
		if(!b->active) continue;
		CHECK(logicOnWater(b->x0) && logicOnWater(b->x1) && b->x >= b->x0 && b->x <= b->x1);
		nGun += b->kind == BOAT_GUN;
		nPatrol += b->kind == BOAT_PATROL;
	}
	CHECK(nGun == 2 && nPatrol == 2);
	logicSetDifficulty(&g, 1);
	logicStartAt(&g, 1, 0);
	uint8_t nGun1 = 0;
	for(uint8_t i = 0; i < BOAT_MAX; ++i) nGun1 += g.pBoats[i].active && g.pBoats[i].kind == BOAT_GUN;
	CHECK(nGun1 == 1);
	logicSetDifficulty(&g, 5);
	logicStartAt(&g, 1, 2);
	uint8_t nGun5 = 0;
	for(uint8_t i = 0; i < BOAT_MAX; ++i) nGun5 += g.pBoats[i].active && g.pBoats[i].kind == BOAT_GUN;
	CHECK(nGun5 > nGun);
	// they patrol: after a while the boat has moved and never leaves its stretch
	newWorld2(0);
	int16_t x0 = g.pBoats[1].x;
	for(int i = 0; i < 1500; ++i) {
		step(NONE, 1);
		for(uint8_t b = 0; b < BOAT_MAX; ++b) if(g.pBoats[b].active) CHECK(g.pBoats[b].x >= g.pBoats[b].x0 - 1 && g.pBoats[b].x <= g.pBoats[b].x1 + 1);
	}
	CHECK(g.pBoats[1].x != x0 && boatsActive() >= 3);
	newGame();            // world 1 has none
	CHECK(boatsActive() == 0);
}

// The gunboat tell table: 22 / 18 / 16 / 14 / 12 frames by skill (SPEC 6, content plan 4.4)
static void testW2BoatTellTable(void) {
	static const uint8_t pTell[6] = {16, 22, 18, 16, 14, 12};
	newWorld2(0);
	for(uint8_t d = 0; d <= DIFFICULTY_MAX; ++d) {
		logicSetDifficulty(&g, d);
		CHECK(logicBoatTell(&g) == pTell[d] && logicBoatTell(&g) >= 12);
	}
}

// Put the first armed boat (index 1 in mission 4) dx px from a chopper hovering at (x, y), facing left (the camera leads that way: a boat to its left is on screen)
static tBoat *gunboatNear(int16_t x, int16_t y, int16_t dx) {
	hoverAt(x, y);
	tBoat *b = &g.pBoats[1];
	b->x0 = (int16_t)(x - 300);
	b->x1 = (int16_t)(x + 300);
	b->x = (int16_t)(x + dx);
	b->cool = 0;
	b->tell = 0;
	b->dir = 1;
	g.atkWait = 0;
	g.jetsTrip = 255;   // (no jets in this boat test: the jets' budget is spent)
	g.pBoats[2].active = 0;
	return b;
}

// Every shell comes after a tell of logicBoatTell() frames, braced and unbroken, on every skill;
// the shell flies toward the chopper
static void testW2GunboatTellsBeforeItShoots(void) {
	for(uint8_t d = 1; d <= DIFFICULTY_MAX; ++d) {
		newWorld2(0);
		logicSetDifficulty(&g, d);
		tBoat *b = gunboatNear(2400, GROUND_Y - 30, -150);      // open sea, a low chopper
		uint8_t nTell = 0, nTellFrames = 0, nShells = 0;
		for(int i = 0; i < 400 && !nShells; ++i) {
			int16_t xb = b->x;
			uint8_t wasTell = b->tell != 0;
			step(NONE, 1);
			if(g.events2 & EV2_BOATTELL) { ++nTell; nTellFrames = 0; CHECK(!g.pEShots[0].active); }
			if(b->tell) { ++nTellFrames; CHECK(wasTell ? b->x == xb : 1); }
			if(g.events2 & EV2_BOATFIRE) {
				++nShells;
				CHECK(nTellFrames + 1 >= logicBoatTell(&g));
				CHECK(g.pEShots[0].active && g.pEShots[0].vx > 0);   // the chopper is to the boat's right
			}
		}
		CHECK(nTell == 1 && nShells == 1);
		CHECK(logicBoatTell(&g) >= 12);
	}
}

// ...and not at a high chopper, not from off screen, not at point-blank, not beyond its sight
static void testW2GunboatFairness(void) {
	// a high chopper (skids above 150) is ignored
	newWorld2(0);
	gunboatNear(2400, 120, -150);
	step(NONE, 300);
	CHECK(!g.pBoats[1].tell && !g.pEShots[0].active);
	// off screen: the boat is past the left edge
	newWorld2(0);
	gunboatNear(2400, GROUND_Y - 30, -200);
	for(int i = 0; i < 200; ++i) { g.cam = (int16_t)(2400 - 100); g.pBoats[1].x = 2200; step(NONE, 1); CHECK(!g.pBoats[1].tell); }
	// point-blank
	newWorld2(0);
	gunboatNear(2400, GROUND_Y - 30, -40);
	for(int i = 0; i < 200; ++i) { g.pBoats[1].x = 2360; step(NONE, 1); CHECK(!g.pBoats[1].tell); }
	// out of its sight
	newWorld2(0);
	gunboatNear(2400, GROUND_Y - 30, (int16_t)-(BOAT_RANGE + 20));
	for(int i = 0; i < 200; ++i) { g.cam = (int16_t)(2400 - 300); g.pBoats[1].x = (int16_t)(2400 - BOAT_RANGE - 20); step(NONE, 1); CHECK(!g.pBoats[1].tell); }
	// just after take-off: no new attack on a low chopper (the jets' 2 s grace, shared)
	newWorld2(0);
	gunboatNear(2400, GROUND_Y - 30, -150);
	for(int i = 0; i < 90; ++i) { g.airGrace = JET_TAKEOFF_GRACE; step(NONE, 1); CHECK(!g.pBoats[1].tell); }
}

// A chopper that has just landed is not a target until its ground dwell (6 s on normal, 4 s, 3 s;
// never on skill 1-2: the garrison overflies), the one table every ground weapon shares
static void testW2LandedChopperHasGrace(void) {
	for(uint8_t d = 1; d <= DIFFICULTY_MAX; ++d) {
		newWorld2(0);
		logicSetDifficulty(&g, d);
		tBoat *b = gunboatNear(g_pIslands[2].padX, GROUND_Y - 20, -140);
		step((tInput){.dy = 1}, 30);
		CHECK(g.chop.landed);
		uint16_t uwDwell = logicGroundDwell(&g), uwFirstTell = 0;
		for(uint16_t i = 0; i < 700 && !uwFirstTell; ++i) {
			b->x = (int16_t)(g.chop.x - 140);                        // (it keeps its place)
			b->cool = 0;
			g.atkWait = 0;
			step(NONE, 1);
			g.pBoats[2].active = 0;
			if(b->tell) uwFirstTell = g.landFrames;
		}
		if(uwDwell == GROUND_NEVER) CHECK(!uwFirstTell);
		else CHECK(uwFirstTell >= uwDwell);
	}
}

// One attack at a time: once a boat has started its tell, another waits BOAT_ATTACK_GAP frames
static void testW2OneBoatAttackAtATime(void) {
	newWorld2(0);
	logicSetDifficulty(&g, 4);
	tBoat *a = gunboatNear(2400, GROUND_Y - 30, -130);
	tBoat *b = &g.pBoats[2];
	b->active = 1; b->kind = BOAT_GUN; b->hp = 3; b->x0 = 2000; b->x1 = 2800; b->x = 2310; b->cool = 0; b->tell = 0; b->dir = 1;
	tBoat *pFirst = 0, *pOther = 0;
	int16_t wFirst = -1;
	for(int i = 0; i < 400; ++i) {
		step(NONE, 1);
		if(wFirst < 0 && (a->tell || b->tell)) { wFirst = (int16_t)i; pFirst = a->tell ? a : b; pOther = a->tell ? b : a; }
		if(wFirst >= 0 && i < wFirst + BOAT_ATTACK_GAP) CHECK(!pOther->tell);
		CHECK(!(a->tell && b->tell));
	}
	CHECK(wFirst >= 0 && pFirst != pOther);
}

// ---- patrol boats (decided by Paul, 2026-11-13): unarmed in mission 4, armed from mission 5 under
// the gunboats' fairness rules, with a slower wind-up (PATROL_TELL_EXTRA more frames) and a longer rest

// The mission's patrol boat (the last stretch of sea), alone on the map, dx px from a chopper hovering at (x, y)
static tBoat *patrolNear(int16_t x, int16_t y, int16_t dx) {
	hoverAt(x, y);
	tBoat *p = 0;
	for(uint8_t i = 0; i < BOAT_MAX; ++i) {
		if(g.pBoats[i].active && g.pBoats[i].kind == BOAT_PATROL && !p) p = &g.pBoats[i];
		else g.pBoats[i].active = 0;
	}
	CHECK(p != 0);
	if(!p) return &g.pBoats[0];
	p->x0 = (int16_t)(x - 300);
	p->x1 = (int16_t)(x + 300);
	p->x = (int16_t)(x + dx);
	p->cool = 0;
	p->tell = 0;
	p->dir = 1;
	g.atkWait = 0;
	g.jetsTrip = 255;   // (this trip's jets are all out: the boat alone, jets are world 2's from skill 3, v1.16)
	return p;
}

static void testW2PatrolArmedFromMission5(void) {
	newWorld2(0); logicSetDifficulty(&g, 2); CHECK(!logicPatrolArmed(&g));
	newWorld2(0); logicSetDifficulty(&g, 3); CHECK(logicPatrolArmed(&g));   // skill 3 up: armed from mission 4 too (Paul, 2026-11-14)
	newWorld2(1); CHECK(logicPatrolArmed(&g));
	newWorld2(2); CHECK(logicPatrolArmed(&g));
	newGame(); CHECK(!logicPatrolArmed(&g));
	// the wind-up: the gunboat's table plus PATROL_TELL_EXTRA, never shorter than a gunboat's
	newWorld2(1);
	for(uint8_t d = 0; d <= DIFFICULTY_MAX; ++d) {
		logicSetDifficulty(&g, d);
		CHECK(logicPatrolTell(&g) == logicBoatTell(&g) + PATROL_TELL_EXTRA && logicPatrolTell(&g) >= 12);
	}
}

// Mission 4: skills 1-2 never see a patrol shot (not even a tell), however it is set up for one
static void testW2PatrolUnarmedInMission4(void) {
	for(uint8_t d = 1; d < 3; ++d) {
		newWorld2(0);
		logicSetDifficulty(&g, d);
		tBoat *p = patrolNear(2400, GROUND_Y - 30, -150);
		for(int i = 0; i < 600; ++i) {
			p->cool = 0; g.atkWait = 0;
			step(NONE, 1);
			CHECK(!p->tell && !(g.events2 & (EV2_BOATTELL | EV2_BOATFIRE)) && !g.pEShots[0].active);
		}
	}
}

// A gunboat's pause between shells: never longer on a higher skill, and from skill 3 shorter than the tank's table alone gave
static void testW2GunboatFiresFasterWithSkill(void) {
	newWorld2(0);
	uint8_t prev = 255;
	for(uint8_t d = 1; d <= DIFFICULTY_MAX; ++d) {
		logicSetDifficulty(&g, d);
		uint8_t gap = logicBoatGap(&g);
		CHECK(gap <= prev);
		prev = gap;
	}
	logicSetDifficulty(&g, 2); uint8_t g2 = logicBoatGap(&g);
	logicSetDifficulty(&g, 3); uint8_t g3 = logicBoatGap(&g);
	logicSetDifficulty(&g, 5); uint8_t g5 = logicBoatGap(&g);
	CHECK(g3 + 10 <= g2 && g5 + 20 <= g3);
}

// Mission 5 and 6: a tell of logicPatrolTell() frames (the tell frame drawn, braced), then the shell
static void testW2PatrolTellsThenShoots(void) {
	for(uint8_t m = 1; m <= 2; ++m) {
		for(uint8_t d = 1; d <= DIFFICULTY_MAX; ++d) {
			newWorld2(m);
			logicSetDifficulty(&g, d);
			tBoat *p = patrolNear(2400, GROUND_Y - 30, -150);
			uint8_t nTell = 0, nShells = 0, nTellDrawn = 0;
			uint16_t nTellFrames = 0;
			for(int i = 0; i < 400 && !nShells; ++i) {
				int16_t xb = p->x;
				uint8_t wasTell = p->tell != 0;
				step(NONE, 1);
				if(g.events2 & EV2_BOATTELL) { ++nTell; nTellFrames = 0; CHECK(!g.pEShots[0].active); }
				if(p->tell) {
					++nTellFrames;
					CHECK(wasTell ? p->x == xb : 1);
					tDraw pDraw[DRAW_MAX];
					uint8_t n = logicDraw(&g, pDraw);
					for(uint8_t k = 0; k < n; ++k) {
						if(pDraw[k].bob == BOB_PATROL && pDraw[k].frame == PATROL_TELL_R) ++nTellDrawn;   // (it faces the chopper: to its right)
					}
				}
				if(g.events2 & EV2_BOATFIRE) {
					++nShells;
					CHECK(nTellFrames + 1 >= logicPatrolTell(&g));
					CHECK(g.pEShots[0].active && g.pEShots[0].vx > 0);
				}
			}
			CHECK(nTell == 1 && nShells == 1 && nTellDrawn + 1 >= logicPatrolTell(&g));
		}
	}
}

// ...under the gunboats' fairness rules: not at a high chopper, from off screen, point-blank, out of
// sight, just after take-off or a fresh landing, and one attack at a time with the gunboats
static void testW2PatrolFairness(void) {
	for(uint8_t m = 1; m <= 2; ++m) {
		newWorld2(m); patrolNear(2400, 120, -150);
		for(int i = 0; i < 300; ++i) { step(NONE, 1); CHECK(!g.events2 && !g.pEShots[0].active); }
		tBoat *p;
		newWorld2(m); p = patrolNear(2400, GROUND_Y - 30, -200);
		for(int i = 0; i < 200; ++i) { g.cam = (int16_t)(2400 - 100); p->x = 2200; step(NONE, 1); CHECK(!p->tell); }
		newWorld2(m); p = patrolNear(2400, GROUND_Y - 30, -40);
		for(int i = 0; i < 200; ++i) { p->x = 2360; step(NONE, 1); CHECK(!p->tell); }
		newWorld2(m); p = patrolNear(2400, GROUND_Y - 30, (int16_t)-(BOAT_RANGE + 20));
		for(int i = 0; i < 200; ++i) { g.cam = (int16_t)(2400 - 300); p->x = (int16_t)(2400 - BOAT_RANGE - 20); step(NONE, 1); CHECK(!p->tell); }
		newWorld2(m); p = patrolNear(2400, GROUND_Y - 30, -150);
		for(int i = 0; i < 90; ++i) { g.airGrace = JET_TAKEOFF_GRACE; step(NONE, 1); CHECK(!p->tell); }
	}
	// a fresh landing: no tell before the ground dwell (never on skill 1-2)
	for(uint8_t d = 1; d <= DIFFICULTY_MAX; ++d) {
		newWorld2(1);
		logicSetDifficulty(&g, d);
		tBoat *p = patrolNear(g_pIslands[2].padX, GROUND_Y - 20, -140);
		step((tInput){.dy = 1}, 30);
		CHECK(g.chop.landed);
		uint16_t uwDwell = logicGroundDwell(&g), uwFirstTell = 0;
		for(uint16_t i = 0; i < 700 && !uwFirstTell; ++i) {
			p->x = (int16_t)(g.chop.x - 140); p->cool = 0; g.atkWait = 0;
			step(NONE, 1);
			if(p->tell) uwFirstTell = g.landFrames;
		}
		if(uwDwell == GROUND_NEVER) CHECK(!uwFirstTell);
		else CHECK(uwFirstTell >= uwDwell);
	}
	// one attack at a time: a gunboat that has begun its tell holds the patrol boat back BOAT_ATTACK_GAP frames
	newWorld2(1);
	logicSetDifficulty(&g, 4);
	tBoat *p = patrolNear(2400, GROUND_Y - 30, -130);
	tBoat *b = &g.pBoats[1];
	b->active = 1; b->kind = BOAT_GUN; b->hp = 3; b->x0 = 2000; b->x1 = 2800; b->x = 2310; b->cool = 0; b->tell = 0; b->dir = 1;
	int16_t wFirst = -1;
	tBoat *pFirst = 0, *pOther = 0;
	for(int i = 0; i < 400; ++i) {
		step(NONE, 1);
		if(wFirst < 0 && (p->tell || b->tell)) { wFirst = (int16_t)i; pFirst = p->tell ? p : b; pOther = p->tell ? b : p; }
		if(wFirst >= 0 && i < wFirst + BOAT_ATTACK_GAP) CHECK(!pOther->tell);
		CHECK(!(p->tell && b->tell));
	}
	CHECK(wFirst >= 0 && pFirst != pOther);
}

// Boats die to the cannon (3 hits for a gunboat) and to a bomb; they pay
static void testW2BoatsCanBeShot(void) {
	newWorld2(0);
	tBoat *b = gunboatNear(2400, GROUND_Y - 4, -100);
	g.chop.face = g.chop.faceTarget = FACE_LEFT;
	uint32_t ulScore = g.score;
	for(int i = 0; i < 200 && b->active; ++i) { b->x = 2300; b->cool = 250; step((tInput){.fire = (uint8_t)(i & 1)}, 1); }
	CHECK(!b->active && g.score >= ulScore + 100);
	// a bomb kills a gunboat outright
	newWorld2(0);
	b = gunboatNear(2400, 100, 0);
	b->cool = 250;
	g.pShots[0] = (tShot){.x = 2400, .y = 225, .vx = 0, .vy = 16, .active = 1, .kind = SHOT_BOMB, .life = 100};
	step(NONE, 12);
	CHECK(!b->active);
}

// From the third mission on to the fourth: world 2 begins, lives refilled, the score kept, the
// chopper on the base island's pad, its boats placed; world 2's last mission ends the game
static void testW2MissionsFollowWorld1(void) {
	newGame();
	g.mission = MISSION_COUNT - 1;
	g.score = 1234;
	g.lives = 1;
	g.saved = HOSTAGE_COUNT;
	step(NONE, 1);
	CHECK(g.state == GS_CLEAR);
	step(NONE, CLEAR_FRAMES + 1);
	CHECK(g.state == GS_INTRO && g.world == 1 && g.mission == 0 && g.lives == LIVES_START && g.score >= 1234 + HOSTAGE_COUNT * 10);   // (the score is kept, with the clear's)
	CHECK(g.chop.x == W2_START_X && logicWorldW(&g) == W2_W && g.saved == 0 && boatsActive() >= 3);
	step(NONE, INTRO_FRAMES + 1);
	CHECK(g.state == GS_PLAY);
	g.saved = HOSTAGE_COUNT;
	step(NONE, 1);
	step(NONE, CLEAR_FRAMES + 1);
	CHECK(g.state == GS_INTRO && g.world == 1 && g.mission == 1 && g.chop.x == W2_START_X);
	g.mission = MISSION_COUNT - 1;
	g.state = GS_PLAY;
	g.saved = HOSTAGE_COUNT;
	step(NONE, 1);
	CHECK(g.state == GS_CLEAR);
	step(NONE, CLEAR_FRAMES + 1);
	CHECK(g.state == GS_VICTORY);
	// the hall of fame knows six missions and "all done"
	CHECK(g_sHiConfig.ubReachedMax == MISSIONS_TOTAL + 1);
	logicInit(&g);
	logicStartAt(&g, 1, 1);
	step(NONE, INTRO_FRAMES + 1);
	g.score = 99999;
	g.state = GS_OVER;
	g.stateTimer = 1;
	step(NONE, 2);
	CHECK(g.state == GS_ENTRY && g.hi.pEntries[g.newRank].reached == MISSION_COUNT + 2);   // mission 5
}

// The card's one-line brief and strip map: what is out there, and the islands, barracks,
// pads, hangar and the "you are here" marker on a map of 4608 px at an 18th
static void testW2CardBriefAndMap(void) {
	newWorld2(0);
	char sz4[40], sz5[40], sz6[40];
	strcpy(sz4, logicBrief(&g));
	logicStartAt(&g, 1, 1);
	strcpy(sz5, logicBrief(&g));
	logicStartAt(&g, 1, 2);
	strcpy(sz6, logicBrief(&g));
	CHECK(sz4[0] && sz5[0] && sz6[0] && strcmp(sz4, sz5) && strcmp(sz5, sz6) && strstr(sz4, "BOAT"));
	logicStartAt(&g, 1, 0);
	tRouteMark pM[ROUTE_MARKS];
	uint8_t n = logicRouteMap(&g, pM), nIsland = 0, nBarracks = 0, nPad = 0, nHere = 0, nHangar = 0;
	CHECK(n <= ROUTE_MARKS);
	for(uint8_t i = 0; i < n; ++i) {
		CHECK(pM[i].x + pM[i].w <= ROUTE_W);
		nIsland += pM[i].kind == ROUTE_ISLAND;
		nBarracks += pM[i].kind == ROUTE_BARRACKS;
		nPad += pM[i].kind == ROUTE_PAD;
		nHangar += pM[i].kind == ROUTE_HANGAR;
		if(pM[i].kind == ROUTE_HERE) { ++nHere; CHECK(pM[i].x == W2_START_X / 18); }
	}
	CHECK(nIsland == W2_ISLANDS && nBarracks == BARRACKS_COUNT && nPad == W2_ISLANDS && nHere == 1 && nHangar == 1);
	CHECK(pM[n - 1].kind == ROUTE_HERE);
}

// The island barracks, the pads (a cap, units, an H, units, a cap, two lamps) and the hangar
// are drawn where the camera sees them
static void testW2DrawsIslands(void) {
	newWorld2(0);
	g.cam = (int16_t)(g_pIslands[3].padX - 200);
	g.frame = 0;                              // (the lamps blink: shown at frame 0)
	tDraw pDraw[DRAW_MAX];
	g.chop.x = g_pIslands[3].padX;             // (the lamps wake up as the chopper comes near)
	uint8_t n = logicDraw(&g, pDraw), nBarracks = 0, nPad = 0, nLamp = 0, nBoat = 0;
	for(uint8_t i = 0; i < n; ++i) {
		nBarracks += pDraw[i].bob == BOB_W2_BARRACKS;
		nPad += pDraw[i].bob == BOB_PAD48 + (logicPadPx(&g, 3) - 48) / 16 && pDraw[i].x == g_pIslands[3].padX - logicPadPx(&g, 3) / 2 - g.cam;
		nLamp += pDraw[i].bob == BOB_PAD_LAMP;
		CHECK(pDraw[i].bob != BOB_BARRACKS && pDraw[i].bob != BOB_FENCE && pDraw[i].bob != BOB_FLAG);
		CHECK(pDraw[i].bob < BOB_TYPES);
	}
	CHECK(nBarracks == 1 && nPad == 1 && nLamp == 2);   // a pad is one BOB (not a dozen: each BOB is ~2 % of an A500 frame)
	g.chop.x = W2_START_X;                              // far from it: no lamps
	n = logicDraw(&g, pDraw);
	nLamp = 0;
	for(uint8_t i = 0; i < n; ++i) nLamp += pDraw[i].bob == BOB_PAD_LAMP;
	CHECK(nLamp == 0);
	g.cam = (int16_t)(g_pIslands[1].x1 + 20);                  // the sea east of island 1
	n = logicDraw(&g, pDraw);
	for(uint8_t i = 0; i < n; ++i) nBoat += pDraw[i].bob == BOB_GUNBOAT || pDraw[i].bob == BOB_PATROL;
	CHECK(nBoat >= 1);
	g.cam = (int16_t)(W2_HANGAR_X - 100);
	n = logicDraw(&g, pDraw);
	uint8_t nHangar = 0;
	for(uint8_t i = 0; i < n; ++i) nHangar += pDraw[i].bob == BOB_HANGAR;
	CHECK(nHangar == 1);
	for(uint8_t i = 0; i < BOB_TYPES; ++i) CHECK(g_pBobW[i] && g_pBobH[i] && g_pBobFrames[i]);
}

// ---- jets in world 2 (Paul's playtest, 2026-11-14: mission 4 at skill 3 had "no airplanes"; v1.16) ----
// The spec's table (docs/SPEC.md section 6, "Jets in world 2"), written out here on its own: the trip a mission's
// jets begin on (255: none at that skill) and the jets that may appear in a trip, by mission 4 / 5 / 6 and skill 0-5
// (skill 0, GODMODE, plays like 3)
static const uint8_t s_pW2JetFrom[3][DIFFICULTY_MAX + 1] = {
	{0, 255, 255, 0, 0, 0}, {0, 255, 2, 0, 0, 0}, {0, 3, 1, 0, 0, 0},
};
static const uint8_t s_pW2JetBudget[3][DIFFICULTY_MAX + 1] = {
	{3, 0, 0, 3, 4, 5}, {4, 0, 2, 4, 5, 6}, {5, 1, 3, 5, 6, 6},
};

static void noBoats(void) {
	for(uint8_t i = 0; i < BOAT_MAX; ++i) g.pBoats[i].active = 0;
}

// Jets are on exactly where the table says: nothing on skill 1-2 in mission 4 (gentle), more of them by skill and
// mission, GODMODE like normal, and world 1's rule untouched
static void testW2JetsOnlyFromTheAllowedSkillAndMission(void) {
	for(uint8_t m = 0; m < 3; ++m) {
		for(uint8_t d = 0; d <= DIFFICULTY_MAX; ++d) {
			newWorld2(m);
			logicSetDifficulty(&g, d);
			for(uint8_t trip = 0; trip < 8; ++trip) {
				g.trip = trip;
				uint8_t isOn = s_pW2JetFrom[m][d] != 255 && trip >= s_pW2JetFrom[m][d];
				if(logicJetsOn(&g) != isOn) printf("world 2 mission %d skill %d trip %d: jets %d, want %d\n", m + 4, d, trip, logicJetsOn(&g), isOn);
				CHECK(logicJetsOn(&g) == isOn);
			}
			CHECK(logicJetsTripMax(&g) == s_pW2JetBudget[m][d]);
		}
		// scaled by skill: never fewer on a higher skill, never fewer in a later mission
		for(uint8_t d = 2; d <= DIFFICULTY_MAX; ++d) CHECK(s_pW2JetBudget[m][d] >= s_pW2JetBudget[m][d - 1]);
		// ... and never beginning later on a higher skill (255 = none, the latest of all)
		for(uint8_t d = 2; d <= DIFFICULTY_MAX; ++d) CHECK(s_pW2JetFrom[m][d] <= s_pW2JetFrom[m][d - 1]);
	}
	for(uint8_t d = 1; d <= DIFFICULTY_MAX; ++d) for(uint8_t m = 1; m < 3; ++m) CHECK(s_pW2JetBudget[m][d] >= s_pW2JetBudget[m - 1][d]);
	for(uint8_t d = 1; d <= DIFFICULTY_MAX; ++d) for(uint8_t m = 1; m < 3; ++m) CHECK(s_pW2JetFrom[m][d] <= s_pW2JetFrom[m - 1][d]);
	// Paul's playtest (decided 2026-11-17): mission 4 at skill 3, the default, has jets from the first trip; skill 1-2 none
	newWorld2(0);
	logicSetDifficulty(&g, 3);
	g.trip = 0;
	CHECK(logicJetsOn(&g) && logicJetsTripMax(&g) == 3);
	logicSetDifficulty(&g, 2);
	CHECK(!logicJetsOn(&g));
	logicSetDifficulty(&g, 1);
	CHECK(!logicJetsOn(&g));
	// world 1 as before
	newGame();
	logicSetDifficulty(&g, 3);
	g.trip = 1;
	CHECK(logicJetsOn(&g) && logicJetsTripMax(&g) == 5);
	g.trip = 0;
	CHECK(!logicJetsOn(&g));
}

// A jet appears off screen with its warning (the alert, the radar flash, 2 s before any attack run), and the trip's
// budget is all there is: the game's own counts, mission by mission and skill by skill
static void testW2JetsAppearOffScreenWithAWarningWithinTheBudget(void) {
	for(uint8_t m = 0; m < 3; ++m) {
		for(uint8_t d = 1; d <= DIFFICULTY_MAX; ++d) {
			newWorld2(m);
			logicSetDifficulty(&g, d);
			noBoats();
			g.ghost = 1;
			g.trip = s_pW2JetFrom[m][d] == 255 ? 9 : s_pW2JetFrom[m][d];
			flyAt(1500, 120);
			int iSpawns = 0, iBorn[JET_MAX] = {-1, -1};
			for(int f = 0; f < 9000; ++f) {
				g.chop.x = 1500; g.chop.y = 120; g.chop.landed = 0; g.chop.vx = g.chop.vy = 0;
				g.spawnTimer = 60000;
				uint8_t pWas[JET_MAX] = {g.pJets[0].active, g.pJets[1].active};
				uint8_t pTell[JET_MAX] = {g.pJets[0].tell, g.pJets[1].tell};
				step(NONE, 1);
				for(int i = 0; i < JET_MAX; ++i) {
					const tJet *j = &g.pJets[i];
					if(j->active && !pWas[i]) {
						iBorn[i] = f;
						CHECK(j->x <= g.cam - JET_LEAD || j->x >= g.cam + SCREEN_W + JET_LEAD);   // well off screen
						CHECK(j->warn >= JET_WARN_FRAMES - 1);
						CHECK((g.events & EV_JET) && (g.events2 & EV2_JETWARN));            // heard, and the alert
					}
					if(j->active && j->tell && !pTell[i]) CHECK(iBorn[i] >= 0 && f - iBorn[i] >= JET_WARN_FRAMES);
				}
				iSpawns += (g.events & EV_JET) != 0;
			}
			if(iSpawns != s_pW2JetBudget[m][d]) printf("mission %d skill %d: %d jets, want %d\n", m + 4, d, iSpawns, s_pW2JetBudget[m][d]);
			CHECK(iSpawns == s_pW2JetBudget[m][d]);
			// the trip before the first one with jets: none
			if(s_pW2JetFrom[m][d] != 255 && s_pW2JetFrom[m][d] > 0) {
				newWorld2(m);
				logicSetDifficulty(&g, d);
				noBoats();
				g.ghost = 1;
				g.trip = (uint8_t)(s_pW2JetFrom[m][d] - 1);
				flyAt(1500, 120);
				int iEarly = 0;
				for(int f = 0; f < 3000; ++f) {
					g.chop.x = 1500; g.chop.y = 120; g.chop.vx = g.chop.vy = 0;
					g.spawnTimer = 60000;
					step(NONE, 1);
					iEarly += (g.events & EV_JET) != 0;
				}
				CHECK(iEarly == 0);
			}
		}
	}
	// back on the base pad: a new trip, a new budget
	newWorld2(0);
	logicSetDifficulty(&g, 3);
	g.jetsTrip = 3;
	g.chop.x = g_pIslands[W2_BASE_ISLAND].padX; g.chop.y = GROUND_Y; g.chop.landed = 1;
	step(NONE, 2);
	CHECK(g.jetsTrip == 0);
}

// None while the chopper is in the base's approach (right of W2_JET_SAFE_X), none in the intro or while it sits at the base
static void testW2JetsNeverNearTheBaseOrInTheIntro(void) {
	logicInit(&g);
	logicStartAt(&g, 1, 2);
	logicSetDifficulty(&g, 5);
	g.trip = 6;
	for(int f = 0; f < INTRO_FRAMES + 600; ++f) {
		g.spawnTimer = 60000;
		step(NONE, 1);
		CHECK(!g.pJets[0].active && !g.pJets[1].active && !(g.events & EV_JET));
	}
	newWorld2(2);
	logicSetDifficulty(&g, 5);
	noBoats();
	g.ghost = 1;
	g.trip = 6;
	flyAt(W2_JET_SAFE_X + 40, 120);
	for(int f = 0; f < 3000; ++f) {
		g.chop.x = W2_JET_SAFE_X + 40; g.chop.y = 120; g.chop.vx = g.chop.vy = 0;
		g.spawnTimer = 60000;
		step(NONE, 1);
		CHECK(!(g.events & EV_JET));
	}
	// (the same chopper a little further from the base: they come)
	flyAt(W2_JET_SAFE_X - 40, 120);
	int iSpawns = 0;
	for(int f = 0; f < 3000; ++f) {
		g.chop.x = W2_JET_SAFE_X - 40; g.chop.y = 120; g.chop.vx = g.chop.vy = 0;
		g.spawnTimer = 60000;
		step(NONE, 1);
		iSpawns += (g.events & EV_JET) != 0;
	}
	CHECK(iSpawns >= 1);
}

// Skill 1-2 in mission 4, skill 1 in mission 5: nothing, however long you fly and however many trips
static void testW2LowSkillsStayGentle(void) {
	for(uint8_t k = 0; k < 3; ++k) {
		uint8_t m = k == 2 ? 1 : 0, d = k == 2 ? 1 : (uint8_t)(k + 1);
		newWorld2(m);
		logicSetDifficulty(&g, d);
		noBoats();
		g.ghost = 1;
		g.trip = 9;
		flyAt(1500, 120);
		for(int f = 0; f < 9000; ++f) {
			g.chop.x = 1500; g.chop.y = 120; g.chop.vx = g.chop.vy = 0;
			g.spawnTimer = 60000;
			step(NONE, 1);
			CHECK(!g.pJets[0].active && !g.pJets[1].active);
		}
	}
}

// Paul's playtest, decided 2026-11-17: mission 4 at skill 3 has jets from its first trip, but the first jet of a world 2
// mission arrives late in it (10 s of flying at least, then 2 s of warning): the pads and the water come first.
// Every skill's budget is still used up in that first trip.
static void testW2FirstTripJetComesLate(void) {
	for(uint8_t m = 0; m < 3; ++m) {
		for(uint8_t d = 1; d <= DIFFICULTY_MAX; ++d) {
			if(s_pW2JetFrom[m][d] != 0) continue;
			newWorld2(m);
			logicSetDifficulty(&g, d);
			noBoats();
			g.ghost = 1;
			g.trip = 0;
			flyAt(1500, 120);
			int iFirst = -1, iSpawns = 0;
			for(int f = 0; f < 9000; ++f) {
				g.chop.x = 1500; g.chop.y = 120; g.chop.landed = 0; g.chop.vx = g.chop.vy = 0;
				g.spawnTimer = 60000;
				step(NONE, 1);
				if(g.events & EV_JET) { if(iFirst < 0) iFirst = f; ++iSpawns; }
			}
			if(iFirst < 500) printf("mission %d skill %d: the first jet at frame %d\n", m + 4, d, iFirst);
			CHECK(iFirst >= 500 && iFirst < 900);
			CHECK(iSpawns == s_pW2JetBudget[m][d]);
		}
	}
	// the second trip is not held back again: a jet well inside the 10 s
	newWorld2(0);
	logicSetDifficulty(&g, 3);
	noBoats();
	g.ghost = 1;
	g.trip = 0;
	flyAt(1500, 120);
	for(int f = 0; f < 3000; ++f) {
		g.chop.x = 1500; g.chop.y = 120; g.chop.landed = 0; g.chop.vx = g.chop.vy = 0;
		g.spawnTimer = 60000;
		step(NONE, 1);
	}
	g.jetsTrip = 0;
	g.trip = 1;
	int iAgain = -1;
	for(int f = 0; f < 600 && iAgain < 0; ++f) {
		g.chop.x = 1500; g.chop.y = 120; g.chop.landed = 0; g.chop.vx = g.chop.vy = 0;
		g.spawnTimer = 60000;
		step(NONE, 1);
		if(g.events & EV_JET) iAgain = f;
	}
	CHECK(iAgain >= 0 && iAgain < 400);
}

// The mission card's brief says "AND JETS" exactly when jets can appear at the skill in force (Paul, decided 2026-11-17),
// in a line that fits the card (40 columns; the longest before was 28), and is unchanged where there are none
static void testW2BriefSaysJetsExactlyWhenTheyCanAppear(void) {
	static const char *const pNoJets[3] = {"PATROL BOATS AND GUNBOATS", "GUNBOATS GUARD THE SEA LANES", "GUNBOATS HOLD THE STRAIT"};
	char pSeen[3][DIFFICULTY_MAX + 1][48];
	for(uint8_t m = 0; m < 3; ++m) {
		for(uint8_t d = 0; d <= DIFFICULTY_MAX; ++d) {
			newWorld2(m);
			logicSetDifficulty(&g, d);
			const char *sz = logicBrief(&g);
			CHECK(strlen(sz) > 0 && strlen(sz) <= 36);
			uint8_t isJets = s_pW2JetFrom[m][d] != 255;
			if(!!strstr(sz, "JETS") != isJets) printf("mission %d skill %d: brief \"%s\", jets %d\n", m + 4, d, sz, isJets);
			CHECK(!!strstr(sz, "JETS") == isJets);
			if(!isJets) CHECK(!strcmp(sz, pNoJets[m]));
			CHECK(strstr(sz, "BOAT") != 0);              // (it still says what the sea holds)
			strcpy(pSeen[m][d], sz);
		}
		CHECK(!strcmp(pSeen[m][0], pSeen[m][3]));        // GODMODE reads like normal
	}
	// the briefs differ by mission, and mission 4 at normal skill (default) names jets and boats
	CHECK(strcmp(pSeen[0][3], pSeen[1][3]) && strcmp(pSeen[1][3], pSeen[2][3]));
	CHECK(strstr(pSeen[0][3], "PATROL BOATS") && strstr(pSeen[0][3], "JETS"));
	// world 1 as before
	newGame();
	logicSetDifficulty(&g, 3);
	logicStartMission(&g, 1);
	CHECK(!strcmp(logicBrief(&g), "TANKS AND JETS ON PATROL"));
}

// Every missile comes after the whole flash (logicJetTell frames) on the water too, flying at the chopper
static void testW2JetTellsThenFires(void) {
	for(uint8_t d = 3; d <= DIFFICULTY_MAX; ++d) {
		newWorld2(1);
		logicSetDifficulty(&g, d);
		noBoats();
		uint8_t ubTell = logicJetTell(&g);
		int iMissiles = 0;
		for(int pass = 0; pass < 6; ++pass) {
			flyAt(1500, 120);
			g.trip = 5;
			g.pJets[0] = (tJet){.x = (int16_t)(g.cam + 20), .y = 112, .dir = 1, .active = 1, .hp = 1, .seen = 100};
			uint8_t ubRun = 0;
			for(int f = 0; f < 200; ++f) {
				g.chop.x = 1530; g.chop.y = 120; g.chop.vx = g.chop.vy = 0; g.chop.landed = 0;
				g.cam = 1340;
				g.pJets[1].active = 0; g.pJets[1].cool = 255;
				uint8_t ubBefore = ubRun;
				step(NONE, 1);
				if(!g.pJets[0].active) break;
				ubRun = g.pJets[0].tell ? (uint8_t)(ubRun + 1) : 0;
				for(int s = 0; s < ESHOT_MAX; ++s) {
					tShot *p = &g.pEShots[s];
					if(p->active && p->kind == ESHOT_MISSILE && p->life == JET_MISSILE_LIFE) {
						++iMissiles;
						CHECK(ubBefore >= ubTell);          // the whole flash came first
						CHECK(absi(p->vx) == 2 * 20);       // 2.5 px/frame
						p->active = 0;
						g.pJets[0].active = 0;
					}
				}
				g.chop.alive = 1;
			}
		}
		CHECK(iMissiles >= 1);
	}
}

// A chopper on an island's pad: the ground grace of the shared table (never on skill 1-2, 6 s on 3, ...); no bomb before it
static void testW2JetGroundGrace(void) {
	static const uint16_t pDwell[DIFFICULTY_MAX + 1] = {300, GROUND_NEVER, GROUND_NEVER, 300, 200, 150};
	for(uint8_t d = 2; d <= DIFFICULTY_MAX; ++d) {
		newWorld2(d == 2 ? 1 : 0);
		logicSetDifficulty(&g, d);
		noBoats();
		g.ghost = 1;
		g.trip = 6;
		int16_t wX = g_pIslands[1].padX;
		flyAt(wX, GROUND_Y);
		g.chop.landed = 1;
		CHECK(logicGroundDwell(&g) == pDwell[d]);
		int iFirst = -1, iBombs = 0;
		for(int f = 0; f < 1000; ++f) {
			g.chop.x = wX; g.chop.y = GROUND_Y; g.chop.landed = 1; g.chop.vx = g.chop.vy = 0;
			g.spawnTimer = 60000;
			g.pJets[1].active = 0; g.pJets[1].cool = 255;
			if(!g.pJets[0].tell) aimJet(0, 1);
			uint16_t uwBefore = g.jetAttacks;
			step(NONE, 1);
			iBombs += bombsUp() > 0;
			if(g.jetAttacks != uwBefore && iFirst < 0) {
				iFirst = f;
				if(pDwell[d] != GROUND_NEVER) CHECK(g.landFrames >= pDwell[d]);
			}
		}
		if(pDwell[d] == GROUND_NEVER) CHECK(iFirst < 0 && iBombs == 0);
		else CHECK(iFirst >= 0 && iFirst < pDwell[d] + 40);
	}
}

// The take-off grace holds over the sea too: no attack run for JET_TAKEOFF_GRACE frames after lifting off a pad
static void testW2JetTakeoffGrace(void) {
	newWorld2(0);
	logicSetDifficulty(&g, 4);
	noBoats();
	g.ghost = 1;
	g.trip = 6;
	int16_t wX = g_pIslands[1].padX;
	flyAt(wX, GROUND_Y);
	g.chop.landed = 1;
	for(int f = 0; f < 400; ++f) {
		g.chop.x = wX; g.chop.y = GROUND_Y; g.chop.vx = g.chop.vy = 0;
		g.spawnTimer = 60000;
		step(NONE, 1);
	}
	g.chop.landed = 0;
	int iAt = -1;
	for(int f = 0; f < 300 && iAt < 0; ++f) {
		g.chop.x = wX; g.chop.y = 120; g.chop.landed = 0; g.chop.vx = g.chop.vy = 0;
		g.spawnTimer = 60000;
		g.pJets[1].active = 0; g.pJets[1].cool = 255;
		if(!g.pJets[0].tell) aimJet(0, 1);
		uint16_t uwBefore = g.jetAttacks;
		step(NONE, 1);
		if(g.jetAttacks != uwBefore) iAt = f;
	}
	CHECK(iAt >= JET_TAKEOFF_GRACE - 2 && iAt <= JET_TAKEOFF_GRACE + 4);
}

// The cannon shoots a jet down over the water, and it pays 100
static void testW2JetCanBeShotDown(void) {
	newWorld2(0);
	logicSetDifficulty(&g, 3);
	noBoats();
	g.trip = 1;
	flyAt(1500, 120);
	g.pJets[0] = (tJet){.x = 1440, .y = 114, .dir = 1, .active = 1, .hp = 1, .cool = 200};
	uint32_t uwScore = g.score;
	step((tInput){.fire = 1}, 3);
	CHECK(!g.pJets[0].active);
	CHECK(g.chop.alive);
	CHECK(g.score == uwScore + 100);
}

// One attack at a time across jets and boats (CONTENT-PLAN R3, SPEC 6): a boat's tell holds a jet's off, a jet's run holds a
// boat's off, for BOAT_ATTACK_GAP frames
static void testW2JetsAndBoatsShareTheAttackToken(void) {
	// a boat's tell started: no jet run for the gap
	newWorld2(1);
	logicSetDifficulty(&g, 4);
	noBoats();
	g.ghost = 1;
	g.trip = 6;
	flyAt(1500, 120);
	g.atkWait = BOAT_ATTACK_GAP;
	g.atkLast = 1;
	for(int f = 0; f < BOAT_ATTACK_GAP - 4; ++f) {
		g.chop.x = 1500; g.chop.y = 120; g.chop.vx = g.chop.vy = 0;
		g.spawnTimer = 60000;
		g.pJets[1].active = 0; g.pJets[1].cool = 255;
		if(!g.pJets[0].tell) aimJet(0, 1);
		step(NONE, 1);
		CHECK(g.jetAttacks == 0);
	}
	int iRun = 0;
	for(int f = 0; f < 30 && !iRun; ++f) {
		g.chop.x = 1500; g.chop.y = 120; g.chop.vx = g.chop.vy = 0;
		g.spawnTimer = 60000;
		g.pJets[1].active = 0; g.pJets[1].cool = 255;
		if(!g.pJets[0].tell) aimJet(0, 1);
		step(NONE, 1);
		iRun = g.jetAttacks;
	}
	CHECK(iRun == 1);
	// a jet's run started: no boat tell for the gap
	newWorld2(1);
	logicSetDifficulty(&g, 4);
	tBoat *b = gunboatNear(2400, GROUND_Y - 30, -130);
	g.ghost = 1;
	g.trip = 6;
	b->cool = 30;
	int iStart = -1;
	for(int f = 0; f < 400; ++f) {
		g.chop.x = 2400; g.chop.y = GROUND_Y - 30; g.chop.vx = g.chop.vy = 0;
		g.spawnTimer = 60000;
		g.pJets[1].active = 0; g.pJets[1].cool = 255;
		if(iStart < 0 && !g.pJets[0].tell) aimJet(0, 1);
		uint16_t uwBefore = g.jetAttacks;
		if(iStart >= 0 && f == iStart + 1) b->cool = 0;
		step(NONE, 1);
		if(g.jetAttacks != uwBefore && iStart < 0) iStart = f;
		if(iStart >= 0 && f > iStart && f < iStart + BOAT_ATTACK_GAP - 2) CHECK(!b->tell);
	}
	CHECK(iStart >= 0);
}

// A jet in world 2 is drawn from world 2's own BOB (the set's copy of the jet: world 1's BOBs are not in the arena), blinking in its tell
static void testW2JetIsDrawnFromTheSet(void) {
	newWorld2(0);
	logicSetDifficulty(&g, 3);
	g.trip = 1;
	flyAt(1500, 120);
	g.cam = 1340;
	tDraw d[DRAW_MAX];
	int iFlash = 0, iPlain = 0;
	for(uint8_t tell = 1; tell <= 16; ++tell) {
		g.pJets[0] = (tJet){.x = 1400, .y = 112, .dir = -1, .active = 1, .hp = 1, .tell = tell};
		uint8_t n = logicDraw(&g, d);
		for(uint8_t i = 0; i < n; ++i) {
			CHECK(d[i].bob != BOB_JET);
			if(d[i].bob != BOB_W2_JET) continue;
			if(d[i].frame == JET_FLASH_L) ++iFlash;
			else if(d[i].frame == JET_L) ++iPlain;
		}
	}
	CHECK(iFlash >= 6 && iPlain >= 6);
	for(uint8_t i = 0; i < BOB_TYPES; ++i) CHECK(g_pBobW[i] && g_pBobH[i] && g_pBobFrames[i]);
	CHECK(g_pBobW[BOB_W2_JET] == g_pBobW[BOB_JET] && g_pBobH[BOB_W2_JET] == g_pBobH[BOB_JET] && g_pBobFrames[BOB_W2_JET] == g_pBobFrames[BOB_JET]);
}

// ---- radar sites and the jet zone rule (world 2 slice 2, slice A; SPEC 11 "World 2, slice 2") ----
// The sites, as the spec writes them: mission 4 none; mission 5 x 1608 (second island) and 3416 (fourth); mission 6 those and 440 (first)
static const int16_t s_pRadarX[3][3] = {{0, 0, 0}, {1608, 3416, 0}, {440, 1608, 3416}};
static const uint8_t s_pRadarN[3] = {0, 2, 3};

static void hitRadar(uint8_t i, uint8_t ubShots) {
	for(uint8_t k = 0; k < ubShots; ++k) {
		g.pShots[0] = (tShot){.x = g.pRadars[i].x, .y = FEET_Y - 20, .active = 1, .kind = SHOT_CANNON, .life = 36};
		step(NONE, 1);
	}
}

// Missions 5 and 6 have their sites on land (an island, not the base island), the same at every skill; mission 4, world 1
// and a fresh game have none
static void testW2RadarSitesArePlaced(void) {
	for(uint8_t m = 0; m < 3; ++m) {
		for(uint8_t d = 0; d <= DIFFICULTY_MAX; ++d) {
			newWorld2(m);
			logicSetDifficulty(&g, d);
			CHECK(g.radarCount == s_pRadarN[m]);
			for(uint8_t i = 0; i < g.radarCount; ++i) {
				const tRadar *r = &g.pRadars[i];
				CHECK(r->alive && r->hp == RADAR_HP);
				uint8_t isFound = 0;
				for(uint8_t k = 0; k < s_pRadarN[m]; ++k) isFound |= r->x == s_pRadarX[m][k];
				CHECK(isFound);
				CHECK(!logicOnWater((int16_t)(r->x - RADAR_W_PX / 2)) && !logicOnWater((int16_t)(r->x + RADAR_W_PX / 2 - 1)));
				uint8_t iIsland = 0xFF;
				for(uint8_t k = 0; k < W2_ISLANDS; ++k) if(r->x >= g_pIslands[k].x0 && r->x < g_pIslands[k].x1) iIsland = k;
				CHECK(iIsland < W2_BASE_ISLAND);
				CHECK(logicPadAt(&g, (int16_t)(r->x - RADAR_W_PX / 2)) == 0xFF && logicPadAt(&g, (int16_t)(r->x + RADAR_W_PX / 2)) == 0xFF);   // not on a pad
			}
		}
	}
	newGame();
	CHECK(g.radarCount == 0);
	logicStartAt(&g, 1, 1);
	CHECK(g.radarCount == 2);
	logicStartMission(&g, 0);
	CHECK(g.radarCount == 0);   // (mission 4 again: the sites do not carry over)
	newGame();
	logicStartAt(&g, 0, 2);
	CHECK(g.radarCount == 0);
}

// The zone is 1200 px around a live site, cut at the base's approach (W2_JET_SAFE_X); edges exact; mission 4 and world 1 are everywhere
static void testW2RadarZonesAndTheRule(void) {
	newWorld2(1);
	tZone z[RADAR_MAX];
	CHECK(logicRadarZones(&g, z, 0) == 2);
	CHECK(z[0].lo == 1608 - RADAR_ZONE_HALF && z[0].hi == 1608 + RADAR_ZONE_HALF);
	CHECK(z[1].lo == 3416 - RADAR_ZONE_HALF && z[1].hi == W2_JET_SAFE_X);
	CHECK(!logicJetZone(&g, z[0].lo - 1) && logicJetZone(&g, z[0].lo) && logicJetZone(&g, z[0].hi) && !logicJetZone(&g, z[0].hi + 1));
	CHECK(!logicJetZone(&g, z[1].lo - 1) && logicJetZone(&g, z[1].lo) && logicJetZone(&g, W2_JET_SAFE_X) && !logicJetZone(&g, W2_JET_SAFE_X + 1));
	CHECK(!logicJetZone(&g, 500) && !logicJetZone(&g, 2500) && !logicJetZone(&g, W2_START_X));
	CHECK(logicJetZone(&g, 1608) && logicJetZone(&g, 3416));
	// a site down: its zone is gone, the other's stays; ubAll still lists both (the card)
	g.pRadars[0].alive = 0;
	CHECK(logicRadarZones(&g, z, 0) == 1 && z[0].lo == 3416 - RADAR_ZONE_HALF);
	CHECK(logicRadarZones(&g, z, 1) == 2);
	CHECK(!logicJetZone(&g, 1608) && logicJetZone(&g, 3416));
	g.pRadars[1].alive = 0;
	CHECK(logicRadarZones(&g, z, 0) == 0);
	for(int16_t x = 0; x < W2_W; x += 8) CHECK(!logicJetZone(&g, x));   // every site down: no jet start anywhere
	// mission 6: three zones; the first cut at the left edge of the map
	newWorld2(2);
	CHECK(logicRadarZones(&g, z, 0) == 3);
	uint8_t isLeft = 0;
	for(uint8_t i = 0; i < 3; ++i) isLeft |= z[i].lo == 0 && z[i].hi == 440 + RADAR_ZONE_HALF;
	CHECK(isLeft);
	CHECK(logicJetZone(&g, 0) && logicJetZone(&g, 1040) && !logicJetZone(&g, 2500));
	// mission 4 and world 1: no sites, so a jet may start anywhere (today's rule)
	newWorld2(0);
	CHECK(logicRadarZones(&g, z, 1) == 0);
	for(int16_t x = 0; x < W2_W; x += 8) CHECK(logicJetZone(&g, x));
	newGame();
	for(int16_t x = 0; x < WORLD_W; x += 8) CHECK(logicJetZone(&g, x));
}

// In the game: a jet starts only while the chopper is inside a live zone. Outside every zone nothing comes for as long as it
// stays (the budget is not spent), inside one the jets come as before
static void testW2JetsStartOnlyInsideALiveZone(void) {
	static const int16_t s_pOut[] = {500, 800, 2400, 2500, 2700};
	static const int16_t s_pIn[] = {1100, 1608, 2100, 2900, 3416, 3700};
	for(uint8_t m = 1; m < 3; ++m) {
		for(uint8_t k = 0; k < 5; ++k) {
			if(m == 2 && s_pOut[k] < 1000) continue;   // (mission 6: the first island's zone is 0-1040)
			newWorld2(m);
			logicSetDifficulty(&g, 5);
			noBoats();
			g.ghost = 1;
			g.trip = 6;
			flyAt(s_pOut[k], 120);
			uint8_t iJets = 0;
			for(int f = 0; f < 2500; ++f) {
				g.chop.x = s_pOut[k]; g.chop.y = 120; g.chop.vx = g.chop.vy = 0;
				g.spawnTimer = 60000;
				step(NONE, 1);
				iJets += (g.events & EV_JET) != 0;
			}
			if(iJets) printf("mission %d: %d jets at x %d outside every zone\n", m + 4, iJets, s_pOut[k]);
			CHECK(iJets == 0);
			CHECK(g.jetsTrip == 0);   // the budget is untouched
		}
		for(uint8_t k = 0; k < 6; ++k) {
			newWorld2(m);
			logicSetDifficulty(&g, 5);
			noBoats();
			g.ghost = 1;
			g.trip = 6;
			flyAt(s_pIn[k], 120);
			uint8_t iJets = 0;
			for(int f = 0; f < 2500; ++f) {
				g.chop.x = s_pIn[k]; g.chop.y = 120; g.chop.vx = g.chop.vy = 0;
				g.spawnTimer = 60000;
				step(NONE, 1);
				iJets += (g.events & EV_JET) != 0;
			}
			if(!iJets) printf("mission %d: no jet at x %d inside a zone\n", m + 4, s_pIn[k]);
			CHECK(iJets >= 1);
		}
	}
}

// The first-jet delay counts only while the chopper is inside a zone (as it already did for the base's approach): 500 frames
// of a zone, not of the map
static void testW2JetDelayCountsInsideZonesOnly(void) {
	newWorld2(1);
	logicSetDifficulty(&g, 5);
	noBoats();
	g.ghost = 1;
	g.trip = 6;
	flyAt(2500, 120);   // the gap between the zones: 800 frames, no countdown
	for(int f = 0; f < 800; ++f) {
		g.chop.x = 2500; g.chop.y = 120; g.chop.vx = g.chop.vy = 0;
		g.spawnTimer = 60000;
		step(NONE, 1);
	}
	CHECK(g.pJets[0].cool >= W2_JET_FIRST - 1);
	int iFirst = -1;
	for(int f = 0; f < 1200 && iFirst < 0; ++f) {
		g.chop.x = 1608; g.chop.y = 120; g.chop.vx = g.chop.vy = 0;
		g.spawnTimer = 60000;
		step(NONE, 1);
		if(g.events & EV_JET) iFirst = f;
	}
	CHECK(iFirst >= W2_JET_FIRST - 5 && iFirst <= W2_JET_FIRST + 5);
}

// A radar site takes 4 cannon hits and pays 150, a bomb within 30 px kills it; its zone goes with it and the jets of its
// stretch end for the mission
static void testW2RadarHitsBombsAndTheZoneGoes(void) {
	newWorld2(1);
	logicSetDifficulty(&g, 3);
	noBoats();
	g.ghost = 1;
	flyAt(1000, 100);
	uint32_t ulScore = g.score;
	hitRadar(0, 3);
	CHECK(g.pRadars[0].alive && g.pRadars[1].alive);
	CHECK(g.pRadars[0].hp == RADAR_HP - 3 && g.score == ulScore);
	// a shot beside it, or above it, does nothing
	g.pShots[0] = (tShot){.x = (int16_t)(g.pRadars[0].x + RADAR_W_PX / 2 + 6), .y = FEET_Y - 20, .active = 1, .kind = SHOT_CANNON, .life = 36};
	step(NONE, 1);
	g.pShots[0] = (tShot){.x = g.pRadars[0].x, .y = FEET_Y - RADAR_H_PX - 8, .active = 1, .kind = SHOT_CANNON, .life = 36};
	step(NONE, 1);
	CHECK(g.pRadars[0].alive && g.pRadars[0].hp == RADAR_HP - 3);
	hitRadar(0, 1);
	CHECK(!g.pRadars[0].alive && g.pRadars[0].hp == 0);
	CHECK(g.score == ulScore + RADAR_SCORE);
	uint8_t iBooms = 0;
	for(uint8_t i = 0; i < BOOM_MAX; ++i) iBooms += g.pBooms[i].active;
	CHECK(iBooms >= 1);
	// a bomb: 40 px off does nothing, 20 px off kills at once
	int16_t wX1 = g.pRadars[1].x;
	g.pShots[0] = (tShot){.x = (int16_t)(wX1 + 40), .y = FEET_Y - 8, .vy = 48, .active = 1, .kind = SHOT_BOMB, .life = 60};
	step(NONE, 2);
	CHECK(g.pRadars[1].alive && g.pRadars[1].hp == RADAR_HP);
	g.pShots[0] = (tShot){.x = (int16_t)(wX1 + 20), .y = FEET_Y - 8, .vy = 48, .active = 1, .kind = SHOT_BOMB, .life = 60};
	step(NONE, 2);
	CHECK(!g.pRadars[1].alive);
	CHECK(g.score == ulScore + 2 * RADAR_SCORE);
	// with both down no jet starts anywhere in the mission
	g.trip = 6;
	logicSetDifficulty(&g, 5);
	for(int16_t x = 1000; x <= 3600; x += 800) {
		g.pJets[0].active = g.pJets[1].active = 0;
		flyAt(x, 120);
		for(int f = 0; f < 1500; ++f) {
			g.chop.x = x; g.chop.y = 120; g.chop.vx = g.chop.vy = 0;
			g.spawnTimer = 60000;
			step(NONE, 1);
			CHECK(!(g.events & EV_JET));
		}
	}
}

// Shooting one site ends the jets of its stretch only: the other site's zone still brings them
static void testW2OneSiteDownLeavesTheOthersZone(void) {
	newWorld2(1);
	logicSetDifficulty(&g, 5);
	noBoats();
	g.ghost = 1;
	g.trip = 6;
	uint8_t iWest = g.pRadars[0].x == 1608 ? 0 : 1;
	g.pRadars[iWest].alive = 0;
	flyAt(1608, 120);
	uint8_t iJets = 0;
	for(int f = 0; f < 2000; ++f) {
		g.chop.x = 1608; g.chop.y = 120; g.chop.vx = g.chop.vy = 0;
		g.spawnTimer = 60000;
		step(NONE, 1);
		iJets += (g.events & EV_JET) != 0;
	}
	CHECK(iJets == 0);
	newWorld2(1);
	logicSetDifficulty(&g, 5);
	noBoats();
	g.ghost = 1;
	g.trip = 6;
	g.pRadars[iWest].alive = 0;
	flyAt(3416, 120);
	iJets = 0;
	for(int f = 0; f < 2000; ++f) {
		g.chop.x = 3416; g.chop.y = 120; g.chop.vx = g.chop.vy = 0;
		g.spawnTimer = 60000;
		step(NONE, 1);
		iJets += (g.events & EV_JET) != 0;
	}
	CHECK(iJets >= 1);
}

// A radar never shoots: nothing is fired beside one at the hardest skill (no boats, no jets), the chopper not in ghost mode
static void testW2RadarNeverShoots(void) {
	newWorld2(2);
	logicSetDifficulty(&g, 5);
	noBoats();
	g.jetsTrip = 255;
	flyAt(g.pRadars[0].x, GROUND_Y - 30);
	CHECK(g.radarCount == 3);
	for(int f = 0; f < 600; ++f) {
		g.chop.x = (int16_t)(g.pRadars[0].x + 60); g.chop.y = GROUND_Y - 30; g.chop.vx = g.chop.vy = 0;
		g.spawnTimer = 60000;
		step(NONE, 1);
		for(uint8_t i = 0; i < ESHOT_MAX; ++i) CHECK(!g.pEShots[i].active);
	}
	CHECK(g.chop.alive);
}

// Mission 4 and world 1 keep their jets wherever the chopper flies (no sites there)
static void testW2Mission4KeepsItsJetsEverywhere(void) {
	newWorld2(0);
	logicSetDifficulty(&g, 5);
	noBoats();
	g.ghost = 1;
	g.trip = 0;
	flyAt(2500, 120);
	uint8_t iJets = 0;
	for(int f = 0; f < 1500; ++f) {
		g.chop.x = 2500; g.chop.y = 120; g.chop.vx = g.chop.vy = 0;
		g.spawnTimer = 60000;
		step(NONE, 1);
		iJets += (g.events & EV_JET) != 0;
	}
	CHECK(iJets >= 1);
	newGame();
	logicSetDifficulty(&g, 5);
	g.trip = 1;
	flyAt(1200, 120);
	iJets = 0;
	for(int f = 0; f < 800; ++f) {
		g.chop.x = 1200; g.chop.y = 120; g.chop.vx = g.chop.vy = 0;
		g.spawnTimer = 60000;
		step(NONE, 1);
		iJets += (g.events & EV_JET) != 0;
	}
	CHECK(iJets >= 1);
}

// The site is drawn from the set (the dish turning through all 4 frames) while it stands and is in view, never in mission 4;
// the card's strip map carries each zone as a red line and each site as a tick
static void testW2RadarIsDrawnAndMapped(void) {
	tDraw d[DRAW_MAX];
	newWorld2(1);
	flyAt(1608, 120);
	uint8_t pSeen[4] = {0, 0, 0, 0};
	int iAt = 0;
	for(int f = 0; f < 80; ++f) {
		uint8_t n = logicDraw(&g, d);
		for(uint8_t i = 0; i < n; ++i) {
			if(d[i].bob != BOB_RADAR) continue;
			CHECK(d[i].frame < 4 && d[i].y == FEET_Y - 38);
			pSeen[d[i].frame] = 1;
			iAt = d[i].x + 16 + g.cam;
		}
		g.chop.x = 1608; g.chop.y = 120; g.chop.vx = g.chop.vy = 0;
		step(NONE, 1);
	}
	CHECK(pSeen[0] && pSeen[1] && pSeen[2] && pSeen[3]);
	CHECK(iAt == 1608);
	uint8_t iA = g.pRadars[0].x == 1608 ? 0 : 1;
	g.pRadars[iA].alive = 0;
	uint8_t n = logicDraw(&g, d);
	for(uint8_t i = 0; i < n; ++i) CHECK(d[i].bob != BOB_RADAR);
	newWorld2(0);
	flyAt(1608, 120);
	n = logicDraw(&g, d);
	for(uint8_t i = 0; i < n; ++i) CHECK(d[i].bob != BOB_RADAR);
	CHECK(g_pBobW[BOB_RADAR] == 33 && g_pBobH[BOB_RADAR] == 39 && g_pBobFrames[BOB_RADAR] == 4);
	// the strip map
	for(uint8_t m = 0; m < 3; ++m) {
		newWorld2(m);
		tRouteMark mk[ROUTE_MARKS + 8];
		uint8_t nm = logicRouteMap(&g, mk);
		CHECK(nm <= ROUTE_MARKS);
		uint8_t iZones = 0, iTicks = 0;
		for(uint8_t i = 0; i < nm; ++i) {
			iZones += mk[i].kind == ROUTE_ZONE;
			iTicks += mk[i].kind == ROUTE_RADAR;
			if(mk[i].kind == ROUTE_ZONE) CHECK(mk[i].w >= 1 && mk[i].x + mk[i].w <= 256);
		}
		CHECK(iZones == s_pRadarN[m] && iTicks == s_pRadarN[m]);
	}
	newGame();
	tRouteMark mk[ROUTE_MARKS + 8];
	uint8_t nm = logicRouteMap(&g, mk);
	for(uint8_t i = 0; i < nm; ++i) CHECK(mk[i].kind != ROUTE_ZONE && mk[i].kind != ROUTE_RADAR);
}

// The card's map is made for every mission of the world at its load, from the sites table and the zone of a site (main.c routeOverlays),
// not from a game in that mission: both must be what the game itself places and uses
static void testW2RadarSitesApiMatchesTheGame(void) {
	int16_t pX[RADAR_MAX];
	for(uint8_t m = 0; m < 3; ++m) {
		newWorld2(m);
		CHECK(logicRadarSites(m, pX) == g.radarCount);
		tZone z[RADAR_MAX];
		uint8_t nz = logicRadarZones(&g, z, 1);
		CHECK(nz == g.radarCount);
		for(uint8_t i = 0; i < g.radarCount; ++i) {
			CHECK(pX[i] == g.pRadars[i].x);
			tZone zo;
			logicRadarZoneOf(pX[i], &zo);
			CHECK(zo.lo == z[i].lo && zo.hi == z[i].hi);
			CHECK(zo.hi - zo.lo <= 2 * RADAR_ZONE_HALF && zo.lo >= 0 && zo.hi <= W2_JET_SAFE_X);
		}
	}
	CHECK(logicRadarSites(3, pX) == 0 && logicRadarSites(200, pX) == 0);
	// the map's x for a world x (x / 18, the card's strip map)
	CHECK(logicRouteX2(0) == 0 && logicRouteX2(1800) == 100 && logicRouteX2(W2_JET_SAFE_X) == 211);
}

// ---- wind gusts and the windsocks (world 2 slice B; SPEC 11 "World 2, slice 2") ----
// Hover at x in mission m (0-2 = missions 4-6) on a skill, nothing near, ghost on; `frames` of it; calls fn(frame) after every step
static void gustWorld(uint8_t m, uint8_t skill, int16_t x) {
	newWorld2(m);
	logicSetDifficulty(&g, skill);
	noBoats();
	g.jetsTrip = 255;   // (no jet or boat in the way)
	g.ghost = 1;
	flyAt(x, 120);
}

static int gustPos(uint8_t m, uint8_t skill, int16_t x, int frames, int *pFirstTell, int *pTells, int *pStarts, int *pMaxPush) {
	gustWorld(m, skill, x);
	int16_t wX0 = g.chop.x;
	*pFirstTell = -1; *pTells = 0; *pStarts = 0; *pMaxPush = 0;
	int iTellAt = -1, iStartAt = -1, iEndAt = -1, iLastEnd = -1;
	for(int f = 0; f < frames; ++f) {
		g.chop.vx = g.chop.vy = 0; g.chop.y = 120; g.spawnTimer = 60000;
		step(NONE, 1);
		CHECK(!(g.gustTell && g.gustOn));   // one at a time
		if(g.events3 & EV3_GUSTTELL) {
			if(*pFirstTell < 0) *pFirstTell = f;
			if(iLastEnd >= 0) CHECK(f - iLastEnd >= GUST_GAP_MIN - 2 && f - iLastEnd <= GUST_GAP_MAX + 2);
			iTellAt = f; ++*pTells;
		}
		if(g.events3 & EV3_GUST) { CHECK(f - iTellAt == GUST_TELL_FRAMES); iStartAt = f; ++*pStarts; wX0 = g.chop.x; }
		if(iStartAt >= 0 && iEndAt < iStartAt && !g.gustOn && !g.gustTell) { iEndAt = f; iLastEnd = f; if(g.chop.x - wX0 > *pMaxPush) *pMaxPush = g.chop.x - wX0; }
	}
	return g.chop.x - 2500;
}

static void testW2GustsByMissionAndSkill(void) {
	for(uint8_t m = 0; m < 3; ++m) {
		for(uint8_t d = 0; d <= DIFFICULTY_MAX; ++d) {
			newWorld2(m);
			logicSetDifficulty(&g, d);
			uint8_t isOn = m >= 1 && d != 1;
			CHECK(logicGustsOn(&g) == isOn);
		}
	}
	newGame();
	CHECK(!logicGustsOn(&g));
}

// The first tell not before 18 s (and within the gap's spread of it), the push 50 frames after, 150 frames long, then 12-20 s of calm
static void testW2GustTimingAndTell(void) {
	int iFirst, iTells, iStarts, iMax;
	gustPos(1, 4, 2500, 7000, &iFirst, &iTells, &iStarts, &iMax);
	CHECK(iFirst >= GUST_FIRST - 3 && iFirst <= GUST_FIRST + (GUST_GAP_MAX - GUST_GAP_MIN) + 3);
	CHECK(iTells >= 4 && iStarts >= iTells - 1);
	// the push of one gust is bounded: 150 frames at 0.25 px, 37 or 38 px, toward +x
	CHECK(iMax >= 36 && iMax <= 38);
}

static void testW2GustPushIsSidewaysAndBoundedAndTellDoesNotMove(void) {
	gustWorld(1, 4, 2500);
	int16_t wX = g.chop.x;
	int iTell = 0, iPush = 0;
	for(int f = 0; f < 3000; ++f) {
		g.chop.vx = g.chop.vy = 0; g.chop.y = 120; g.spawnTimer = 60000;
		int16_t wBefore = g.chop.x;
		step(NONE, 1);
		if(g.gustTell) { ++iTell; CHECK(g.chop.x == wBefore); }
		if(g.gustOn && !(g.events3 & EV3_GUST)) { ++iPush; CHECK(g.chop.x - wBefore >= 0 && g.chop.x - wBefore <= 1); }
		CHECK(g.chop.y == 120);   // sideways only
		if(f > 2800 && !g.gustOn) break;
	}
	CHECK(iTell >= GUST_TELL_FRAMES - 2 && iPush >= GUST_FRAMES - 3);
	CHECK(g.chop.x > wX);
}

static void testW2GustNeverLandedOrJustAfterTakeoff(void) {
	// landed on the base pad: nothing for 4000 frames
	newWorld2(1);
	logicSetDifficulty(&g, 5);
	noBoats();
	g.gustTimer = 0;
	int iEv = 0;
	int16_t wX = g.chop.x;
	for(int f = 0; f < 4000; ++f) { g.spawnTimer = 60000; step(NONE, 1); iEv += g.events3 != 0; }
	CHECK(iEv == 0 && g.chop.x == wX && g.chop.landed);
	// just after take-off: the tell waits until the grace is over
	gustWorld(1, 5, 2500);
	g.gustTimer = 0;
	g.airGrace = JET_TAKEOFF_GRACE;
	int iAt = -1;
	for(int f = 0; f < 400 && iAt < 0; ++f) {
		g.chop.vx = g.chop.vy = 0; g.chop.y = 120; g.spawnTimer = 60000;
		step(NONE, 1);
		if(g.events3 & EV3_GUSTTELL) { iAt = f; CHECK(g.airGrace == 0); }
	}
	CHECK(iAt >= JET_TAKEOFF_GRACE - 3);
	// landing cancels the tell and the push
	for(int phase = 0; phase < 2; ++phase) {
		gustWorld(1, 5, 2500);
		g.gustTimer = 0;
		for(int f = 0; f < 400 && (phase ? !g.gustOn : !g.gustTell); ++f) { g.chop.vx = g.chop.vy = 0; g.chop.y = 120; step(NONE, 1); }
		CHECK(phase ? g.gustOn : g.gustTell);
		g.chop.landed = 1; g.chop.y = GROUND_Y;
		int16_t wL = g.chop.x;
		step(NONE, 3);
		CHECK(!g.gustTell && !g.gustOn && g.chop.x == wL);
	}
}

static void testW2GustSkillsAndBarracks(void) {
	int iFirst, iTells, iStarts, iMax;
	// skill 1: never
	gustPos(1, 1, 2500, 4000, &iFirst, &iTells, &iStarts, &iMax);
	CHECK(iTells == 0);
	// skill 2-3: not within 100 px of a barracks (x 1232), whatever the wait; 4-5 there too
	for(uint8_t d = 2; d <= 5; ++d) {
		int iT = 0;
		gustWorld(1, d, 1232 + 60);
		for(int f = 0; f < 3000; ++f) { g.chop.vx = g.chop.vy = 0; g.chop.y = 120; g.spawnTimer = 60000; step(NONE, 1); iT += (g.events3 & EV3_GUSTTELL) != 0; }
		if(d <= 3) CHECK(iT == 0); else CHECK(iT >= 1);
	}
	// a push in progress is held while it is near a barracks (skill 3)
	gustWorld(1, 3, 2500);
	for(int f = 0; f < 3000 && !g.gustOn; ++f) { g.chop.vx = g.chop.vy = 0; g.chop.y = 120; step(NONE, 1); }
	CHECK(g.gustOn);
	g.chop.x = 1232 + 60; g.chop.fx = 0;
	int16_t wX = g.chop.x;
	step(NONE, 60);
	CHECK(g.chop.x == wX);
}

static void testW2Mission4AndWorld1HaveNoWind(void) {
	int iFirst, iTells, iStarts, iMax;
	gustPos(0, 5, 2500, 3000, &iFirst, &iTells, &iStarts, &iMax);
	CHECK(iTells == 0 && g.chop.x == 2500);
	newGame();
	logicSetDifficulty(&g, 5);
	g.ghost = 1;
	flyAt(1500, 120);
	for(int f = 0; f < 2000; ++f) { g.chop.vx = g.chop.vy = 0; g.chop.y = 120; g.spawnTimer = 60000; step(NONE, 1); CHECK(!g.events3); }
	CHECK(g.chop.x == 1500);
}

// The windsock stands beside each island's pad in missions 5-6 only, limp, whipping in the tell, full in the push
static void testW2SockIsDrawnAndWhips(void) {
	tDraw d[DRAW_MAX];
	CHECK(g_pBobW[BOB_SOCK] == 10 && g_pBobH[BOB_SOCK] == 20 && g_pBobFrames[BOB_SOCK] == 2);
	gustWorld(1, 4, 1392);
	g.cam = (int16_t)(1392 - 160);
	uint8_t pSeen[2] = {0, 0};
	for(int f = 0; f < 40; ++f) {
		uint8_t n = logicDraw(&g, d);
		for(uint8_t i = 0; i < n; ++i) {
			if(d[i].bob != BOB_SOCK) continue;
			CHECK(d[i].y == FEET_Y - 19 && d[i].x + g.cam == 1392 + SOCK_DX - 5 && d[i].frame == SOCK_LIMP);
			pSeen[d[i].frame] = 1;
		}
		step(NONE, 1);
		g.chop.vx = g.chop.vy = 0;
		g.cam = (int16_t)(1392 - 160);
	}
	CHECK(pSeen[SOCK_LIMP] && !pSeen[SOCK_FULL]);
	g.gustTell = GUST_TELL_FRAMES; g.gustOn = 0;
	uint8_t pTell[2] = {0, 0};
	for(int f = 0; f < 16; ++f) {
		g.gustTell = GUST_TELL_FRAMES;
		uint8_t n = logicDraw(&g, d);
		for(uint8_t i = 0; i < n; ++i) if(d[i].bob == BOB_SOCK) pTell[d[i].frame] = 1;
		step(NONE, 1);
		g.cam = (int16_t)(1392 - 160);
	}
	CHECK(pTell[SOCK_LIMP] && pTell[SOCK_FULL]);
	g.gustTell = 0; g.gustOn = GUST_FRAMES;
	for(int f = 0; f < 8; ++f) {
		g.gustOn = GUST_FRAMES;
		uint8_t n = logicDraw(&g, d);
		for(uint8_t i = 0; i < n; ++i) if(d[i].bob == BOB_SOCK) CHECK(d[i].frame == SOCK_FULL);
	}
	newWorld2(0);
	flyAt(1392, 120);
	g.cam = (int16_t)(1392 - 160);
	uint8_t n = logicDraw(&g, d);
	for(uint8_t i = 0; i < n; ++i) CHECK(d[i].bob != BOB_SOCK);
}

int main(void) {
	testRotorRateFollowsSpeedAndClimb();
	testRotorIdlesSlowAndSoftOnTheGround();
	testRotorSpinsUpAtTakeoff();
	testRotorSilentWhenDead();
	testRotorApprovedEmptyStaysExactly();
	testRotorLoadedAndDamagedNeverFasterThanEmpty();
	testRotorHeavyLiftoffIsCalm();
	testRotorDamagedWobbleIsGentle();
	testRotorHardwarePeriodIsOversampled();
	testRotorLoopIsDeepAndSoft();
	testRotorDescendSettlesLower();
	testRotorEmptyTakeoffIsNoBuzz();
	testSfxGapsWhileDamaged();
	testFirstHitDamages();
	testDamagedFliesSlower();
	testInvulnerableAfterHitAndBlinks();
	testSecondHitDestroys();
	testPadRepairs();
	testSmokeTrailOnlyWhenDamaged();
	testSmokeSparserWhenHovering();
	testSmokeGoesWithTheCrash();
	testInvincibleIsNeverDamaged();
	testJetTellShowsFlash();
	testTankShellSpeedAndGap();
	testNoMissileWithoutTell();
	testJetNeverFiresOffScreen();
	testTankQuotaStopsFarming();
	testGodmodeTablesEqualNormal();
	testHeadOnStrafeIsSnappy();
	testJetsGiveWarning();
	testTitleNeedsFire();
	testPreviewCard();
	testScoresTable();
	testHighScoreEntry();
	testHighScoreKeys();
	testHighScoreEsc();
	testNoEntryWhenInvincible();
	testAttractLoop();
	testDemoEndsOnFire();
	testStartsLandedAtBase();
	testTakeOffAndClimb();
	testAcceleratesToMaxSpeed();
	testTurnThroughFront();
	testFrontNeedsLongPush();
	testFire2Cycles();
	testVolleysNeverBomb();
	testTapsWithoutTheStickNeverBomb();
	testDeliberateBomb();
	testBombStanceTimesOut();
	testTurnShowsLeanFramesInOrder();
	testLeanFollowsTheTurn();
	testFrontHoverSways();
	testSidestepLeansIntoTheMove();
	testStrafeLeanIsEasedAndMatchesTheDirection();
	testDownFireFacesFrontThenBombs();
	testCannonFiresForwardAndRepeats();
	testCannonOpensBarracks();
	testBombOpensBarracks();
	testHostagesBoardLandedChopperUpToSixteen();
	testCabinFullStopsBoarding();
	testLandingOnPeopleKillsThem();
	testUnloadAtBaseSavesAndCountsTrip();
	testNoUnloadAwayFromBase();
	testShotDownLosesPassengers();
	testGameOverAfterLastChopper();
	testMissionClearWhenAllAccountedFor();
	testVictoryAfterLastMission();
	testDifficultySchedule();
	testTankShootsLowChopper();
	testNoShellWithoutTell();
	testTankIgnoresHighChopper();
	testTanksStayInEnemyGround();
	testBombKillsTank();
	testCannonKillsJet();
	testMineContactDestroysChopper();
	testCameraFollowsWithLead();
	testCameraIsSmooth();
	testRespawnPan();
	testSkyLayer();
	testScrollMaths();
	testScrollMathsAga();
	testFloorSpeeds();
	testFloorCopper();
	testSeaCopper();
	testGrad24();
	testWorld2Start();
	testTerrainFloor();
	testFloorGround();
	testSkyGradient();
	testSkyDither();
	testDrawListShowsBaseAtStart();
	testChopperShadow();
	testWeather();
	testRank();
	testDeterministic();
	testDifficultyDefaultsToNormalAndPersists();
	testInvincibleSurvivesHits();
	testInvincibleKeepsHiScore();
	testDifficultyScalesEnemies();
	testNoBombOnLandedChopper();
	testTakeoffGrace();
	testOneJetAtATime();
	testPairOffset();
	testWarningBeforeAttack();
	testJetFromBehindHasTime();
	testJetTripBudget();
	testJetTellTable();
	testSkillOneEasier();
	testHarderTanksFireSooner();
	testW2IslandsMatchTheMap();
	testW2PadWidthBySkill();
	testW2WaterIsASoftStop();
	testW2LandOnlyOnPads();
	testW2PadTolerance();
	testW2RescueFromAnIsland();
	testW2CannonOpensABarracks();
	testW2BoatsArePlaced();
	testW2BoatTellTable();
	testW2GunboatTellsBeforeItShoots();
	testW2GunboatFairness();
	testW2LandedChopperHasGrace();
	testW2OneBoatAttackAtATime();
	testW2PatrolArmedFromMission5();
	testW2PatrolUnarmedInMission4();
	testW2GunboatFiresFasterWithSkill();
	testW2PatrolTellsThenShoots();
	testW2PatrolFairness();
	testW2BoatsCanBeShot();
	testW2MissionsFollowWorld1();
	testW2CardBriefAndMap();
	testW2DrawsIslands();
	testW2JetsOnlyFromTheAllowedSkillAndMission();
	testW2JetsAppearOffScreenWithAWarningWithinTheBudget();
	testW2JetsNeverNearTheBaseOrInTheIntro();
	testW2LowSkillsStayGentle();
	testW2FirstTripJetComesLate();
	testW2BriefSaysJetsExactlyWhenTheyCanAppear();
	testW2JetTellsThenFires();
	testW2JetGroundGrace();
	testW2JetTakeoffGrace();
	testW2JetCanBeShotDown();
	testW2JetsAndBoatsShareTheAttackToken();
	testW2JetIsDrawnFromTheSet();
	testW2RadarSitesArePlaced();
	testW2RadarZonesAndTheRule();
	testW2JetsStartOnlyInsideALiveZone();
	testW2JetDelayCountsInsideZonesOnly();
	testW2RadarHitsBombsAndTheZoneGoes();
	testW2OneSiteDownLeavesTheOthersZone();
	testW2RadarNeverShoots();
	testW2Mission4KeepsItsJetsEverywhere();
	testW2RadarIsDrawnAndMapped();
	testW2RadarSitesApiMatchesTheGame();
	testW2GustsByMissionAndSkill();
	testW2GustTimingAndTell();
	testW2GustPushIsSidewaysAndBoundedAndTellDoesNotMove();
	testW2GustNeverLandedOrJustAfterTakeoff();
	testW2GustSkillsAndBarracks();
	testW2Mission4AndWorld1HaveNoWind();
	testW2SockIsDrawnAndWhips();
	if(s_failures) {
		printf("%d check(s) failed\n", s_failures);
		return 1;
	}
	printf("all logic tests passed\n");
	return 0;
}
