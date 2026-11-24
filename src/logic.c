// ROTOR RESCUE - game rules (portable C, tested on the host by agk unit).
// (c) 2026 Desert Palms Software. Game by Paul Ross Seibert.

#include "logic.h"
#include "world3_map.h"   // world 3's pilot map and its scenery (art/tools/make_world3_disk.py)

// Code that runs between games (menus, the hall of fame): small, not inlined
// into the per-frame update (the A500's slow RAM is nearly full)
#if defined(__GNUC__) && !defined(__clang__)
#define COLD __attribute__((noinline, optimize("Os")))
#else
#define COLD __attribute__((noinline))
#endif

const uint8_t g_pBobW[BOB_TYPES] = {
	[BOB_HANGAR] = 64, [BOB_FENCE] = 16, [BOB_BARRACKS] = 48, [BOB_TANK] = 32,
	[BOB_HOSTAGE] = 16, [BOB_JET] = 32, [BOB_MINE] = 16, [BOB_SHOT] = 16,
	[BOB_BOMB] = 16, [BOB_SHELL] = 16, [BOB_BOOM] = 32, [BOB_FLAG] = 16,
	[BOB_SHADOW] = 32, [BOB_SMOKE] = 16,
	[BOB_W2_BARRACKS] = 51, [BOB_PATROL] = 43, [BOB_GUNBOAT] = 61, [BOB_PAD48] = 48,
	[BOB_PAD64] = 64, [BOB_PAD80] = 80, [BOB_PAD96] = 96, [BOB_PAD_LAMP] = 5, [BOB_SPLASH] = 24, [BOB_W2_JET] = 32,
	[BOB_W3_LODGE] = 52, [BOB_W3_NEST] = 30,
};
const uint8_t g_pBobH[BOB_TYPES] = {
	[BOB_HANGAR] = 40, [BOB_FENCE] = 32, [BOB_BARRACKS] = 28, [BOB_TANK] = 14,
	[BOB_HOSTAGE] = 12, [BOB_JET] = 12, [BOB_MINE] = 12, [BOB_SHOT] = 3,
	[BOB_BOMB] = 6, [BOB_SHELL] = 4, [BOB_BOOM] = 32, [BOB_FLAG] = 24,
	[BOB_SHADOW] = 4, [BOB_SMOKE] = 8,
	[BOB_W2_BARRACKS] = 31, [BOB_PATROL] = 17, [BOB_GUNBOAT] = 30, [BOB_PAD48] = PAD_ROWS,
	[BOB_PAD64] = PAD_ROWS, [BOB_PAD80] = PAD_ROWS, [BOB_PAD96] = PAD_ROWS, [BOB_PAD_LAMP] = 5, [BOB_SPLASH] = 24, [BOB_W2_JET] = 12,
	[BOB_W3_LODGE] = 32, [BOB_W3_NEST] = 24,
};
// (world 2's frames are the ones the floppy set carries: art/tools/make_world_disk.py picks
// them from the art: one wake frame a facing and the patrol boat's tell a facing, the gunboat's fire frame is its second tell's,
// the pad's lit phase only)
const uint8_t g_pBobFrames[BOB_TYPES] = {
	[BOB_HANGAR] = 1, [BOB_FENCE] = 1, [BOB_BARRACKS] = 4, [BOB_TANK] = 10,
	[BOB_HOSTAGE] = 8, [BOB_JET] = 4, [BOB_MINE] = 2, [BOB_SHOT] = 1,
	[BOB_BOMB] = 1, [BOB_SHELL] = 1, [BOB_BOOM] = 5, [BOB_FLAG] = 2,
	[BOB_SHADOW] = SHADOW_SIZES * 2, [BOB_SMOKE] = 3,
	[BOB_W2_BARRACKS] = 4, [BOB_PATROL] = 4, [BOB_GUNBOAT] = 6, [BOB_PAD48] = 1,
	[BOB_PAD64] = 1, [BOB_PAD80] = 1, [BOB_PAD96] = 1, [BOB_PAD_LAMP] = 2, [BOB_SPLASH] = 5, [BOB_W2_JET] = 4,
	[BOB_W3_LODGE] = 3, [BOB_W3_NEST] = 3,   // (the lodge: intact, shot, burning)
};

// World 2's islands (logic.h): the tile map's "island" lines (art/world2/tilemap.txt), px
const tIsland g_pIslands[W2_ISLANDS] = {
	{64, 512, 160, 256, 64}, {1056, 1664, 1232, 1392, 64}, {1920, 2400, 2080, 2220, 56},
	{2944, 3584, 3120, 3312, 96}, {4000, 4576, 0, 4136, 80},
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
	if((g->state != GS_PLAY && g->state != GS_LOST) || g->world) return 0;   // (world 2's weather: the gameplay slice)
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
static void rotorTick(tGame *g);

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
	// an invincible game or the demo doesn't count for the hi-score
	if(g->score > g->hiScore && g->difficulty != DIFFICULTY_INVINCIBLE && !g->demo) g->hiScore = g->score;
}

// A tank destroyed: it pays 50 for the first TANK_QUOTA of a mission, nothing
// after (tanks respawn on a timer: without the quota they were a score tap)
static void tankKilled(tGame *g) {
	if(g->tankKills < 255) ++g->tankKills;
	if(g->tankKills <= TANK_QUOTA) {
		addScore(g, 50);
		if(g->tankKills == TANK_QUOTA) g->quotaMsg = QUOTA_MSG_FRAMES;
	}
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
	// (the title, the hall of fame and a new entry are world 1's: main.c loads its art back)
	if(ubState == GS_TITLE || ubState == GS_SCORES || ubState == GS_ENTRY) g->world = 0;
	g->state = ubState;
	g->stateTimer = uwTimer;
	g->events |= EV_STATE;
}

// ------------------------------------------------------------- setup ---

COLD void logicInit(tGame *g) {
	uint8_t *p = (uint8_t *)g;
	for(uint16_t i = 0; i < sizeof(*g); ++i) p[i] = 0;
	g->lfsr = 0xACE1;
	g->lives = LIVES_START;
	g->state = GS_TITLE;
	g->cam = START_X - SCREEN_W / 2;
	logicStartMission(g, 0);
	g->state = GS_TITLE;
	g->stateTimer = 0;
	g->difficulty = DIFFICULTY_NORMAL;
	g->newRank = HI_COUNT;
	logicScoresDefault(g);
}

// ------------------------------------------------------------ hall of fame ---

// The file keeps the table it always had: "RRH1", per entry the initials,
// mission, hostages saved (2), score (4), a 16-bit sum: an old disk's table
// reads as it is (tests/unit/test_logic.c has one)
const tHiConfig g_sHiConfig = {"RRH1", MISSIONS_TOTAL + 1, "rrscores"};

COLD void logicScoresDefault(tGame *g) {
	static const tHiScore pDefaults[HI_COUNT] = {
		{"DPS", 3, 150, 5000}, {"PRS", 2, 120, 4000}, {"ACE", 2, 90, 3000},
		{"AGK", 1, 60, 2000}, {"RTR", 1, 30, 1000},
	};
	hiDefaults(&g->hi, pDefaults);
	g->hiScore = hiTop(&g->hi);
}

uint8_t logicIsMenu(const tGame *g) {
	return g->state == GS_TITLE || g->state == GS_SCORES;
}

// After a game: the hall of fame if the score made it (not invincible, not the demo)
COLD static void gameEnded(tGame *g) {
	uint8_t r = hiQualify(&g->hi, g->score, g->difficulty == DIFFICULTY_INVINCIBLE, g->demo);
	if(r >= HI_COUNT) {
		setState(g, GS_TITLE, 50);
		g->titleArmed = 0;
		return;
	}
	r = hiInsert(&g->hi, g->score, (uint8_t)(g->state == GS_VICTORY ? MISSIONS_TOTAL + 1 : g->world * MISSION_COUNT + g->mission + 1), g->savedTotal);
	g->hiScore = hiTop(&g->hi);
	g->newRank = r;
	hiEditBegin(&g->edit, r, ENTRY_FRAMES);   // (fire held from the end screen doesn't choose a letter)
	setState(g, GS_ENTRY, ENTRY_FRAMES);
}

// The initials: the stick (up/down a letter, fire the next, left back one) and
// the keyboard (A-Z, 0-9, Space, Backspace, Return), either or both
COLD static void entryUpdate(tGame *g, const tInput *pIn) {
	uint8_t isKeyDone = hiEditKey(&g->edit, &g->hi, pIn->key ? pIn->key : HI_KEY_NONE);
	uint8_t isStickDone = hiEditUpdate(&g->edit, &g->hi, pIn->dx, pIn->dy, pIn->fire);
	if(isKeyDone || isStickDone) {
		g->scoresChanged = 1;
		g->scoresNext = GS_TITLE;
		g->titleArmed = 0;
		setState(g, GS_SCORES, SCORES_FRAMES);
	}
}

// ------------------------------------------------------------ the demo ---
// A recorded trip (the inputs of tests/rescue.agk): from the base to the
// nearest barracks, shoot it open, take 16 aboard, fly them home. Runs of
// {frames, input bits}: 1 left, 2 right, 4 up, 8 down, 16 fire.
#define DI_L 1
#define DI_R 2
#define DI_U 4
#define DI_D 8
#define DI_F 16
static const uint8_t s_pDemo[][2] = {
	{126, 0}, {30, DI_U}, {170, DI_L}, {60, 0}, {70, DI_D}, {1, DI_F}, {25, 0}, {1, DI_F},
	{250, 0}, {70, 0}, {30, DI_U}, {180, DI_R}, {50, 0}, {70, DI_D}, {250, 0}, {70, 0},
};
#define DEMO_RUNS (sizeof(s_pDemo) / sizeof(s_pDemo[0]))

COLD void logicStartDemo(tGame *g) {
	g->demoDifficulty = g->difficulty;
	g->difficulty = DIFFICULTY_NORMAL;
	g->lfsr = 0xACE1;     // as at boot: the recording's game, frame for frame
	g->frame = 11;        // (its first frame's number: the same odd and even frames)
	logicStartGame(g);
	g->demo = 1;
	g->demoRun = 0;
	g->demoLeft = s_pDemo[0][0];
}

uint8_t logicDemoInput(tGame *g, tInput *pOut) {
	if(g->demoRun >= DEMO_RUNS) return 0;
	uint8_t b = s_pDemo[g->demoRun][1];
	pOut->dx = (int8_t)((b & DI_L) ? -1 : (b & DI_R) ? 1 : 0);
	pOut->dy = (int8_t)((b & DI_U) ? -1 : (b & DI_D) ? 1 : 0);
	pOut->fire = (uint8_t)((b & DI_F) != 0);
	pOut->fire2 = 0;
	if(!--g->demoLeft && ++g->demoRun < DEMO_RUNS) g->demoLeft = s_pDemo[g->demoRun][0];
	return 1;
}

COLD static void demoEnd(tGame *g) {
	g->demo = 0;
	g->difficulty = g->demoDifficulty;
	g->score = 0;
	logicStartMission(g, 0);   // a fresh battlefield behind the title
	setState(g, GS_TITLE, 25);
	g->titleArmed = 0;
}

COLD void logicChopperRespawn(tGame *g) {
	tChopper *c = &g->chop;
	uint8_t *p = (uint8_t *)c;
	for(uint16_t i = 0; i < sizeof(*c); ++i) p[i] = 0;
	c->x = logicIsW3(g) ? W3_START_X : logicIsW2(g) ? W2_START_X : START_X;
	c->y = logicIsW3(g) ? logicW3FloorAt(c->x) : GROUND_Y;   // (world 3: on its pad bed)
	c->landed = 1;
	c->alive = 1;
	c->face = FACE_LEFT;
	c->faceTarget = FACE_LEFT;
	c->lastSide = FACE_LEFT;
	c->prevFire = 1;   // a held fire from the menu doesn't shoot
	c->prevFire2 = 1;
	g->unloadTimer = UNLOAD_FRAMES;
}

// World 2's boats, placed by the map (docs/CONTENT-PLAN.md 4.2: a garrison is fixed and seen
// from afar): a stretch of sea each, the armed ones from the skill that meets them. A boat
// keeps its slot whether it is there or not (a lower skill leaves the slot empty).
COLD static void padsSet(tGame *g);
typedef struct { int16_t x0, x1; uint8_t kind, minSkill; } tBoatPlace;
static const tBoatPlace s_pBoatPlace[MISSION_COUNT][BOAT_MAX] = {
	{{560, 980, BOAT_PATROL, 0}, {1690, 1890, BOAT_GUN, 1}, {2450, 2900, BOAT_GUN, 2}, {3620, 3960, BOAT_PATROL, 0}},
	{{560, 1000, BOAT_GUN, 1}, {1690, 1890, BOAT_GUN, 1}, {2450, 2680, BOAT_GUN, 1}, {2680, 2900, BOAT_GUN, 3},
		{3620, 3960, BOAT_PATROL, 0}},
	{{540, 1020, BOAT_GUN, 1}, {1680, 1900, BOAT_GUN, 1}, {2440, 2680, BOAT_GUN, 1}, {2680, 2910, BOAT_GUN, 2},
		{560, 780, BOAT_GUN, 4}, {3620, 3960, BOAT_PATROL, 0}},
};

