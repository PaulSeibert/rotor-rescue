// ROTOR RESCUE - the Amiga side (by Paul Ross Seibert): display, input, sound.
// (c) 2026 Desert Palms Software.
// The game's rules are in logic.c (host-testable); this shows what they say.
//
// OCS dual playfield, 6 bitplanes, 50 frames a second, one raw copper list per
// buffer:
//   PF1 (planes 1/3/5, colours 1-7): lines 0-23 the HUD bitmap (text and a
//        radar of the whole battlefield); from line 24 a double-buffered
//        frame buffer with every BOB - buildings, hostages, tanks, jets,
//        mines, shots, explosions - drawn by the blitter from a queue while
//        the CPU runs the game (agk/blitq.h).
//   PF2 (planes 2/4/6, colours 9-15): three looping parallax bands, each
//        with its own fine scroll and palette set by the copper: far
//        mountains (camera / 4), near hills (camera / 2), the ground (1:1).
//   COLOR00: a sky gradient per mission, line by line from the copper,
//        line-dithered on OCS (half steps: alternate lines of two colours).
//   Sprites 0-3: the chopper, 32 px wide, 15 colours (attached pairs).
//   Sprites 4-5: the weather (dust, rain, spray), in front of everything.
//   Sprite 6: the far sky's wisps or stars, behind the playfields.
//   At boot: the Desert Palms Software logo (splash.c), its own copper lists.
//   The title: a second copper list per buffer (the game's list jumps to it):
//        the ROTOR RESCUE logo as a 4-plane, 16-colour picture over the sky
//        with 8 of its colours reloaded line by line, the land bands below.
//   Paula channel 2: the rotor, a looped blade thump whose rate follows the
//        speed and climb (logic.c's rules, driven here); music on 0, 1, 3; effects on 1 and 3.
//   On an A1200 (AGA, found at startup): a 24-bit sky and an 8-plane,
//        224-colour title logo (see "AGA" below).
//   The attract loop (logic.c): the title, the hall of fame (saved to the
//        floppy: hiscore.c), a self-playing demo, the title again.

#include <ace/generic/main.h>
#include <ace/managers/blit.h>
#include <ace/managers/copper.h>
#include <ace/managers/joy.h>
#include <ace/managers/key.h>
#include <ace/managers/memory.h>
#include <ace/managers/ptplayer.h>
#include <ace/managers/sprite.h>
#include <ace/managers/system.h>
#include <ace/managers/timer.h>
#include <ace/utils/custom.h>
#include <ace/utils/extview.h>
#include <hardware/dmabits.h>
#include <agk/blitq.h>
#include <agk/copw.h>
#include <agk/debug.h>
#include <agk/pack.h>
#include <agk/perf.h>
#include <agk/text.h>
#include "art.h"        // generated from art/ by agk (agk help-art)
#include "sound.h"      // generated from sound/ by agk (agk help-sound)
#include "logic.h"
#include "splash.h"
#include "hiscore.h"
#include "sky_gfx.h"    // tools/mkclouds.py
#include "title_logo.h" // art/tools/make_title_logo.py
#include "title_logo8.h" // art/tools/make_title_logo8.py (the A1200's)

// ------------------------------------------------------ display constants ---

#define SYNC_FRAME 10         // "AGK t0": scenarios start here on every profile
#define BPLCON0_DPF6 0x6600   // 6 planes | DBLPF | COLOR
// PF1 in front of PF2; priority code 3 for both: channels 0-5 (the chopper,
// the weather) in front of both playfields, channels 6-7 (the far sky layer)
// behind them - they only show through the sky. (Lines 24-119 run as a single
// 3-plane playfield, where PF2's code applies: so both.)
#define BPLCON2_RR 0x001B
#define DDFSTRT_SCROLL 0x30   // one fetch word early for fine scroll (costs sprite 7)
#define DDFSTOP_LORES 0xD0
#define FETCH_BYTES 42        // 21 words a line: 320 px + the scroll word
#define SWITCH_WAIT_X 0xD8    // after the last fetch of a line
#define COP_SPRITES_POS 0
#define COP_TOP_POS 16
#define COP_RAW_COUNT 360
#define COP_JUMP_POS COP_TOP_POS  // MOVE COP2LC (2), MOVE COPJMP2: on, or to the title list

// The PF2 bands, in game lines (the clouds: CLOUDS_Y in logic.h)
#define CLOUDS_END (CLOUDS_Y + ART_CLOUDS_H)  // 112: blank PF2 rows to the mountains
#define MTN_Y 120
#define GAP_Y (MTN_Y + ART_MOUNTAINS_H)      // 196: one blank line, the land palette loads
#define HILLS_Y (GAP_Y + 1)                  // 197
#define GROUND_BAND_Y (HILLS_Y + ART_HILLS_H) // 224 .. 255
#define SKY_END GAP_Y

// The title's own layout (its copper list): the logo, then its lines of text,
// the mountains lower down (the clouds don't show), the hills and ground as in
// play. Lines 24-25 load the logo's first colours, 26 switches to its planes.
#define TITLE_LOGO_Y 27
#define TITLE_LOGO_END (TITLE_LOGO_Y + TITLE_LOGO_H)  // back to the frame buffer
#define TITLE_LOGO_X ((SCREEN_W - TITLE_LOGO_W) / 2)
#define TITLE_LOGO_ROW_BYTES (TITLE_LOGO_W / 8)
#define TITLE_LOGO_DDFSTRT (0x38 + TITLE_LOGO_X / 2)  // fetch just the logo's words
#define TITLE_LOGO_DDFSTOP (TITLE_LOGO_DDFSTRT + (TITLE_LOGO_W / 16 - 1) * 8)
#define TITLE_MTN_Y (TITLE_LOGO_END + 6)              // its first rows are empty sky
#define TITLE_TEXT_Y (TITLE_LOGO_END + 3)             // PF1's colours are back by then
_Static_assert(TITLE_LOGO_W % 16 == 0 && TITLE_LOGO_PLANES == 4, "the logo: 4 planes, whole words");
_Static_assert(TITLE_LOGO_Y - 1 > PLAY_TOP + 1, "lines 24-25 load the logo's colours, the next switches to it");
// (the switches after the logo take its last line and the next 3)
_Static_assert(TITLE_MTN_Y - 1 > TITLE_LOGO_END + 3 && TITLE_MTN_Y < GAP_Y - 24, "the title's mountains");
// HUD, top, every sky line's WAIT + COLOR00, the logo's colour changes, switches, bands
#define TITLE_COP_COUNT (256 + 2 * (SKY_END - PLAY_TOP) + TITLE_LOGO_CHANGES + 64)
// AGA (A1200): every sky line's COLOR00 is 4 MOVEs (24 bits), the AGA
// registers at the top, the 8-plane logo's palette at the top of the title's
// (per bank of 32: BPLCON3, 32 high nibbles, BPLCON3, 32 low nibbles)
#define AGA_SKY_MOVES (4 * (SKY_END - PLAY_TOP))
#define TITLE_LOGO8_PAL_MOVES ((TITLE_LOGO8_COLORS / 32) * 66 + 1)
// (the OCS sky changes on few lines: AGA's has a WAIT and 4 MOVEs on each)
#define COP_RAW_COUNT_AGA (COP_RAW_COUNT + AGA_SKY_MOVES + (SKY_END - PLAY_TOP) + 6)
#define TITLE_COP_COUNT_AGA (TITLE_COP_COUNT + AGA_SKY_MOVES + 6 + TITLE_LOGO8_PAL_MOVES)
_Static_assert(TITLE_LOGO8_W == TITLE_LOGO_W && TITLE_LOGO8_H == TITLE_LOGO_H, "the A1200's logo: the same size");
_Static_assert(TITLE_LOGO8_FIRST == 32 && TITLE_LOGO8_COLORS == 224, "the A1200's logo: pens 32-255");
_Static_assert(TITLE_LOGO8_BYTES >= TITLE_LOGO_BYTES, "its memory holds the OCS logo too (the fallback)");
#define TITLE_LOGO8_ROW_BYTES (TITLE_LOGO_W / 8)

// PF1 frame buffers: 48 px of margin left, 64 right, so objects never need
// clipping. Screen (x, y) is buffer (x + FB_X0, y).
#define FB_X0 48
#define FB_W (FB_X0 + SCREEN_W + 64)
#define FB_H (SCREEN_H + 8)
#define FB_BPP 3
#define FB_PLANE_BYTES (FB_W / 8)
#define FB_ROW_BYTES (FB_PLANE_BYTES * FB_BPP)   // interleaved
#define FB_FETCH_OFFS ((FB_X0 - 16) / 8)        // the early fetch word
// The HUD: 336 x PLAY_TOP (the first 16 px are the early fetch word)
#define HUD_W (SCREEN_W + 16)
#define HUD_PLANE_BYTES (HUD_W / 8)
#define HUD_ROW_BYTES (HUD_PLANE_BYTES * FB_BPP)
#define RADAR_Y 14
#define RADAR_H 8

static tView *s_pView;
static tVPort *s_pVPort;
static tBitMap *s_pFb[2], *s_pHud[2];
static tBitMap *s_pMountains, *s_pTitleMtn, *s_pHills, *s_pGround;   // (the title's mountains: a strip that fades out at its foot)
static UBYTE *s_pBlankRow;          // FETCH_BYTES of zeros: PF2 where there's no band
static tCopBfr *s_pCopBfrA;         // copper buffer 0
static UWORD s_uwCopUsed, s_uwTitleCopUsed;
static UBYTE s_pListMission[2];     // which mission's colours each copper list has
static UWORD s_pSky[MISSION_COUNT][SKY_END - PLAY_TOP];   // COLOR00 per line (from logic.c, at startup: OCS line-dithered)
static UBYTE s_ubSkyHiddenFrom = SKY_END;   // from this line down the mountains cover the sky everywhere

typedef struct {
	UWORD uwCon1;          // index of the MOVE BPLCON1
	UWORD pPtr[3];         // per plane: index of its pointer's high-word MOVE
} tCopSlots;
// per list ([title][buffer]): the sky's length varies
static tCopSlots s_sCloudSlots[2], s_sMtnSlots[2][2], s_sHillSlots[2][2], s_sGndSlots[2][2];
static tBitMap *s_pCloudsBm;        // the far clouds band (PF2, camera / 8)
static UBYTE *s_pTitleLogo;         // chip RAM: the logo (from the floppy)
static UBYTE s_isTitlePending, s_isTitleLogo;   // to load; loaded
static UBYTE s_isTitleLogo8;        // A1200: the logo loaded is the 8-plane one
static UWORD s_uwCopCount = COP_RAW_COUNT, s_uwTitleCopCount = TITLE_COP_COUNT;   // (more on AGA)
static ULONG s_ulTitleLogoBytes = TITLE_LOGO_BYTES;
static UWORD (*s_pSkyLo)[SKY_END - PLAY_TOP];   // AGA: [mission][line] the 24-bit sky's low nibbles (s_pSky: the high)
static const UWORD *s_pTitleFixed;  // in it: colours 1..TITLE_LOGO_FIXED
static const UWORD (*s_pTitleRows)[TITLE_LOGO_ROW_REGS];   // colours per line
static tCopCmd *s_pTitleList[2];    // chip RAM: the title's copper list per buffer
static UBYTE s_pListTitle[2];       // whether game list b jumps to title list b
static UBYTE s_pTitleListMission[2];
// The title lists' mission colours (the same places in both): COLOR00 on every
// sky line, the mountains' and the land's PF2 palettes
static UWORD s_pTitleSkySlot[SKY_END - PLAY_TOP];
static UWORD s_uwTitleMtnPal, s_uwTitleLandPal;

static tGame s_sGame;
static ULONG s_ulFrameVbl;
static UBYTE s_isMusicOn = 1, s_ubSong = 0xFF;
static UBYTE s_isPaused;
static UBYTE s_isSplash;         // the logo's showing
static UBYTE s_ubAfterSplash;    // frames since it ended (its memory goes after 2)
static UBYTE s_isReady;
static UBYTE s_ubSkillKeyWait;  // frames left to press 0-5 after Ctrl+L
static UBYTE s_ubScrollTrace;   // Ctrl+L, T: frames left to print the far bands' BPLCON1 (tests)
static UWORD s_uwMtnCon1, s_uwHillCon1, s_uwMtnX4;   // what copperUpdate wrote last
static UBYTE s_ubDiskStatus;    // HS_*: the hall of fame on the floppy (shown under it)

static void scoresReport(void) {
	// "AGK scores names=DPS/PRS/..." best first, then the top and last scores
	char szLine[] = "AGK scores names=AAA/AAA/AAA/AAA/AAA\n";
	for(UBYTE i = 0; i < SCORE_COUNT; ++i)
		for(UBYTE c = 0; c < SCORE_NAME; ++c) szLine[17 + 4 * i + c] = s_sGame.pScores[i].name[c];
	agkPrint(szLine);
	agkState("top", (LONG)s_sGame.pScores[0].score);
	agkState("last", (LONG)s_sGame.pScores[SCORE_COUNT - 1].score);
	agkEnd();
}
static const UBYTE s_pDigitKeys[DIFFICULTY_MAX + 1] = {KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5};

// Ctrl+L, then 0-5 within 2 s: the difficulty (0 = invincible, for testing)
static void skillKeys(void) {
	if(keyCheck(KEY_CONTROL) && keyUse(KEY_L)) s_ubSkillKeyWait = 100;
	if(!s_ubSkillKeyWait) return;
	--s_ubSkillKeyWait;
	if(keyUse(KEY_T)) {
		s_ubScrollTrace = 12;
		s_ubSkillKeyWait = 0;
		return;
	}
	if(keyUse(KEY_G)) {
		// ghost: nothing destroys the chopper, at any difficulty (the pacing run)
		s_sGame.ghost = !s_sGame.ghost;
		s_ubSkillKeyWait = 0;
		agkState("ghost", s_sGame.ghost);
		agkEnd();
		return;
	}
	if(keyUse(KEY_N)) {
		// the next trip's schedule at once (the pacing run: jets and mines come with the trips)
		if(s_sGame.trip < 9) ++s_sGame.trip;
		s_ubSkillKeyWait = 0;
		agkState("trip", s_sGame.trip);
		agkEnd();
		return;
	}
	if(keyUse(KEY_H)) {
		// the hall of fame read from the disk again, as at boot (tests)
		s_ubSkillKeyWait = 0;
		agkPrint(hiscoreLoad(&s_sGame) == DISK_LOADED ? "AGK scores loaded\n" : "AGK scores default\n");
		scoresReport();
		agkPerfSkip();
		return;
	}
	for(UBYTE d = 0; d <= DIFFICULTY_MAX; ++d) {
		if(keyUse(s_pDigitKeys[d])) {
			logicSetDifficulty(&s_sGame, d);
			s_ubSkillKeyWait = 0;
			agkState("difficulty", d);
			agkEnd();
			break;
		}
	}
}

// ------------------------------------------------------------------ AGA ---
// On an A1200 (AGA, Lisa) the game is the same OCS game, plus (aga*, s_isAga):
//  - the sky in 24-bit colour: COLOR00's high nibbles, then BPLCON3's LOCT
//    and its low nibbles, on every sky line (smooth, not 12-bit bands);
//  - the title logo in 8 bitplanes, 224 colours of 24 bits (pens 32-255: the
//    game's colours 0-31 stay as they are), loaded by the title's copper list.
// The AGA registers the OCS code never writes are put to the values it
// assumes, at the top of every copper list: Kickstart 3.1 leaves BPLCON3 at 0,
// and PF2OF 0 there gives playfield 2 colours 0-7 rather than 8-15.
// Registers by offset (ACE's OCS build has no names for them).
#define REG_DENISEID 0x07C
#define REG_BPLCON3 0x106
#define REG_BPLCON4 0x10C
#define REG_FMODE 0x1FC
#define REG_COLOR00 0x180
#define BPLCON3_OCS 0x0C00   // bank 0, PF2's colours from 8 (PF2OF 3), high nibbles
#define BPLCON3_LOCT 0x0E00  // the same, writing low nibbles
#define BPLCON4_OCS 0x0011   // sprites in colours 16-31
static UBYTE s_isAga;
static UWORD s_pSkyDotColorSlot[2];   // AGA: [title] where a list's colour 29-31 MOVEs are

// Lisa answers $F8 in DENISEID's low byte every time; OCS Denise has no such
// register (whatever the bus holds), ECS Denise answers $FC
__attribute__((noinline, optimize("Os"))) static UBYTE agaDetect(void) {
	volatile UWORD *pId = (volatile UWORD *)((volatile UBYTE *)g_pCustom + REG_DENISEID);
	for(UBYTE i = 0; i < 8; ++i) if((*pId & 0xFF) != 0xF8) return 0;
	return 1;
}

static inline void cwReg(tAgkCopWriter *pW, UWORD uwReg, UWORD uwValue) {
	pW->pList[pW->uwPos++].ulCode = ((ULONG)uwReg << 16) | uwValue;
}

// AGA: the registers the OCS lists don't write, as the OCS code assumes them
__attribute__((noinline, optimize("Os"))) static void cwAgaOcsRegs(tAgkCopWriter *pW) {
	cwReg(pW, REG_FMODE, 0);
	cwReg(pW, REG_BPLCON3, BPLCON3_OCS);
	cwReg(pW, REG_BPLCON4, BPLCON4_OCS);
}

