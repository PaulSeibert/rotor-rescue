# Spike: terrain height (content plan phase 0, T5 / T17)

**Question.** Can the engine show varying terrain height (mountain passes, valleys, a cloud
ceiling, barracks on mountain tops) within the A500's frame and chip budgets, with the current
copper, parallax and perspective-floor setup?

**Verdict: yes.** A 1:1 terrain strip in playfield 2 streams at the camera's speed for about the
cost of the perspective floor it replaces. The heaviest frame in play moves 81% -> 82% on a500 and
80% -> 82% on a500-ks31, and the average goes down 0.5 points. The tall strip costs 23 KB of chip
RAM all in. Phase 0's memory pay-back left 119 KB free on a500 and 153 KB on a500-ks31, against a
64 KB floor. One frame needs care: the full redraw when a mission starts (below).

Measured 2026-11-12 on vAmiga, prototype on branch `spike-terrain-height` (commit `b5cc176`, off
`phase0-memory`; throwaway code, not for main).

## What was built

- **The strip:** PF2 on game lines 130-255 (126 rows), 3 planes, the camera's own speed. It replaces
  the plain's floor, the hills and the ground on its lines. The clouds (/8) and the top of the
  mountain band (/4, lines 120-129) stay above it.
- **A made-up height map:** 192 columns of 16 px (`WORLD_W` 3072). It has a slope up to a 100 px
  ridge, a plateau (where a barracks on a mountain top would go), a pass at 30 px, a 104 px second
  ridge, a valley floor, and flat ground at the home pad. Rock is drawn from a 64 x 126 texture: per
  plane, one clear blit above the rock and one copy blit for the rock (6 blits a column).
- **A pointer-creep ring with no copper wrap.** Map column j, row r lives at
  `plane + 2j + r * 44` (rows of 22 words: 21 fetched with the fine-scroll word, plus 1). Scrolling
  only moves the three bitplane pointers, 2 bytes a column. A column coming into view is drawn over
  the one 22 columns away, which left the view long ago. The whole map creeps `WORLD_W / 8` = 384
  bytes, so no copper wrap or vertical split is needed (a 4096 px world: 512 bytes).
  - The planes are separate, not interleaved: a creeping pointer would run into the next plane's row.
  - Both pictures' columns are kept drawn (the front one's and the back one's), so nothing the
    copper is showing is ever overwritten. The camera moves less than 16 px a frame.
- **Copper:** at line 129 the strip's three pointers and `BPL2MOD` = 2 replace the band switch
  (`cwBand`, scrolled by the same `bandSet` as the bands). Then one WAIT + MOVE a line put COLOR00 in
  a haze gradient behind the rock, 126 lines. The rock uses the mountain band's palette, so no
  palette MOVEs are needed.
- **Streaming:** `stripUpdate` runs in `copperUpdate` when the camera moves and queues the new
  column's 6 blits with the BOBs. That is one column every ~5 frames at 3 px/frame.

## Measurements (a500 / a500-ks31; `tests/perf.agk`, a whole rescue trip)

| | phase0-memory (floor, hills, ground) | spike (terrain strip) |
|---|---|---|
| average load | 34.9% / 34.9% | 34.3% / 34.4% |
| heaviest frame in play | 81% / 80% | 82% / 82% |
| next heaviest | 79, 78, 78 / 79, 79, 79 | 80, 79 / 81, 79 |
| mission start, all 22 columns in one frame (132 blits, waiting) | n/a | **135%** (one frame) |
| mission start, 4 columns a frame | n/a | 86% |
| dropped frames | 0 | 0 |
| chip free in play | 119 KB / 153 KB | 96 KB / 130 KB |
| copper list (OCS), used of allocated | 559 of 606 | 626 of 874 |

- **Chip, 23.1 KB:** the strip's planes 3 x 5,974 = 17.9 KB (44 x 127 rows + 386 bytes of creep
  each), the texture 3.0 KB, and the copper +268 entries x 4 bytes x 2 lists = 2.1 KB. The world-1
  floor, hills and ground were still allocated in the spike (18.8 KB); a world-3 arena would not
  hold them.
- **CPU:** `stripUpdate` is ~1% of the working time over the run. The 6 blit setups come to about
  7 raster lines on a frame that draws a column. They cost less than what the strip saves:
  `copperUpdate` and `floorSet` together drop from 5,921 to 4,131 samples, with no per-line floor
  scroll and no hills or ground bands to set. The blitter time per column is about 750 words of
  A -> D and D-only blits, overlapped with the CPU.
