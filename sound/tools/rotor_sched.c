// Host tool for sound/tools/make_rotor.py: drives the game's own rules (logic.c)
// with scripted flights and prints the rotor's Paula period and volume a frame.
//   gcc -I src -o /tmp/rotor_sched sound/tools/rotor_sched.c src/logic.c
//   rotor_sched hover|cruise|full|takeoff      -> "period volume" per frame
//   rotor_sched <kind>-old                     -> "speed climbing landed" per frame
//                                                 (for the old rule, which main.c had)
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
	char *pOld = strstr(szKind, "-old");
	if(pOld) { *pOld = 0; isOld = 1; }
	tInput none = {0}, right = {.dx = 1}, up = {.dy = -1};
	logicInit(&g);
	logicStartGame(&g);
	for(int i = 0; i < INTRO_FRAMES + 1; ++i) logicUpdate(&g, &none);
	g.spawnTimer = 60000;
	if(!strcmp(szKind, "takeoff")) {
		run(none, 100, -1, -1);     // 2 s idling on the skids
		run(up, 1, -1, -1);         // lift off,
		run(none, 150, -1, 100);    // then hold at 100 px
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
		else if(!strcmp(szKind, "cruise")) run(none, 250, 24, 100);
		else if(!strcmp(szKind, "full")) run(right, 250, -1, 100);
	}
	return 0;
}