// AGA: COLOR00 in 24 bits (4 MOVEs; the low nibbles' MOVE is 2 after the
// high's): a write with LOCT clear sets both nibbles to the high ones, then
// LOCT and the low nibbles, then BPLCON3 back
__attribute__((noinline, optimize("Os"))) static void cwSky24(tAgkCopWriter *pW, UWORD uwHi, UWORD uwLo) {
	cwReg(pW, REG_COLOR00, uwHi);
	cwReg(pW, REG_BPLCON3, BPLCON3_LOCT);
	cwReg(pW, REG_COLOR00, uwLo);
	cwReg(pW, REG_BPLCON3, BPLCON3_OCS);
}

// AGA: bitplane p's pointer, for any of the 8 (BPL1PT at $E0)
__attribute__((noinline, optimize("Os"))) static void cwPtr8(tAgkCopWriter *pW, UBYTE p, ULONG ulAddr) {
	cwReg(pW, (UWORD)(0x0E0 + 4 * p), (UWORD)(ulAddr >> 16));
	cwReg(pW, (UWORD)(0x0E2 + 4 * p), (UWORD)(ulAddr & 0xFFFF));
}

// AGA: the 8-plane logo's 224 colours, pens 32-255 (banks 1-7; nothing else
// uses them, so they need no putting back): at the top of the title's list
__attribute__((noinline, optimize("Os"))) static void cwLogo8Palette(tAgkCopWriter *pW) {
	const UWORD *pHi = (const UWORD *)s_pTitleLogo;
	const UWORD *pLo = (const UWORD *)(s_pTitleLogo + TITLE_LOGO8_LO_OFFS);
	for(UBYTE b = TITLE_LOGO8_FIRST / 32; b < 8; ++b) {
		cwReg(pW, REG_BPLCON3, (UWORD)((b << 13) | BPLCON3_OCS));
		for(UBYTE i = 0; i < 32; ++i) cwReg(pW, (UWORD)(REG_COLOR00 + 2 * i), *pHi++);
		cwReg(pW, REG_BPLCON3, (UWORD)((b << 13) | BPLCON3_LOCT));
		for(UBYTE i = 0; i < 32; ++i) cwReg(pW, (UWORD)(REG_COLOR00 + 2 * i), *pLo++);
	}
	cwReg(pW, REG_BPLCON3, BPLCON3_OCS);
}

// AGA: wait for the HUD's lines (game lines 2-21), where the copper leaves
// BPLCON3 alone: CPU colour writes there go to bank 0's high nibbles (and
// set the low ones too), as on OCS. (The title's palette load is over by
// then; the sky's LOCT MOVEs haven't started.)
__attribute__((noinline, optimize("Os"))) static void agaWaitPaletteWindow(void) {
	UWORD uwFrom = (UWORD)(s_pView->ubPosY + 2), uwTo = (UWORD)(s_pView->ubPosY + PLAY_TOP - 3);
	for(;;) {
		UWORD uwLine = (UWORD)(((g_pCustom->vposr & 1) << 8) | (g_pCustom->vhposr >> 8));
		if(uwLine >= uwFrom && uwLine <= uwTo) return;
	}
}

static inline UBYTE backBuffer(void) {
	return agkCopBfr(s_pView->pCopList, s_pCopBfrA);
}

// --------------------------------------------------------- copper list ---
// Lines 0-39 show nothing in PF2 (HUD, open sky), so they run as a plain
// 3-plane screen (BPL1-3 = PF1's bitmap, same colours 0-7): half the
// bitplane DMA, more bus for the CPU. At the cloud band the copper switches to
// the 6-plane dual playfield (BPL1/3/5 = PF1, BPL2/4/6 = PF2): clouds (camera
// / 8, their own palette), blank rows while the mountain palette loads, then
// the mountains, hills and ground bands.

#define BPLCON0_3 0x3200      // 3 planes | COLOR

static void cwPlanePtrs(tAgkCopWriter *pW, UBYTE ubFirst, const ULONG *pAddr) {
	for(UBYTE p = 0; p < 3; ++p) agkCopPtrs(pW, (UBYTE)(ubFirst + 2 * p), pAddr[p]);
}

static void cwPf2Palette(tAgkCopWriter *pW, const UWORD *pPal) {
	for(UBYTE i = 1; i < 8; ++i) agkCopMove(pW, &g_pCustom->color[8 + i], pPal[i]);
}

// A band switch at the end of line uwGameY - 1 (8 MOVEs: fits the blank)
static tCopSlots cwBand(tAgkCopWriter *pW, UWORD uwGameY, const tBitMap *pBm) {
	tCopSlots sSlots;
	ULONG pAddr[3];
	for(UBYTE p = 0; p < 3; ++p) pAddr[p] = (ULONG)pBm->Planes[p];
	agkCopWait(pW, uwGameY - 1, SWITCH_WAIT_X);
	for(UBYTE p = 0; p < 3; ++p) sSlots.pPtr[p] = (UWORD)(pW->uwPos + 2 * p);
	cwPlanePtrs(pW, 1, pAddr);
	sSlots.uwCon1 = pW->uwPos;
	agkCopMove(pW, &g_pCustom->bplcon1, 0);
	agkCopMove(pW, &g_pCustom->bpl2mod, pBm->BytesPerRow - FETCH_BYTES);
	return sSlots;
}

// The HUD panel (lines 0..PLAY_TOP-1), all from the copper: a brushed-steel
// gradient behind it with a lit top edge and a chrome rim along the bottom,
// letters (colour 4) shading from white to gold down each text row, and the
// radar sunk into a dark inset (COLOR00 switched mid-line either side of it).
#define HUD_INK 4
#define HUD_TEXT1_Y 3                // the text rows (hudInit)
#define HUD_TEXT2_Y RADAR_Y
#define HUD_INSET_X0 0x5C            // copper positions (colour clocks) either side of the radar
#define HUD_INSET_X1 0xC2
#define HUD_INSET_COLOR 0x011
static const UWORD s_pHudBg[PLAY_TOP] = {
	0x88A, 0x558, 0x346, 0x235, 0x235, 0x234, 0x224, 0x224, 0x224, 0x124, 0x123, 0x123,
	0x123, 0x113, 0x113, 0x113, 0x113, 0x112, 0x112, 0x112, 0x112, 0x112, 0x99B, 0x001,
};
static const UWORD s_pHudInkRow[8] = {0xFFF, 0xFFE, 0xFFD, 0xFEB, 0xFD9, 0xFC7, 0xEB6, 0xDA5};
static UWORD s_pHudInk[PLAY_TOP];    // colour 4 per line (s_pHudInkRow on the text rows)

static void hudPanelInit(void) {
	UWORD uwInk = s_pHudInkRow[0];
	for(UBYTE y = 0; y < PLAY_TOP; ++y) {
		if(y >= HUD_TEXT1_Y && y < HUD_TEXT1_Y + 8) uwInk = s_pHudInkRow[y - HUD_TEXT1_Y];
		else if(y >= HUD_TEXT2_Y && y < HUD_TEXT2_Y + 8) uwInk = s_pHudInkRow[y - HUD_TEXT2_Y];
		s_pHudInk[y] = uwInk;
	}
}

static void cwHudPanel(tAgkCopWriter *pW) {
	for(UBYTE y = 1; y < PLAY_TOP; ++y) {
		UBYTE isInset = y >= RADAR_Y - 1 && y < RADAR_Y + RADAR_H;
		if(s_pHudBg[y] != s_pHudBg[y - 1] || s_pHudInk[y] != s_pHudInk[y - 1] || isInset) {
			agkCopWait(pW, y, 0);
			if(s_pHudBg[y] != s_pHudBg[y - 1]) agkCopMove(pW, &g_pCustom->color[0], s_pHudBg[y]);
			if(s_pHudInk[y] != s_pHudInk[y - 1]) agkCopMove(pW, &g_pCustom->color[HUD_INK], s_pHudInk[y]);
		}
		if(isInset) {
			agkCopWait(pW, y, HUD_INSET_X0);
			agkCopMove(pW, &g_pCustom->color[0], HUD_INSET_COLOR);
			agkCopWait(pW, y, HUD_INSET_X1);
			agkCopMove(pW, &g_pCustom->color[0], s_pHudBg[y]);
		}
	}
}

// The jump at the top of game list pList: to title list ubBfr, or on to the
// MOVE after it
static void copperJump(tCopCmd *pList, UBYTE ubBfr, UBYTE isTitle) {
	ULONG ulTo = isTitle ? (ULONG)s_pTitleList[ubBfr] : (ULONG)&pList[COP_JUMP_POS + 3];
	copSetMoveVal(&pList[COP_JUMP_POS].sMove, ulTo >> 16);
	copSetMoveVal(&pList[COP_JUMP_POS + 1].sMove, ulTo & 0xFFFF);
}

// After the logo's last line (y): back to the frame buffer, 3 planes
static void cwTitleLogoOff(tAgkCopWriter *pW, UWORD y, const tBitMap *pFb) {
	ULONG ulRow = (ULONG)(TITLE_LOGO_END * FB_ROW_BYTES + FB_FETCH_OFFS);
	agkCopWait(pW, y, SWITCH_WAIT_X);
	agkCopMove(pW, &g_pCustom->ddfstrt, DDFSTRT_SCROLL);
	agkCopMove(pW, &g_pCustom->ddfstop, DDFSTOP_LORES);
	agkCopMove(pW, &g_pCustom->bplcon0, BPLCON0_3);
	agkCopMove(pW, &g_pCustom->bpl1mod, FB_ROW_BYTES - FETCH_BYTES);
	agkCopMove(pW, &g_pCustom->bpl2mod, FB_ROW_BYTES - FETCH_BYTES);
	for(UBYTE p = 0; p < 3; ++p) agkCopPtrs(pW, p, (ULONG)pFb->Planes[p] + ulRow);
}

// The A1200's logo lines: 8 planes, 1x fetch (lores allows it: the planes
// take every slot of the logo's 12 words, the copper's sky MOVEs come before
// them), pens 32-255 from the palette at the list's top, no colour changes
// down the picture. Planes 4-8 aren't fetched by the 3-plane lines above:
// their pointers go in a line early, so the switch's MOVEs that run into the
// logo's first line (and delay its sky MOVEs) are few.
__attribute__((noinline, optimize("Os"))) static void cwTitleLogo8(tAgkCopWriter *pW, UWORD y, const tBitMap *pFb) {
	const UBYTE *pPlanes = s_pTitleLogo + TITLE_LOGO8_PLANES_OFFS;
	if(y == TITLE_LOGO_Y - 2) {
		agkCopWait(pW, y, SWITCH_WAIT_X);
		for(UBYTE p = 3; p < 8; ++p) cwPtr8(pW, p, (ULONG)(pPlanes + p * TITLE_LOGO8_ROW_BYTES));
	}
	else if(y == TITLE_LOGO_Y - 1) {
		agkCopWait(pW, y, SWITCH_WAIT_X);
		agkCopMove(pW, &g_pCustom->ddfstrt, TITLE_LOGO_DDFSTRT);
		agkCopMove(pW, &g_pCustom->ddfstop, TITLE_LOGO_DDFSTOP);
		agkCopMove(pW, &g_pCustom->bplcon0, 0x0211);   // 8 planes (BPU3), ECSENA, COLOR
		agkCopMove(pW, &g_pCustom->bpl1mod, TITLE_LOGO8_ROW_BYTES * 7);
		agkCopMove(pW, &g_pCustom->bpl2mod, TITLE_LOGO8_ROW_BYTES * 7);
		for(UBYTE p = 0; p < 3; ++p) cwPtr8(pW, p, (ULONG)(pPlanes + p * TITLE_LOGO8_ROW_BYTES));
	}
	else if(y == TITLE_LOGO_END - 1) cwTitleLogoOff(pW, y, pFb);
}

// The title's logo lines: the switch to its planes at the end of line y (and
// its first colours before that), the colours that change on line y, the way
// back to the frame buffer after it. The logo has its own fetch (just its 12
// words, no early word: PF1's delay is 0 here), 4 planes, single playfield.
static void cwTitleLogo(tAgkCopWriter *pW, UWORD y, const tBitMap *pFb) {
	if(!s_isTitleLogo) return;
	if(s_isTitleLogo8) {
		cwTitleLogo8(pW, y, pFb);
		return;
	}
	if(y == PLAY_TOP) {
		// (nothing in PF1 here on the title: its colours can change)
		agkCopWait(pW, y, SWITCH_WAIT_X);
		for(UBYTE i = 0; i < TITLE_LOGO_FIXED; ++i) agkCopMove(pW, &g_pCustom->color[1 + i], s_pTitleFixed[i]);
		agkCopMove(pW, &g_pCustom->color[TITLE_LOGO_FIRST_ROW_REG], s_pTitleRows[0][0]);
	}
	else if(y == PLAY_TOP + 1) {
		agkCopWait(pW, y, SWITCH_WAIT_X);
		for(UBYTE r = 1; r < TITLE_LOGO_ROW_REGS; ++r)
			agkCopMove(pW, &g_pCustom->color[TITLE_LOGO_FIRST_ROW_REG + r], s_pTitleRows[0][r]);
	}
	else if(y == TITLE_LOGO_Y - 1) {
		agkCopWait(pW, y, SWITCH_WAIT_X);
		agkCopMove(pW, &g_pCustom->ddfstrt, TITLE_LOGO_DDFSTRT);
		agkCopMove(pW, &g_pCustom->ddfstop, TITLE_LOGO_DDFSTOP);
		agkCopMove(pW, &g_pCustom->bplcon0, (TITLE_LOGO_PLANES << 12) | 0x0200);
		agkCopMove(pW, &g_pCustom->bpl1mod, TITLE_LOGO_ROW_BYTES * (TITLE_LOGO_PLANES - 1));
		agkCopMove(pW, &g_pCustom->bpl2mod, TITLE_LOGO_ROW_BYTES * (TITLE_LOGO_PLANES - 1));
		for(UBYTE p = 0; p < TITLE_LOGO_PLANES; ++p) agkCopPtrs(pW, p, (ULONG)(s_pTitleLogo + TITLE_LOGO_PLANES_OFFS + p * TITLE_LOGO_ROW_BYTES));
	}
	else if(y > TITLE_LOGO_Y && y < TITLE_LOGO_END) {
		// at the line's start (the logo starts 64 px in): the colours that change
		const UWORD *pRow = s_pTitleRows[y - TITLE_LOGO_Y], *pPrev = pRow - TITLE_LOGO_ROW_REGS;
		for(UBYTE r = 0; r < TITLE_LOGO_ROW_REGS; ++r) {
			if(pRow[r] != pPrev[r]) agkCopMove(pW, &g_pCustom->color[TITLE_LOGO_FIRST_ROW_REG + r], pRow[r]);
		}
	}
	if(y == TITLE_LOGO_END - 1) cwTitleLogoOff(pW, y, pFb);
	else if(y == TITLE_LOGO_END) {
		// PF1's colours back (the text starts below)
		agkCopWait(pW, y, SWITCH_WAIT_X);
		for(UBYTE i = 1; i < 8; ++i) agkCopMove(pW, &g_pCustom->color[i], s_pVPort->pPalette[i]);
	}
}

static void copperWriteList(tCopCmd *pList, UBYTE ubBfr, UBYTE ubMission, UBYTE isTitle);
static void musicFor(UBYTE ubState);
static void routeCreate(void);

// The title logo from the floppy (10 KB packed; ACE switches the OS on for the
// file), the title's copper lists, then the music (the OS stops it while it
// loads). At boot the game's first frames are the splash, black at first: the
// load waits for them, to keep the boot (the time to the first picture) short.
static void titleLoad(void) {
	s_isTitlePending = 0;
	routeCreate();   // (the mission card's map: not at startup either)
	// the hall of fame from the floppy (none yet: the defaults). Here, not at
	// startup: AROS takes ~2800 frames to load the program and the harness
	// waits 3000 for the first picture
	agkPrint(hiscoreLoad(&s_sGame) == DISK_LOADED ? "AGK scores loaded\n" : "AGK scores default\n");
	// (the A1200: its 8-plane logo, or without the file the OCS one)
	s_isTitleLogo8 = s_isAga && s_pTitleLogo &&
		agkLoadFile("titlelogo8.bin.pk", s_pTitleLogo, TITLE_LOGO8_BYTES) == TITLE_LOGO8_BYTES;
	s_isTitleLogo = s_isTitleLogo8 || (s_pTitleLogo &&
		agkLoadFile("titlelogo.bin.pk", s_pTitleLogo, TITLE_LOGO_BYTES) == TITLE_LOGO_BYTES);
	// (a hard-disk copy without the file: a title without the logo)
	if(!s_isTitleLogo) agkPrint("AGK title logo missing\n");
	for(UBYTE b = 0; b < 2; ++b) {
		copperWriteList(s_pTitleList[b], b, s_sGame.mission, 1);
		s_pTitleListMission[b] = s_sGame.mission;
	}
	musicFor(s_sGame.state);
	agkPerfSkip();
}

