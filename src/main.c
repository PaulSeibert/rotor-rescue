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
#include <proto/exec.h>   // (AllocMem: the chip squeeze test hook)
#include <ace/utils/custom.h>
#include <ace/utils/disk_file.h>
#include <ace/utils/extview.h>
#include <ace/utils/file.h>
#include <hardware/dmabits.h>
#include <stddef.h>
#include <agk/asm.h>
#include <agk/blitq.h>
#include <agk/copw.h>
// The copper writer's entries as whole longs, the same bits as ACE's copSetWait /
// copSetMove: those fill bitfields, a read-modify-write of chip RAM per field, and a
// whole game list took ~1.6 A500 frames (measured: the M5 -> M6 card's rewrite
// picture 277 %, 2 pictures dropped; every mission start's two rewrites, world 1's too)
static inline void cwFastWait(tAgkCopWriter *pW, UWORD uwGameY, UBYTE ubX) {
	UWORD uwBeamY = (UWORD)(pW->uwBeamTop + uwGameY);
	if(uwBeamY > 0xFF && !pW->isPast255) {
		pW->pList[pW->uwPos++].ulCode = 0xFFDFFFFEUL;   // the end of line 255 first
		pW->isPast255 = 1;
	}
	pW->pList[pW->uwPos++].ulCode = ((ULONG)(((uwBeamY & 0xFF) << 8) | (ubX & 0xFE) | 1) << 16) | 0xFFFE;
}
static inline void cwFastMove(tAgkCopWriter *pW, volatile void *pReg, UWORD uwValue) {
	pW->pList[pW->uwPos++].ulCode = ((ULONG)((ULONG)pReg - (ULONG)g_pCustom) << 16) | uwValue;
}
static inline void cwFastPtrs(tAgkCopWriter *pW, UBYTE ubPlane, ULONG ulAddr) {
	cwFastMove(pW, &g_pBplFetch[ubPlane].uwHi, (UWORD)(ulAddr >> 16));
	cwFastMove(pW, &g_pBplFetch[ubPlane].uwLo, (UWORD)(ulAddr & 0xFFFF));
}
#define agkCopWait cwFastWait
#define agkCopMove cwFastMove
#define agkCopPtrs cwFastPtrs
#include <agk/debug.h>
#include <agk/pack.h>
#include <agk/perf.h>
#include <agk/text.h>
#include "art.h"        // generated from art/ by agk (agk help-art)
#include "disk_art.h"   // art/tools/make_disk_art.py: the art on the floppy
#include "world2_art.h" // art/tools/make_world_disk.py: world 2's display art and palettes
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
#define GAP_Y 196                            // the title: one blank line, the land palette loads
#define HILLS_Y (GAP_Y + 1)                  // 197 (the title)
#define GROUND_BAND_Y (HILLS_Y + ART_HILLS_H) // 224 .. 255
#define SKY_END GAP_Y
// In play (SPEC 8, Paul 2026-11-12): the mountains band runs on into the
// plain's far strip (lines 120-179, one block at camera / 4), then the
// perspective floor (FLOOR_Y .. FLOOR_END - 1, a speed per line, logic.h), then
// the hills from their row 2 (rows 0-1 are empty) at line 199
#define FLOOR_END (FLOOR_Y + FLOOR_LINES)    // 199
#define PLAY_HILLS_ROW (FLOOR_END - HILLS_Y)  // 2
_Static_assert(MTN_Y + ART_MOUNTAINS_H == FLOOR_Y, "the mountains band (with the far strip) ends where the floor starts");
// T3: logic.c's band table (logicBands) is these constants for worlds 1 and 2 (the copper builder reads the table in play)
_Static_assert(CLOUDS_Y == 72 && ART_CLOUDS_H == 40 && MTN_Y == 120 && ART_MOUNTAINS_H == 60 && FLOOR_Y == 180 && FLOOR_LINES == 19 &&
	FLOOR_END == 199 && GROUND_BAND_Y == 224 && ART_GROUND_H == 32 && ART_HILLS_H - PLAY_HILLS_ROW == 25, "logic.c's band table, world 1");
// The floor's art: disk/plainfloor.bin (art/tools/make_plain.py), 640 px rows,
// 3 interleaved planes, loaded during the splash (titleLoad) like the logo:
// in the program it would push AROS's first picture past the boot window
#define PLAIN_FLOOR_W 640
#define PLAIN_FLOOR_BYTES ((ULONG)(PLAIN_FLOOR_W / 8) * 3 * FLOOR_LINES)
_Static_assert(PLAY_HILLS_ROW == 2 && FLOOR_END + ART_HILLS_H - PLAY_HILLS_ROW == GROUND_BAND_Y, "the hills in play: rows 2-26, lines 199-223");
// The floor's copper, per line at its start: WAIT, COLOR00 (the plain's ground;
// 4 MOVEs in 24 bits on AGA), BPLCON1, BPL2MOD. Line 179: the land palette at
// its start (a WAIT and 7 MOVEs), PF2's pointers to floor row 0 at its end (a
// WAIT and 6). Against the old gap line (WAIT, blank pointers, modulo, WAIT, palette: 16):
#define FLOOR_COP_OCS (2 + 6 + 7 + FLOOR_LINES * 4 - 16)
#define FLOOR_COP_AGA (2 + 6 + 7 + FLOOR_LINES * 7 - 16 - 5 * (SKY_END - FLOOR_Y))
// World 2 against that (cwSea): SEA_LINES blocks instead of FLOOR_LINES, the
// island palette's 7 MOVEs after line 207's block, no hills band (a WAIT and 8
// MOVEs); the strip's switch replaces the ground band's
#define W2_COP_OCS (FLOOR_COP_OCS + (SEA_LINES - FLOOR_LINES) * 4 + 7 - 9)
#define W2_COP_AGA_MORE ((SEA_LINES - FLOOR_LINES) * 7 + 7 - 9)

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
#define COP_RAW_COUNT_AGA (COP_RAW_COUNT + AGA_SKY_MOVES + (SKY_END - PLAY_TOP) + 6 + FLOOR_COP_AGA)
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
// The plain's floor (SPEC 8): its art, the hills from their row 2, the ground
// colour per mission and line, and where each list's floor MOVEs are
static tBitMap *s_pPlainFloor;
static tBitMap s_sHillsPlay;        // s_pHills from row PLAY_HILLS_ROW (its planes moved down)
static UWORD s_pFloorBg[MISSION_COUNT][FLOOR_LINES];   // COLOR00 (AGA: the high nibbles)
static UWORD s_pFloorBgLo[MISSION_COUNT][FLOOR_LINES]; // AGA: the low nibbles
static UWORD s_pFloorPtr[2];        // per buffer: floor row 0's pointer MOVEs (3 pairs)
static UWORD s_pFloorCon1[2];       // per buffer: line 0's BPLCON1 MOVE (its BPL2MOD follows; a line a stride)
static UBYTE s_ubFloorStride = 4;   // copper entries a floor line (7 on AGA)

// World 2, Coral Islands (missions 4-6; SPEC 11, docs/STYLE-world2.md section 2).
// The same lines as world 1 down to 179 (sky, clouds at 72, blank PF2 rows while
// the far palette loads at 113, the far islands and their strip at 120-179 as
// one band), then the sea: SEA_LINES perspective lines (180-207, a block each as
// the plain's), and from 208 the terrain strip, 1:1, a ring of 16 px tile
// columns. Palette loads: the clouds' at the top, the far islands' at 113, the
// sea's at the start of 179 (the far strip's bare last row), the islands' after
// line 207's block (the sea's bare last row), PF2's pointers at $D8 of 179 and
// 207. Its art (disk/world2.bin) loads at the mission card into the world
// arena, the band region of the floppy art's chip block: world 1's clouds,
// mountains, title mountains, hills and ground, loaded back (disk/world1.bin)
// when the game returns to world 1. The strip's ring is a chip block of its own
// while the game is in world 2.
// Chip RAM the game can't run without, and none left (a machine with less of it than the tests have: WinUAE's A500
// has about 23 KB less free than vAmiga's at the same point): say so on the serial log and stop, rather than
// read and write through a null pointer (on a 68000 that is the exception vectors). Every chip allocation below is
// checked; the optional ones degrade instead (the songs, the rotor's wave).
static void chipFatal(const char *szWhat) {
	agkPrint("AGK ERR out of chip RAM: ");
	agkPrint(szWhat);
	agkPrint("\n");
	agkDebugFlush();
	for(;;) continue;
}
#define CHIP_CHECK(p, szWhat) do { if(!(p)) chipFatal(szWhat); } while(0)

#define ARENA_OFFS DISK_ART_CLOUDS_OFFS
#define ARENA_BYTES ((ULONG)(DISK_ART_FENCE_OFFS - DISK_ART_CLOUDS_OFFS))
// World 2's BOBs (its "set", after its display art in disk/world2.bin) take the rest of the arena
// and world 1's own BOBs' chip (they follow the bands: world 1's file, disk/world1.bin, brings
// them back) and, where that is not enough, WORLD2_EXTRA bytes past the art block's end
_Static_assert(WORLD1_BANDS_BYTES == DISK_ART_BYTES - ARENA_OFFS, "disk/world1.bin is the art block from its bands on");
_Static_assert(WORLD2_DISPLAY_BYTES <= ARENA_BYTES, "world 2's display art fits the world arena");
#define WORLD2_EXTRA ((ULONG)(WORLD2_BYTES > WORLD1_BANDS_BYTES ? WORLD2_BYTES - WORLD1_BANDS_BYTES : 0))
#define DISK_ART_ALLOC (DISK_ART_BYTES + WORLD2_EXTRA + WORLD2_PAD_COMPOSED_BYTES)
_Static_assert(SEA_Y == FLOOR_Y && STRIP_Y == 208 && ART_W2_SEA_H == SEA_LINES, "STYLE-world2's lines");
_Static_assert(ART_W2_CLOUDS_H == ART_CLOUDS_H && ART_W2_FAR_H == ART_MOUNTAINS_H &&
	ART_W2_CLOUDS_LOOP_W == ART_CLOUDS_LOOP_W && ART_W2_FAR_LOOP_W == ART_MOUNTAINS_LOOP_W,
	"world 2's clouds and far band have world 1's lines and loops");
// The strip: pointer creep, no copper wrap (docs/spikes/terrain-height.md): map
// column j, row r at plane + 2 j + r * STRIP_RB; scrolling moves the pointers
// only. STRIP_SPAN columns are kept drawn, the 21 fetched words of both
// pictures; a column drawn overwrites the one STRIP_SPAN to its left (a row
// down), which neither picture shows. Separate planes (a creeping pointer must
// not run into the next plane's row).
#define STRIP_RB 44                          // 22 words: 21 fetched + 1
#define STRIP_SPAN 22
#define STRIP_PLANE_BYTES ((ULONG)STRIP_RB * (STRIP_ROWS + 1) + W2_MAP_COLS * 2 + 2)
#define STRIP_FILL_PER_FRAME 4               // after a jump: 12 blits a picture, behind a blank strip
#define SEA_PLANE_BYTES (ART_W2_SEA_BITMAP_W / 8)
#define STRIP3_ROWS W3_STRIP_ROWS             // 126: lines 130-255
#define STRIP3_PLANE_BYTES ((ULONG)STRIP_RB * (STRIP3_ROWS + 1) + W3_MAP_COLS * 2 + 2)
#define W3_SKY_LINES (130 - PLAY_TOP)         // 106: COLOR00 lines 24-129
#define W3_HAZE_LINES STRIP3_ROWS             // 126: lines 130-255
#define W3_GRAD_WORDS (2 * (W3_SKY_LINES + W3_HAZE_LINES))
#define WORLD3_GRAD_OFFS ((WORLD3_BYTES + 7UL) & ~7UL)
#define WORLD3_RING_OFFS (WORLD3_GRAD_OFFS + 3UL * W3_GRAD_WORDS * 2)
#define WORLD3_END (WORLD3_RING_OFFS + 3UL * STRIP3_PLANE_BYTES)
_Static_assert(WORLD3_END <= WORLD1_BANDS_BYTES + WORLD2_EXTRA + WORLD2_PAD_COMPOSED_BYTES, "world 3's file, gradients and ring fit the arena world 2 sized");
#define W3_COP_OCS 640    // measured: 631 entries used (a500), world 2's list (661) is the bigger one
#define W3_COP_AGA 1340   // measured: 1333 used (a1200)
static UBYTE s_ubWorld;             // the world whose art is in the arena and the game lists
static UBYTE s_ubRasterOff;         // after a world's load: pictures until the bitplanes are back on
static tBitMap s_sW2Clouds, s_sW2Far, s_sW2Sea, s_sStrip;
// World 3 (the Highlands, SPEC 13; docs/STYLE-world3.md; art/tools/make_world3_disk.py). disk/world3.bin loads into the same arena as
// world 2's: the far range (640 x 42), the tile bank and rock fills, the column plan (3 bytes a column: tile, fill, the tile's top row
// in the ring); past the file come the gradients (sky and haze, per mission, made at the load) and the strip's ring. World 3 so
// replaces world 2's art, BOBs and ring (a chip block of its own) and adds no chip to the game.
static tBitMap s_sW3Far;
static const UBYTE *s_pW3Tiles, *s_pW3Fills, *s_pW3Plan, *s_pW3TileTab;   // (in the arena)
static const UWORD s_pW3GradOffs[3] = {0, W3_GRAD_WORDS, 2 * W3_GRAD_WORDS};   // (a mission's gradients: no multiply)
static UWORD *s_pW3Grad;            // (in the arena) per mission W3_GRAD_WORDS: sky, sky low nibbles, haze, haze low nibbles
static ULONG s_ulStripPlane = STRIP_PLANE_BYTES;   // the ring's plane bytes in the world on screen
static UBYTE s_isRingAlloc;         // s_pStripMem is a chip block of its own (world 2's); world 3's ring is in the arena
static UBYTE s_ubStripFillRate = STRIP_FILL_PER_FRAME;   // columns of the fill a picture (world 3's are 9 blits, world 2's 3)
static UWORD s_uwHazeSlot[2], s_uwHazeStride = 2;        // per list: line 0's COLOR00 MOVE of the haze; entries a line
static tBitMap *s_pFarBm;           // PF2 at line 120 in play: world 1's mountains, world 2's far islands
static tBitMap *s_pW1CloudsBm;      // world 1's clouds (s_pCloudsBm is the world's)
static const UBYTE *s_pW2Tiles, *s_pW2Map;   // (in the arena)
static UBYTE *s_pStripMem;          // (chip) the strip's three planes, in world 2
static UWORD s_pW2Sky[MISSION_COUNT][FLOOR_Y - PLAY_TOP];     // COLOR00 lines 24-179: top to H (AGA: high nibbles)
static UWORD s_pW2SkyLo[MISSION_COUNT][FLOOR_Y - PLAY_TOP];   // AGA: the low nibbles
static UWORD s_pW2SeaBg[MISSION_COUNT][SEA_LINES];            // lines 180-207: H to C
static UWORD s_pW2SeaBgLo[MISSION_COUNT][SEA_LINES];
static UWORD s_pSkySlot[2][FLOOR_Y - PLAY_TOP];   // per game list, both worlds: each sky line's COLOR00 MOVE (copperMission)
// The animated sea (SPEC 11, "Sea animation"), per list: the swell phase its lines show
// and the 8-frame step its pens 4-7 show (0xFF / 0xFFFF: unknown: the list was just written
// or patched with the mission's own palette, so the next picture writes them again)
static UBYTE s_pSeaPhase[2] = {0xFF, 0xFF};
static UWORD s_pSeaBase[SEA_LINES];               // each sea line's undrifted left edge at s_wSeaBaseCam (logicSeaScroll's, for the swell alone)
static WORD s_wSeaBaseCam = -1;
#define SEA_BLIT_MAX 14                            // the swell alone waits for a picture of at most this many queued blits (hover: 11-13, a fight 19-23)
static UWORD s_pSeaPenCol[MISSION_COUNT][4][3];   // the sea's pens 4-7 per mission and level (world2Gradients)
static UBYTE s_ubSeaHold;                         // pictures left that hold the swell and shimmer at frame 0 after a list is written or patched
static UWORD s_pSeaPenStep[2] = {0xFFFF, 0xFFFF};
static UWORD s_pPalSlot[2][4];   // its PF2 palettes: clouds, far band (mountains, far islands), land or sea, islands

typedef struct {
	UWORD uwCon1;          // index of the MOVE BPLCON1
	UWORD pPtr[3];         // per plane: index of its pointer's high-word MOVE
} tCopSlots;
// per list ([title][buffer]): the sky's length varies
static tCopSlots s_sCloudSlots[2], s_sMtnSlots[2][2], s_sHillSlots[2][2], s_sGndSlots[2][2];
static tCopSlots s_sStripSlots[2];  // world 2's terrain strip, per buffer (its BPL2MOD MOVE follows uwCon1)
static tBitMap *s_pCloudsBm;        // the far clouds band (PF2, camera / 8)
static UBYTE *s_pTitleLogo;         // chip RAM: the logo (from the floppy)
static UBYTE s_isTitlePending, s_isTitleLogo;   // to load; loaded
static UBYTE s_ubSongsPending;      // songs on the floppy still to load (songsLoad), one a frame
static UBYTE s_isArtPending;        // the art on the floppy to load (diskArtLoad)
static UBYTE *s_pDiskArt;           // (chip) that art: disk/diskart.bin (diskArtCreate)
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
// The vertical blank count read just after a picture's copper list went to
// COP1LC: the copper takes it at the next blank, so until then the back list
// (and frame buffer) is still the one on screen (review A3)
static ULONG s_ulSwapVbl = ~0UL;
static UBYTE s_ubOverrun;        // Ctrl+L, O: pictures left that run over their frame (tests)
static UBYTE s_isMusicOn = 1, s_ubSong = 0xFF;
static UBYTE s_isPaused;
static UBYTE s_isSplash;         // the logo's showing
static UBYTE s_ubAfterSplash;    // frames since it ended (its memory goes after 2)
static UBYTE s_isReady;
static UBYTE s_ubSkillKeyWait;  // frames left to press 0-5 after Ctrl+L
static UBYTE s_ubScrollTrace;   // Ctrl+L, T: frames left to print the far bands' BPLCON1 (tests)
static UBYTE s_ubSeaStill;       // Ctrl+L, S: the sea's swell and shimmer held at frame 0 (tests/world2.agk measures the camera's scroll)
static UWORD s_uwMtnCon1, s_uwHillCon1, s_uwMtnX4;   // what copperUpdate wrote last
static UBYTE s_ubDiskStatus;    // HI_DISK_*: the hall of fame on the floppy (shown under it)

// The table from the floppy (HI_DISK_LOADED, or HI_DISK_DEFAULT: the defaults stay)
static UBYTE scoresLoad(void) {
	UBYTE ubStatus = hiDiskLoad(&s_sGame.hi, &g_sHiConfig);
	s_sGame.hiScore = hiTop(&s_sGame.hi);
	agkPrint(ubStatus == HI_DISK_LOADED ? "AGK scores loaded\n" : "AGK scores default\n");
	return ubStatus;
}

static void scoresReport(void) {
	// "AGK scores names=DPS/PRS/..." best first, then the top and last scores
	char szLine[] = "AGK scores names=AAA/AAA/AAA/AAA/AAA\n";
	for(UBYTE i = 0; i < HI_COUNT; ++i)
		for(UBYTE c = 0; c < HI_NAME; ++c) szLine[17 + 4 * i + c] = s_sGame.hi.pEntries[i].name[c];
	agkPrint(szLine);
	agkState("top", (LONG)s_sGame.hi.pEntries[0].score);
	agkState("last", (LONG)s_sGame.hi.pEntries[HI_COUNT - 1].score);
	agkEnd();
}
static const UBYTE s_pDigitKeys[DIFFICULTY_MAX + 1] = {KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5};

// Ctrl+L, then 0-5 within 2 s: the difficulty (0 = invincible, for testing)
// Test hook (tests/chipwinuae.agk, Ctrl+L then Q): takes CHIP_SQUEEZE_KB of chip RAM away for the rest of the run, as
// WinUAE's A500 has about that much less free than vAmiga's at the same point (Rotor Raider's measurement: 111 KB
// against 134 KB). Ctrl+L then E takes four times as much: how far the margin goes before an allocation fails.
// (Keys on the splash are not seen by the harness: the floppy loads run under it. Both are pressed at the title.)
#define CHIP_SQUEEZE_KB 23
static void chipSqueeze(ULONG ulKb) {
	// exec's AllocMem straight (a Forbid and a Permit): ACE's memAllocChip wakes the OS up
	if(AllocMem(ulKb << 10, MEMF_CHIP)) {
		agkPrint("AGK chip squeezed\n");
		agkMemReport();
	}
	else agkPrint("AGK chip squeeze failed\n");
}

