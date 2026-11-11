// Host-side tests for src/logic.c (ROTOR RESCUE). Run with: agk unit
#include <stdio.h>
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
	CHECK((logicChopperFrame(&g) & 0x7F) == CHOP_TURN);
	step((tInput){.dx = 1}, TURN_STEP_FRAMES);
	CHECK(g.chop.face == FACE_RIGHT);
}

static void testFrontNeedsLongPush(void) {
	newGame();
	flyAt(2000, 120);
	g.chop.face = g.chop.faceTarget = FACE_FRONT;
	step((tInput){.dx = -1}, TURN_HOLD_FRAMES * 2);
	CHECK(g.chop.faceTarget == FACE_FRONT);   // strafing, still facing front
	step((tInput){.dx = -1}, TURN_HOLD_FRAMES * 2 + TURN_STEP_FRAMES + 1);
	CHECK(g.chop.face == FACE_LEFT);
}

static void testFire2Cycles(void) {
	newGame();
	flyAt(2000, 120);
	step((tInput){.fire2 = 1}, 1);
	step(NONE, TURN_STEP_FRAMES);
	CHECK(g.chop.face == FACE_FRONT);
	step((tInput){.fire2 = 1}, 1);
	step(NONE, TURN_STEP_FRAMES);
	CHECK(g.chop.face == FACE_RIGHT);   // on through to the other side
	step((tInput){.fire2 = 1}, 1);
	step(NONE, TURN_STEP_FRAMES);
	CHECK(g.chop.face == FACE_FRONT);
	step((tInput){.fire2 = 1}, 1);
	step(NONE, TURN_STEP_FRAMES);
	CHECK(g.chop.face == FACE_LEFT);
}

static void testDownFireFacesFrontThenBombs(void) {
	newGame();
	flyAt(2000, 120);
	step((tInput){.dy = 1, .fire = 1}, 1);
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
				ubNew += p->active && p->kind == ESHOT_SHELL && p->life == 100;
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
	step((tInput){.dy = 1, .fire = 1}, 1);
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
	// no missile while it's off screen, nor in its first frames on screen
	int firstShotSeen = -1;
	for(int f = 0; f < 200 && firstShotSeen < 0; ++f) {
		g.chop.x = 2400; g.chop.y = 120; g.chop.vx = g.chop.vy = 0;
		g.chop.y = (int16_t)(j->y + 8);            // in its line of fire
		step(NONE, 1);
		for(int s = 0; s < ESHOT_MAX; ++s)
			if(g.pEShots[s].active && g.pEShots[s].kind == ESHOT_MISSILE) firstShotSeen = j->seen;
		if(!g.chop.alive) break;
	}
	CHECK(firstShotSeen < 0 || firstShotSeen >= 18);
	printf("jets: first missile after %d frames on screen\n", firstShotSeen);
}

int main(void) {
	testHeadOnStrafeIsSnappy();
	testJetsGiveWarning();
	testTitleNeedsFire();
	testStartsLandedAtBase();
	testTakeOffAndClimb();
	testAcceleratesToMaxSpeed();
	testTurnThroughFront();
	testFrontNeedsLongPush();
	testFire2Cycles();
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
	testHarderTanksFireSooner();
	if(s_failures) {
		printf("%d check(s) failed\n", s_failures);
		return 1;
	}
	printf("all logic tests passed\n");
	return 0;
}