// (no loop unswitching: the AGA branches would copy whole loops - code size
// counts on the A500, whose slow RAM is full)
__attribute__((optimize("no-unswitch-loops")))
static void copperWriteList(tCopCmd *pList, UBYTE ubBfr, UBYTE ubMission, UBYTE isTitle) {
	tAgkCopWriter sW;
	// the game list starts after the sprite pointers, the title list jumped to
	// from there
	agkCopInit(&sW, pList, isTitle ? 0 : COP_TOP_POS, s_pView->ubPosY);
	ULONG pBlank[3] = {(ULONG)s_pBlankRow, (ULONG)s_pBlankRow, (ULONG)s_pBlankRow};
	const tBitMap *pFb = s_pFb[ubBfr];
	ULONG ulFbRow = (ULONG)(PLAY_TOP * FB_ROW_BYTES + FB_FETCH_OFFS);
	ULONG ulFbCloudRow = (ULONG)(CLOUDS_Y * FB_ROW_BYTES + FB_FETCH_OFFS);
	ULONG ulFbTitleRow = (ULONG)((TITLE_LOGO_END + 3) * FB_ROW_BYTES + FB_FETCH_OFFS);
	tCopSlots *pCld = &s_sCloudSlots[ubBfr];

	if(!isTitle) {
		// (copperJump() sets where to)
		agkCopMove(&sW, (volatile UWORD *)&g_pCustom->cop2lc, 0);
		agkCopMove(&sW, (volatile UWORD *)&g_pCustom->cop2lc + 1, 0);
		agkCopMove(&sW, &g_pCustom->copjmp2, 0);
	}
	if(isTitle && s_isTitleLogo8) cwLogo8Palette(&sW);   // (~8 lines of the vertical blank)
	if(s_isAga) {
		cwAgaOcsRegs(&sW);
		// the sky dots' colours 29-31 (skyUpdate() patches them)
		s_pSkyDotColorSlot[isTitle] = sW.uwPos;
		for(UBYTE i = 0; i < 3; ++i) cwReg(&sW, (UWORD)(REG_COLOR00 + 2 * (29 + i)), g_pMissionSkyDotPal[ubMission][i]);
	}
	// Top of the frame (vertical blank): the HUD, 3 planes
	agkCopMove(&sW, &g_pCustom->bplcon0, BPLCON0_3);
	agkCopMove(&sW, &g_pCustom->bplcon2, BPLCON2_RR);
	agkCopMove(&sW, &g_pCustom->ddfstrt, DDFSTRT_SCROLL);
	agkCopMove(&sW, &g_pCustom->ddfstop, DDFSTOP_LORES);
	agkCopMove(&sW, &g_pCustom->bplcon1, 0);
	agkCopMove(&sW, &g_pCustom->bpl1mod, HUD_ROW_BYTES - FETCH_BYTES);
	agkCopMove(&sW, &g_pCustom->bpl2mod, HUD_ROW_BYTES - FETCH_BYTES);
	for(UBYTE p = 0; p < 3; ++p) agkCopPtrs(&sW, p, (ULONG)s_pHud[ubBfr]->Planes[p]);
	agkCopMove(&sW, &g_pCustom->color[0], s_pHudBg[0]);
	agkCopMove(&sW, &g_pCustom->color[HUD_INK], s_pHudInk[0]);
	if(!isTitle) cwPf2Palette(&sW, g_pMissionCloudPal[ubMission]);   // PF2 shows nothing until the clouds
	cwHudPanel(&sW);

	// After the HUD: the frame buffer, still 3 planes; the ink back to PF1's colour 4
	agkCopWait(&sW, PLAY_TOP - 1, SWITCH_WAIT_X);
	agkCopMove(&sW, &g_pCustom->color[HUD_INK], s_pVPort->pPalette[HUD_INK]);
	agkCopMove(&sW, &g_pCustom->bpl1mod, FB_ROW_BYTES - FETCH_BYTES);
	agkCopMove(&sW, &g_pCustom->bpl2mod, FB_ROW_BYTES - FETCH_BYTES);
	for(UBYTE p = 0; p < 3; ++p) agkCopPtrs(&sW, p, (ULONG)pFb->Planes[p] + ulFbRow);

	if(isTitle) {
		// The title: COLOR00 on every sky line (a new mission only rewrites
		// those values), the logo, the switch to dual playfield with PF2 blank,
		// the mountains lower down
		for(UWORD y = PLAY_TOP; y < SKY_END; ++y) {
			agkCopWait(&sW, y, 0);
			s_pTitleSkySlot[y - PLAY_TOP] = sW.uwPos;
			if(s_isAga) cwSky24(&sW, s_pSky[ubMission][y - PLAY_TOP], s_pSkyLo[ubMission][y - PLAY_TOP]);
			else agkCopMove(&sW, &g_pCustom->color[0], s_pSky[ubMission][y - PLAY_TOP]);
			cwTitleLogo(&sW, y, pFb);
			if(y == TITLE_LOGO_END + 1) {
				// BPL4-6 aren't fetched yet: load them early (PF2 blank, PF1's third plane)
				agkCopWait(&sW, y, SWITCH_WAIT_X);
				agkCopPtrs(&sW, 3, (ULONG)s_pBlankRow);
				agkCopPtrs(&sW, 5, (ULONG)s_pBlankRow);
				agkCopPtrs(&sW, 4, (ULONG)pFb->Planes[2] + ulFbTitleRow);
			}
			if(y == TITLE_LOGO_END + 2) {
				agkCopWait(&sW, y, SWITCH_WAIT_X);
				agkCopMove(&sW, &g_pCustom->bplcon0, BPLCON0_DPF6);
				agkCopPtrs(&sW, 1, (ULONG)s_pBlankRow);
				agkCopPtrs(&sW, 2, (ULONG)pFb->Planes[1] + ulFbTitleRow);
				agkCopMove(&sW, &g_pCustom->bpl2mod, (UWORD)-FETCH_BYTES);
			}
			if(y == TITLE_LOGO_END + 3) {
				agkCopWait(&sW, y, SWITCH_WAIT_X);
				s_uwTitleMtnPal = sW.uwPos;
				cwPf2Palette(&sW, g_pMissionMtnPal[ubMission]);
			}
			if(y == TITLE_MTN_Y - 1) {
				s_sMtnSlots[1][ubBfr] = cwBand(&sW, TITLE_MTN_Y, s_pTitleMtn);
			}
		}
	}
	else {
		// The sky, line by line where the colour changes; the switch to dual playfield
		UWORD uwPrev = 0xFFFF;
		ULONG ulPrev24 = ~0UL;
		for(UWORD y = PLAY_TOP; y < SKY_END; ++y) {
			UWORD uwColor = s_pSky[ubMission][y - PLAY_TOP];
			if(s_isAga) {
				// 24 bits: nearly every line differs
				UWORD uwLo = s_pSkyLo[ubMission][y - PLAY_TOP];
				ULONG ul24 = ((ULONG)uwColor << 16) | uwLo;
				if(ul24 != ulPrev24) {
					agkCopWait(&sW, y, 0);
					cwSky24(&sW, uwColor, uwLo);
					ulPrev24 = ul24;
				}
			}
			else if(uwColor != uwPrev && (y < s_ubSkyHiddenFrom || y == SKY_END - 1)) {
				// (no change where nothing shows the sky - copper DMA is bus time -
				// but the last line's colour shows through the hills below)
				agkCopWait(&sW, y, 0);
				agkCopMove(&sW, &g_pCustom->color[0], uwColor);
				uwPrev = uwColor;
			}
			if(y == CLOUDS_Y - 3) {
				// BPL4-6 aren't fetched yet: load them early (cloud planes 2-3,
				// PF1's third plane at the clouds' first line)
				agkCopWait(&sW, y, SWITCH_WAIT_X);
				pCld->pPtr[1] = sW.uwPos;
				agkCopPtrs(&sW, 3, (ULONG)s_pCloudsBm->Planes[1]);
				pCld->pPtr[2] = sW.uwPos;
				agkCopPtrs(&sW, 5, (ULONG)s_pCloudsBm->Planes[2]);
				agkCopPtrs(&sW, 4, (ULONG)pFb->Planes[2] + ulFbCloudRow);
			}
			if(y == CLOUDS_Y - 1) {
				// 6 planes, dual playfield: BPL2 -> the clouds, BPL3 -> PF1's
				// second plane (it was its third)
				agkCopWait(&sW, y, SWITCH_WAIT_X);
				agkCopMove(&sW, &g_pCustom->bplcon0, BPLCON0_DPF6);
				pCld->pPtr[0] = sW.uwPos;
				agkCopPtrs(&sW, 1, (ULONG)s_pCloudsBm->Planes[0]);
				agkCopPtrs(&sW, 2, (ULONG)pFb->Planes[1] + ulFbCloudRow);
				pCld->uwCon1 = sW.uwPos;
				agkCopMove(&sW, &g_pCustom->bplcon1, 0);
				agkCopMove(&sW, &g_pCustom->bpl2mod, s_pCloudsBm->BytesPerRow - FETCH_BYTES);
			}
			if(y == CLOUDS_END - 1) {
				// clouds -> blank PF2 rows: the mountain palette loads unseen
				agkCopWait(&sW, y, SWITCH_WAIT_X);
				cwPlanePtrs(&sW, 1, pBlank);
				agkCopMove(&sW, &g_pCustom->bpl2mod, (UWORD)-FETCH_BYTES);
			}
			if(y == CLOUDS_END + 1) {
				agkCopWait(&sW, y, SWITCH_WAIT_X);
				cwPf2Palette(&sW, g_pMissionMtnPal[ubMission]);
			}
			if(y == MTN_Y - 1) {
				s_sMtnSlots[0][ubBfr] = cwBand(&sW, MTN_Y, s_pMountains);
			}
		}
	}
	// Mountains -> hills: a blank line in between, for the land palette
	agkCopWait(&sW, GAP_Y - 1, SWITCH_WAIT_X);
	cwPlanePtrs(&sW, 1, pBlank);
	agkCopMove(&sW, &g_pCustom->bpl2mod, (UWORD)-FETCH_BYTES);
	agkCopWait(&sW, GAP_Y, 0);
	if(isTitle) s_uwTitleLandPal = sW.uwPos;
	cwPf2Palette(&sW, g_pMissionLandPal[ubMission]);
	s_sHillSlots[isTitle][ubBfr] = cwBand(&sW, HILLS_Y, s_pHills);
	s_sGndSlots[isTitle][ubBfr] = cwBand(&sW, GROUND_BAND_Y, s_pGround);

	if(isTitle) {
		s_uwTitleCopUsed = sW.uwPos;
		agkCopFill(&sW, s_uwTitleCopCount);
	}
	else {
		s_uwCopUsed = sW.uwPos;
		agkCopFill(&sW, s_uwCopCount);
		copperJump(pList, ubBfr, s_pListTitle[ubBfr]);
	}
}

// A title list's mission colours: the sky and the PF2 palettes (no rewrite)
static void titleListMission(UBYTE ubBfr, UBYTE ubMission) {
	tCopCmd *pList = s_pTitleList[ubBfr];
	const UWORD *pSky = s_pSky[ubMission];
	for(UWORD i = 0; i < SKY_END - PLAY_TOP; ++i) copSetMoveVal(&pList[s_pTitleSkySlot[i]].sMove, pSky[i]);
	if(s_isAga) {
		// (the low nibbles' MOVE: cwSky24())
		const UWORD *pLo = s_pSkyLo[ubMission];
		for(UWORD i = 0; i < SKY_END - PLAY_TOP; ++i) copSetMoveVal(&pList[s_pTitleSkySlot[i] + 2].sMove, pLo[i]);
	}
	for(UBYTE i = 0; i < 7; ++i) {
		copSetMoveVal(&pList[s_uwTitleMtnPal + i].sMove, g_pMissionMtnPal[ubMission][i + 1]);
		copSetMoveVal(&pList[s_uwTitleLandPal + i].sMove, g_pMissionLandPal[ubMission][i + 1]);
	}
	s_pTitleListMission[ubBfr] = ubMission;
}

static void bandSet(tCopCmd *pList, const tCopSlots *pSlots, const tBitMap *pBm, UWORD uwX) {
	copSetMoveVal(&pList[pSlots->uwCon1].sMove, logicScrollDelay(uwX) << 4);  // PF2 delay
	WORD wOffs = logicScrollByteOffset(uwX);
	for(UBYTE p = 0; p < 3; ++p) {
		ULONG ulAddr = (ULONG)pBm->Planes[p] + wOffs;
		UWORD i = pSlots->pPtr[p];
		copSetMoveVal(&pList[i].sMove, ulAddr >> 16);
		copSetMoveVal(&pList[i + 1].sMove, ulAddr & 0xFFFF);
	}
}

// AGA: a band at camera >> ubShift in quarter pixels (the 35 ns bits of BPLCON1)
__attribute__((noinline, optimize("Os"))) static UWORD bandSet4(tCopCmd *pList, const tCopSlots *pSlots,
	const tBitMap *pBm, WORD wCam, UBYTE ubShift, UWORD uwLoopW) {
	UWORD uwX4 = logicBandOffset4(wCam, ubShift, uwLoopW);
	UWORD uwCon1 = logicScrollCon1Pf2Aga(uwX4);
	pList[pSlots->uwCon1].ulCode = ((ULONG)0x102 << 16) | uwCon1;
	WORD wOffs = logicScrollByteOffset4(uwX4);
	for(UBYTE p = 0; p < 3; ++p) {
		ULONG ulAddr = (ULONG)pBm->Planes[p] + wOffs;
		UWORD i = pSlots->pPtr[p];
		copSetMoveVal(&pList[i].sMove, ulAddr >> 16);
		copSetMoveVal(&pList[i + 1].sMove, ulAddr & 0xFFFF);
	}
	if(ubShift == 2) s_uwMtnX4 = uwX4;
	return uwCon1;
}

static UWORD s_uwMtnX, s_uwHillX, s_uwGndX;

// The back list: the jump to the title list or not; this mission's colours
// (the game list rewritten once when it changes, the title list's patched),
// then each band's scroll for the camera, in whichever list shows.
__attribute__((noinline)) static void copperUpdate(UBYTE ubBfr) {
	static WORD pListCam[2][2] = {{-1, -1}, {-1, -1}};   // [title][buffer]: the camera each list's scroll is for
	tCopCmd *pList = s_pView->pCopList->pBackBfr->pList;
	WORD wCam = s_sGame.cam;
	UBYTE isTitle = s_sGame.state == GS_TITLE;
	if(isTitle != s_pListTitle[ubBfr]) {
		s_pListTitle[ubBfr] = isTitle;
		copperJump(pList, ubBfr, isTitle);
	}
	if(isTitle) {
		pList = s_pTitleList[ubBfr];
		if(s_pTitleListMission[ubBfr] != s_sGame.mission) titleListMission(ubBfr, s_sGame.mission);
	}
	else if(s_pListMission[ubBfr] != s_sGame.mission) {
		s_pListMission[ubBfr] = s_sGame.mission;
		copperWriteList(pList, ubBfr, s_sGame.mission, 0);
		pListCam[0][ubBfr] = -1;
	}
	if(wCam == pListCam[isTitle][ubBfr]) return;   // a still camera: nothing to scroll (chip RAM writes are slow)
	pListCam[isTitle][ubBfr] = wCam;
	if(s_isAga) {
		// the far bands in quarter pixels: no judder at 1/8, 1/4, 1/2 speed
		if(!isTitle) bandSet4(pList, &s_sCloudSlots[ubBfr], s_pCloudsBm, wCam, 3, ART_CLOUDS_LOOP_W);
		s_uwMtnCon1 = isTitle ? bandSet4(pList, &s_sMtnSlots[1][ubBfr], s_pTitleMtn, wCam, 2, ART_TITLE_MTN_LOOP_W) :
			bandSet4(pList, &s_sMtnSlots[0][ubBfr], s_pMountains, wCam, 2, ART_MOUNTAINS_LOOP_W);
		s_uwHillCon1 = bandSet4(pList, &s_sHillSlots[isTitle][ubBfr], s_pHills, wCam, 1, ART_HILLS_LOOP_W);
		s_uwGndX = logicBandOffset(wCam, 0, ART_GROUND_LOOP_W);
		bandSet(pList, &s_sGndSlots[isTitle][ubBfr], s_pGround, s_uwGndX);
		return;
	}
	if(!isTitle) bandSet(pList, &s_sCloudSlots[ubBfr], s_pCloudsBm, logicBandOffset(wCam, 3, ART_CLOUDS_LOOP_W));
	s_uwMtnX = logicBandOffset(wCam, 2, ART_MOUNTAINS_LOOP_W);
	s_uwHillX = logicBandOffset(wCam, 1, ART_HILLS_LOOP_W);
	s_uwGndX = logicBandOffset(wCam, 0, ART_GROUND_LOOP_W);
	bandSet(pList, &s_sMtnSlots[isTitle][ubBfr], isTitle ? s_pTitleMtn : s_pMountains, s_uwMtnX);
	bandSet(pList, &s_sHillSlots[isTitle][ubBfr], s_pHills, s_uwHillX);
	s_uwMtnCon1 = (UWORD)(logicScrollDelay(s_uwMtnX) << 4);
	s_uwHillCon1 = (UWORD)(logicScrollDelay(s_uwHillX) << 4);
	s_uwMtnX4 = (UWORD)(s_uwMtnX << 2);
	bandSet(pList, &s_sGndSlots[isTitle][ubBfr], s_pGround, s_uwGndX);
}

// ----------------------------------------------------------- blit queue ---
// Each picture: erase what this buffer showed last time, then draw the
// objects back to front. The CPU feeds the blits to the blitter while it
// runs the game.

