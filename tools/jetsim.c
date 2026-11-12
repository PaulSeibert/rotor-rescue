// Host-side pacing simulation: a bot flies whole rescue trips (base -> barracks -> land, open,
// board -> base -> unload) on the real rules (src/logic.c, ghost mode: the difficulty's tables,
// an immortal chopper) and reports the jet pressure per trip and per barracks. Fast (seconds
// for hours of play), deterministic per seed. Build and run:
//
//   gcc -O2 -Isrc -o /tmp/jetsim tools/jetsim.c src/logic.c && /tmp/jetsim [skill] [mission] [seeds] [order]
//
// order: 3210 (default: nearest barracks first, as the player does) or 0123 (the far one first).
// Per trip: seconds, jet attack runs a minute, the share of time with a jet flash or missile up
// ("under attack"), would-be hits a minute (ghost shrugs), the share of time landed.
// (docs/SPEC.md section 5, "Last barracks"; a bot is not a person: compare trips, not absolutes.)
#include <stdio.h>
#include <stdlib.h>
#include "logic.h"

static tGame g;
static long s_bandFrames[12], s_bandAtk[12], s_bandAir[12];

typedef struct { long frames, atk, attacked, hits, landed, jets, hjet, hshell, hmine, tankon, spawns; } tStat;

enum { PH_OUT, PH_LAND, PH_BOARD, PH_HOME, PH_PAD, PH_DONE };

static void sample(tStat *s, tGame *gg, uint32_t prevEv) {
	++s->frames;
	if(gg->chop.landed) ++s->landed;
	uint8_t att = 0, jets = 0;
	for(int i = 0; i < JET_MAX; ++i) {
		if(gg->pJets[i].active) {
			if(gg->pJets[i].x > gg->cam - 16 && gg->pJets[i].x < gg->cam + SCREEN_W + 16) jets = 1;
			if(gg->pJets[i].tell) att = 1;
		}
	}
	for(int i = 0; i < ESHOT_MAX; ++i) if(gg->pEShots[i].active && gg->pEShots[i].kind != ESHOT_SHELL) att = 1;
	s->attacked += att;
	s->jets += jets;
	(void)prevEv;
}