COLD static void boatsPlace(tGame *g) {
	g->atkWait = 0;
	g->atkLast = 0;
	g->wet = 0;
	g->splashTimer = 0;
	uint8_t ubSkill = g->difficulty ? g->difficulty : DIFFICULTY_NORMAL;   // (GODMODE plays as normal)
	for(uint8_t i = 0; i < BOAT_MAX; ++i) {
		tBoat *b = &g->pBoats[i];
		const tBoatPlace *p = &s_pBoatPlace[g->mission][i];
		b->active = logicIsW2(g) && p->x1 && ubSkill >= p->minSkill;
		if(!b->active) continue;
		b->kind = p->kind;
		b->x0 = p->x0;
		b->x1 = p->x1;
		b->x = (int16_t)((p->x0 + p->x1) >> 1);
		b->dir = -1;
		b->hp = p->kind == BOAT_GUN ? 3 : 2;
		b->tell = 0;
		b->fireAnim = 0;
		b->cool = (uint8_t)(60 + i * 30);   // (a quiet start)
		b->phase = 0;
	}
}

COLD void logicStartMission(tGame *g, uint8_t ubMission) {
	g->mission = ubMission;
	g->trip = 0;
	g->tankKills = 0;
	g->quotaMsg = 0;
	for(uint8_t i = 0; i < SMOKE_MAX; ++i) g->pSmoke[i].active = 0;
	g->aboard = 0;
	g->saved = 0;
	g->killed = 0;
	g->spawnTimer = (uint16_t)(700 - ubMission * 150);   // a quiet start
	g->wxFall = 0x8000;
	g->wxDrift = 0;
	// (world 2's barracks stand on its four islands, the people wander about half way to the pad)
	for(uint8_t b = 0; b < BARRACKS_COUNT; ++b) {
		g->pBarracks[b].x = logicIsW3(g) ? (int16_t)-1000 : logicIsW2(g) ? g_pIslands[b].barracksX : s_pBarracksX[b];   // (world 3's pilot: no barracks)
		g->pBarracks[b].wx = logicIsW2(g) ? (int16_t)((g_pIslands[b].barracksX + g_pIslands[b].padX) >> 1) : g->pBarracks[b].x;
		g->pBarracks[b].hp = 2;
		g->pBarracks[b].released = 0;
		g->pBarracks[b].timer = 0;
	}
	for(uint8_t i = 0; i < HOSTAGE_COUNT; ++i) {
		tHostage *h = &g->pHostages[i];
		h->home = (int8_t)(i / HOSTAGES_PER_BARRACKS);
		h->state = HS_INSIDE;
		h->x = g->pBarracks[h->home].x;
		h->anim = 0;
		h->timer = 0;
		h->dir = 1;
	}
	g->outCount = 0;
	for(uint8_t i = 0; i < TANK_MAX; ++i) g->pTanks[i].active = 0;
	for(uint8_t i = 0; i < JET_MAX; ++i) { g->pJets[i].active = 0; g->pJets[i].cool = (uint16_t)((logicIsW2(g) ? W2_JET_FIRST : 100) + i * 120); }
	g->landFrames = 0; g->airGrace = 0; g->jetWait = 0; g->jetsTrip = 0;
	for(uint8_t i = 0; i < MINE_MAX; ++i) g->pMines[i].active = 0;
	for(uint8_t i = 0; i < SHOT_MAX; ++i) g->pShots[i].active = 0;
	for(uint8_t i = 0; i < ESHOT_MAX; ++i) g->pEShots[i].active = 0;
	for(uint8_t i = 0; i < BOOM_MAX; ++i) g->pBooms[i].active = 0;
	boatsPlace(g);
	padsSet(g);
	logicChopperRespawn(g);
	logicCameraSnap(g);
	setState(g, GS_INTRO, INTRO_FRAMES);
}

COLD void logicStartGame(tGame *g) {
	g->lives = LIVES_START;
	g->score = 0;
	g->savedTotal = 0;
	g->world = 0;
	logicStartMission(g, 0);
}

COLD void logicStartAt(tGame *g, uint8_t ubWorld, uint8_t ubMission) {
	if(g->demo || ubWorld >= WORLD_DEBUG_COUNT || ubMission >= MISSION_COUNT) return;
	if(logicIsMenu(g) || g->state == GS_ENTRY || g->state == GS_OVER || g->state == GS_VICTORY) {
		g->lives = LIVES_START;
		g->score = 0;
		g->savedTotal = 0;
		g->titleArmed = 0;
	}
	g->world = ubWorld;
	logicStartMission(g, ubMission);
}

// ---------------------------------------------------------- difficulty ---
// Per difficulty 0..5 (0 invincible: plays like 3, normal). Tables, no maths.

static const int8_t s_pTankExtra[DIFFICULTY_MAX + 1] = {0, -2, -1, 0, 1, 2};     // tanks at once
static const int8_t s_pTankCool[DIFFICULTY_MAX + 1] = {0, 90, 30, 0, -15, -30};  // frames between shells
static const uint8_t s_pTankCoolMin[DIFFICULTY_MAX + 1] = {40, 75, 50, 40, 32, 26};
static const uint8_t s_pAimMask[DIFFICULTY_MAX + 1] = {31, 63, 63, 31, 15, 7};   // aim error: +-(mask+1)/2 px
static const uint8_t s_pJetsFrom[DIFFICULTY_MAX + 1] = {1, 3, 2, 1, 1, 0};       // trips + missions before jets
static const uint8_t s_pMinesFrom[DIFFICULTY_MAX + 1] = {2, 4, 3, 2, 1, 1};      // ... and mines
static const int16_t s_pSpawnExtra[DIFFICULTY_MAX + 1] = {0, 120, 60, 0, -40, -70};
static const int16_t s_pJetRest[DIFFICULTY_MAX + 1] = {0, 100, 60, 0, -30, -50};  // frames between jet passes (jet cool-down is 16 bits: it was 8, and skill 1's 240-303 wrapped)
// a tank's wind-up before each shell (it comes out of the time between shells:
// the rate of fire is the same, the shell is announced)
static const uint8_t s_pTankTell[DIFFICULTY_MAX + 1] = {16, 22, 18, 16, 14, 12};
static const uint8_t s_pBoatTell[DIFFICULTY_MAX + 1] = {16, 22, 18, 16, 14, 12};  // a gunboat's barrel rises and its lamp flashes this long (content plan 4.4: 22 / 18 / 16 / 14 / 12)
// World 2's pad: the widest the skill wants, px (SPEC 4.4: 96 / 80 / 64 / 56 / 48); the island's bed and the 16 px grid cut it down
static const uint8_t s_pPadWant[DIFFICULTY_MAX + 1] = {64, 96, 80, 64, 56, 48};
static const uint8_t s_pJetTell[DIFFICULTY_MAX + 1] = {16, 24, 20, 16, 14, 12};   // a jet's flash before a shot (its own table since the fairness pass: slower to react to on the low skills)
// Jets and the chopper on the ground (Paul's playtest, 2026-11-11; studio tuning): frames it must have
// sat landed, without a break, before a jet may bomb it (65535: never, the jets overfly it)
static const uint16_t s_pLandDwell[DIFFICULTY_MAX + 1] = {300, GROUND_NEVER, GROUND_NEVER, 300, 200, 150};
// How many jets may be out at once: missions 1-2, and mission 3
static const uint8_t s_pJetsMax[DIFFICULTY_MAX + 1] = {1, 1, 1, 1, 2, 2};
static const uint8_t s_pJetsMaxM3[DIFFICULTY_MAX + 1] = {2, 1, 1, 2, 2, 2};
// Jets that may appear in one trip, base pad to base pad (Paul's playtest, 2026-11-12: the far barracks)
static const uint8_t s_pJetsTrip[DIFFICULTY_MAX + 1] = {5, 2, 3, 5, 6, 6};
// World 2's jets (SPEC 6, "Jets in world 2"; Paul's playtest, 2026-11-14: mission 4 at skill 3 had no airplanes; decided
// 2026-11-17: mission 4 at skill 3-5 from the first trip, the first jet late in it, W2_JET_FIRST), by the
// mission within the world (4 / 5 / 6) and the skill. The trip they begin on (a trip is one delivery home: 255 = none
// at this skill, so skill 1-2 in mission 4 and skill 1 in mission 5 stay gentle) and the jets that may appear in a trip
static const uint8_t s_pW2JetsFrom[3][DIFFICULTY_MAX + 1] = {
	{0, 255, 255, 0, 0, 0},
	{0, 255, 2, 0, 0, 0},
	{0, 3, 1, 0, 0, 0},
};
static const uint8_t s_pW2JetsTrip[3][DIFFICULTY_MAX + 1] = {
	{3, 0, 0, 3, 4, 5},
	{4, 0, 2, 4, 5, 6},
	{5, 1, 3, 5, 6, 6},
};

// Every per-difficulty table, for the unit test that index 0 (GODMODE) equals
// index 3 (normal) in each. A new table goes in this list.
static const tDiffTable s_pDiffTables[] = {
	{"tankExtra", s_pTankExtra, sizeof(s_pTankExtra[0])},
	{"tankCool", s_pTankCool, sizeof(s_pTankCool[0])},
	{"tankCoolMin", s_pTankCoolMin, sizeof(s_pTankCoolMin[0])},
	{"aimMask", s_pAimMask, sizeof(s_pAimMask[0])},
	{"jetsFrom", s_pJetsFrom, sizeof(s_pJetsFrom[0])},
	{"minesFrom", s_pMinesFrom, sizeof(s_pMinesFrom[0])},
	{"spawnExtra", s_pSpawnExtra, sizeof(s_pSpawnExtra[0])},
	{"jetRest", s_pJetRest, sizeof(s_pJetRest[0])},
	{"tankTell", s_pTankTell, sizeof(s_pTankTell[0])},
	{"jetTell", s_pJetTell, sizeof(s_pJetTell[0])},
	{"boatTell", s_pBoatTell, sizeof(s_pBoatTell[0])},
	{"padWant", s_pPadWant, sizeof(s_pPadWant[0])},
	{"landDwell", s_pLandDwell, sizeof(s_pLandDwell[0])},
	{"jetsMax", s_pJetsMax, sizeof(s_pJetsMax[0])},
	{"jetsMaxM3", s_pJetsMaxM3, sizeof(s_pJetsMaxM3[0])},
	{"jetsTrip", s_pJetsTrip, sizeof(s_pJetsTrip[0])},
	{"w2JetsFromM4", s_pW2JetsFrom[0], sizeof(s_pW2JetsFrom[0][0])},
	{"w2JetsFromM5", s_pW2JetsFrom[1], sizeof(s_pW2JetsFrom[0][0])},
	{"w2JetsFromM6", s_pW2JetsFrom[2], sizeof(s_pW2JetsFrom[0][0])},
	{"w2JetsTripM4", s_pW2JetsTrip[0], sizeof(s_pW2JetsTrip[0][0])},
	{"w2JetsTripM5", s_pW2JetsTrip[1], sizeof(s_pW2JetsTrip[0][0])},
	{"w2JetsTripM6", s_pW2JetsTrip[2], sizeof(s_pW2JetsTrip[0][0])},
};

const tDiffTable *logicDiffTables(uint8_t *pCount) {
	*pCount = (uint8_t)(sizeof(s_pDiffTables) / sizeof(s_pDiffTables[0]));
	return s_pDiffTables;
}

void logicSetDifficulty(tGame *g, uint8_t ubLevel) {
	if(ubLevel > DIFFICULTY_MAX) return;
	g->difficulty = ubLevel;
	padsSet(g);
}

// (world 2: no enemies in its display slice; the gameplay slice gives it its own)
uint8_t logicTankLimit(const tGame *g) {
	if(g->world) return 0;
	int16_t n = (int16_t)(1 + g->trip + g->mission + s_pTankExtra[g->difficulty]);
	return (uint8_t)(n < 1 ? 1 : n > TANK_MAX ? TANK_MAX : n);
}

uint8_t logicJetsOn(const tGame *g) {
	if(logicIsW3(g)) return 0;   // (the pilot has no jets)
	if(g->world) {
		uint8_t ubFrom = s_pW2JetsFrom[g->mission][g->difficulty];
		return ubFrom != 255 && g->trip >= ubFrom;
	}
	return g->trip + g->mission >= s_pJetsFrom[g->difficulty];
}

uint8_t logicJetsPossible(const tGame *g) {
	return logicIsW2(g) && s_pW2JetsFrom[g->mission][g->difficulty] != 255;
}

uint8_t logicMinesOn(const tGame *g) {
	return !g->world && g->trip + g->mission >= s_pMinesFrom[g->difficulty];
}

uint8_t logicTankTell(const tGame *g) {
	return s_pTankTell[g->difficulty];
}

uint8_t logicJetTell(const tGame *g) {
	return s_pJetTell[g->difficulty];
}

uint8_t logicBoatTell(const tGame *g) {
	return s_pBoatTell[g->difficulty];
}
uint8_t logicPatrolArmed(const tGame *g) {
	return logicIsW2(g) && (g->mission >= 1 || g->difficulty >= 3);   // M5 on; M4 too from skill 3 (Paul, 2026-11-14: M4 was too easy)
}
// A boat's extra speed-up per skill (frames off the pause between shells, on top of the tank table)
static const int8_t s_pBoatCool[DIFFICULTY_MAX + 1] = {0, 0, 0, -15, -25, -30};
uint8_t logicBoatGap(const tGame *g) {
	int16_t wCool = (int16_t)(TANK_GAP_BASE - g->mission * 20 + s_pTankCool[g->difficulty] + s_pBoatCool[g->difficulty]);
	uint8_t ubMin = s_pTankCoolMin[g->difficulty];
	return (uint8_t)(wCool < ubMin ? ubMin : wCool);
}
uint8_t logicPatrolTell(const tGame *g) {
	return (uint8_t)(s_pBoatTell[g->difficulty] + PATROL_TELL_EXTRA);
}

