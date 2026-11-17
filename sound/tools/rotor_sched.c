// Host tool for sound/tools/make_rotor.py: drives the game's own rules (logic.c)
// with scripted flights and prints the rotor's Paula period and volume a frame.
//   gcc -I src -o /tmp/rotor_sched sound/tools/rotor_sched.c src/logic.c src/hiscore.c
//   rotor_sched hover|cruise|full|takeoff      -> "period volume" per frame (the logic's period; Paula's is logicRotorHwPeriod)
//   rotor_sched climb|descend|flight           -> v4: a climb, a descent, a whole flight (idle, lift-off, cruise, climb, descend, hover)
//   rotor_sched <kind>-old                     -> "speed climbing landed" per frame
//                                                 (for the old rule, which main.c had)
//   rotor_sched cruise|lift@dmg|l8|l16|dl16    -> v3's states: damaged, 8 or 16 aboard, damaged with 16.
//                                                 cruise: 5 s at 1.5 px/frame; lift: lift off away from
//                                                 the home pad (idle 2 s) and fly right 6 s
#include <stdio.h>
#include <string.h>
#include "logic.h"

static tGame g;
static int isOld, isQuiet;

static void emit(void) {
	if(isOld) {
		int vx = g.chop.vx < 0 ? -g.chop.vx : g.chop.vx;
		printf("%d %d %d\n", vx, g.chop.vy < 0, g.chop.landed);
	}
	else printf("%d %d\n", logicRotorPeriod(&g), logicRotorVolume(&g));
}

static void run(tInput in, int frames, int vx, int y) {
	for(int i = 0; i < frames; ++i) {
		if(vx >= 0) g.chop.vx = (int16_t)vx;   // a forced speed (cruise)
		if(y >= 0) { g.chop.y = (int16_t)y; g.chop.vy = 0; }
		logicUpdate(&g, &in);
		if(!isQuiet) emit();
	}
}

int main(int argc, char **argv) {
	char szKind[32];
	strncpy(szKind, argc > 1 ? argv[1] : "hover", sizeof(szKind) - 1);
	szKind[sizeof(szKind) - 1] = 0;
	int iAboard = 0, isDamaged = 0;
	char *pAt = strchr(szKind, '@');
	if(pAt) {
		*pAt++ = 0;
		if(!strcmp(pAt, "dmg")) isDamaged = 1;
		else if(!strcmp(pAt, "l8")) iAboard = 8;
		else if(!strcmp(pAt, "l16")) iAboard = 16;
		else if(!strcmp(pAt, "dl16")) { iAboard = 16; isDamaged = 1; }
	}
	char *pOld = strstr(szKind, "-old");
	if(pOld) { *pOld = 0; isOld = 1; }
	tInput none = {0}, right = {.dx = 1}, up = {.dy = -1}, down = {.dy = 1};
	logicInit(&g);
	logicStartGame(&g);
	for(int i = 0; i < INTRO_FRAMES + 1; ++i) logicUpdate(&g, &none);
	g.spawnTimer = 60000;
	g.aboard = (uint8_t)iAboard;
	g.chop.damaged = (uint8_t)isDamaged;
	if(!strcmp(szKind, "lift")) {
		g.chop.x = 1500;   // away from the home pad: it would unload and mend
		g.cam = (int16_t)(1500 - SCREEN_W / 2);
		run(none, 100, -1, -1);
		run(up, 25, -1, -1);
		run(right, 300, -1, -1);
	}
	else if(!strcmp(szKind, "takeoff")) {
		run(none, 100, -1, -1);     // 2 s idling on the skids
		run(up, 1, -1, -1);         // lift off,
		run(none, 150, -1, 100);    // then hold at 100 px
	}
	else if(!strcmp(szKind, "flight")) {
		g.chop.x = 1500;   // away from the home pad: it would unload and mend
		g.cam = (int16_t)(1500 - SCREEN_W / 2);
		run(none, 100, -1, -1);     // idle 2 s
		run(up, 25, -1, -1);        // lift off, spin up
		run(none, 150, -1, 100);    // hover 3 s
		run(right, 150, -1, 100);   // cruise 3 s
		run(up, 30, -1, -1);        // climb
		run(none, 60, -1, -1);
		run(down, 30, -1, -1);      // descend
		run(none, 100, -1, -1);     // settle
	}
	else {
		g.chop.landed = 0;
		g.chop.y = 100;
		g.chop.x = 1000;   // (open sky either side)
		g.cam = (int16_t)(1000 - SCREEN_W / 2);
		isQuiet = 1;
		run(none, 100, -1, 100);    // (the rotor settles to the hover, unheard)
		isQuiet = 0;
		if(!strcmp(szKind, "hover")) run(none, 150, -1, 100);
		else if(!strcmp(szKind, "cruise")) run(none, 250, 24, 100);   // (g.aboard / g.chop.damaged kept: the v3 states)
		else if(!strcmp(szKind, "full")) run(right, 250, -1, 100);
		else if(!strcmp(szKind, "climb")) { run(up, 40, -1, -1); run(none, 60, -1, -1); }
		else if(!strcmp(szKind, "descend")) { run(none, 40, -1, 60); run(down, 30, -1, -1); run(none, 80, -1, -1); }
	}
	return 0;
}
