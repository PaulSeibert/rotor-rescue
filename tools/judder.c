// The judder meter: runs the real game rules (src/logic.c) on the host through
// a scripted flight and measures how evenly things move on screen, frame by frame.
//
//   cc -O2 -Isrc -o build/judder tools/judder.c src/logic.c && build/judder
//
// What makes motion look smooth at 50 fps is not speed but evenness: an object
// crossing at 2.5 px a frame should step 2,3,2,3 - not 2,2,4,1,3. For each
// measured thing this prints, over the frames where the input is steady:
//   moved   - share of frames it moved at all (25 Hz motion shows as ~50%)
//   jerk    - mean |step - previous step| (0 = perfectly even)
//   worst   - the largest single change of step
// "chopper on screen" (x - cam) is what the eye tracks: ideally it settles and
// stays put while the world scrolls under it.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "logic.h"

typedef struct {
	const char *name;
	int loop;            // a band's loop width: steps across the wrap are taken modulo it
	int has, prev, prevStep, n, moved, jerkSum, worst;
} tMeter;

static void meter(tMeter *m, int v, int steady) {
	if(m->has && steady) {
		int step = v - m->prev;
		if(m->loop) step = ((step + m->loop + m->loop / 2) % m->loop) - m->loop / 2;
		if(step) ++m->moved;
		if(m->n) {
			int d = abs(step - m->prevStep);
			m->jerkSum += d;
			if(d > m->worst) m->worst = d;
		}
		++m->n;
		m->prevStep = step;
	}
	else if(!steady) {
		m->n = m->n;   // a phase change: restart the step chain
		m->has = 0;
	}
	if(!m->has) { m->has = 1; m->prevStep = 0; }
	m->prev = v;
}

static void report(const tMeter *m) {
	if(!m->n) { printf("  %-22s (no samples)\n", m->name); return; }
	printf("  %-22s moved %3d%%   jerk %5.2f   worst %d   (%d frames)\n", m->name,
		100 * m->moved / m->n, (double)m->jerkSum / m->n, m->worst, m->n);
}

int main(void) {
	static tGame g;
	logicInit(&g);
	logicSetDifficulty(&g, DIFFICULTY_INVINCIBLE);
	logicStartGame(&g);
	tInput in = {0, 0, 0, 0};
	for(int f = 0; f < 400 && g.state != GS_PLAY; ++f) logicUpdate(&g, &in);
	tMeter cam = {"camera"}, scr = {"chopper on screen"}, mtn = {"mountains (cam>>2)", 640},
		hil = {"hills (cam>>1)", 640}, tank = {"a tank"}, man = {"a hostage"}, mine = {"a mine"};
	// The script: take off, fly left at full speed, cruise, ease off, fly back right.
	struct { int frames; int8_t dx, dy; } script[] = {
		{40, 0, -1}, {30, -1, 0}, {500, -1, 0}, {60, 0, 0}, {30, 1, 0}, {300, 1, 0}, {60, 0, 0},
	};
	int total = 0;
	for(unsigned s = 0; s < sizeof(script) / sizeof(script[0]); ++s) {
		// the first 30 frames of each phase are the acceleration: not "steady"
		for(int f = 0; f < script[s].frames; ++f, ++total) {
			in.dx = script[s].dx;
			in.dy = script[s].dy;
			// keep hostages and enemies around to measure
			if(total == 100) {
				g.pTanks[0].active = 1; g.pTanks[0].x = (int16_t)(g.cam + 100); g.pTanks[0].hp = 99; g.pTanks[0].cool = 255;
				g.pMines[0].active = 1; g.pMines[0].x = (int16_t)(g.cam + 40); g.pMines[0].y = 120;
				logicHostageOut(&g, 0, HS_TO_HANGAR, (int16_t)(g.cam + 60));
			}
			logicUpdate(&g, &in);
			int steady = f >= 30;
			meter(&cam, g.cam, steady);
			meter(&scr, g.chop.x - g.cam, steady);
			meter(&mtn, logicBandOffset(g.cam, 2, 640), steady);
			meter(&hil, logicBandOffset(g.cam, 1, 640), steady);
			if(g.pTanks[0].active) meter(&tank, g.pTanks[0].x - g.cam, steady);
			if(g.pMines[0].active) meter(&mine, g.pMines[0].x - g.cam, steady);
			if(g.pHostages[0].state == HS_TO_HANGAR) meter(&man, g.pHostages[0].x - g.cam, steady);
		}
		// a phase boundary breaks the chains
		meter(&cam, g.cam, 0); meter(&scr, g.chop.x - g.cam, 0);
		meter(&mtn, logicBandOffset(g.cam, 2, 640), 0); meter(&hil, logicBandOffset(g.cam, 1, 640), 0);
	}
	printf("judder over %d frames of scripted flight (steady parts only):\n", total);
	report(&cam); report(&scr); report(&mtn); report(&hil); report(&tank); report(&man); report(&mine);
	return 0;
}
