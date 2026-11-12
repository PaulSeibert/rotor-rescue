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
	for(uint8_t i = 1; i < SCORE_COUNT; ++i) CHECK(g.pScores[i].score < g.pScores[i - 1].score);
	CHECK(g.hiScore == g.pScores[0].score);
	CHECK(logicScoreRank(&g, g.pScores[0].score + 1) == 0);
	CHECK(logicScoreRank(&g, g.pScores[1].score) == 2);            // an equal score goes below
	CHECK(logicScoreRank(&g, 1) == SCORE_COUNT - 1 || g.pScores[SCORE_COUNT - 1].score >= 1);
	CHECK(logicScoreRank(&g, 0) == SCORE_COUNT);
	uint8_t pDisk[SCORES_BYTES];
	g.pScores[2].name[0] = 'Q';
	g.pScores[2].score = 3333;
	g.pScores[2].saved = 300;
	logicScoresPack(&g, pDisk);
	logicScoresDefault(&g);
	CHECK(g.pScores[2].score != 3333);
	CHECK(logicScoresUnpack(&g, pDisk));
	CHECK(g.pScores[2].score == 3333 && g.pScores[2].name[0] == 'Q' && g.pScores[2].saved == 300);
	uint8_t pBad[SCORES_BYTES];
	for(uint8_t i = 0; i < SCORES_BYTES; ++i) pBad[i] = pDisk[i];
	pBad[10] ^= 1;                                                  // a bit flipped
	logicScoresDefault(&g);
	CHECK(!logicScoresUnpack(&g, pBad) && g.pScores[2].score == 4000 - 1000);
	for(uint8_t i = 0; i < SCORES_BYTES; ++i) pBad[i] = 0;          // an empty file
	CHECK(!logicScoresUnpack(&g, pBad));
	CHECK(g.hiScore == g.pScores[0].score);
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
	CHECK(g.pScores[2].score == ulOld1 && g.pScores[4].score == 2000 && ulOld4 == 1000);   // shifted down
	step((tInput){.fire = 1}, 3);                 // fire held from the end screen: nothing
	CHECK(g.entryPos == 0);
	step(NONE, 1);
	step((tInput){.dy = -1}, 1);                  // A -> B
	step(NONE, 1);
	step((tInput){.dy = -1}, 1);                  // -> C
	step(NONE, 1);
	CHECK(g.pScores[1].name[0] == 'C');
	step((tInput){.fire = 1}, 1);                 // next letter (starts as C)
	step(NONE, 1);
	step((tInput){.dy = 1}, 1);                   // C -> B
	step(NONE, 1);
	step((tInput){.dx = -1}, 1);                  // back to the first
	step(NONE, 1);
	CHECK(g.entryPos == 0 && g.pScores[1].name[1] == 'B');
	step((tInput){.fire = 1}, 1);
	step(NONE, 1);
	step((tInput){.fire = 1}, 1);
	step(NONE, 1);
	CHECK(g.state == GS_ENTRY && g.entryPos == 2);
	step((tInput){.dy = -1}, 20);                 // held: repeats every 8 frames (3 steps)
	step(NONE, 1);
	CHECK(g.pScores[1].name[2] == 'E');
	step((tInput){.fire = 1}, 1);
	CHECK(g.state == GS_SCORES && g.scoresChanged && g.scoresNext == GS_TITLE);
	CHECK(g.pScores[1].name[0] == 'C' && g.pScores[1].name[1] == 'B' && g.pScores[1].name[2] == 'E');
	CHECK(g.pScores[1].score == 4500 && g.pScores[1].mission == 1 && g.hiScore == 5000);
	step(NONE, SCORES_FRAMES + 1);
	CHECK(g.state == GS_TITLE);
	// no input at all: after ENTRY_FRAMES the initials stand as they are
	endGameWith(9000);
	CHECK(g.state == GS_ENTRY && g.newRank == 0);
	step(NONE, ENTRY_FRAMES + 1);
	CHECK(g.state == GS_SCORES && g.pScores[0].name[0] == 'A' && g.hiScore == 9000);
	// too low: straight to the title
	endGameWith(10);
	CHECK(g.state == GS_TITLE);
}

static void testNoEntryWhenInvincible(void) {
	newGame();
	logicSetDifficulty(&g, DIFFICULTY_INVINCIBLE);
	g.score = 99999;
	g.state = GS_OVER;
	g.stateTimer = 1;
	step(NONE, 1);
	CHECK(g.state == GS_TITLE && g.pScores[0].score == 5000);
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
	CHECK(g.state == GS_SCORES && g.newRank == SCORE_COUNT);
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
	step((tInput){.dx = -1}, 14);
	g.chop.y = 120; g.chop.vy = 0;
	fr = logicChopperFrame(&g);
	CHECK((fr & CHOP_LEFT) && ((fr & 0x7F) == CHOP_LEAN2 || (fr & 0x7F) == CHOP_LEAN1));   // left
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

static void testVictoryAfterLastMission(void) {
	newGame();
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
	CHECK(iFast >= 80 && iFast <= 90);                    // 8-9 at full speed
	CHECK(logicRotorVolume(&g) > iHoverVol);              // and louder
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

int main(void) {
	testRotorRateFollowsSpeedAndClimb();
	testRotorIdlesSlowAndSoftOnTheGround();
	testRotorSpinsUpAtTakeoff();
	testRotorSilentWhenDead();
	testFirstHitDamages();
	testDamagedFliesSlower();
	testInvulnerableAfterHitAndBlinks();
	testSecondHitDestroys();
	testPadRepairs();
	testSmokeTrailOnlyWhenDamaged();
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
	testJetTellTable();
	testSkillOneEasier();
	testHarderTanksFireSooner();
	if(s_failures) {
		printf("%d check(s) failed\n", s_failures);
		return 1;
	}
	printf("all logic tests passed\n");
	return 0;
}
