// Host-side measurement: how soon does a jet that appears BEHIND the chopper start an attack run
// (and reach it), by what the chopper is doing? Forces random seeds until a spawn is from behind.
//
//   gcc -O2 -Isrc -o /tmp/jetbehind tools/jetbehind.c src/logic.c && /tmp/jetbehind [skill]
//
// Per chopper behaviour (cruise right at top speed, slow 1.5 px/frame, hover, flying away then
// stopping dead at 60 frames, flying away then turning round at 60): frames from the jet's
// appearance to its first attack run (flash start), its first missile, and to contact (ram), and
// the distance behind at the start. docs/SPEC.md section 5, "Jets from behind".
#include <stdio.h>
#include <stdlib.h>
#include "logic.h"

static tGame g;

enum { B_CRUISE, B_SLOW, B_HOVER, B_STOP60, B_TURN60, B_COUNT };
static const char *s_pName[B_COUNT] = {"cruise 3 px/f", "slow 1.5 px/f", "hover", "cruise, stop at 1.2 s", "cruise, turn round at 1.2 s"};

static tInput drive(int beh, int f) {
	tInput in = {0};
	switch(beh) {
	case B_CRUISE: in.dx = 1; break;
	case B_SLOW: in.dx = (f % 2) ? 1 : 0; break;
	case B_HOVER: break;
	case B_STOP60: in.dx = f < 60 ? 1 : 0; break;
	case B_TURN60: in.dx = f < 60 ? 1 : -1; break;
	}
	return in;
}

int main(int argc, char **argv) {
	int skill = argc > 1 ? atoi(argv[1]) : 1;
	for(int beh = 0; beh < B_COUNT; ++beh) {
		long n = 0, sumAtk = 0, sumFire = 0, sumHit = 0, minAtk = 99999, minFire = 99999, minHit = 99999;
		int cntFire = 0, cntHit = 0;
		for(int seed = 1; seed <= 400; ++seed) {
			logicInit(&g);
			logicStartGame(&g);
			tInput none = {0};
			for(int i = 0; i < INTRO_FRAMES + 1; ++i) logicUpdate(&g, &none);
			logicSetDifficulty(&g, (uint8_t)skill);
			g.ghost = 1;
			g.trip = 10;
			g.lfsr = (uint16_t)(seed * 40503u + 7);
			// airborne mid-map, cruising right
			g.chop.x = 1200; g.chop.y = 120; g.chop.landed = 0; g.chop.face = g.chop.faceTarget = FACE_RIGHT;
			g.chop.vx = beh == B_HOVER ? 0 : beh == B_SLOW ? 24 : 48;
			g.cam = 1200 - 160;
			g.spawnTimer = 60000; g.airGrace = 0; g.landFrames = 0;
			g.pJets[0].cool = 1; g.pJets[1].cool = 255; g.pJets[1].active = 0;
			int born = -1, atk = -1, fire = -1, hit = -1, behind = 0;
			uint16_t a0 = g.jetAttacks;
			for(int f = 0; f < 900; ++f) {
				g.spawnTimer = 60000;
				tInput in = drive(beh, f);
				if(beh == B_HOVER) { g.chop.vx = 0; }
				uint8_t was = g.pJets[0].active;
				logicUpdate(&g, &in);
				if(born < 0 && g.pJets[0].active && !was) {
					born = f;
					behind = g.pJets[0].dir > 0;   // flying right, chopper facing right: it overtakes
					if(!behind) break;
				}
				if(born >= 0) {
					if(atk < 0 && g.jetAttacks != a0) atk = f - born;
					if(fire < 0 && (g.events2 & EV2_JETFIRE)) fire = f - born;
					if(hit < 0 && (g.events & EV_SHRUG)) hit = f - born;
					if(!g.pJets[0].active && f > born) break;
				}
			}
			if(born < 0 || !behind) continue;
			++n;
			if(atk >= 0) { sumAtk += atk; if(atk < minAtk) minAtk = atk; }
			if(fire >= 0) { sumFire += fire; ++cntFire; if(fire < minFire) minFire = fire; }
			if(hit >= 0) { sumHit += hit; ++cntHit; if(hit < minHit) minHit = hit; }
		}
		printf("%-28s skill %d: %3ld from-behind jets; first attack run at min %5ld frames; first missile %d of them min %5ld; contact %d of them min %5ld\n",
			s_pName[beh], skill, n, minAtk, cntFire, minFire, cntHit, minHit);
	}
	return 0;
}
