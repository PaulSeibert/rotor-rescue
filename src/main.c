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
//   COLOR00: a sky gradient per mission, line by line from the copper.
//   Sprites 0-3: the chopper, 32 px wide, 15 colours (attached pairs).
//   Sprites 4-5: the weather (dust, rain, spray), in front of everything.
//   Sprite 6: the far sky's wisps or stars, behind the playfields.
//   At boot: the Desert Palms Software logo (splash.c), its own copper lists.
//   Paula channel 2: the rotor, a looped chop whose pitch follows the
//        airspeed (driven here); music on 0, 1, 3; effects on 1 and 3.

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
#include <agk/perf.h>
#include <agk/text.h>
#include "art.h"        // generated from art/ by agk (agk help-art)
#include "sound.h"      // generated from sound/ by agk (agk help-sound)
#include "logic.h"
#include "splash.h"
#include "sky_gfx.h"    // tools/mkclouds.py

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

// The PF2 bands, in game lines (the clouds: CLOUDS_Y in logic.h)
#define CLOUDS_END (CLOUDS_Y + ART_CLOUDS_H)  // 112: blank PF2 rows to the mountains
#define MTN_Y 120
#define GAP_Y (MTN_Y + ART_MOUNTAINS_H)      // 196: one blank line, the land palette loads
#define HILLS_Y (GAP_Y + 1)                  // 197
#define GROUND_BAND_Y (HILLS_Y + ART_HILLS_H) // 224 .. 255
#define SKY_END GAP_Y
#define HUD_COLOR 0x112

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
static tBitMap *s_pMountains, *s_pHills, *s_pGround;
static UBYTE *s_pBlankRow;          // FETCH_BYTES of zeros: PF2 where there's no band
static tCopBfr *s_pCopBfrA;         // copper buffer 0
static UWORD s_uwCopUsed;
static UBYTE s_pListMission[2];     // which mission's colours each copper list has
static UWORD s_pSky[MISSION_COUNT][SKY_END - PLAY_TOP];   // COLOR00 per line (from logic.c, at startup)

typedef struct {
	UWORD uwCon1;          // index of the MOVE BPLCON1
	UWORD pPtr[3];         // per plane: index of its pointer's high-word MOVE
} tCopSlots;
static tCopSlots s_sCloudSlots[2], s_sMtnSlots[2], s_sHillSlots[2], s_sGndSlots[2];   // per list: the sky's length varies
static tBitMap *s_pCloudsBm;        // the far clouds band (PF2, camera / 8)

static tGame s_sGame;
static ULONG s_ulFrameVbl;
static UBYTE s_isMusicOn = 1, s_ubSong = 0xFF;
static UBYTE s_isPaused;
static UBYTE s_isSplash;         // the logo's showing
static UBYTE s_ubAfterSplash;    // frames since it ended (its memory goes after 2)
static UBYTE s_isReady;
static UBYTE s_ubSkillKeyWait;  // frames left to press 0-5 after Ctrl+L
static const UBYTE s_pDigitKeys[DIFFICULTY_MAX + 1] = {KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5};