#define MSG_LINES 16
#define MSG_LEN 40
static tAgkText s_pMsg[2][MSG_LINES];
// Three layouts: lines 0-3 for the mission, game over, initials...; 4-8 the
// title's, under its logo; 9-15 the hall of fame. No two layouts' lines share
// a row (a layout's lines are cleared a picture at a time after a switch)
#define MSG_TITLE 4
#define MSG_SCORES 9
enum { LAYOUT_PLAY, LAYOUT_TITLE, LAYOUT_SCORES };
static const UBYTE s_pMsgY[MSG_LINES] = {
	96, 110, 132, 146,
	TITLE_TEXT_Y, TITLE_TEXT_Y + 10, TITLE_TEXT_Y + 22, TITLE_TEXT_Y + 32, TITLE_TEXT_Y + 45,
	34, 48, 58, 68, 78, 88, 120,
};
static const UBYTE s_pMsgFirst[3] = {0, MSG_TITLE, MSG_SCORES};
static const UBYTE s_pMsgEnd[3] = {MSG_TITLE, MSG_SCORES, MSG_LINES};
_Static_assert(TITLE_TEXT_Y > 146 + 9, "the title's lines are below the others");
_Static_assert(88 + 8 <= 96 && 120 >= 110 + 8 && 120 + 8 <= 132, "the hall of fame's lines are between the others");

static UBYTE msgLayout(void) {
	return s_sGame.state == GS_TITLE ? LAYOUT_TITLE : s_sGame.state == GS_SCORES ? LAYOUT_SCORES : LAYOUT_PLAY;
}
#define MSG_TYPE_PER_FRAME 2         // new letters a picture: text is slow, and it types itself
                                     // (3 made the title's worst frame 91% with the cloud band)
static char s_pMsgStage[2][MSG_LINES][MSG_LEN + 1];   // what each line is showing / about to show
static ULONG s_pMsgCover[2][MSG_LINES][2];  // columns (bits) this buffer's BOBs covered last time
static ULONG s_pMsgFix[2][MSG_LINES][2];    // columns an erase crossed: redraw their letters
static UBYTE s_pMsgAnyFix[2];
static UBYTE s_pMsgAnyCover[2];              // any bits in s_pMsgCover[buffer]
static UWORD s_pMsgKey[2] = {0xFFFF, 0xFFFF};   // what the lines say (state, blink...) once settled

#define TPL_MAX 52
static tAgkBlitArea s_pErase[2][DRAW_MAX];
static UBYTE s_pErased[2];
static UBYTE s_ubLastDrawn;          // objects in the last picture (the HUD types less after a busy one)
static tBitMap *s_pBobBm[BOB_TYPES], *s_pBobMask[BOB_TYPES];
static UBYTE s_pTplFirst[BOB_TYPES];
static tAgkBlit s_pTpl[TPL_MAX][2];
static UWORD s_pFbRowOffs[FB_H];
static tDraw s_pDraw[DRAW_MAX];

static void bobAdd(UBYTE ubBob, tBitMap *pBm, tBitMap *pMask, UBYTE ubW, UBYTE ubH, UBYTE ubFrames) {
	s_pBobBm[ubBob] = pBm;
	s_pBobMask[ubBob] = pMask;
	if(ubW != g_pBobW[ubBob] || ubH != g_pBobH[ubBob] || ubFrames != g_pBobFrames[ubBob]) {
		agkPrint("AGK ERR logic.c's BOB sizes don't match the art\n");
	}
}

static void bobsCreate(void) {
	bobAdd(BOB_HANGAR, artHangarCreate(), artHangarCreateMask(), ART_HANGAR_W, ART_HANGAR_H, ART_HANGAR_FRAMES);
	bobAdd(BOB_FENCE, artFenceCreate(), artFenceCreateMask(), ART_FENCE_W, ART_FENCE_H, ART_FENCE_FRAMES);
	bobAdd(BOB_BARRACKS, artBarracksCreate(), artBarracksCreateMask(), ART_BARRACKS_W, ART_BARRACKS_H, ART_BARRACKS_FRAMES);
	bobAdd(BOB_TANK, artTankCreate(), artTankCreateMask(), ART_TANK_W, ART_TANK_H, ART_TANK_FRAMES);
	bobAdd(BOB_HOSTAGE, artHostageCreate(), artHostageCreateMask(), ART_HOSTAGE_W, ART_HOSTAGE_H, ART_HOSTAGE_FRAMES);
	bobAdd(BOB_JET, artJetCreate(), artJetCreateMask(), ART_JET_W, ART_JET_H, ART_JET_FRAMES);
	bobAdd(BOB_MINE, artMineCreate(), artMineCreateMask(), ART_MINE_W, ART_MINE_H, ART_MINE_FRAMES);
	bobAdd(BOB_SHOT, artShotCreate(), artShotCreateMask(), ART_SHOT_W, ART_SHOT_H, ART_SHOT_FRAMES);
	bobAdd(BOB_BOMB, artBombCreate(), artBombCreateMask(), ART_BOMB_W, ART_BOMB_H, ART_BOMB_FRAMES);
	bobAdd(BOB_SHELL, artShellCreate(), artShellCreateMask(), ART_SHELL_W, ART_SHELL_H, ART_SHELL_FRAMES);
	bobAdd(BOB_BOOM, artBoomCreate(), artBoomCreateMask(), ART_BOOM_W, ART_BOOM_H, ART_BOOM_FRAMES);
	bobAdd(BOB_FLAG, artFlagCreate(), artFlagCreateMask(), ART_FLAG_W, ART_FLAG_H, ART_FLAG_FRAMES);
	bobAdd(BOB_SHADOW, artShadowCreate(), artShadowCreateMask(), ART_SHADOW_W, ART_SHADOW_H, ART_SHADOW_FRAMES);
	bobAdd(BOB_SMOKE, artSmokeCreate(), artSmokeCreateMask(), ART_SMOKE_W, ART_SMOKE_H, ART_SMOKE_FRAMES);
	UBYTE t = 0;
	for(UBYTE b = 0; b < BOB_TYPES; ++b) {
		s_pTplFirst[b] = t;
		for(UBYTE f = 0; f < g_pBobFrames[b] && t < TPL_MAX; ++f, ++t) {
			// Shots and shells are small and solid: a plain copy (half the bus)
			// Buildings are drawn first, onto an empty buffer: a plain copy too
			UBYTE isCopy = b == BOB_SHOT || b == BOB_SHELL || b == BOB_HANGAR ||
				b == BOB_FENCE || b == BOB_BARRACKS || b == BOB_FLAG;
			agkBlitBob(s_pTpl[t], s_pBobBm[b], isCopy ? 0 : s_pBobMask[b], g_pBobW[b], g_pBobH[b], f, FB_PLANE_BYTES);
		}
	}
	if(t >= TPL_MAX) agkPrint("AGK ERR too many BOB frames: raise TPL_MAX\n");
	for(UWORD y = 0; y < FB_H; ++y) s_pFbRowOffs[y] = (UWORD)(y * FB_ROW_BYTES);
}

__attribute__((noinline)) static void erasesQueue(UBYTE ubBfr) {
	agkBlitqReset();
	const tAgkBlitArea *pDrawn = s_pErase[ubBfr];
	for(UBYTE i = s_pErased[ubBfr]; i--; ++pDrawn) {
		agkBlitClear(agkBlitqSlot(), pDrawn);
		agkBlitqPush();
	}
}

__attribute__((noinline)) static void objectsQueue(UBYTE ubBfr) {
	tBitMap *pFb = s_pFb[ubBfr];
	UBYTE n = logicDraw(&s_sGame, s_pDraw);
	tAgkBlitArea *pErase = s_pErase[ubBfr];
	UBYTE ubDrawn = 0;
	const tDraw *pD = s_pDraw;
	// the lines this state writes in (only their letters need guarding)
	UBYTE ubLayout = msgLayout();
	UBYTE ubMsgFirst = s_pMsgFirst[ubLayout], ubMsgEnd = s_pMsgEnd[ubLayout];
	// what last time's BOBs covered gets erased now: those letters need redrawing
	// (not in the other layout's lines: msgLayoutClear() is about to clear them)
	if(s_pMsgAnyCover[ubBfr]) {
		s_pMsgAnyCover[ubBfr] = 0;
		for(UBYTE l = 0; l < MSG_LINES; ++l) {
			if(l >= ubMsgFirst && l < ubMsgEnd) {
				s_pMsgFix[ubBfr][l][0] |= s_pMsgCover[ubBfr][l][0];
				s_pMsgFix[ubBfr][l][1] |= s_pMsgCover[ubBfr][l][1];
				s_pMsgAnyFix[ubBfr] |= (s_pMsgCover[ubBfr][l][0] | s_pMsgCover[ubBfr][l][1]) != 0;
			}
			else s_pMsgFix[ubBfr][l][0] = s_pMsgFix[ubBfr][l][1] = 0;
			s_pMsgCover[ubBfr][l][0] = s_pMsgCover[ubBfr][l][1] = 0;
		}
	}
	for(UBYTE i = n; i--; ++pD) {
		UBYTE b = pD->bob;
		WORD x = pD->x + FB_X0, y = pD->y;
		if(x < 0 || y < 0 || x + g_pBobW[b] + 16 > FB_W || y + g_pBobH[b] > FB_H) continue;
		tAgkBlit *pB = agkBlitqSlot();
		agkBlitPlace(pB, s_pTpl[s_pTplFirst[b] + pD->frame], g_pBobW[b], pFb->Planes[0] + s_pFbRowOffs[y], (UWORD)x);
		agkBlitArea(pErase++, pB);
		agkBlitqPush();
		++ubDrawn;
		if(y < s_pMsgY[ubMsgEnd - 1] + 8 && y + g_pBobH[b] > s_pMsgY[ubMsgFirst]) {
			// (the erase covers whole words: from the word's start, + 16 px)
			WORD c0 = (WORD)(((x - FB_X0) & ~15) >> 3), c1 = (WORD)((x - FB_X0 + g_pBobW[b] + 15) >> 3);
			if(c0 < 0) c0 = 0;
			if(c1 > MSG_LEN - 1) c1 = MSG_LEN - 1;
			s_pMsgAnyCover[ubBfr] = 1;
			for(UBYTE l = ubMsgFirst; l < ubMsgEnd; ++l) {
				if(y < s_pMsgY[l] + 8 && y + g_pBobH[b] > s_pMsgY[l]) {
					for(WORD c = c0; c <= c1; ++c) s_pMsgCover[ubBfr][l][c >> 5] |= 1UL << (c & 31);
				}
			}
		}
	}
	s_pErased[ubBfr] = ubDrawn;
	s_ubLastDrawn = ubDrawn;
}

// ------------------------------------------------------------------ HUD ---
// Text (agk/text.h): letters in colour 4, shadow in colour 1. The radar is a
// strip of the whole battlefield, 16 world px to a dot, drawn by the CPU.

#define HUD_LEN 40
#define HUD_TYPE_PER_FRAME 8
#define HUD_TYPE_BUSY 3              // ... when the last picture had HUD_BUSY_BOBS objects or more:
#define HUD_BUSY_BOBS 16             // (16 people running into the hangar: the heaviest pictures)
static char s_pHudStage[2][HUD_LEN + 1];
static UBYTE s_pHudPending[2];
static tAgkFont s_sFont;
// The kit's font plus ( ) , and a copyright sign (written '@' in strings)
#define FONT_CHARS AGK_FONT8_CHARS "(),@"
static unsigned char s_pFontRows[AGK_FONT8_COUNT + 4][8];
static const unsigned char s_pFontExtra[4][8] = {
	{0x08, 0x10, 0x20, 0x20, 0x20, 0x10, 0x08, 0x00},   // (
	{0x20, 0x10, 0x08, 0x08, 0x08, 0x10, 0x20, 0x00},   // )
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x08, 0x10},   // ,
	{0x3C, 0x42, 0x9A, 0xA2, 0x9A, 0x42, 0x3C, 0x00},   // (c)
};
static tAgkText s_pHudText[2], s_pHudText2[2], s_pHudSkill[2];
#define HUD_SKILL_X (16 + 264)       // right of the radar

static void hudInit(void) {
	for(UBYTE c = 0; c < AGK_FONT8_COUNT; ++c)
		for(UBYTE r = 0; r < 8; ++r) s_pFontRows[c][r] = g_pAgkFont8[c][r];
	for(UBYTE c = 0; c < 4; ++c)
		for(UBYTE r = 0; r < 8; ++r) s_pFontRows[AGK_FONT8_COUNT + c][r] = s_pFontExtra[c][r];
	agkFontInit(&s_sFont, (const unsigned char (*)[8])s_pFontRows, FONT_CHARS);
	for(UBYTE b = 0; b < 2; ++b) {
		agkTextInit(&s_pHudText[b], &s_sFont, s_pHud[b], 16, 3, HUD_LEN, 4, 1);
		for(UBYTE c = 0; c < HUD_LEN; ++c) s_pHudStage[b][c] = ' ';
		s_pHudStage[b][HUD_LEN] = 0;
		agkTextInit(&s_pHudText2[b], &s_sFont, s_pHud[b], 16, RADAR_Y, 8, 4, 1);   // (8 letters: up to the radar)
		agkTextInit(&s_pHudSkill[b], &s_sFont, s_pHud[b], HUD_SKILL_X, RADAR_Y, 7, 4, 1);
	}
}

// Show szTarget in pText, at most ubBudget changed letters now (the rest
// on later calls: text is slow to draw). Returns 1 when it's all shown.
static UBYTE textType(tAgkText *pText, char *pStage, const char *szTarget, UBYTE ubBudget) {
	UBYTE isDone = 1, isChange = 0;
	for(UBYTE c = 0; szTarget[c]; ++c) {
		if(pStage[c] != szTarget[c]) {
			if(ubBudget) { pStage[c] = szTarget[c]; --ubBudget; isChange = 1; }
			else isDone = 0;
		}
	}
	if(isChange) agkTextSet(pText, pStage);
	return isDone;
}


// The HUD's numbers, right to left: agkTextNumber's digits for less. One DIVU
// a digit while the rest of the value fits a 16-bit quotient (< 655360), two
// above it (agkTextNumber always does two), and none once the rest is 0 (the
// leading zeros). 16 people running into the hangar change three fields and
// the score in one picture (the heaviest one): 20 DIVUs there, ~6 now.
static void hudNumber(char *pOut, ULONG ulValue, UBYTE ubDigits) {
	for(BYTE d = (BYTE)(ubDigits - 1); d >= 0; --d) {
		if(!ulValue) {
			pOut[d] = '0';
			continue;
		}
#if defined(__mc68000__)
		if(ulValue < 655360UL) {
			ULONG ul = ulValue;
			__asm__("divu.w #10,%0" : "+d"(ul));   // quotient low, remainder high
			pOut[d] = (char)('0' + (char)(ul >> 16));
			ulValue = ul & 0xFFFF;
		}
		else {
			ULONG ulHi = ulValue >> 16, ulLo = ulValue & 0xFFFF;
			__asm__("divu.w #10,%0" : "+d"(ulHi));
			ulLo |= ulHi & 0xFFFF0000;
			__asm__("divu.w #10,%0" : "+d"(ulLo));
			pOut[d] = (char)('0' + (char)(ulLo >> 16));
			ulValue = (ulHi << 16) | (ulLo & 0xFFFF);
		}
#else
		pOut[d] = (char)('0' + (char)(ulValue % 10));
		ulValue /= 10;
#endif
	}
}