int main(int argc, char **argv) {
	int skill = argc > 1 ? atoi(argv[1]) : 1;
	int mission = argc > 2 ? atoi(argv[2]) : 0;
	int seeds = argc > 3 ? atoi(argv[3]) : 20;
	const char *order = argc > 4 ? argv[4] : "3210";
	const int bx[4] = {420, 1080, 1700, 2240};
	tStat tot[4] = {{0}};
	for(int seed = 1; seed <= seeds; ++seed) {
		logicInit(&g);
		logicStartGame(&g);
		tInput none = {0};
		for(int i = 0; i < INTRO_FRAMES + 1; ++i) logicUpdate(&g, &none);
		logicSetDifficulty(&g, (uint8_t)skill);
		g.ghost = 1;
		if(mission) logicStartMission(&g, (uint8_t)mission);
		if(mission) for(int i = 0; i < INTRO_FRAMES + 1; ++i) logicUpdate(&g, &none);
		g.ghost = 1;
		g.lfsr = (uint16_t)(seed * 2654435761u + 1);
		if(!g.lfsr) g.lfsr = 1;
		for(int t = 0; t < 4; ++t) {
			int b = order[t] - '0';
			int tx = bx[b] + 94;
			int ph = PH_OUT, wait = 0, fireT = 0;
			long total = 0;
			tStat *s = &tot[t];
			g.chop.landed = 1;
			if(t == 0) { g.chop.x = 2880; g.chop.y = GROUND_Y; }
			while(ph != PH_DONE && total < 40000) {
				tInput in = {0};
				tChopper *c = &g.chop;
				++total;
				switch(ph) {
				case PH_OUT: {
					int d = tx - c->x;
					if(c->landed || c->y > 130) in.dy = -1;
					else in.dy = 0;
					if(!c->landed) {
						if(d < -70) in.dx = -1;
						else if(d > 20) in.dx = 1;
						if(d >= -70 && d <= 20) { in.dx = 0; ph = PH_LAND; }
					}
					break; }
				case PH_LAND:
					in.dy = 1;
					if(c->landed) { ph = PH_BOARD; wait = 0; }
					break;
				case PH_BOARD:
					++wait;
					if(g.pBarracks[b].hp) {
						if(c->face != FACE_LEFT) in.dx = -1;
						in.fire = (fireT++ / 4) & 1;
					}
					if(g.aboard >= 16 || wait > 900) { ph = PH_HOME; wait = 0; }
					break;
				case PH_HOME: {
					int d = 2883 - c->x;
					in.dy = (c->landed || c->y > 130) ? -1 : 0;
					if(!c->landed && c->y <= 140) in.dx = d > 45 ? 1 : 0;
					if(d <= 45 && !c->landed) { in.dx = 0; in.dy = 1; }
					if(c->x >= BASE_PAD_X0 && c->landed && c->x <= BASE_PAD_X1) { ph = PH_PAD; wait = 0; in.dy = 0; }
					break; }
				case PH_PAD:
					if(!g.aboard) { if(++wait > 90) ph = PH_DONE; }
					break;
				}
				if(getenv("DBG") && total % 100 == 0 && seed == 1 && t == 0) printf("f%ld ph%d x=%d y=%d land=%d vx=%d aboard=%d\n", total, ph, c->x, c->y, c->landed, c->vx, g.aboard);
				int nearJet = 0, nearMissile = 0, nearMine = 0;
				for(int i = 0; i < JET_MAX; ++i) if(g.pJets[i].active && abs(g.pJets[i].x - c->x) < 40 && abs(g.pJets[i].y - c->y) < 30) nearJet = 1;
				for(int i = 0; i < ESHOT_MAX; ++i) if(g.pEShots[i].active && g.pEShots[i].kind != ESHOT_SHELL && abs(g.pEShots[i].x - c->x) < 30 && abs(g.pEShots[i].y - c->y) < 30) nearMissile = 1;
				for(int i = 0; i < MINE_MAX; ++i) if(g.pMines[i].active && abs(g.pMines[i].x - c->x) < 30 && abs(g.pMines[i].y - c->y) < 30) nearMine = 1;
				uint32_t ev = g.events;
				uint16_t atk0 = g.jetAttacks;
				logicUpdate(&g, &in);
				s->atk += g.jetAttacks - atk0;
				if(g.events & EV_JET) ++s->spawns;
				{ int bi = c->x / 300; if(bi > 11) bi = 11; ++s_bandFrames[bi]; if(!c->landed) ++s_bandAir[bi]; s_bandAtk[bi] += g.jetAttacks - atk0; }
				if(g.events & EV_SHRUG) {
					++s->hits;
					if(nearJet || nearMissile) ++s->hjet; else if(nearMine) ++s->hmine; else ++s->hshell;
				}
				{ int n = 0; for(int i = 0; i < TANK_MAX; ++i) n += g.pTanks[i].active && abs(g.pTanks[i].x - c->x) < 260; s->tankon += n > 0; }
				sample(s, &g, ev);
			}
			if(ph != PH_DONE) printf("seed %d trip %d: stuck in phase %d at x=%d y=%d landed=%d aboard=%d\n", seed, t, ph, g.chop.x, g.chop.y, g.chop.landed, g.aboard);
		}
	}
	printf("skill %d mission %d order %s, %d seeds (trip n = barracks %c,%c,%c,%c)\n", skill, mission, order, seeds, order[0], order[1], order[2], order[3]);
	printf("trip barracks  secs/run  atk/min  attacked%%  jetonscr%%  hits/min (jet/shell/mine)  landed%%  tank-near%%  jets/trip  runs/trip\n");
	for(int t = 0; t < 4; ++t) {
		tStat *s = &tot[t];
		double min = s->frames / 50.0 / 60.0;
		printf("%4d %8c  %8.1f  %7.1f  %9.1f  %9.1f  %5.1f (%4.1f/%4.1f/%4.1f)  %6.1f  %6.1f  %9.2f  %9.2f\n", t, order[t], s->frames / 50.0 / seeds, s->atk / min,
			100.0 * s->attacked / s->frames, 100.0 * s->jets / s->frames, s->hits / min, s->hjet / min, s->hshell / min, s->hmine / min, 100.0 * s->landed / s->frames, 100.0 * s->tankon / s->frames, (double)s->spawns / seeds, (double)s->atk / seeds);
	}
	if(getenv("BANDS")) for(int i = 0; i < 11; ++i) printf("x %4d-%4d: %5.1f min/run  airborne %3.0f%%  attacks %.2f /airborne-min\n", i * 300, i * 300 + 299, s_bandFrames[i] / 3000.0 / seeds, s_bandFrames[i] ? 100.0 * s_bandAir[i] / s_bandFrames[i] : 0.0, s_bandAir[i] ? s_bandAtk[i] / (s_bandAir[i] / 3000.0) : 0.0);
	return 0;
}