// Ctrl+L, then 0-5 within 2 s: the difficulty (0 = invincible, for testing)
static void skillKeys(void) {
	if(keyCheck(KEY_CONTROL) && keyUse(KEY_L)) s_ubSkillKeyWait = 100;
	if(!s_ubSkillKeyWait) return;
	--s_ubSkillKeyWait;
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

static void copperWriteList(tCopCmd *pList, UBYTE ubBfr, UBYTE ubMission) {
	tAgkCopWriter sW;
	agkCopInit(&sW, pList, COP_TOP_POS, s_pView->ubPosY);
	ULONG pBlank[3] = {(ULONG)s_pBlankRow, (ULONG)s_pBlankRow, (ULONG)s_pBlankRow};
	const tBitMap *pFb = s_pFb[ubBfr];
	ULONG ulFbRow = (ULONG)(PLAY_TOP * FB_ROW_BYTES + FB_FETCH_OFFS);
	ULONG ulFbCloudRow = (ULONG)(CLOUDS_Y * FB_ROW_BYTES + FB_FETCH_OFFS);
	tCopSlots *pCld = &s_sCloudSlots[ubBfr];

	// Top of the frame (vertical blank): the HUD, 3 planes
	agkCopMove(&sW, &g_pCustom->bplcon0, BPLCON0_3);
	agkCopMove(&sW, &g_pCustom->bplcon2, BPLCON2_RR);
	agkCopMove(&sW, &g_pCustom->ddfstrt, DDFSTRT_SCROLL);
	agkCopMove(&sW, &g_pCustom->ddfstop, DDFSTOP_LORES);
	agkCopMove(&sW, &g_pCustom->bplcon1, 0);
	agkCopMove(&sW, &g_pCustom->bpl1mod, HUD_ROW_BYTES - FETCH_BYTES);
	agkCopMove(&sW, &g_pCustom->bpl2mod, HUD_ROW_BYTES - FETCH_BYTES);
	for(UBYTE p = 0; p < 3; ++p) agkCopPtrs(&sW, p, (ULONG)s_pHud[ubBfr]->Planes[p]);
	agkCopMove(&sW, &g_pCustom->color[0], HUD_COLOR);
	cwPf2Palette(&sW, g_pMissionCloudPal[ubMission]);   // PF2 shows nothing until the clouds

	// After the HUD: the frame buffer, still 3 planes
	agkCopWait(&sW, PLAY_TOP - 1, SWITCH_WAIT_X);
	agkCopMove(&sW, &g_pCustom->bpl1mod, FB_ROW_BYTES - FETCH_BYTES);
	agkCopMove(&sW, &g_pCustom->bpl2mod, FB_ROW_BYTES - FETCH_BYTES);
	for(UBYTE p = 0; p < 3; ++p) agkCopPtrs(&sW, p, (ULONG)pFb->Planes[p] + ulFbRow);

	// The sky, line by line where the colour changes; the switch to dual playfield
	UWORD uwPrev = 0xFFFF;
	for(UWORD y = PLAY_TOP; y < SKY_END; ++y) {
		UWORD uwColor = s_pSky[ubMission][y - PLAY_TOP];
		if(uwColor != uwPrev) {
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
			s_sMtnSlots[ubBfr] = cwBand(&sW, MTN_Y, s_pMountains);
		}
	}
	// Mountains -> hills: a blank line in between, for the land palette
	agkCopWait(&sW, GAP_Y - 1, SWITCH_WAIT_X);
	cwPlanePtrs(&sW, 1, pBlank);
	agkCopMove(&sW, &g_pCustom->bpl2mod, (UWORD)-FETCH_BYTES);
	agkCopWait(&sW, GAP_Y, 0);
	cwPf2Palette(&sW, g_pMissionLandPal[ubMission]);
	s_sHillSlots[ubBfr] = cwBand(&sW, HILLS_Y, s_pHills);
	s_sGndSlots[ubBfr] = cwBand(&sW, GROUND_BAND_Y, s_pGround);

	s_uwCopUsed = sW.uwPos;
	agkCopFill(&sW, COP_RAW_COUNT);
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

static UWORD s_uwMtnX, s_uwHillX, s_uwGndX;

// The back list: this mission's colours (rewritten once when it changes),
// then each band's scroll for the camera.
__attribute__((noinline)) static void copperUpdate(UBYTE ubBfr) {
	static WORD pListCam[2] = {-1, -1};   // the camera each list's scroll is for
	tCopCmd *pList = s_pView->pCopList->pBackBfr->pList;
	WORD wCam = s_sGame.cam;
	if(s_pListMission[ubBfr] != s_sGame.mission) {
		s_pListMission[ubBfr] = s_sGame.mission;
		copperWriteList(pList, ubBfr, s_sGame.mission);
	}
	else if(wCam == pListCam[ubBfr]) return;   // a still camera: nothing to scroll (chip RAM writes are slow)
	pListCam[ubBfr] = wCam;
	bandSet(pList, &s_sCloudSlots[ubBfr], s_pCloudsBm, logicBandOffset(wCam, 3, ART_CLOUDS_LOOP_W));
	s_uwMtnX = logicBandOffset(wCam, 2, ART_MOUNTAINS_LOOP_W);
	s_uwHillX = logicBandOffset(wCam, 1, ART_HILLS_LOOP_W);
	s_uwGndX = logicBandOffset(wCam, 0, ART_GROUND_LOOP_W);
	bandSet(pList, &s_sMtnSlots[ubBfr], s_pMountains, s_uwMtnX);
	bandSet(pList, &s_sHillSlots[ubBfr], s_pHills, s_uwHillX);
	bandSet(pList, &s_sGndSlots[ubBfr], s_pGround, s_uwGndX);
}

// ----------------------------------------------------------- blit queue ---
// Each picture: erase what this buffer showed last time, then draw the
// objects back to front. The CPU feeds the blits to the blitter while it
// runs the game.

#define MSG_LINES 5
#define MSG_LEN 40
static tAgkText s_pMsg[2][MSG_LINES];
static const UBYTE s_pMsgY[MSG_LINES] = {96, 110, 132, 146, 168};
#define MSG_TYPE_PER_FRAME 2         // new letters a picture: text is slow, and it types itself
                                     // (3 made the title's worst frame 91% with the cloud band)
static char s_pMsgStage[2][MSG_LINES][MSG_LEN + 1];   // what each line is showing / about to show
static ULONG s_pMsgCover[2][MSG_LINES][2];  // columns (bits) this buffer's BOBs covered last time
static ULONG s_pMsgFix[2][MSG_LINES][2];    // columns an erase crossed: redraw their letters
static UBYTE s_pMsgAnyFix[2];
static UBYTE s_pMsgAnyCover[2];              // any bits in s_pMsgCover[buffer]
static UWORD s_pMsgKey[2] = {0xFFFF, 0xFFFF};   // what the lines say (state, blink...) once settled

#define TPL_MAX 48
static tAgkBlitArea s_pErase[2][DRAW_MAX];
static UBYTE s_pErased[2];
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
	bobAdd(BOB_LOGO, artLogoCreate(), artLogoCreateMask(), ART_LOGO_W, ART_LOGO_H, ART_LOGO_FRAMES);
	bobAdd(BOB_SHADOW, artShadowCreate(), artShadowCreateMask(), ART_SHADOW_W, ART_SHADOW_H, ART_SHADOW_FRAMES);
	UBYTE t = 0;
	for(UBYTE b = 0; b < BOB_TYPES; ++b) {
		s_pTplFirst[b] = t;
		for(UBYTE f = 0; f < g_pBobFrames[b] && t < TPL_MAX; ++f, ++t) {
			// Shots and shells are small and solid: a plain copy (half the bus)
			// Buildings are drawn first, onto an empty buffer: a plain copy too
			UBYTE isCopy = b == BOB_SHOT || b == BOB_SHELL || b == BOB_HANGAR ||
				b == BOB_FENCE || b == BOB_BARRACKS || b == BOB_FLAG || b == BOB_LOGO;
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
	// what last time's BOBs covered gets erased now: those letters need redrawing
	if(s_pMsgAnyCover[ubBfr]) {
		s_pMsgAnyCover[ubBfr] = 0;
		for(UBYTE l = 0; l < MSG_LINES; ++l) {
			s_pMsgFix[ubBfr][l][0] |= s_pMsgCover[ubBfr][l][0];
			s_pMsgFix[ubBfr][l][1] |= s_pMsgCover[ubBfr][l][1];
			s_pMsgAnyFix[ubBfr] |= (s_pMsgCover[ubBfr][l][0] | s_pMsgCover[ubBfr][l][1]) != 0;
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
		if(y < s_pMsgY[MSG_LINES - 1] + 8 && y + g_pBobH[b] > s_pMsgY[0]) {
			// (the erase covers whole words: from the word's start, + 16 px)
			WORD c0 = (WORD)(((x - FB_X0) & ~15) >> 3), c1 = (WORD)((x - FB_X0 + g_pBobW[b] + 15) >> 3);
			if(c0 < 0) c0 = 0;
			if(c1 > MSG_LEN - 1) c1 = MSG_LEN - 1;
			s_pMsgAnyCover[ubBfr] = 1;
			for(UBYTE l = 0; l < MSG_LINES; ++l) {
				if(y < s_pMsgY[l] + 8 && y + g_pBobH[b] > s_pMsgY[l]) {
					for(WORD c = c0; c <= c1; ++c) s_pMsgCover[ubBfr][l][c >> 5] |= 1UL << (c & 31);
				}
			}
		}
	}
	s_pErased[ubBfr] = ubDrawn;
}

// ------------------------------------------------------------------ HUD ---
// Text (agk/text.h): letters in colour 4, shadow in colour 1. The radar is a
// strip of the whole battlefield, 16 world px to a dot, drawn by the CPU.

#define HUD_LEN 40
#define HUD_TYPE_PER_FRAME 8
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
		agkTextInit(&s_pHudText2[b], &s_sFont, s_pHud[b], 16, RADAR_Y, 7, 4, 1);
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


__attribute__((noinline)) static void hudUpdate(UBYTE ubBfr) {
	static UBYTE pSaved[2] = {0xFF, 0xFF}, pAboard[2] = {0xFF, 0xFF}, pKilled[2] = {0xFF, 0xFF};
	static UBYTE pLives[2] = {0xFF, 0xFF}, pTitle[2] = {0xFF, 0xFF}, pSkill[2] = {0xFF, 0xFF};
	static ULONG pScore[2] = {~0UL, ~0UL};
	const tGame *g = &s_sGame;
	UBYTE isTitle = g->state == GS_TITLE;
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
		if(g->saved != pSaved[ubBfr]) { pSaved[ubBfr] = g->saved; agkTextNumber(&szLine[6], g->saved, 2); s_pHudPending[ubBfr] = 1; }
		if(g->aboard != pAboard[ubBfr]) { pAboard[ubBfr] = g->aboard; agkTextNumber(&szLine[16], g->aboard, 2); s_pHudPending[ubBfr] = 1; }
		if(g->killed != pKilled[ubBfr]) { pKilled[ubBfr] = g->killed; agkTextNumber(&szLine[24], g->killed, 2); s_pHudPending[ubBfr] = 1; }
	}
	if(ulScore != pScore[ubBfr]) { pScore[ubBfr] = ulScore; agkTextNumber(&szLine[33], ulScore, 6); s_pHudPending[ubBfr] = 1; }
	if(s_pHudPending[ubBfr]) {
		s_pHudPending[ubBfr] = !textType(&s_pHudText[ubBfr], s_pHudStage[ubBfr], szLine, HUD_TYPE_PER_FRAME);
	}
	if(g->lives != pLives[ubBfr]) {
		pLives[ubBfr] = g->lives;
		char szLine[] = "HELI 3";
		szLine[5] = (char)('0' + g->lives);
		agkTextSet(&s_pHudText2[ubBfr], szLine);
	}
	if(g->difficulty != pSkill[ubBfr]) {
		pSkill[ubBfr] = g->difficulty;
		char szSkill[] = "SKILL 3";
		szSkill[6] = (char)('0' + g->difficulty);
		agkTextSet(&s_pHudSkill[ubBfr], g->difficulty == DIFFICULTY_INVINCIBLE ? "GODMODE" : szSkill);
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
		WORD x = g->pJets[i].x;
		if(g->pJets[i].active && x >= 0 && x < WORLD_W) radarDot(pRow[1], logicRadarX(x), 5);
	}
	for(UBYTE i = 0; i < MINE_MAX; ++i) {
		WORD x = g->pMines[i].x;
		if(g->pMines[i].active && x >= 0 && x < WORLD_W) radarDot(pRow[1], logicRadarX(x), 3);
	}
	if(g->state != GS_TITLE) {
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

static void msgInit(void) {
	for(UBYTE b = 0; b < 2; ++b) {
		for(UBYTE l = 0; l < MSG_LINES; ++l) {
			agkTextInit(&s_pMsg[b][l], &s_sFont, s_pFb[b], FB_X0, s_pMsgY[l], MSG_LEN, 4, 1);
			for(UBYTE c = 0; c < MSG_LEN; ++c) s_pMsgStage[b][l][c] = ' ';
			s_pMsgStage[b][l][MSG_LEN] = 0;
		}
	}
}

static void msgCentre(char *szOut, const char *szText) {
	UBYTE ubLen = 0;
	while(szText[ubLen] && ubLen < MSG_LEN) ++ubLen;
	UBYTE ubPad = (UBYTE)((MSG_LEN - ubLen) >> 1);
	for(UBYTE i = 0; i < MSG_LEN; ++i) szOut[i] = ' ';
	for(UBYTE i = 0; i < ubLen; ++i) szOut[ubPad + i] = szText[i];
	szOut[MSG_LEN] = 0;
}

__attribute__((noinline)) static void msgUpdate(UBYTE ubBfr) {
	const tGame *g = &s_sGame;
	const char *pText[MSG_LINES] = {"", "", "", "", ""};
	char szA[24], szB[28];
	switch(g->state) {
		case GS_TITLE:
			pText[0] = "BY PAUL ROSS SEIBERT";
			pText[1] = "@ 2026 DESERT PALMS SOFTWARE";
			pText[2] = "FLY IN. BLOW THE BARRACKS OPEN.";
			pText[3] = "LAND. BRING THEM HOME.";
			pText[4] = (g->frame & 32) ? "PRESS FIRE" : "";
			break;
		case GS_INTRO: {
			const char *szM = "MISSION 1";
			for(UBYTE i = 0; i < 10; ++i) szA[i] = szM[i];
			szA[8] = (char)('1' + g->mission);
			pText[1] = szA;
			pText[2] = g_pMissionName[g->mission];
			pText[3] = "64 HOSTAGES ARE WAITING";
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
	UWORD uwKey = (UWORD)(g->state << 8 | g->mission << 4 |
		(g->state == GS_TITLE ? (g->frame >> 5) & 1 : 0) |
		(g->state == GS_LOST && g->stateTimer < LOST_FRAMES - 40 ? 2 : 0));
	if(uwKey == s_pMsgKey[ubBfr] && !s_pMsgAnyFix[ubBfr]) return;
	s_pMsgAnyFix[ubBfr] = 0;
	char szLine[MSG_LEN + 1];
	UBYTE ubBudget = MSG_TYPE_PER_FRAME;
	for(UBYTE l = 0; l < MSG_LINES; ++l) {
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
		if(isChange) agkTextSet(&s_pMsg[ubBfr][l], pStage);
	}
	// all typed: settled until the key changes
	s_pMsgKey[ubBfr] = ubBudget ? uwKey : 0xFFFF;
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
	g_pCustom->color[31] = isStars ? logicStarTwinkle(g) : pPal[2];   // (twinkles)
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
	g_pCustom->color[29] = pPal[0];
	g_pCustom->color[30] = pPal[1];
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
// Paula channel 2: a looped chop (two blade passes of filtered noise) made
// at startup; the period follows the airspeed, the volume the state.

#define ROTOR_CHANNEL 2
#define ROTOR_SAMPLES 1200
static BYTE *s_pRotorWave;   // chip RAM

static void rotorCreate(void) {
	s_pRotorWave = memAllocChip(ROTOR_SAMPLES);
	UWORD uwLfsr = 0x1D2B;
	WORD wLow = 0, wLow2 = 0;
	for(UWORD i = 0; i < ROTOR_SAMPLES; ++i) {
		UWORD lsb = uwLfsr & 1;
		uwLfsr >>= 1;
		if(lsb) uwLfsr ^= 0xB400;
		WORD wNoise = (WORD)((uwLfsr & 0xFF) - 128);
		// two low-passes: a thump, not a hiss
		wLow = (WORD)(wLow + ((wNoise - wLow) >> 2));
		wLow2 = (WORD)(wLow2 + ((wLow - wLow2) >> 2));
		// envelope: a blade pass every half loop, decaying
		UWORD uwPhase = i < ROTOR_SAMPLES / 2 ? i : (UWORD)(i - ROTOR_SAMPLES / 2);
		WORD wEnv = uwPhase < 40 ? (WORD)(uwPhase * 3) : (WORD)(120 - ((uwPhase - 40) >> 2));
		if(wEnv < 8) wEnv = 8;
		WORD v = (WORD)((wLow2 * wEnv) >> 5);
		s_pRotorWave[i] = (BYTE)(v > 127 ? 127 : v < -128 ? -128 : v);
	}
	ptplayerSetChannelsForPlayer(0xF & ~(1 << ROTOR_CHANNEL));
	volatile struct AudChannel *pAud = &g_pCustom->aud[ROTOR_CHANNEL];
	pAud->ac_ptr = (UWORD *)s_pRotorWave;
	pAud->ac_len = ROTOR_SAMPLES / 2;
	pAud->ac_vol = 0;
	pAud->ac_per = 400;
	g_pCustom->dmacon = DMAF_SETCLR | (DMAF_AUD0 << ROTOR_CHANNEL);
}

__attribute__((noinline)) static void rotorUpdate(void) {
	const tGame *g = &s_sGame;
	const tChopper *c = &g->chop;
	UBYTE ubVol = 0;
	UWORD uwPer = 300;
	if(c->alive && (g->state == GS_PLAY || g->state == GS_INTRO)) {
		WORD wSpeed = c->vx < 0 ? (WORD)-c->vx : c->vx;
		uwPer = (UWORD)(300 - wSpeed - (c->landed ? 0 : 24));
		ubVol = c->landed ? (g->state == GS_INTRO ? 14 : 20) : 30;
	}
	if(s_isPaused) ubVol = 0;
	volatile struct AudChannel *pAud = &g_pCustom->aud[ROTOR_CHANNEL];
	pAud->ac_per = uwPer;
	pAud->ac_vol = ubVol;
}

static void rotorDestroy(void) {
	g_pCustom->dmacon = DMAF_AUD0 << ROTOR_CHANNEL;
	g_pCustom->aud[ROTOR_CHANNEL].ac_vol = 0;
	memFree(s_pRotorWave, ROTOR_SAMPLES);
}

// ---------------------------------------------------------------- sound ---

static void musicFor(UBYTE ubState) {
	UBYTE ubSong = 0xFF;
	if(ubState == GS_TITLE || ubState == GS_INTRO) ubSong = SOUND_MUSIC_THEME;
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
		if(ubStateBefore == GS_TITLE) soundPlay(SOUND_SFX_START);
	}
	if(e & EV_CRASH) soundPlay(SOUND_SFX_CRASH);
	else if(e & EV_OPEN) soundPlay(SOUND_SFX_OPEN);
	else if(e & EV_BOOM) soundPlay(SOUND_SFX_BOOM);
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
	keyCreate();
	joyOpen();

	s_pView = viewCreate(0,
		TAG_VIEW_GLOBAL_PALETTE, 1,
		TAG_VIEW_COPLIST_MODE, VIEW_COPLIST_MODE_RAW,
		TAG_VIEW_COPLIST_RAW_COUNT, COP_RAW_COUNT,
		TAG_DONE);
	s_pVPort = vPortCreate(0, TAG_VPORT_VIEW, s_pView, TAG_VPORT_BPP, 6, TAG_DONE);

	for(UBYTE b = 0; b < 2; ++b) s_pFb[b] = bitmapCreate(FB_W, FB_H, FB_BPP, BMF_CLEAR | BMF_INTERLEAVED);
	for(UBYTE b = 0; b < 2; ++b) s_pHud[b] = bitmapCreate(HUD_W, PLAY_TOP, FB_BPP, BMF_CLEAR | BMF_INTERLEAVED);
	s_pBlankRow = memAllocChipClear(FETCH_BYTES);
	s_pCloudsBm = artCloudsCreate();
	s_pMountains = artMountainsCreate();
	s_pHills = artHillsCreate();
	s_pGround = artGroundCreate();
	bobsCreate();
	hudInit();
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

	for(UBYTE m = 0; m < MISSION_COUNT; ++m)
		for(UWORD y = 0; y < SKY_END - PLAY_TOP; ++y) s_pSky[m][y] = logicSkyColor(m, (UBYTE)y);
	tCopList *pCopList = s_pView->pCopList;
	s_pCopBfrA = pCopList->pBackBfr;
	copperWriteList(pCopList->pBackBfr->pList, 0, 0);
	copperWriteList(pCopList->pFrontBfr->pList, 1, 0);
	s_pListMission[0] = s_pListMission[1] = 0;

	soundCreate();
	rotorCreate();
	musicFor(s_sGame.state);

	viewLoad(s_pView);
	systemUnuse();
	splashShow();
	agkDebugAsync(1);
}

// ------------------------------------------------------------ the frame ---

__attribute__((noinline)) static void reportState(UBYTE ubStateBefore) {
	const tGame *g = &s_sGame;
	const tChopper *c = &g->chop;
	if(g->evBarracks) {
		agkState("barracks open", g->evBarracks - 1);
		agkEnd();
	}
	if(g->events & EV_CRASH) agkPrint("AGK chopper lost\n");
	if(g->events & EV_SHRUG) agkPrint("AGK hit ignored (invincible)\n");
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
		agkEnd();
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
		return;
	}
	if(s_ubAfterSplash < 3 && ++s_ubAfterSplash == 2) {
		// the game's copper list is showing now: its colours back, the logo freed
		for(UBYTE i = 1; i < 32; ++i) g_pCustom->color[i] = s_pVPort->pPalette[i];
		// The splash's 47 KB stay allocated until the game quits (genericDestroy):
		// ACE's memFree switches the OS back on for each free, which froze the
		// title for ~6 frames right as it appeared. Chip RAM has room for it.
	}
	UBYTE ubBfr = backBuffer();
	// the HUD's text first, while the blitter's idle: chip RAM writes are slow beside it
	hudUpdate(ubBfr);
	erasesQueue(ubBfr);
	radarClear(ubBfr);
	UBYTE ubStateBefore = s_sGame.state;
	if(!s_isPaused) {
		logicUpdate(&s_sGame, &sIn);
		playSounds(ubStateBefore);
	}
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
	msgUpdate(ubBfr);
	copProcessBlocks();
	agkPerfEnd();
	while(timerGet() - s_ulFrameVbl < 1) continue;

	if(s_sGame.frame == 2) {
		agkState("copper_used", s_uwCopUsed);
		agkState("copper_max", COP_RAW_COUNT);
		agkEnd();
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
	bitmapDestroy(s_pCloudsBm);
	memFree(s_pBlankRow, FETCH_BYTES);
	for(UBYTE b = 0; b < 2; ++b) {
		bitmapDestroy(s_pHud[b]);
		bitmapDestroy(s_pFb[b]);
	}
	viewDestroy(s_pView);
	joyClose();
	keyDestroy();
}
