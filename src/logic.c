// ROTOR RESCUE - game rules (portable C, tested on the host by agk unit).
// (c) 2026 Desert Palms Software. Game by Paul Ross Seibert.

#include "logic.h"

const uint8_t g_pBobW[BOB_TYPES] = {
	[BOB_HANGAR] = 64, [BOB_FENCE] = 16, [BOB_BARRACKS] = 48, [BOB_TANK] = 32,
	[BOB_HOSTAGE] = 16, [BOB_JET] = 32, [BOB_MINE] = 16, [BOB_SHOT] = 16,
	[BOB_BOMB] = 16, [BOB_SHELL] = 16, [BOB_BOOM] = 32, [BOB_FLAG] = 16,
	[BOB_SHADOW] = 32,
};
const uint8_t g_pBobH[BOB_TYPES] = {
	[BOB_HANGAR] = 40, [BOB_FENCE] = 32, [BOB_BARRACKS] = 28, [BOB_TANK] = 14,
	[BOB_HOSTAGE] = 12, [BOB_JET] = 12, [BOB_MINE] = 12, [BOB_SHOT] = 3,
	[BOB_BOMB] = 6, [BOB_SHELL] = 4, [BOB_BOOM] = 32, [BOB_FLAG] = 24,
	[BOB_SHADOW] = 4,
};
const uint8_t g_pBobFrames[BOB_TYPES] = {
	[BOB_HANGAR] = 1, [BOB_FENCE] = 1, [BOB_BARRACKS] = 4, [BOB_TANK] = 10,
	[BOB_HOSTAGE] = 8, [BOB_JET] = 2, [BOB_MINE] = 2, [BOB_SHOT] = 1,
	[BOB_BOMB] = 1, [BOB_SHELL] = 1, [BOB_BOOM] = 5, [BOB_FLAG] = 2,
	[BOB_SHADOW] = SHADOW_SIZES * 2,
};

// Barracks positions (centres), far to near
static const int16_t s_pBarracksX[BARRACKS_COUNT] = {420, 1080, 1700, 2240};

const char *const g_pMissionName[MISSION_COUNT] = {
	"DESERT DUSK", "MOUNTAIN NIGHT", "COASTAL DAWN"
};

// Sky: top colour, horizon colour per mission
static const uint16_t s_pSkyTop[MISSION_COUNT] = {0x214, 0x001, 0x237};
static const uint16_t s_pSkyLow[MISSION_COUNT] = {0xE75, 0x235, 0xFB8};

// PF2 band palettes ([0] transparent): distant mountains, near land
const uint16_t g_pMissionMtnPal[MISSION_COUNT][8] = {
	{0x000, 0x413, 0x524, 0x635, 0x746, 0x857, 0xA68, 0xC9A},
	{0x000, 0x012, 0x123, 0x234, 0x345, 0x457, 0x79A, 0xBDE},
	{0x000, 0x546, 0x657, 0x768, 0x879, 0x98A, 0xBAB, 0xEDD},
};
const uint16_t g_pMissionLandPal[MISSION_COUNT][8] = {
	{0x000, 0x310, 0x521, 0x742, 0x963, 0xB84, 0xDA6, 0xEC8},
	{0x000, 0x010, 0x121, 0x232, 0x343, 0x354, 0x576, 0x798},
	{0x000, 0x131, 0x242, 0x363, 0x585, 0x9A6, 0xCC8, 0xEEB},
};

// The far sky (logic.h). Cloud band colours 9-15, dark to light: bodies, then
// the rims lit from below.
const uint16_t g_pMissionCloudPal[MISSION_COUNT][8] = {
	{0x000, 0x312, 0x423, 0x534, 0x745, 0xA55, 0xD86, 0xFB8},   // dusk: sunset-lit
	{0x000, 0x011, 0x112, 0x123, 0x234, 0x346, 0x68A, 0x9BD},   // night: moonlit edges
	{0x000, 0x245, 0x356, 0x568, 0x89A, 0xCAA, 0xFCB, 0xFEE},   // dawn: pink and gold
};
// Sky dots: y ascending, >= 2 lines apart (a sprite DMA list reads the next
// item's control words in the line after one ends)
const tSkyItem g_pSkyDots[SKYDOT_COUNT] = {
	{27, 3, 0, 90}, {42, 3, 0, 300}, {60, 3, 0, 20}, {79, 3, 0, 200}, {96, 3, 0, 420}, {112, 3, 0, 150},
};
// colours 29-31: dusk - sunlit wisps; night - stars; dawn - pale wisps
const uint16_t g_pMissionSkyDotPal[MISSION_COUNT][3] = {
	{0xA45, 0xE86, 0xFCA}, {0x335, 0x99B, 0xFFF}, {0xDA9, 0xFDB, 0xFFE},
};

// Weather (logic.h): per mission its slide and wind per frame, 1/4 px.
// Dusk: motes sinking slowly on the breeze; night: rain, 4 px a frame, slanting
// left; dawn: spray rising off the sea, carried right.
static const int8_t s_pWxFallQ[MISSION_COUNT] = {1, 16, -1};
static const int8_t s_pWxDriftQ[MISSION_COUNT] = {2, -4, 3};
const int16_t g_pWxX[2][WX_PARTICLES] = {
	{17, 213, 96, 284, 141, 52, 250, 177, 8, 120, 299, 70, 232, 158, 35, 196},
	{262, 81, 189, 3, 135, 307, 44, 222, 108, 270, 160, 25, 205, 92, 245, 60},
};

uint8_t logicWeatherKind(uint8_t ubMission) {
	return ubMission;    // WX_DUST, WX_RAIN, WX_SPRAY: one a mission
}

static void weatherUpdate(tGame *g) {
	g->wxFall = (uint16_t)(g->wxFall + s_pWxFallQ[g->mission]);
	int16_t d = (int16_t)(g->wxDrift + s_pWxDriftQ[g->mission]);
	if(d >= 4 * SCREEN_W) d = (int16_t)(d - 4 * SCREEN_W);
	else if(d < 0) d = (int16_t)(d + 4 * SCREEN_W);
	g->wxDrift = d;
}

uint8_t logicWeather(const tGame *g, tWeatherFrame *pW) {
	if(g->state != GS_PLAY && g->state != GS_LOST) return 0;
	uint16_t uwPos = (uint16_t)(g->wxFall >> 2);
	pW->off = (uint8_t)(uwPos & (WX_PITCH - 1));
	pW->wraps = (uint8_t)(uwPos >> 4);
	int16_t c = g->cam;
	while(c >= SCREEN_W) c = (int16_t)(c - SCREEN_W);
	while(c < 0) c = (int16_t)(c + SCREEN_W);
	int16_t s = (int16_t)((g->wxDrift >> 2) - c);
	if(s < 0) s = (int16_t)(s + SCREEN_W);
	pW->shift = s;
	return 1;
}

// ------------------------------------------------------------- helpers ---

static inline int16_t absw(int16_t v) { return v < 0 ? (int16_t)-v : v; }

static int16_t skyX(int16_t wX, int16_t wCam, uint8_t ubShift, uint16_t uwDrift) {
	// (x - cam >> shift - drift) mod SKY_LOOP, then the left edge's overhang
	int16_t sx = (int16_t)((uint16_t)(wX - (wCam >> ubShift) - uwDrift) & (SKY_LOOP - 1));
	if(sx >= SKY_LOOP - 32) sx = (int16_t)(sx - SKY_LOOP);   // coming in from the left
	return sx >= SCREEN_W ? SKY_HIDDEN : sx;
}

int16_t logicSkyDotX(const tGame *g, uint8_t i) {
	return skyX(g_pSkyDots[i].x, g->cam, 4, 0);   // further away: 1/16
}

uint8_t logicSkyDotsAreStars(uint8_t ubMission) {
	return ubMission == 1;
}

uint16_t logicStarTwinkle(const tGame *g) {
	// the brightest star colour dims for 4 frames in every 32
	return (g->frame & 31) < 4 ? 0xAAC : 0xFFF;
}
static inline int16_t clampw(int16_t v, int16_t lo, int16_t hi) {
	return v < lo ? lo : v > hi ? hi : v;
}

uint16_t logicRandom(tGame *g) {
	// 16-bit Galois LFSR, taps 0xB400
	uint16_t r = g->lfsr;
	uint16_t lsb = r & 1;
	r >>= 1;
	if(lsb) r ^= 0xB400;
	g->lfsr = r;
	return r;
}