// ---------------------------------------------------- world 2: the land ---

COLD uint8_t logicOnWater(int16_t x) {
	for(uint8_t i = 0; i < W2_ISLANDS; ++i) {
		if(x >= g_pIslands[i].x0 && x < g_pIslands[i].x1) return 0;
	}
	return 1;
}

// Each pad's width for the skill: the widest the skill wants that the bed takes, on the 16 px grid
COLD static void padsSet(tGame *g) {
	for(uint8_t i = 0; i < W2_ISLANDS; ++i) {
		uint8_t w = s_pPadWant[g->difficulty];
		if(w > g_pIslands[i].bedPx) w = g_pIslands[i].bedPx;
		g->padPx[i] = (uint8_t)(32 + ((w - 32) & ~15));
	}
}

uint8_t logicPadAt(const tGame *g, int16_t x) {
	for(uint8_t i = 0; i < W2_ISLANDS; ++i) {
		if(absw((int16_t)(x - g_pIslands[i].padX)) <= (g->padPx[i] >> 1) + PAD_TOLERANCE) return i;
	}
	return 0xFF;
}

COLD static uint8_t basePadW2(const tGame *g, int16_t x) {
	return logicPadAt(g, x) == W2_BASE_ISLAND;
}

// (world 1's base pad stays two compares: this runs every frame)
static inline uint8_t onBasePad(const tGame *g, int16_t x) {
	return logicIsW3(g) ? logicW3IsPad(x) : g->world ? basePadW2(g, x) : (x >= BASE_PAD_X0 && x <= BASE_PAD_X1);
}

uint8_t logicOnBasePad(const tGame *g, int16_t x) {
	return onBasePad(g, x);
}

uint16_t logicGroundDwell(const tGame *g) {
	return s_pLandDwell[g->difficulty];
}

uint8_t logicJetsTripMax(const tGame *g) {
	if(logicIsW2(g)) return s_pW2JetsTrip[g->mission][g->difficulty];
	if(logicIsW3(g)) return 0;
	return s_pJetsTrip[g->difficulty];
}