__attribute__((noinline, noclone)) static void hudUpdate(UBYTE ubBfr) {
	static UBYTE pSaved[2] = {0xFF, 0xFF}, pAboard[2] = {0xFF, 0xFF}, pKilled[2] = {0xFF, 0xFF};
	static UBYTE pLives[2] = {0xFF, 0xFF}, pTitle[2] = {0xFF, 0xFF}, pSkill[2] = {0xFF, 0xFF};
	static ULONG pScore[2] = {~0UL, ~0UL};
	const tGame *g = &s_sGame;
	UBYTE isTitle = logicIsMenu(g);
	ULONG ulScore = isTitle ? g->hiScore : g->score;
	// the line to show, per buffer: only the fields that changed are formatted
	// again (a 6-digit score is 12 DIVUs)
	static char pLine[2][HUD_LEN + 1];
	char *szLine = pLine[ubBfr];
	if(isTitle != pTitle[ubBfr]) {
		pTitle[ubBfr] = isTitle;
		// SAVED 00 ABOARD 00 LOST 00 SCORE 000000   or   (27 blanks) HI 000000
		const char *szT = isTitle ? "HI" : "SAVED 00 ABOARD 00 LOST 00 SCORE";
		UBYTE ubAt = isTitle ? 27 : 0;
		for(UBYTE i = 0; i < HUD_LEN; ++i) szLine[i] = ' ';
		for(UBYTE i = 0; szT[i]; ++i) szLine[ubAt + i] = szT[i];
		szLine[HUD_LEN] = 0;
		pSaved[ubBfr] = pAboard[ubBfr] = pKilled[ubBfr] = 0xFF;
		pScore[ubBfr] = ~0UL;
		s_pHudPending[ubBfr] = 1;
	}
	if(!isTitle) {
		if(g->saved != pSaved[ubBfr]) { pSaved[ubBfr] = g->saved; hudNumber(&szLine[6], g->saved, 2); s_pHudPending[ubBfr] = 1; }
		if(g->aboard != pAboard[ubBfr]) { pAboard[ubBfr] = g->aboard; hudNumber(&szLine[16], g->aboard, 2); s_pHudPending[ubBfr] = 1; }
		if(g->killed != pKilled[ubBfr]) { pKilled[ubBfr] = g->killed; hudNumber(&szLine[24], g->killed, 2); s_pHudPending[ubBfr] = 1; }
	}
	if(ulScore != pScore[ubBfr]) { pScore[ubBfr] = ulScore; hudNumber(&szLine[33], ulScore, 6); s_pHudPending[ubBfr] = 1; }
	if(s_pHudPending[ubBfr]) {
		s_pHudPending[ubBfr] = !textType(&s_pHudText[ubBfr], s_pHudStage[ubBfr], szLine,
			s_ubLastDrawn >= HUD_BUSY_BOBS ? HUD_TYPE_BUSY : HUD_TYPE_PER_FRAME);
	}
	// the left slot: the choppers left; for a while after the last scoring tank
	// kill of a mission, the quota (later tanks pay nothing)
	UBYTE ubLeft = (UBYTE)(g->lives | (g->quotaMsg ? 0x10 : 0));
	if(ubLeft != pLives[ubBfr]) {
		pLives[ubBfr] = ubLeft;
		char szLine[] = "HELI 3";
		szLine[5] = (char)('0' + g->lives);
		agkTextSet(&s_pHudText2[ubBfr], g->quotaMsg ? "QUOTA 10" : szLine);
	}
	// the right slot: the skill; DAMAGED while the chopper is damaged (until the pad mends it)
	UBYTE isDamaged = g->chop.damaged && g->chop.alive && !g->demo;
	UBYTE ubSkill = (UBYTE)(g->difficulty | (g->demo << 4) | (isDamaged << 5));
	if(ubSkill != pSkill[ubBfr]) {
		pSkill[ubBfr] = ubSkill;
		char szSkill[] = "SKILL 3";
		szSkill[6] = (char)('0' + g->difficulty);
		agkTextSet(&s_pHudSkill[ubBfr], g->demo ? "   DEMO" : isDamaged ? "DAMAGED" :
			g->difficulty == DIFFICULTY_INVINCIBLE ? "GODMODE" : szSkill);
	}
}

// The radar: RADAR_W x RADAR_H px at HUD (RADAR_X0, RADAR_Y). Rows 0-4 move
// (the view, jets and mines, people, tanks, the chopper at any of them): cleared
// and redrawn each picture. Rows 5-6, the buildings, are redrawn only when a
// barracks opens; row 7 is the ground line, drawn once. Pens: base and closed
// barracks 7, open barracks and tanks 5, people 4, jets and mines 3, the
// chopper 6. A cleared strip only needs the pen's set planes ORed in.
#define RADAR_DYN_H 5
#define RADAR_P1 HUD_PLANE_BYTES
#define RADAR_P2 (2 * HUD_PLANE_BYTES)
#define RADAR_BYTE0 ((RADAR_X0 + 16) >> 3)

static inline void radarDot(UBYTE *pRow, UBYTE ubX, UBYTE ubPen) {
	UWORD uwX = (UWORD)(((RADAR_X0 + 16) & 7) + ubX);
	UBYTE *p = pRow + (uwX >> 3);
	UBYTE ubBit = (UBYTE)(0x80 >> (uwX & 7));
	if(ubPen & 1) p[0] |= ubBit;
	if(ubPen & 2) p[RADAR_P1] |= ubBit;
	if(ubPen & 4) p[RADAR_P2] |= ubBit;
}

// The strip's rows 0..RADAR_DYN_H-1 are cleared by the blitter (radarClear(),
// queued first in the picture); row RADAR_H-1 is the ground line, drawn once.
static tAgkBlitArea s_pRadarArea[2];
static UBYTE s_pRadarBuildings[2] = {0xFF, 0xFF};   // which barracks were open when drawn

static void radarCreate(void) {
	for(UBYTE b = 0; b < 2; ++b) {
		UBYTE *pTop = s_pHud[b]->Planes[0] + RADAR_Y * HUD_ROW_BYTES + RADAR_BYTE0;
		s_pRadarArea[b].pD = pTop;
		s_pRadarArea[b].uwSize = (UWORD)(((RADAR_DYN_H * FB_BPP) << 6) | (RADAR_W / 16));
		s_pRadarArea[b].wMod = (WORD)(HUD_PLANE_BYTES - RADAR_W / 8);
		UBYTE *pGround = pTop + (RADAR_H - 1) * HUD_ROW_BYTES + RADAR_P1;   // pen 2
		for(UBYTE i = 0; i < RADAR_W / 8; ++i) pGround[i] = 0xFF;
	}
}

static inline void radarClear(UBYTE ubBfr) {
	agkBlitClear(agkBlitqSlot(), &s_pRadarArea[ubBfr]);
	agkBlitqPush();
}

// After agkBlitqFinish(): the strip is clear
__attribute__((noinline)) static void radarUpdate(UBYTE ubBfr) {
	const tGame *g = &s_sGame;
	UBYTE *pRow[RADAR_H];
	UBYTE *pTop = s_pRadarArea[ubBfr].pD;
	for(UBYTE y = 0; y < RADAR_H; ++y, pTop += HUD_ROW_BYTES) pRow[y] = pTop;
	// the buildings (rows 5-6): only when a barracks has opened since
	UBYTE ubOpen = 0;
	for(UBYTE b = 0; b < BARRACKS_COUNT; ++b) ubOpen |= (UBYTE)((g->pBarracks[b].hp == 0) << b);
	if(ubOpen != s_pRadarBuildings[ubBfr]) {
		s_pRadarBuildings[ubBfr] = ubOpen;
		for(UBYTE y = 5; y < 7; ++y) {
			UBYTE *p = pRow[y];
			for(UBYTE i = 0; i < FB_BPP * RADAR_P1; i += RADAR_P1)
				for(UBYTE k = 0; k < RADAR_W / 8 + 1; ++k) p[i + k] = 0;
		}
		UBYTE ubBase = logicRadarX(HANGAR_X);
		radarDot(pRow[6], ubBase, 7);
		radarDot(pRow[6], (UBYTE)(ubBase + 1), 7);
		radarDot(pRow[5], logicRadarX(BORDER_X), 3);
		radarDot(pRow[6], logicRadarX(BORDER_X), 3);
		for(UBYTE b = 0; b < BARRACKS_COUNT; ++b) {
			UBYTE x = logicRadarX(g->pBarracks[b].x);
			UBYTE pen = (ubOpen >> b) & 1 ? 5 : 7;
			radarDot(pRow[5], x, pen);
			radarDot(pRow[5], (UBYTE)(x + 1), pen);
			radarDot(pRow[6], x, pen);
			radarDot(pRow[6], (UBYTE)(x + 1), pen);
		}
	}
	UBYTE ubCam = logicRadarX(g->cam);
	radarDot(pRow[0], ubCam, 3);
	radarDot(pRow[0], (UBYTE)(ubCam + SCREEN_W / 16), 3);
	UBYTE *pPeople = pRow[3] + RADAR_P2;   // pen 4: plane 3 only
	for(UBYTE k = 0; k < g->outCount; ++k) {
		const tHostage *h = &g->pHostages[g->pOut[k]];
		if(h->state != HS_DEAD) {
			UWORD uwX = (UWORD)(((RADAR_X0 + 16) & 7) + logicRadarX(h->x));
			pPeople[uwX >> 3] |= (UBYTE)(0x80 >> (uwX & 7));
		}
	}
	for(UBYTE i = 0; i < TANK_MAX; ++i) {
		if(g->pTanks[i].active) radarDot(pRow[4], logicRadarX(g->pTanks[i].x), 5);
	}
	for(UBYTE i = 0; i < JET_MAX; ++i) {
		const tJet *pJet = &g->pJets[i];
		WORD x = pJet->x;
		if(!pJet->active) continue;
		if(pJet->warn) {
			// the incoming-jet warning (its first 2 s): the blip, wider and a row taller, flashes white,
			// and holds at the radar's edge while the jet is still beyond the battlefield
			x = x < 0 ? 0 : x >= WORLD_W ? WORLD_W - 1 : x;
			if(g->frame & 8) {
				UBYTE ubX = logicRadarX(x);
				radarDot(pRow[1], ubX, 7);
				radarDot(pRow[2], ubX, 7);
				radarDot(pRow[1], (UBYTE)(ubX + 1), 7);
				radarDot(pRow[2], (UBYTE)(ubX + 1), 7);
				continue;
			}
		}
		if(x >= 0 && x < WORLD_W) radarDot(pRow[1], logicRadarX(x), 5);
	}
	for(UBYTE i = 0; i < MINE_MAX; ++i) {
		WORD x = g->pMines[i].x;
		if(g->pMines[i].active && x >= 0 && x < WORLD_W) radarDot(pRow[1], logicRadarX(x), 3);
	}
	if(!logicIsMenu(g) && g->state != GS_ENTRY) {
		UBYTE x = logicRadarX(g->chop.x);
		UBYTE y = (UBYTE)((g->chop.y - CEIL_Y) >> 5);   // 0..6
		if(y > RADAR_DYN_H - 1) y = RADAR_DYN_H - 1;
		radarDot(pRow[y], x, 6);
		radarDot(pRow[y], (UBYTE)(x + 1), 6);
	}
}

// --------------------------------------------------------------- messages ---
// Lines of text in the play area (title, mission, game over), drawn into the
// frame buffer by the CPU after the blitter's done. They're redrawn every
// picture they show: a BOB's erase may cross them.

// Per buffer, a bit per line: letters may be showing there. When the screen
// changes, the blitter clears the lines of the layout not in use, a line a
// picture, rather than typing spaces for seconds.
static UWORD s_pMsgLit[2];
static const UWORD s_pMsgLayoutBits[3] = {   // each layout's lines, a bit each
	(1 << MSG_TITLE) - 1, (1 << MSG_SCORES) - (1 << MSG_TITLE), (UWORD)((1UL << MSG_LINES) - (1 << MSG_SCORES)),
};

static void msgInit(void) {
	for(UBYTE b = 0; b < 2; ++b) {
		for(UBYTE l = 0; l < MSG_LINES; ++l) {
			agkTextInit(&s_pMsg[b][l], &s_sFont, s_pFb[b], FB_X0, s_pMsgY[l], MSG_LEN, 4, 1);
			for(UBYTE c = 0; c < MSG_LEN; ++c) s_pMsgStage[b][l][c] = ' ';
			s_pMsgStage[b][l][MSG_LEN] = 0;
		}
	}
}

// Queued before the picture's objects: a line of the layout not in use now
// cleared, if anything shows there (the screen changed). One line a picture
// (the switch itself is a heavy picture), just the words its letters are in
// (one character = one byte of each plane).
__attribute__((noinline)) static void msgLayoutClear(UBYTE ubBfr) {
	// (after the HUD's done typing: both at once made the heaviest picture)
	if(!s_pMsgLit[ubBfr]) return;
	UWORD uwLit = (UWORD)(s_pMsgLit[ubBfr] & ~s_pMsgLayoutBits[msgLayout()]);
	if(!uwLit || s_pHudPending[ubBfr]) return;
	UBYTE l = 0;
	while(!(uwLit & 1)) { uwLit >>= 1; ++l; }
	s_pMsgLit[ubBfr] &= (UWORD)~(1 << l);
	char *pStage = s_pMsgStage[ubBfr][l];
	UBYTE c0 = 0, c1 = MSG_LEN;
	while(c0 < MSG_LEN && pStage[c0] == ' ') ++c0;
	if(c0 == MSG_LEN) return;   // spaces typed over it already
	while(pStage[c1 - 1] == ' ') --c1;
	UBYTE b0 = (UBYTE)(c0 & ~1), b1 = (UBYTE)((c1 + 1) & ~1);
	tAgkBlitArea sArea;
	sArea.pD = s_pFb[ubBfr]->Planes[0] + s_pFbRowOffs[s_pMsgY[l]] + FB_X0 / 8 + b0;
	sArea.uwSize = (UWORD)(((8 * FB_BPP) << 6) | ((b1 - b0) >> 1));
	sArea.wMod = (WORD)(FB_PLANE_BYTES - (b1 - b0));
	agkBlitClear(agkBlitqSlot(), &sArea);
	agkBlitqPush();
	char *pShown = s_pMsg[ubBfr][l].szShown;
	for(UBYTE c = c0; c < c1; ++c) pStage[c] = pShown[c] = ' ';
	s_pMsgFix[ubBfr][l][0] = s_pMsgFix[ubBfr][l][1] = 0;
	s_pMsgCover[ubBfr][l][0] = s_pMsgCover[ubBfr][l][1] = 0;
}

static void msgCentre(char *szOut, const char *szText) {
	UBYTE ubLen = 0;
	while(szText[ubLen] && ubLen < MSG_LEN) ++ubLen;
	UBYTE ubPad = (UBYTE)((MSG_LEN - ubLen) >> 1);
	for(UBYTE i = 0; i < MSG_LEN; ++i) szOut[i] = ' ';
	for(UBYTE i = 0; i < ubLen; ++i) szOut[ubPad + i] = szText[i];
	szOut[MSG_LEN] = 0;
}

// A hall of fame line: "1  ABC  012345  M2  CAPTAIN", the new entry between dashes
__attribute__((noinline, optimize("Os"))) static void scoreRow(char *p, UBYTE i) {
	const tScore *s = &s_sGame.pScores[i];
	UBYTE isNew = i == s_sGame.newRank;
	*p++ = isNew ? '-' : ' ';
	*p++ = ' ';
	*p++ = (char)('1' + i);
	*p++ = ' ';
	*p++ = ' ';
	for(UBYTE c = 0; c < SCORE_NAME; ++c) *p++ = s->name[c];
	*p++ = ' ';
	*p++ = ' ';
	agkTextNumber(p, s->score, 6);
	p += 6;
	*p++ = ' ';
	*p++ = ' ';
	if(s->mission > MISSION_COUNT) { *p++ = 'A'; *p++ = 'L'; *p++ = 'L'; }
	else { *p++ = 'M'; *p++ = (char)('0' + s->mission); *p++ = ' '; }
	*p++ = ' ';
	const char *szR = logicRank(s->saved);
	UBYTE n = 0;
	while(szR[n]) *p++ = szR[n++];
	while(n++ < 7) *p++ = ' ';
	*p++ = ' ';
	*p++ = isNew ? '-' : ' ';
	*p = 0;
}

// The hall of fame's and the initials' lines; returns the low bits of the key
__attribute__((noinline, optimize("Os"))) static UWORD msgMenu(const char **pText, char (*pRows)[32], char *szA, char *szB) {
	const tGame *g = &s_sGame;
	if(g->state == GS_SCORES) {
		pText[MSG_SCORES] = "HALL OF FAME";
		for(UBYTE i = 0; i < SCORE_COUNT; ++i) {
			scoreRow(pRows[i], i);
			pText[MSG_SCORES + 1 + i] = pRows[i];
		}
		static const char *const pStatus[] = {
			"", "SAVING TO DISK...", "SAVED TO DISK", "DISK WRITE-PROTECTED: NOT SAVED",
			"DISK ERROR: NOT SAVED", "", "",
		};
		pText[MSG_SCORES + 6] = g->newRank < SCORE_COUNT ? pStatus[s_ubDiskStatus] : "";
		return (UWORD)(s_ubDiskStatus << 1);
	}
	// the initials: up/down a letter, fire the next, the current one blinking
	const tScore *s = &g->pScores[g->newRank];
	const char *szR = "RANK 1   000000";
	for(UBYTE i = 0; i < 16; ++i) szB[i] = szR[i];
	szB[5] = (char)('1' + g->newRank);
	agkTextNumber(&szB[9], s->score, 6);
	pText[0] = "GREAT SCORE!";
	pText[1] = szB;
	pText[2] = "ENTER YOUR INITIALS";
	UBYTE isBlink = (g->frame >> 4) & 1;
	for(UBYTE c = 0; c < SCORE_NAME; ++c) {
		szA[2 * c] = (c == g->entryPos && isBlink) ? '-' : s->name[c];
		szA[2 * c + 1] = ' ';
	}
	szA[2 * SCORE_NAME - 1] = 0;
	pText[3] = szA;
	// (the key changes with any letter, the cursor or the blink)
	return (UWORD)(0x8000 | ((g->entryPos & 3) << 13) | (isBlink << 12) |
		((s->name[0] * 37 + s->name[1] * 11 + s->name[2]) & 0xFFF));
}