// Move a value by v/16 px per frame, keeping the remainder in *pFrac.
static inline int16_t subStep(int16_t *pPos, int16_t v, uint8_t *pFrac) {
	int16_t t = (int16_t)(*pFrac + (v & 15));
	int16_t whole = (int16_t)((v >> 4) + (t >> 4));
	*pFrac = (uint8_t)(t & 15);
	*pPos = (int16_t)(*pPos + whole);
	return whole;
}

static uint8_t boxHit(int16_t ax, int16_t ay, int16_t aw, int16_t ah,
	int16_t bx, int16_t by, int16_t bw, int16_t bh) {
	// boxes as left, top, width, height
	return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

static void addScore(tGame *g, uint16_t uwPts) {
	g->score += uwPts;
	// an invincible game doesn't count for the hi-score
	if(g->score > g->hiScore && g->difficulty != DIFFICULTY_INVINCIBLE) g->hiScore = g->score;
}

static void boomAt(tGame *g, int16_t x, int16_t y) {
	tBoom *pFree = 0;
	for(uint8_t i = 0; i < BOOM_MAX; ++i) {
		if(!g->pBooms[i].active) { pFree = &g->pBooms[i]; break; }
	}
	if(!pFree) {
		// reuse the oldest-looking one (the furthest on)
		pFree = &g->pBooms[0];
		for(uint8_t i = 1; i < BOOM_MAX; ++i) {
			if(g->pBooms[i].frame > pFree->frame) pFree = &g->pBooms[i];
		}
	}
	pFree->active = 1;
	pFree->x = x;
	pFree->y = y;
	pFree->frame = 0;
	pFree->timer = 0;
	g->events |= EV_BOOM;
}

static void setState(tGame *g, uint8_t ubState, uint16_t uwTimer) {
	g->state = ubState;
	g->stateTimer = uwTimer;
	g->events |= EV_STATE;
}

// ------------------------------------------------------------- setup ---

void logicInit(tGame *g) {
	uint8_t *p = (uint8_t *)g;
	for(uint16_t i = 0; i < sizeof(*g); ++i) p[i] = 0;
	g->lfsr = 0xACE1;
	g->lives = LIVES_START;
	g->state = GS_TITLE;
	g->cam = START_X - SCREEN_W / 2;
	logicStartMission(g, 0);
	g->state = GS_TITLE;
	g->hiScore = 5000;
	g->stateTimer = 0;
	g->difficulty = DIFFICULTY_NORMAL;
}

void logicChopperRespawn(tGame *g) {
	tChopper *c = &g->chop;
	uint8_t *p = (uint8_t *)c;
	for(uint16_t i = 0; i < sizeof(*c); ++i) p[i] = 0;
	c->x = START_X;
	c->y = GROUND_Y;
	c->landed = 1;
	c->alive = 1;
	c->face = FACE_LEFT;
	c->faceTarget = FACE_LEFT;
	c->lastSide = FACE_LEFT;
	c->prevFire = 1;   // a held fire from the menu doesn't shoot
	c->prevFire2 = 1;
	g->unloadTimer = UNLOAD_FRAMES;
}

void logicStartMission(tGame *g, uint8_t ubMission) {
	g->mission = ubMission;
	g->trip = 0;
	g->aboard = 0;
	g->saved = 0;
	g->killed = 0;
	g->spawnTimer = (uint16_t)(700 - ubMission * 150);   // a quiet start
	g->wxFall = 0x8000;
	g->wxDrift = 0;
	for(uint8_t b = 0; b < BARRACKS_COUNT; ++b) {
		g->pBarracks[b].x = s_pBarracksX[b];
		g->pBarracks[b].hp = 2;
		g->pBarracks[b].released = 0;
		g->pBarracks[b].timer = 0;
	}
	for(uint8_t i = 0; i < HOSTAGE_COUNT; ++i) {
		tHostage *h = &g->pHostages[i];
		h->home = (int8_t)(i / HOSTAGES_PER_BARRACKS);
		h->state = HS_INSIDE;
		h->x = s_pBarracksX[h->home];
		h->anim = 0;
		h->timer = 0;
		h->dir = 1;
	}
	g->outCount = 0;
	for(uint8_t i = 0; i < TANK_MAX; ++i) g->pTanks[i].active = 0;
	for(uint8_t i = 0; i < JET_MAX; ++i) { g->pJets[i].active = 0; g->pJets[i].cool = (uint8_t)(100 + i * 120); }
	for(uint8_t i = 0; i < MINE_MAX; ++i) g->pMines[i].active = 0;
	for(uint8_t i = 0; i < SHOT_MAX; ++i) g->pShots[i].active = 0;
	for(uint8_t i = 0; i < ESHOT_MAX; ++i) g->pEShots[i].active = 0;
	for(uint8_t i = 0; i < BOOM_MAX; ++i) g->pBooms[i].active = 0;
	logicChopperRespawn(g);
	logicCameraSnap(g);
	setState(g, GS_INTRO, INTRO_FRAMES);
}

void logicStartGame(tGame *g) {
	g->lives = LIVES_START;
	g->score = 0;
	g->savedTotal = 0;
	logicStartMission(g, 0);
}

// ---------------------------------------------------------- difficulty ---
// Per difficulty 0..5 (0 invincible: plays like 3, normal). Tables, no maths.

static const int8_t s_pTankExtra[DIFFICULTY_MAX + 1] = {0, -1, -1, 0, 1, 2};     // tanks at once
static const int8_t s_pTankCool[DIFFICULTY_MAX + 1] = {0, 60, 30, 0, -15, -30};  // frames between shells
static const uint8_t s_pTankCoolMin[DIFFICULTY_MAX + 1] = {40, 60, 50, 40, 32, 26};
static const uint8_t s_pAimMask[DIFFICULTY_MAX + 1] = {31, 63, 63, 31, 15, 7};   // aim error: +-(mask+1)/2 px
static const uint8_t s_pJetsFrom[DIFFICULTY_MAX + 1] = {1, 3, 2, 1, 1, 0};       // trips + missions before jets
static const uint8_t s_pMinesFrom[DIFFICULTY_MAX + 1] = {2, 4, 3, 2, 1, 1};      // ... and mines
static const int16_t s_pSpawnExtra[DIFFICULTY_MAX + 1] = {0, 120, 60, 0, -40, -70};
static const int16_t s_pJetRest[DIFFICULTY_MAX + 1] = {0, 80, 40, 0, -30, -50};  // frames between jet passes
// how far off screen a jet starts (its sound and radar dot are the warning) and
// how long it's on screen before its first shot
static const uint8_t s_pJetLead[DIFFICULTY_MAX + 1] = {255, 255, 220, 180, 150, 130};
static const uint8_t s_pJetAim[DIFFICULTY_MAX + 1] = {40, 40, 30, 24, 20, 18};
// a tank's wind-up before each shell (it comes out of the time between shells:
// the rate of fire is the same, the shell is announced)
static const uint8_t s_pTankTell[DIFFICULTY_MAX + 1] = {16, 22, 18, 16, 14, 12};

void logicSetDifficulty(tGame *g, uint8_t ubLevel) {
	if(ubLevel <= DIFFICULTY_MAX) g->difficulty = ubLevel;
}

uint8_t logicTankLimit(const tGame *g) {
	int16_t n = (int16_t)(1 + g->trip + g->mission + s_pTankExtra[g->difficulty]);
	return (uint8_t)(n < 1 ? 1 : n > TANK_MAX ? TANK_MAX : n);
}

uint8_t logicJetsOn(const tGame *g) {
	return g->trip + g->mission >= s_pJetsFrom[g->difficulty];
}

uint8_t logicMinesOn(const tGame *g) {
	return g->trip + g->mission >= s_pMinesFrom[g->difficulty];
}

uint8_t logicTankTell(const tGame *g) {
	return s_pTankTell[g->difficulty];
}

// --------------------------------------------------------------- hostages ---

void logicHostageOut(tGame *g, uint8_t i, uint8_t ubState, int16_t x) {
	tHostage *h = &g->pHostages[i];
	uint8_t isOut = h->state == HS_FREE || h->state == HS_TO_CHOPPER || h->state == HS_TO_HANGAR ||
		(h->state == HS_DEAD && h->timer);
	h->state = ubState;
	h->x = x;
	h->timer = 0;
	if(!isOut && g->outCount < HOSTAGE_COUNT) g->pOut[g->outCount++] = i;
}

static void hostageKill(tGame *g, tHostage *h) {
	h->state = HS_DEAD;
	h->timer = 100;
	h->anim = 0;
	++g->killed;
	g->events |= EV_DIED;
}

// Kill free hostages within r px of x (on the ground)
static void hostagesBlast(tGame *g, int16_t x, int16_t r) {
	for(uint8_t k = 0; k < g->outCount; ++k) {
		tHostage *h = &g->pHostages[g->pOut[k]];
		if((h->state == HS_FREE || h->state == HS_TO_CHOPPER) && absw((int16_t)(h->x - x)) <= r) {
			hostageKill(g, h);
		}
	}
}

__attribute__((noinline)) static void hostagesUpdate(tGame *g) {
	tChopper *c = &g->chop;
	uint8_t isCalling = c->alive && c->landed && g->aboard < CABIN_MAX;
	uint8_t isOdd = g->frame & 1;
	uint8_t ubKept = 0;
	for(uint8_t k = 0; k < g->outCount; ++k) {
		uint8_t i = g->pOut[k];
		tHostage *h = &g->pHostages[i];
		switch(h->state) {
			case HS_FREE: {
				int16_t dx = (int16_t)(c->x - h->x);
				if(isCalling && absw(dx) < HOSTAGE_CALL_RANGE) {
					h->state = HS_TO_CHOPPER;
					break;
				}
				if(c->alive && !c->landed && absw(dx) < 120) {
					// waving at the chopper overhead
					h->anim = (uint8_t)((g->frame >> 3) & 1 ? MAN_WAVE1 : MAN_WAVE0);
					break;
				}
				// wander around the barracks
				if(h->timer) --h->timer;
				else {
					uint16_t r = logicRandom(g);
					h->timer = (uint8_t)(30 + (r & 63));
					h->dir = (r & 0x100) ? 1 : -1;
					if(r & 0x600) h->dir = 0;  // mostly standing
				}
				int16_t wHome = g->pBarracks[(uint8_t)h->home].x;
				if(h->x < wHome - 56) h->dir = 1;
				if(h->x > wHome + 56) h->dir = -1;
				if(h->dir && isOdd) h->x = (int16_t)(h->x + h->dir);
				h->anim = h->dir == 0 ? MAN_STAND :
					(uint8_t)((h->dir > 0 ? MAN_RUN0 : MAN_RUNL0) + ((g->frame >> 3) & 1));
			} break;
			case HS_TO_CHOPPER: {
				if(!isCalling) { h->state = HS_FREE; h->timer = 0; break; }
				int16_t dx = (int16_t)(c->x - h->x);
				if(absw(dx) <= 3) {
					h->state = HS_ABOARD;
					++g->aboard;
					g->events |= EV_BOARD;
					isCalling = g->aboard < CABIN_MAX;
					break;
				}
				int8_t s = dx > 0 ? 1 : -1;
				h->x = (int16_t)(h->x + s);
				if(!(g->frame & 3)) h->x = (int16_t)(h->x + s);  // 1.25 px a frame
				h->anim = (uint8_t)((s > 0 ? MAN_RUN0 : MAN_RUNL0) + ((g->frame >> 2) & 1));
			} break;
			case HS_TO_HANGAR: {
				h->x = (int16_t)(h->x + 1);
				h->anim = (uint8_t)(MAN_RUN0 + ((g->frame >> 2) & 1));
				if(h->x >= HANGAR_X + 12) {
					h->state = HS_SAVED;
					++g->saved;
					++g->savedTotal;
					addScore(g, 100);
					g->events |= EV_SAVED;
				}
			} break;
			case HS_DEAD:
				if(h->timer) --h->timer;
				break;
			default:
				break;
		}
		// still out in the open: keep it on the list
		if(h->state == HS_FREE || h->state == HS_TO_CHOPPER || h->state == HS_TO_HANGAR ||
			(h->state == HS_DEAD && h->timer)) {
			g->pOut[ubKept++] = i;
		}
	}
	// (anyone put out during the loop - none are - would be appended after it)
	g->outCount = ubKept;
}

// --------------------------------------------------------------- barracks ---

static void barracksDamage(tGame *g, uint8_t b, uint8_t ubDmg) {
	tBarracks *pB = &g->pBarracks[b];
	if(!pB->hp) return;
	pB->hp = ubDmg >= pB->hp ? 0 : (uint8_t)(pB->hp - ubDmg);
	if(!pB->hp) {
		g->events |= EV_OPEN;
		g->evBarracks = (uint8_t)(b + 1);
		boomAt(g, pB->x, FEET_Y - 20);
		addScore(g, 25);
		pB->timer = 20;
	}
}

__attribute__((noinline)) static void barracksUpdate(tGame *g) {
	for(uint8_t b = 0; b < BARRACKS_COUNT; ++b) {
		tBarracks *pB = &g->pBarracks[b];
		if(pB->hp || pB->released >= HOSTAGES_PER_BARRACKS) continue;
		if(pB->timer) { --pB->timer; continue; }
		pB->timer = 8;
		tHostage *h = &g->pHostages[b * HOSTAGES_PER_BARRACKS + pB->released];
		++pB->released;
		if(h->state != HS_INSIDE) continue;
		uint16_t r = logicRandom(g);
		logicHostageOut(g, (uint8_t)(b * HOSTAGES_PER_BARRACKS + pB->released - 1), HS_FREE,
			(int16_t)(pB->x - 8 + (int16_t)(r & 15)));
		h->dir = (r & 0x100) ? 1 : -1;
		h->timer = (uint8_t)(20 + (r >> 10));
	}
}

// ---------------------------------------------------------------- chopper ---

static void chopperDestroy(tGame *g) {
	tChopper *c = &g->chop;
	if(!c->alive) return;
	if(g->difficulty == DIFFICULTY_INVINCIBLE) {
		// it would have been: show the hit, carry on
		boomAt(g, c->x, (int16_t)(c->y - 8));
		g->events |= EV_SHRUG;
		return;
	}
	c->alive = 0;
	c->landed = 0;
	c->crashTimer = CRASH_FRAMES;
	c->vx = (int16_t)(c->vx >> 1);
	// everyone aboard is lost
	for(uint8_t i = 0; i < HOSTAGE_COUNT; ++i) {
		tHostage *h = &g->pHostages[i];
		if(h->state == HS_ABOARD) {
			h->state = HS_DEAD;
			h->timer = 0;
			++g->killed;
		}
	}
	g->aboard = 0;
	boomAt(g, c->x, (int16_t)(c->y - 8));
	g->events |= EV_CRASH;
	if(g->lives) --g->lives;
	setState(g, GS_LOST, LOST_FRAMES);
}

// The highest the skids may go down to at x: the ground, or a roof
static int16_t floorAt(const tGame *g, int16_t x, uint8_t *pIsGround) {
	*pIsGround = 1;
	for(uint8_t b = 0; b < BARRACKS_COUNT; ++b) {
		if(absw((int16_t)(x - g->pBarracks[b].x)) < 24 + 12) {
			*pIsGround = 0;
			return FEET_Y - 28;
		}
	}
	if(x + 14 > HANGAR_X && x - 14 < HANGAR_X + 64) {
		*pIsGround = 0;
		return FEET_Y - 40;
	}
	return GROUND_Y;
}

static void shotFire(tGame *g, uint8_t ubKind) {
	tChopper *c = &g->chop;
	for(uint8_t i = 0; i < SHOT_MAX; ++i) {
		tShot *s = &g->pShots[i];
		if(s->active) continue;
		s->active = 1;
		s->kind = ubKind;
		s->fx = s->fy = 0;
		if(ubKind == SHOT_CANNON) {
			int8_t dir = c->face == FACE_RIGHT ? 1 : -1;
			int16_t fwd = dir > 0 ? c->vx : (int16_t)-c->vx;
			s->x = (int16_t)(c->x + dir * 16);
			s->y = (int16_t)(c->y - 6);
			s->vx = (int16_t)(dir * CANNON_SPEED + c->vx);
			// tilted forward at speed: the gun points down a little
			s->vy = fwd >= 32 ? 14 : fwd >= 12 ? 7 : 0;
			s->life = CANNON_LIFE;
			g->events |= EV_CANNON;
		}
		else {
			s->x = c->x;
			s->y = (int16_t)(c->y + 1);
			s->vx = (int16_t)(c->vx >> 1);
			s->vy = 16;
			s->life = 120;
			g->events |= EV_BOMB;
		}
		return;
	}
}

static void turnTo(tChopper *c, uint8_t ubFace) {
	if(c->faceTarget == ubFace) return;
	c->faceTarget = ubFace;
	if(!c->turnTimer && c->face != ubFace) c->turnTimer = TURN_STEP_FRAMES;
}

void logicChopperUpdate(tGame *g, const tInput *pIn) {
	tChopper *c = &g->chop;
	uint8_t isFireEdge = pIn->fire && !c->prevFire;
	uint8_t isFire2Edge = pIn->fire2 && !c->prevFire2;
	c->prevFire = pIn->fire;
	c->prevFire2 = pIn->fire2;
	uint8_t isTurning = c->turnTimer || c->face != c->faceTarget;
	uint8_t isFireUsed = 0;

	// --- facing
	if(isFire2Edge) {
		// cycle: left -> front -> right -> front -> left
		uint8_t from = c->faceTarget;
		if(from == FACE_FRONT) turnTo(c, c->lastSide == FACE_LEFT ? FACE_RIGHT : FACE_LEFT);
		else turnTo(c, FACE_FRONT);
	}
	if(pIn->dy > 0 && isFireEdge && !c->landed && c->faceTarget != FACE_FRONT) {
		turnTo(c, FACE_FRONT);   // down + fire: face the screen to bomb
		isFireUsed = 1;
	}
	if(pIn->dx) {
		uint8_t want = pIn->dx < 0 ? FACE_LEFT : FACE_RIGHT;
		if(c->faceTarget != want && !isTurning) {
			++c->turnHold;
			// from the front it takes a longer push: you can strafe while bombing
			uint8_t hold = c->face == FACE_FRONT ? TURN_HOLD_FRAMES * 4 : TURN_HOLD_FRAMES;
			if(c->turnHold >= hold) {
				turnTo(c, want);
				c->turnHold = 0;
			}
		}
		else c->turnHold = 0;
	}
	else c->turnHold = 0;

	if(c->turnTimer) {
		if(!--c->turnTimer) {
			// one step: side <-> front
			if(c->faceTarget > c->face) ++c->face;
			else if(c->faceTarget < c->face) --c->face;
			if(c->face != FACE_FRONT) c->lastSide = c->face;
			if(c->face != c->faceTarget) c->turnTimer = TURN_STEP_FRAMES;
		}
	}
	isTurning = c->turnTimer || c->face != c->faceTarget;

	// --- flying
	if(c->landed) {
		c->vx = 0;
		c->vy = 0;
		if(pIn->dy < 0) {
			c->landed = 0;
			c->vy = -VY_ACCEL * 2;
			g->events |= EV_TAKEOFF;
		}
	}
	else {
		if(c->face == FACE_FRONT && !isTurning) {
			// head-on: a sidestep - quick to go, quicker to turn round, quick to stop
			if(pIn->dx) {
				int16_t wWant = (int16_t)(pIn->dx * HO_VX_MAX);
				uint8_t isAgainst = (c->vx > 0 && pIn->dx < 0) || (c->vx < 0 && pIn->dx > 0);
				int16_t wStep = isAgainst ? HO_REVERSE : HO_ACCEL;
				if(c->vx < wWant) c->vx = (int16_t)(c->vx + wStep > wWant ? wWant : c->vx + wStep);
				else if(c->vx > wWant) c->vx = (int16_t)(c->vx - wStep < wWant ? wWant : c->vx - wStep);
			}
			else if(c->vx > 0) c->vx = (int16_t)(c->vx > HO_DRAG ? c->vx - HO_DRAG : 0);
			else if(c->vx < 0) c->vx = (int16_t)(c->vx < -HO_DRAG ? c->vx + HO_DRAG : 0);
		}
		else if(pIn->dx > 0) c->vx = (int16_t)(c->vx + VX_ACCEL);
		else if(pIn->dx < 0) c->vx = (int16_t)(c->vx - VX_ACCEL);
		else if(c->vx > 0) c->vx = (int16_t)(c->vx - VX_DRAG);
		else if(c->vx < 0) c->vx = (int16_t)(c->vx + VX_DRAG);
		c->vx = clampw(c->vx, -VX_MAX, VX_MAX);
		if(pIn->dy < 0) c->vy = (int16_t)(c->vy - VY_ACCEL);
		else if(pIn->dy > 0) c->vy = (int16_t)(c->vy + VY_ACCEL);
		else if(c->vy > 1) c->vy = (int16_t)(c->vy - 2);
		else if(c->vy < -1) c->vy = (int16_t)(c->vy + 2);
		else c->vy = 0;
		c->vy = clampw(c->vy, -VY_UP_MAX, VY_DOWN_MAX);
	}
	subStep(&c->x, c->vx, &c->fx);
	subStep(&c->y, c->vy, &c->fy);
	if(c->x < 24) { c->x = 24; c->vx = 0; }
	if(c->x > WORLD_W - 24) { c->x = WORLD_W - 24; c->vx = 0; }
	if(c->y < CEIL_Y) { c->y = CEIL_Y; c->vy = 0; }
	uint8_t isGround;
	int16_t wFloor = floorAt(g, c->x, &isGround);
	if(c->y >= wFloor && !c->landed) {
		c->y = wFloor;
		c->fy = 0;
		if(c->vy > 0) c->vy = 0;
		if(isGround) {
			c->landed = 1;
			c->vx = 0;
			g->events |= EV_LAND;
			g->unloadTimer = UNLOAD_FRAMES;
			hostagesBlast(g, c->x, 13);   // landed on someone
		}
	}

	// --- firing
	if(pIn->fire && !isFireUsed && !isTurning) {
		if(isFireEdge || c->fireTimer <= 1) {
			if(c->face == FACE_FRONT) {
				if(!c->landed) shotFire(g, SHOT_BOMB);
			}
			else shotFire(g, SHOT_CANNON);
			c->fireTimer = FIRE_REPEAT;
		}
		else --c->fireTimer;
	}
	else c->fireTimer = 0;
}

static void chopperWreckUpdate(tGame *g) {
	tChopper *c = &g->chop;
	if(c->crashTimer) --c->crashTimer;
	if(c->y < GROUND_Y) {
		c->vy = (int16_t)(c->vy + 2);
		if(c->vy > 48) c->vy = 48;
		subStep(&c->y, c->vy, &c->fy);
		subStep(&c->x, c->vx, &c->fx);
		if(c->y >= GROUND_Y) {
			c->y = GROUND_Y;
			boomAt(g, c->x, GROUND_Y - 10);
			hostagesBlast(g, c->x, 18);
		}
	}
	if((c->crashTimer & 15) == 8 && c->crashTimer > 20) {
		boomAt(g, (int16_t)(c->x - 8 + (int16_t)(logicRandom(g) & 15)), (int16_t)(c->y - 10));
	}
}

static void unloadUpdate(tGame *g) {
	tChopper *c = &g->chop;
	if(!c->alive || !c->landed || c->x < BASE_PAD_X0 || c->x > BASE_PAD_X1 || !g->aboard) return;
	if(--g->unloadTimer > 0) return;
	g->unloadTimer = UNLOAD_FRAMES;
	for(uint8_t i = 0; i < HOSTAGE_COUNT; ++i) {
		tHostage *h = &g->pHostages[i];
		if(h->state != HS_ABOARD) continue;
		logicHostageOut(g, i, HS_TO_HANGAR, c->x);
		--g->aboard;
		g->events |= EV_UNLOAD;
		if(!g->aboard) ++g->trip;   // a trip home completed
		break;
	}
}

// ------------------------------------------------------------- shots ---

static uint8_t shotHitsTargets(tGame *g, tShot *s) {
	// The player's cannon: jets, mines, tanks, barracks, people. 1 = used up.
	int16_t sx = (int16_t)(s->x - 3), sy = (int16_t)(s->y - 1);
	for(uint8_t i = 0; i < JET_MAX; ++i) {
		tJet *j = &g->pJets[i];
		if(j->active && boxHit(sx, sy, 6, 3, (int16_t)(j->x - 16), (int16_t)(j->y - 6), 32, 12)) {
			j->active = 0;
			j->cool = 200;
			boomAt(g, j->x, j->y);
			addScore(g, 100);
			return 1;
		}
	}
	for(uint8_t i = 0; i < MINE_MAX; ++i) {
		tMine *m = &g->pMines[i];
		if(m->active && boxHit(sx, sy, 6, 3, (int16_t)(m->x - 7), (int16_t)(m->y - 6), 14, 12)) {
			m->active = 0;
			boomAt(g, m->x, m->y);
			addScore(g, 75);
			return 1;
		}
	}
	if(sy < FEET_Y - 30) return 0;
	for(uint8_t i = 0; i < TANK_MAX; ++i) {
		tTank *t = &g->pTanks[i];
		if(t->active && boxHit(sx, sy, 6, 3, (int16_t)(t->x - 12), FEET_Y - 14, 24, 14)) {
			if(t->hp) --t->hp;
			if(!t->hp) {
				t->active = 0;
				boomAt(g, t->x, FEET_Y - 10);
				addScore(g, 50);
			}
			return 1;
		}
	}
	for(uint8_t b = 0; b < BARRACKS_COUNT; ++b) {
		tBarracks *pB = &g->pBarracks[b];
		if(pB->hp && boxHit(sx, sy, 6, 3, (int16_t)(pB->x - 24), FEET_Y - 28, 48, 28)) {
			barracksDamage(g, b, 1);
			return 1;
		}
	}
	if(sy >= FEET_Y - 12) {
		for(uint8_t k = 0; k < g->outCount; ++k) {
			tHostage *h = &g->pHostages[g->pOut[k]];
			if((h->state == HS_FREE || h->state == HS_TO_CHOPPER) &&
				boxHit(sx, sy, 6, 3, (int16_t)(h->x - 3), FEET_Y - 12, 6, 12)) {
				hostageKill(g, h);
				return 1;
			}
		}
	}
	return 0;
}

static void bombExplode(tGame *g, int16_t x) {
	boomAt(g, x, FEET_Y - 12);
	for(uint8_t i = 0; i < TANK_MAX; ++i) {
		tTank *t = &g->pTanks[i];
		if(t->active && absw((int16_t)(t->x - x)) < 22) {
			t->active = 0;
			boomAt(g, t->x, FEET_Y - 10);
			addScore(g, 50);
		}
	}
	for(uint8_t b = 0; b < BARRACKS_COUNT; ++b) {
		if(absw((int16_t)(g->pBarracks[b].x - x)) < 30) barracksDamage(g, b, 2);
	}
	hostagesBlast(g, x, 14);
}

__attribute__((noinline)) static void shotsUpdate(tGame *g) {
	for(uint8_t i = 0; i < SHOT_MAX; ++i) {
		tShot *s = &g->pShots[i];
		if(!s->active) continue;
		if(s->kind == SHOT_BOMB) s->vy = (int16_t)(s->vy + BOMB_GRAVITY);
		subStep(&s->x, s->vx, &s->fx);
		subStep(&s->y, s->vy, &s->fy);
		if(s->kind == SHOT_BOMB) {
			// barracks roofs stop a bomb too
			uint8_t isGround;
			int16_t wFloor = floorAt(g, s->x, &isGround);
			if(!isGround && wFloor < GROUND_Y && s->y >= wFloor) {
				s->active = 0;
				bombExplode(g, s->x);
				continue;
			}
			if(s->y >= FEET_Y - 2) {
				s->active = 0;
				bombExplode(g, s->x);
				continue;
			}
			// a bomb can hit a jet or a mine on the way down
		}
		else {
			if(shotHitsTargets(g, s)) { s->active = 0; continue; }
			if(s->y >= FEET_Y - 1) {
				s->active = 0;
				boomAt(g, s->x, FEET_Y - 8);
				continue;
			}
		}
		if(!--s->life || s->x < 0 || s->x >= WORLD_W) s->active = 0;
	}
}

static tShot *eshotNew(tGame *g) {
	for(uint8_t i = 0; i < ESHOT_MAX; ++i) {
		if(!g->pEShots[i].active) return &g->pEShots[i];
	}
	return 0;
}

__attribute__((noinline)) static void eshotsUpdate(tGame *g) {
	tChopper *c = &g->chop;
	for(uint8_t i = 0; i < ESHOT_MAX; ++i) {
		tShot *s = &g->pEShots[i];
		if(!s->active) continue;
		if(s->kind == ESHOT_BOMB) s->vy = (int16_t)(s->vy + 2);
		subStep(&s->x, s->vx, &s->fx);
		subStep(&s->y, s->vy, &s->fy);
		if(c->alive && boxHit((int16_t)(s->x - 2), (int16_t)(s->y - 2), 4, 4,
			(int16_t)(c->x - 13), (int16_t)(c->y - 13), 26, 13)) {
			s->active = 0;
			chopperDestroy(g);
			continue;
		}
		if(s->y >= FEET_Y - 1) {
			s->active = 0;
			boomAt(g, s->x, FEET_Y - 8);
			hostagesBlast(g, s->x, 8);   // shells landing among people
			continue;
		}
		if(!--s->life || s->y < PLAY_TOP || s->x < 0 || s->x >= WORLD_W) s->active = 0;
	}
}

// ------------------------------------------------------------- enemies ---

// Aim from (x, y) at (tx, ty): a speed of 2-4 px a frame, no division
static void aim(int16_t dx, int16_t dy, int16_t *pVx, int16_t *pVy) {
	int16_t m = absw(dx) > absw(dy) ? absw(dx) : absw(dy);
	if(!m) { *pVx = 0; *pVy = -32; return; }
	while(m >= 64) { dx = (int16_t)(dx >> 1); dy = (int16_t)(dy >> 1); m = (int16_t)(m >> 1); }
	while(m < 32) { dx = (int16_t)(dx << 1); dy = (int16_t)(dy << 1); m = (int16_t)(m << 1); }
	*pVx = dx;
	*pVy = dy;
}

__attribute__((noinline)) static void tanksUpdate(tGame *g) {
	tChopper *c = &g->chop;
	for(uint8_t i = 0; i < TANK_MAX; ++i) {
		tTank *t = &g->pTanks[i];
		if(!t->active) continue;
		int16_t dx = (int16_t)(c->x - t->x);
		if(t->fireAnim) --t->fireAnim;
		uint8_t isInRange = c->alive && c->y > 150 && absw(dx) < 200;
		if(t->tell) {
			// the wind-up: braced (it doesn't drive), barrel up, flashing
			if(--t->tell) continue;
			// the shell - unless the chopper got away meanwhile
			tShot *s = isInRange ? eshotNew(g) : 0;
			if(s) {
				// lead the target, give or take 16 px
				uint8_t ubMask = s_pAimMask[g->difficulty];
				int16_t tx = (int16_t)(c->x + (c->vx >> 1) - t->x + (int16_t)(logicRandom(g) & ubMask) - (ubMask >> 1) - 1);
				int16_t ty = (int16_t)(c->y - 8 - (FEET_Y - 12));
				s->active = 1;
				s->kind = ESHOT_SHELL;
				s->x = (int16_t)(t->x + (tx > 0 ? 10 : -10));
				s->y = FEET_Y - 13;
				s->fx = s->fy = 0;
				aim(tx, ty, &s->vx, &s->vy);
				s->life = 100;
				t->fireAnim = 8;
				g->events |= EV_TANKFIRE;
			}
			// the time to the next shell, less the next tell (the same rate of fire)
			int16_t wCool = (int16_t)(110 - g->mission * 20 - g->trip * 8 + s_pTankCool[g->difficulty]);
			uint8_t ubMin = s_pTankCoolMin[g->difficulty];
			t->cool = (uint8_t)((wCool < ubMin ? ubMin : wCool) - logicTankTell(g));
			t->cool = (uint8_t)(t->cool + (logicRandom(g) & 31));
			continue;
		}
		// drive at the chopper (half a pixel a frame), stay out of friendly ground
		if(absw(dx) > 20 && (g->frame & 1)) {
			t->dir = dx > 0 ? 1 : -1;
			int16_t nx = (int16_t)(t->x + t->dir);
			if(nx <= BORDER_X - 24 && nx >= 16) { t->x = nx; ++t->anim; }
		}
		if(t->cool) { --t->cool; continue; }
		// a landed or low chopper in range: the tell, then (tell frames on) the shell
		if(isInRange) {
			t->dir = dx > 0 ? 1 : -1;
			t->tell = logicTankTell(g);
			g->events |= EV_TANKTELL;
			continue;
		}
		int16_t wCool = (int16_t)(110 - g->mission * 20 - g->trip * 8 + s_pTankCool[g->difficulty]);
		uint8_t ubMin = s_pTankCoolMin[g->difficulty];
		t->cool = (uint8_t)(wCool < ubMin ? ubMin : wCool);
		t->cool = (uint8_t)(t->cool + (logicRandom(g) & 31));
	}
}

__attribute__((noinline)) static void jetsUpdate(tGame *g) {
	tChopper *c = &g->chop;
	for(uint8_t i = 0; i < JET_MAX; ++i) {
		tJet *j = &g->pJets[i];
		if(!j->active) {
			if(!logicJetsOn(g) || !c->alive || c->x > BORDER_X + 120) continue;
			if(j->cool) { --j->cool; continue; }
			// mostly from ahead (where the chopper is heading: you see it coming),
			// 1 in 4 from behind; well off screen, so its sound and its radar dot
			// come first
			j->active = 1;
			uint16_t r = logicRandom(g);
			int8_t ahead = c->vx > 0 ? -1 : c->vx < 0 ? 1 : (c->face == FACE_RIGHT ? -1 : 1);
			j->dir = (r & 3) ? ahead : (int8_t)-ahead;
			int16_t wLead = s_pJetLead[g->difficulty];
			j->x = (int16_t)(j->dir > 0 ? g->cam - wLead : g->cam + SCREEN_W + wLead);
			j->seen = 0;
			j->y = clampw((int16_t)(c->y - 20), PLAY_TOP + 16, 180);
			j->hp = 1;
			j->pass = 0;
			j->cool = 20;
			g->events |= EV_JET;
			continue;
		}
		j->x = (int16_t)(j->x + j->dir * 4);
		// follow the chopper's height slowly
		int16_t wTargetY = clampw((int16_t)(c->y - 8), PLAY_TOP + 16, 200);
		if((g->frame & 1) && j->y < wTargetY) ++j->y;
		if((g->frame & 1) && j->y > wTargetY) --j->y;
		if(j->cool) --j->cool;
		if(j->x > g->cam - 16 && j->x < g->cam + SCREEN_W + 16) {
			if(j->seen < 255) ++j->seen;
		}
		int16_t dx = (int16_t)(c->x - j->x);
		if(c->alive && !j->cool && j->seen >= s_pJetAim[g->difficulty]) {
			int16_t ahead = j->dir > 0 ? dx : (int16_t)-dx;
			if(!c->landed && ahead > JET_AHEAD_MIN && ahead < 200 && absw((int16_t)(c->y - 8 - j->y)) < 14) {
				tShot *s = eshotNew(g);
				if(s) {
					s->active = 1;
					s->kind = ESHOT_MISSILE;
					s->x = (int16_t)(j->x + j->dir * 16);
					s->y = (int16_t)(j->y + 2);
					s->vx = (int16_t)(j->dir * JET_MISSILE_SPEED);
					s->vy = 0;
					s->fx = s->fy = 0;
					s->life = 60;
					j->cool = 50;
				}
			}
			else if(c->landed && absw(dx) < 24) {
				tShot *s = eshotNew(g);
				if(s) {
					s->active = 1;
					s->kind = ESHOT_BOMB;
					s->x = j->x;
					s->y = (int16_t)(j->y + 6);
					s->vx = (int16_t)(j->dir * 40);
					s->vy = 8;
					s->fx = s->fy = 0;
					s->life = 120;
					j->cool = 40;
				}
			}
		}
		if(c->alive && boxHit((int16_t)(j->x - 14), (int16_t)(j->y - 5), 28, 10,
			(int16_t)(c->x - 13), (int16_t)(c->y - 13), 26, 13)) {
			j->active = 0;
			boomAt(g, j->x, j->y);
			chopperDestroy(g);
			continue;
		}
		if((j->dir > 0 && j->x > g->cam + SCREEN_W + 64 && j->seen) || (j->dir < 0 && j->x < g->cam - 64 && j->seen)) {
			j->active = 0;
			int16_t wRest = (int16_t)(160 - g->mission * 30 + s_pJetRest[g->difficulty]);
			j->cool = (uint8_t)((wRest < 40 ? 40 : wRest) + (logicRandom(g) & 63));
		}
	}
}

__attribute__((noinline)) static void minesUpdate(tGame *g) {
	tChopper *c = &g->chop;
	for(uint8_t i = 0; i < MINE_MAX; ++i) {
		tMine *m = &g->pMines[i];
		if(!m->active) continue;
		++m->anim;
		if(c->alive && (g->frame & 1)) {
			int16_t dx = (int16_t)(c->x - m->x), dy = (int16_t)(c->y - 8 - m->y);
			if(absw(dx) < 300) {
				m->x = (int16_t)(m->x + (dx > 0 ? 1 : dx < 0 ? -1 : 0));
				if(!(g->frame & 2)) m->y = (int16_t)(m->y + (dy > 0 ? 1 : dy < 0 ? -1 : 0));
			}
		}
		if(m->y > FEET_Y - 40) m->y = FEET_Y - 40;
		if(c->alive && boxHit((int16_t)(m->x - 6), (int16_t)(m->y - 6), 12, 12,
			(int16_t)(c->x - 13), (int16_t)(c->y - 13), 26, 13)) {
			m->active = 0;
			boomAt(g, m->x, m->y);
			chopperDestroy(g);
		}
	}
}

static void enemiesSpawn(tGame *g) {
	if(g->spawnTimer) { --g->spawnTimer; return; }
	uint16_t r = logicRandom(g);
	int16_t wSpawn = (int16_t)(220 - g->mission * 40 + s_pSpawnExtra[g->difficulty]);
	g->spawnTimer = (uint16_t)((wSpawn < 60 ? 60 : wSpawn) + (r & 127));
	// tanks: from off screen, in enemy ground
	uint8_t n = 0;
	for(uint8_t i = 0; i < TANK_MAX; ++i) n = (uint8_t)(n + g->pTanks[i].active);
	if(n < logicTankLimit(g) && g->chop.x < BORDER_X + 200) {
		for(uint8_t i = 0; i < TANK_MAX; ++i) {
			tTank *t = &g->pTanks[i];
			if(t->active) continue;
			int16_t x = (int16_t)(g->cam - 40);
			if(x < 24 || (r & 2)) x = (int16_t)(g->cam + SCREEN_W + 40);
			if(x > BORDER_X - 30) x = (int16_t)(g->cam - 40);
			if(x < 24) break;
			t->active = 1;
			t->x = x;
			t->hp = 2;
			t->dir = x < g->chop.x ? 1 : -1;
			t->cool = 150;
			t->anim = 0;
			t->fireAnim = 0;
			t->tell = 0;
			break;
		}
	}
	if(logicMinesOn(g) && (r & 4) && g->chop.x < BORDER_X) {
		for(uint8_t i = 0; i < MINE_MAX; ++i) {
			tMine *m = &g->pMines[i];
			if(m->active) continue;
			m->active = 1;
			m->x = (int16_t)((r & 8) ? g->cam - 24 : g->cam + SCREEN_W + 24);
			m->y = (int16_t)(PLAY_TOP + 30 + (int16_t)((r >> 8) & 63));
			m->anim = 0;
			break;
		}
	}
}

__attribute__((noinline)) static void boomsUpdate(tGame *g) {
	for(uint8_t i = 0; i < BOOM_MAX; ++i) {
		tBoom *b = &g->pBooms[i];
		if(!b->active) continue;
		if(++b->timer >= 4) {
			b->timer = 0;
			if(++b->frame >= 5) b->active = 0;
		}
	}
}

// -------------------------------------------------------------- camera ---

static int16_t leadTarget(const tGame *g) {
	uint8_t f = g->chop.faceTarget;
	return (int16_t)(f == FACE_LEFT ? -CAM_LEAD * 16 : f == FACE_RIGHT ? CAM_LEAD * 16 : 0);
}

static int16_t cameraWanted(const tGame *g) {
	// whole pixels on both sides: the chopper's screen x is exactly
	// SCREEN_W / 2 - lead, whatever its own sub-pixel is doing
	return clampw((int16_t)(g->chop.x - SCREEN_W / 2 + (g->camLead >> 4)), 0, WORLD_W - SCREEN_W);
}

static void cameraPlace(tGame *g) {
	// The scroll speed may change by 1 px a frame at most (2 -> 0 at the world's
	// edge was a visible bump every trip home: the base is beside it). Near an
	// edge it slows in time: 1 + 2 + ... + s px are needed to stop from s.
	int16_t want = (int16_t)(cameraWanted(g) - g->cam);
	int16_t prev = g->camStep;
	int16_t s = clampw(want, (int16_t)(prev - 1), (int16_t)(prev + 1));
	// A long way to go (the new chopper at the base after a crash): a pan that
	// speeds up 1 px a frame but never faster than it can stop from in the
	// distance left (v + (v-1) + ... + 1 <= d), so it arrives without
	// overshooting. Flying never gets this far from the camera.
	int16_t aw = want < 0 ? (int16_t)-want : want;
	if(aw > CAM_PAN_FROM) g->camPan = 1;
	if(g->camPan) {
		int16_t v = prev < 0 ? (int16_t)-prev : prev;
		if((prev < 0) != (want < 0)) v = 0;            // (a pan never reverses)
		++v;
		while(v > 1 && (int16_t)((uint16_t)((uint16_t)v * (uint16_t)(v + 1)) >> 1) > aw) --v;
		if(v > CAM_PAN_VMAX) v = CAM_PAN_VMAX;
		if(v > aw) v = aw;
		s = want < 0 ? (int16_t)-v : v;
		if(aw <= 1) g->camPan = 0;                     // arrived: follow again
	}
	int16_t as = s < 0 ? (int16_t)-s : s;
	int16_t room = s > 0 ? (int16_t)(WORLD_W - SCREEN_W - g->cam) : (int16_t)g->cam;
	if(as > 1 && room < (int16_t)((uint16_t)((uint16_t)as * (uint16_t)(as + 1)) >> 1)) {
		s = (int16_t)(s > 0 ? s - 1 : s + 1);   // brake for the edge
	}
	g->cam = clampw((int16_t)(g->cam + s), 0, WORLD_W - SCREEN_W);
	g->camStep = s;
}

void logicCameraSnap(tGame *g) {
	g->camLead = leadTarget(g);
	g->camLeadV = 0;
	g->cam = cameraWanted(g);
	g->camStep = 0;
	g->camPan = 0;
}

void logicCameraUpdate(tGame *g) {
	// The look-ahead moves like a body: it speeds up by CAM_LEAD_ACCEL a frame
	// up to CAM_LEAD_VMAX, and starts slowing when the distance left is what it
	// needs to stop (v^2 / 2a), so it arrives without a bump.
	int16_t d = (int16_t)(leadTarget(g) - g->camLead);
	int16_t v = g->camLeadV;
	int16_t s = d > 0 ? 1 : d < 0 ? -1 : 0;
	int16_t av = v < 0 ? (int16_t)-v : v;
	// v^2 / 2a with a = 2: a 16x16 multiply (MULU.W) and a shift, av <= 32
	int16_t stopDist = (int16_t)((uint16_t)((uint16_t)av * (uint16_t)(av + CAM_LEAD_ACCEL)) >> 2);
	uint8_t isToward = (v > 0 && s > 0) || (v < 0 && s < 0);
	if(s == 0 || (isToward && absw(d) <= stopDist)) {
		// arriving (or there): slow down, but never past the target
		if(v > 0) v = (int16_t)(v > CAM_LEAD_ACCEL ? v - CAM_LEAD_ACCEL : 0);
		else if(v < 0) v = (int16_t)(v < -CAM_LEAD_ACCEL ? v + CAM_LEAD_ACCEL : 0);
	}
	else {
		v = clampw((int16_t)(s > 0 ? v + CAM_LEAD_ACCEL : v - CAM_LEAD_ACCEL), -CAM_LEAD_VMAX, CAM_LEAD_VMAX);
	}
	if(absw(d) <= absw(v) && absw(v) <= CAM_LEAD_ACCEL) {
		g->camLead = (int16_t)(g->camLead + d);   // the last step lands exactly
		v = 0;
	}
	else {
		g->camLead = (int16_t)(g->camLead + v);
	}
	g->camLeadV = v;
	cameraPlace(g);
}

// -------------------------------------------------------------- update ---

static void worldUpdate(tGame *g) {
	barracksUpdate(g);
	hostagesUpdate(g);
	shotsUpdate(g);
	eshotsUpdate(g);
	tanksUpdate(g);
	jetsUpdate(g);
	minesUpdate(g);
	boomsUpdate(g);
}

static uint8_t missionDone(const tGame *g) {
	if(g->saved + g->killed < HOSTAGE_COUNT) return 0;
	return 1;
}

void logicUpdate(tGame *g, const tInput *pIn) {
	++g->frame;
	weatherUpdate(g);
	g->events = 0;
	g->evBarracks = 0;
	if(g->stateTimer) --g->stateTimer;
	switch(g->state) {
		case GS_TITLE:
			if(!pIn->fire) g->titleArmed = 1;
			else if(g->titleArmed && !g->stateTimer) {
				g->titleArmed = 0;
				logicStartGame(g);
			}
			// the camera drifts over the land
			if(g->frame & 1) {
				g->cam = (int16_t)(g->cam - 1);
				if(g->cam <= 0) g->cam = WORLD_W - SCREEN_W;
			}
			break;
		case GS_INTRO:
			g->chop.prevFire = pIn->fire;
			g->chop.prevFire2 = pIn->fire2;
			logicCameraUpdate(g);
			if(!g->stateTimer) setState(g, GS_PLAY, 0);
			break;
		case GS_PLAY:
			logicChopperUpdate(g, pIn);
			unloadUpdate(g);
			enemiesSpawn(g);
			worldUpdate(g);
			logicCameraUpdate(g);
			if(g->state == GS_PLAY && missionDone(g) && !g->aboard) {
				// wait for the last ones to reach the hangar
				uint8_t isWalking = 0;
				for(uint8_t k = 0; k < g->outCount; ++k) {
					if(g->pHostages[g->pOut[k]].state == HS_TO_HANGAR) { isWalking = 1; break; }
				}
				if(!isWalking) {
					addScore(g, (uint16_t)(g->saved * 10));
					setState(g, GS_CLEAR, CLEAR_FRAMES);
				}
			}
			break;
		case GS_LOST:
			chopperWreckUpdate(g);
			worldUpdate(g);
			if(!g->stateTimer) {
				if(!g->lives) setState(g, GS_OVER, OVER_FRAMES);
				else if(missionDone(g)) setState(g, GS_CLEAR, CLEAR_FRAMES);
				else {
					logicChopperRespawn(g);
					// clear the air near the base
					for(uint8_t i = 0; i < ESHOT_MAX; ++i) g->pEShots[i].active = 0;
					for(uint8_t i = 0; i < JET_MAX; ++i) { g->pJets[i].active = 0; g->pJets[i].cool = 150; }
					for(uint8_t i = 0; i < MINE_MAX; ++i) g->pMines[i].active = 0;
					setState(g, GS_PLAY, 0);
				}
			}
			else logicCameraUpdate(g);
			break;
		case GS_CLEAR:
			boomsUpdate(g);
			if(!g->stateTimer) {
				if(g->mission + 1 < MISSION_COUNT) logicStartMission(g, (uint8_t)(g->mission + 1));
				else setState(g, GS_VICTORY, OVER_FRAMES);
			}
			break;
		case GS_OVER:
		case GS_VICTORY:
			boomsUpdate(g);
			if(!g->stateTimer || (pIn->fire && g->stateTimer < OVER_FRAMES - 100)) {
				setState(g, GS_TITLE, 50);
				g->titleArmed = 0;
			}
			break;
	}
}

// --------------------------------------------------------------- drawing ---

uint8_t logicChopperVisible(const tGame *g) {
	return g->state != GS_TITLE && !(g->state == GS_LOST && !g->chop.alive && g->chop.crashTimer == 0);
}

uint8_t logicChopperFrame(const tGame *g) {
	const tChopper *c = &g->chop;
	if(!c->alive) return CHOP_WRECK | (c->lastSide == FACE_LEFT ? CHOP_LEFT : 0);
	if(c->turnTimer) {
		// between a side and the front: the three-quarter view of that side
		uint8_t side = c->face != FACE_FRONT ? c->face :
			(c->faceTarget == FACE_FRONT ? c->lastSide : c->faceTarget);
		return CHOP_TURN | (side == FACE_LEFT ? CHOP_LEFT : 0);
	}
	if(c->face == FACE_FRONT) return CHOP_FRONT;
	int16_t fwd = c->face == FACE_RIGHT ? c->vx : (int16_t)-c->vx;
	uint8_t pose = fwd >= 32 ? CHOP_SIDE2 : fwd >= 12 ? CHOP_SIDE1 : CHOP_SIDE0;
	return pose | (c->face == FACE_LEFT ? CHOP_LEFT : 0);
}

uint8_t logicRotorPhase(const tGame *g) {
	if(!g->chop.alive) return 0;
	return (uint8_t)((g->frame >> 1) & 1);
}

static inline uint8_t onScreen(int16_t sx, uint8_t w) {
	return sx > -(int16_t)w && sx < SCREEN_W;
}

static uint8_t drawAdd(tDraw *pOut, uint8_t n, int16_t sx, int16_t sy, uint8_t bob, uint8_t frame) {
	if(n >= DRAW_MAX || !onScreen(sx, g_pBobW[bob])) return n;
	pOut[n].x = sx;
	pOut[n].y = sy;
	pOut[n].bob = bob;
	pOut[n].frame = frame;
	return (uint8_t)(n + 1);
}

uint8_t logicDraw(const tGame *g, tDraw *pOut) {
	uint8_t n = 0;
	int16_t cam = g->cam;
	// structures
	n = drawAdd(pOut, n, (int16_t)(HANGAR_X - cam), FEET_Y - 40, BOB_HANGAR, 0);
	n = drawAdd(pOut, n, (int16_t)(BASE_PAD_X0 - 20 - cam), FEET_Y - 24, BOB_FLAG, (uint8_t)((g->frame >> 4) & 1));
	n = drawAdd(pOut, n, (int16_t)(BORDER_X - 8 - cam), FEET_Y - 32, BOB_FENCE, 0);
	for(uint8_t b = 0; b < BARRACKS_COUNT; ++b) {
		const tBarracks *pB = &g->pBarracks[b];
		uint8_t f = pB->hp == 2 ? BARRACKS_OK : pB->hp == 1 ? BARRACKS_HIT :
			(uint8_t)(BARRACKS_OPEN0 + ((g->frame >> 3) & 1));
		n = drawAdd(pOut, n, (int16_t)(pB->x - 24 - cam), FEET_Y - 28, BOB_BARRACKS, f);
	}
	// the chopper's shadow, on the track under it (and under the people)
	if(logicChopperVisible(g)) {
		static const uint8_t pSize[13] = {0, 0, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3};   // by altitude / 16
		int16_t wAlt = (int16_t)(GROUND_Y - g->chop.y);
		if(wAlt < 0) wAlt = 0;
		if(wAlt > 12 * 16) wAlt = 12 * 16;
		int16_t sx = (int16_t)(g->chop.x - cam - 16 + (wAlt >> 4));   // the low sun pushes it right
		n = drawAdd(pOut, n, sx, SHADOW_Y, BOB_SHADOW, (uint8_t)((pSize[wAlt >> 4] << 1) | (sx & 1)));
	}
	// ground units
	for(uint8_t i = 0; i < TANK_MAX; ++i) {
		const tTank *t = &g->pTanks[i];
		if(!t->active) continue;
		uint8_t f = t->tell ? (uint8_t)(((t->tell & 4) ? TANK_FLASH_L : TANK_AIM_L) + (t->dir > 0)) :
			t->fireAnim ? (t->dir < 0 ? TANK_FIRE_L : TANK_FIRE_R) :
			(uint8_t)((t->dir < 0 ? TANK_L0 : TANK_R0) + ((t->anim >> 2) & 1));
		n = drawAdd(pOut, n, (int16_t)(t->x - 16 - cam), FEET_Y - 14, BOB_TANK, f);
	}
	for(uint8_t k = 0; k < g->outCount; ++k) {
		const tHostage *h = &g->pHostages[g->pOut[k]];
		uint8_t f;
		if(h->state == HS_FREE || h->state == HS_TO_CHOPPER || h->state == HS_TO_HANGAR) f = h->anim;
		else if(h->state == HS_DEAD && h->timer) f = MAN_DEAD;
		else continue;
		n = drawAdd(pOut, n, (int16_t)(h->x - 8 - cam), FEET_Y - 12, BOB_HOSTAGE, f);
	}
	// air
	for(uint8_t i = 0; i < JET_MAX; ++i) {
		const tJet *j = &g->pJets[i];
		if(j->active) n = drawAdd(pOut, n, (int16_t)(j->x - 16 - cam), (int16_t)(j->y - 6), BOB_JET, j->dir < 0 ? JET_L : JET_R);
	}
	for(uint8_t i = 0; i < MINE_MAX; ++i) {
		const tMine *m = &g->pMines[i];
		if(m->active) n = drawAdd(pOut, n, (int16_t)(m->x - 8 - cam), (int16_t)(m->y - 6), BOB_MINE, (uint8_t)((m->anim >> 3) & 1));
	}
	for(uint8_t i = 0; i < SHOT_MAX; ++i) {
		const tShot *s = &g->pShots[i];
		if(!s->active) continue;
		if(s->kind == SHOT_CANNON) n = drawAdd(pOut, n, (int16_t)(s->x - 4 - cam), (int16_t)(s->y - 1), BOB_SHOT, 0);
		else n = drawAdd(pOut, n, (int16_t)(s->x - 2 - cam), (int16_t)(s->y - 3), BOB_BOMB, 0);
	}
	for(uint8_t i = 0; i < ESHOT_MAX; ++i) {
		const tShot *s = &g->pEShots[i];
		if(!s->active) continue;
		if(s->kind == ESHOT_MISSILE) n = drawAdd(pOut, n, (int16_t)(s->x - 4 - cam), (int16_t)(s->y - 1), BOB_SHOT, 0);
		else n = drawAdd(pOut, n, (int16_t)(s->x - 2 - cam), (int16_t)(s->y - 2), BOB_SHELL, 0);
	}
	for(uint8_t i = 0; i < BOOM_MAX; ++i) {
		const tBoom *b = &g->pBooms[i];
		if(b->active) n = drawAdd(pOut, n, (int16_t)(b->x - 16 - cam), (int16_t)(b->y - 16), BOB_BOOM, b->frame);
	}
	return n;
}

// ------------------------------------------------------------ scrolling ---

uint16_t logicBandOffset(int16_t wCam, uint8_t ubShift, uint16_t uwLoopW) {
	uint16_t x = (uint16_t)wCam >> ubShift;
	while(x >= uwLoopW) x = (uint16_t)(x - uwLoopW);
	return x;
}

uint16_t logicScrollDelay(uint16_t uwX) {
	// The fetch starts one word early (DDFSTRT 0x30), shifted right by the
	// delay: pixel x is at the left edge with a delay of 16 - x % 16.
	return (uint16_t)((16 - (uwX & 15)) & 15);
}

int16_t logicScrollByteOffset(uint16_t uwX) {
	return (int16_t)((((int16_t)uwX - 1) >> 4) * 2);
}

// ----------------------------------------------------------------- colours ---

static uint16_t mix(uint16_t a, uint16_t b, uint16_t t, uint16_t n) {
	// a + (b - a) * t / n per channel (startup only)
	uint16_t out = 0;
	for(uint8_t s = 0; s < 12; s = (uint8_t)(s + 4)) {
		int16_t ca = (int16_t)((a >> s) & 15), cb = (int16_t)((b >> s) & 15);
		int16_t c = (int16_t)(ca + ((cb - ca) * (int16_t)t + (int16_t)(n / 2)) / (int16_t)n);
		out |= (uint16_t)(c << s);
	}
	return out;
}

uint16_t logicSkyColor(uint8_t ubMission, uint8_t ubLine) {
	if(ubLine >= SKY_LINES) ubLine = SKY_LINES - 1;
	return mix(s_pSkyTop[ubMission], s_pSkyLow[ubMission], ubLine, SKY_LINES - 1);
}

#if defined(__GNUC__) && !defined(__clang__)
__attribute__((optimize("Os")))   // (A1200 startup only: small beats fast on the A500's full slow RAM)
#endif
uint32_t logicSkyColor24(uint8_t ubMission, uint8_t ubLine) {
	// the same gradient between the same two colours (x17 per channel), in
	// 8 bits a channel, rounded to nearest (A1200 startup only: 32-bit maths)
	if(ubLine >= SKY_LINES) ubLine = SKY_LINES - 1;
	const int32_t n = SKY_LINES - 1;
	uint32_t ulOut = 0;
	for(uint8_t s = 0; s < 12; s = (uint8_t)(s + 4)) {
		int32_t a = (int32_t)((s_pSkyTop[ubMission] >> s) & 15) * 17;
		int32_t b = (int32_t)((s_pSkyLow[ubMission] >> s) & 15) * 17;
		int32_t num = (b - a) * (int32_t)ubLine;
		int32_t c = a + (num >= 0 ? (num + n / 2) / n : -((-num + n / 2) / n));
		ulOut |= (uint32_t)c << (2 * s);
	}
	return ulOut;
}

const char *logicRank(uint16_t uwSaved) {
	if(uwSaved >= 180) return "LEGEND";
	if(uwSaved >= 140) return "HERO";
	if(uwSaved >= 90) return "CAPTAIN";
	if(uwSaved >= 40) return "PILOT";
	return "ROOKIE";
}