__attribute__((noinline, optimize("Os"))) static void skillKeys(void) {
	static UBYTE s_ubWorldKeyWait;   // Ctrl+L, W: frames left to press 1-6 (a mission, testing)
	if(keyCheck(KEY_CONTROL) && keyUse(KEY_L)) s_ubSkillKeyWait = 100;
	if(s_ubWorldKeyWait) {
		--s_ubWorldKeyWait;
		static const UBYTE pMissionKeys[9] = {KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9};   // (7-9: world 3, the pilot)
		for(UBYTE m = 0; m < 9; ++m) {
			if(keyUse(pMissionKeys[m])) {
				s_ubWorldKeyWait = 0;
				logicStartAt(&s_sGame, (UBYTE)(m / MISSION_COUNT), (UBYTE)(m % MISSION_COUNT));
				agkState("start world", m / MISSION_COUNT + 1);
				agkState("mission", m + 1);
				agkEnd();
				break;
			}
		}
		return;
	}
	if(!s_ubSkillKeyWait) return;
	--s_ubSkillKeyWait;
	if(keyUse(KEY_W)) {
		s_ubWorldKeyWait = 100;
		s_ubSkillKeyWait = 0;
		return;
	}
	if(keyUse(KEY_T)) {
		s_ubScrollTrace = 12;
		s_ubSkillKeyWait = 0;
		return;
	}
	if(keyUse(KEY_S)) {
		s_ubSeaStill = !s_ubSeaStill;
		s_ubSkillKeyWait = 0;
		agkState("sea still", s_ubSeaStill);
		agkEnd();
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
	if(keyUse(KEY_B)) {
		// three equal-priority effects in one picture: the hangar drop-off's worst case (tests/hqsound.agk)
		for(UBYTE i = 0; i < 3; ++i) soundPlay(SOUND_SFX_SAVED);
		s_ubSkillKeyWait = 0;
		return;
	}
	if(keyUse(KEY_Q)) {
		chipSqueeze(CHIP_SQUEEZE_KB);
		s_ubSkillKeyWait = 0;
		return;
	}
	if(keyUse(KEY_E)) {
		chipSqueeze(4 * CHIP_SQUEEZE_KB);
		s_ubSkillKeyWait = 0;
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
	if(keyUse(KEY_O)) {
		// the next pictures run ~1.2 frames each (a test of the swap: tests/tear.agk)
		s_ubOverrun = 8;
		s_ubSkillKeyWait = 0;
		agkPrint("AGK overrun\n");
		return;
	}
	if(keyUse(KEY_K)) {
		// everyone rescued at once: the mission's clear and the next card (tests/w2clear.agk)
		s_sGame.saved = HOSTAGE_COUNT;
		s_sGame.killed = 0;
		s_sGame.aboard = 0;
		s_sGame.deckLeft = 0;
		s_ubSkillKeyWait = 0;
		return;
	}
	if(keyUse(KEY_Z)) {
		// the freighter 3 s from sinking (tests/w2frtr.agk)
		if(s_sGame.deckOn && s_sGame.sinkTotal) s_sGame.sinkTimer = (UWORD)(s_sGame.sinkTotal - 150);
		s_ubSkillKeyWait = 0;
		return;
	}
	if(keyUse(KEY_D)) {
		// to the strait destroyer (tests/w2dest.agk, the playtest list): low over the water 94 px east of its aft turret, in the air, facing left
		if(s_sGame.destOn) {
			s_sGame.chop.x = 2800; s_sGame.chop.y = 228; s_sGame.chop.vx = s_sGame.chop.vy = 0; s_sGame.chop.landed = 0;
			s_sGame.landFrames = 0; s_sGame.airGrace = 0;
			s_sGame.chop.face = s_sGame.chop.faceTarget = FACE_LEFT;
			logicCameraSnap(&s_sGame);
		}
		s_ubSkillKeyWait = 0;
		return;
	}
	if(keyUse(KEY_B)) {
		// the destroyer's clock 2 s from its barrage; Ctrl+L, Y: 2 s from the turrets opening (tests/w2dest.agk)
		if(s_sGame.destOn && s_sGame.destClock < s_sGame.barNext - 100) s_sGame.destClock = (UWORD)(s_sGame.barNext - 100);
		s_ubSkillKeyWait = 0;
		agkState("dest clock", s_sGame.destClock);
		agkEnd();
		return;
	}
	if(keyUse(KEY_Y)) {
		if(s_sGame.destOn && s_sGame.destClock < DEST_OPEN_AT - 100) s_sGame.destClock = DEST_OPEN_AT - 100;
		s_ubSkillKeyWait = 0;
		agkState("dest clock", s_sGame.destClock);
		agkEnd();
		return;
	}
	if(keyUse(KEY_R)) {
		// every radar site down at once: the zone bonus and its HUD line (tests/w2dest.agk)
		logicKillRadars(&s_sGame);
		if(s_sGame.events3 & EV4_ZONE) agkPrint("AGK zone clear\n");   // (the next logicUpdate clears the events before the usual report)
		s_ubSkillKeyWait = 0;
		return;
	}
	if(keyUse(KEY_H)) {
		// the hall of fame read from the disk again, as at boot (tests)
		s_ubSkillKeyWait = 0;
		scoresLoad();
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
#define REG_BPLCON1 0x102
#define REG_BPL2MOD 0x10A
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

// The plain's perspective floor in play (SPEC 8, logic.h): after the far
// strip's last line PF2 goes to floor row 0 and the land palette loads (row 0 is
// empty: the palette's MOVEs, which run into line FLOOR_Y, can't show). Then a
// block at the start of every floor line, before its fetch: COLOR00 (the
// plain's ground), BPLCON1 (this line's fine scroll), BPL2MOD (added at the end
// of this line's fetch: where the next row starts, so all three PF2 planes
// move with one MOVE). floorSet() fills in the scroll each frame. Then the
// hills, from their row 2.
__attribute__((noinline, optimize("Os"))) static void cwFloor(tAgkCopWriter *pW, UBYTE ubBfr, UBYTE ubMission, const tBandLayout *pLay) {
	const UWORD uwFloorY = pLay->b[BK_FLOOR].y, uwLines = pLay->b[BK_FLOOR].rows;
	// (raw MOVEs by register offset: small code, the A500's slow RAM is full)
	// Line FLOOR_Y - 1, the far strip's last row (bare): the land palette at its
	// start, before its fetch (after the sky's COLOR00 there). At the end of the
	// line with the pointers its 7 MOVEs ran into line FLOOR_Y and pushed that
	// line's block (COLOR00, BPLCON1, BPL2MOD; on AGA 4 MOVEs of COLOR00) into the
	// $30 6-plane fetch, where the copper gets a MOVE per 8 colour clocks (review
	// A2; the kit's perspectivefloor, d4ba718). A red COLOR00 probe at the end of
	// line FLOOR_Y's block started at full-frame x 140 (OCS) / 237 (AGA)
	agkCopWait(pW, uwFloorY - 1, 0);
	s_pPalSlot[ubBfr][2] = pW->uwPos;
	for(UBYTE i = 1; i < 8; ++i) cwReg(pW, (UWORD)(REG_COLOR00 + 2 * (8 + i)), g_pMissionLandPal[ubMission][i]);
	// its end: PF2 at floor row 0 (six MOVEs, done before line FLOOR_Y's block)
	agkCopWait(pW, uwFloorY - 1, SWITCH_WAIT_X);
	s_pFloorPtr[ubBfr] = pW->uwPos;
	for(UBYTE p = 0; p < 3; ++p) cwPtr8(pW, (UBYTE)(1 + 2 * p), (ULONG)s_pPlainFloor->Planes[p]);
	for(UBYTE j = 0; j < uwLines; ++j) {
		agkCopWait(pW, uwFloorY + j, 0);
		if(s_isAga) cwSky24(pW, s_pFloorBg[ubMission][j], s_pFloorBgLo[ubMission][j]);
		else cwReg(pW, REG_COLOR00, s_pFloorBg[ubMission][j]);
		if(!j) s_pFloorCon1[ubBfr] = pW->uwPos;
		cwReg(pW, REG_BPLCON1, 0);
		cwReg(pW, REG_BPL2MOD, (UWORD)(s_pPlainFloor->BytesPerRow - FETCH_BYTES));
	}
	s_sHillSlots[0][ubBfr] = cwBand(pW, pLay->b[BK_HILLS].y, &s_sHillsPlay);
}

// Each frame the camera moves: every floor line's BPLCON1 and modulo, straight
// into the back list (logic.c: an add, a mask and a table a line), then row 0's
// pointers. AGA keeps BPLCON1's quarter pixels, OCS drops them.
__attribute__((noinline, optimize("Os"))) static void floorSet(tCopCmd *pList, UBYTE ubBfr, WORD wCam) {
	UWORD *pVal = (UWORD *)&pList[s_pFloorCon1[ubBfr]] + 1;   // (a MOVE: register, value)
	WORD wOff = logicFloorScroll(wCam, s_isAga ? 0xFFFF : 0xFFFC,
		(UWORD)(s_pPlainFloor->BytesPerRow - FETCH_BYTES), pVal, (UBYTE)(2 * s_ubFloorStride));
	UWORD *pPtr = (UWORD *)&pList[s_pFloorPtr[ubBfr]] + 1;
	for(UBYTE p = 0; p < 3; ++p) {
		ULONG ulAddr = (ULONG)s_pPlainFloor->Planes[p] + wOff;
		pPtr[0] = (UWORD)(ulAddr >> 16);
		pPtr[2] = (UWORD)ulAddr;
		pPtr += 4;
	}
}

// World 2's sea, the plain's floor re-cut (STYLE-world2 section 2): line 179 (the
// far strip's bare last row) loads the sea palette at its start and points PF2
// at sea row 0 at $D8, as the plain does; then a block at the start of each of
// the SEA_LINES lines (WAIT, COLOR00 the H-to-C gradient, BPLCON1, BPL2MOD).
// Line 207, the sea's bare last row: the island palette after its block (it
// runs on into the line's 6-plane fetch, harmless on an empty row: no pen 9-15
// shows there, and COLOR00 came first), then at $D8 PF2 to the terrain strip
// (cwBand: 6 pointer MOVEs, BPLCON1, BPL2MOD, done before line 208's fetch).
// COLOR00 stays the gradient's last colour, C, under the strip: no MOVE.
__attribute__((noinline, optimize("Os"))) static void cwSea(tAgkCopWriter *pW, UBYTE ubBfr, UBYTE ubMission, const tBandLayout *pLay) {
	const UWORD uwSeaY = pLay->b[BK_FLOOR].y, uwLines = pLay->b[BK_FLOOR].rows, uwStripY = pLay->b[BK_GROUND].y;
	agkCopWait(pW, uwSeaY - 1, 0);
	s_pPalSlot[ubBfr][2] = pW->uwPos;
	for(UBYTE i = 1; i < 8; ++i) cwReg(pW, (UWORD)(REG_COLOR00 + 2 * (8 + i)), g_pW2SeaPal[ubMission][i]);
	agkCopWait(pW, uwSeaY - 1, SWITCH_WAIT_X);
	s_pFloorPtr[ubBfr] = pW->uwPos;
	for(UBYTE p = 0; p < 3; ++p) cwPtr8(pW, (UBYTE)(1 + 2 * p), (ULONG)s_sW2Sea.Planes[p]);
	for(UBYTE j = 0; j < uwLines; ++j) {
		agkCopWait(pW, uwSeaY + j, 0);
		if(s_isAga) cwSky24(pW, s_pW2SeaBg[ubMission][j], s_pW2SeaBgLo[ubMission][j]);
		else cwReg(pW, REG_COLOR00, s_pW2SeaBg[ubMission][j]);
		if(!j) s_pFloorCon1[ubBfr] = pW->uwPos;
		cwReg(pW, REG_BPLCON1, 0);
		cwReg(pW, REG_BPL2MOD, (UWORD)(s_sW2Sea.BytesPerRow - FETCH_BYTES));
#ifdef W2_COP_PROBE
		cwReg(pW, REG_COLOR00, j ? s_pW2SeaBg[ubMission][j] : 0xF00);   // (a block a MOVE longer: the stride +1)
#endif
	}
	s_pPalSlot[ubBfr][3] = pW->uwPos;
	for(UBYTE i = 1; i < 8; ++i) cwReg(pW, (UWORD)(REG_COLOR00 + 2 * (8 + i)), g_pW2IslPal[ubMission][i]);
#ifdef W2_COP_PROBE
	cwReg(pW, REG_COLOR00, 0x0F0);
#endif
	s_sStripSlots[ubBfr] = cwBand(pW, uwStripY, &s_sStrip);
#ifdef W2_COP_PROBE
	cwReg(pW, REG_COLOR00, 0x00F);
#endif
}

// World 3's copper (docs/STYLE-world3.md section 2): the sky down to line 129 (a COLOR00 MOVE a line), PF2 from blank to the far range
// at line 87 (its palette, slot 0, loads in the vertical blank: M7 has no ceiling strip yet), the strip's palette at the start of line 129
// (the far range's bare last row), PF2 to the strip at its end, then the strip from line 130 with the haze behind it: COLOR00 a line.
__attribute__((noinline, optimize("Os"))) static void cwWorld3(tAgkCopWriter *pW, UBYTE ubBfr, UBYTE ubMission, const tBitMap *pFb, const tBandLayout *pLay) {
	const UWORD *pG = s_pW3Grad + s_pW3GradOffs[ubMission];
	const UWORD *pH = pG + 2 * W3_SKY_LINES;
	const UWORD uwFarY = pLay->b[BK_FAR].y, uwStripY = pLay->b[BK_GROUND].y;
	ULONG ulFbFarRow = (UWORD)((UWORD)(uwFarY * FB_ROW_BYTES) + FB_FETCH_OFFS);   // (16-bit maths: the row is under 64 KB)
	tCopSlots *pFar = &s_sMtnSlots[0][ubBfr];
	for(UWORD y = PLAY_TOP; y < pLay->skyEnd; ++y) {
		agkCopWait(pW, y, 0);
		s_pSkySlot[ubBfr][y - PLAY_TOP] = pW->uwPos;
		if(s_isAga) cwSky24(pW, pG[y - PLAY_TOP], pG[W3_SKY_LINES + y - PLAY_TOP]);
		else agkCopMove(pW, &g_pCustom->color[0], pG[y - PLAY_TOP]);
		if(y == uwFarY - 3) {
			// BPL4-6 aren't fetched yet: the far range's planes 2-3 early, PF1's third plane
			agkCopWait(pW, y, SWITCH_WAIT_X);
			pFar->pPtr[1] = pW->uwPos;
			agkCopPtrs(pW, 3, (ULONG)s_sW3Far.Planes[1]);
			pFar->pPtr[2] = pW->uwPos;
			agkCopPtrs(pW, 5, (ULONG)s_sW3Far.Planes[2]);
			agkCopPtrs(pW, 4, (ULONG)pFb->Planes[2] + ulFbFarRow);
		}
		if(y == uwFarY - 1) {
			agkCopWait(pW, y, SWITCH_WAIT_X);
			agkCopMove(pW, &g_pCustom->bplcon0, BPLCON0_DPF6);
			pFar->pPtr[0] = pW->uwPos;
			agkCopPtrs(pW, 1, (ULONG)s_sW3Far.Planes[0]);
			agkCopPtrs(pW, 2, (ULONG)pFb->Planes[1] + ulFbFarRow);
			pFar->uwCon1 = pW->uwPos;
			agkCopMove(pW, &g_pCustom->bplcon1, 0);
			agkCopMove(pW, &g_pCustom->bpl2mod, s_sW3Far.BytesPerRow - FETCH_BYTES);
		}
	}
	agkCopWait(pW, uwStripY - 1, 0);
	s_pPalSlot[ubBfr][1] = pW->uwPos;
	for(UBYTE i = 1; i < 8; ++i) cwReg(pW, (UWORD)(REG_COLOR00 + 2 * (8 + i)), g_pW3StripPal[ubMission][i]);
	s_sStripSlots[ubBfr] = cwBand(pW, uwStripY, &s_sStrip);
	for(UWORD j = 0; j < W3_HAZE_LINES; ++j) {
		agkCopWait(pW, uwStripY + j, 0);
		if(!j) s_uwHazeSlot[ubBfr] = pW->uwPos;
		if(s_isAga) cwSky24(pW, pH[j], pH[W3_HAZE_LINES + j]);
		else cwReg(pW, REG_COLOR00, pH[j]);
	}
}

// A new mission in world 3: the same layout, the sky, the haze and the two PF2 palettes patched
__attribute__((noinline, optimize("Os"))) static void w3Mission(tCopCmd *pList, UBYTE ubBfr, UBYTE ubMission) {
	const UWORD *pG = s_pW3Grad + s_pW3GradOffs[ubMission];
	for(UBYTE y = 0; y < W3_SKY_LINES; ++y) {
		UWORD i = s_pSkySlot[ubBfr][y];
		copSetMoveVal(&pList[i].sMove, pG[y]);
		if(s_isAga) copSetMoveVal(&pList[i + 2].sMove, pG[W3_SKY_LINES + y]);
	}
	const UWORD *pH = pG + 2 * W3_SKY_LINES;
	UWORD uwBg = s_uwHazeSlot[ubBfr];
	for(UBYTE j = 0; j < W3_HAZE_LINES; ++j, uwBg += s_uwHazeStride) {
		copSetMoveVal(&pList[uwBg].sMove, pH[j]);
		if(s_isAga) copSetMoveVal(&pList[uwBg + 2].sMove, pH[W3_HAZE_LINES + j]);
	}
	for(UBYTE i = 0; i < 7; ++i) {
		copSetMoveVal(&pList[s_pPalSlot[ubBfr][0] + i].sMove, g_pW3FarPal[ubMission][i + 1]);
		copSetMoveVal(&pList[s_pPalSlot[ubBfr][1] + i].sMove, g_pW3StripPal[ubMission][i + 1]);
	}
}

// A world's three missions share one list layout (a sky MOVE on every line), so
// a new mission in the same world patches its colours into the back list: the
// sky, the floor's or the sea's ground, the PF2 palettes (world 1: clouds,
// mountains, land; world 2: clouds, far islands, sea, islands). A few hundred
// word writes, where writing the whole list again took ~0.9 A500 frame and a
// mission card dropped 4 pictures (tests/w1card.agk)
__attribute__((noinline, optimize("Os"))) static void copperMission(tCopCmd *pList, UBYTE ubBfr, UBYTE ubMission) {
	if(s_ubWorld == W3_WORLD) {
		w3Mission(pList, ubBfr, ubMission);
		return;
	}
	UBYTE isW2 = s_ubWorld;
	s_pSeaPhase[ubBfr] = 0xFF;
	s_pSeaPenStep[ubBfr] = 0xFFFF;
	s_ubSeaHold = 12;
	const UWORD *pSky = isW2 ? s_pW2Sky[ubMission] : s_pSky[ubMission];
	const UWORD *pSkyLo = isW2 ? s_pW2SkyLo[ubMission] : s_pSkyLo ? s_pSkyLo[ubMission] : 0;
	for(UBYTE y = 0; y < FLOOR_Y - PLAY_TOP; ++y) {
		UWORD i = s_pSkySlot[ubBfr][y];
		copSetMoveVal(&pList[i].sMove, pSky[y]);
		if(s_isAga) copSetMoveVal(&pList[i + 2].sMove, pSkyLo[y]);
	}
	const UWORD *pBg = isW2 ? s_pW2SeaBg[ubMission] : s_pFloorBg[ubMission];
	const UWORD *pBgLo = isW2 ? s_pW2SeaBgLo[ubMission] : s_pFloorBgLo[ubMission];
	UWORD uwBg = (UWORD)(s_pFloorCon1[ubBfr] - (s_isAga ? 4 : 1));   // line 0's COLOR00 (cwFloor, cwSea)
	for(UBYTE j = 0; j < (isW2 ? SEA_LINES : FLOOR_LINES); ++j, uwBg += s_ubFloorStride) {
		copSetMoveVal(&pList[uwBg].sMove, pBg[j]);
		if(s_isAga) copSetMoveVal(&pList[uwBg + 2].sMove, pBgLo[j]);
	}
	const UWORD *pPal[4] = {g_pW2CloudPal[ubMission], g_pW2FarPal[ubMission], g_pW2SeaPal[ubMission], g_pW2IslPal[ubMission]};
	if(!isW2) {
		pPal[0] = g_pMissionCloudPal[ubMission];
		pPal[1] = g_pMissionMtnPal[ubMission];
		pPal[2] = g_pMissionLandPal[ubMission];
	}
	for(UBYTE k = 0; k < 3 + isW2; ++k)
		for(UBYTE i = 0; i < 7; ++i) copSetMoveVal(&pList[s_pPalSlot[ubBfr][k] + i].sMove, pPal[k][i + 1]);
}

// Each picture the camera moved: the sea's lines (logicSeaScroll: the camera's per-line
// scroll plus the swell's drift on the near water) and row 0's pointers
__attribute__((noinline, optimize("Os"))) static void seaSet(tCopCmd *pList, UBYTE ubBfr, WORD wCam, UBYTE ubPhase) {
	UWORD *pVal = (UWORD *)&pList[s_pFloorCon1[ubBfr]] + 1;
	WORD wOff;
	if(ubPhase == SEA_NO_SWELL) {
		// (a mission's first pictures: the camera's scroll alone, as before the swell)
		wOff = logicSeaScroll(wCam, s_isAga ? 0xFFFF : 0xFFFC, (UWORD)(s_sW2Sea.BytesPerRow - FETCH_BYTES), pVal, (UBYTE)(2 * s_ubFloorStride));
		s_wSeaBaseCam = -1;
	}
	else {
		wOff = logicSeaScrollSwell(wCam, ubPhase, s_isAga ? 0xFFFF : 0xFFFC,
			(UWORD)(s_sW2Sea.BytesPerRow - FETCH_BYTES), pVal, (UBYTE)(2 * s_ubFloorStride), s_pSeaBase);
		s_wSeaBaseCam = wCam;
	}
	UWORD *pPtr = (UWORD *)&pList[s_pFloorPtr[ubBfr]] + 1;
	for(UBYTE p = 0; p < 3; ++p) {
		ULONG ulAddr = (ULONG)s_sW2Sea.Planes[p] + wOff;
		pPtr[0] = (UWORD)(ulAddr >> 16);
		pPtr[2] = (UWORD)ulAddr;
		pPtr += 4;
	}
}

// The camera is where the sea last was: only the swell moves (its near lines' BPLCON1 and modulos)
__attribute__((noinline, optimize("Os"))) static void seaSwell(tCopCmd *pList, UBYTE ubBfr, UBYTE ubPhase) {
	logicSeaSwell(ubPhase, s_isAga ? 0xFFFF : 0xFFFC, (UWORD)(s_sW2Sea.BytesPerRow - FETCH_BYTES), s_pSeaBase,
		(UWORD *)&pList[s_pFloorCon1[ubBfr]] + 1, (UBYTE)(2 * s_ubFloorStride));
}

// The glints' shimmer: the sea palette's pens 4-7 (the palette MOVEs after line 179's WAIT)
// lighten and fall back in a swell of light, one step every 8 frames
__attribute__((noinline, optimize("Os"))) static void seaPens(tCopCmd *pList, UBYTE ubBfr, UBYTE ubMission, UWORD uwFrame) {
	UWORD uwStep = (UWORD)(uwFrame >> SEA_PEN_SHIFT);
	if(uwStep == s_pSeaPenStep[ubBfr]) return;
	s_pSeaPenStep[ubBfr] = uwStep;
	for(UBYTE k = 4; k < 8; ++k)
		copSetMoveVal(&pList[s_pPalSlot[ubBfr][2] + k - 1].sMove, s_pSeaPenCol[ubMission][k - 4][logicSeaPenLevel(k, uwFrame)]);
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
static void bobsBind(UBYTE ubWorld);
static void diskArtLoad(void);
static void songsLoad(void);
static void songStart(UBYTE ubSong);
static void songsFree(void);

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
	scoresLoad();
	// (the A1200: its 8-plane logo, or without the file the OCS one)
	s_isTitleLogo8 = s_isAga && s_pTitleLogo &&
		agkLoadFile("titlelogo8.bin.pk", s_pTitleLogo, TITLE_LOGO8_BYTES) == TITLE_LOGO8_BYTES;
	s_isTitleLogo = s_isTitleLogo8 || (s_pTitleLogo &&
		agkLoadFile("titlelogo.bin.pk", s_pTitleLogo, TITLE_LOGO_BYTES) == TITLE_LOGO_BYTES);
	// (a hard-disk copy without the file: a title without the logo)
	if(!s_isTitleLogo) agkPrint("AGK title logo missing\n");
	// the plain's floor (without the file: just its ground colours)
	if(agkLoadFile("plainfloor.bin.pk", s_pPlainFloor->Planes[0], PLAIN_FLOOR_BYTES) != PLAIN_FLOOR_BYTES)
		agkPrint("AGK plain floor missing\n");
	for(UBYTE b = 0; b < 2; ++b) {
		copperWriteList(s_pTitleList[b], b, s_sGame.mission, 1);
		s_pTitleListMission[b] = s_sGame.mission;
	}
	agkPerfSkip();
}

// The songs from the floppy (sound/sound.toml disk = true: 33 KB, 3 files), one
// a frame because each load is a few seconds of OS time on a slow loader (the
// harness gives a frame 250). In the splash's black start, after the title:
// the fanfare and the game over first, the theme last, which then starts. Music
// plays nowhere else but the title, the cards and the cleared / over screens,
// so nothing is loaded later (a load stops the music and freezes the picture).
//
// Our own loader, not soundMusicLoad: that puts the whole file in chip RAM, and
// a song's patterns (10 KB of the 33) are read by the CPU, so here only the
// samples go to chip and the patterns to fast RAM (as in the program before).
// Order, names and volumes are sound.toml's (theme, fanfare, gameover).
#define SONG_COUNT 3
static const char *const s_pSongFile[SONG_COUNT] = {"m_theme.mod.pk", "m_fanfare.mod.pk", "m_gameover.mod.pk"};
static const char *const s_pSongName[SONG_COUNT] = {"AGK music theme\n", "AGK music fanfare\n", "AGK music gameover\n"};
static const UBYTE s_pSongVolume[SONG_COUNT] = {50, 52, 50};
_Static_assert(SOUND_MUSIC_COUNT == SONG_COUNT && SOUND_MUSIC_THEME == 0 && SOUND_MUSIC_FANFARE == 1 && SOUND_MUSIC_GAMEOVER == 2,
	"sound.toml's song order");
static tPtplayerMod s_pSongMod[SONG_COUNT];
static UBYTE *s_pSongPat[SONG_COUNT], *s_pSongSmp[SONG_COUNT];   // fast (patterns), chip (samples)
static ULONG s_pSongPatBytes[SONG_COUNT], s_pSongSmpBytes[SONG_COUNT];

static void songLoad(UBYTE ubSong) {
	tPtplayerMod *pMod = &s_pSongMod[ubSong];
	UBYTE *pFile = 0;
	ULONG ulFile = 0;
	ptplayerEnableMusic(0);   // (AmigaDOS takes the interrupts for the read)
	// the packed file is read, then unpacked into a scratch block (any RAM)
	tFile *pF = diskFileOpen(s_pSongFile[ubSong], DISK_FILE_MODE_READ, 1);
	if(pF) {
		LONG lPacked = fileGetSize(pF);
		UBYTE *pPacked = lPacked >= 8 ? memAlloc((ULONG)lPacked, MEMF_ANY) : 0;
		ULONG ulRead = pPacked ? fileReadBytes(pF, pPacked, (ULONG)lPacked) : 0;
		fileClose(pF);
		if(pPacked) {
			ulFile = agkUnpackedSize(pPacked);
			if(ulRead == (ULONG)lPacked && ulFile > 1084) {
				pFile = memAlloc(ulFile, MEMF_ANY);
				if(pFile) agkUnpack(pPacked, pFile);
			}
			memFree(pPacked, (ULONG)lPacked);
		}
	}
	if(!pFile) {
		agkPrint("AGK song missing\n");
		return;
	}
	for(UWORD b = 0; b < 1084; ++b) ((UBYTE*)pMod)[b] = pFile[b];   // (the file's header is the player's)
	ULONG ulSmp = 0;
	for(UBYTE i = 0; i < PTPLAYER_MOD_SAMPLE_COUNT; ++i) ulSmp += (ULONG)pMod->pSampleHeaders[i].uwLength * 2;
	if(1084 + ulSmp <= ulFile) {
		ULONG ulPat = ulFile - 1084 - ulSmp;
		UBYTE *pPat = memAlloc(ulPat ? ulPat : 2, MEMF_ANY);
		UBYTE *pSmp = ulSmp ? memAllocChip(ulSmp) : 0;
		if(pPat && (pSmp || !ulSmp)) {
			for(ULONG i = 0; i < ulPat; ++i) pPat[i] = pFile[1084 + i];
			for(ULONG i = 0; i < ulSmp; ++i) pSmp[i] = pFile[1084 + ulPat + i];
			pMod->pPatterns = pPat;
			pMod->ulPatternsSize = ulPat;
			pMod->isOwningSamples = 0;
			UBYTE *pS = pSmp;
			for(UBYTE i = 0; i < PTPLAYER_MOD_SAMPLE_COUNT; ++i) {
				UWORD uwWords = pMod->pSampleHeaders[i].uwLength;
				pMod->pSampleStarts[i] = uwWords ? (UWORD*)pS : 0;
				pS += uwWords * 2;
			}
			s_pSongPat[ubSong] = pPat;
			s_pSongPatBytes[ubSong] = ulPat ? ulPat : 2;
			s_pSongSmp[ubSong] = pSmp;
			s_pSongSmpBytes[ubSong] = ulSmp;
			pPat = pSmp = 0;
		}
		if(pPat) memFree(pPat, ulPat ? ulPat : 2);
		if(pSmp) memFree(pSmp, ulSmp);
	}
	memFree(pFile, ulFile);
	if(!s_pSongPat[ubSong]) agkPrint("AGK song missing\n");
}

static void songsLoad(void) {
	static const UBYTE s_pSongOrder[SONG_COUNT] = {SOUND_MUSIC_FANFARE, SOUND_MUSIC_GAMEOVER, SOUND_MUSIC_THEME};
	songLoad(s_pSongOrder[SONG_COUNT - s_ubSongsPending]);
	if(--s_ubSongsPending == 0) {
		agkPrint("AGK songs loaded\n");
		musicFor(s_sGame.state);
	}
	agkPerfSkip();
}

static void songStart(UBYTE ubSong) {
	if(!s_pSongPat[ubSong]) return;   // (not on the floppy: no music)
	ptplayerLoadMod(&s_pSongMod[ubSong], 0, 0);
	ptplayerSetMasterVolume(s_pSongVolume[ubSong]);   // music only, not the effects
	ptplayerConfigureSongRepeat(1, 0);
	ptplayerEnableMusic(1);
	agkPrint(s_pSongName[ubSong]);
}

static void songsFree(void) {
	for(UBYTE i = 0; i < SONG_COUNT; ++i) {
		if(s_pSongPat[i]) memFree(s_pSongPat[i], s_pSongPatBytes[i]);
		if(s_pSongSmp[i]) memFree(s_pSongSmp[i], s_pSongSmpBytes[i]);
		s_pSongPat[i] = s_pSongSmp[i] = 0;
	}
}

// (no loop unswitching: the AGA branches would copy whole loops - code size
// counts on the A500, whose slow RAM is full)
// (optimized for size: the program's growth costs AROS ~3 boot frames a KB, and with the
// whole-long entries a list is still ~0.9 A500 frame; it runs at a mission's start)
__attribute__((optimize("Os", "no-unswitch-loops")))
static void copperWriteList(tCopCmd *pList, UBYTE ubBfr, UBYTE ubMission, UBYTE isTitle) {
	tAgkCopWriter sW;
	if(!isTitle) { s_pSeaPhase[ubBfr] = 0xFF; s_pSeaPenStep[ubBfr] = 0xFFFF; s_ubSeaHold = 12; }
	// the game list starts after the sprite pointers, the title list jumped to
	// from there
	agkCopInit(&sW, pList, isTitle ? 0 : COP_TOP_POS, s_pView->ubPosY);
	ULONG pBlank[3] = {(ULONG)s_pBlankRow, (ULONG)s_pBlankRow, (ULONG)s_pBlankRow};
	const tBitMap *pFb = s_pFb[ubBfr];
	ULONG ulFbRow = (ULONG)(PLAY_TOP * FB_ROW_BYTES + FB_FETCH_OFFS);
	ULONG ulFbCloudRow = (ULONG)(logicBands(isTitle ? 0 : s_ubWorld)->b[BK_CLOUDS].y * FB_ROW_BYTES + FB_FETCH_OFFS);
	ULONG ulFbTitleRow = (ULONG)((TITLE_LOGO_END + 3) * FB_ROW_BYTES + FB_FETCH_OFFS);
	tCopSlots *pCld = &s_sCloudSlots[ubBfr];
	const tBandLayout *pLay = logicBands(isTitle ? 0 : s_ubWorld);   // (T3: the play list's bands; the title has its own layout)
	const UWORD uwCldY = pLay->b[BK_CLOUDS].y, uwCldEnd = (UWORD)(uwCldY + pLay->b[BK_CLOUDS].rows), uwFarY = pLay->b[BK_FAR].y;
	// the world's tables (world 2: STYLE-world2's sky, palettes and bands; the title is world 1's)
	UBYTE isW3 = !isTitle && s_ubWorld == W3_WORLD;
	UBYTE isW2 = !isTitle && s_ubWorld == 1;
	const UWORD *pSkyCol = isW2 ? s_pW2Sky[ubMission] : s_pSky[ubMission];
	const UWORD *pSkyColLo = isW2 ? s_pW2SkyLo[ubMission] : s_pSkyLo ? s_pSkyLo[ubMission] : 0;
	const UWORD *pSkyDots = isW3 ? g_pW3SkyDotPal[ubMission] : isW2 ? g_pW2SkyDotPal[ubMission] : g_pMissionSkyDotPal[ubMission];
	const UWORD *pFarPal = isW2 ? g_pW2FarPal[ubMission] : g_pMissionMtnPal[ubMission];

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
		for(UBYTE i = 0; i < 3; ++i) cwReg(&sW, (UWORD)(REG_COLOR00 + 2 * (29 + i)), pSkyDots[i]);
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
	if(!isTitle) s_pPalSlot[ubBfr][0] = sW.uwPos;
	if(!isTitle) cwPf2Palette(&sW, isW3 ? g_pW3FarPal[ubMission] : isW2 ? g_pW2CloudPal[ubMission] : g_pMissionCloudPal[ubMission]);   // PF2 shows nothing until the clouds
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
	else if(isW3) cwWorld3(&sW, ubBfr, ubMission, pFb, pLay);   // (its sky, far range, strip and haze)
	else {
		// The sky down to the plain, a MOVE on every line (even where the
		// mountains hide the sky: COLOR00 is also the overscan border either side
		// of the picture), so a world's three missions share one list layout and
		// a new one only patches the colours (copperMission); the switch to dual
		// playfield
		for(UWORD y = PLAY_TOP; y < pLay->skyEnd; ++y) {
			agkCopWait(&sW, y, 0);
			s_pSkySlot[ubBfr][y - PLAY_TOP] = sW.uwPos;
			if(s_isAga) cwSky24(&sW, pSkyCol[y - PLAY_TOP], pSkyColLo[y - PLAY_TOP]);
			else agkCopMove(&sW, &g_pCustom->color[0], pSkyCol[y - PLAY_TOP]);
			if(y == uwCldY - 3) {
				// BPL4-6 aren't fetched yet: load them early (cloud planes 2-3,
				// PF1's third plane at the clouds' first line)
				agkCopWait(&sW, y, SWITCH_WAIT_X);
				pCld->pPtr[1] = sW.uwPos;
				agkCopPtrs(&sW, 3, (ULONG)s_pCloudsBm->Planes[1]);
				pCld->pPtr[2] = sW.uwPos;
				agkCopPtrs(&sW, 5, (ULONG)s_pCloudsBm->Planes[2]);
				agkCopPtrs(&sW, 4, (ULONG)pFb->Planes[2] + ulFbCloudRow);
			}
			if(y == uwCldY - 1) {
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
			if(y == uwCldEnd - 1) {
				// clouds -> blank PF2 rows: the mountain palette loads unseen
				agkCopWait(&sW, y, SWITCH_WAIT_X);
				cwPlanePtrs(&sW, 1, pBlank);
				agkCopMove(&sW, &g_pCustom->bpl2mod, (UWORD)-FETCH_BYTES);
			}
			if(y == uwCldEnd + 1) {
				agkCopWait(&sW, y, SWITCH_WAIT_X);
				s_pPalSlot[ubBfr][1] = sW.uwPos;
				cwPf2Palette(&sW, pFarPal);
			}
			if(y == uwFarY - 1) {
				s_sMtnSlots[0][ubBfr] = cwBand(&sW, uwFarY, s_pFarBm);
			}
		}
	}
	if(isTitle) {
		// Mountains -> hills: a blank line in between, for the land palette
		agkCopWait(&sW, GAP_Y - 1, SWITCH_WAIT_X);
		cwPlanePtrs(&sW, 1, pBlank);
		agkCopMove(&sW, &g_pCustom->bpl2mod, (UWORD)-FETCH_BYTES);
		agkCopWait(&sW, GAP_Y, 0);
		s_uwTitleLandPal = sW.uwPos;
		cwPf2Palette(&sW, g_pMissionLandPal[ubMission]);
		s_sHillSlots[1][ubBfr] = cwBand(&sW, HILLS_Y, s_pHills);
	}
	else if(isW3) { /* cwWorld3 wrote the strip and the haze */ }
	else if(isW2) cwSea(&sW, ubBfr, ubMission, pLay);   // the sea, then the strip to the bottom
	else cwFloor(&sW, ubBfr, ubMission, pLay);
	if(!isW2 && !isW3) s_sGndSlots[isTitle][ubBfr] = cwBand(&sW, pLay->b[BK_GROUND].y, s_pGround);

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

// ------------------------------------------------- world 2's terrain strip ---
// The ring (STRIP_SPAN columns drawn: lo .. lo + 21) follows the camera: a
// column coming into either picture is queued with the BOBs, 3 blits (one a
// plane, A -> D, 48 words) from the tile bank, about one every 5 pictures at
// 3 px a frame. A jump (a mission's start, a world's load) draws the whole span
// again, STRIP_FILL_PER_FRAME columns a picture (all 22 at once took a picture
// 135 % in the spike), while both lists point the strip's lines at the blank
// row: the islands appear whole, under the mission card.
static WORD s_wStripFirst = -100;   // the first column the last picture showed
static WORD s_wStripLo;             // the span drawn (once full)
static UBYTE s_ubStripFill;         // columns of the span drawn since the jump (STRIP_SPAN: full)
static UBYTE s_pStripLive[2];       // per list: its strip lines show the strip (1) or blank (0)
static WORD s_pStripCam[2];         // the camera its strip pointers are for

__attribute__((noinline, optimize("Os"))) static void stripColumn2(WORD j) {
	UBYTE t = (j < 0 || j >= W2_MAP_COLS) ? 0 : s_pW2Map[j];   // (off the map: water)
	const UBYTE *pTile = s_pW2Tiles + (UWORD)t * (ART_W2_TILE_H * 6);
	for(UBYTE p = 0; p < 3; ++p) {
		tAgkBlit *pB = agkBlitqSlot();
		pB->uwCon0 = 0x09F0;        // USEA | USED, D = A
		pB->uwCon1 = 0;
		pB->uwAlwm = 0xFFFF;
		pB->pA = (UBYTE *)pTile + 2 * p;       // the tile's plane p: rows 6 bytes apart (interleaved)
		pB->pB = 0;
		pB->pCD = (UBYTE *)s_sStrip.Planes[p] + 2 * j;
		pB->wSrcMod = 6 - 2;
		pB->wDstMod = STRIP_RB - 2;
		pB->uwSize = (UWORD)((ART_W2_TILE_H << 6) | 1);
		agkBlitqPush();
	}
}

// World 3's column (SPEC 13, the column plan of art/tools/make_world3_disk.py): per plane a clear above the tile, the tile's rows
// (a top-edge tile, 16 px wide, its surface continuing the last joint) and the rock fill from where the tile ends down to the ring's
// last row: 9 blits, A -> D from the arena. Off the map: all clear.
__attribute__((noinline, optimize("Os"))) static void stripBlit3(UWORD uwCon0, const UBYTE *pA, UBYTE *pD, WORD wSrcMod, UWORD uwRows) {
	tAgkBlit *pB = agkBlitqSlot();
	pB->uwCon0 = uwCon0;
	pB->uwCon1 = 0;
	pB->uwAlwm = 0xFFFF;
	pB->pA = (UBYTE *)pA;
	pB->pB = 0;
	pB->pCD = pD;
	pB->wSrcMod = wSrcMod;
	pB->wDstMod = STRIP_RB - 2;
	pB->uwSize = (UWORD)((uwRows << 6) | 1);
	agkBlitqPush();
}

__attribute__((noinline, optimize("Os"))) static void stripColumn3(WORD j) {
	UWORD uwTop = STRIP3_ROWS, uwRows = 0, uwFrom = STRIP3_ROWS;
	const UBYTE *pTile = 0, *pFill = 0;
	if(j >= 0 && j < W3_MAP_COLS) {
		const UBYTE *pP = s_pW3Plan + 3 * (UWORD)j;
		const UBYTE *pT = s_pW3TileTab + 4 * (UWORD)pP[0];   // {UWORD offset, UBYTE rows, pad}, big endian
		uwTop = pP[2];
		uwRows = pT[2];
		if(uwTop + uwRows > STRIP3_ROWS) uwRows = (UWORD)(STRIP3_ROWS - uwTop);
		pTile = s_pW3Tiles + (UWORD)((pT[0] << 8) | pT[1]);
		pFill = s_pW3Fills + (UWORD)((UWORD)pP[1] * (W3_FILL_ROWS * 6)) - W3_FILL_ROW0 * 6;   // (a fill stores rows W3_FILL_ROW0 .. 125)
		uwFrom = (UWORD)(uwTop + uwRows);
	}
	for(UBYTE p = 0; p < 3; ++p) {
		UBYTE *pCol = (UBYTE *)s_sStrip.Planes[p] + 2 * j;
		if(uwTop) stripBlit3(USED, 0, pCol, 0, uwTop);
		if(uwRows) stripBlit3(USEA | USED | MINTERM_A, pTile + 2 * p, pCol + (UWORD)(uwTop * STRIP_RB), 6 - 2, uwRows);
		if(uwFrom < STRIP3_ROWS) stripBlit3(USEA | USED | MINTERM_A, pFill + (UWORD)(uwFrom * 6) + 2 * p, pCol + (UWORD)(uwFrom * STRIP_RB), 6 - 2, (UWORD)(STRIP3_ROWS - uwFrom));
	}
}

static void stripColumn(WORD j) {
	if(s_ubWorld == W3_WORLD) stripColumn3(j);
	else stripColumn2(j);
}

static void stripNew(WORD wLo, WORD wOld) {
	WORD wFrom, wTo;
	logicStripNew(wLo, wOld, STRIP_SPAN, &wFrom, &wTo);
	for(WORD j = wFrom; j < wTo; ++j) stripColumn(j);
}

__attribute__((noinline, optimize("Os"))) static void stripUpdate(tCopCmd *pList, UBYTE ubBfr, WORD wCam) {
	WORD wFirst = (WORD)((wCam - 1) >> 4);   // logicScrollByteOffset's word
	WORD wStep = (WORD)(wFirst - s_wStripFirst);
	UBYTE isJump = wStep > 1 || wStep < -1;
	if(isJump) {
		// a jump: the span again, from scratch
		s_ubStripFill = 0;
		s_wStripLo = wFirst;
		s_pStripLive[0] = s_pStripLive[1] = 0xFF;   // (both lists: blank now)
	}
	else if(s_ubStripFill == STRIP_SPAN) {
		// both pictures' columns: this one's and the last one's
		WORD wLo = wStep < 0 ? wFirst : s_wStripFirst;
		if(wLo != s_wStripLo) {
			WORD wOld = s_wStripLo;
			s_wStripLo = wLo;
			stripNew(wLo, wOld);
		}
	}
	s_wStripFirst = wFirst;
	// (a mission's start is a heavy picture already, its colours and the logic's new
	// start: the fill begins at the next one, unless the bitplanes are off)
	if(s_ubStripFill < STRIP_SPAN && (!isJump || s_ubRasterOff)) {
		// (with the bitplanes off after a world's load, nothing shows and the load's
		// picture isn't timed: the whole span at once)
		UBYTE ubN = s_ubRasterOff && s_ubWorld != W3_WORLD ? STRIP_SPAN : s_ubStripFillRate;   // (world 3's columns are 9 blits: the queue holds a few)
		for(UBYTE k = 0; k < ubN && s_ubStripFill < STRIP_SPAN; ++k) stripColumn((WORD)(s_wStripLo + s_ubStripFill++));
		// (the camera may have moved a few columns during the fill: no picture shows the
		// strip yet, so the span is this picture's; queued after the fill's blits)
		if(s_ubStripFill == STRIP_SPAN && wFirst != s_wStripLo) {
			WORD wOld = s_wStripLo;
			s_wStripLo = wFirst;
			stripNew(wFirst, wOld);
		}
	}
	// this list's strip lines: blank until the span is whole (it is drawn by the
	// end of this picture), then the strip at the camera
	UBYTE isLive = s_ubStripFill == STRIP_SPAN;
	if(isLive == s_pStripLive[ubBfr] && (!isLive || wCam == s_pStripCam[ubBfr])) return;
	s_pStripLive[ubBfr] = isLive;
	s_pStripCam[ubBfr] = wCam;
	const tCopSlots *pSlots = &s_sStripSlots[ubBfr];
	if(isLive) {
		bandSet(pList, pSlots, &s_sStrip, (UWORD)wCam);
		copSetMoveVal(&pList[pSlots->uwCon1 + 1].sMove, STRIP_RB - FETCH_BYTES);
	}
	else {
		for(UBYTE p = 0; p < 3; ++p) {
			copSetMoveVal(&pList[pSlots->pPtr[p]].sMove, (ULONG)s_pBlankRow >> 16);
			copSetMoveVal(&pList[pSlots->pPtr[p] + 1].sMove, (ULONG)s_pBlankRow & 0xFFFF);
		}
		copSetMoveVal(&pList[pSlots->uwCon1 + 1].sMove, (UWORD)-FETCH_BYTES);   // every line re-reads the blank row
	}
}

// World 2's bands for the back list: the strip every picture (its columns, its
// fill); when the camera moved, the sea (with its swell) and the other bands; when it
// did not, the swell alone every 4th picture (a phase step) and the pens' shimmer every
// 8th, but only in a picture that queued at most SEA_BLIT_MAX blits (a quiet one: a
// fight's pictures wait, they are the frame budget's worst). Deterministic: the blit
// count is a function of the game, like the frame counter.
__attribute__((noinline, optimize("Os"))) static void w2Update(tCopCmd *pList, UBYTE ubBfr, WORD wCam, WORD *pCamShown) {
	stripUpdate(pList, ubBfr, wCam);
	UWORD uwFrame = s_ubSeaStill ? 0 : s_sGame.frame;
	// (a list just written or patched: a mission's first pictures are the budget's worst, so the
	// swell and the shimmer wait a few pictures, the water as the camera alone puts it)
	UBYTE ubPhase = s_ubSeaHold ? SEA_NO_SWELL : logicSeaPhase(uwFrame);
	if(s_ubSeaHold) --s_ubSeaHold;
	UBYTE isMoved = wCam != *pCamShown;
	if(!isMoved) {
		if(g_uwAgkBlitqCount > SEA_BLIT_MAX || ubPhase == SEA_NO_SWELL) return;
		if(ubPhase != s_pSeaPhase[ubBfr]) {
			if(s_pSeaPhase[ubBfr] != 0xFF && s_wSeaBaseCam == wCam) seaSwell(pList, ubBfr, ubPhase);
			else seaSet(pList, ubBfr, wCam, ubPhase);
			s_pSeaPhase[ubBfr] = ubPhase;
		}
		seaPens(pList, ubBfr, s_sGame.mission, uwFrame);
		return;
	}
	s_pSeaPhase[ubBfr] = ubPhase;
	seaSet(pList, ubBfr, wCam, ubPhase);
	if(ubPhase != SEA_NO_SWELL && g_uwAgkBlitqCount <= SEA_BLIT_MAX) seaPens(pList, ubBfr, s_sGame.mission, uwFrame);
	*pCamShown = wCam;
	if(s_isAga) {
		bandSet4(pList, &s_sCloudSlots[ubBfr], &s_sW2Clouds, wCam, 3, ART_W2_CLOUDS_LOOP_W);
		s_uwMtnCon1 = bandSet4(pList, &s_sMtnSlots[0][ubBfr], &s_sW2Far, wCam, 2, ART_W2_FAR_LOOP_W);
		return;
	}
	bandSet(pList, &s_sCloudSlots[ubBfr], &s_sW2Clouds, logicBandOffset(wCam, 3, ART_W2_CLOUDS_LOOP_W));
	s_uwMtnX = logicBandOffset(wCam, 2, ART_W2_FAR_LOOP_W);
	bandSet(pList, &s_sMtnSlots[0][ubBfr], &s_sW2Far, s_uwMtnX);
	s_uwMtnCon1 = (UWORD)(logicScrollDelay(s_uwMtnX) << 4);
	s_uwMtnX4 = (UWORD)(s_uwMtnX << 2);
}

// World 3's bands for the back list: the strip every picture, the far range (1/4 speed) when the camera moved
__attribute__((noinline, optimize("Os"))) static void w3Update(tCopCmd *pList, UBYTE ubBfr, WORD wCam, WORD *pCamShown) {
	stripUpdate(pList, ubBfr, wCam);
	if(wCam == *pCamShown) return;
	*pCamShown = wCam;
	if(s_isAga) {
		s_uwMtnCon1 = bandSet4(pList, &s_sMtnSlots[0][ubBfr], &s_sW3Far, wCam, 2, W3_FAR_LOOP_W);
		return;
	}
	s_uwMtnX = logicBandOffset(wCam, 2, W3_FAR_LOOP_W);
	bandSet(pList, &s_sMtnSlots[0][ubBfr], &s_sW3Far, s_uwMtnX);
	s_uwMtnCon1 = (UWORD)(logicScrollDelay(s_uwMtnX) << 4);
	s_uwMtnX4 = (UWORD)(s_uwMtnX << 2);
}

// The back list: the jump to the title list or not; this mission's colours
// (the game list rewritten once when its world changes, patched for a new
// mission, written at the title too while a world's load has the bitplanes
// off; the title list's patched),
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
	if(isTitle && !s_ubRasterOff) {
		// (the game list waits for the game: a new mission's colours are a patch then)
	}
	else if(s_sGame.world != s_ubWorld) {
		// (the other world's art loads at the next picture's start: no list for this one now)
	}
	else if(s_pListMission[ubBfr] != 0xFF && (s_pListMission[ubBfr] >> 2) == s_ubWorld &&
		s_pListMission[ubBfr] != (UBYTE)((s_ubWorld << 2) | s_sGame.mission)) {
		// another mission of this world: the same layout, new colours
		s_pListMission[ubBfr] = (UBYTE)((s_ubWorld << 2) | s_sGame.mission);
		copperMission(pList, ubBfr, s_sGame.mission);
	}
	else if(s_pListMission[ubBfr] != (UBYTE)((s_ubWorld << 2) | s_sGame.mission)) {
		s_pListMission[ubBfr] = (UBYTE)((s_ubWorld << 2) | s_sGame.mission);
		copperWriteList(pList, ubBfr, s_sGame.mission, 0);
		pListCam[0][ubBfr] = -1;
		s_pStripLive[ubBfr] = 0xFF;   // (its strip pointers: written again)
		// (a whole list is about 1.3 A500 frames of CPU: the second list after a world's
		// load is written with the bitplanes still off, a picture of the load, unseen)
		if(s_ubRasterOff) agkPerfSkip();
		if(s_ubWorld) {
			// world 2's list (its size: genericCreate)
			agkState(s_ubWorld == W3_WORLD ? "world3 copper_used" : "world2 copper_used", s_uwCopUsed);
			agkState("copper_max", s_uwCopCount);
			agkEnd();
			if(s_uwCopUsed > s_uwCopCount) agkPrint("AGK ERR copper list overflow\n");
		}
	}
	if(isTitle) {
		// (world 1's load at the title, after a game in world 2: the game lists
		// above are written now, while the bitplanes are off, not at the next
		// game's start or the hall of fame's with them on: tests/w2over.agk)
		pList = s_pTitleList[ubBfr];
		if(s_pTitleListMission[ubBfr] != s_sGame.mission) titleListMission(ubBfr, s_sGame.mission);
	}
	if(!isTitle && s_ubWorld) {
		if(s_ubWorld == W3_WORLD) w3Update(pList, ubBfr, wCam, &pListCam[0][ubBfr]);
		else w2Update(pList, ubBfr, wCam, &pListCam[0][ubBfr]);
		return;
	}
	if(wCam == pListCam[isTitle][ubBfr]) return;   // a still camera: nothing to scroll (chip RAM writes are slow)
	pListCam[isTitle][ubBfr] = wCam;
	const tBitMap *pHills = isTitle ? s_pHills : &s_sHillsPlay;
	if(!isTitle) floorSet(pList, ubBfr, wCam);
	if(s_isAga) {
		// the far bands in quarter pixels: no judder at 1/8, 1/4, 1/2 speed
		if(!isTitle) bandSet4(pList, &s_sCloudSlots[ubBfr], s_pCloudsBm, wCam, 3, ART_CLOUDS_LOOP_W);
		s_uwMtnCon1 = isTitle ? bandSet4(pList, &s_sMtnSlots[1][ubBfr], s_pTitleMtn, wCam, 2, ART_TITLE_MTN_LOOP_W) :
			bandSet4(pList, &s_sMtnSlots[0][ubBfr], s_pMountains, wCam, 2, ART_MOUNTAINS_LOOP_W);
		s_uwHillCon1 = bandSet4(pList, &s_sHillSlots[isTitle][ubBfr], pHills, wCam, 1, ART_HILLS_LOOP_W);
		s_uwGndX = logicBandOffset(wCam, 0, ART_GROUND_LOOP_W);
		bandSet(pList, &s_sGndSlots[isTitle][ubBfr], s_pGround, s_uwGndX);
		return;
	}
	if(!isTitle) bandSet(pList, &s_sCloudSlots[ubBfr], s_pCloudsBm, logicBandOffset(wCam, 3, ART_CLOUDS_LOOP_W));
	s_uwMtnX = logicBandOffset(wCam, 2, ART_MOUNTAINS_LOOP_W);
	s_uwHillX = logicBandOffset(wCam, 1, ART_HILLS_LOOP_W);
	s_uwGndX = logicBandOffset(wCam, 0, ART_GROUND_LOOP_W);
	bandSet(pList, &s_sMtnSlots[isTitle][ubBfr], isTitle ? s_pTitleMtn : s_pMountains, s_uwMtnX);
	bandSet(pList, &s_sHillSlots[isTitle][ubBfr], pHills, s_uwHillX);
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
// (rows of 44: long-aligned, for msgUpdate's four-letter compare)
static char s_pMsgStage[2][MSG_LINES][MSG_LEN + 4] __attribute__((aligned(4)));   // what each line is showing / about to show
typedef ULONG __attribute__((may_alias)) tUlongAlias;
static UWORD s_pMsgTypeKey[2] = {0xFFFF, 0xFFFF};   // the key msgUpdate is typing (or typed)
static UWORD s_pMsgDone[2];                  // per line (a bit): typed out under that key
static ULONG s_pMsgCover[2][MSG_LINES][2];  // columns (bits) this buffer's BOBs covered last time
static ULONG s_pMsgFix[2][MSG_LINES][2];    // columns an erase crossed: redraw their letters
static ULONG s_pMsgInk[2][MSG_LINES][2];    // columns with a letter staged (not a space): the only ones a fix redraws
static UBYTE s_pMsgAnyFix[2];
static UBYTE s_pMsgAnyCover[2];              // any bits in s_pMsgCover[buffer]
static UWORD s_pMsgCoverLines[2], s_pMsgFixLines[2];   // per buffer, the lines (a bit each) that may hold cover or fix bits: objectsQueue visits only those
static UWORD s_pMsgKey[2] = {0xFFFF, 0xFFFF};   // what the lines say (state, blink...) once settled

#define TPL_MAX 104   // the shared BOBs' 28 frames + world 2's 29 and the common slice 2 set's 12 (world 1's own: 23) + the biggest mission part's 32 (mission 6: the freighter and the destroyer, the hull whole and in pieces); one more than the last template made
static tAgkBlitArea s_pErase[2][DRAW_MAX];
static UBYTE s_pErased[2];
static UBYTE s_ubLastDrawn;          // objects in the last picture (the HUD types less after a busy one)
static tBitMap *s_pBobBm[BOB_TYPES], *s_pBobMask[BOB_TYPES];
static UBYTE s_pTplFirst[BOB_TYPES];
// the destroyer's hull is one BOB while it lies between these (logic.h): where objectsQueue draws a BOB whole inside its margins
_Static_assert(FB_W - 16 - 154 - FB_X0 == DEST_WHOLE_MAX && -FB_X0 == DEST_WHOLE_MIN, "logic.h: the whole hull's range");
static tAgkBlit s_pTpl[TPL_MAX][2];
// Per BOB type, for objectsQueue's loop (src/objblits.s reads it: 16 bytes)
typedef struct {
	const tAgkBlit *pTpl;    // its frame 0's pair of templates (frame f: + 2f)
	UWORD uwMaxX, uwMaxY;    // the buffer positions it fits at (the margins: no clipping)
	UBYTE ubSlack;           // unused pixels in its last word: a shift past it needs the wider template
	UBYTE pPad[7];
} tBobInfo;
_Static_assert(sizeof(tBobInfo) == 16, "objblits.s: 16 bytes a BOB type");
static tBobInfo s_pBobInfo[BOB_TYPES];

// objectsQueue's loop in 68000 assembly (src/objblits.s); its C version below
// (OBJ_BLITS_C) is the reference it was checked against, picture for picture
typedef struct {
	const tDraw *pDraw;
	tAgkBlit *pQueue;        // the queue's first free slot
	tAgkBlit *pNext;         // the next queued blit to start
	tAgkBlitArea *pErase;
	UBYTE *pFb0;
	const tBobInfo *pInfo;
	UWORD uwN;
	UWORD uwRoom;            // in: free slots; out: left
	UWORD uwStarted;         // out
	UWORD uwDrawn;           // out (bit 15: the queue was full)
} tObjBlits;
void objBlitsAsm(AGK_REG(tObjBlits *pCtx, "a0"));
_Static_assert(offsetof(tObjBlits, uwN) == 24 && offsetof(tObjBlits, uwDrawn) == 30, "objblits.s: OB_*");
_Static_assert(sizeof(tAgkBlit) == 24 && offsetof(tAgkBlit, pCD) == 16 && offsetof(tAgkBlit, wDstMod) == 22, "objblits.s: BL_*");
_Static_assert(sizeof(tAgkBlitArea) == 8 && sizeof(tDraw) == 6 && offsetof(tDraw, bob) == 4, "objblits.s: the erase and draw lists");
_Static_assert(FB_X0 == 48 && FB_ROW_BYTES == 162, "objblits.s: FB_X0, FB_ROW_BYTES");
static UWORD s_pFbRowOffs[FB_H];
static tDraw s_pDraw[DRAW_MAX];

// Message columns c0..c1 (c0 <= c1 < MSG_LEN) as bits: word 0 columns 0-31, word 1 32-39
static void msgColumns(ULONG *pOut, UBYTE c0, UBYTE c1) {
	pOut[0] = pOut[1] = 0;
	if(c0 < 32) pOut[0] = (0xFFFFFFFFUL >> (31 - ((c1 < 32 ? c1 : 31) - c0))) << c0;
	if(c1 >= 32) {
		UBYTE a = (UBYTE)(c0 > 32 ? c0 - 32 : 0);
		pOut[1] = (0xFFFFFFFFUL >> (31 - (c1 - 32 - a))) << a;
	}
}

static void bobAdd(UBYTE ubBob, tBitMap *pBm, tBitMap *pMask, UBYTE ubW, UBYTE ubH, UBYTE ubFrames) {
	s_pBobBm[ubBob] = pBm;
	s_pBobMask[ubBob] = pMask;
	if(ubW != g_pBobW[ubBob] || ubH != g_pBobH[ubBob] || ubFrames != g_pBobFrames[ubBob]) {
		agkPrint("AGK ERR logic.c's BOB sizes don't match the art\n");
	}
}

// A BOB's image and mask: bitmaps in the floppy art's block (diskArtCreate),
// blank until diskArtLoad; the blit templates only need their addresses
#define BOB_AT(B, N) bobAdd(B, bobBm(DISK_ART_##N##_OFFS, ART_##N##_BITMAP_W, ART_##N##_H * ART_##N##_FRAMES), \
	bobBm(DISK_ART_##N##MASK_OFFS, ART_##N##_BITMAP_W, ART_##N##_H * ART_##N##_FRAMES), \
	ART_##N##_W, ART_##N##_H, ART_##N##_FRAMES)
static tBitMap s_pBobBmStore[2 * BOB_TYPES];
static UBYTE s_ubBobBms;
// World 2's BOBs (art/tools/make_world_disk.py): its set follows the display art in disk/world2.bin
#define BOB_W2(B, N) bobAdd(B, bobBm(ARENA_OFFS + WORLD2_##N##_OFFS, ART_##N##_BITMAP_W, ART_##N##_H * ART_##N##_FRAMES), \
	bobBm(ARENA_OFFS + WORLD2_##N##MASK_OFFS, ART_##N##_BITMAP_W, ART_##N##_H * ART_##N##_FRAMES), \
	ART_##N##_W, ART_##N##_H, ART_##N##_FRAMES)
// World 2's slice 2 BOBs (missions 5 and 6), from disk/world2x.bin in the block s_pW2X (allocated at world 2's load): an
// offset with W2X_BASE set is in that block, not in the art block. The mission's own part (the ship deck, or the freighter and
// the destroyer's set: disk/world2x5.bin, world2x6.bin) is in a second block s_pW2M made at the mission's card: W2M_BASE
#define W2X_BASE 0x40000000UL
#define W2M_BASE 0x80000000UL
#define BOB_W2X(B, N) bobAdd(B, bobBm(W2X_BASE + WORLD2X_##N##_OFFS, ART_##N##_BITMAP_W, ART_##N##_H * ART_##N##_FRAMES), \
	bobBm(W2X_BASE + WORLD2X_##N##MASK_OFFS, ART_##N##_BITMAP_W, ART_##N##_H * ART_##N##_FRAMES), \
	ART_##N##_W, ART_##N##_H, ART_##N##_FRAMES)
#define BOB_W2M(B, N) bobAdd(B, bobBm(W2M_BASE + WORLD2M_##N##_OFFS, ART_##N##_BITMAP_W, ART_##N##_H * ART_##N##_FRAMES), \
	bobBm(W2M_BASE + WORLD2M_##N##MASK_OFFS, ART_##N##_BITMAP_W, ART_##N##_H * ART_##N##_FRAMES), \
	ART_##N##_W, ART_##N##_H, ART_##N##_FRAMES)
#define BOB_W2M_NM(B, N) bobAdd(B, bobBm(W2M_BASE + WORLD2M_##N##_OFFS, ART_##N##_BITMAP_W, ART_##N##_H * ART_##N##_FRAMES), 0, \
	ART_##N##_W, ART_##N##_H, ART_##N##_FRAMES)   // (no mask: a plain copy, bobIsCopy)
static UBYTE *s_pW2X;                // (chip) world 2's slice 2 common BOBs: allocated when world 2 loads, freed when it is left
static UBYTE *s_pW2M;                // (chip) the mission's own BOBs: made at its card (missionSetLoad), freed at the next
static ULONG s_ulW2MBytes;           // its size (0: none made)
static UBYTE s_ubMPart = 0xFF;       // the mission (1 or 2) whose part is in s_pW2M; 0xFF: none
static UBYTE s_ubBmsCommon;          // bobBm stores used by the world's BOBs, the common slice 2 set included: a mission's part follows
static UBYTE s_ubTplMission;         // the first blit template of a mission's own BOBs
#define BOB_W3(B, N) bobAdd(B, bobBm(ARENA_OFFS + WORLD3_##N##_OFFS, ART_W3_##N##_BITMAP_W, ART_W3_##N##_H * ART_W3_##N##_FRAMES), \
	bobBm(ARENA_OFFS + WORLD3_##N##MASK_OFFS, ART_W3_##N##_BITMAP_W, ART_W3_##N##_H * ART_W3_##N##_FRAMES), \
	ART_W3_##N##_W, ART_W3_##N##_H, ART_W3_##N##_FRAMES)
static UBYTE s_ubTplWorld;           // the first blit template of the world's own BOBs (after the shared ones)
static UBYTE s_ubDiskArtLoaded;      // diskArtLoad has run: the BOBs' art is in the block
static void padCompose(void);
static void bobsCrop(UBYTE b0, UBYTE b1);

static tBitMap *bobBm(ULONG ulOffs, UWORD uwW, UWORD uwRows) {
	tBitMap *pBm = &s_pBobBmStore[s_ubBobBms++];
	pBm->BytesPerRow = (UWORD)(uwW / 8 * 3);
	pBm->Rows = uwRows;
	pBm->Flags = BMF_INTERLEAVED;
	pBm->Depth = 3;
	UBYTE *pBase = ulOffs >= W2M_BASE ? s_pW2M + (ulOffs - W2M_BASE) : ulOffs >= W2X_BASE ? s_pW2X + (ulOffs - W2X_BASE) : s_pDiskArt + ulOffs;
	for(UBYTE p = 0; p < 3; ++p) pBm->Planes[p] = (void *)(pBase + p * (uwW / 8));
	return pBm;
}

// Shots and shells are small and solid: a plain copy (half the bus).
// Buildings are drawn first, onto an empty buffer: a plain copy too
static UBYTE bobIsCopy(UBYTE b) {
	return b == BOB_SHOT || b == BOB_SHELL || b == BOB_HANGAR || b == BOB_FENCE || b == BOB_BARRACKS || b == BOB_FLAG ||
		b == BOB_W2_BARRACKS || b == BOB_W3_LODGE || (b >= BOB_PAD48 && b <= BOB_PAD96) || (b >= BOB_DHULL && b <= BOB_DHULL3);
}

// A BOB's blit templates (from its frame 0 at template t on) and its tables for objectsQueue
__attribute__((noinline, optimize("Os"))) static UBYTE bobFill(UBYTE b, UBYTE t) {
	s_pTplFirst[b] = t;
	for(UBYTE f = 0; f < g_pBobFrames[b] && t < TPL_MAX; ++f, ++t) {
		agkBlitBob(s_pTpl[t], s_pBobBm[b], bobIsCopy(b) ? 0 : s_pBobMask[b], g_pBobW[b], g_pBobH[b], f, FB_PLANE_BYTES);
	}
	// objectsQueue's tables: in the buffer when x <= max x and y <= max y (the
	// buffer's margins: no clipping); the template for one word more (agkBlitPlace's
	// choice) when the shift is past the slack in the last word
	UWORD uwW = g_pBobW[b];
	tBobInfo *pI = &s_pBobInfo[b];
	pI->pTpl = s_pTpl[s_pTplFirst[b]];
	pI->uwMaxX = (UWORD)(FB_W - 16 - uwW);
	pI->uwMaxY = (UWORD)(FB_H - g_pBobH[b]);
	pI->ubSlack = (UBYTE)(((uwW + 15) & ~15) - uwW);
	return t;
}

// The BOBs of both worlds (what the floppy art block holds at boot): the shared ones here, the
// world's own by bobsBind(), whose templates take the same slots in either world
__attribute__((optimize("Os"))) static void bobsCreate(void) {
	BOB_AT(BOB_HOSTAGE, HOSTAGE);
	BOB_AT(BOB_SHOT, SHOT);
	BOB_AT(BOB_BOMB, BOMB);
	BOB_AT(BOB_SHELL, SHELL);
	BOB_AT(BOB_BOOM, BOOM);
	BOB_AT(BOB_SHADOW, SHADOW);
	BOB_AT(BOB_SMOKE, SMOKE);
	BOB_AT(BOB_HANGAR, HANGAR);
	UBYTE t = 0;
	for(UBYTE b = 0; b < BOB_W1_FIRST; ++b) t = bobFill(b, t);
	s_ubTplWorld = t;
	bobsBind(0);
	for(UWORD y = 0; y < FB_H; ++y) s_pFbRowOffs[y] = (UWORD)(y * FB_ROW_BYTES);
}

// The four widths of a pad, each one BOB (no mask: it is drawn as a plain copy), put together from the
// pieces in the set: the left cap (frame 0 of the caps), n units, the H, n units, the right cap, rows
// PAD_SLAB_ROW.. for the slab (the caps' pylons stand above it). A piece is 8 px: the high byte of the
// 16 px word it is stored in (and the H two bytes). The chevrons point at the H, on every other unit.
#define PAD_PIECE_ROW 6            // a piece's row: 2 bytes a plane, 3 planes
__attribute__((noinline, optimize("Os"))) static void padCompose(void) {
	UBYTE *pArena = s_pDiskArt + ARENA_OFFS;
	const UBYTE *pCap = pArena + WORLD2_PAD_CAP_OFFS, *pSlab = pArena + WORLD2_PAD_SLAB_OFFS,
		*pCentre = pArena + WORLD2_PAD_CENTRE_OFFS;
	static const ULONG pOffs[4] = {WORLD2_PAD48_OFFS, WORLD2_PAD64_OFFS, WORLD2_PAD80_OFFS, WORLD2_PAD96_OFFS};
	for(UBYTE k = 0; k < 4; ++k) {
		UBYTE ubBytes = (UBYTE)((48 + 16 * k) >> 3);    // a plane's row of the pad
		UBYTE ubUnits = (UBYTE)(k + 1);                  // 8 px units each side
		UBYTE *pDst = pArena + pOffs[k];
		tBitMap *pBm = bobBm(ARENA_OFFS + pOffs[k], (UWORD)(ubBytes << 3), PAD_ROWS);
		bobAdd((UBYTE)(BOB_PAD48 + k), pBm, 0, (UBYTE)(ubBytes << 3), PAD_ROWS, 1);
		for(UBYTE y = 0; y < PAD_ROWS; ++y) {
			for(UBYTE p = 0; p < 3; ++p, pDst += ubBytes) {
				for(UBYTE b = 0; b < ubBytes; ++b) pDst[b] = 0;
				UWORD uwPlane = (UWORD)(p << 1);
				pDst[0] = pCap[(UWORD)y * PAD_PIECE_ROW + uwPlane];                        // the left cap: frame 0
				pDst[ubBytes - 1] = pCap[(UWORD)(PAD_ROWS + y) * PAD_PIECE_ROW + uwPlane];  // the right: frame 1
				if(y < PAD_SLAB_ROW) continue;
				UWORD r = (UWORD)((y - PAD_SLAB_ROW) * PAD_PIECE_ROW + uwPlane);
				for(UBYTE u = 0; u < ubUnits; ++u) {
					UBYTE ubFrom = (UBYTE)(ubUnits - 1 - u);   // its distance from the H
					pDst[1 + u] = pSlab[(UWORD)((ubFrom & 1) ? 2 * 16 * PAD_PIECE_ROW : 0) + r];   // plain, or the chevron: frame 0 points right
					pDst[3 + ubUnits + u] = pSlab[(UWORD)((u & 1) ? 2 * 16 * PAD_PIECE_ROW : 16 * PAD_PIECE_ROW) + r];   // plain, or frame 1: points left
				}
				pDst[1 + ubUnits] = pCentre[r];                // the H, two bytes
				pDst[2 + ubUnits] = pCentre[r + 1];
			}
		}
	}
}

// The world's own BOBs: world 1's (fence, barracks, tank, jet, mine, flag) or world 2's, over the
// same chip and the same templates (worldLoad: after the world's art is in the block)
__attribute__((noinline, optimize("Os"))) static void bobsBind(UBYTE ubWorld) {
	s_ubBobBms = 2 * BOB_W1_FIRST;
	UBYTE b0, b1;
	if(ubWorld == W3_WORLD) {
		// the lodge and the pad are plain copies (no mask), the nest is masked
		bobAdd(BOB_W3_LODGE, bobBm(ARENA_OFFS + WORLD3_LODGE_OFFS, ART_W3_LODGE_BITMAP_W, ART_W3_LODGE_H * ART_W3_LODGE_FRAMES), 0,
			ART_W3_LODGE_W, ART_W3_LODGE_H, ART_W3_LODGE_FRAMES);
		BOB_W3(BOB_W3_NEST, NEST);
		bobAdd(BOB_PAD64, bobBm(ARENA_OFFS + WORLD3_PAD64_OFFS, ART_W3_PAD64_BITMAP_W, ART_W3_PAD64_H), 0, ART_W3_PAD64_W, ART_W3_PAD64_H, 1);
		b0 = BOB_W3_FIRST;
		b1 = BOB_TYPES;
	}
	else if(ubWorld) {
		BOB_W2(BOB_W2_BARRACKS, W2_BARRACKS);
		BOB_W2(BOB_PATROL, PATROL);
		BOB_W2(BOB_GUNBOAT, GUNBOAT);
		padCompose();
		BOB_W2(BOB_PAD_LAMP, PAD_LAMP);
		BOB_W2(BOB_SPLASH, SPLASH);
		BOB_W2(BOB_W2_JET, W2_JET);
		BOB_W2X(BOB_RADAR, RADAR);   // (slice 2's common set, in its own block; a mission's part: bobsBindMission)
		BOB_W2X(BOB_SOCK, SOCK);
		BOB_W2X(BOB_ROPE, ROPE);
		BOB_W2X(BOB_HOOK, HOOK);
		BOB_W2X(BOB_CLIMB, CLIMB);
		s_ubBmsCommon = s_ubBobBms;
		b0 = BOB_W2_FIRST;
		b1 = BOB_M5_FIRST;
	}
	else {
		BOB_AT(BOB_FENCE, FENCE);
		BOB_AT(BOB_BARRACKS, BARRACKS);
		BOB_AT(BOB_TANK, TANK);
		BOB_AT(BOB_JET, JET);
		BOB_AT(BOB_MINE, MINE);
		BOB_AT(BOB_FLAG, FLAG);
		b0 = BOB_W1_FIRST;
		b1 = BOB_W2_FIRST;
	}
	UBYTE t = s_ubTplWorld;
	for(UBYTE b = b0; b < b1; ++b) t = bobFill(b, t);
	if(ubWorld == W3_WORLD) t = bobFill(BOB_PAD64, t);
	if(t >= TPL_MAX) agkPrint("AGK ERR too many BOB frames: raise TPL_MAX\n");
	if(ubWorld) s_ubTplMission = t;
	if(s_ubDiskArtLoaded) bobsCrop(b0, b1);   // (the art is in the block: at boot diskArtLoad crops them)
}

// A mission's own BOBs (the ship deck in mission 5; the freighter, the strait destroyer and the whitecaps in mission 6), once their
// part is in s_pW2M: their bitmaps and templates, over the same slots each time (the other mission's are not drawn)
__attribute__((noinline, optimize("Os"))) static void bobsBindMission(UBYTE ubMission) {
	s_ubBobBms = s_ubBmsCommon;
	UBYTE b0, b1;
	if(ubMission == 1) {
		BOB_W2M(BOB_DECK, DECK);
		b0 = BOB_M5_FIRST;
		b1 = BOB_M6_FIRST;
	}
	else {
		BOB_W2M(BOB_FRTR, FRTR);
		BOB_W2M(BOB_SPRAY, SPRAY);
		BOB_W2M_NM(BOB_DHULL, DHULL);
		for(UBYTE k = 0; k < 4; ++k) {   // the four pieces: views of the hull's columns (the same bitmap, 3 words a piece, the last one 1)
			tBitMap *pV = &s_pBobBmStore[s_ubBobBms++];
			*pV = *s_pBobBm[BOB_DHULL];
			for(UBYTE p = 0; p < 3; ++p) pV->Planes[p] += 6 * k;
			bobAdd((UBYTE)(BOB_DHULL0 + k), pV, 0, g_pBobW[BOB_DHULL0 + k], g_pBobH[BOB_DHULL0 + k], g_pBobFrames[BOB_DHULL0 + k]);
		}
		BOB_W2M(BOB_TURRET, TURRET);
		BOB_W2M(BOB_THIT, THIT);
		BOB_W2M(BOB_AAGUN, AAGUN);
		BOB_W2M(BOB_BARRAGE, BARRAGE);
		BOB_W2M(BOB_CAP, CAP);
		b0 = BOB_M6_FIRST;
		b1 = BOB_W3_FIRST;
	}
	UBYTE t = s_ubTplMission;
	for(UBYTE b = b0; b < b1; ++b) t = bobFill(b, t);
	if(t >= TPL_MAX) agkPrint("AGK ERR too many BOB frames: raise TPL_MAX\n");
	bobsCrop(b0, b1);
}

static UBYTE s_ubDraws;              // objects in s_pDraw this picture (objectsQueue)

// After the art is loaded (diskArtLoad): each masked BOB frame's two templates
// cut to the rows its mask uses. Where the mask is 0 a cookie-cut writes the
// background back (D = C), and each BOB's erase only has to take out what it
// changed, so the pictures are the same, with fewer blitter words: the
// blitter is what a busy picture waits for (explosions, smoke, the people).
// The rows skipped at the top go in the template's (unused) destination, as
// a byte offset objectsQueue adds. Plain copies (shots, shells, buildings)
// write their blank rows too, so they stay whole.
__attribute__((noinline, optimize("Os"))) static void bobsCrop(UBYTE b0, UBYTE b1) {
	for(UBYTE b = b0; b < b1; ++b) {
		if(bobIsCopy(b)) continue;
		const tBitMap *pMask = s_pBobMask[b];
		UBYTE ubH = g_pBobH[b];
		UWORD uwBpr = pMask->BytesPerRow, uwRowBytes = (UWORD)(g_pBobW[b] / 8 * FB_BPP);
		for(UBYTE f = 0; f < g_pBobFrames[b]; ++f) {
			UBYTE t = (UBYTE)(s_pTplFirst[b] + f);
			if(t >= TPL_MAX) break;
			const UBYTE *pFrame = pMask->Planes[0] + (ULONG)f * ubH * uwBpr;
			UBYTE ubTop = 0, ubEnd = ubH;
			for(; ubTop < ubH; ++ubTop) {
				const UBYTE *p = pFrame + (ULONG)ubTop * uwBpr;
				UBYTE ubAny = 0;
				for(UWORD i = 0; i < uwRowBytes; ++i) ubAny |= p[i];   // (all planes' rows: interleaved)
				if(ubAny) break;
			}
			if(ubTop == ubH) { ubTop = 0; ubEnd = 1; }   // a blank frame: one row (a size of 0 rows is 1024)
			else {
				for(; ubEnd > ubTop + 1; --ubEnd) {
					const UBYTE *p = pFrame + (ULONG)(ubEnd - 1) * uwBpr;
					UBYTE ubAny = 0;
					for(UWORD i = 0; i < uwRowBytes; ++i) ubAny |= p[i];
					if(ubAny) break;
				}
			}
			if(!ubTop && ubEnd == ubH) continue;
			for(UBYTE v = 0; v < 2; ++v) {
				tAgkBlit *pT = &s_pTpl[t][v];
				ULONG ulSkip = (ULONG)ubTop * uwBpr;
				pT->pA += ulSkip;
				pT->pB += ulSkip;
				pT->uwSize = (UWORD)((((ubEnd - ubTop) * FB_BPP) << 6) | (pT->uwSize & 0x3F));
				pT->pCD = (UBYTE *)((ULONG)ubTop * FB_ROW_BYTES);
			}
		}
	}
}

// The letters of this layout's message lines a BOB at buffer (x, y) covers,
// for redrawing after its erase (msgCoverAll)
static void msgCoverBob(UBYTE ubBfr, WORD x, WORD y, UBYTE b, UBYTE ubFirst, UBYTE ubEnd) {
	// (the erase covers whole words: from the word's start, + 16 px)
	WORD c0 = (WORD)(((x - FB_X0) & ~15) >> 3), c1 = (WORD)((x - FB_X0 + g_pBobW[b] + 15) >> 3);
	if(c0 < 0) c0 = 0;
	if(c1 > MSG_LEN - 1) c1 = MSG_LEN - 1;
	if(c0 > c1) return;
	// columns c0..c1 as bits (word 0: 0-31, word 1: 32-39), once a BOB
	ULONG pCols[2];
	msgColumns(pCols, (UBYTE)c0, (UBYTE)c1);
	s_pMsgAnyCover[ubBfr] = 1;
	for(UBYTE l = ubFirst; l < ubEnd; ++l) {
		if(y < s_pMsgY[l] + 8 && y + g_pBobH[b] > s_pMsgY[l]) {
			s_pMsgCover[ubBfr][l][0] |= pCols[0];
			s_pMsgCover[ubBfr][l][1] |= pCols[1];
			s_pMsgCoverLines[ubBfr] |= (UWORD)(1 << l);
		}
	}
}

// After the picture's letters are typed (msgUpdate): the ones under its BOBs,
// which their erase takes out next time in this buffer. The blitter drew the
// BOBs first, the CPU typed over them, so the letters there now are the ones to
// redraw. Only while the layout's lines have letters: in play they're empty,
// and a jet, its missiles and the damage smoke in the band cost 10% of a frame
__attribute__((noinline)) static void msgCoverAll(UBYTE ubBfr) {
	UBYTE ubLayout = msgLayout();
	UBYTE ubFirst = s_pMsgFirst[ubLayout], ubEnd = s_pMsgEnd[ubLayout];
	ULONG ulInk = 0;
	for(UBYTE l = ubFirst; l < ubEnd; ++l) ulInk |= s_pMsgInk[ubBfr][l][0] | s_pMsgInk[ubBfr][l][1];
	if(!ulInk) return;
	// the objects drawn (objectsQueue's test) across the layout's lines
	WORD wTop = s_pMsgY[ubFirst], wEnd = (WORD)(s_pMsgY[ubEnd - 1] + 8);
	// (from the last object back: in the play layout the lines are above everything on the ground,
	// and logicDraw puts the ground first - structures, pads, the shadow, tanks, the people - and the
	// air after them, so the walk stops at the last person: with 9 of 17 objects people, that
	// was half the 9 raster lines this took in a crash's typing pictures)
	UBYTE isPlay = ubLayout == LAYOUT_PLAY && s_sGame.world != W3_WORLD;   // (world 3 draws its own list: no ground-first order to lean on)
	const tDraw *pD = s_pDraw + s_ubDraws;
	for(UBYTE i = s_ubDraws; i--;) {
		--pD;
		UBYTE b = pD->bob;
		if(isPlay && b == BOB_HOSTAGE) break;
		WORD y = pD->y;
		if(y >= wEnd) continue;
		if(y + g_pBobH[b] <= wTop) continue;
		WORD x = (WORD)(pD->x + FB_X0);
		const tBobInfo *pI = &s_pBobInfo[b];
		if((UWORD)x > pI->uwMaxX || (UWORD)y > pI->uwMaxY) continue;
		msgCoverBob(ubBfr, x, y, b, ubFirst, ubEnd);
	}
}

__attribute__((noinline)) static void erasesQueue(UBYTE ubBfr) {
	agkBlitqReset();
	// (as agkBlitClear + agkBlitqPush, leaner: D only, so a slot needs just
	// BLTCON0/1, D, its modulo and the size - A, B, the masks and the source
	// modulo don't count; the queue is empty, and DRAW_MAX of them fit it)
	_Static_assert(DRAW_MAX < AGK_BLITQ_MAX, "a picture's erases fit the blit queue");
	const tAgkBlitArea *pDrawn = s_pErase[ubBfr];
	tAgkBlit *pQ = g_pAgkBlitq;
	for(UBYTE i = s_pErased[ubBfr]; i--; ++pDrawn, ++pQ) {
		pQ->uwCon0 = 0x0100;                  // USED, minterm 0
		pQ->uwCon1 = 0;
		pQ->pCD = pDrawn->pD;
		pQ->wDstMod = pDrawn->wMod;
		pQ->uwSize = pDrawn->uwSize;
		++g_uwAgkBlitqCount;
		agkBlitqPoll();
	}
}

// Erase areas side by side on the same rows near the ground (the people at a barracks, two
// tanks): one clear of the box around them, a blit the CPU starts instead of up to sixteen
// (each start was ~1 raster line of an A500 frame, and the heaviest pictures draw a crowd).
// The erases all run before any object is drawn, so what the gap between two areas held (an
// object's pixels, erased on its own, or nothing) goes either way: the picture is the same.
// None of the text lives that low (ERASE_MERGE_Y: below the lowest message line), and the
// box is at most ERASE_MERGE_BYTES wide, so a lone far-off object never grows a big clear.
#define ERASE_MERGE_Y (TITLE_TEXT_Y + 46 + 8)
#define ERASE_MERGE_BYTES 24
#define ERASE_MERGE_MIN 4          // fewer people than this: the merge costs more than it saves
#define ERASE_MERGE_ROWS (14 * FB_BPP)
_Static_assert(ERASE_MERGE_Y <= FEET_Y - 14, "tanks and people stand below the lowest message line");
#ifndef OBJ_BLITS_C
UWORD erasesMergeAsm(AGK_REG(tAgkBlitArea *pE, "a0"), AGK_REG(UWORD n, "d0"), AGK_REG(const UBYTE *pFloor, "a1"));   // (src/objblits.s)
#define erasesMerge(pE, n, pFloor) ((UBYTE)erasesMergeAsm(pE, n, pFloor))
#else
static UBYTE erasesMerge(tAgkBlitArea *pE, UBYTE n, const UBYTE *pFloor) {
	if(n < 2) return n;
	tAgkBlitArea *pLast = pE;
	for(UBYTE i = 1; i < n; ++i) {
		const tAgkBlitArea *pA = &pE[i];
		UWORD uwRows = pA->uwSize & 0xFFC0;
		if(uwRows == (pLast->uwSize & 0xFFC0) && uwRows <= (ERASE_MERGE_ROWS << 6) &&
			pA->pD >= pFloor && pLast->pD >= pFloor) {
			// (a different row is a whole row's bytes away: too wide, so not merged)
			UBYTE *pS = pA->pD < pLast->pD ? pA->pD : pLast->pD;
			UBYTE *pEnd = pA->pD + 2 * (pA->uwSize & 63);
			UBYTE *pEndL = pLast->pD + 2 * (pLast->uwSize & 63);
			if(pEndL > pEnd) pEnd = pEndL;
			ULONG ulBytes = (ULONG)(pEnd - pS);
			if(ulBytes <= ERASE_MERGE_BYTES) {
				pLast->pD = pS;
				pLast->uwSize = (UWORD)(uwRows | (ulBytes >> 1));
				pLast->wMod = (WORD)(FB_PLANE_BYTES - ulBytes);
				continue;
			}
		}
		*++pLast = *pA;
	}
	return (UBYTE)(pLast - pE + 1);
}
#endif

__attribute__((noinline)) static void objectsQueue(UBYTE ubBfr) {
	tBitMap *pFb = s_pFb[ubBfr];
	// (the picture the world changes in: the new world's BOBs are loaded at the start of the next)
	UBYTE n = s_sGame.world == s_ubWorld ? logicDraw(&s_sGame, s_pDraw) : 0;
	tAgkBlitArea *pErase = s_pErase[ubBfr];
	tAgkBlitArea *const pErase0 = pErase;
	UBYTE ubDrawn = 0;
	const tDraw *pD = s_pDraw;
	// the lines this state writes in (only their letters need guarding)
	UBYTE ubLayout = msgLayout();
	UBYTE ubMsgFirst = s_pMsgFirst[ubLayout], ubMsgEnd = s_pMsgEnd[ubLayout];
	// what last time's BOBs covered gets erased now: those letters need redrawing
	// (not in the other layout's lines: msgLayoutClear() is about to clear them).
	// Just the columns with letters in them: in play the lines are empty, and a
	// jet or a missile crossing them made msgUpdate retype all four every frame
	// (a quarter of an A500 frame; the skill-5 trip ran at 130-180%)
	if(s_pMsgAnyCover[ubBfr]) {
		s_pMsgAnyCover[ubBfr] = 0;
		ULONG *pCover = s_pMsgCover[ubBfr][0], *pFix = s_pMsgFix[ubBfr][0];
		const ULONG *pInk = s_pMsgInk[ubBfr][0];
		ULONG ulAny = 0;
		// (only the lines that may hold bits: the sixteen lines' covers and fixes cleared or
		// checked each time were 9 raster lines of a typing picture with a missile crossing the text)
		UWORD uwLayout = (UWORD)(((1UL << ubMsgEnd) - 1) & ~((1UL << ubMsgFirst) - 1));
		UWORD uwCov = s_pMsgCoverLines[ubBfr], uwFixOut = (UWORD)(s_pMsgFixLines[ubBfr] & ~uwLayout);
		s_pMsgCoverLines[ubBfr] = 0;
		s_pMsgFixLines[ubBfr] &= uwLayout;
		for(UBYTE l = 0; uwFixOut; ++l, uwFixOut >>= 1) {
			if(uwFixOut & 1) pFix[2 * l] = pFix[2 * l + 1] = 0;   // (the other layouts' lines keep no fix)
		}
		for(UBYTE l = 0; uwCov; ++l, uwCov >>= 1) {
			if(!(uwCov & 1)) continue;
			ULONG *pC = pCover + 2 * l, *pF = pFix + 2 * l;
			if(uwLayout & (1 << l)) {
				ULONG ulFix0 = pC[0] & pInk[2 * l], ulFix1 = pC[1] & pInk[2 * l + 1];
				pF[0] |= ulFix0;
				pF[1] |= ulFix1;
				ulAny |= ulFix0 | ulFix1;
				if(ulFix0 | ulFix1) s_pMsgFixLines[ubBfr] |= (UWORD)(1 << l);
			}
			else pF[0] = pF[1] = 0;
			pC[0] = pC[1] = 0;
		}
		if(ulAny) s_pMsgAnyFix[ubBfr] = 1;
	}
	// The per-object loop: it runs from slow RAM in the 6-plane lines, where
	// the bus is full (bitplanes 55%, the blitter 15%), and the C loop cost ~6
	// raster lines an object: half of a busy A500 frame. Per type, a table made
	// once (bobsCreate) gives the limits and the template; src/objblits.s keeps
	// everything in registers. (OBJ_BLITS_C: the C version it was checked
	// against, every golden picture identical.)
	s_ubDraws = n;
#ifndef OBJ_BLITS_C
	tObjBlits sB;
	sB.pDraw = s_pDraw;
	sB.pQueue = &g_pAgkBlitq[g_uwAgkBlitqCount];
	sB.pNext = &g_pAgkBlitq[g_uwAgkBlitqNext];
	sB.pErase = pErase;
	sB.pFb0 = pFb->Planes[0];
	sB.pInfo = s_pBobInfo;
	sB.uwN = n;
	sB.uwRoom = (UWORD)(AGK_BLITQ_MAX - g_uwAgkBlitqCount);
	objBlitsAsm(&sB);
	g_uwAgkBlitqCount = (UWORD)(AGK_BLITQ_MAX - sB.uwRoom);
	g_uwAgkBlitqNext = (UWORD)(g_uwAgkBlitqNext + sB.uwStarted);
	if(sB.uwDrawn & 0x8000) agkBlitqFull();
	ubDrawn = (UBYTE)sB.uwDrawn;
	(void)pD;
#else
	UBYTE *pFb0 = pFb->Planes[0];
	tAgkBlit *pQ = &g_pAgkBlitq[g_uwAgkBlitqCount];
	for(UBYTE i = n; i--; ++pD) {
		UBYTE b = pD->bob;
		WORD x = pD->x + FB_X0, y = pD->y;
		const tBobInfo *pI = &s_pBobInfo[b];
		// (x < 0 and y < 0 are large unsigned: off too)
		if((UWORD)x > pI->uwMaxX || (UWORD)y > pI->uwMaxY) continue;
		UBYTE ubShift = (UBYTE)(x & 15);
		const tAgkBlit *pT = pI->pTpl + 2 * pD->frame + (ubShift > pI->ubSlack);
		*pQ = *pT;
		UWORD uwShift = (UWORD)ubShift << 12;
		pQ->uwCon0 |= uwShift;   // ASHIFT
		pQ->uwCon1 = uwShift;    // BSHIFT
		UBYTE *pCD = pFb0 + s_pFbRowOffs[y] + ((x >> 4) << 1) + (ULONG)pT->pCD;   // (+ its blank rows: bobsCrop)
		pQ->pCD = pCD;
		pErase->pD = pCD;
		pErase->uwSize = pQ->uwSize;
		pErase->wMod = pQ->wDstMod;
		++pErase;
		if(g_uwAgkBlitqCount < AGK_BLITQ_MAX) {
			++g_uwAgkBlitqCount;
			++pQ;
		}
		else agkBlitqFull();
		agkBlitqPoll();
		++ubDrawn;
	}
#endif
	// the people's erases (logicDraw's run of them: a crowd at a barracks) merged; the rest move up
	UBYTE ubErases = ubDrawn;
	{
		UBYTE ubP0 = g_ubDrawPeople0 < ubDrawn ? g_ubDrawPeople0 : ubDrawn, ubP1 = g_ubDrawPeople1 < ubDrawn ? g_ubDrawPeople1 : ubDrawn;
		if(ubP1 >= ubP0 + ERASE_MERGE_MIN) {
			tAgkBlitArea *pE = pErase0;
			UBYTE ubKept = erasesMerge(pE + ubP0, (UBYTE)(ubP1 - ubP0), pFb->Planes[0] + s_pFbRowOffs[ERASE_MERGE_Y]);
			UBYTE ubGone = (UBYTE)(ubP1 - ubP0 - ubKept);
			if(ubGone) {
				tAgkBlitArea *pTo = pE + ubP0 + ubKept, *pFrom = pE + ubP1;
				for(UBYTE i = ubP1; i < ubDrawn; ++i) *pTo++ = *pFrom++;
				ubErases = (UBYTE)(ubDrawn - ubGone);
			}
		}
	}
	s_pErased[ubBfr] = ubErases;
	s_ubLastDrawn = ubDrawn;
}

// ------------------------------------------------------------------ HUD ---
// Text (agk/text.h): letters in colour 4, shadow in colour 1. The radar is a
// strip of the whole battlefield, 16 world px to a dot, drawn by the CPU.

#define HUD_LEN 40
#define HUD_TYPE_PER_FRAME 8
#define HUD_TYPE_BUSY 3              // ... when the last picture had HUD_BUSY_BOBS objects or more:
#define HUD_BUSY_BOBS 16             // (16 people running into the hangar: the heaviest pictures)
static char s_pHudStage[2][HUD_LEN + 4] __attribute__((aligned(4)));   // (rows of 44: textType reads longs)
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
// The two slots under the line, per buffer ([0] left: HELI / QUOTA, 8 letters;
// [1] right: SKILL / DAMAGED..., 7): typed like the line, in its budget
#define HUD_SLOT_LEN 8
static char s_pHudSlotLine[2][2][HUD_SLOT_LEN + 4] __attribute__((aligned(4)));   // (rows of 12: textType reads longs)
static char s_pHudSlotStage[2][2][HUD_SLOT_LEN + 4] __attribute__((aligned(4)));
static UBYTE s_pHudSlotPending[2][2];
static UBYTE s_pHudDraw[2];
static ULONG s_pHudCols[2][3][2];        // per buffer and text (line, left, right): the columns staged, to draw              // per buffer: texts staged, to draw (hudDraw): 1 the line, 2 left, 4 right

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
		for(UBYTE s = 0; s < 2; ++s) {
			UBYTE ubLen = s ? 7 : 8;
			for(UBYTE c = 0; c < ubLen; ++c) s_pHudSlotStage[b][s][c] = ' ';
			s_pHudSlotStage[b][s][ubLen] = 0;
			s_pHudSlotLine[b][s][ubLen] = 0;
		}
	}
}

// Show szTarget in pText, at most ubBudget changed letters now (the rest
// on later calls: text is slow to draw). Returns 1 when it's all shown.
// One letter of a line drawn, and noted as shown: what agkTextSet() does for
// each letter that differs (the kit's drawChar), without its scan of the whole
// line: msgUpdate and hudDraw know which letters changed (the scans were ~20
// lines of an A500 frame while CHOPPER DOWN showed over explosions). The HUD's
// and the messages' pens (4, shadow 1, 3 planes) as three plain copies
static const UBYTE s_pZero8[8];
__attribute__((noinline)) static void msgChar(tAgkText *pT, UBYTE ubCol, char c) {
	pT->szShown[ubCol] = c;
	const tAgkFont *pFont = pT->pFont;
	UBYTE ubChar = pFont->pIndex[(UBYTE)c & 0x7F];
	UWORD uwStep = pT->uwRowBytes;
	if(pT->ubColor == 4 && pT->ubShadow == 1 && pT->ubDepth == 3) {
		const UBYTE *pS = pFont->pShadow[ubChar], *pL = pFont->pRows[ubChar];
		UBYTE *p0 = pT->pPlane[0] + ubCol, *p1 = pT->pPlane[1] + ubCol, *p2 = pT->pPlane[2] + ubCol;
		for(UBYTE r = 0; r < 8; ++r, p0 += uwStep, p1 += uwStep, p2 += uwStep) {
			*p0 = pS[r];
			*p1 = 0;
			*p2 = pL[r];
		}
		return;
	}
	for(UBYTE p = 0; p < pT->ubDepth; ++p) {
		UBYTE isL = (pT->ubColor >> p) & 1, isS = (pT->ubShadow >> p) & 1;
		const UBYTE *pSrc = isL ? (isS ? pFont->pBoth[ubChar] : pFont->pRows[ubChar])
		                        : (isS ? pFont->pShadow[ubChar] : s_pZero8);
		UBYTE *pD = pT->pPlane[p] + ubCol;
		for(UBYTE r = 0; r < 8; ++r, pD += uwStep) *pD = pSrc[r];
	}
}

// (Only the stage here: hudDraw() draws it later in the picture, *pDraw set.
// Both long-aligned, ubLen letters: four at a time where they match - the
// 40-letter line, compared a letter at a time on every picture of its typing,
// was 14 lines of the A500's frame after a lost chopper.)
static UBYTE textType(char *pStage, const char *szTarget, UBYTE ubLen, UBYTE *pBudget, UBYTE *pDraw, UBYTE ubDrawBit, ULONG *pCols) {
	UBYTE isDone = 1, isChange = 0, ubBudget = *pBudget;
	for(UBYTE c = 0; c < ubLen;) {
		if(*(const tUlongAlias *)&pStage[c] == *(const tUlongAlias *)&szTarget[c]) {
			c += 4;
			continue;
		}
		for(UBYTE e = (UBYTE)(c + 4); c < e && c < ubLen; ++c) {
			if(pStage[c] != szTarget[c]) {
				if(ubBudget) { pStage[c] = szTarget[c]; --ubBudget; isChange = 1; pCols[c >> 5] |= 1UL << (c & 31); }
				else {
					// no letters left and one still differs: not done, whatever is further on
					// (the rest of the line was scanned for nothing, on each picture of a HUD change)
					isDone = 0;
					goto out;
				}
			}
		}
	}
out:
	if(isChange) *pDraw |= ubDrawBit;
	*pBudget = ubBudget;
	return isDone;
}


// The HUD's numbers, right to left: agkTextNumber's digits for less. One DIVU
// a digit while the rest of the value fits a 16-bit quotient (< 655360), two
// above it (agkTextNumber always does two), and none once the rest is 0 (the
// leading zeros). 16 people running into the hangar change three fields and
// the score in one picture (the heaviest one): 20 DIVUs there, ~6 now.
__attribute__((noinline, noclone, optimize("Os"))) static void hudNumber(char *pOut, ULONG ulValue, UBYTE ubDigits) {
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
	static char pLine[2][HUD_LEN + 4] __attribute__((aligned(4)));
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
	// One budget of letters a picture for the whole HUD: the two slots first
	// (the alerts), then the line. (The slots were drawn whole at once: at a lost
	// chopper HELI, SKILL and the line's three numbers changed in one picture,
	// beside the explosions: 15% of an A500 frame)
	UBYTE ubBudget = s_ubLastDrawn >= HUD_BUSY_BOBS ? HUD_TYPE_BUSY : HUD_TYPE_PER_FRAME;
	// the left slot: the choppers left; for a while after the last scoring tank
	// kill of a mission, the quota (later tanks pay nothing)
	// (or a bonus: ZONE CLR when the last radar site fell, DEST DWN when the strait destroyer was disabled)
	UBYTE ubLeft = (UBYTE)(g->lives | (g->quotaMsg ? 0x10 : 0));   // (the quota line's slot also says ZONE CLR and DEST DWN: quotaKind picks the text when it starts)
	if(ubLeft != pLives[ubBfr]) {
		pLives[ubBfr] = ubLeft;
		static const char *const s_pQuotaText[3] = {"QUOTA 10", "ZONE CLR", "DEST DWN"};
		UBYTE isMsg = g->quotaMsg;
		const char *szL = g->quotaMsg ? s_pQuotaText[g->quotaKind] : "HELI 3  ";
		char *pL = s_pHudSlotLine[ubBfr][0];
		for(UBYTE i = 0; i < HUD_SLOT_LEN; ++i) pL[i] = szL[i];
		if(!isMsg) pL[5] = (char)('0' + g->lives);
		s_pHudSlotPending[ubBfr][0] = 1;
	}
	// the right slot: the skill; DAMAGED while the chopper is damaged (until the pad mends it)
	UBYTE isDamaged = g->chop.damaged && g->chop.alive && !g->demo;
	UBYTE ubSkill = (UBYTE)(g->difficulty | (g->demo << 4) | (isDamaged << 5));
	if(ubSkill != pSkill[ubBfr]) {
		pSkill[ubBfr] = ubSkill;
		const char *szS = g->demo ? "   DEMO " : isDamaged ? "DAMAGED " :
			g->difficulty == DIFFICULTY_INVINCIBLE ? "GODMODE " : "SKILL 3 ";
		char *pS = s_pHudSlotLine[ubBfr][1];
		for(UBYTE i = 0; i < HUD_SLOT_LEN; ++i) pS[i] = szS[i];
		pS[HUD_SLOT_LEN - 1] = 0;   // (7 letters)
		if(!g->demo && !isDamaged && g->difficulty != DIFFICULTY_INVINCIBLE) pS[6] = (char)('0' + g->difficulty);
		s_pHudSlotPending[ubBfr][1] = 1;
	}
	UBYTE *pDraw = &s_pHudDraw[ubBfr];
	if(s_pHudSlotPending[ubBfr][0]) {
		s_pHudSlotPending[ubBfr][0] = !textType(s_pHudSlotStage[ubBfr][0], s_pHudSlotLine[ubBfr][0], 8, &ubBudget, pDraw, 2, s_pHudCols[ubBfr][1]);
	}
	if(s_pHudSlotPending[ubBfr][1]) {
		s_pHudSlotPending[ubBfr][1] = !textType(s_pHudSlotStage[ubBfr][1], s_pHudSlotLine[ubBfr][1], 7, &ubBudget, pDraw, 4, s_pHudCols[ubBfr][2]);
	}
	if(s_pHudPending[ubBfr]) {
		s_pHudPending[ubBfr] = !textType(s_pHudStage[ubBfr], szLine, HUD_LEN, &ubBudget, pDraw, 1, s_pHudCols[ubBfr][0]);
	}
}

// The letters hudUpdate() staged this picture, drawn into this buffer's HUD
// (the CPU's, not on screen until the swap): late in the picture, while the
// blitter finishes the objects, rather than at its start, where the objects'
// blits waited for them (the busiest pictures end waiting for the blitter)
__attribute__((noinline)) static void hudDraw(UBYTE ubBfr) {
	UBYTE ubDraw = s_pHudDraw[ubBfr];
	if(!ubDraw) return;
	s_pHudDraw[ubBfr] = 0;
	tAgkText *pTexts[3] = {&s_pHudText[ubBfr], &s_pHudText2[ubBfr], &s_pHudSkill[ubBfr]};
	const char *pStages[3] = {s_pHudStage[ubBfr], s_pHudSlotStage[ubBfr][0], s_pHudSlotStage[ubBfr][1]};
	for(UBYTE t = 0; t < 3; ++t) {
		if(!(ubDraw & (1 << t))) continue;
		ULONG *pCols = s_pHudCols[ubBfr][t];
		UBYTE c = 0;
		for(ULONG m = pCols[0]; m; m >>= 1, ++c) if(m & 1) msgChar(pTexts[t], c, pStages[t][c]);
		c = 32;
		for(ULONG m = pCols[1]; m; m >>= 1, ++c) if(m & 1) msgChar(pTexts[t], c, pStages[t][c]);
		pCols[0] = pCols[1] = 0;
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
static void radarBar(UBYTE *pRow, UBYTE ubX0, UBYTE ubX1);
static UBYTE s_pSinkBar[2];   // the freighter's timer bar as drawn (px) in each HUD buffer

// The timer bar of the sinking freighter (SPEC 11, slice D): the two HUD rows under the radar (22-23), pen 5, ubLen px from the radar's left edge
__attribute__((noinline, optimize("Os"))) static void sinkBarDraw(UBYTE *pRadarTop, UBYTE ubLen, UBYTE ubOld) {
	if(ubLen < ubOld) {
		// the bar only shrinks as the freighter sinks: the pixels it lost are cleared in both rows (the bar redrawn whole was 40 raster lines a picture)
		for(UBYTE r = RADAR_H; r < RADAR_H + 2; ++r) {
			UBYTE *p = pRadarTop + r * HUD_ROW_BYTES;
			for(UBYTE ubByte = (UBYTE)(ubLen >> 3); ubByte <= (UBYTE)((ubOld - 1) >> 3); ++ubByte) {   // (a byte at a time: the bar can drop 100 pixels at a stage)
				UBYTE ubLo = (UBYTE)(ubByte << 3) >= ubLen ? 0 : (UBYTE)(ubLen & 7);   // bits kept on the left
				UBYTE ubHi = (UBYTE)((ubByte << 3) + 8) <= ubOld ? 8 : (UBYTE)(ubOld & 7);   // bits up to here are cleared
				UBYTE ubMask = (UBYTE)~((0xFF >> ubLo) & (0xFF << (8 - ubHi)));
				UBYTE *q = p + ubByte;
				q[0] &= ubMask;
				q[RADAR_P1] &= ubMask;
				q[RADAR_P2] &= ubMask;
			}
		}
		return;
	}
	for(UBYTE r = RADAR_H; r < RADAR_H + 2; ++r) {
		UBYTE *p = pRadarTop + r * HUD_ROW_BYTES;
		for(UBYTE k = 0; k < RADAR_W / 8; ++k) { p[k] = 0; p[RADAR_P1 + k] = 0; p[RADAR_P2 + k] = 0; }
		if(ubLen) radarBar(p, 0, (UBYTE)(ubLen - 1));
	}
}

static UBYTE s_ubRadarHeavy;   // this picture redrew a part of the radar strip (60-90 raster lines): msgUpdate lets its typing wait one picture
static UBYTE s_pRadarZones[2];   // the same for the radar sites and their zones (0: none, world 1; else bit 6, the mission, the sites standing)
static UBYTE s_pRadarBuildings[2] = {0xFF, 0xFF};   // which barracks were open when drawn

// A run of pixels ubX0..ubX1 of the ground line in pen 5, red (planes 0 and 2 set, plane 1 clear: pen 2 was the stone line),
// whole bytes where it can (the strip starts on a byte: RADAR_X0 + 16 is a multiple of 8)
__attribute__((noinline, optimize("Os"))) static void radarBar(UBYTE *pRow, UBYTE ubX0, UBYTE ubX1) {
	for(UWORD x = ubX0; x <= ubX1;) {
		UBYTE *p = pRow + (x >> 3);
		UBYTE ubMask;
		if(!(x & 7) && x + 7 <= ubX1) { ubMask = 0xFF; x += 8; }
		else { ubMask = (UBYTE)(0x80 >> (x & 7)); ++x; }
		p[0] |= ubMask;
		p[RADAR_P1] &= (UBYTE)~ubMask;
		p[RADAR_P2] |= ubMask;
	}
}

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
	UBYTE *pTop = s_pRadarArea[ubBfr].pD;   // (a row is its top plus a constant: no table of the eight)
#define ROW(y) (pTop + (y) * HUD_ROW_BYTES)
	UBYTE isW2 = g->world;   // (its 4608 px at a 24th, not a 16th)
	// the buildings (rows 5-6): only when a barracks has opened since
	_Static_assert(BARRACKS_COUNT == 4, "radarUpdate: the open barracks, unrolled (a picture's cycles count)");
	UBYTE ubOpen = (UBYTE)((g->pBarracks[0].hp == 0) | ((g->pBarracks[1].hp == 0) << 1) | ((g->pBarracks[2].hp == 0) << 2) |
		((g->pBarracks[3].hp == 0) << 3) | (isW2 << 4));
	// (a new mission's barracks are closed again: that redraw waits six pictures of its card, as the sites' below: the picture that starts a mission is the heaviest of the game)
	if(ubOpen != s_pRadarBuildings[ubBfr] && !(g->state == GS_INTRO && g->stateTimer > INTRO_FRAMES - 6)) {
		s_ubRadarHeavy = 1;
		s_pRadarBuildings[ubBfr] = ubOpen;
		s_pRadarZones[ubBfr] = 0xFF;   // (rows 5-6 are cleared: the radar sites' dots are drawn again with the zones)
		for(UBYTE y = 5; y < 7; ++y) {
			UBYTE *p = ROW(y);
			for(UBYTE i = 0; i < FB_BPP * RADAR_P1; i += RADAR_P1)
				for(UBYTE k = 0; k < RADAR_W / 8 + 1; ++k) p[i + k] = 0;
		}
		UBYTE ubBase = isW2 ? logicRadarX2(W2_HANGAR_X) : logicRadarX(HANGAR_X);
		radarDot(ROW(6), ubBase, 7);
		radarDot(ROW(6), (UBYTE)(ubBase + 1), 7);
		if(!isW2) {
			radarDot(ROW(5), logicRadarX(BORDER_X), 3);
			radarDot(ROW(6), logicRadarX(BORDER_X), 3);
		}
		for(UBYTE b = 0; b < BARRACKS_COUNT; ++b) {
			UBYTE x = isW2 ? logicRadarX2(g->pBarracks[b].x) : logicRadarX(g->pBarracks[b].x);
			UBYTE pen = (ubOpen >> b) & 1 ? 5 : 7;
			radarDot(ROW(5), x, pen);
			radarDot(ROW(5), (UBYTE)(x + 1), pen);
			radarDot(ROW(6), x, pen);
			radarDot(ROW(6), (UBYTE)(x + 1), pen);
		}
	}
	// World 2's radar sites (missions 5-6): each standing site a white dot (pen 3, two wide, row 6) and, on the ground line (row 7,
	// pen 2), each live jet zone in red (pen 5), cut where the base's approach begins. Drawn when a site falls or the mission changes,
	// not in the first pictures of a card: the picture that starts a mission is the heaviest of the game, and these a few pictures
	// later are not seen to lag
	UBYTE ubZoneKey = 0;   // (world 1: nothing to draw, one test a picture)
	if(isW2) {
		ubZoneKey = (UBYTE)(0x40 | ((g->mission + 1) << 3));
		for(UBYTE i = 0; i < g->radarCount; ++i) ubZoneKey |= (UBYTE)(g->pRadars[i].alive << i);
		UBYTE ubBar = logicSinkBar(g);   // (mission 6: the freighter's time left, 0 when there is none or nobody aboard)
		if(ubBar != s_pSinkBar[ubBfr] && !(g->state == GS_INTRO && g->stateTimer > INTRO_FRAMES - 6)) {   // (not in the first pictures of the card, as the sites')
			UBYTE ubOldBar = s_pSinkBar[ubBfr];
			if(ubBar > ubOldBar) s_ubRadarHeavy = 1;   // (a shrinking bar is two pixels' work)
			s_pSinkBar[ubBfr] = ubBar;
			sinkBarDraw(pTop, ubBar, ubOldBar);
		}
	}
	if(ubZoneKey != s_pRadarZones[ubBfr] && !(g->state == GS_INTRO && g->stateTimer > INTRO_FRAMES - 6)) {
		s_ubRadarHeavy = 1;
		s_pRadarZones[ubBfr] = ubZoneKey;
		for(UBYTE i = 0; i < g->radarCount; ++i) {
			UBYTE x = logicRadarX2(g->pRadars[i].x);
			for(UBYTE d = 0; d < 2; ++d) {
				UWORD uwX = (UWORD)(((RADAR_X0 + 16) & 7) + x + d);
				UBYTE *pD = ROW(6) + (uwX >> 3);
				UBYTE ubBit = (UBYTE)(0x80 >> (uwX & 7));
				pD[0] &= (UBYTE)~ubBit; pD[RADAR_P1] &= (UBYTE)~ubBit; pD[RADAR_P2] &= (UBYTE)~ubBit;
				if(g->pRadars[i].alive) radarDot(ROW(6), (UBYTE)(x + d), 3);
			}
		}
		UBYTE *p = ROW(RADAR_H - 1);
		for(UBYTE k = 0; k < RADAR_W / 8; ++k) { p[k] = 0; p[RADAR_P1 + k] = 0xFF; p[RADAR_P2 + k] = 0; }
		tZone sZone[RADAR_MAX];
		for(UBYTE i = 0, n = logicRadarZones(g, sZone, 0); i < n; ++i) radarBar(p, logicRadarX2(sZone[i].lo), logicRadarX2(sZone[i].hi));
	}
	UBYTE ubCam = isW2 ? logicRadarX2(g->cam) : logicRadarX(g->cam);
	radarDot(ROW(0), ubCam, 3);
	radarDot(ROW(0), (UBYTE)(ubCam + (isW2 ? SCREEN_W / 24 : SCREEN_W / 16)), 3);
	UBYTE *pPeople = ROW(3) + RADAR_P2;   // pen 4: plane 3 only
	for(UBYTE k = 0; k < g->outCount; ++k) {
		const tHostage *h = &g->pHostages[g->pOut[k]];
		if(h->state != HS_DEAD) {
			UWORD uwX = (UWORD)(((RADAR_X0 + 16) & 7) + (isW2 ? logicRadarX2(h->x) : logicRadarX(h->x)));
			pPeople[uwX >> 3] |= (UBYTE)(0x80 >> (uwX & 7));
		}
	}
	for(UBYTE i = 0; !isW2 && i < TANK_MAX; ++i) {
		if(g->pTanks[i].active) radarDot(ROW(4), logicRadarX(g->pTanks[i].x), 5);
	}
	for(UBYTE i = 0; isW2 && i < BOAT_MAX; ++i) {
		const tBoat *b = &g->pBoats[i];   // (world 2's boats: a gunboat red, a patrol boat stone)
		if(b->active) radarDot(ROW(4), logicRadarX2(b->x), b->kind == BOAT_GUN ? 5 : 3);
	}
	WORD wRadarW = isW2 ? W2_W : WORLD_W;   // (world 2's jets: the same blips on its 4608 px)
	for(UBYTE i = 0; i < JET_MAX; ++i) {
		const tJet *pJet = &g->pJets[i];
		WORD x = pJet->x;
		if(!pJet->active) continue;
		if(pJet->warn) {
			// the incoming-jet warning (its first 2 s): the blip, wider and a row taller, flashes white,
			// and holds at the radar's edge while the jet is still beyond the battlefield
			x = x < 0 ? 0 : x >= wRadarW ? wRadarW - 1 : x;
			if(g->frame & 8) {
				UBYTE ubX = isW2 ? logicRadarX2(x) : logicRadarX(x);
				radarDot(ROW(1), ubX, 7);
				radarDot(ROW(2), ubX, 7);
				radarDot(ROW(1), (UBYTE)(ubX + 1), 7);
				radarDot(ROW(2), (UBYTE)(ubX + 1), 7);
				continue;
			}
		}
		if(x >= 0 && x < wRadarW) radarDot(ROW(1), isW2 ? logicRadarX2(x) : logicRadarX(x), 5);
	}
	for(UBYTE i = 0; !isW2 && i < MINE_MAX; ++i) {
		WORD x = g->pMines[i].x;
		if(g->pMines[i].active && x >= 0 && x < WORLD_W) radarDot(ROW(1), logicRadarX(x), 3);
	}
	if(!logicIsMenu(g) && g->state != GS_ENTRY) {
		UBYTE x = isW2 ? logicRadarX2(g->chop.x) : logicRadarX(g->chop.x);
		UBYTE y = (UBYTE)((g->chop.y - CEIL_Y) >> 5);   // 0..6
		if(y > RADAR_DYN_H - 1) y = RADAR_DYN_H - 1;
		radarDot(ROW(y), x, 6);
		radarDot(ROW(y), (UBYTE)(x + 1), 6);
	}
}
#undef ROW

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
	s_pMsgInk[ubBfr][l][0] = s_pMsgInk[ubBfr][l][1] = 0;
	s_pMsgFix[ubBfr][l][0] = s_pMsgFix[ubBfr][l][1] = 0;
	s_pMsgCover[ubBfr][l][0] = s_pMsgCover[ubBfr][l][1] = 0;
}

static void msgCentre(char *szOut, const char *szText) {
	UBYTE ubLen = 0;
	while(szText[ubLen] && ubLen < MSG_LEN) ++ubLen;
	UBYTE ubPad = (UBYTE)((MSG_LEN - ubLen) >> 1);
	for(UBYTE i = 0; i < MSG_LEN; i += 4) *(tUlongAlias *)&szOut[i] = 0x20202020UL;   // (szOut: long-aligned)
	for(UBYTE i = 0; i < ubLen; ++i) szOut[ubPad + i] = szText[i];
	szOut[MSG_LEN] = 0;
}

// A hall of fame line (the kit's hiRow): "- 1  ABC  012345  MSN 2  CAPTAIN  -",
// the new entry between dashes. The mission it reached and the rank its
// hostages saved earned are this game's two words.
__attribute__((noinline, optimize("Os"))) static void scoreRow(char *p, UBYTE i) {
	const tHiScore *s = &s_sGame.hi.pEntries[i];
	char szReached[HI_REACHED_W + 1] = "ALL";
	if(s->reached <= MISSIONS_TOTAL) {
		szReached[0] = 'M'; szReached[1] = 'S'; szReached[2] = 'N'; szReached[3] = ' ';
		szReached[4] = (char)('0' + s->reached);
		szReached[5] = 0;
	}
	hiRow(p, &s_sGame.hi, i, i == s_sGame.newRank, szReached, logicRank(s->extra));
}

// The hall of fame's and the initials' lines; returns the low bits of the key
__attribute__((noinline, optimize("Os"))) static UWORD msgMenu(const char **pText, char (*pRows)[HI_ROW_LEN + 1], char *szA, char *szB) {
	const tGame *g = &s_sGame;
	if(g->state == GS_SCORES) {
		// The rows (12 DIVUs each, for the score) made again only when the
		// table or the new entry changes: every picture of the table's typing
		// they were a fifth of an A500 frame
		static tHiTable sRowsOf;
		static UBYTE ubRowsRank = 0xFF, isRows;
		UBYTE isSame = isRows && ubRowsRank == g->newRank;
		const UBYTE *pNow = (const UBYTE *)&g->hi, *pThen = (const UBYTE *)&sRowsOf;
		for(UWORD i = 0; i < sizeof(sRowsOf) && isSame; ++i) isSame = pNow[i] == pThen[i];
		if(!isSame) {
			for(UBYTE i = 0; i < HI_COUNT; ++i) scoreRow(pRows[i], i);
			sRowsOf = g->hi;
			ubRowsRank = g->newRank;
			isRows = 1;
		}
		pText[MSG_SCORES] = "HALL OF FAME";
		for(UBYTE i = 0; i < HI_COUNT; ++i) pText[MSG_SCORES + 1 + i] = pRows[i];
		pText[MSG_SCORES + 6] = g->newRank < HI_COUNT ? hiDiskText(s_ubDiskStatus) : "";
		return (UWORD)(s_ubDiskStatus << 1);
	}
	// the initials: typed, or up/down a letter and fire the next; the current one blinking
	const tHiScore *s = &g->hi.pEntries[g->newRank];
	const char *szR = "RANK 1   000000";
	for(UBYTE i = 0; i < 16; ++i) szB[i] = szR[i];
	szB[5] = (char)('1' + g->newRank);
	hiNumber(&szB[9], s->score, 6);
	pText[0] = "GREAT SCORE!";
	pText[1] = szB;
	pText[2] = "ENTER YOUR INITIALS";
	UBYTE isBlink = (g->frame >> 4) & 1;
	for(UBYTE c = 0; c < HI_NAME; ++c) {
		szA[2 * c] = (c == g->edit.ubPos && isBlink) ? '-' : s->name[c];
		szA[2 * c + 1] = ' ';
	}
	szA[2 * HI_NAME - 1] = 0;
	pText[3] = szA;
	// (the key changes with any letter, the cursor or the blink)
	return (UWORD)(0x8000 | ((g->edit.ubPos & 3) << 13) | (isBlink << 12) |
		((s->name[0] * 37 + s->name[1] * 11 + s->name[2]) & 0xFFF));
}

// What the message lines say, as a number: the same key means the same text
// (the initials: any letter, the cursor, the blink; the table: the disk status)
static UWORD msgKeyNow(void) {
	const tGame *g = &s_sGame;
	if(g->state == GS_ENTRY) {
		const tHiScore *s = &g->hi.pEntries[g->newRank];
		UBYTE isBlink = (g->frame >> 4) & 1;
		return (UWORD)(0x8000 | ((g->edit.ubPos & 3) << 13) | (isBlink << 12) |
			((s->name[0] * 37 + s->name[1] * 11 + s->name[2]) & 0xFFF));
	}
	UWORD uwKeyLow = g->state == GS_SCORES ? (UWORD)(s_ubDiskStatus << 1) : 0;
	return (UWORD)(g->state << 8 | g->world << 6 | g->mission << 4 | uwKeyLow |
		(g->state == GS_TITLE ? (g->frame >> 5) & 1 : 0) |
		(g->state == GS_INTRO && logicJetsPossible(g) ? 1 : 0) |   // (the card's brief follows the skill: "AND JETS")
		(g->hint ? logicHintKey(g) : 0) |   // (in play: the bomb prompt, 0-3; its two wordings alternate)
		(g->state == GS_LOST && g->stateTimer < LOST_FRAMES - 40 ? 2 : 0));
}

__attribute__((noinline)) static void msgUpdateBody(UBYTE ubBfr, UWORD uwKey, UBYTE ubSpent);

// Typing text over many pictures (a lost chopper's lines, the hall of fame): the body sets it all
// up again each picture (16 line pointers, the state's strings, each centred line) to find two
// letters to draw: ~3/4 of the CPU of a typing picture, the heaviest ones of a crash. In the
// states whose texts do not change under one key the body keeps each line's centred text (or
// that it is blank) as it first needs it; the next pictures walk the lines here, as the body
// does, and hand over to it (with the letters still to type) when a line has no plan yet or a
// blank line has letters on it to clear. New key, or a letter an erase crossed: the body's.
static UBYTE s_pPlanOn[2];                      // the plan below is for s_pMsgTypeKey
static UWORD s_pPlanHave[2], s_pPlanBlank[2];   // per line (a bit): its text is planned / planned blank
static char s_pPlan[2][MSG_LINES][MSG_LEN + 4] __attribute__((aligned(4)));
static UBYTE s_pPlanCol[2][MSG_LINES];          // per line: typed up to here (a multiple of 4): where the scan resumes
static void msgBodyFrom(UBYTE ubBfr, UWORD uwKey, UBYTE ubSpent);
static const char s_szSpaces[MSG_LEN + 4] __attribute__((aligned(4))) = {[0 ... MSG_LEN + 2] = ' '};
__attribute__((noinline)) static UBYTE msgTypeFast(UBYTE ubBfr, UWORD uwKey) {
	if(!s_pPlanOn[ubBfr] || uwKey != s_pMsgTypeKey[ubBfr]) return 0;
	UBYTE ubLayout = msgLayout();
	UBYTE ubBudget = MSG_TYPE_PER_FRAME;
	for(UBYTE l = s_pMsgFirst[ubLayout]; l < s_pMsgEnd[ubLayout] && ubBudget; ++l) {
		UWORD uwLineBit = (UWORD)(1 << l);
		if(s_pMsgDone[ubBfr] & uwLineBit) continue;
		ULONG *pInk = s_pMsgInk[ubBfr][l];
		const char *pLine;
		if(s_pPlanBlank[ubBfr] & uwLineBit) {
			if(!(pInk[0] | pInk[1])) {
				s_pMsgDone[ubBfr] |= uwLineBit;
				continue;
			}
			pLine = s_szSpaces;   // (a line to be blank with letters on it: spaces typed over them, as the body does)
		}
		else if(!(s_pPlanHave[ubBfr] & uwLineBit)) { msgBodyFrom(ubBfr, uwKey, (UBYTE)(MSG_TYPE_PER_FRAME - ubBudget)); return 1; }
		else pLine = s_pPlan[ubBfr][l];
		char *pStage = s_pMsgStage[ubBfr][l];
		tAgkText *pT = &s_pMsg[ubBfr][l];
		UBYTE c = s_pPlanCol[ubBfr][l], isChange = 0;
		while(c < MSG_LEN && ubBudget) {
			if(*(const tUlongAlias *)&pStage[c] == *(const tUlongAlias *)&pLine[c]) {
				c += 4;
				continue;
			}
			for(UBYTE e = (UBYTE)(c + 4); c < e && ubBudget; ++c) {
				if(pStage[c] != pLine[c]) {
					pStage[c] = pLine[c];
					ULONG ulBit = 1UL << (c & 31);
					if(pLine[c] != ' ') pInk[c >> 5] |= ulBit;
					else pInk[c >> 5] &= ~ulBit;
					msgChar(pT, c, pLine[c]);
					--ubBudget;
					isChange = 1;
				}
			}
		}
		if(c >= MSG_LEN) s_pMsgDone[ubBfr] |= uwLineBit;
		else s_pPlanCol[ubBfr][l] = (UBYTE)(c & ~3);
		if(isChange) s_pMsgLit[ubBfr] |= uwLineBit;
	}
	s_pMsgKey[ubBfr] = ubBudget ? uwKey : 0xFFFF;
	return 1;
}

// A settled key (everything typed) and letters an erase crossed - an explosion over a line in play, a
// missile across the typed prompt: those letters drawn again and nothing else, which is all the body does
// then after its set-up (sixteen text pointers, the state's switch, the settings of four lines: 18-20
// raster lines of the barrage's pictures in world 2). Only the lines flagged in s_pMsgFixLines.
__attribute__((noinline)) static void msgFixOnly(UBYTE ubBfr) {
	UBYTE ubLayout = msgLayout();
	UWORD uwLayout = (UWORD)(((1UL << s_pMsgEnd[ubLayout]) - 1) & ~((1UL << s_pMsgFirst[ubLayout]) - 1));
	UWORD uwLines = (UWORD)(s_pMsgFixLines[ubBfr] & uwLayout);
	s_pMsgFixLines[ubBfr] &= (UWORD)~uwLayout;
	s_pMsgAnyFix[ubBfr] = 0;
	for(UBYTE l = 0; uwLines; ++l, uwLines >>= 1) {
		if(!(uwLines & 1)) continue;
		const char *pStage = s_pMsgStage[ubBfr][l];
		ULONG *pFix = s_pMsgFix[ubBfr][l];
		const ULONG *pInk = s_pMsgInk[ubBfr][l];
		tAgkText *pT = &s_pMsg[ubBfr][l];
		ULONG ulFix0 = pFix[0] & pInk[0], ulFix1 = pFix[1] & pInk[1];
		pFix[0] = pFix[1] = 0;
		if(!(ulFix0 | ulFix1)) continue;
		UBYTE c = 0;
		for(ULONG m = ulFix0; m; m >>= 1, ++c) if(m & 1) msgChar(pT, c, pStage[c]);
		c = 32;
		for(ULONG m = ulFix1; m; m >>= 1, ++c) if(m & 1) msgChar(pT, c, pStage[c]);
		s_pMsgLit[ubBfr] |= (UWORD)(1 << l);
	}
}

__attribute__((noinline)) static void msgUpdate(UBYTE ubBfr) {
	UWORD uwKey = msgKeyNow();
	if(uwKey == s_pMsgKey[ubBfr] && !s_pMsgAnyFix[ubBfr]) return;
	if(s_ubRadarHeavy) {
		// The radar's redraw (a card's seventh and eighth pictures, a barracks opening) and two letters of text
		// in one picture were the mission start's heaviest (w2barrage, w2dest, w2frtr 98-102 %): the letters wait
		// one picture on a mission card (nothing was erased in this one: letters crossed by an object are still done now)
		s_ubRadarHeavy = 0;
		if(s_sGame.state == GS_INTRO && !s_pMsgAnyFix[ubBfr]) return;   // (the cards: the title's and the play states' text keeps its pictures)
	}
	if(uwKey == s_pMsgKey[ubBfr] && uwKey == s_pMsgTypeKey[ubBfr]) {   // (settled, so a fix is what is wanted)
		msgFixOnly(ubBfr);
		return;
	}
	if(uwKey != s_pMsgKey[ubBfr] && !s_pMsgAnyFix[ubBfr] &&
		((s_sGame.state == GS_PLAY && !s_sGame.hint) || (s_sGame.state == GS_LOST && s_sGame.stateTimer >= LOST_FRAMES - 40))) {   // (a bomb prompt is a key with something to say)
		// A new key with nothing to say (play starting, the 40 pictures after a lost chopper before
		// its text) over lines with no letters: settled at once, which is all the body would do
		// after its set-up (the heavy picture of a crash)
		const ULONG *pInk = s_pMsgInk[ubBfr][0];   // lines 0-3: the play layout's
		if(!(pInk[0] | pInk[1] | pInk[2] | pInk[3] | pInk[4] | pInk[5] | pInk[6] | pInk[7])) {
			s_pMsgTypeKey[ubBfr] = uwKey;
			s_pMsgDone[ubBfr] = 0xFFFF;
			s_pMsgKey[ubBfr] = uwKey;
			s_pPlanOn[ubBfr] = 0;
			return;
		}
	}
	if(!s_pMsgAnyFix[ubBfr]) {
		if(msgTypeFast(ubBfr, uwKey)) return;
	}
	else if(uwKey == s_pMsgTypeKey[ubBfr] && s_pPlanOn[ubBfr]) {
		// letters crossed while the text is still typing out (a missile over a prompt): the crossed ones
		// first, then the typing, both from the plan: the body's set-up was 25-30 raster lines a picture
		msgFixOnly(ubBfr);
		if(msgTypeFast(ubBfr, uwKey)) return;
	}
	msgUpdateBody(ubBfr, uwKey, 0);
}
static void msgBodyFrom(UBYTE ubBfr, UWORD uwKey, UBYTE ubSpent) {
	msgUpdateBody(ubBfr, uwKey, ubSpent);
}

__attribute__((noinline)) static void msgUpdateBody(UBYTE ubBfr, UWORD uwKey, UBYTE ubSpent) {
	const tGame *g = &s_sGame;
	// (the texts of these states do not change under one key: lines are planned for msgTypeFast)
	UBYTE isPlan = g->state == GS_LOST || g->state == GS_CLEAR || g->state == GS_OVER || g->state == GS_VICTORY || g->state == GS_PLAY;   // (play: the bomb prompt, one line; not the hall of fame: seven lines planned at its first picture cost it 10 points)
	if(!isPlan) s_pPlanOn[ubBfr] = 0;
	else if(uwKey != s_pMsgTypeKey[ubBfr] || !s_pPlanOn[ubBfr]) {
		s_pPlanOn[ubBfr] = 1;
		s_pPlanHave[ubBfr] = s_pPlanBlank[ubBfr] = 0;
		for(UBYTE l = 0; l < MSG_LINES; ++l) s_pPlanCol[ubBfr][l] = 0;
	}
	const char *pText[MSG_LINES] = {"", "", "", "", "", "", "", "", "", "", "", "", "", "", "", ""};
	char szA[24], szB[28];
	static char pRows[HI_COUNT][HI_ROW_LEN + 1];   // (msgMenu keeps them while the table stays)
	UBYTE isSettled = uwKey == s_pMsgKey[ubBfr];
	if(!isSettled) switch(g->state) {
		case GS_SCORES:
		case GS_ENTRY:
			msgMenu(pText, pRows, szA, szB);
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
			if(g->world == W3_WORLD) {
				// world 3, the pilot (SPEC 13.7): only mission 7's first screens are built; 8 and 9 show their palettes on the same map
				static const char *const pW3Name[MISSION_COUNT] = {"FOOTHILLS", "HIGH PASS", "CLIFF LEDGES"};
				szA[8] = (char)('7' + g->mission);
				szA[9] = 0;
				pText[1] = pW3Name[g->mission];
				pText[2] = "HARDWARE PILOT: THE TERRAIN STRIP";
			}
			else if(g->world) {
				// world 2: its islands (the strip map goes between lines 1 and 2, as world 1's)
				static const char *const pW2Name[MISSION_COUNT] = {"REEF RUN", "TRADE WINDS", "TYPHOON STRAIT"};
				szA[8] = (char)('4' + g->mission);
				szA[9] = 0;
				pText[1] = pW2Name[g->mission];
				// (mission 5: three islands and a ship; mission 6: the freighter's timer, the skill in force at the card)
				static const char *const pSink[DIFFICULTY_MAX + 1] = {"FREIGHTER SINKS IN 90 SECONDS", "FREIGHTER SINKS IN 120 SECONDS",
					"FREIGHTER SINKS IN 105 SECONDS", "FREIGHTER SINKS IN 90 SECONDS", "FREIGHTER SINKS IN 75 SECONDS", "FREIGHTER SINKS IN 60 SECONDS"};
				pText[2] = g->mission == 0 ? "64 HELD ON 4 ISLANDS" : g->mission == 1 ? "48 ON 3 ISLANDS, 16 ON A SHIP" : pSink[g->difficulty];
			}
		} break;
		case GS_PLAY:
			pText[0] = logicHintText(logicHintKey(g));   // the first-use bomb prompt (SPEC 3): empty most of the time
			break;
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
	UBYTE ubLayout = msgLayout();
	if(!isSettled && !s_pMsgAnyFix[ubBfr]) {
		// (a new key whose lines are all blank, on lines already blank - play
		// starting, a chopper lost: settled at once)
		UBYTE isBlank = 1;
		for(UBYTE l = s_pMsgFirst[ubLayout]; l < s_pMsgEnd[ubLayout] && isBlank; ++l) {
			isBlank = !pText[l][0] && !(s_pMsgInk[ubBfr][l][0] | s_pMsgInk[ubBfr][l][1]);
		}
		if(isBlank) {
			s_pMsgTypeKey[ubBfr] = uwKey;
			s_pMsgDone[ubBfr] = 0xFFFF;
			s_pMsgKey[ubBfr] = uwKey;
			return;
		}
	}
	s_pMsgAnyFix[ubBfr] = 0;
	if(uwKey != s_pMsgTypeKey[ubBfr]) {
		// new text: every line to check again
		s_pMsgTypeKey[ubBfr] = uwKey;
		s_pMsgDone[ubBfr] = 0;
	}
	char szLine[MSG_LEN + 4] __attribute__((aligned(4)));
	if(isPlan && !isSettled) {
		// every line's text for msgTypeFast, at the first picture of the key (the picture is a light
		// one: the typing starts; a later line's first picture was the heavy one otherwise)
		for(UBYTE l = s_pMsgFirst[ubLayout]; l < s_pMsgEnd[ubLayout]; ++l) {
			UWORD uwBit = (UWORD)(1 << l);
			if((s_pPlanHave[ubBfr] | s_pPlanBlank[ubBfr]) & uwBit) continue;
			if(!pText[l][0]) s_pPlanBlank[ubBfr] |= uwBit;
			else {
				msgCentre(s_pPlan[ubBfr][l], pText[l]);
				s_pPlanHave[ubBfr] |= uwBit;
			}
		}
	}
	UBYTE ubBudget = (UBYTE)(MSG_TYPE_PER_FRAME - ubSpent);
	// (just this layout's lines: msgLayoutClear() blanks the others)
	for(UBYTE l = s_pMsgFirst[ubLayout]; l < s_pMsgEnd[ubLayout]; ++l) {
		char *pStage = s_pMsgStage[ubBfr][l];
		ULONG *pFix = s_pMsgFix[ubBfr][l];
		ULONG *pInk = s_pMsgInk[ubBfr][l];
		tAgkText *pT = &s_pMsg[ubBfr][l];
		UBYTE isChange = 0;
		UWORD uwLineBit = (UWORD)(1 << l);
		if(ubBudget && !isSettled && !(s_pMsgDone[ubBfr] & uwLineBit)) {
			// (no letters left to type this picture: the later lines wait, unread.
			// Settled, everything is typed: only the fixes below. A line typed
			// out under this key, or blank and to stay blank, isn't read again:
			// centring and comparing all four lines a picture was 15-20% of an
			// A500 frame at a state change, beside the explosions)
			UBYTE c = 0;
			if(!pText[l][0] && !(pInk[0] | pInk[1])) c = MSG_LEN;
			else msgCentre(szLine, pText[l]);
			while(c < MSG_LEN && ubBudget) {
				// four letters at a time where nothing changes
				if(*(const tUlongAlias *)&pStage[c] == *(const tUlongAlias *)&szLine[c]) {
					c += 4;
					continue;
				}
				for(UBYTE e = (UBYTE)(c + 4); c < e && ubBudget; ++c) {
					if(pStage[c] != szLine[c]) {
						pStage[c] = szLine[c];
						ULONG ulBit = 1UL << (c & 31);
						if(szLine[c] != ' ') pInk[c >> 5] |= ulBit;
						else pInk[c >> 5] &= ~ulBit;
						msgChar(pT, c, szLine[c]);
						--ubBudget;
						isChange = 1;
					}
				}
			}
			if(c >= MSG_LEN) s_pMsgDone[ubBfr] |= uwLineBit;
		}
		// letters an erase crossed: draw again (only letters: spaces would cut
		// into the BOB; objectsQueue keeps just the inked columns)
		ULONG ulFix0 = pFix[0] & pInk[0], ulFix1 = pFix[1] & pInk[1];
		pFix[0] = pFix[1] = 0;
		if(ulFix0 | ulFix1) {
			// (the set bits only, the mask shifted along)
			UBYTE c = 0;
			for(ULONG m = ulFix0; m; m >>= 1, ++c) if(m & 1) msgChar(pT, c, pStage[c]);
			c = 32;
			for(ULONG m = ulFix1; m; m >>= 1, ++c) if(m & 1) msgChar(pT, c, pStage[c]);
			isChange = 1;
		}
		if(isChange) s_pMsgLit[ubBfr] |= uwLineBit;
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

// World 2's map differs from mission to mission (the jet zones: a red line in row 1, the radar sites: a tick in row 2, mission 5's ship
// deck in place of a hut). Drawing a map row by row costs a quarter of an A500 frame in the 6-plane display, so at the world's load (a
// frame nothing is measured) the map is made for each of the world's three missions (960 bytes each), and a mission's card shows its
// own (routeShow): nothing drawn at the card
#define ROUTE_IMG_LONGS (ROUTE_H * FB_BPP * ROUTE_ROW_BYTES / 4)
static ULONG s_pRouteOver[MISSION_COUNT][ROUTE_IMG_LONGS];

// At startup (after logicInit), and at a world's load (worldLoad: making it takes a couple of
// frames, which a card's picture can't spare): a map, from its marks, into an image
__attribute__((noinline, optimize("Os"))) static void routeDraw(UBYTE *pTop, const tRouteMark *pMarks, UBYTE n) {
	// a dark panel (pen 1: plane 0 set, the others clear), longs a row of a plane (no divides:
	// the map is made at a world 2 card too)
	ULONG *pL = (ULONG *)pTop;
	for(UBYTE y = 0; y < ROUTE_H; ++y) {
		for(UBYTE k = 0; k < ROUTE_ROW_BYTES / 4; ++k) *pL++ = 0xFFFFFFFFUL;
		for(UBYTE k = 0; k < 2 * ROUTE_ROW_BYTES / 4; ++k) *pL++ = 0;
	}
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
			case ROUTE_ISLAND:                                  // world 2: land, olive, on the sea
				routeBox(pTop, x, m->w, 8, 9, 2, 0);
				break;
			case ROUTE_ZONE:                                    // world 2, missions 5-6: a jet zone, a red line over the islands
				routeBox(pTop, x, m->w, 1, 1, 5, 0);
				break;
			case ROUTE_RADAR:                                   // ... and a radar site's tick
				routeBox(pTop, x - 1, 3, 2, 2, 4, 0);
				break;
			case ROUTE_DECK:                                    // mission 5's ship deck: a blue hull on the sea, a cream house aft
				routeBox(pTop, x, m->w, 8, 9, 7, 0);
				routeBox(pTop, x + m->w - 3, 2, 6, 7, 4, 0);
				routeBox(pTop, x + 2, 1, 7, 7, 3, 0);
				break;
			case ROUTE_HERE:                                    // you are here: a yellow arrow
				routeBox(pTop, x - 2, 5, 0, 0, 6, 0);
				routeBox(pTop, x - 1, 3, 1, 1, 6, 0);
				routeBox(pTop, x, 1, 2, 3, 6, 0);
				break;
		}
	}
}

__attribute__((noinline, optimize("Os"))) static void routeCreate(void) {
	tRouteMark pMarks[ROUTE_MARKS];
	if(logicIsW2(&s_sGame)) {
		for(UBYTE m = 0; m < MISSION_COUNT; ++m) routeDraw((UBYTE *)s_pRouteOver[m], pMarks, logicRouteMapFor(&s_sGame, m, pMarks));
	}
	else routeDraw(s_pRouteImg, pMarks, logicRouteMap(&s_sGame, pMarks));
}

// The card's first picture in a buffer (the blitter done): the map copied in
__attribute__((noinline)) static void routeShow(UBYTE ubBfr) {
	// (world 2, once its art is loaded: this mission's map, made at the load)
	const ULONG *pImg = logicIsW2(&s_sGame) && s_sGame.world == s_ubWorld ? s_pRouteOver[s_sGame.mission] : (const ULONG *)s_pRouteImg;
	UBYTE *pRow = s_pFb[ubBfr]->Planes[0] + s_pFbRowOffs[ROUTE_Y] + (FB_X0 + ROUTE_X0) / 8;
	for(UBYTE y = 0; y < ROUTE_H * FB_BPP; ++y, pRow += FB_PLANE_BYTES) {
		const ULONG *pSrc = pImg + y * (ROUTE_ROW_BYTES / 4);
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
// Every frame's sprite rows are in one chip block from the floppy (the art's
// block: frame, part, ART_CHOPPER_H rows of 2 words, no control words), loaded
// during the splash. Sprite DMA reads small blocks: per copper buffer and rotor
// phase the four parts one after another, each its control words, its rows
// and two end words. The rotor phase alternates every 2 pictures, so each
// buffer holds the pose in both phases (frames 2k and 2k + 1, next to each
// other in the store as their blocks are next to each other): one blit, queued
// with the BOBs, only when that buffer's pose changes, not every picture
// (every picture cost 1% of a frame: the CPU is slow in the 6-plane display).
// Per picture the back buffer's phase block gets the new position (its
// control words) and the back copper list points channels 0-3 at it. Two
// resident sets of every frame, as before, cost 22.5 KB of chip RAM; this is
// 10.0 KB (the store, in the art's block) + 1.4 KB.
#define CHOP_PART_BYTES ((ART_CHOPPER_H + 2) * 4)   // control words, rows, end words
#define CHOP_DMA_BYTES (ART_CHOPPER_PARTS * CHOP_PART_BYTES)
#define CHOP_FRAME_BYTES (ART_CHOPPER_PARTS * ART_CHOPPER_H * 4)
_Static_assert(DISK_ART_CHOPPER_BYTES == ART_CHOPPER_FRAMES * CHOP_FRAME_BYTES, "diskart.bin's chopper layout");
_Static_assert(!(ART_CHOPPER_MIRROR & 1), "a pose's two rotor phases are frames 2k and 2k + 1");

static UBYTE *s_pChopStore;       // (chip) every frame: in the floppy art's block (diskArtCreate)
static UBYTE *s_pChopDma[2];      // (chip) what sprite DMA reads, per copper buffer: phase 0, phase 1
static UBYTE s_pChopDmaPose[2];   // the pose (frame / 2) each buffer holds (0xFF: none yet)

static void chopperCreate(void) {
	for(UBYTE b = 0; b < 2; ++b) {
		s_pChopDma[b] = memAllocChipClear(2 * CHOP_DMA_BYTES);
		CHIP_CHECK(s_pChopDma[b], "the chopper's sprite blocks");
		s_pChopDmaPose[b] = 0xFF;
	}
}

static void chopperDestroy(void) {
	for(UBYTE b = 0; b < 2; ++b) memFree(s_pChopDma[b], 2 * CHOP_DMA_BYTES);
}

// The art on the floppy into its chip block (art/disk_art.toml, diskArtCreate):
// one file, the OS back for it (agkLoadFile). Without it (a hard-disk copy
// that lacks the file) the bands and the chopper stay blank.
__attribute__((noinline, optimize("Os"))) static void diskArtLoad(void) {
	s_isArtPending = 0;
	if(agkLoadFile("diskart.bin.pk", s_pDiskArt, DISK_ART_BYTES) != DISK_ART_BYTES) agkPrint("AGK art missing\n");
	s_ubDiskArtLoaded = 1;
	bobsCrop(0, BOB_W2_FIRST);
	agkPerfSkip();
}

// A world's display art into the arena, at its mission card (or back to world 1
// for the title): the bitplanes and sprites are off from the start of this
// picture (in the vertical blank, before any line shows) until both copper
// lists are the new world's, two pictures later, so no picture of the arena
// half-loaded or of a list for the other world's art is seen. World 2's strip
// ring is allocated here and freed when the game leaves world 2.
static void world2Gradients(void);

// A mission's own BOBs (SPEC 11, "World 2, slice 2": the per-mission set): the block of the previous mission's part freed, a block
// of this one's size made, its file read into it (the OS is on, the display off: the caller turned it off) and its BOBs bound. Mission 4
// has none (its block is freed, s_ubMPart 0). The deck (5,760 bytes) is not in chip in mission 6, nor the freighter and the destroyer (30,368) in mission 5.
__attribute__((noinline, optimize("Os"))) static void missionSetRead(UBYTE ubMission) {
	if(s_pW2M) memFree(s_pW2M, s_ulW2MBytes);
	s_pW2M = 0;
	s_ulW2MBytes = 0;
	s_ubMPart = 0;
	if(!ubMission) return;   // (mission 4's BOBs are all in the common set)
	s_ulW2MBytes = ubMission == 1 ? WORLD2M5_BYTES : WORLD2M6_BYTES;
	s_pW2M = memAllocChipClear(s_ulW2MBytes);
	CHIP_CHECK(s_pW2M, "the mission's BOBs");
	if(agkLoadFile(ubMission == 1 ? "world2x5.bin.pk" : "world2x6.bin.pk", s_pW2M, s_ulW2MBytes) != s_ulW2MBytes) agkPrint("AGK ERR world2 mission art missing\n");
	bobsBindMission(ubMission);
	s_ubMPart = ubMission;
}

// At the start of the first picture of a mission whose part is not in chip (a card of mission 5 or 6 after another mission): the same
// skipped picture and the same two pictures with the bitplanes off as a world's load, so the mission's first picture is not
// slower for it (the floppy read, the templates and the crop all happen here, before it)
__attribute__((noinline, optimize("Os"))) static void missionSetLoad(void) {
	systemSetDmaMask(DMAF_RASTER | DMAF_SPRITE, 0);
	missionSetRead(s_sGame.mission);
	s_ubRasterOff = 2;
	s_pListMission[0] = s_pListMission[1] = 0xFF;   // both game lists written again
	agkState("mission set", s_sGame.world * MISSION_COUNT + s_sGame.mission + 1);
	agkState("w2 chipfree", (LONG)(AvailMem(MEMF_CHIP) >> 10));
	agkState("w2 chipbytes", (LONG)AvailMem(MEMF_CHIP));
	agkEnd();
	agkPerfSkip();
}

static void world3Gradients(void);
__attribute__((noinline, optimize("Os"))) static void worldLoad(UBYTE ubWorld) {
	systemSetDmaMask(DMAF_RASTER | DMAF_SPRITE, 0);
	UBYTE *pArena = s_pDiskArt + ARENA_OFFS;
	// (the boot splash's chip went at the title, genericProcess: SPEC v1.40: no free here, and no W3_KEEP_SPLASH measuring switch)
	if(s_isRingAlloc && ubWorld != 1) {
		memFree(s_pStripMem, 3 * STRIP_PLANE_BYTES);   // (world 2's ring goes when the game leaves world 2)
		s_isRingAlloc = 0;
	}
	if(ubWorld != 1) {   // (world 2's own blocks: slice 2's common and mission BOBs, the freighter's bar)
		if(s_pW2X) memFree(s_pW2X, WORLD2X_BYTES);
		s_pW2X = 0;
		if(s_pW2M) memFree(s_pW2M, s_ulW2MBytes);
		s_pW2M = 0;
		s_ulW2MBytes = 0;
		s_ubMPart = 0xFF;
		for(UBYTE b = 0; b < 2; ++b) { s_pSinkBar[b] = 0; sinkBarDraw(s_pRadarArea[b].pD, 0, 0); }   // (no freighter's bar in the other worlds' HUD)
	}
	if(ubWorld == W3_WORLD) {
		if(agkLoadFile("world3.bin.pk", pArena, WORLD3_BYTES) != WORLD3_BYTES) agkPrint("AGK ERR world3 art missing\n");
		s_pW3Grad = (UWORD *)(pArena + WORLD3_GRAD_OFFS);
		world3Gradients();
		ULONG *pRing = (ULONG *)(pArena + WORLD3_RING_OFFS);   // (the ring is in the arena: cleared, the strip draws into it)
		for(ULONG k = 0; k < 3UL * STRIP3_PLANE_BYTES / 4; ++k) pRing[k] = 0;
		s_pStripMem = (UBYTE *)pRing;
		s_ulStripPlane = STRIP3_PLANE_BYTES;
		s_sStrip.Rows = STRIP3_ROWS;
		s_ubStripFillRate = 1;   // (a column is 9 blits: one a picture keeps the first pictures under the limit; 22 pictures, under the card)
		s_pCloudsBm = &s_sW3Far;
		s_pFarBm = &s_sW3Far;
		s_wStripFirst = -100;   // (a jump: the span drawn again)
	}
	else if(ubWorld) {
		static UBYTE isGradients;
		if(!isGradients) {
			isGradients = 1;
			world2Gradients();
		}
		if(!s_isRingAlloc) {
			s_pStripMem = memAllocChipClear(3 * STRIP_PLANE_BYTES);
			CHIP_CHECK(s_pStripMem, "world 2's terrain ring");
			s_isRingAlloc = 1;
		}
		s_ulStripPlane = STRIP_PLANE_BYTES;
		s_sStrip.Rows = STRIP_ROWS;
		s_ubStripFillRate = STRIP_FILL_PER_FRAME;
		if(!s_pW2X) {   // slice 2's common BOBs (the radar site...): a block of their own, so the boot, the title and world 1 are as they were
			s_pW2X = memAllocChipClear(WORLD2X_BYTES);
			CHIP_CHECK(s_pW2X, "world 2's slice 2 BOBs");
		}
		if(agkLoadFile("world2x.bin.pk", s_pW2X, WORLD2X_BYTES) != WORLD2X_BYTES) agkPrint("AGK ERR world2x art missing\n");
		if(agkLoadFile("world2.bin.pk", pArena, WORLD2_BYTES) != WORLD2_BYTES) agkPrint("AGK ERR world2 art missing\n");
		s_pCloudsBm = &s_sW2Clouds;
		s_pFarBm = &s_sW2Far;
		s_wStripFirst = -100;   // (a jump: the span drawn again)
	}
	else {
		s_pStripMem = 0;
		if(agkLoadFile("world1.bin.pk", pArena, WORLD1_BANDS_BYTES) != WORLD1_BANDS_BYTES) agkPrint("AGK ERR world1 art missing\n");
		s_pCloudsBm = s_pW1CloudsBm;
		s_pFarBm = s_pMountains;
	}
	for(UBYTE p = 0; p < 3; ++p) s_sStrip.Planes[p] = s_pStripMem ? s_pStripMem + p * s_ulStripPlane + 2 : 0;
	bobsBind(ubWorld);   // (the world's own BOBs: their art is loaded, their templates made)
	if(ubWorld == 1) missionSetRead(s_sGame.mission);   // (the mission's own part, in this same skipped picture: a card for mission 5 or 6 starts the game from the debug keys)
	routeCreate();   // (this world's card map)
	s_ubWorld = ubWorld;
	s_ubRasterOff = 2;   // (counted down at the start of each picture: on again when both lists are this world's)
	s_pListMission[0] = s_pListMission[1] = 0xFF;   // both game lists written again
	agkState("world", ubWorld + 1);
	agkState("mission", ubWorld * MISSION_COUNT + s_sGame.mission + 1);
	agkEnd();
	if(ubWorld) {   // the chip budget of the world's play, to the byte (the perf report's "AGK mem" is in KB)
		agkState(ubWorld == W3_WORLD ? "w3 chipfree" : "w2 chipfree", (LONG)(AvailMem(MEMF_CHIP) >> 10));
		agkState(ubWorld == W3_WORLD ? "w3 chipbytes" : "w2 chipbytes", (LONG)AvailMem(MEMF_CHIP));
		agkEnd();
	}
	agkPrint(ubWorld == W3_WORLD ? "AGK world 3 loaded\n" : ubWorld ? "AGK world 2 loaded\n" : "AGK world 1 loaded\n");
	agkPerfSkip();
	// (the loads above ran for several frames, with the OS on: the rest of this picture, and so the pictures after it,
	// start at a vertical blank, not wherever the last read ended)
}

// World 3's bitmaps and tables in the arena (blank until worldLoad)
__attribute__((noinline, optimize("Os"))) static void world3Create(void) {
	UBYTE *pArena = s_pDiskArt + ARENA_OFFS;
	tBitMap *pBm = &s_sW3Far;
	pBm->BytesPerRow = (UWORD)(W3_FAR_W / 8 * 3);
	pBm->Rows = W3_FAR_H;
	pBm->Flags = BMF_INTERLEAVED;
	pBm->Depth = 3;
	for(UBYTE p = 0; p < 3; ++p) pBm->Planes[p] = (void *)(pArena + WORLD3_FAR_OFFS + p * (W3_FAR_W / 8));
	s_pW3TileTab = pArena + WORLD3_TILETAB_OFFS;
	s_pW3Tiles = pArena + WORLD3_TILES_OFFS;
	s_pW3Fills = pArena + WORLD3_FILLS_OFFS;
	s_pW3Plan = pArena + WORLD3_PLAN_OFFS;
	s_uwHazeStride = s_isAga ? 5 : 2;
}

// World 3's sky (top to H, lines 24-129) and haze (H to V, lines 130-255) per mission: OCS line-dithered, AGA in 24 bits;
// into the arena, at each load
__attribute__((noinline, optimize("Os"))) static void world3Gradients(void) {
	for(UBYTE m = 0; m < MISSION_COUNT; ++m) {
		UWORD *pSky = s_pW3Grad + (UWORD)m * W3_GRAD_WORDS, *pSkyLo = pSky + W3_SKY_LINES;
		UWORD *pHaze = pSkyLo + W3_SKY_LINES, *pHazeLo = pHaze + W3_HAZE_LINES;
		logicGradFill(g_pW3SkyTop[m], g_pW3SkyH[m], W3_SKY_LINES, pSky);
		logicGradFill(g_pW3SkyH[m], g_pW3SkyV[m], W3_HAZE_LINES, pHaze);
		for(UBYTE y = 0; s_isAga && y < W3_SKY_LINES; ++y) {
			ULONG c = logicGrad24(g_pW3SkyTop24[m], g_pW3SkyH24[m], y, W3_SKY_LINES);
			pSky[y] = (UWORD)(((c >> 12) & 0xF00) | ((c >> 8) & 0x0F0) | ((c >> 4) & 0x00F));
			pSkyLo[y] = (UWORD)(((c >> 8) & 0xF00) | ((c >> 4) & 0x0F0) | (c & 0x00F));
		}
		for(UBYTE j = 0; s_isAga && j < W3_HAZE_LINES; ++j) {
			ULONG c = logicGrad24(g_pW3SkyH24[m], g_pW3SkyV24[m], j, W3_HAZE_LINES);
			pHaze[j] = (UWORD)(((c >> 12) & 0xF00) | ((c >> 8) & 0x0F0) | ((c >> 4) & 0x00F));
			pHazeLo[j] = (UWORD)(((c >> 8) & 0xF00) | ((c >> 4) & 0x0F0) | (c & 0x00F));
		}
	}
}

// At startup: world 2's bitmaps in the arena (blank until worldLoad)
__attribute__((noinline, optimize("Os"))) static void world2Create(void) {
	UBYTE *pArena = s_pDiskArt + ARENA_OFFS;
	const struct { tBitMap *pBm; ULONG ulOffs; UWORD uwW, uwH; } pBands[3] = {
		{&s_sW2Clouds, WORLD2_CLOUDS_OFFS, ART_W2_CLOUDS_BITMAP_W, ART_W2_CLOUDS_H},
		{&s_sW2Far, WORLD2_FAR_OFFS, ART_W2_FAR_BITMAP_W, ART_W2_FAR_H},
		{&s_sW2Sea, WORLD2_SEA_OFFS, ART_W2_SEA_BITMAP_W, ART_W2_SEA_H},
	};
	for(UBYTE b = 0; b < 3; ++b) {
		tBitMap *pBm = pBands[b].pBm;
		pBm->BytesPerRow = (UWORD)(pBands[b].uwW / 8 * 3);
		pBm->Rows = pBands[b].uwH;
		pBm->Flags = BMF_INTERLEAVED;
		pBm->Depth = 3;
		for(UBYTE p = 0; p < 3; ++p) pBm->Planes[p] = (void *)(pArena + pBands[b].ulOffs + p * (pBands[b].uwW / 8));
	}
	s_pW2Tiles = pArena + WORLD2_TILES_OFFS;
	s_pW2Map = pArena + WORLD2_MAP_OFFS;
	s_sStrip.BytesPerRow = STRIP_RB;
	s_sStrip.Rows = STRIP_ROWS;
	s_sStrip.Depth = 3;
}

// World 2's sky and sea gradients per mission (OCS line-dithered; AGA 24 bits from
// STYLE-world2's own 24-bit ends, split into the high and low nibbles' words): at its
// first load, not at boot (the boot's time to the first picture is tested on AROS)
__attribute__((noinline, optimize("Os"))) static void world2Gradients(void) {
	for(UBYTE m = 0; m < MISSION_COUNT; ++m) {
		logicGradFill(g_pW2SkyTop[m], g_pW2SkyH[m], FLOOR_Y - PLAY_TOP, s_pW2Sky[m]);
		logicGradFill(g_pW2SkyH[m], g_pW2SeaC[m], SEA_LINES, s_pW2SeaBg[m]);
		for(UBYTE k = 0; k < 4; ++k)
			for(UBYTE lv = 0; lv < 3; ++lv) s_pSeaPenCol[m][k][lv] = logicSeaLighten(g_pW2SeaPal[m][4 + k], lv);
		for(UBYTE y = 0; s_isAga && y < FLOOR_Y - PLAY_TOP; ++y) {
			ULONG c = logicGrad24(g_pW2SkyTop24[m], g_pW2SkyH24[m], y, FLOOR_Y - PLAY_TOP);
			s_pW2Sky[m][y] = (UWORD)(((c >> 12) & 0xF00) | ((c >> 8) & 0x0F0) | ((c >> 4) & 0x00F));
			s_pW2SkyLo[m][y] = (UWORD)(((c >> 8) & 0xF00) | ((c >> 4) & 0x0F0) | (c & 0x00F));
		}
		for(UBYTE j = 0; s_isAga && j < SEA_LINES; ++j) {
			ULONG c = logicGrad24(g_pW2SkyH24[m], g_pW2SeaC24[m], j, SEA_LINES);
			s_pW2SeaBg[m][j] = (UWORD)(((c >> 12) & 0xF00) | ((c >> 8) & 0x0F0) | ((c >> 4) & 0x00F));
			s_pW2SeaBgLo[m][j] = (UWORD)(((c >> 8) & 0xF00) | ((c >> 4) & 0x0F0) | (c & 0x00F));
		}
	}
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
	UBYTE ubPair = (UBYTE)(ubFrame >> 1);
	// The wreck (not alive) has no rotor phase but 0 (logicRotorPhase): only
	// phase 0's block is shown, so only it is filled (half the blit, at a lost
	// chopper beside the explosions); bit 7 notes the half, so a live pose
	// later fills both
	_Static_assert(ART_CHOPPER_FRAMES / 2 < 0x80, "the pose numbers leave bit 7 free");
	UBYTE isHalf = !g->chop.alive;
	UBYTE ubHeld = s_pChopDmaPose[ubBfr];
	if(ubHeld != (UBYTE)(ubPair | (isHalf << 7)) && !(isHalf && ubHeld == ubPair)) {
		// the pose's two frames' parts' rows (contiguous in the store) into the
		// eight parts' row areas (each between its control words and its end
		// words): A -> D, one blit of 8 x ART_CHOPPER_H * 2 words (the wreck: 4)
		s_pChopDmaPose[ubBfr] = (UBYTE)(ubPair | (isHalf << 7));
		tAgkBlit *pB = agkBlitqSlot();
		pB->uwCon0 = USEA | USED | MINTERM_A;
		pB->uwCon1 = 0;
		pB->uwAlwm = 0xFFFF;
		pB->pA = s_pChopStore + (UWORD)(ubPair * (2 * CHOP_FRAME_BYTES));
		pB->pB = 0;
		pB->pCD = s_pChopDma[ubBfr] + 4;
		pB->wSrcMod = 0;
		pB->wDstMod = CHOP_PART_BYTES - ART_CHOPPER_H * 4;
		pB->uwSize = (UWORD)((((isHalf ? 1 : 2) * ART_CHOPPER_PARTS) << 6) | (ART_CHOPPER_H * 2));
		agkBlitqPush();
	}
	UBYTE *pDma = s_pChopDma[ubBfr] + ((ubFrame & 1) ? CHOP_DMA_BYTES : 0);
	for(UBYTE p = 0; p < ART_CHOPPER_PARTS; ++p) {
		UBYTE *pPart = pDma + p * CHOP_PART_BYTES;
		UWORD uwHStart = (UWORD)(s_pView->ubPosX - 1 + wX + (p >> 1) * 16);
		UWORD *pHead = (UWORD *)pPart;
		pHead[0] = (UWORD)((uwVStart << 8) | ((uwHStart >> 1) & 0xFF));
		pHead[1] = (UWORD)((uwVStop << 8) | ((p & 1) << 7) | (((uwVStart >> 8) & 1) << 2) |
			(((uwVStop >> 8) & 1) << 1) | (uwHStart & 1));
		ULONG ulPtr = (ULONG)pPart;
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
			CHIP_CHECK(s_pDotList[b][k], "the sky's dot lists");
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
	// (the world on screen: world 2's wisps by day, never stars)
	UBYTE isW2 = s_ubWorld && g->state != GS_TITLE;
	UBYTE isStars = !isW2 && logicSkyDotsAreStars(g->mission);
	UWORD *pDots = s_pDotList[ubBfr][isStars];
	const UWORD *pPal = isW2 ? (s_ubWorld == W3_WORLD ? g_pW3SkyDotPal[g->mission] : g_pW2SkyDotPal[g->mission]) : g_pMissionSkyDotPal[g->mission];
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
	WORD wKey = (WORD)(((g->cam >> 4) << 3) | (isW2 << 2) | g->mission);
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
			CHIP_CHECK(s_pWxList[k][c], "the weather's lists");
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
// Paula channel 2: the rotor, a loop made at startup (logicRotorSynth, logic.c: the same integer
// arithmetic as sound/tools/make_rotor.py, which previews it: keep the two in step). A deep blade
// thump, like a Huey's whop-whop-whop: on every blade pass three damped resonators (~55, ~130 and
// ~230 Hz: the weight and the body small speakers still play), a soft band-passed noise slap, a quiet
// rumble. Two passes of ROTOR_LOOP_PASS samples a loop, each a little different. The rate is the
// Paula period, the logic's (logicRotorPeriod: 5 thumps a second at hover, 6.5 flat out, a faster
// spin-up at lift-off) divided by ROTOR_OS: the loop is stored at twice that rate, so Paula's
// sample-and-hold images sit at 12 kHz instead of gliding through the ears' keenest 4-8 kHz.
// SPEC section 9 (rotor v4).

// The channel is also CLAIMED in ptplayer (SPEC section 9, rotor channel claim): the rotor is a looped
// "effect" of the highest priority there, so the automatic channel choice of an effect (soundPlay) can
// never pick it. Without the claim, an effect with both effect channels (1, 3) busy and no higher
// priority on them took channel 2 (reserved by the mask only when a song is playing and the stale
// per-channel counts say so), and when it ended ptplayer left the channel looping the effect's samples:
// the HQ's "saved" arpeggio hung on for the rest of the game and the rotor never came back.
// ptplayer's own looped effects skip the first word of the sample (the idle word): the wave starts 2
// bytes in.
#define ROTOR_CHANNEL 2
#define ROTOR_ALLOC (ROTOR_SAMPLES + 2)
static BYTE *s_pRotorWave;   // chip RAM: an idle word, then the loop
static tPtplayerSfx s_sRotorSfx;

static UBYTE s_isRotorPending;

// The loop itself, after boot (the splash is black; ~a few frames of work:
// at startup it pushed AROS past the harness's boot window)
static void rotorSetup(void) {
	s_isRotorPending = 0;
	logicRotorSynth((int8_t *)s_pRotorWave + 2);
	agkPerfSkip();                                    // (more than a frame of work, on purpose)
}

// (ptplayer starts the channel on its next tick: wave, length, period, volume 0; rotorUpdate then writes
// the period and volume, the only registers it touches, from the first frame)
static void rotorClaim(void) {
	if(!s_pRotorWave) return;
	ptplayerSfxPlayLooped(&s_sRotorSfx, ROTOR_CHANNEL, 0);
}

static void rotorCreate(void) {
	s_pRotorWave = memAllocChipClear(ROTOR_ALLOC);  // silent until rotorSetup fills it
	if(!s_pRotorWave) {
		agkPrint("AGK ERR out of chip RAM: the rotor's wave (no rotor sound)\n");
		return;
	}
	s_isRotorPending = 1;
	ptplayerSetChannelsForPlayer(0xF & ~(1 << ROTOR_CHANNEL));
	s_sRotorSfx.pData = (UWORD *)s_pRotorWave;
	s_sRotorSfx.uwWordLength = ROTOR_ALLOC / 2;
	s_sRotorSfx.uwPeriod = logicRotorHwPeriod(591);   // (the hover)
	rotorClaim();
}

// The rules (period, volume, the spin-up) are logic.c's, unit tested
__attribute__((noinline)) static void rotorUpdate(void) {
	const tGame *g = &s_sGame;
	volatile struct AudChannel *pAud = &g_pCustom->aud[ROTOR_CHANNEL];
	pAud->ac_per = logicRotorHwPeriod(logicRotorPeriod(g));
	pAud->ac_vol = s_isPaused ? 0 : logicRotorVolume(g);
}

static void rotorDestroy(void) {
	if(!s_pRotorWave) return;
	ptplayerSfxStopOnChannel(ROTOR_CHANNEL);
	g_pCustom->dmacon = DMAF_AUD0 << ROTOR_CHANNEL;
	g_pCustom->aud[ROTOR_CHANNEL].ac_vol = 0;
	memFree(s_pRotorWave, ROTOR_ALLOC);
}

// ---------------------------------------------------------------- sound ---

static void musicFor(UBYTE ubState) {
	if(s_ubSongsPending) return;   // (songsLoad starts the music once they're in)
	UBYTE ubSong = 0xFF;
	if(ubState == GS_TITLE || ubState == GS_INTRO || ubState == GS_SCORES || ubState == GS_ENTRY) ubSong = SOUND_MUSIC_THEME;
	else if(ubState == GS_CLEAR || ubState == GS_VICTORY) ubSong = SOUND_MUSIC_FANFARE;
	else if(ubState == GS_OVER) ubSong = SOUND_MUSIC_GAMEOVER;
	if(!s_isMusicOn) ubSong = 0xFF;
	if(ubSong == s_ubSong) return;
	s_ubSong = ubSong;
	if(ubSong == 0xFF) {
		soundMusicStop();
		rotorClaim();   // (ptplayerStop frees every channel's effect claim, the rotor's too)
	}
	else songStart(ubSong);
}

// (SPEC 9, rotor v3: logicSfxAllow keeps one effect from repeating inside its gap)
static UWORD s_pSfxLast[SFXK_COUNT];

__attribute__((noinline)) static void playSounds(UBYTE ubStateBefore) {
	const tGame *g = &s_sGame;
	UWORD e = g->events;
	UWORD uwNow = g->frame ? g->frame : 1;
	if(g->state != ubStateBefore) {
		musicFor(g->state);
		if(ubStateBefore == GS_TITLE && g->state == GS_INTRO) soundPlay(SOUND_SFX_START);
	}
	if(e & EV_CRASH) soundPlay(SOUND_SFX_CRASH);
	else if(e & EV_OPEN) soundPlay(SOUND_SFX_OPEN);
	else if((e & EV_BOOM) && logicSfxAllow(s_pSfxLast, uwNow, SFXK_BOOM)) soundPlay(SOUND_SFX_BOOM);
	if((g->events2 & EV2_JETWARN) && logicSfxAllow(s_pSfxLast, uwNow, SFXK_JETWARN)) soundPlay(SOUND_SFX_JETWARN);   // the alert (priority 3: before the jet's scream)
	if((e & EV_JET) && logicSfxAllow(s_pSfxLast, uwNow, SFXK_JET)) soundPlay(SOUND_SFX_JET);
	if((e & EV_TANKFIRE) && logicSfxAllow(s_pSfxLast, uwNow, SFXK_TANKFIRE)) soundPlay(SOUND_SFX_TANKFIRE);
	if(e & EV_SAVED) {
		if(logicSfxAllow(s_pSfxLast, uwNow, SFXK_SAVED)) soundPlay(SOUND_SFX_SAVED);
	}
	else if((e & (EV_BOARD | EV_UNLOAD)) && logicSfxAllow(s_pSfxLast, uwNow, SFXK_BOARD)) soundPlay(SOUND_SFX_BOARD);
	if(e & EV_CANNON) soundPlay(SOUND_SFX_CANNON);
	if(e & EV_BOMB) soundPlay(SOUND_SFX_BOMB);
	if((e & EV_LAND) && logicSfxAllow(s_pSfxLast, uwNow, SFXK_LAND)) soundPlay(SOUND_SFX_LAND);
	// world 2: the sea's soft stop sounds as skids touching down; a gunboat's shell as a tank's
	if((g->events2 & EV2_SPLASH) && logicSfxAllow(s_pSfxLast, uwNow, SFXK_LAND)) soundPlay(SOUND_SFX_LAND);
	if((g->events2 & EV2_BOATFIRE) && logicSfxAllow(s_pSfxLast, uwNow, SFXK_TANKFIRE)) soundPlay(SOUND_SFX_TANKFIRE);
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
	// (in the initials Space is a space, typed: only the stick's button is fire)
	pIn->fire = !(g_pCia[CIA_A]->pra & 0x80) || (s_sGame.state != GS_ENTRY && keyCheck(KEY_SPACE));
	pIn->fire2 = !(g_pCustom->potinp & (1 << 14)) || keyCheck(KEY_X);
}

// The art on the floppy (art/disk_art.toml, disk/diskart.bin): one chip block,
// blank until diskArtLoad fills it, with the chopper's frame store and the
// parallax bands' bitmaps in it (3 planes, interleaved: what agk's
// artNAMECreate() made, here pointing into the block)
static tBitMap s_pBandBm[5];

static tBitMap *bandAt(UBYTE ubBand, ULONG ulOffs, UWORD uwW, UWORD uwH) {
	tBitMap *pBm = &s_pBandBm[ubBand];
	pBm->BytesPerRow = (UWORD)(uwW / 8 * 3);
	pBm->Rows = uwH;
	pBm->Flags = BMF_INTERLEAVED;
	pBm->Depth = 3;
	for(UBYTE p = 0; p < 3; ++p) pBm->Planes[p] = (void *)(s_pDiskArt + ulOffs + p * (uwW / 8));
	return pBm;
}

__attribute__((noinline, optimize("Os"))) static void diskArtCreate(void) {
	s_pDiskArt = memAllocChipClear(DISK_ART_ALLOC);
	CHIP_CHECK(s_pDiskArt, "the art block");
	s_pChopStore = s_pDiskArt + DISK_ART_CHOPPER_OFFS;
	s_pCloudsBm = bandAt(0, DISK_ART_CLOUDS_OFFS, ART_CLOUDS_BITMAP_W, ART_CLOUDS_H);
	s_pMountains = bandAt(1, DISK_ART_MOUNTAINS_OFFS, ART_MOUNTAINS_BITMAP_W, ART_MOUNTAINS_H);
	s_pTitleMtn = bandAt(2, DISK_ART_TITLEMTN_OFFS, ART_TITLE_MTN_BITMAP_W, ART_TITLE_MTN_H);
	s_pHills = bandAt(3, DISK_ART_HILLS_OFFS, ART_HILLS_BITMAP_W, ART_HILLS_H);
	s_pGround = bandAt(4, DISK_ART_GROUND_OFFS, ART_GROUND_BITMAP_W, ART_GROUND_H);
	s_pFarBm = s_pMountains;   // (world 1's: worldLoad switches it)
	s_pW1CloudsBm = s_pCloudsBm;
}

// The plain's floor (SPEC 8): its art, the hills' play view (from their row 2),
// the ground under it per mission and line (AGA: 24 bits, split as the sky's)
__attribute__((noinline, optimize("Os"))) static void floorCreate(void) {
	s_pPlainFloor = bitmapCreate(PLAIN_FLOOR_W, FLOOR_LINES, 3, BMF_CLEAR | BMF_INTERLEAVED);   // (empty: transparent until titleLoad)
	CHIP_CHECK(s_pPlainFloor, "the plain's floor");
	s_sHillsPlay = *s_pHills;
	for(UBYTE p = 0; p < 3; ++p) s_sHillsPlay.Planes[p] += PLAY_HILLS_ROW * s_pHills->BytesPerRow;
	for(UBYTE m = 0; m < MISSION_COUNT; ++m) {
		logicFloorFillGround(m, s_pFloorBg[m]);
		for(UBYTE j = 0; s_isAga && j < FLOOR_LINES; ++j) {
			ULONG c = logicFloorGround24(m, j);
			s_pFloorBg[m][j] = (UWORD)(((c >> 12) & 0xF00) | ((c >> 8) & 0x0F0) | ((c >> 4) & 0x00F));
			s_pFloorBgLo[m][j] = (UWORD)(((c >> 8) & 0xF00) | ((c >> 4) & 0x0F0) | (c & 0x00F));
		}
	}
}

__attribute__((optimize("Os"))) void genericCreate(void) {   // (startup: small beats fast, AROS loads ~3 frames a KB)
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

	diskArtCreate();   // the bands and the chopper's frames: blank until diskArtLoad
	world2Create();    // world 2's bitmaps in the same block's band region, its gradients
	world3Create();    // world 3's, over the same arena
	// The sky per mission and line: OCS line-dithered, the A1200's 24-bit below
	_Static_assert(SKY_END - PLAY_TOP == SKY_LINES, "the sky table is logic.c's");
	for(UBYTE m = 0; m < MISSION_COUNT; ++m) logicSkyFillDither(m, s_pSky[m]);
	// (the rest of the list takes ~210 of COP_RAW_COUNT; chip RAM is tight on
	// Kickstart 3.1, so no more slack than that.) Both worlds have a sky MOVE on
	// every line down to the plain (a WAIT and a MOVE each, one layout for a
	// world's missions); the lists hold the bigger, world 2's: the sea's 9 more
	// lines a block each and the island palette (7 MOVEs at line 207), not the
	// hills' band (a WAIT, 8 MOVEs); its strip's switch takes the ground band's place
	_Static_assert(W2_COP_OCS > FLOOR_COP_OCS, "world 2's list is the bigger");
	if(!s_isAga) s_uwCopCount = (UWORD)(COP_RAW_COUNT - 120 + 2 * (FLOOR_Y - PLAY_TOP) + W2_COP_OCS);
	else {
		s_ubFloorStride = 7;
		s_uwCopCount = (UWORD)(COP_RAW_COUNT_AGA + W2_COP_AGA_MORE);
	}

	{
		// (world 3's list is the biggest: a haze MOVE on every strip line as well as a sky MOVE on every line above)
		UWORD uwW3 = s_isAga ? W3_COP_AGA : W3_COP_OCS;
		if(uwW3 > s_uwCopCount) s_uwCopCount = uwW3;
	}
	s_pView = viewCreate(0,
		TAG_VIEW_GLOBAL_PALETTE, 1,
		TAG_VIEW_COPLIST_MODE, VIEW_COPLIST_MODE_RAW,
		TAG_VIEW_COPLIST_RAW_COUNT, s_uwCopCount,
		TAG_DONE);
	s_pVPort = vPortCreate(0, TAG_VPORT_VIEW, s_pView, TAG_VPORT_BPP, 6, TAG_DONE);

	for(UBYTE b = 0; b < 2; ++b) {
		s_pFb[b] = bitmapCreate(FB_W, FB_H, FB_BPP, BMF_CLEAR | BMF_INTERLEAVED);
		CHIP_CHECK(s_pFb[b], "a frame buffer");
	}
	for(UBYTE b = 0; b < 2; ++b) {
		s_pHud[b] = bitmapCreate(HUD_W, PLAY_TOP, FB_BPP, BMF_CLEAR | BMF_INTERLEAVED);
		CHIP_CHECK(s_pHud[b], "the HUD");
	}
	s_pBlankRow = memAllocChipClear(FETCH_BYTES);
	CHIP_CHECK(s_pBlankRow, "the blank row");
	// the title logo's memory and the title's copper lists (filled by titleLoad())
	s_pTitleLogo = memAllocChip(s_ulTitleLogoBytes);
	CHIP_CHECK(s_pTitleLogo, "the title logo");
	s_pTitleFixed = (const UWORD *)s_pTitleLogo;
	s_pTitleRows = (const UWORD (*)[TITLE_LOGO_ROW_REGS])(s_pTitleLogo + TITLE_LOGO_ROWS_OFFS);
	for(UBYTE b = 0; b < 2; ++b) {
		s_pTitleList[b] = memAllocChip(s_uwTitleCopCount * sizeof(tCopCmd));
		CHIP_CHECK(s_pTitleList[b], "the title's copper list");
	}
	floorCreate();
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
	s_isArtPending = 1;
	s_ubSongsPending = 3;
	if(!s_isSplash) {
		diskArtLoad();
		titleLoad();
		while(s_ubSongsPending) songsLoad();
	}

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
	if(s_ubDiskStatus != HI_DISK_SAVING) {
		s_ubDiskStatus = HI_DISK_SAVING;
		return;
	}
	if(g->stateTimer > SCORES_FRAMES - 120) return;   // (the table and this line type themselves first)
	g->scoresChanged = 0;
	s_ubDiskStatus = hiDiskSave(&g->hi, &g_sHiConfig);
	agkPrint(s_ubDiskStatus == HI_DISK_SAVED ? "AGK scores saved\n" :
		s_ubDiskStatus == HI_DISK_PROTECTED ? "AGK scores not saved: write-protected\n" : "AGK scores not saved: disk error\n");
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
	if(g->evRadar) {
		agkState("radar down", g->evRadar - 1);
		agkState("radar zones", logicRadarZones(g, (tZone[RADAR_MAX]){{0}}, 0));
		agkEnd();
	}
	if(g->events & EV_CRASH) agkPrint("AGK chopper lost\n");
	if(g->events & EV_SHRUG) agkPrint("AGK hit ignored (invincible)\n");
	if(g->events & EV_TANKTELL) agkPrint("AGK tank tell\n");
	if(g->events3) {   // (one test a picture in world 1)
		if(g->events3 & EV3_SINKSTAGE) { agkState("sink stage", g->sinkStage); agkEnd(); }
		if(g->events3 & EV3_SUNK) { agkState("sunk lost", g->deckLost); agkEnd(); }
		if(g->events3 & EV3_WINCHSTART) agkPrint("AGK winch start\n");
		if(g->events3 & EV3_WINCHBOARD) { agkState("winch board", g->aboard); agkEnd(); }
		if(g->events3 & EV3_GUSTTELL) agkPrint("AGK gust tell\n");
		if(g->events3 & EV3_GUST) agkPrint("AGK gust start\n");
	}
	if(g->events3) {   // (the strait destroyer and the zone bonus: SPEC 11, slice E)
		if(g->events3 & EV4_TURTELL) agkPrint("AGK destroyer tell\n");
		if(g->events3 & EV4_TURFIRE) agkPrint("AGK destroyer fire\n");
		if(g->events3 & EV4_TURDOWN) { agkState("turret down", (LONG)((!g->turHp[0]) + (!g->turHp[1]))); agkEnd(); }
		if(g->events3 & EV4_DESTDOWN) agkPrint("AGK destroyer down\n");
		if(g->events3 & EV4_DESTOPEN) agkPrint("AGK destroyer open\n");
		if(g->events3 & EV4_BARTELL) agkPrint("AGK barrage tell\n");
		if(g->events3 & EV4_BARFIRE) agkPrint("AGK barrage fire\n");
		if(g->events3 & EV4_AATELL) agkPrint("AGK flak tell\n");
		if(g->events3 & EV4_AABURST) agkPrint("AGK flak burst\n");
		if(g->events3 & EV4_ZONE) agkPrint("AGK zone clear\n");
	}
	if(g->events2 & EV2_BOATTELL) agkPrint("AGK boat tell\n");
	if(g->events2 & EV2_BOATFIRE) agkPrint("AGK boat fire\n");
	if(g->events2 & EV2_SPLASH) agkPrint("AGK splash\n");
	if(g->events2 & EV2_JETWARN) agkPrint("AGK jet warn\n");
	if(g->events2 & EV2_JETTELL) agkPrint("AGK jet tell\n");
	if(g->events2 & EV2_JETFIRE) agkPrint("AGK jet fire\n");
	if(g->events2 & EV2_DAMAGE) agkPrint("AGK chopper damaged\n");
	if(g->events2 & EV2_REPAIR) agkPrint("AGK chopper repaired\n");
	// the bomb prompt starting and ending (tests/hint.agk): "AGK hint=bomb", "=stance", "=none"
	static UBYTE s_ubHintSeen;
	if(g->hint != s_ubHintSeen) {
		s_ubHintSeen = g->hint;
		agkPrint(g->hint == HINT_BOMB ? "AGK hint=bomb\n" : g->hint == HINT_STANCE ? "AGK hint=stance\n" : "AGK hint=none\n");
	}
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
	if(g->state == GS_PLAY && (g->frame & 15) == 8) {
		// the pacing sample (tools/pace.py): what threatens the chopper right now
		// (half-way between the status lines below, every 32 frames: the two in
		// one picture were its heaviest frame)
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
		agkState("rotorHw", logicRotorHwPeriod(logicRotorPeriod(g)));
		agkState("rotorVol", logicRotorVolume(g));
		agkEnd();
		if(g->demo && g->state != ubStateBefore) agkPrint("AGK demo\n");
	}
}

// The first picture is up: how long the boot took, then "AGK ready". CIA-A's
// TOD counter counts vertical syncs, but not through the ROM's start-up, so
// bootvbl trails the harness's frame count (its boot window is 3000 frames
// from power-on) by a fixed amount per ROM: AROS 102, Kickstart 3.1 138,
// Kickstart 1.3 333 (measured on vAmiga, 2026-11-12). tests/memory.agk keeps
// the AROS boot's margin (it loads the program at ~10 frames a KB).
__attribute__((noinline, optimize("Os"))) static void bootReady(void) {
	volatile tCia *pCia = g_pCia[CIA_A];
	ULONG ulHi = pCia->todhi;   // (reading the high byte latches the count)
	ULONG ulMid = pCia->todmid;
	ULONG ulLo = pCia->todlow;
	agkState("bootvbl", (LONG)((ulHi << 16) | (ulMid << 8) | ulLo));
	agkEnd();
	agkReady();
}

void genericProcess(void) {
	agkPerfBegin();
	s_ulFrameVbl = timerGet();
	// A world's art, at its mission card (and world 1's back for the title): in the
	// vertical blank, first thing, so the bitplanes are off before a line shows
	if(s_ubRasterOff && !--s_ubRasterOff) systemSetDmaMask(DMAF_RASTER | DMAF_SPRITE, 1);
	if(s_sGame.world != s_ubWorld && !s_isSplash && !s_isArtPending) {
#ifdef W2_COP_PROBE
		s_ubFloorStride = (UBYTE)((s_isAga ? 7 : 4) + (s_sGame.world ? 1 : 0));
#endif
		worldLoad(s_sGame.world);
	}
	// the weather first, in the vertical blank (last frame's positions: a frame
	// behind the ground, unseen in rain and dust)
	if(!s_isSplash) weatherUpdate();
	keyProcess();     // (the joystick: readInput(), from the hardware)
	// In the initials the keyboard types (hiscore): the game's own keys (M, P,
	// Ctrl+L...) are not read, so typing an M doesn't mute the music. Esc there
	// ends the entry and keeps the score (hiKeyPoll takes it); it quits only as a
	// fresh press outside (keyUse: the Esc that ended the entry is still held)
	UBYTE isEntry = s_sGame.state == GS_ENTRY;
	if(!isEntry) {
		if(keyUse(KEY_ESCAPE)) {
			gameExit();
			return;
		}
		if(keyUse(KEY_M)) {
			s_isMusicOn = !s_isMusicOn;
			musicFor(s_sGame.state);
		}
		if(keyUse(KEY_P) && s_sGame.state == GS_PLAY) s_isPaused = !s_isPaused;
		skillKeys();
	}

	tInput sIn;
	readInput(&sIn);
	{
		UBYTE ubKey = isEntry ? hiKeyPoll() : HI_KEY_NONE;
		sIn.key = ubKey == HI_KEY_NONE ? 0 : ubKey;
	}
	if(s_isSplash) {
		s_isSplash = splashProcess(sIn.fire || keyCheck(KEY_RETURN));
		agkPerfEnd();
		while(timerGet() - s_ulFrameVbl < 1) continue;
		if(!s_isReady) {
			s_isReady = 1;
			bootReady();
		}
		// the art on the floppy: after the first picture ("AGK ready" goes out
		// at the end of its frame: the boot window), before the title and its
		// music (a load stops the music), in a frame of its own (each load is a
		// few seconds of OS time on AROS, and the harness gives a frame 250)
		else if(s_isArtPending) diskArtLoad();
		else if(s_isTitlePending) titleLoad();   // the splash is still black
		else if(s_ubSongsPending) songsLoad();   // (a frame each)
		else if(s_isRotorPending) rotorSetup();  // (a frame later)
		return;
	}
	if(s_isArtPending) diskArtLoad();   // (not loaded during the splash: now)
	if(s_isTitlePending) titleLoad();
	else if(s_ubSongsPending) songsLoad();
	if(s_isRotorPending) rotorSetup();
	if(s_ubAfterSplash < 3 && ++s_ubAfterSplash == 2) {
		// the game's copper list is showing now (the screen still black): the logo's 47 KB go, then
		// the colours are back. Nothing shows the logo again (no attract page, no credits, not the
		// card or the hall of fame use it), and 47 KB of chip RAM were held for the whole game: world 1
		// plays with that much more room, and the worlds after it start from there. ACE's memFree
		// switches the OS on and off for each free (three of them froze the title for ~6 frames when
		// they came after its colours): all three in one trip through the OS, before the colours,
		// is two frames with nothing to see (like the floppy loads), and a long frame to the meter.
		systemUse();
		splashDestroy();
		systemUnuse();
		agkPerfSkip();
		agkPrint("AGK splash freed\n");
		if(s_isAga) agaWaitPaletteWindow();
		for(UBYTE i = 1; i < 32; ++i) g_pCustom->color[i] = s_pVPort->pPalette[i];
	}
	UBYTE ubBfr = backBuffer();
	// (the back list must not be the one the copper still shows: tests/tear.agk)
	if(timerGet() == s_ulSwapVbl) agkPrint("AGK ERR a picture began before its last one was on screen\n");
	// last picture's BOBs erased first: the blitter clears them while the CPU
	// stages the HUD and runs the logic (the top of the picture is in the
	// border, the bus mostly free)
	erasesQueue(ubBfr);
	UWORD uwRadarBlit = g_uwAgkBlitqCount;   // (radarUpdate: after this blit)
	radarClear(ubBfr);
	hudUpdate(ubBfr);   // (what to type: hudDraw types it)
	agkBlitqPoll();
	UBYTE ubStateBefore = s_sGame.state;
	if(!s_isPaused) logicUpdate(&s_sGame, &sIn);
	if(s_ubWorld == 1 && s_ubMPart != s_sGame.mission && s_sGame.world == s_ubWorld) missionSetLoad();   // (a new mission's own BOBs, before this picture draws them: SPEC 11, slice E)
	if(s_sGame.state == GS_ENTRY && ubStateBefore != GS_ENTRY) hiKeyFlush();   // (a key held from the game isn't a letter)
	else if(ubStateBefore == GS_ENTRY && s_sGame.state != GS_ENTRY) hiKeyFlush();   // (an Esc behind another key isn't a quit)
	msgLayoutClear(ubBfr);
	routeClear(ubBfr);
	agkBlitqPoll();
	objectsQueue(ubBfr);
	agkBlitqPoll();
	copperUpdate(ubBfr);
	agkBlitqPoll();
	chopperUpdate(ubBfr);
	skyUpdate(ubBfr);
	// The HUD's CPU work while the blitter finishes the objects (a busy
	// picture ends waiting for the blitter): the radar once its strip's clear
	// is done (blits run in order: a later one started, or the blitter idle
	// after it), then the letters staged at the start
	while(g_uwAgkBlitqNext <= uwRadarBlit + 1 &&
		(g_uwAgkBlitqNext <= uwRadarBlit || ((void)g_pCustom->dmaconr, g_pCustom->dmaconr & DMAF_BLTDONE))) {
		agkBlitqPoll();
	}
	radarUpdate(ubBfr);
	hudDraw(ubBfr);

	agkBlitqFinish();
	if(s_sGame.state == GS_INTRO && !s_pRouteLit[ubBfr]) routeShow(ubBfr);
	else msgUpdate(ubBfr);   // (not both in one picture)
	msgCoverAll(ubBfr);
	if(s_ubOverrun) {
		// (Ctrl+L, O) past this frame's end: to line 60 of the next
		--s_ubOverrun;
		while(timerGet() == s_ulFrameVbl) continue;
		while(getRayPos().bfPosY < 60) continue;
	}
	copProcessBlocks();
	s_ulSwapVbl = timerGet();
	agkPerfEnd();
	// What isn't in the picture, after its swap (the deadline is the swap before
	// the vertical blank): the sounds this picture's events start and the serial
	// report, still before the next frame's tick. (At a lost chopper they were a
	// tenth of the A500's frame, beside the explosions and the HUD's typing)
	if(!s_isPaused) playSounds(ubStateBefore);
	rotorUpdate();
	reportState(ubStateBefore);
	// The new list is on screen from the next vertical blank (COP1LC): wait for
	// the first one after the swap. (Waiting for one after this picture's start
	// let a picture that ran over by less than a frame be followed at once by
	// one writing into the list and frame buffer still being shown: review A3.
	// Such a picture now costs a whole frame, not a tear.)
	while(timerGet() == s_ulSwapVbl) continue;
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
			bootReady();
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
	songsFree();
	systemSetDmaBit(DMAB_SPRITE, 0);
	spriteManagerDestroy();
	skyDestroy();
	weatherDestroy();
	chopperDestroy();
	if(s_pStripMem && s_isRingAlloc) memFree(s_pStripMem, 3 * STRIP_PLANE_BYTES);
	if(s_pW2X) memFree(s_pW2X, WORLD2X_BYTES);
	if(s_pW2M) memFree(s_pW2M, s_ulW2MBytes);
	memFree(s_pDiskArt, DISK_ART_ALLOC);   // (the bands, the BOBs and the chopper's frames)
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