__attribute__((noinline)) static void msgUpdate(UBYTE ubBfr) {
	const tGame *g = &s_sGame;
	const char *pText[MSG_LINES] = {"", "", "", "", "", "", "", "", "", "", "", "", "", "", "", ""};
	char szA[24], szB[28];
	char pRows[SCORE_COUNT][32];
	UWORD uwKeyLow = 0;
	switch(g->state) {
		case GS_SCORES:
		case GS_ENTRY:
			uwKeyLow = msgMenu(pText, pRows, szA, szB);
			break;
		case GS_TITLE:
			pText[MSG_TITLE] = "BY PAUL ROSS SEIBERT";
			pText[MSG_TITLE + 1] = "@ 2026 DESERT PALMS SOFTWARE";
			pText[MSG_TITLE + 2] = "FLY IN. BLOW THE BARRACKS OPEN.";
			pText[MSG_TITLE + 3] = "LAND. BRING THEM HOME.";
			pText[MSG_TITLE + 4] = (g->frame & 32) ? "PRESS FIRE" : "";
			break;
		case GS_INTRO: {
			// the preview card (the route map goes between lines 1 and 2)
			const char *szM = "MISSION 1 OF 3";
			for(UBYTE i = 0; i < 15; ++i) szA[i] = szM[i];
			szA[8] = (char)('1' + g->mission);
			szA[13] = (char)('0' + MISSION_COUNT);
			pText[0] = szA;
			pText[1] = g_pMissionName[g->mission];
			pText[2] = "64 HELD IN 4 BARRACKS";
			pText[3] = logicBrief(g);
		} break;
		case GS_LOST:
			if(g->stateTimer < LOST_FRAMES - 40) {
				pText[1] = "CHOPPER DOWN";
				const char *szL = g->lives == 0 ? "NO CHOPPERS LEFT" :
					g->lives == 1 ? "1 CHOPPER LEFT" : "2 CHOPPERS LEFT";
				pText[2] = szL;
			}
			break;
		case GS_CLEAR: {
			pText[1] = "MISSION COMPLETE";
			const char *szS = "SAVED 00 OF 64";
			for(UBYTE i = 0; i < 15; ++i) szB[i] = szS[i];
			agkTextNumber(&szB[6], g->saved, 2);
			pText[2] = szB;
		} break;
		case GS_OVER:
		case GS_VICTORY: {
			pText[0] = g->state == GS_OVER ? "GAME OVER" : "ALL MISSIONS COMPLETE";
			const char *szS = "TOTAL SAVED 000";
			for(UBYTE i = 0; i < 16; ++i) szB[i] = szS[i];
			agkTextNumber(&szB[12], g->savedTotal, 3);
			pText[2] = szB;
			const char *szR = logicRank(g->savedTotal);
			const char *szP = "RANK: ";
			UBYTE n = 0;
			while(szP[n]) { szA[n] = szP[n]; ++n; }
			for(UBYTE i = 0; szR[i]; ++i) szA[n++] = szR[i];
			szA[n] = 0;
			pText[3] = szA;
		} break;
		default:
			break;
	}
	// Nothing new, nothing crossed: nothing to do (the usual case in play)
	UWORD uwKey = uwKeyLow & 0x8000 ? uwKeyLow : (UWORD)(g->state << 8 | g->mission << 4 | uwKeyLow |
		(g->state == GS_TITLE ? (g->frame >> 5) & 1 : 0) |
		(g->state == GS_LOST && g->stateTimer < LOST_FRAMES - 40 ? 2 : 0));
	if(uwKey == s_pMsgKey[ubBfr] && !s_pMsgAnyFix[ubBfr]) return;
	s_pMsgAnyFix[ubBfr] = 0;
	char szLine[MSG_LEN + 1];
	UBYTE ubBudget = MSG_TYPE_PER_FRAME;
	UBYTE ubLayout = msgLayout();
	// (just this layout's lines: msgLayoutClear() blanks the others)
	for(UBYTE l = s_pMsgFirst[ubLayout]; l < s_pMsgEnd[ubLayout]; ++l) {
		char *pStage = s_pMsgStage[ubBfr][l];
		ULONG *pFix = s_pMsgFix[ubBfr][l];
		UBYTE isChange = 0;
		if(ubBudget) {
			// (no letters left to type this picture: the later lines wait, unread)
			msgCentre(szLine, pText[l]);
			for(UBYTE c = 0; c < MSG_LEN && ubBudget; ++c) {
				if(pStage[c] != szLine[c]) {
					pStage[c] = szLine[c];
					--ubBudget;
					isChange = 1;
				}
			}
		}
		if(pFix[0] | pFix[1]) {
			// letters an erase crossed: draw again (not spaces: they'd cut into the BOB)
			tAgkText *pT = &s_pMsg[ubBfr][l];
			for(UBYTE c = 0; c < MSG_LEN; ++c) {
				if((pFix[c >> 5] & (1UL << (c & 31))) && pStage[c] != ' ') {
					pT->szShown[c] = 1;
					isChange = 1;
				}
			}
			pFix[0] = pFix[1] = 0;
		}
		if(isChange) {
			agkTextSet(&s_pMsg[ubBfr][l], pStage);
			s_pMsgLit[ubBfr] |= (UWORD)(1 << l);
		}
	}
	// all typed: settled until the key changes
	s_pMsgKey[ubBfr] = ubBudget ? uwKey : 0xFFFF;
}

// -------------------------------------------------------------- route map ---
// The preview card's strip map of the battlefield (logicRouteMap: world x / 12,
// 256 px) on a dark panel between its text lines 1 and 2: the enemy ground hatched red, ours
// blue, the fence, the four barracks, the pad and the hangar, and the chopper's
// "you are here" arrow. It's drawn once at startup into an image (the map is
// the same every mission: the chopper starts at the base); at the card the CPU
// copies it into each frame buffer once, after the blitter's done (drawing it
// there took 1.6 frames); the blitter clears it when play starts (queued before
// the objects). Nothing else is drawn on those lines during the card.
#define ROUTE_Y 120
#define ROUTE_H 10
#define ROUTE_X0 ((SCREEN_W - ROUTE_W) / 2)
_Static_assert(ROUTE_Y >= 110 + 9 && ROUTE_Y + ROUTE_H <= 132, "the map sits between the card's lines 1 and 2");
_Static_assert(ROUTE_X0 % 16 == 0 && ROUTE_W % 16 == 0, "the map: whole words (the blitter clears it)");
static UBYTE s_pRouteLit[2];
#define ROUTE_ROW_BYTES (ROUTE_W / 8)   // the image: rows of 3 planes (interleaved, like the frame buffers)
static UBYTE s_pRouteImg[ROUTE_H * FB_BPP * ROUTE_ROW_BYTES] __attribute__((aligned(4)));

// row y of x0..x0+w-1 in a pen, through a pattern (0xFF solid, 0xAA/0x55 hatched):
// whole bytes where it can (the map starts on a byte)
__attribute__((noinline, optimize("Os"))) static void routeSpan(UBYTE *pTop, UWORD x0, UWORD w, UBYTE y, UBYTE ubPen, UBYTE ubPat) {
	UBYTE *pRow = pTop + y * FB_BPP * ROUTE_ROW_BYTES;
	UWORD x1 = (UWORD)(x0 + w);
	for(UWORD x = x0; x < x1;) {
		UBYTE *p = pRow + (x >> 3);
		UBYTE ubMask;
		if(!(x & 7) && x + 8 <= x1) { ubMask = 0xFF; x += 8; }
		else { ubMask = (UBYTE)(0x80 >> (x & 7)); ++x; }
		ubMask &= ubPat;
		for(UBYTE i = 0; i < FB_BPP; ++i, p += ROUTE_ROW_BYTES) {
			if(ubPen & (1 << i)) *p |= ubMask;
			else *p &= (UBYTE)~ubMask;
		}
	}
}

__attribute__((noinline, optimize("Os"))) static void routeBox(UBYTE *pTop, UWORD x0, UWORD w, UBYTE y0, UBYTE y1, UBYTE ubPen, UBYTE isHatch) {
	for(UBYTE y = y0; y <= y1; ++y) routeSpan(pTop, x0, w, y, ubPen, isHatch ? ((y & 1) ? 0x55 : 0xAA) : 0xFF);
}

// At startup (after logicInit): the map into s_pRouteImg
__attribute__((noinline, optimize("Os"))) static void routeCreate(void) {
	UBYTE *pTop = s_pRouteImg;
	for(UWORD i = 0; i < sizeof(s_pRouteImg); ++i) {
		pTop[i] = (i / ROUTE_ROW_BYTES) % FB_BPP ? 0 : 0xFF;    // a dark panel (pen 1)
	}
	tRouteMark pMarks[ROUTE_MARKS];
	UBYTE n = logicRouteMap(&s_sGame, pMarks);
	for(UBYTE i = 0; i < n; ++i) {
		const tRouteMark *m = &pMarks[i];
		UWORD x = m->x;
		switch(m->kind) {
			case ROUTE_ENEMY:                                   // hostile ground: red hatching
			case ROUTE_HOME:                                    // ours: blue
				routeBox(pTop, x, m->w, 7, 8, m->kind == ROUTE_ENEMY ? 5 : 7, 1);
				routeBox(pTop, x, m->w, 9, 9, 2, 0);              // the ground line
				break;
			case ROUTE_FENCE:
				routeBox(pTop, x, 1, 3, 8, 3, 0);
				break;
			case ROUTE_BARRACKS:                                // a hut: red roof, stone walls, a door
				routeBox(pTop, x - 2, 5, 3, 3, 5, 0);
				routeBox(pTop, x - 3, 7, 4, 4, 5, 0);
				routeBox(pTop, x - 3, 7, 5, 8, 3, 0);
				routeBox(pTop, x, 1, 6, 8, 1, 0);
				break;
			case ROUTE_PAD:
				routeBox(pTop, x, m->w, 8, 8, 6, 0);
				break;
			case ROUTE_HANGAR:
				routeBox(pTop, x, m->w, 3, 3, 4, 0);
				routeBox(pTop, x, m->w, 4, 8, 7, 0);
				break;
			case ROUTE_HERE:                                    // you are here: a yellow arrow
				routeBox(pTop, x - 2, 5, 0, 0, 6, 0);
				routeBox(pTop, x - 1, 3, 1, 1, 6, 0);
				routeBox(pTop, x, 1, 2, 3, 6, 0);
				break;
		}
	}
}

// The card's first picture in a buffer (the blitter done): the map copied in
__attribute__((noinline)) static void routeShow(UBYTE ubBfr) {
	const ULONG *pSrc = (const ULONG *)s_pRouteImg;
	UBYTE *pRow = s_pFb[ubBfr]->Planes[0] + s_pFbRowOffs[ROUTE_Y] + (FB_X0 + ROUTE_X0) / 8;
	for(UBYTE y = 0; y < ROUTE_H * FB_BPP; ++y, pRow += FB_PLANE_BYTES) {
		ULONG *pDst = (ULONG *)pRow;   // (even addresses: longs are fine on a 68000)
		for(UBYTE k = 0; k < ROUTE_ROW_BYTES / 4; ++k) *pDst++ = *pSrc++;
	}
	s_pRouteLit[ubBfr] = 1;
}

// Queued before the objects: the map cleared once the card's over
__attribute__((noinline)) static void routeClear(UBYTE ubBfr) {
	if(!s_pRouteLit[ubBfr] || s_sGame.state == GS_INTRO) return;
	s_pRouteLit[ubBfr] = 0;
	tAgkBlitArea sArea;
	sArea.pD = s_pFb[ubBfr]->Planes[0] + s_pFbRowOffs[ROUTE_Y] + (FB_X0 + ROUTE_X0) / 8;
	sArea.uwSize = (UWORD)(((ROUTE_H * FB_BPP) << 6) | (ROUTE_W / 16));
	sArea.wMod = (WORD)(FB_PLANE_BYTES - ROUTE_W / 8);
	agkBlitClear(agkBlitqSlot(), &sArea);
	agkBlitqPush();
}

// ---------------------------------------------------------------- chopper ---
// Two sets of the chopper's sprite bitmaps, one per copper buffer: the back
// set gets the new position (its control words) and the back copper list
// points channels 0-3 at it.

static tBitMap *s_pChopFrames[2][ART_CHOPPER_FRAMES][ART_CHOPPER_PARTS];

static void chopperCreate(void) {
	for(UBYTE b = 0; b < 2; ++b)
		for(UBYTE f = 0; f < ART_CHOPPER_FRAMES; ++f)
			for(UBYTE p = 0; p < ART_CHOPPER_PARTS; ++p) s_pChopFrames[b][f][p] = artChopperCreate(f, p);
}

__attribute__((noinline)) static void chopperUpdate(UBYTE ubBfr) {
	const tGame *g = &s_sGame;
	UBYTE ubPose = logicChopperFrame(g);
	UBYTE ubFrame = (UBYTE)(((ubPose & 0x7F) << 1) + logicRotorPhase(g));
	if(ubPose & CHOP_LEFT) ubFrame = (UBYTE)(ubFrame + ART_CHOPPER_MIRROR);
	WORD wX = (WORD)(g->chop.x - g->cam - 16), wY = (WORD)(g->chop.y - 16);
	if(!logicChopperVisible(g) || wX < -32 || wX > SCREEN_W) wY = -64;
	UWORD uwVStart = (UWORD)(s_pView->ubPosY + wY), uwVStop = (UWORD)(uwVStart + ART_CHOPPER_H);
	tCopCmd *pList = s_pView->pCopList->pBackBfr->pList;
	for(UBYTE p = 0; p < ART_CHOPPER_PARTS; ++p) {
		tBitMap *pBm = s_pChopFrames[ubBfr][ubFrame][p];
		UWORD uwHStart = (UWORD)(s_pView->ubPosX - 1 + wX + (p >> 1) * 16);
		UWORD *pHead = (UWORD *)pBm->Planes[0];
		pHead[0] = (UWORD)((uwVStart << 8) | ((uwHStart >> 1) & 0xFF));
		pHead[1] = (UWORD)((uwVStop << 8) | ((p & 1) << 7) | (((uwVStart >> 8) & 1) << 2) |
			(((uwVStop >> 8) & 1) << 1) | (uwHStart & 1));
		ULONG ulPtr = (ULONG)pBm->Planes[0];
		copSetMoveVal(&pList[COP_SPRITES_POS + 2 * (ART_CHOPPER_CHANNEL + p)].sMove, ulPtr >> 16);
		copSetMoveVal(&pList[COP_SPRITES_POS + 2 * (ART_CHOPPER_CHANNEL + p) + 1].sMove, ulPtr & 0xFFFF);
	}
}

// ---------------------------------------------------------------- far sky ---
// In front of the cloud band (PF2) and behind PF1: wisps (dusk, dawn) or stars
// (night) on sprite channel 6, colours 29-31 per mission (logic.h). The DMA
// list holds all of them one under the other; per copper buffer a copy, whose
// control words the CPU rewrites each frame (positions only).

#define SKY_CH_DOT 6
#define SKY_PARKED_H 0x1F0                  // past the right edge: not shown
#define DOT_WORDS (SKYDOT_COUNT * 2 + 2 * 3 * SKYDOT_COUNT + 2)
static UWORD *s_pDotList[2][2];              // [buffer][wisps, stars]
static UWORD s_pDotHead[SKYDOT_COUNT];       // word offsets of each item's control words

static void sprCtl(UWORD *pHead, UWORD uwH, UWORD uwV0, UWORD uwV1) {
	pHead[0] = (UWORD)((uwV0 << 8) | ((uwH >> 1) & 0xFF));
	pHead[1] = (UWORD)((uwV1 << 8) | (((uwV0 >> 8) & 1) << 2) | (((uwV1 >> 8) & 1) << 1) | (uwH & 1));
}

static void skyListFill(UWORD *pList, const UWORD *pShape) {
	UWORD w = 0;
	for(UBYTE i = 0; i < SKYDOT_COUNT; ++i) {
		s_pDotHead[i] = w;
		pList[w++] = 0;
		pList[w++] = 0;
		for(UWORD k = 0; k < 2u * g_pSkyDots[i].h; ++k) pList[w++] = pShape[k];
	}
	pList[w++] = 0;   // end of the list
	pList[w++] = 0;
}

static void skyCreate(void) {
	for(UBYTE b = 0; b < 2; ++b) {
		for(UBYTE k = 0; k < 2; ++k) {
			s_pDotList[b][k] = memAllocChip(DOT_WORDS * sizeof(UWORD));
			skyListFill(s_pDotList[b][k], k ? s_pStars : s_pWisp);
		}
	}
}

static void skyDestroy(void) {
	for(UBYTE b = 0; b < 2; ++b)
		for(UBYTE k = 0; k < 2; ++k) memFree(s_pDotList[b][k], DOT_WORDS * sizeof(UWORD));
}

static void skyPointer(tCopCmd *pList, UBYTE ubCh, const void *pData) {
	ULONG ulPtr = (ULONG)pData;
	copSetMoveVal(&pList[COP_SPRITES_POS + 2 * ubCh].sMove, ulPtr >> 16);
	copSetMoveVal(&pList[COP_SPRITES_POS + 2 * ubCh + 1].sMove, ulPtr & 0xFFFF);
}