uint8_t logicJetsMax(const tGame *g) {
	return g->mission >= MISSION_COUNT - 1 ? s_pJetsMaxM3[g->difficulty] : s_pJetsMax[g->difficulty];
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
				if(isCalling && absw(dx) < (logicIsW2(g) ? W2_CALL_RANGE : HOSTAGE_CALL_RANGE)) {
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
				int16_t wHome = g->pBarracks[(uint8_t)h->home].wx;
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
				h->x = (int16_t)(h->x + (logicIsW2(g) ? 2 : 1));   // (world 2: a quicker run, fewer people on screen at once)
				h->anim = (uint8_t)(MAN_RUN0 + ((g->frame >> 2) & 1));
				if(h->x >= logicHangarX(g) + 12) {
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
		if(logicIsW2(g) && g->outCount >= W2_OUT_MAX) continue;   // (the next waits for room on the beach)
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

// Whether a shot, a mine or a ram can hurt the chopper now: not in the two
// seconds after its first hit (they pass through it)
static inline uint8_t chopperHurtable(const tChopper *c) {
	return c->alive && !c->invuln;
}

// A hit on the chopper. The first one only damages it (smoke, slower, everyone
// kept) and gives INVULN_FRAMES of grace; a hit while damaged destroys it.
static void chopperDestroy(tGame *g) {
	tChopper *c = &g->chop;
	if(!c->alive) return;
	if(g->difficulty == DIFFICULTY_INVINCIBLE || g->ghost) {
		// it would have been: show the hit, carry on
		boomAt(g, c->x, (int16_t)(c->y - 8));
		g->events |= EV_SHRUG;
		return;
	}
	if(!c->damaged) {
		c->damaged = 1;
		c->invuln = INVULN_FRAMES;
		boomAt(g, c->x, (int16_t)(c->y - 8));
		g->events2 |= EV2_DAMAGE;
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
	// its smoke goes up in the explosion (drawn on through the crash, inside the
	// boom, the puffs overran the A500's frame there)
	for(uint8_t i = 0; i < SMOKE_MAX; ++i) g->pSmoke[i].active = 0;
	g->events |= EV_CRASH;
	if(g->lives) --g->lives;
	setState(g, GS_LOST, LOST_FRAMES);
}

// ------------------------------------------------------ world 3's terrain ---
// SPEC 13.5. The map: the surface height (px above GROUND_Y) at each 16 px joint, a flag a column, the deck's base line a
// joint (0: none). Between joints the surface is the straight line (the tile's own ragged profile is its art).
static const uint8_t *s_pW3H = g_pW3Height, *s_pW3F = g_pW3Flags, *s_pW3C = 0;

void logicW3UseMap(const uint8_t *pHeight, const uint8_t *pFlags, const uint8_t *pCeil) {
	s_pW3H = pHeight ? pHeight : g_pW3Height;
	s_pW3F = pHeight ? pFlags : g_pW3Flags;
	s_pW3C = pHeight ? pCeil : 0;
}

uint8_t logicW3Joint(uint16_t uwJoint) { return uwJoint > W3_COLS ? 0 : s_pW3H[uwJoint]; }

static int16_t w3HeightAt(int16_t x) {
	if(x < 0) x = 0;
	if(x >= W3_W) x = W3_W - 1;
	uint16_t j = (uint16_t)x >> 4;
	int16_t a = s_pW3H[j], b = s_pW3H[j + 1];
	return (int16_t)(a + (((b - a) * (x & 15)) >> 4));
}

int16_t logicW3SurfaceY(int16_t x) { return (int16_t)(GROUND_Y - w3HeightAt(x)); }

int16_t logicW3FloorAt(int16_t x) {
	int16_t hMax = w3HeightAt((int16_t)(x - W3_SKID_HALF)), hB = w3HeightAt((int16_t)(x + W3_SKID_HALF));
	if(hB > hMax) hMax = hB;
	// (a joint between the skids can be higher than both)
	for(int16_t jx = (int16_t)(((x - W3_SKID_HALF) >> 4) + 1); jx * 16 <= x + W3_SKID_HALF; ++jx) {
		if(jx >= 0 && jx <= W3_COLS && s_pW3H[jx] > hMax) hMax = s_pW3H[jx];
	}
	return (int16_t)(GROUND_Y - hMax);
}

static uint8_t w3PadCol(int16_t x) {
	if(x < 0 || x >= W3_W) return 0;
	return (uint8_t)(s_pW3F[(uint16_t)x >> 4] & W3_FLAG_PAD);
}

// a landing needs both skids on the bed
uint8_t logicW3IsPad(int16_t x) {
	return (uint8_t)(w3PadCol(x) && w3PadCol((int16_t)(x - W3_SKID_HALF)) && w3PadCol((int16_t)(x + W3_SKID_HALF)));
}

int16_t logicW3CeilAt(int16_t x) {
	if(!s_pW3C) return CEIL_Y;
	if(x < 0) x = 0;
	if(x >= W3_W) x = W3_W - 1;
	uint16_t j = (uint16_t)x >> 4;
	int16_t a = s_pW3C[j], b = s_pW3C[j + 1], base;
	if(a && b) base = (int16_t)(a + (((b - a) * (x & 15)) >> 4));
	else base = a ? a : b;
	if(!base) return CEIL_Y;
	return (int16_t)(base + W3_DECK_CLEAR > CEIL_Y ? base + W3_DECK_CLEAR : CEIL_Y);
}

// The body's leading edge xEdge, skids at y, heading bDir (+1 right, -1 left): inside rock whose flank here is a wall
// (60 degrees or steeper facing the heading)? Gentler flanks carry the chopper up (floorAt lifts the skids), they don't stop it.
uint8_t logicW3WallAt(int16_t xEdge, int16_t y, int8_t bDir) {
	if(xEdge < 0 || xEdge >= W3_W) return 0;
	if(logicW3SurfaceY(xEdge) >= y - 1) return 0;
	uint16_t j = (uint16_t)xEdge >> 4;
	int16_t wRise = (int16_t)(bDir > 0 ? s_pW3H[j + 1] - s_pW3H[j] : s_pW3H[j] - s_pW3H[j + 1]);
	return wRise >= W3_WALL_RISE;
}

// SPEC 13.2: under a deck the route keeps at least W3_MIN_GAP px of air between the deck's base and the crest
uint8_t logicW3GapOk(const uint8_t *pHeight, const uint8_t *pFlags, const uint8_t *pCeil) {
	if(!pHeight) { pHeight = g_pW3Height; pFlags = g_pW3Flags; pCeil = 0; }
	if(!pCeil) return 1;
	for(uint16_t j = 0; j < W3_COLS; ++j) {
		if(!pCeil[j] && !pCeil[j + 1]) continue;
		if(pFlags[j] & W3_FLAG_MASSIF) continue;
		int16_t h = pHeight[j] > pHeight[j + 1] ? pHeight[j] : pHeight[j + 1];
		int16_t base = pCeil[j] > pCeil[j + 1] ? pCeil[j] : pCeil[j + 1];
		if(GROUND_Y - h - base < W3_MIN_GAP) return 0;
	}
	return 1;
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
	if(x + 14 > logicHangarX(g) && x - 14 < logicHangarX(g) + 64) {
		*pIsGround = 0;
		return FEET_Y - 40;
	}
	// world 3: the height map (SPEC 13.5): the skids stop at the highest surface under them, a landing only on a pad bed
	if(logicIsW3(g)) {
		*pIsGround = logicW3IsPad(x);
		return logicW3FloorAt(x);
	}
	// world 2: a pad is a landing; the beach, the palms and the sea are floors to stop on
	if(g->world) *pIsGround = logicPadAt(g, x) != 0xFF;
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
	c->faceTarget = ubFace;
}

// One frame of the turn, eased: side -> the three-quarter view -> the front (a lean toward
// the way the nose swings, held) -> the three-quarter view -> the other side. Every pose is
// shown for several frames and none is skipped; the front also sways (logicChopperFrame).
static void turnStep(tChopper *c) {
	if(c->turnTimer) {
		if(--c->turnTimer) return;
		uint8_t ubFrom = c->face;   // one step: side <-> front
		if(c->faceTarget > c->face) ++c->face;
		else if(c->faceTarget < c->face) --c->face;
		if(c->face != FACE_FRONT) c->lastSide = c->face;
		else if(ubFrom != FACE_FRONT) {
			c->leanDir = ubFrom == FACE_LEFT ? FACE_RIGHT : FACE_LEFT;
			c->leanTimer = LEAN_FRAMES;
			c->swayTimer = 0;
		}
		return;
	}
	if(c->face == c->faceTarget) {
		if(c->leanTimer) --c->leanTimer;
		else if(c->face == FACE_FRONT && ++c->swayTimer >= SWAY_PHASE * 4) c->swayTimer = 0;
		return;
	}
	if(c->face != FACE_FRONT) { c->turnTimer = TURN_STEP_FRAMES; return; }
	// leaving the front: lean toward the way out first
	c->leanDir = c->faceTarget;
	if(c->leanTimer > 1) --c->leanTimer;
	else if(c->leanTimer) { c->leanTimer = 0; c->turnTimer = TURN_STEP_FRAMES; }
	else c->leanTimer = LEAN_FRAMES;
}

void logicChopperUpdate(tGame *g, const tInput *pIn) {
	tChopper *c = &g->chop;
	if(c->invuln) --c->invuln;
	// a damaged chopper is about 20 % slower
	int16_t wVxMax = c->damaged ? DMG_VX_MAX : VX_MAX;
	int16_t wHoMax = c->damaged ? DMG_HO_VX_MAX : HO_VX_MAX;
	int16_t wUpMax = c->damaged ? DMG_VY_UP_MAX : VY_UP_MAX;
	int16_t wDownMax = c->damaged ? DMG_VY_DOWN_MAX : VY_DOWN_MAX;
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
	// The stick's way to the bombs is deliberate: held down BOMB_ARM_FRAMES with no fire in
	// them, then a press. Firing restarts the count, so a volley (taps or held, diving or not)
	// is always the cannon. Up + fire from the front goes back to the cannon.
	if(isFireEdge && !c->landed) {
		if(c->faceTarget != FACE_FRONT && c->dyHold >= BOMB_ARM_FRAMES) {
			turnTo(c, FACE_FRONT);
			c->stance = BOMB_STANCE_FRAMES;
			isFireUsed = 1;
		}
		else if(c->faceTarget == FACE_FRONT && pIn->dy < 0) {
			turnTo(c, c->lastSide);
			isFireUsed = 1;
		}
	}
	if(pIn->fire || pIn->dy == 0) c->dyHold = 0;
	else if(pIn->dy > 0) c->dyHold = c->dyHold < 0 ? 1 : c->dyHold < 100 ? c->dyHold + 1 : 100;
	else c->dyHold = 0;
	if(pIn->dx != c->holdDx) { c->holdDx = pIn->dx; c->turnHold = 0; }   // (a reversal starts the push again)
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

	turnStep(c);
	if(c->faceTarget != FACE_FRONT) c->stance = 0;
	else if(c->stance && c->face == FACE_FRONT && !--c->stance) turnTo(c, c->lastSide);
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
				int16_t wWant = (int16_t)(pIn->dx * wHoMax);
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
		c->vx = clampw(c->vx, -wVxMax, wVxMax);
		if(pIn->dy < 0) c->vy = (int16_t)(c->vy - VY_ACCEL);
		else if(pIn->dy > 0) c->vy = (int16_t)(c->vy + VY_ACCEL);
		else if(c->vy > 1) c->vy = (int16_t)(c->vy - 2);
		else if(c->vy < -1) c->vy = (int16_t)(c->vy + 2);
		else c->vy = 0;
		c->vy = clampw(c->vy, -wUpMax, wDownMax);
	}
	int16_t wPrevX = c->x;
	subStep(&c->x, c->vx, &c->fx);
	if(logicIsW3(g) && c->vx) {
		// a wall is solid from the side: the body stops, its speed goes, nothing is damaged
		int8_t bDir = c->vx > 0 ? 1 : -1;
		if(logicW3WallAt((int16_t)(c->x + bDir * W3_BODY_HALF), c->y, bDir)) {
			c->x = wPrevX;
			c->fx = 0;
			c->vx = 0;
			g->events3 |= EV3_BUMP;
		}
	}
	subStep(&c->y, c->vy, &c->fy);
	if(c->x < 24) { c->x = 24; c->vx = 0; }
	if(c->x > logicWorldW(g) - 24) { c->x = (int16_t)(logicWorldW(g) - 24); c->vx = 0; }
	if(c->y < CEIL_Y) { c->y = CEIL_Y; c->vy = 0; }
	if(logicIsW3(g)) {
		// a cloud deck or a tunnel roof: the skids stop under it (eased down, never a pop)
		int16_t wCeil = logicW3CeilAt(c->x);
		if(c->y < wCeil) {
			c->y = (int16_t)(c->y + 3 > wCeil ? wCeil : c->y + 3);
			c->fy = 0;
			if(c->vy < 0) c->vy = 0;
		}
	}
	// the pad guide (skill 1-3): the last PAD_ASSIST_PX of a descent nudge the chopper toward
	// the pad's centre, half a pixel a frame
	if(logicIsW2(g) && c->vy > 0 && c->y >= GROUND_Y - PAD_ASSIST_PX && (g->frame & 1) && g->difficulty <= DIFFICULTY_NORMAL) {
		for(uint8_t i = 0; i < W2_ISLANDS; ++i) {
			int16_t dxp = (int16_t)(g_pIslands[i].padX - c->x), ad = absw(dxp);
			if(ad > 2 && ad <= logicPadPx(g, i) / 2 + PAD_TOLERANCE + PAD_ASSIST_REACH) c->x = (int16_t)(c->x + (dxp > 0 ? 1 : -1));
		}
	}
	uint8_t isGround;
	int16_t wFloor = floorAt(g, c->x, &isGround);
	if(c->y >= wFloor && !c->landed) {
		c->y = wFloor;
		c->fy = 0;
		if(c->vy > 0) c->vy = 0;
		if(logicIsW2(g) && !g->wet && logicOnWater(c->x)) {
			// the sea: a soft stop and a splash (no damage), one a touch
			g->wet = 1;
			g->splashX = c->x;
			g->splashTimer = SPLASH_FRAMES;
			g->events2 |= EV2_SPLASH;
		}
		if(isGround) {
			c->landed = 1;
			c->vx = 0;
			g->events |= EV_LAND;
			g->unloadTimer = UNLOAD_FRAMES;
			hostagesBlast(g, c->x, 13);   // landed on someone
		}
	}

	if(g->wet && (c->y < GROUND_Y - 6 || !logicOnWater(c->x))) g->wet = 0;

	// the head-on strafe's lean follows the speed (slight from 0.75 px/frame, full from 1.75), a pose
	// every LEAN_STEP_FRAMES: eased in and out, no pose skipped, never a snap from one side to the other
	{
		int8_t bTo = 0;
		if(c->face == FACE_FRONT && c->faceTarget == FACE_FRONT && !c->turnTimer && !c->leanTimer) {
			int16_t wV = c->vx;
			bTo = wV >= 28 ? 2 : wV >= 12 ? 1 : wV <= -28 ? -2 : wV <= -12 ? -1 : 0;
		}
		if(c->leanStep) --c->leanStep;
		else if(c->lean != bTo) { c->lean += c->lean < bTo ? 1 : -1; c->leanStep = LEAN_STEP_FRAMES; }
		if(c->lean) c->swayTimer = 0;
	}

	// --- firing
	if(pIn->fire && !isFireUsed && !isTurning) {
		if(isFireEdge || c->fireTimer <= 1) {
			if(c->face == FACE_FRONT) {
				if(!c->landed) {
					shotFire(g, SHOT_BOMB);
					if(c->stance) c->stance = BOMB_STANCE_FRAMES;
				}
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
	// landing on the home pad mends a damaged chopper
	if(c->alive && c->landed && c->damaged && onBasePad(g, c->x)) {
		c->damaged = 0;
		g->events2 |= EV2_REPAIR;
	}
	if(!c->alive || !c->landed || !g->aboard || !onBasePad(g, c->x)) return;
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

// A boat's hull (cannon 1 a hit, a bomb 3): its box, 56 px wide and 26 tall, standing in the water
#define BOAT_BOX_Y 222
COLD static void boatHit(tGame *g, tBoat *b, uint8_t ubDmg) {
	if(!b->active) return;
	b->hp = ubDmg >= b->hp ? 0 : (uint8_t)(b->hp - ubDmg);
	if(b->hp) return;
	b->active = 0;
	boomAt(g, b->x, BOAT_BOX_Y + 8);
	addScore(g, b->kind == BOAT_GUN ? 100 : 50);
}

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
	for(uint8_t i = 0; logicIsW2(g) && i < BOAT_MAX; ++i) {
		tBoat *b = &g->pBoats[i];
		if(b->active && boxHit(sx, sy, 6, 3, (int16_t)(b->x - 28), BOAT_BOX_Y, 56, 26)) {
			boatHit(g, b, 1);
			return 1;
		}
	}
	for(uint8_t i = 0; i < TANK_MAX; ++i) {
		tTank *t = &g->pTanks[i];
		if(t->active && boxHit(sx, sy, 6, 3, (int16_t)(t->x - 12), FEET_Y - 14, 24, 14)) {
			if(t->hp) --t->hp;
			if(!t->hp) {
				t->active = 0;
				boomAt(g, t->x, FEET_Y - 10);
				tankKilled(g);
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
			tankKilled(g);
		}
	}
	for(uint8_t b = 0; b < BARRACKS_COUNT; ++b) {
		if(absw((int16_t)(g->pBarracks[b].x - x)) < 30) barracksDamage(g, b, 2);
	}
	for(uint8_t i = 0; logicIsW2(g) && i < BOAT_MAX; ++i) {
		if(g->pBoats[i].active && absw((int16_t)(g->pBoats[i].x - x)) < 30) boatHit(g, &g->pBoats[i], 3);
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
		if(logicIsW3(g) && s->x >= 0 && s->x < W3_W && s->y >= logicW3SurfaceY(s->x)) {
			// world 3: terrain stops shots
			s->active = 0;
			boomAt(g, s->x, s->y);
			continue;
		}
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
		if(!--s->life || s->x < 0 || s->x >= logicWorldW(g)) s->active = 0;
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
		if(chopperHurtable(c) && boxHit((int16_t)(s->x - 2), (int16_t)(s->y - 2), 4, 4,
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
		if(!--s->life || s->y < PLAY_TOP || s->x < 0 || s->x >= logicWorldW(g)) s->active = 0;
	}
}

// ------------------------------------------------------------- enemies ---

// 24 * 256 / (32 + i): the shell's speed over the aim vector's length
static const int16_t s_pAimRecip[32] = {
	192, 186, 181, 176, 171, 166, 162, 158,
	154, 150, 146, 143, 140, 137, 134, 131,
	128, 125, 123, 120, 118, 116, 114, 112,
	110, 108, 106, 104, 102, 101, 99, 98,
};

// Aim from (x, y) at (tx, ty): a speed of TANK_SHELL_SPEED (1.5 px a frame)
// along the larger axis, a little more on a slant (the length is estimated as
// max + 3/8 min, within 7%), no division
static void aim(int16_t dx, int16_t dy, int16_t *pVx, int16_t *pVy) {
	int16_t ax = absw(dx), ay = absw(dy);
	int16_t l = ax > ay ? (int16_t)(ax + (ay >> 1) - (ay >> 3)) : (int16_t)(ay + (ax >> 1) - (ax >> 3));
	if(!l) { *pVx = 0; *pVy = -TANK_SHELL_SPEED; return; }
	while(l >= 64) { dx = (int16_t)(dx >> 1); dy = (int16_t)(dy >> 1); l = (int16_t)(l >> 1); }
	while(l < 32) { dx = (int16_t)(dx * 2); dy = (int16_t)(dy * 2); l = (int16_t)(l * 2); }
	int16_t r = s_pAimRecip[l - 32];
	*pVx = (int16_t)(((int32_t)dx * r) >> 8);
	*pVy = (int16_t)(((int32_t)dy * r) >> 8);
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
				s->life = SHELL_LIFE;
				t->fireAnim = 8;
				g->events |= EV_TANKFIRE;
			}
			// the time to the next shell, less the next tell (the same rate of fire)
			int16_t wCool = (int16_t)(TANK_GAP_BASE - g->mission * 20 - g->trip * 8 + s_pTankCool[g->difficulty]);
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
		int16_t wCool = (int16_t)(TANK_GAP_BASE - g->mission * 20 - g->trip * 8 + s_pTankCool[g->difficulty]);
		uint8_t ubMin = s_pTankCoolMin[g->difficulty];
		t->cool = (uint8_t)(wCool < ubMin ? ubMin : wCool);
		t->cool = (uint8_t)(t->cool + (logicRandom(g) & 31));
	}
}

// The whole jet (32 px) is inside the screen: no jet fires from, or into, the margin
static inline uint8_t jetFullyOnScreen(const tGame *g, const tJet *j) {
	return j->x - 16 >= g->cam && j->x + 16 <= g->cam + SCREEN_W;
}

// The flash may begin up to three quarters of its length before the whole jet is on screen (it is
// still whole on screen to shoot at its end): a tell-independent window, else the
// long tells of the low skills leave a jet no time to start one
static inline uint8_t jetNearlyOnScreen(const tGame *g, const tJet *j, uint8_t ubTell) {
	int16_t wSlack = (int16_t)(ubTell * 3);
	return j->x - 16 >= g->cam - wSlack && j->x + 16 <= g->cam + SCREEN_W + wSlack;
}

// An earlier jet is still out (alive): where only one may attack, the later one waits
static uint8_t jetEarlierOut(const tGame *g, uint8_t ubSelf) {
	for(uint8_t k = 0; k < ubSelf; ++k) if(g->pJets[k].active) return 1;
	return 0;
}

__attribute__((noinline)) static void jetsUpdate(tGame *g) {
	tChopper *c = &g->chop;
	// the chopper on the ground, and just off it: jets wait (SPEC 6, jet fairness)
	if(c->landed) {
		if(g->landFrames < 65535) ++g->landFrames;
		g->airGrace = 0;
	}
	else if(g->landFrames) {
		g->landFrames = 0;
		g->airGrace = JET_TAKEOFF_GRACE;
	}
	else if(g->airGrace) --g->airGrace;
	if(g->jetWait) --g->jetWait;
	// the jet budget starts afresh on the home pad
	if(c->landed && onBasePad(g, c->x)) g->jetsTrip = 0;
	uint8_t ubMax = logicJetsMax(g);
	uint16_t uwDwell = logicGroundDwell(g);
	uint8_t isBombOk = c->landed && uwDwell != GROUND_NEVER && g->landFrames >= uwDwell;
	uint8_t isMissileOk = !c->landed && !g->airGrace;
	for(uint8_t i = 0; i < JET_MAX; ++i) {
		tJet *j = &g->pJets[i];
		if(!j->active) {
			if(!logicJetsOn(g) || !c->alive || c->x > (logicIsW2(g) ? W2_JET_SAFE_X : BORDER_X + 120) || i >= ubMax) continue;
			if(g->jetsTrip >= logicJetsTripMax(g)) continue;   // this trip's jets are all out
			if(j->cool) { --j->cool; continue; }
			// mostly from ahead (where the chopper is heading: you see it coming),
			// 1 in 4 from behind; well off screen, so its sound and its radar dot
			// come first
			j->active = 1;
			uint16_t r = logicRandom(g);
			int8_t ahead = c->vx > 0 ? -1 : c->vx < 0 ? 1 : (c->face == FACE_RIGHT ? -1 : 1);
			uint8_t isBehind = !(r & 3);
			j->dir = isBehind ? (int8_t)-ahead : ahead;
			// (from behind: further off and a longer warning, time to turn and face it)
			int16_t wLead = isBehind ? JET_LEAD_BEHIND : JET_LEAD;
			j->x = (int16_t)(j->dir > 0 ? g->cam - wLead : g->cam + SCREEN_W + wLead);
			j->seen = 0;
			j->tell = 0;
			j->warn = isBehind ? JET_WARN_BEHIND : JET_WARN_FRAMES;
			++g->jetsTrip;
			j->y = clampw((int16_t)(c->y - 20), PLAY_TOP + 16, 180);
			j->hp = 1;
			j->pass = 0;
			j->cool = 20;
			g->events |= EV_JET;
			g->events2 |= EV2_JETWARN;
			continue;
		}
		j->x = (int16_t)(j->x + j->dir * 4);
		// follow the chopper's height slowly
		int16_t wTargetY = clampw((int16_t)(c->y - 8), PLAY_TOP + 16, 200);
		if((g->frame & 1) && j->y < wTargetY) ++j->y;
		if((g->frame & 1) && j->y > wTargetY) --j->y;
		if(j->cool) --j->cool;
		if(j->warn) --j->warn;
		if(j->x > g->cam - 16 && j->x < g->cam + SCREEN_W + 16) {
			if(j->seen < 255) ++j->seen;
		}
		int16_t dx = (int16_t)(c->x - j->x);
		int16_t ahead = j->dir > 0 ? dx : (int16_t)-dx;
		uint8_t isFull = jetFullyOnScreen(g, j);
		uint8_t ubTell = logicJetTell(g);
		if(j->tell) {
			// the flash: the jet flies on; at the end it shoots if it's still a good shot
			if(!--j->tell) {
				uint8_t isFired = 0;
				tShot *s = c->alive && isFull ? eshotNew(g) : 0;
				if(s && !c->landed && ahead > JET_AHEAD_MIN && absw((int16_t)(c->y - 8 - j->y)) < 14) {
					s->active = 1;
					s->kind = ESHOT_MISSILE;
					s->x = (int16_t)(j->x + j->dir * 16);
					s->y = (int16_t)(j->y + 2);
					s->vx = (int16_t)(j->dir * JET_MISSILE_SPEED);
					s->vy = 0;
					s->fx = s->fy = 0;
					s->life = JET_MISSILE_LIFE;
					isFired = 1;
				}
				else if(s && c->landed && isBombOk && absw(dx) < 32) {
					s->active = 1;
					s->kind = ESHOT_BOMB;
					s->x = j->x;
					s->y = (int16_t)(j->y + 6);
					s->vx = (int16_t)(j->dir * 40);
					s->vy = 8;
					s->fx = s->fy = 0;
					s->life = 120;
					isFired = 1;
				}
				if(isFired) g->events2 |= EV2_JETFIRE;
				// the next shot's time, less its own flash (the rate of fire is JET_GAP)
				j->cool = isFired ? (uint8_t)(JET_GAP - ubTell) : 20;
			}
		}
		else if(c->alive && !j->cool && !j->warn && jetNearlyOnScreen(g, j, ubTell) && (c->landed ? isBombOk : isMissileOk) &&
			(!g->jetWait || g->jetLast == i) && (ubMax > 1 || !jetEarlierOut(g, i)) &&
			(!g->world || !g->atkWait || g->atkLast == ATK_JET)) {
			// start the flash if a shot is on, allowing for how far the jet flies meanwhile
			// (4 px a frame): a missile needs JET_AHEAD_MIN left at the end, a bomb the chopper under it
			int16_t wRun = (int16_t)(ubTell * 4);
			uint8_t isShot = !c->landed ? (ahead > JET_AHEAD_MIN + wRun && ahead < 200 + wRun &&
					absw((int16_t)(c->y - 8 - j->y)) < 14) :
				(ahead >= wRun && ahead < wRun + 24);
			if(isShot && eshotNew(g)) {
				j->tell = ubTell;
				++g->jetAttacks;
				g->jetLast = i;
				g->jetWait = JET_PAIR_GAP;
				if(g->world) { g->atkWait = BOAT_ATTACK_GAP; g->atkLast = ATK_JET; }   // (R3: one attack at a time, jets and boats)
				g->events2 |= EV2_JETTELL;
			}
		}
		if(chopperHurtable(c) && boxHit((int16_t)(j->x - 14), (int16_t)(j->y - 5), 28, 10,
			(int16_t)(c->x - 13), (int16_t)(c->y - 13), 26, 13)) {
			j->active = 0;
			boomAt(g, j->x, j->y);
			chopperDestroy(g);
			continue;
		}
		if((j->dir > 0 && j->x > g->cam + SCREEN_W + 64 && j->seen) || (j->dir < 0 && j->x < g->cam - 64 && j->seen)) {
			j->active = 0;
			// (one jet at a time: 140 frames shorter, the warning flight is time the jet is no threat)
			int16_t wRest = (int16_t)((ubMax > 1 ? 160 : 20) - g->mission * 30 + s_pJetRest[g->difficulty]);
			j->cool = (uint16_t)((wRest < 40 ? 40 : wRest) + (logicRandom(g) & 63));
		}
	}
}

// --------------------------------------------------------------- boats ---

// Whether a boat may take the chopper as its target now: the shared ground-threat rules
// (docs/CONTENT-PLAN.md 4.1). It is low (skids below y 150, as a tank's target is), in the boat's
// sight and not point-blank (R7, R8), the whole boat is on screen (R7); landed it has sat
// there the ground dwell (R1: never on skill 1-2), and just off the ground the take-off grace
// is over (R2)
COLD static uint8_t boatTarget(const tGame *g, const tBoat *b) {
	const tChopper *c = &g->chop;
	if(!c->alive || c->y <= 150) return 0;
	int16_t ad = absw((int16_t)(c->x - b->x));
	if(ad >= BOAT_RANGE || ad < BOAT_MIN_RANGE) return 0;
	if(b->x - 30 < g->cam || b->x + 30 > g->cam + SCREEN_W) return 0;
	if(c->landed) {
		uint16_t uwDwell = logicGroundDwell(g);
		return uwDwell != GROUND_NEVER && g->landFrames >= uwDwell;
	}
	return !g->airGrace;
}

COLD static void boatsUpdate(tGame *g) {
	tChopper *c = &g->chop;
	if(g->splashTimer) --g->splashTimer;
	if(g->atkWait) --g->atkWait;
	for(uint8_t i = 0; i < BOAT_MAX; ++i) {
		tBoat *b = &g->pBoats[i];
		if(!b->active) continue;
		if(b->fireAnim) --b->fireAnim;
		if(b->tell) {
			// the wind-up: braced (it doesn't sail), barrel up, lamp flashing; then the shell,
			// unless the chopper got away meanwhile
			if(--b->tell) continue;
			uint8_t ubGun = b->kind == BOAT_GUN;
			tShot *s = boatTarget(g, b) ? eshotNew(g) : 0;
			if(s) {
				uint8_t ubMask = s_pAimMask[g->difficulty];
				int16_t tx = (int16_t)(c->x + (c->vx >> 1) - b->x + (int16_t)(logicRandom(g) & ubMask) - (ubMask >> 1) - 1);
				int16_t wMuzzleY = ubGun ? BOAT_MUZZLE_Y : PATROL_MUZZLE_Y;
				int16_t ty = (int16_t)(c->y - 8 - wMuzzleY);
				int16_t wMuzzleDx = ubGun ? 26 : PATROL_MUZZLE_DX;
				s->active = 1;
				s->kind = ESHOT_SHELL;
				s->x = (int16_t)(b->x + (tx > 0 ? wMuzzleDx : -wMuzzleDx));
				s->y = wMuzzleY;
				s->fx = s->fy = 0;
				aim(tx, ty, &s->vx, &s->vy);
				s->life = SHELL_LIFE;
				b->fireAnim = 8;
				g->events2 |= EV2_BOATFIRE;
			}
			b->cool = (uint8_t)(logicBoatGap(g) - (ubGun ? logicBoatTell(g) : logicPatrolTell(g)));
			if(!ubGun) b->cool = (uint8_t)(b->cool + PATROL_COOL_EXTRA);
			b->cool = (uint8_t)(b->cool + (logicRandom(g) & 31));
			continue;
		}
		if(b->cool) --b->cool;
		else if((b->kind == BOAT_GUN || logicPatrolArmed(g)) && boatTarget(g, b) && (!g->atkWait || g->atkLast == i)) {
			// the tell: it turns to the chopper, stops, raises its barrel
			b->dir = c->x > b->x ? 1 : -1;
			b->tell = b->kind == BOAT_GUN ? logicBoatTell(g) : logicPatrolTell(g);
			g->atkWait = BOAT_ATTACK_GAP;
			g->atkLast = i;
			g->events2 |= EV2_BOATTELL;
			continue;
		}
		// patrol its stretch of sea, 0.6 px a frame (a patrol boat 0.8)
		if(++b->phase >= 5) b->phase = 0;
		if(b->phase < (b->kind == BOAT_GUN ? 3 : 4)) {
			b->x = (int16_t)(b->x + b->dir);
			if(b->x <= b->x0) { b->x = b->x0; b->dir = 1; }
			else if(b->x >= b->x1) { b->x = b->x1; b->dir = -1; }
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
		if(chopperHurtable(c) && boxHit((int16_t)(m->x - 6), (int16_t)(m->y - 6), 12, 12,
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
	return clampw((int16_t)(g->chop.x - SCREEN_W / 2 + (g->camLead >> 4)), 0, (int16_t)(logicWorldW(g) - SCREEN_W));
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
	int16_t room = s > 0 ? (int16_t)(logicWorldW(g) - SCREEN_W - g->cam) : (int16_t)g->cam;
	if(as > 1 && room < (int16_t)((uint16_t)((uint16_t)as * (uint16_t)(as + 1)) >> 1)) {
		s = (int16_t)(s > 0 ? s - 1 : s + 1);   // brake for the edge
	}
	g->cam = clampw((int16_t)(g->cam + s), 0, (int16_t)(logicWorldW(g) - SCREEN_W));
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

// The damaged chopper's trail: a puff every 3 frames in the air (6 on the
// ground, and hovering: under half a pixel a frame they'd pile up on one spot)
// from its tail; each rises a pixel every other frame and fades
static void smokeUpdate(tGame *g) {
	const tChopper *c = &g->chop;
	if(c->alive && c->damaged) {
		if(g->smokeTimer) --g->smokeTimer;
		else {
			g->smokeTimer = c->landed || absw(c->vx) < 8 ? 6 : 3;
			tSmoke *pFree = 0;
			for(uint8_t i = 0; i < SMOKE_MAX; ++i) {
				if(!g->pSmoke[i].active) { pFree = &g->pSmoke[i]; break; }
			}
			if(pFree) {
				pFree->active = 1;
				pFree->age = 0;
				pFree->x = (int16_t)(c->x + (c->face == FACE_LEFT ? 12 : c->face == FACE_RIGHT ? -12 : 0));
				pFree->y = (int16_t)(c->y - 12);
			}
		}
	}
	for(uint8_t i = 0; i < SMOKE_MAX; ++i) {
		tSmoke *p = &g->pSmoke[i];
		if(!p->active) continue;
		++p->age;
		if(p->age & 1) --p->y;
		if(p->age >= SMOKE_LIFE) p->active = 0;
	}
}

static void worldUpdate(tGame *g) {
	smokeUpdate(g);
	if(g->quotaMsg) --g->quotaMsg;
	if(logicIsW3(g)) {
		// the pilot: terrain and the chopper's shots only (SPEC 13.7)
		shotsUpdate(g);
		boomsUpdate(g);
		return;
	}
	barracksUpdate(g);
	hostagesUpdate(g);
	shotsUpdate(g);
	eshotsUpdate(g);
	tanksUpdate(g);
	jetsUpdate(g);
	if(logicIsW2(g)) boatsUpdate(g);
	minesUpdate(g);
	boomsUpdate(g);
}

static uint8_t missionDone(const tGame *g) {
	if(g->saved + g->killed < HOSTAGE_COUNT) return 0;
	return 1;
}

// The title and the hall of fame
COLD static void menuUpdate(tGame *g, const tInput *pIn) {
	if(!pIn->fire) g->titleArmed = 1;
	else if(g->titleArmed && !g->stateTimer && g->state == GS_TITLE) {
		g->titleArmed = 0;
		logicStartGame(g);
		return;
	}
	else if(g->titleArmed && g->state == GS_SCORES) {
		// (fire on the hall of fame: the title, ready to start)
		setState(g, GS_TITLE, 0);
		g->titleArmed = 0;
		return;
	}
	// the camera drifts over the land
	if(g->frame & 1) {
		g->cam = (int16_t)(g->cam - 1);
		if(g->cam <= 0) g->cam = WORLD_W - SCREEN_W;
	}
	if(g->state == GS_TITLE && ++g->attract >= ATTRACT_FRAMES) {
		g->newRank = HI_COUNT;
		g->scoresNext = GS_INTRO;
		g->titleArmed = 0;
		setState(g, GS_SCORES, SCORES_FRAMES);
	}
	else if(g->state == GS_SCORES && !g->stateTimer) {
		if(g->scoresNext == GS_INTRO) {
			logicStartDemo(g);
			g->titleArmed = 0;
		}
		else setState(g, GS_TITLE, 0);
	}
}

// The demo's input instead of the player's; 1: it's over (back on the title)
COLD static uint8_t demoStep(tGame *g, const tInput *pIn, tInput *pDemoIn) {
	// fire (pressed after it began) or its end: the title
	if(!pIn->fire) g->titleArmed = 1;
	else if(g->titleArmed) { demoEnd(g); return 1; }
	if(!logicDemoInput(g, pDemoIn) || g->state == GS_CLEAR || g->state == GS_OVER ||
		(g->state == GS_LOST && g->stateTimer == 1)) {
		demoEnd(g);
		return 1;
	}
	return 0;
}

void logicUpdate(tGame *g, const tInput *pIn) {
	++g->frame;
	weatherUpdate(g);
	rotorTick(g);   // (before the events are cleared: it sees last frame's lift-off)
	g->events = 0;
	g->events2 = 0;
	g->evBarracks = 0;
	if(g->stateTimer) --g->stateTimer;
	if(g->state != GS_TITLE) g->attract = 0;
	tInput sDemoIn;
	if(g->demo) {
		if(demoStep(g, pIn, &sDemoIn)) return;
		pIn = &sDemoIn;   // the demo plays itself
	}
	switch(g->state) {
		case GS_TITLE:
		case GS_SCORES:
			menuUpdate(g, pIn);
			break;
		case GS_ENTRY:
			entryUpdate(g, pIn);
			break;
		case GS_INTRO: {
			// the preview card: a fresh press of fire skips it
			uint8_t isSkip = pIn->fire && !g->chop.prevFire;
			g->chop.prevFire = pIn->fire;
			g->chop.prevFire2 = pIn->fire2;
			logicCameraUpdate(g);
			if(!g->stateTimer || isSkip) setState(g, GS_PLAY, 0);
		} break;
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
				else if(g->world + 1 < WORLD_COUNT) {
					// the next world's first mission: lives refill (decided by Paul), the score is kept
					++g->world;
					g->lives = LIVES_START;
					logicStartMission(g, 0);
				}
				else setState(g, GS_VICTORY, OVER_FRAMES);
			}
			break;
		case GS_OVER:
		case GS_VICTORY:
			boomsUpdate(g);
			if(!g->stateTimer || (pIn->fire && g->stateTimer < OVER_FRAMES - 100)) gameEnded(g);
			break;
	}
}

// --------------------------------------------------------------- drawing ---

uint8_t logicChopperVisible(const tGame *g) {
	// (after a first hit it blinks: BLINK_FRAMES (4) shown, 4 hidden)
	return !logicIsMenu(g) && g->state != GS_ENTRY &&
		!(g->state == GS_LOST && !g->chop.alive && g->chop.crashTimer == 0) &&
		!(g->chop.invuln & BLINK_FRAMES);
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
	if(c->face == FACE_FRONT) {
		uint8_t ubLeft = c->leanDir == FACE_LEFT ? CHOP_LEFT : 0;
		if(c->leanTimer) return CHOP_LEAN2 | ubLeft;   // the turn's lean
		if(c->faceTarget != FACE_FRONT) return CHOP_FRONT;
		// a sidestep leans into the move, one pose at a time (c->lean, eased in chopperUpdate)
		if(c->lean) return (c->lean == 2 || c->lean == -2 ? CHOP_LEAN2 : CHOP_LEAN1) | (c->lean < 0 ? CHOP_LEFT : 0);
		// the hover's sway: front, slight lean one way, front, the other way
		uint8_t ubPhase = c->swayTimer / SWAY_PHASE;
		if(ubPhase & 1) return CHOP_LEAN1 | (ubPhase == 3 ? CHOP_LEFT : 0);
		return CHOP_FRONT;
	}
	int16_t fwd = c->face == FACE_RIGHT ? c->vx : (int16_t)-c->vx;
	uint8_t pose = fwd >= 32 ? CHOP_SIDE2 : fwd >= 12 ? CHOP_SIDE1 : CHOP_SIDE0;
	return pose | (c->face == FACE_LEFT ? CHOP_LEFT : 0);
}

// ---- the rotor's sound. Periods: 845 idle (3.5 thumps/s), 591 hover (5), 45/16 less
// per 1/16 px of speed (523 at 24: 5.6, 500 at 32: 5.9, 455 at 48: 6.5/s; Paul's
// playtest, 2026-11-12: 8.4 at top speed was irritating), 4 less per climb unit; the
// spin-up takes 3 per boosted frame off (up to 380, 7.8/s). Never below 455 (6.5/s)
// outside the spin-up. v4 adds half the descent speed to the period (settling down).
#define ROTOR_PER_IDLE 845
#define ROTOR_PER_HOVER 591
#define ROTOR_PER_MIN 455
#define ROTOR_PER_MIN_BOOST 380    // v4: the empty spin-up peaks at 7.8 thumps/s (was 300: 9.8/s with a climb, a buzz)
#define ROTOR_BOOST_FRAMES 100
#define ROTOR_HEAVY_PER_HEAD 2   // v3: period units a person aboard adds (16 aboard: 32, 6 %: lower, never higher)
#define ROTOR_HURT_BASE 32       // v3: a damaged chopper's rotor is this much lower, plus the wobble

// v3: the hurt engine labours: a slow, small wobble in the pitch (0-15 period units, two
// triangles of 64 and 21 frames), so it is not a steady tone and not a siren
static int16_t rotorWobble(uint16_t uwFrame) {
	int16_t a = (int16_t)(uwFrame & 63);
	if(a > 31) a = (int16_t)(63 - a);
	int16_t b = (int16_t)((uwFrame * 3) & 63);
	if(b > 31) b = (int16_t)(63 - b);
	return (int16_t)((a + b) >> 2);
}

static void rotorTick(tGame *g) {
	const tChopper *c = &g->chop;
	uint8_t ubLoad = g->aboard > CABIN_MAX ? CABIN_MAX : g->aboard;
	if(g->events & EV_TAKEOFF) g->rotorBoost = ROTOR_BOOST_FRAMES;
	else if(g->rotorBoost) --g->rotorBoost;
	int16_t wPer = ROTOR_PER_IDLE;
	int16_t wVol = 0;
	if(c->alive && (g->state == GS_PLAY || g->state == GS_INTRO)) {
		if(c->landed) {
			g->rotorBoost = 0;
			wVol = g->state == GS_INTRO ? 16 : 26;
		}
		else {
			int16_t wSpeed = absw(c->vx);
			int16_t wClimb = c->vy < 0 ? (int16_t)-c->vy : 0;
			wPer = (int16_t)(ROTOR_PER_HOVER - ((wSpeed * 45) >> 4) - wClimb * 4);
			if(c->vy > 0) wPer = (int16_t)(wPer + (c->vy >> 1));   // v4: settling down, a little lower and slower (2 px/frame: +16, 2.7 %)
			wVol = (int16_t)(40 + ((wSpeed * 11) >> 7) + (wClimb ? 3 : 0));   // (speed/3 rose to 56 flat out)
			// v3: a load or a hurt engine only ever sounds lower and heavier, never faster
			int16_t wHeavy = (int16_t)(ubLoad * ROTOR_HEAVY_PER_HEAD);
			if(c->damaged) wHeavy = (int16_t)(wHeavy + ROTOR_HURT_BASE + rotorWobble(g->frame));
			wPer = (int16_t)(wPer + wHeavy);
			wVol = (int16_t)(wVol + (ubLoad >> 2));
			if(g->rotorBoost) {
				// the spin-up: the empty chopper's is the lively one; a heavy one lifts off calmly
				wPer = (int16_t)(wPer - g->rotorBoost * (wHeavy ? 1 : 3));
				wVol = (int16_t)(wVol + (wHeavy ? g->rotorBoost / 20 : g->rotorBoost / 10));
			}
			int16_t wMin = g->rotorBoost && !wHeavy ? ROTOR_PER_MIN_BOOST : ROTOR_PER_MIN;
			if(wPer < wMin) wPer = wMin;
			if(wVol > 62) wVol = 62;
		}
	}
	else if(g->rotorPer) wPer = (int16_t)g->rotorPer;   // (paused on a menu: hold the pitch)
	if(!g->rotorPer) g->rotorPer = (uint16_t)wPer;
	// slew: the pitch rises fast (the blades spin up), falls slowly; the volume likewise
	int16_t wCur = (int16_t)g->rotorPer;
	if(wPer < wCur) wCur = (int16_t)(wCur - wPer > 16 ? wCur - 16 : wPer);
	else if(wPer > wCur) wCur = (int16_t)(wPer - wCur > 3 ? wCur + 3 : wPer);
	g->rotorPer = (uint16_t)wCur;
	int16_t wV = g->rotorVol;
	if(wVol > wV) wV = (int16_t)(wVol - wV > 2 ? wV + 2 : wVol);
	else if(wVol < wV) wV = (int16_t)(wV - wVol > 1 ? wV - 1 : wVol);
	if(!wVol && !c->alive) wV = 0;
	g->rotorVol = (uint8_t)wV;
}

uint16_t logicRotorPeriod(const tGame *g) { return g->rotorPer; }
uint16_t logicRotorHwPeriod(uint16_t uwLogical) { return (uint16_t)((uwLogical + ROTOR_OS / 2) / ROTOR_OS); }

// The loop (v4: made at ROTOR_OS times the rate the logic's periods are for, so Paula's sample-rate images
// sit at 12 kHz, not 6): a deep blade thump on every pass. Three damped resonators kicked together (the
// weight, 55 Hz, and the body, 130 and 230 Hz), a soft band-passed noise slap (two poles, ~120-450 Hz), a
// quiet rumble. A warm-up pass first (the last pass again, so the resonators' tails are in the first one and
// the loop closes seamlessly), then the loop. Peak ~124. sound/tools/make_rotor.py mirrors this with the
// same integer arithmetic: keep the two in step.
static int32_t rotorSynthRun(int8_t *pOut, int16_t wScale) {
	static const int16_t pA1[3] = {32722, 32614, 32396};     // 2 r cos w, Q14, for 12003 Hz
	static const int16_t pA2[3] = {16352, 16306, 16248};     // r^2, Q14
	static const int16_t pKick[3] = {750, 1150, 600};
	static const uint8_t pStrength[ROTOR_PASSES] = {70, 60};
	uint16_t uwLfsr = 0x1D2B;
	int32_t pY1[3] = {0, 0, 0}, pY2[3] = {0, 0, 0};
	int32_t lLp1 = 0, lLp1b = 0, lLp2 = 0, lRum = 0, lMax = 0;
	for(uint8_t ubRun = 0; ubRun < ROTOR_PASSES + 1; ++ubRun) {
		uint8_t k = ubRun ? (uint8_t)(ubRun - 1) : ROTOR_PASSES - 1;   // (run 0 is the warm-up: the last pass)
		for(uint16_t t = 0; t < ROTOR_LOOP_PASS; ++t) {
			uint16_t uwLsb = uwLfsr & 1;
			uwLfsr >>= 1;
			if(uwLsb) uwLfsr ^= 0xB400;
			int32_t lNoise = (int32_t)(uwLfsr & 0xFF) - 128;
			int32_t lThump = 0;
			for(uint8_t j = 0; j < 3; ++j) {
				int32_t lY = ((((int32_t)pA1[j] * (int16_t)pY1[j]) - ((int32_t)pA2[j] * (int16_t)pY2[j])) >> 14) + (t == 0 ? pKick[j] : 0);
				pY2[j] = pY1[j];
				pY1[j] = lY;
				lThump += lY;
			}
			lThump >>= 4;
			// the slap: noise through two low-pass poles minus a slow one, enveloped
			lLp1 += ((int32_t)(int16_t)(lNoise * 16 - lLp1) * 40) >> 8;
			lLp1b += ((int32_t)(int16_t)(lLp1 - lLp1b) * 40) >> 8;
			lLp2 += ((int32_t)(int16_t)(lLp1b - lLp2) * 12) >> 8;
			int32_t lBand = (lLp1b - lLp2) >> 2;
			int16_t wTs = (int16_t)t - 20;
			int32_t lEnv = wTs < 0 || wTs >= 300 ? 0 : wTs < 16 ? (int32_t)wTs * 16 : 256 - (((int32_t)(int16_t)(wTs - 16) * (int16_t)230) >> 8);
			int32_t lSlap = (((int32_t)(int16_t)lBand * (int16_t)lEnv >> 7) * (int16_t)pStrength[k]) >> 7;
			lRum += (lNoise * 16 - lRum) >> 7;
			int32_t v = lThump + lSlap + (lRum >> 4);
			if(!ubRun) continue;
			if(!wScale) {
				int32_t lAbs = v < 0 ? -v : v;
				if(lAbs > lMax) lMax = lAbs;
				continue;
			}
			int32_t s = ((int32_t)(int16_t)v * wScale) >> 8;
			pOut[(uint16_t)(k * ROTOR_LOOP_PASS + t)] = (int8_t)(s > 127 ? 127 : s < -128 ? -128 : s);
		}
	}
	return lMax;
}

void logicRotorSynth(int8_t *pOut) {
	int32_t lPeak = rotorSynthRun(pOut, 0);
	rotorSynthRun(pOut, (int16_t)((126L << 8) / (lPeak ? lPeak : 1)));   // peak at ~124
}

uint8_t logicRotorVolume(const tGame *g) { return g->rotorVol; }
int logicRotorThumpsX10(uint16_t uwPeriod) {
	return (int)((ROTOR_CLOCK * 10) / ((int32_t)uwPeriod * ROTOR_PASS));
}

// The least frames between two plays of one effect. Explosions and landings can come in
// clusters, a tank or a jet twice at once; none should machine-gun the two effect channels.
uint8_t logicSfxGap(uint8_t ubKind) {
	static const uint8_t pGap[SFXK_COUNT] = {
		8,    // boom
		50,   // jetwarn
		50,   // jet
		12,   // tankfire
		6,    // saved
		6,    // board
		12    // land
	};
	return ubKind < SFXK_COUNT ? pGap[ubKind] : 0;
}

uint8_t logicSfxAllow(uint16_t *pLast, uint16_t uwNow, uint8_t ubKind) {
	if(ubKind >= SFXK_COUNT) return 1;
	if(pLast[ubKind] && uwNow >= pLast[ubKind] && (uint16_t)(uwNow - pLast[ubKind]) < logicSfxGap(ubKind)) return 0;
	pLast[ubKind] = uwNow ? uwNow : 1;
	return 1;
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
// (world 2's drawing, in small code: not inlined at each of its dozens of calls)
COLD static uint8_t drawAddS(tDraw *pOut, uint8_t n, int16_t sx, int16_t sy, uint8_t bob, uint8_t frame) {
	return drawAdd(pOut, n, sx, sy, bob, frame);
}

// World 2's pads, one BOB each (an end cap, n 8 px units, the H, n units, a cap: n 1-4, the pad's
// width by skill; main.c puts each width together at the card). Two lamps blink over its ends once the
// chopper is within PAD_LAMP_REACH px: yellow while its skids are over the pad, red while not; the
// blink hides them a quarter of the time.
#define PAD_LAMP_REACH 150
COLD static uint8_t padsDraw(const tGame *g, tDraw *pOut, uint8_t n) {
	int16_t cam = g->cam;
	for(uint8_t i = 0; i < W2_ISLANDS; ++i) {
		uint8_t ubPx = logicPadPx(g, i);
		int16_t x0 = (int16_t)(g_pIslands[i].padX - ubPx / 2 - cam);
		if(x0 > SCREEN_W || x0 + ubPx < 0) continue;
		n = drawAddS(pOut, n, x0, PAD_Y - PAD_SLAB_ROW, (uint8_t)(BOB_PAD48 + ((ubPx - 48) >> 4)), 0);
		if(g->chop.alive && absw((int16_t)(g->chop.x - g_pIslands[i].padX)) < PAD_LAMP_REACH && ((g->frame >> 3) & 3) != 3) {
			uint8_t ubLamp = logicPadAt(g, g->chop.x) == i ? PAD_LAMP_YELLOW : PAD_LAMP_RED;
			n = drawAddS(pOut, n, (int16_t)(x0 + 10), PAD_Y - 8, BOB_PAD_LAMP, ubLamp);
			n = drawAddS(pOut, n, (int16_t)(x0 + ubPx - 15), PAD_Y - 8, BOB_PAD_LAMP, ubLamp);
		}
	}
	return n;
}

// World 2's sea: the boats and the splash
COLD static uint8_t seaDraw(const tGame *g, tDraw *pOut, uint8_t n) {
	int16_t cam = g->cam;
	for(uint8_t i = 0; i < BOAT_MAX; ++i) {
		const tBoat *b = &g->pBoats[i];
		if(!b->active || b->x + 34 < cam || b->x - 34 > cam + SCREEN_W) continue;   // (off screen: nothing to add)
		if(b->kind == BOAT_PATROL) {
			n = drawAddS(pOut, n, (int16_t)(b->x - 21 - cam), BOAT_BOTTOM_Y - 17, BOB_PATROL, (uint8_t)((b->dir > 0 ? PATROL_R : PATROL_L) + (b->tell ? PATROL_TELL_L : 0)));
		}
		else {
			uint8_t f = b->tell ? ((b->tell & 4) ? GUNBOAT_FIRE_L : GUNBOAT_TELL_L) : b->fireAnim ? GUNBOAT_FIRE_L : GUNBOAT_L;
			n = drawAddS(pOut, n, (int16_t)(b->x - 30 - cam), BOAT_BOTTOM_Y - 30, BOB_GUNBOAT, (uint8_t)(f + (b->dir > 0 ? GUNBOAT_R : 0)));
		}
	}
	if(g->splashTimer) {
		n = drawAddS(pOut, n, (int16_t)(g->splashX - 12 - cam), FEET_Y - 18, BOB_SPLASH, (uint8_t)((SPLASH_FRAMES - g->splashTimer) / 4));
	}
	return n;
}

// World 3's pilot scenery (SPEC 13.7): the lodge and the AA nest stand on their benches, the pad's slab top is the bed's floor (the
// skids rest on its second row, as world 2's). The nest idles: its tell and fire frames are the behaviour slice's.
COLD static uint8_t w3Draw(const tGame *g, tDraw *pOut, uint8_t n) {
	int16_t cam = g->cam;
	n = drawAddS(pOut, n, (int16_t)(W3_LODGE_X - cam), (int16_t)(logicW3SurfaceY(W3_LODGE_X + 26) - 32), BOB_W3_LODGE, 0);
	n = drawAddS(pOut, n, (int16_t)(W3_NEST_X - cam), (int16_t)(logicW3SurfaceY(W3_NEST_X + 15) - 24), BOB_W3_NEST, 0);
	n = drawAddS(pOut, n, (int16_t)(W3_PAD_X - cam), (int16_t)(logicW3SurfaceY(W3_PAD_X + W3_PAD_W / 2) - 2 - PAD_SLAB_ROW), BOB_PAD64, 0);
	return n;
}

uint8_t logicDraw(const tGame *g, tDraw *pOut) {
	uint8_t n = 0;
	int16_t cam = g->cam;
	// structures (world 1's base and fence, world 2's hangar)
	if(!logicIsW3(g)) n = drawAdd(pOut, n, (int16_t)(logicHangarX(g) - cam), FEET_Y - 40, BOB_HANGAR, 0);
	if(!g->world) {
		n = drawAdd(pOut, n, (int16_t)(BASE_PAD_X0 - 20 - cam), FEET_Y - 24, BOB_FLAG, (uint8_t)((g->frame >> 4) & 1));
		n = drawAdd(pOut, n, (int16_t)(BORDER_X - 8 - cam), FEET_Y - 32, BOB_FENCE, 0);
	}
	for(uint8_t b = 0; b < BARRACKS_COUNT; ++b) {
		const tBarracks *pB = &g->pBarracks[b];
		uint8_t f = pB->hp == 2 ? BARRACKS_OK : pB->hp == 1 ? BARRACKS_HIT :
			(uint8_t)(BARRACKS_OPEN0 + ((g->frame >> 3) & 1));
		if(logicIsW2(g)) n = drawAdd(pOut, n, (int16_t)(pB->x - 25 - cam), FEET_Y - 30, BOB_W2_BARRACKS, f);
		else n = drawAdd(pOut, n, (int16_t)(pB->x - 24 - cam), FEET_Y - 28, BOB_BARRACKS, f);
	}
	if(logicIsW2(g)) n = padsDraw(g, pOut, n);
	else if(logicIsW3(g)) n = w3Draw(g, pOut, n);
	// the chopper's shadow, on the track under it (and under the people)
	if(logicChopperVisible(g) && !logicIsW3(g) && !(logicIsW2(g) && logicOnWater(g->chop.x))) {
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
	if(logicIsW2(g)) n = seaDraw(g, pOut, n);
	// air
	for(uint8_t i = 0; i < JET_MAX; ++i) {
		const tJet *j = &g->pJets[i];
		if(j->active) {
			// the tell: the nose and engine glow, blinking every 4 frames
			uint8_t f = (uint8_t)((j->dir < 0 ? JET_L : JET_R) + ((j->tell & 4) ? 2 : 0));
			n = drawAdd(pOut, n, (int16_t)(j->x - 16 - cam), (int16_t)(j->y - 6), logicIsW2(g) ? BOB_W2_JET : BOB_JET, f);
		}
	}
	for(uint8_t i = 0; i < MINE_MAX; ++i) {
		const tMine *m = &g->pMines[i];
		if(m->active) n = drawAdd(pOut, n, (int16_t)(m->x - 8 - cam), (int16_t)(m->y - 6), BOB_MINE, (uint8_t)((m->anim >> 3) & 1));
	}
	for(uint8_t i = 0; i < SMOKE_MAX; ++i) {
		const tSmoke *p = &g->pSmoke[i];
		if(p->active) n = drawAdd(pOut, n, (int16_t)(p->x - 8 - cam), (int16_t)(p->y - 4), BOB_SMOKE,
			p->age >= 16 ? SMOKE_THIN : p->age >= 8 ? SMOKE_BIG : SMOKE_SMALL);
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

// T3: the bands per world (logic.h). World 0: clouds 72 (40 rows), mountains + far strip 120 (60), the floor 180 (19), the
// hills from 199 (their row 2; rows 0-1 are empty), the ground 224 (32). World 1: the sea 180 (28), the strip 208 (48, 1:1).
// World 2: the ceiling strip 44 (41, 1:1; M8 and M9), the far range 88 (42, /4), the strip 130 (126, 1:1), no floor or hills.
static const tBandLayout s_pBandLayout[3] = {
	{180, {{72, 40, 3, 0}, {120, 60, 2, 1}, {180, 19, 0, 2}, {199, 25, 1, 2}, {224, 32, 0, 2}}},
	{180, {{72, 40, 3, 0}, {120, 60, 2, 1}, {180, 28, 0, 2}, {0, 0, 0, 0}, {208, 48, 0, 3}}},
	{130, {{44, 41, 0, 0}, {88, 42, 2, 1}, {0, 0, 0, 0}, {0, 0, 0, 0}, {130, 126, 0, 2}}},
};
const tBandLayout *logicBands(uint8_t ubWorld) { return &s_pBandLayout[ubWorld > 2 ? 2 : ubWorld]; }

void logicStripNew(int16_t wLo, int16_t wOldLo, uint8_t ubSpan, int16_t *pFrom, int16_t *pTo) {
	int16_t wFrom = wLo, wTo = (int16_t)(wLo + ubSpan);
	if(wLo > wOldLo && wLo < wOldLo + ubSpan) wFrom = (int16_t)(wOldLo + ubSpan);          // moved right: the new right end
	else if(wLo < wOldLo && wLo + ubSpan > wOldLo) wTo = wOldLo;                              // moved left: the new left end
	else if(wLo == wOldLo) wTo = wFrom;                                                       // not moved
	*pFrom = wFrom;
	*pTo = wTo;
}

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

uint16_t logicBandOffset4(int16_t wCam, uint8_t ubShift, uint16_t uwLoopW) {
	// (the camera is whole pixels: cam * 4 >> shift is exact for shifts 0-2,
	// and halves for 3: 1/8 speed in 1/2 px)
	uint16_t x = (uint16_t)((uint16_t)wCam << 2) >> ubShift;
	uint16_t uwLoop4 = (uint16_t)(uwLoopW << 2);
	while(x >= uwLoop4) x = (uint16_t)(x - uwLoop4);
	return x;
}

uint16_t logicScrollCon1Pf2Aga(uint16_t uwX4) {
	// a delay of 64 - x4 % 64 quarters (the early fetch word, as above)
	uint16_t d = (uint16_t)((64 - (uwX4 & 63)) & 63);
	return (uint16_t)(((d >> 2) << 4) | ((d & 3) << 12));
}

int16_t logicScrollByteOffset4(uint16_t uwX4) {
	return (int16_t)((((int16_t)uwX4 - 1) >> 6) * 2);
}

// The plain's perspective floor (logic.h). Row j at camera c: (c mod 576) *
// (18 + j) / 18 quarters = c + c j / 18, its loop 32 (18 + j) quarters.
_Static_assert(FLOOR_PERIOD == 32 * (FLOOR_LINES - 1), "every floor row's loop is whole pixels");

#ifndef __m68k__   // (the host tests' reference: not in the Amiga build, whose slow RAM is full)
uint16_t logicFloorRowW(uint8_t j) {
	return (uint16_t)(8 * (FLOOR_LINES - 1 + j));
}

uint16_t logicFloorX4(int16_t wCam, uint8_t j) {
	uint16_t c = (uint16_t)wCam % FLOOR_PERIOD;
	return (uint16_t)(c + (uint16_t)(c * j) / (FLOOR_LINES - 1));
}
#endif

// BPLCON1 (PF2) for a left edge at x4 % 64 quarters: logicScrollCon1Pf2Aga()
#define CON1Q(i) ((uint16_t)((((((64 - (i)) & 63) >> 2) << 4)) | ((((64 - (i)) & 63) & 3) << 12)))
#define CON1Q8(i) CON1Q(i), CON1Q(i + 1), CON1Q(i + 2), CON1Q(i + 3), CON1Q(i + 4), CON1Q(i + 5), CON1Q(i + 6), CON1Q(i + 7)
static const uint16_t s_pFloorCon1[64] = {
	CON1Q8(0), CON1Q8(8), CON1Q8(16), CON1Q8(24), CON1Q8(32), CON1Q8(40), CON1Q8(48), CON1Q8(56),
};
// 65536 / 18 rounded up: (c << 16) + j c K, >> 16, is exactly c + floor(c j / 18)
// for every c < 576 and j <= 18 (the error, under 2 c j / 2^20, never reaches the
// next whole: unit-tested against logicFloorX4)
#define FLOOR_STEP_K 3641u

#if defined(__GNUC__) && !defined(__clang__)
__attribute__((optimize("Os")))   // (a short loop either way: small, for the A500's full slow RAM)
#endif
int16_t logicFloorScroll(int16_t wCam, uint16_t uwX4Mask, uint16_t uwModBase, uint16_t *pOut, uint8_t ubStride) {
	uint16_t c = (uint16_t)wCam;
	while(c >= FLOOR_PERIOD) c = (uint16_t)(c - FLOOR_PERIOD);
	uint32_t ulAcc = (uint32_t)c << 16;
	uint32_t ulStep = (uint32_t)c * (uint16_t)FLOOR_STEP_K;   // (16 x 16: mulu.w)
	// row 0: its offset goes to its bitplane pointers, row j's into line j - 1's modulo
	uint16_t x4 = (uint16_t)(ulAcc >> 16) & uwX4Mask;
	int16_t wOff0 = (int16_t)((((int16_t)x4 - 1) >> 5) & ~1), wPrev = wOff0;   // ((x4 - 1) >> 6) * 2
	pOut[0] = s_pFloorCon1[x4 & 63];
	uint16_t *pMod = pOut + 2;
	for(uint8_t j = 1; j < FLOOR_LINES; ++j) {
		pOut += ubStride;
		ulAcc += ulStep;
		x4 = (uint16_t)(ulAcc >> 16) & uwX4Mask;
		int16_t wOff = (int16_t)((((int16_t)x4 - 1) >> 5) & ~1);
		pOut[0] = s_pFloorCon1[x4 & 63];
		*pMod = (uint16_t)(uwModBase + wOff - wPrev);
		pMod = pOut + 2;
		wPrev = wOff;
	}
	return wOff0;
}

// World 2's sea (logic.h). Row j at camera c (mod 448): c (28 + 3 j) / 28
// quarters = c + 3 c j / 28; with 3 c = 28 q + b that is c + q j + (b j) / 28,
// so each line adds q and b (a carry when the 28ths pass a whole). Its loop
// is 16 (28 + 3 j) quarters, the same 448 px of camera for every row.
_Static_assert(SEA_PERIOD == 4 * 112, "every sea row's loop is whole pixels");
#ifndef __m68k__
uint16_t logicSeaX4(int16_t wCam, uint8_t j) {
	uint16_t c = (uint16_t)wCam % SEA_PERIOD;
	return (uint16_t)((uint32_t)c * (28u + 3u * j) / 28u);
}
#endif

#if defined(__GNUC__) && !defined(__clang__)
__attribute__((optimize("Os")))
#endif
int16_t logicSeaScroll(int16_t wCam, uint16_t uwX4Mask, uint16_t uwModBase, uint16_t *pOut, uint8_t ubStride) {
	uint16_t c = (uint16_t)wCam;
	while(c >= SEA_PERIOD) c = (uint16_t)(c - SEA_PERIOD);
	uint16_t uw3c = (uint16_t)(3 * c);
	// q = 3c / 28: 2341 = 65536 / 28 rounded up is exact for 3c < 1344 (the error stays
	// under 1/28); a 16 x 16 multiply (mulu.w), no divide
	uint16_t q = (uint16_t)(((uint32_t)uw3c * (uint16_t)2341u) >> 16);
	uint16_t b = (uint16_t)(uw3c - 28 * q), r = 0;
	uint16_t x4 = c;
	uint16_t x4m = x4 & uwX4Mask;
	int16_t wOff0 = (int16_t)((((int16_t)x4m - 1) >> 5) & ~1), wPrev = wOff0;
	pOut[0] = s_pFloorCon1[x4m & 63];
	uint16_t *pMod = pOut + 2;
	for(uint8_t j = 1; j < SEA_LINES; ++j) {
		pOut += ubStride;
		x4 = (uint16_t)(x4 + q);
		r = (uint16_t)(r + b);
		if(r >= 28) { r = (uint16_t)(r - 28); ++x4; }
		x4m = x4 & uwX4Mask;
		int16_t wOff = (int16_t)((((int16_t)x4m - 1) >> 5) & ~1);
		pOut[0] = s_pFloorCon1[x4m & 63];
		*pMod = (uint16_t)(uwModBase + wOff - wPrev);
		pMod = pOut + 2;
		wPrev = wOff;
	}
	return wOff0;
}

// World 2's terrain: tile classes of art/world2/tilemap.txt (0-7 water, 32-33 reef)
uint8_t logicTerrainFloorY(const uint8_t *pMap, int16_t x) {
	if(x < 0 || x >= W2_W) return TERRAIN_WATER;
	uint8_t t = pMap[(uint16_t)x >> 4];
	return (t < 8 || t == 32 || t == 33) ? TERRAIN_WATER : GROUND_Y;
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
__attribute__((optimize("Os")))   // (startup only: small beats fast on the A500's full slow RAM)
#endif
uint16_t logicSkyColorDither(uint8_t ubMission, uint8_t ubLine) {
	if(ubLine >= SKY_LINES) ubLine = SKY_LINES - 1;
	const int16_t n = SKY_LINES - 1;
	uint16_t out = 0;
	for(uint8_t s = 0; s < 12; s = (uint8_t)(s + 4)) {
		int16_t a = (int16_t)((s_pSkyTop[ubMission] >> s) & 15);
		int16_t b = (int16_t)((s_pSkyLow[ubMission] >> s) & 15);
		// the channel in half steps, rounded to nearest: odd = between two values
		int16_t num = (int16_t)(2 * (b - a) * (int16_t)ubLine);
		int16_t h = (int16_t)(2 * a + (num >= 0 ? (num + n / 2) / n : -((-num + n / 2) / n)));
		int16_t c = (int16_t)((h >> 1) + ((h & 1) & (ubLine & 1)));
		out |= (uint16_t)(c << s);
	}
	return out;
}

// A line-dithered gradient from colour uwA (line 0) to uwB (line ubLines - 1):
// per channel the half steps 2a + round(2 (b - a) line / n), kept as a quotient
// and a remainder; an odd half step shows the upper value on odd lines
COLD static void ditherFill(uint16_t uwA, uint16_t uwB, uint8_t ubLines, uint16_t *pOut) {
	const int16_t n = (int16_t)(ubLines - 1);
	for(uint8_t y = 0; y < ubLines; ++y) pOut[y] = 0;
	for(uint8_t s = 0; s < 12; s = (uint8_t)(s + 4)) {
		int16_t a = (int16_t)((uwA >> s) & 15);
		int16_t b = (int16_t)((uwB >> s) & 15);
		int16_t d = (int16_t)(2 * (b - a)), sgn = 1;
		if(d < 0) { d = (int16_t)-d; sgn = -1; }
		int16_t q = 0, r = n / 2;      // (|num| + n/2) = q n + r
		for(uint8_t y = 0; y < ubLines; ++y) {
			int16_t h = (int16_t)(2 * a + sgn * q);
			pOut[y] |= (uint16_t)(((h >> 1) + ((h & 1) & (y & 1))) << s);
			r = (int16_t)(r + d);
			while(r >= n) { r = (int16_t)(r - n); ++q; }
		}
	}
}

COLD void logicSkyFillDither(uint8_t ubMission, uint16_t *pOut) {
	// logicSkyColorDither line by line
	ditherFill(s_pSkyTop[ubMission], s_pSkyLow[ubMission], SKY_LINES, pOut);
}

COLD void logicGradFill(uint16_t uwA, uint16_t uwB, uint8_t ubLines, uint16_t *pOut) {
	ditherFill(uwA, uwB, ubLines, pOut);
}

#if defined(__GNUC__) && !defined(__clang__)
__attribute__((optimize("Os")))   // (A1200 startup only)
#endif
COLD uint32_t logicGrad24(uint32_t ulA, uint32_t ulB, uint8_t ubLine, uint8_t ubLines) {
	// per 8-bit channel a + (b - a) line / (lines - 1), rounded to nearest (32-bit maths: startup)
	const int32_t n = (int32_t)ubLines - 1;
	uint32_t ulOut = 0;
	for(uint8_t s = 0; s < 24; s = (uint8_t)(s + 8)) {
		int32_t a = (int32_t)((ulA >> s) & 0xFF), b = (int32_t)((ulB >> s) & 0xFF);
		int32_t num = (b - a) * (int32_t)ubLine;
		int32_t c = a + (num >= 0 ? (num + n / 2) / n : -((-num + n / 2) / n));
		ulOut |= (uint32_t)c << s;
	}
	return ulOut;
}

// The plain's ground: the haze (the sky's own colour on the floor's first line,
// so it continues the sky above) to C. (FLOOR_Y - PLAY_TOP is even: the dither's
// odd lines are the screen's, as the sky's.)
_Static_assert(((FLOOR_Y - PLAY_TOP) & 1) == 0, "the floor's dither in step with the sky's");
COLD void logicFloorFillGround(uint8_t ubMission, uint16_t *pOut) {
	ditherFill(logicSkyColor(ubMission, FLOOR_Y - PLAY_TOP), g_pMissionLandPal[ubMission][6], FLOOR_LINES, pOut);
}

COLD uint32_t logicFloorGround24(uint8_t ubMission, uint8_t j) {
	// (A1200 startup only: 32-bit maths) the 24-bit sky at FLOOR_Y to C x 17
	uint32_t ulA = logicSkyColor24(ubMission, FLOOR_Y - PLAY_TOP), ulOut = 0;
	const int32_t n = FLOOR_LINES - 1;
	for(uint8_t s = 0; s < 24; s = (uint8_t)(s + 8)) {
		int32_t a = (int32_t)((ulA >> s) & 0xFF);
		int32_t b = (int32_t)((g_pMissionLandPal[ubMission][6] >> (s / 2)) & 15) * 17;
		int32_t num = (b - a) * (int32_t)j;
		int32_t c = a + (num >= 0 ? (num + n / 2) / n : -((-num + n / 2) / n));
		ulOut |= (uint32_t)c << s;
	}
	return ulOut;
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

// World 2's brief (mission 5 and 6 differ in how many boats; the winds, the radar and the
// destroyer are the next slice)
static const char *const s_pW2Brief[MISSION_COUNT] = {
	"PATROL BOATS AND GUNBOATS", "GUNBOATS GUARD THE SEA LANES", "GUNBOATS HOLD THE STRAIT",
};
// ... and where jets can appear at the skill in force (decided 2026-11-17: the card must be honest; 24-33 characters, one line)
static const char *const s_pW2BriefJets[MISSION_COUNT] = {
	"PATROL BOATS, GUNBOATS AND JETS", "GUNBOATS AND JETS GUARD THE LANES", "GUNBOATS AND JETS HOLD THE STRAIT",
};

COLD const char *logicBrief(const tGame *g) {
	if(logicIsW3(g)) return g->mission ? "NOT BUILT: PILOT PALETTE" : "A PASS, A PAD, A NEST";
	if(g->world) return logicJetsPossible(g) ? s_pW2BriefJets[g->mission] : s_pW2Brief[g->mission];
	// the threats this mission starts with (trip 0, this difficulty)
	if(logicMinesOn(g)) return "TANKS, JETS AND AIR MINES";
	if(logicJetsOn(g)) return "TANKS AND JETS ON PATROL";
	return "TANKS ON PATROL";
}

static uint8_t routeX(int16_t x) {
	return (uint8_t)((uint16_t)x / ROUTE_SCALE);
}

// World 2's map: 4608 px at an 18th, the sea dark, the islands on it, the barracks, pads
// and hangar, the base island ours (the boats are in the brief: the map is made once, at the load)
static uint8_t routeX2(int16_t x) {
	return (uint8_t)(((uint32_t)(uint16_t)x * 3641u) >> 16);
}

COLD static uint8_t routeMap2(const tGame *g, tRouteMark *pOut) {
	uint8_t n = 0;
	const tIsland *pB = &g_pIslands[W2_BASE_ISLAND];
	pOut[n++] = (tRouteMark){ROUTE_HOME, routeX2(pB->x0), (uint8_t)(routeX2(pB->x1) - routeX2(pB->x0))};
	for(uint8_t i = 0; i < W2_ISLANDS; ++i) {
		pOut[n++] = (tRouteMark){ROUTE_ISLAND, routeX2(g_pIslands[i].x0), (uint8_t)(routeX2(g_pIslands[i].x1) - routeX2(g_pIslands[i].x0))};
	}
	for(uint8_t b = 0; b < BARRACKS_COUNT; ++b) pOut[n++] = (tRouteMark){ROUTE_BARRACKS, routeX2(g->pBarracks[b].x), 7};
	for(uint8_t i = 0; i < W2_ISLANDS; ++i) {
		pOut[n++] = (tRouteMark){ROUTE_PAD, routeX2((int16_t)(g_pIslands[i].padX - logicPadPx(g, i) / 2)), (uint8_t)(logicPadPx(g, i) / 18 + 1)};
	}
	pOut[n++] = (tRouteMark){ROUTE_HANGAR, routeX2(W2_HANGAR_X), 64 / 18};
	pOut[n++] = (tRouteMark){ROUTE_HERE, routeX2(g->chop.x), 1};
	return n;
}

COLD uint8_t logicRouteMap(const tGame *g, tRouteMark *pOut) {
	if(logicIsW3(g)) return 0;   // (the pilot's card has no map)
	if(logicIsW2(g)) return routeMap2(g, pOut);
	uint8_t n = 0, ubFence = routeX(BORDER_X);
	pOut[n++] = (tRouteMark){ROUTE_ENEMY, 0, ubFence};
	pOut[n++] = (tRouteMark){ROUTE_HOME, (uint8_t)(ubFence + 1), (uint8_t)(ROUTE_W - 1 - ubFence)};
	pOut[n++] = (tRouteMark){ROUTE_FENCE, ubFence, 1};
	for(uint8_t b = 0; b < BARRACKS_COUNT; ++b) {
		pOut[n++] = (tRouteMark){ROUTE_BARRACKS, routeX(g->pBarracks[b].x), 7};
	}
	pOut[n++] = (tRouteMark){ROUTE_PAD, routeX(BASE_PAD_X0), (uint8_t)(routeX(BASE_PAD_X1) - routeX(BASE_PAD_X0))};
	pOut[n++] = (tRouteMark){ROUTE_HANGAR, routeX(HANGAR_X), 64 / ROUTE_SCALE};
	pOut[n++] = (tRouteMark){ROUTE_HERE, routeX(g->chop.x), 1};
	return n;
}

const char *logicRank(uint16_t uwSaved) {
	if(uwSaved >= 180) return "LEGEND";
	if(uwSaved >= 140) return "HERO";
	if(uwSaved >= 90) return "CAPTAIN";
	if(uwSaved >= 40) return "PILOT";
	return "ROOKIE";
}