- **Bus/DMA:** unchanged bitplane DMA (still 3 + 3 planes, same DDFSTRT/DDFSTOP); the copper adds
  126 WAIT/MOVE pairs a frame (~1,000 bus cycles, under 1% of a frame).

Screenshots of the prototype: build it on the spike branch and run a short flight (`press left 120`, `screenshot`).
The strip streams with the camera, and the 16 px columns are visible as stair steps (no slope tiles yet).

## How close a500-ks31 gets to overrunning (for the frame-budget plan)

- The trip in `perf.agk` peaks at 80-82% on both A500 profiles, with or without the strip. That
  leaves about 18% before a frame is late.
- **Already late on main (not from this work):**
  - **Skill 5, the chopper going down among explosions** (`tests/jets.agk`): one 50-frame window
    averages 143-144% and drops 9-12 frames on a500 and a500-ks31.
  - **The hall of fame** (`tests/hiscore.agk`): windows peak at 126-136%.
  - **The skill-5 jet tell frame** measures 98% on a500. Adding 1% there (the first try at the
    chopper DMA blocks, see the branch report) made the picture one frame late and failed `jets.agk`.
- The frame loop starts on the blank counter and swaps the copper list at its end. A frame that
  runs even slightly past the vertical blank shows the previous picture once more. With
  `while(timerGet() - s_ulFrameVbl < 1)`, the next frame then also starts late, into a list the
  copper is still showing (the reviewer's torn-frame risk at `main.c` around the swap).
- So "no dropped frames" in `perf.agk` doesn't mean the game has headroom everywhere. The
  explosion and hall-of-fame frames need their own perf scenarios and fixes before world 3 adds
  terrain to them.

## Recommended approach

1. **The strip as built:** pointer creep, separate planes, no copper wrap (maps up to ~8000 px
   creep 1 KB). Use one strip bitmap, not one per copper buffer: the strip is background, drawn
   only where neither picture looks. The band layout table (T3) switches PF2 to it at the world's
   top line.
2. **Columns from small tiles, not full-height art.** A full-height 16 x 126 column is 756 bytes a
   plane set, so 100-150 unique columns would be 75-113 KB. Instead, compose each column from:
   - a clear above the rock;
   - a 16 x 16 top-edge tile (slopes 0/15/30/45 degrees, ledges, summits, pads, tunnel mouths: ~40
     tiles x 96 bytes = 3.8 KB);
   - a body fill from 2-4 tall rock textures (756 bytes each).

   That is 9 blits a column. The tile bank is about 7-12 KB, under the content plan's 25 KB
   estimate. The plan's world-3 arena estimate (74 KB) then comes down to about 55-60 KB.
3. **The mission-start fill spread over the mission card:** 2-4 columns a frame while the card is
   up. Never 22 at once: that frame took 135%.
4. **The cloud ceiling** is the same strip at lines 44-84 (41 rows, ~6.7 KB, a second pointer
   switch). It replaces the cloud band where it shows. A tunnel is a ceiling and a floor in the same
   two strips.
5. **The haze:** line-dither the COLOR00 gradient like the sky (the spike's plain steps band
   visibly) and keep it inside the copper budget. Size the copper lists per world from the band
   table, not as one fixed count: the strip took the OCS list to 874 entries. AGA's 24-bit haze
   would add 2 MOVEs a line (not measured).
6. **Logic (Tier 2, `amiga-game`):** `floorAt`/`ceilAt` read the height map (16 px columns). The
   chopper, people, tanks and shadows use altitude above the local floor, and a wall bump costs
   speed, never damage. Barracks on a plateau are a floor value, and the PF1 BOBs draw anywhere
   already.

## Not covered by the spike

- Slope tiles, ledges, the tunnel, the ceiling strip and the dithered haze were not built. Their
  cost is estimated above from the same blits.
- The A1200 path, and real hardware: vAmiga only.
- **Prove It:** on WinUAE A500 (Kickstart 1.3 and 3.1) or a real A500, fly the spike branch at full
  speed both ways over the ridges. Watch for a column popping in at either screen edge and for
  tearing at the mission start. Paul can run this; the build is `spike-terrain-height`.
- Height-map collision, landing on the plateau, and enemies on slopes: logic, not measured.