__attribute__((noinline)) static void skyUpdate(UBYTE ubBfr) {
	static WORD s_pShownKey[2] = {-1, -1};       // what each buffer's list holds (cam / 16, mission)
	const tGame *g = &s_sGame;
	UWORD uwX0 = (UWORD)(s_pView->ubPosX - 1), uwY0 = s_pView->ubPosY;
	UBYTE isStars = logicSkyDotsAreStars(g->mission);
	UWORD *pDots = s_pDotList[ubBfr][isStars];
	const UWORD *pPal = g_pMissionSkyDotPal[g->mission];
	UWORD uwColor31 = isStars ? logicStarTwinkle(g) : pPal[2];   // (twinkles)
	if(s_isAga) {
		// colours 29-31 from the copper: a CPU write landing between a sky
		// line's LOCT MOVEs would set only the low nibbles
		tCopCmd *pCl = g->state == GS_TITLE ? s_pTitleList[ubBfr] : s_pView->pCopList->pBackBfr->pList;
		UWORD i = s_pSkyDotColorSlot[g->state == GS_TITLE];
		pCl[i].ulCode = ((ULONG)(REG_COLOR00 + 2 * 29) << 16) | pPal[0];
		pCl[i + 1].ulCode = ((ULONG)(REG_COLOR00 + 2 * 30) << 16) | pPal[1];
		pCl[i + 2].ulCode = ((ULONG)(REG_COLOR00 + 2 * 31) << 16) | uwColor31;
	}
	else g_pCustom->color[31] = uwColor31;
	WORD wKey = (WORD)(((g->cam >> 4) << 2) | g->mission);
	if(wKey == s_pShownKey[ubBfr]) return;      // the dots move 1 px per 16 px of camera
	s_pShownKey[ubBfr] = wKey;
	for(UBYTE i = 0; i < SKYDOT_COUNT; ++i) {
		WORD wX = logicSkyDotX(g, i);
		UWORD uwV0 = (UWORD)(uwY0 + g_pSkyDots[i].y);
		sprCtl(pDots + s_pDotHead[i], wX == SKY_HIDDEN ? SKY_PARKED_H : (UWORD)(uwX0 + wX), uwV0,
			(UWORD)(uwV0 + g_pSkyDots[i].h));
	}
	tCopCmd *pList = s_pView->pCopList->pBackBfr->pList;
	skyPointer(pList, SKY_CH_DOT, pDots);
	// colours 29-30 (31 above): nothing else writes them (the chopper leaves 28-31 free)
	if(!s_isAga) {
		g_pCustom->color[29] = pPal[0];
		g_pCustom->color[30] = pPal[1];
	}
}

// ---------------------------------------------------------------- weather ---
// Sprite channels 4-5 (logic.h): per kind and channel a DMA list of WX_SLOTS
// items, the shapes drawn at startup, shared by both copper lists. First thing
// in a picture, still in the vertical blank (no bitplane DMA: the CPU at full
// speed, and the sprite DMA hasn't read the list yet), one channel's control
// words are rewritten: channel 0 on even frames, 1 on odd ones, so each moves
// at 25 Hz, half of them every frame - and it costs half. The vertical parts
// come from a table per slide (s_pWxV): an item is an add, a compare, two ORs.

#define WX_CH0 4
#define WX_ITEM_WORDS (2 + 2 * WX_H)
#define WX_WORDS (WX_SLOTS * WX_ITEM_WORDS + 2)
static UWORD *s_pWxList[3][2];                 // [kind][channel]
static UWORD s_pWxV[2][WX_PITCH][WX_SLOTS][2]; // [channel][slide][slot]: control words, no x
static UWORD s_pWxH[2][WX_PARTICLES];          // particle x + the display's left edge

// Per kind and channel, the pixels of an item: {x, y, colour 1-3 (25-27)},
// x 0..15, y 0..WX_H-1. Two particles an item, a different pair per channel.
typedef struct { UBYTE x, y, c; } tWxDot;
#define WX_DOTS 8
static const tWxDot s_pWxDots[3][2][WX_DOTS] = {
	{   // dust: single motes, orange and yellow, catching the sun
		{{4, 2, 3}, {12, 9, 2}, {0, 0, 0}},
		{{9, 5, 2}, {2, 12, 3}, {0, 0, 0}},
	},
	{   // rain: pale blue streaks, slanting like the fall (4 down, 1 left)
		{{4, 1, 1}, {4, 2, 1}, {3, 3, 1}, {3, 4, 1}, {12, 8, 1}, {12, 9, 1}, {11, 10, 1}, {11, 11, 1}},
		{{8, 4, 1}, {8, 5, 1}, {7, 6, 1}, {7, 7, 1}, {2, 11, 1}, {2, 12, 1}, {1, 13, 1}, {1, 14, 1}},
	},
	{   // spray: pale droplets, a glint of the dawn in some
		{{5, 3, 1}, {6, 3, 1}, {12, 10, 3}, {0, 0, 0}},
		{{9, 6, 1}, {2, 12, 1}, {3, 12, 3}, {0, 0, 0}},
	},
};

static void weatherFill(UWORD *pList, const tWxDot *pDots) {
	UWORD pShape[2 * WX_H];
	for(UBYTE i = 0; i < 2 * WX_H; ++i) pShape[i] = 0;
	for(UBYTE d = 0; d < WX_DOTS; ++d) {
		const tWxDot *p = &pDots[d];
		if(!p->c) continue;
		UWORD uwBit = (UWORD)(0x8000 >> p->x);
		if(p->c & 1) pShape[2 * p->y] |= uwBit;       // DATA: bit 0 of the colour
		if(p->c & 2) pShape[2 * p->y + 1] |= uwBit;   // DATB: bit 1
	}
	UWORD w = 0;
	for(UBYTE k = 0; k < WX_SLOTS; ++k) {
		pList[w++] = 0;
		pList[w++] = 0;
		for(UBYTE i = 0; i < 2 * WX_H; ++i) pList[w++] = pShape[i];
	}
	pList[w++] = 0;   // end of the list
	pList[w] = 0;
}

static void weatherCreate(void) {
	for(UBYTE k = 0; k < 3; ++k) {
		for(UBYTE c = 0; c < 2; ++c) {
			s_pWxList[k][c] = memAllocChipClear(WX_WORDS * sizeof(UWORD));
			weatherFill(s_pWxList[k][c], s_pWxDots[k][c]);
		}
	}
	UWORD uwX0 = (UWORD)(s_pView->ubPosX - 1);
	for(UBYTE c = 0; c < 2; ++c) {
		for(UBYTE p = 0; p < WX_PARTICLES; ++p) s_pWxH[c][p] = (UWORD)(uwX0 + g_pWxX[c][p]);
		for(UBYTE o = 0; o < WX_PITCH; ++o) {
			for(UBYTE k = 0; k < WX_SLOTS; ++k) {
				UWORD uwV0 = (UWORD)(s_pView->ubPosY + WX_TOP + c * (WX_PITCH / 2) + o + k * WX_PITCH);
				sprCtl(s_pWxV[c][o][k], 0, uwV0, (UWORD)(uwV0 + WX_H));
			}
		}
	}
}

static void weatherDestroy(void) {
	for(UBYTE k = 0; k < 3; ++k)
		for(UBYTE c = 0; c < 2; ++c) memFree(s_pWxList[k][c], WX_WORDS * sizeof(UWORD));
}

// Call first in the picture (the vertical blank): see above
__attribute__((noinline)) static void weatherUpdate(void) {
	const tGame *g = &s_sGame;
	tCopCmd *pList = s_pView->pCopList->pBackBfr->pList;
	tWeatherFrame sW;
	if(!logicWeather(g, &sW)) {
		skyPointer(pList, WX_CH0, s_pBlankRow);       // two zero words: nothing
		skyPointer(pList, WX_CH0 + 1, s_pBlankRow);
		return;
	}
	UWORD *const *pLists = s_pWxList[logicWeatherKind(g->mission)];
	UBYTE c = (UBYTE)(g->frame & 1);
	UWORD *p = pLists[c];
	const UWORD *pH = s_pWxH[c];
	const UWORD *pV = s_pWxV[c][sW.off][0];
	const UWORD uwWrapH = (UWORD)(s_pView->ubPosX - 1 + SCREEN_W);
	UWORD uwShift = (UWORD)sW.shift;
	UBYTE ubP = (UBYTE)(-sW.wraps);
	for(UBYTE k = WX_SLOTS; k--; ++ubP, pV += 2, p += WX_ITEM_WORDS) {
		UWORD uwH = (UWORD)(pH[ubP & (WX_PARTICLES - 1)] + uwShift);
		if(uwH >= uwWrapH) uwH = (UWORD)(uwH - SCREEN_W);
		p[0] = (UWORD)(pV[0] | (uwH >> 1));
		p[1] = (UWORD)(pV[1] | (uwH & 1));
	}
	skyPointer(pList, WX_CH0, pLists[0]);
	skyPointer(pList, WX_CH0 + 1, pLists[1]);
}

// ------------------------------------------------------------------ rotor ---
// Paula channel 2: the rotor, a loop made at startup (sound/tools/make_rotor.py
// previews it with the same integer arithmetic: keep the two in step). A deep
// blade thump, like a Huey's whop-whop-whop: on every blade pass three damped
// resonators (~55, ~130 and ~230 Hz: the weight and the body small speakers
// still play), a short band-passed noise slap (~250 Hz-1.2 kHz, a quarter of
// the thump), a quiet rumble between. Four passes of ROTOR_PASS samples a loop,
// each a little different. The rate is the Paula period (logicRotorPeriod):
// 5 thumps a second at hover, 8-9 flat out, a louder, faster spin-up at
// lift-off. SPEC section 9.

#define ROTOR_CHANNEL 2
#define ROTOR_PASSES 4
#define ROTOR_SAMPLES (ROTOR_PASS * ROTOR_PASSES)   // 4800 bytes of chip RAM
static BYTE *s_pRotorWave;   // chip RAM

// One run of the loop's synthesis: a warm-up pass (the last pass again, so the
// resonators' tails are in the first one: it loops seamlessly), then the loop.
// wScale 0: returns the peak; else writes the samples times wScale / 256.
static LONG rotorSynth(WORD wScale) {
	// resonators, Q14 (a1 = 2 r cos w, a2 = r^2), and their kicks
	static const WORD pA1[3] = {32636, 32311, 31559};
	static const WORD pA2[3] = {16306, 16229, 16113};
	static const WORD pKick[3] = {1100, 2300, 2800};
	static const UBYTE pStrength[ROTOR_PASSES] = {128, 112, 122, 104};
	UWORD uwLfsr = 0x1D2B;
	LONG pY1[3] = {0, 0, 0}, pY2[3] = {0, 0, 0};
	LONG lLp1 = 0, lLp2 = 0, lRum = 0, lMax = 0;
	for(UWORD n = 0; n < ROTOR_PASS + ROTOR_SAMPLES; ++n) {
		UWORD i = n < ROTOR_PASS ? (UWORD)(ROTOR_SAMPLES - ROTOR_PASS + n) : (UWORD)(n - ROTOR_PASS);
		UBYTE k = (UBYTE)(i / ROTOR_PASS);
		UWORD t = (UWORD)(i - k * ROTOR_PASS);
		UWORD uwLsb = uwLfsr & 1;
		uwLfsr >>= 1;
		if(uwLsb) uwLfsr ^= 0xB400;
		LONG lNoise = (LONG)(uwLfsr & 0xFF) - 128;
		// the thump: three resonators kicked at the pass start (16 x 16 multiplies: MULS.W)
		LONG lThump = 0;
		for(UBYTE j = 0; j < 3; ++j) {
			LONG lY = ((((LONG)pA1[j] * (WORD)pY1[j]) - ((LONG)pA2[j] * (WORD)pY2[j])) >> 14) + (t == 0 ? pKick[j] : 0);
			pY2[j] = pY1[j];
			pY1[j] = lY;
			lThump += lY;
		}
		lThump >>= 4;
		// the slap: noise band-passed (a fast low-pass minus a slow one), enveloped
		lLp1 += (lNoise * 16 - lLp1) >> 1;
		lLp2 += (lLp1 - lLp2) >> 2;
		LONG lBand = (lLp1 - lLp2) >> 2;
		WORD wTs = (WORD)t - 10;
		LONG lEnv = wTs < 0 || wTs >= 150 ? 0 : wTs < 8 ? (LONG)wTs * 32 : 256 - (((LONG)(WORD)(wTs - 8) * (WORD)461) >> 8);
		LONG lSlap = (((LONG)(WORD)lBand * (WORD)lEnv >> 7) * (WORD)pStrength[k]) >> 7;
		// the rumble: noise low-passed hard, quiet
		lRum += (lNoise * 16 - lRum) >> 6;
		LONG v = lThump + lSlap + (lRum >> 4);
		if(n < ROTOR_PASS) continue;
		if(!wScale) {
			LONG lAbs = v < 0 ? -v : v;
			if(lAbs > lMax) lMax = lAbs;
			continue;
		}
		LONG s = ((LONG)(WORD)v * wScale) >> 8;
		s_pRotorWave[i] = (BYTE)(s > 127 ? 127 : s < -128 ? -128 : s);
	}
	return lMax;
}

static UBYTE s_isRotorPending;

// The loop itself, after boot (the splash is black; ~a few frames of work:
// at startup it pushed AROS past the harness's boot window)
static void rotorSetup(void) {
	s_isRotorPending = 0;
	LONG lPeak = rotorSynth(0);
	rotorSynth((WORD)((126L << 8) / (lPeak ? lPeak : 1)));   // peak at ~124
	agkPerfSkip();                                    // (more than a frame of work, on purpose)
}

static void rotorCreate(void) {
	s_pRotorWave = memAllocChipClear(ROTOR_SAMPLES);  // silent until rotorSetup fills it
	s_isRotorPending = 1;
	ptplayerSetChannelsForPlayer(0xF & ~(1 << ROTOR_CHANNEL));
	volatile struct AudChannel *pAud = &g_pCustom->aud[ROTOR_CHANNEL];
	pAud->ac_ptr = (UWORD *)s_pRotorWave;
	pAud->ac_len = ROTOR_SAMPLES / 2;
	pAud->ac_vol = 0;
	pAud->ac_per = 591;   // (the hover; rotorUpdate takes over from the first frame)
	g_pCustom->dmacon = DMAF_SETCLR | (DMAF_AUD0 << ROTOR_CHANNEL);
}

// The rules (period, volume, the spin-up) are logic.c's, unit tested
__attribute__((noinline)) static void rotorUpdate(void) {
	const tGame *g = &s_sGame;
	volatile struct AudChannel *pAud = &g_pCustom->aud[ROTOR_CHANNEL];
	pAud->ac_per = logicRotorPeriod(g);
	pAud->ac_vol = s_isPaused ? 0 : logicRotorVolume(g);
}

static void rotorDestroy(void) {
	g_pCustom->dmacon = DMAF_AUD0 << ROTOR_CHANNEL;
	g_pCustom->aud[ROTOR_CHANNEL].ac_vol = 0;
	memFree(s_pRotorWave, ROTOR_SAMPLES);
}

// ---------------------------------------------------------------- sound ---

static void musicFor(UBYTE ubState) {
	UBYTE ubSong = 0xFF;
	if(ubState == GS_TITLE || ubState == GS_INTRO || ubState == GS_SCORES || ubState == GS_ENTRY) ubSong = SOUND_MUSIC_THEME;
	else if(ubState == GS_CLEAR || ubState == GS_VICTORY) ubSong = SOUND_MUSIC_FANFARE;
	else if(ubState == GS_OVER) ubSong = SOUND_MUSIC_GAMEOVER;
	if(!s_isMusicOn) ubSong = 0xFF;
	if(ubSong == s_ubSong) return;
	s_ubSong = ubSong;
	if(ubSong == 0xFF) soundMusicStop();
	else soundMusicStart(ubSong);
}

__attribute__((noinline)) static void playSounds(UBYTE ubStateBefore) {
	const tGame *g = &s_sGame;
	UWORD e = g->events;
	if(g->state != ubStateBefore) {
		musicFor(g->state);
		if(ubStateBefore == GS_TITLE && g->state == GS_INTRO) soundPlay(SOUND_SFX_START);
	}
	if(e & EV_CRASH) soundPlay(SOUND_SFX_CRASH);
	else if(e & EV_OPEN) soundPlay(SOUND_SFX_OPEN);
	else if(e & EV_BOOM) soundPlay(SOUND_SFX_BOOM);
	if(g->events2 & EV2_JETWARN) soundPlay(SOUND_SFX_JETWARN);   // the alert (priority 3: before the jet's scream)
	if(e & EV_JET) soundPlay(SOUND_SFX_JET);
	if(e & EV_TANKFIRE) soundPlay(SOUND_SFX_TANKFIRE);
	if(e & EV_SAVED) soundPlay(SOUND_SFX_SAVED);
	else if(e & (EV_BOARD | EV_UNLOAD)) soundPlay(SOUND_SFX_BOARD);
	if(e & EV_CANNON) soundPlay(SOUND_SFX_CANNON);
	if(e & EV_BOMB) soundPlay(SOUND_SFX_BOMB);
	if(e & EV_LAND) soundPlay(SOUND_SFX_LAND);
}

// ---------------------------------------------------------------- setup ---

static void readInput(tInput *pIn) {
	// Joystick in port 2 (ACE JOY1), a second button if the pad has one, and
	// the keyboard (arrows, Space = fire, X = turn). The stick is read straight
	// from the hardware, as ACE's joyProcess() does, but port 2 only (that one
	// filled a 24-entry table for four ports every frame: ~1.5% of a frame).
	UWORD uwDat = g_pCustom->joy1dat;
	UWORD uwUpDown = (UWORD)(uwDat ^ (uwDat >> 1));
	UBYTE isLeft = (uwDat >> 9) & 1, isRight = (uwDat >> 1) & 1;
	UBYTE isUp = (uwUpDown >> 8) & 1, isDown = uwUpDown & 1;
	pIn->dx = (isLeft || keyCheck(KEY_LEFT)) ? -1 : (isRight || keyCheck(KEY_RIGHT)) ? 1 : 0;
	pIn->dy = (isUp || keyCheck(KEY_UP)) ? -1 : (isDown || keyCheck(KEY_DOWN)) ? 1 : 0;
	pIn->fire = !(g_pCia[CIA_A]->pra & 0x80) || keyCheck(KEY_SPACE);
	pIn->fire2 = !(g_pCustom->potinp & (1 << 14)) || keyCheck(KEY_X);
}

void genericCreate(void) {
	agkDebugInit();
	agkPrint("AGK boot rotor-rescue\n");
	s_isAga = agaDetect();
	if(s_isAga) {
		// the A1200: longer copper lists (the 24-bit sky), the 8-plane logo
		agkPrint("AGK AGA\n");
		s_uwCopCount = COP_RAW_COUNT_AGA;
		s_uwTitleCopCount = TITLE_COP_COUNT_AGA;
		s_ulTitleLogoBytes = TITLE_LOGO8_BYTES;
	}
	keyCreate();
	joyOpen();

	s_pMountains = artMountainsCreate();   // (first: the copper list's length depends on it)
	s_pTitleMtn = artTitleMtnCreate();
	// the mountains' rows with no gap anywhere in their loop: below the first
	// of them (down to the band's end) the sky never shows
	for(UBYTE r = ART_MOUNTAINS_H; r--;) {
		UBYTE isOpaque = 1;
		for(UWORD k = 0; k < ART_MOUNTAINS_LOOP_W / 8 && isOpaque; ++k) {
			UWORD uwOffs = (UWORD)(r * s_pMountains->BytesPerRow + k);
			isOpaque = (s_pMountains->Planes[0][uwOffs] | s_pMountains->Planes[1][uwOffs] |
				s_pMountains->Planes[2][uwOffs]) == 0xFF;
		}
		if(!isOpaque) break;
		s_ubSkyHiddenFrom = (UBYTE)(MTN_Y + r);
	}
	// The sky per mission and line: OCS line-dithered (a MOVE on most lines
	// where the sky shows: the list grows by the changes beyond the plain
	// gradient's, which COP_RAW_COUNT has room for), the A1200's 24-bit below
	_Static_assert(SKY_END - PLAY_TOP == SKY_LINES, "the sky table is logic.c's");
	WORD wSkyMoves = 0;
	for(UBYTE m = 0; m < MISSION_COUNT; ++m) {
		logicSkyFillDither(m, s_pSky[m]);
		WORD wN = 0;
		for(UWORD y = 0; y < SKY_LINES; ++y) {
			UBYTE isShown = PLAY_TOP + y < s_ubSkyHiddenFrom || PLAY_TOP + y == SKY_END - 1;
			wN += isShown && (!y || s_pSky[m][y] != s_pSky[m][y - 1]);
		}
		if(wN > wSkyMoves) wSkyMoves = wN;
	}
	// (the rest of the list takes ~210 of COP_RAW_COUNT; chip RAM is tight on
	// Kickstart 3.1, so no more slack than that)
	if(!s_isAga) s_uwCopCount = (UWORD)(COP_RAW_COUNT - 120 + 2 * wSkyMoves);   // (a WAIT and a MOVE each)

	s_pView = viewCreate(0,
		TAG_VIEW_GLOBAL_PALETTE, 1,
		TAG_VIEW_COPLIST_MODE, VIEW_COPLIST_MODE_RAW,
		TAG_VIEW_COPLIST_RAW_COUNT, s_uwCopCount,
		TAG_DONE);
	s_pVPort = vPortCreate(0, TAG_VPORT_VIEW, s_pView, TAG_VPORT_BPP, 6, TAG_DONE);

	for(UBYTE b = 0; b < 2; ++b) s_pFb[b] = bitmapCreate(FB_W, FB_H, FB_BPP, BMF_CLEAR | BMF_INTERLEAVED);
	for(UBYTE b = 0; b < 2; ++b) s_pHud[b] = bitmapCreate(HUD_W, PLAY_TOP, FB_BPP, BMF_CLEAR | BMF_INTERLEAVED);
	s_pBlankRow = memAllocChipClear(FETCH_BYTES);
	// the title logo's memory and the title's copper lists (filled by titleLoad())
	s_pTitleLogo = memAllocChip(s_ulTitleLogoBytes);
	s_pTitleFixed = (const UWORD *)s_pTitleLogo;
	s_pTitleRows = (const UWORD (*)[TITLE_LOGO_ROW_REGS])(s_pTitleLogo + TITLE_LOGO_ROWS_OFFS);
	for(UBYTE b = 0; b < 2; ++b) s_pTitleList[b] = memAllocChip(s_uwTitleCopCount * sizeof(tCopCmd));
	s_pCloudsBm = artCloudsCreate();
	s_pHills = artHillsCreate();
	s_pGround = artGroundCreate();
	bobsCreate();
	hudInit();
	hudPanelInit();
	radarCreate();
	msgInit();

	logicInit(&s_sGame);
	s_isSplash = splashCreate(s_pView->ubPosY, s_pBlankRow);   // from the floppy: before systemUnuse
	agkPerfSkip();
	for(UBYTE i = 0; i < 6; ++i) { hudUpdate(0); hudUpdate(1); }   // (typed a few letters a call)

	spriteManagerCreate(s_pView, COP_SPRITES_POS, 0);
	systemSetDmaBit(DMAB_SPRITE, 1);
	artPaletteApply(s_pVPort->pPalette);       // PF1 and PF2 colours
	artChopperApplyColors(s_pVPort->pPalette); // colours 17-31
	chopperCreate();
	skyCreate();
	weatherCreate();

	if(s_isAga) {
		// the A1200's sky: 24 bits, split into the high and low nibbles' words
		s_pSkyLo = memAllocFast(MISSION_COUNT * (SKY_END - PLAY_TOP) * sizeof(UWORD));
		for(UBYTE m = 0; m < MISSION_COUNT; ++m) {
			for(UWORD y = 0; y < SKY_END - PLAY_TOP; ++y) {
				ULONG c = logicSkyColor24(m, (UBYTE)y);
				s_pSky[m][y] = (UWORD)(((c >> 12) & 0xF00) | ((c >> 8) & 0x0F0) | ((c >> 4) & 0x00F));
				s_pSkyLo[m][y] = (UWORD)(((c >> 8) & 0xF00) | ((c >> 4) & 0x0F0) | (c & 0x00F));
			}
		}
	}
	tCopList *pCopList = s_pView->pCopList;
	s_pCopBfrA = pCopList->pBackBfr;
	// the game lists jump to the title lists (it starts at the title)
	for(UBYTE b = 0; b < 2; ++b) s_pListTitle[b] = s_sGame.state == GS_TITLE;
	copperWriteList(pCopList->pBackBfr->pList, 0, 0, 0);
	copperWriteList(pCopList->pFrontBfr->pList, 1, 0, 0);
	s_pListMission[0] = s_pListMission[1] = 0;

	soundCreate();
	rotorCreate();
	// With the splash: the title logo once it's on screen (genericProcess), then
	// the music. Without it, now.
	s_isTitlePending = 1;
	if(!s_isSplash) titleLoad();

	viewLoad(s_pView);
	systemUnuse();
	splashShow();
	agkDebugAsync(1);
}

// ------------------------------------------------------------ the frame ---

// A new entry in the hall of fame: "SAVING TO DISK..." for a moment, then the
// save (a few seconds of OS time, between pictures), then how it went
__attribute__((noinline, optimize("Os"))) static void scoresSave(void) {
	tGame *g = &s_sGame;
	if(g->state != GS_SCORES) return;
	if(s_ubDiskStatus != DISK_SAVING) {
		s_ubDiskStatus = DISK_SAVING;
		return;
	}
	if(g->stateTimer > SCORES_FRAMES - 120) return;   // (the table and this line type themselves first)
	g->scoresChanged = 0;
	s_ubDiskStatus = hiscoreSave(g);
	agkPrint(s_ubDiskStatus == DISK_SAVED ? "AGK scores saved\n" :
		s_ubDiskStatus == DISK_PROTECTED ? "AGK scores not saved: write-protected\n" : "AGK scores not saved: disk error\n");
	scoresReport();
	agkPerfSkip();
}

__attribute__((noinline)) static void reportState(UBYTE ubStateBefore) {
	const tGame *g = &s_sGame;
	const tChopper *c = &g->chop;
	if(g->evBarracks) {
		agkState("barracks open", g->evBarracks - 1);
		agkEnd();
	}
	if(g->events & EV_CRASH) agkPrint("AGK chopper lost\n");
	if(g->events & EV_SHRUG) agkPrint("AGK hit ignored (invincible)\n");
	if(g->events & EV_TANKTELL) agkPrint("AGK tank tell\n");
	if(g->events2 & EV2_JETWARN) agkPrint("AGK jet warn\n");
	if(g->events2 & EV2_JETTELL) agkPrint("AGK jet tell\n");
	if(g->events2 & EV2_JETFIRE) agkPrint("AGK jet fire\n");
	if(g->events2 & EV2_DAMAGE) agkPrint("AGK chopper damaged\n");
	if(g->events2 & EV2_REPAIR) agkPrint("AGK chopper repaired\n");
	if(s_ubScrollTrace) {
		// the mountains' and hills' fine scroll as written to the copper: the
		// quarter pixels (BPLCON1 bits 13-12, AGA; always 0 on OCS)
		--s_ubScrollTrace;
		agkState("pf2 mtn4", s_uwMtnX4);
		agkState("mtnq", (s_uwMtnCon1 >> 12) & 3);
		agkState("hillq", (s_uwHillCon1 >> 12) & 3);
		agkEnd();
	}
	if(g->events & EV_LAND) {
		agkState("landed x", c->x);
		agkEnd();
	}
	if(g->events & (EV_BOARD | EV_UNLOAD | EV_SAVED | EV_DIED)) {
		// the counters alone: a full line costs a few % of a frame
		agkState("aboard", g->aboard);
		agkState("saved", g->saved);
		agkState("killed", g->killed);
		agkEnd();
	}
	if(g->state == GS_PLAY && !(g->frame & 15)) {
		// the pacing sample (tools/pace.py): what threatens the chopper right now
		UBYTE ubJets = 0, ubShots = 0, ubJetShots = 0, ubTells = 0;
		for(UBYTE i = 0; i < JET_MAX; ++i) {
			const tJet *j = &g->pJets[i];
			if(j->active && j->x > g->cam - 16 && j->x < g->cam + SCREEN_W + 16) ++ubJets;
			if(j->active && j->tell) ++ubTells;
		}
		for(UBYTE i = 0; i < ESHOT_MAX; ++i) {
			if(!g->pEShots[i].active) continue;
			++ubShots;
			if(g->pEShots[i].kind != ESHOT_SHELL) ++ubJetShots;
		}
		agkState("pace", g->frame);
		agkState("pskill", g->difficulty);
		agkState("pghost", g->ghost);
		agkState("pmission", g->mission);
		agkState("ptrip", g->trip);
		agkState("jets", ubJets);
		agkState("tells", ubTells);
		agkState("jatk", g->jetAttacks);
		agkState("eshots", ubShots);
		agkState("jshots", ubJetShots);
		agkState("pland", c->landed);
		agkEnd();
	}
	if(g->state != ubStateBefore || (g->events & EV_TAKEOFF) || !(g->frame & 31) || g->frame <= 3) {
		agkState("frame", g->frame);
		agkState("state", g->state);
		agkState("mission", g->mission);
		agkState("x", c->x);
		agkState("y", c->y);
		agkState("face", c->face);
		agkState("landed", c->landed);
		agkState("cam", g->cam);
		agkState("aboard", g->aboard);
		agkState("saved", g->saved);
		agkState("killed", g->killed);
		agkState("lives", g->lives);
		agkState("trip", g->trip);
		agkState("score", (LONG)g->score);
		agkState("rotorPer", logicRotorPeriod(g));
		agkState("rotorVol", logicRotorVolume(g));
		agkEnd();
		if(g->demo && g->state != ubStateBefore) agkPrint("AGK demo\n");
	}
}

void genericProcess(void) {
	agkPerfBegin();
	s_ulFrameVbl = timerGet();
	// the weather first, in the vertical blank (last frame's positions: a frame
	// behind the ground, unseen in rain and dust)
	if(!s_isSplash) weatherUpdate();
	keyProcess();     // (the joystick: readInput(), from the hardware)
	if(keyCheck(KEY_ESCAPE)) {
		gameExit();
		return;
	}
	if(keyUse(KEY_M)) {
		s_isMusicOn = !s_isMusicOn;
		musicFor(s_sGame.state);
	}
	if(keyUse(KEY_P) && s_sGame.state == GS_PLAY) s_isPaused = !s_isPaused;
	skillKeys();

	tInput sIn;
	readInput(&sIn);
	if(s_isSplash) {
		s_isSplash = splashProcess(sIn.fire || keyCheck(KEY_RETURN));
		agkPerfEnd();
		while(timerGet() - s_ulFrameVbl < 1) continue;
		if(!s_isReady) {
			s_isReady = 1;
			agkReady();
		}
		else if(s_isTitlePending) titleLoad();   // the splash is still black
		else if(s_isRotorPending) rotorSetup();  // (a frame later)
		return;
	}
	if(s_isTitlePending) titleLoad();   // (not loaded during the splash: now)
	if(s_isRotorPending) rotorSetup();
	if(s_ubAfterSplash < 3 && ++s_ubAfterSplash == 2) {
		// the game's copper list is showing now: its colours back, the logo freed
		if(s_isAga) agaWaitPaletteWindow();
		for(UBYTE i = 1; i < 32; ++i) g_pCustom->color[i] = s_pVPort->pPalette[i];
		// The splash's 47 KB stay allocated until the game quits (genericDestroy):
		// ACE's memFree switches the OS back on for each free, which froze the
		// title for ~6 frames right as it appeared. Chip RAM has room for it.
	}
	UBYTE ubBfr = backBuffer();
	// last picture's BOBs erased first: the blitter clears them while the CPU
	// types the HUD (the top of the picture is in the border, the bus mostly free)
	erasesQueue(ubBfr);
	radarClear(ubBfr);
	hudUpdate(ubBfr);
	agkBlitqPoll();
	UBYTE ubStateBefore = s_sGame.state;
	if(!s_isPaused) {
		logicUpdate(&s_sGame, &sIn);
		playSounds(ubStateBefore);
	}
	msgLayoutClear(ubBfr);
	routeClear(ubBfr);
	agkBlitqPoll();
	objectsQueue(ubBfr);
	agkBlitqPoll();
	copperUpdate(ubBfr);
	agkBlitqPoll();
	chopperUpdate(ubBfr);
	skyUpdate(ubBfr);
	rotorUpdate();
	reportState(ubStateBefore);

	agkBlitqFinish();
	radarUpdate(ubBfr);
	if(s_sGame.state == GS_INTRO && !s_pRouteLit[ubBfr]) routeShow(ubBfr);
	else msgUpdate(ubBfr);   // (not both in one picture)
	copProcessBlocks();
	agkPerfEnd();
	while(timerGet() - s_ulFrameVbl < 1) continue;
	if(s_sGame.scoresChanged) scoresSave();

	if(s_sGame.frame == 2) {
		agkState("copper_used", s_uwCopUsed);
		agkState("copper_max", s_uwCopCount);
		agkState("title_copper_used", s_uwTitleCopUsed);
		agkState("title_copper_max", s_uwTitleCopCount);
		agkEnd();
		if(s_uwCopUsed > s_uwCopCount || s_uwTitleCopUsed > s_uwTitleCopCount) agkPrint("AGK ERR copper list overflow\n");
		if(!s_isReady) {
			s_isReady = 1;
			agkReady();
		}
	}
	if(s_sGame.frame == SYNC_FRAME) agkPrint("AGK t0\n");
}

void genericDestroy(void) {
	agkDebugAsync(0);
	agkBlitqFinish();
	systemUse();
	splashDestroy();
	rotorDestroy();
	soundDestroy();
	systemSetDmaBit(DMAB_SPRITE, 0);
	spriteManagerDestroy();
	skyDestroy();
	weatherDestroy();
	for(UBYTE b = 0; b < 2; ++b)
		for(UBYTE f = 0; f < ART_CHOPPER_FRAMES; ++f)
			for(UBYTE p = 0; p < ART_CHOPPER_PARTS; ++p) bitmapDestroy(s_pChopFrames[b][f][p]);
	for(UBYTE b = 0; b < BOB_TYPES; ++b) {
		bitmapDestroy(s_pBobBm[b]);
		bitmapDestroy(s_pBobMask[b]);
	}
	bitmapDestroy(s_pGround);
	bitmapDestroy(s_pHills);
	bitmapDestroy(s_pMountains);
	bitmapDestroy(s_pTitleMtn);
	bitmapDestroy(s_pCloudsBm);
	for(UBYTE b = 0; b < 2; ++b) memFree(s_pTitleList[b], s_uwTitleCopCount * sizeof(tCopCmd));
	if(s_pTitleLogo) memFree(s_pTitleLogo, s_ulTitleLogoBytes);
	if(s_pSkyLo) memFree(s_pSkyLo, MISSION_COUNT * (SKY_END - PLAY_TOP) * sizeof(UWORD));
	memFree(s_pBlankRow, FETCH_BYTES);
	for(UBYTE b = 0; b < 2; ++b) {
		bitmapDestroy(s_pHud[b]);
		bitmapDestroy(s_pFb[b]);
	}
	viewDestroy(s_pView);
	joyClose();
	keyDestroy();
}
