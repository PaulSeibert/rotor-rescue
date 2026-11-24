# World 3 hardware pilot: handoff (branch `w3-pilot`, 2026-11-24)

Read SPEC.md section 13 first. This is what is built, where it stopped, and what is for the next person.

## Built and proven (vAmiga, a500 / a500-ks31 / a500-aros / a1200)
- **T3 band layout table** (`logicBands`, logic.c): the copper builder reads each world's band lines, rows, speeds and palette slots from
  one table (`tBandLayout`). Worlds 1 and 2 are exactly the old constants (static asserts in main.c); every existing golden is unchanged.
- **T4 height map** (logic.c, `logicW3*`): `floorAt` / `ceilAt`, pads, 60-degree walls, the air-gap rule (`logicW3GapOk`), unit tested.
  World 3 is `g->world == 2` (`W3_WORLD`); `WORLD_COUNT` stays 2 so the game never walks into it: only the debug entry does. Every
  world-2-only rule now asks `logicIsW2(g)`.
- **T5 terrain strip** (main.c, `cwWorld3`, `stripColumn3`, `w3Update`): a 1:1 ring in PF2 on lines 130-255 (126 rows, 3 planes, 18.3 KB,
  pointer creep as world 2's, 22 columns kept), the far range at 1/4 on lines 88-129, the sky and the haze as COLOR00 a line (OCS
  line-dithered, AGA 24 bits). A column is 9 blits (clear, tile, fill a plane). Under the card the ring fills one column a picture behind a
  blank strip; the chopper's sprites and the BOBs are in front (PF1 in front of PF2).
- **Converter** `art/tools/make_world3_disk.py` (Windows Python, Pillow and numpy) reads the style pass's own planner
  (`art/world3/tools/w3compose.Terrain`, `w3scenes`, `w3tiles`, `w3bobs`; default the style worktree, `--art DIR` for another) and writes
  `disk/world3.bin`, `src/world3_art.h` (offsets, palettes, gradients' ends) and `src/world3_map.h` (heights, pad flags, scenery). It
  rebuilds the strip from its own column plan and refuses to write if that is not the art's raster. `--check` for staleness. New art drops in:
  re-run it; the tile bank limit (40) is enforced.
- **Pilot level**: mission 7's first screens from the style pass's M7 scene (low pass, pad on a bench at x 144, nest at 81, lodge at 246,
  pines), extended flat to 4096 px. Ctrl+L, W, 7 (8 and 9 show the other two palettes on the same map, card says NOT BUILT).
  The nest and lodge are scenery.
- **Arena**: world 3's file, gradients and ring are all in the arena world 2 sized (replacing, never adding): `WORLD3_END` 45.5 KB of 68.2 KB.
  World 3 adds no chip and no copper (its list, 631 OCS / 1,333 AGA entries, fits the lists' 661 / 1,340).

## Tests (tests/)
`w3pilot.agk` (card, start pixels, a flight east: sky, far range, rock, no dropped frames, load <= 85 %), `w3load.agk` (the real
transitions 1 -> 2 -> 3 -> 8 -> 9 -> 2 -> 1 -> 3, no dropped frame), `w3squeeze.agk` (WinUAE's chip: floor 64 KB). Unit tests: band table,
height map (surface, skids, landing on a bench pad, no landing off a pad, 70-degree wall, slope slide, ceiling, air gap, built-in map),
`logicStripNew` streaming sweeps. `tools/check_w3strip.py [PROFILE]` compares the flight's screenshots with the art's raster at the camera
x it finds: 0 differing rock pixels at 5 camera offsets on the a500.

## Numbers (vAmiga; bytes unless said)
Chip free in play in world 3, WITH the splash lever (the boot splash's 47,840 B freed at the first world load; commit e45dc69 from
`w2-slice2`, cherry-picked) and WITHOUT (`#define W3_KEEP_SPLASH`), and with WinUAE's 23 KB squeeze (`w3squeeze.agk`):

| profile | with lever | without | squeeze, with | squeeze, without | floor |
|---|---|---|---|---|---|
| a500 | 155,592 | 107,752 | 132,040 | 84,200 | 96 KB / 64 KB |
| a500-ks31 | 190,912 | 143,072 | 167,360 | 119,520 | 96 KB / 64 KB |
| a500-aros | 197,280 | 149,440 | 173,728 | 125,888 | |
| a1200 | 1,287,632 | 1,239,808 | 1,264,080 | 1,216,256 | |

World 1 in play keeps 105 KB (a500; nothing is freed there), world 2 with the lever 147,384. World 3 has MORE chip than world 2 because
its content (pilot 51,394 B) is smaller than world 2's 64,856 B + pads in the shared arena (69,832 B, made at boot).
**Arena ceiling** (a500): the arena is boot-allocated, so growing it takes chip from world 1 too. With the splash freed only at a world's
load, world 1's 9 KB above the 96 KB floor is the limit: about 79 KB. If the splash is freed at the first game start (not done), the limit
is world 2/3's headroom, 155,592 - 98,304 = about 57 KB more, i.e. about 124 KB; on the squeezed run 132,040 - 65,536 = 66 KB more. The
art pass's levered M8 66,878 B and M9 67,262 B fit the arena as it is (69,832): no growth needed.
Copper entries: 631 (OCS) / 1,333 (AGA) used; the lists are 661 / 1,340 (world 2's is the bigger on the a500). Frame load (a500): average
26-47 %, worst 74 % (the first picture after the world load, fill one column a picture); A1200 average 12-24 %. No dropped frame in
w3pilot, w3load (1 -> 2 -> 3 -> 8 -> 9 -> 2 -> 1 -> 3), w3squeeze on any profile. Streaming: 0 differing rock pixels against the art's raster
at 5 camera offsets (a500) and 3 (a1200). Executable 251,728 -> 260,632 B (+8.9 KB); the AROS boot test passes.

## Gate (four profiles, one at a time)
38/38 on a500-aros and a1200; a500 and a500-ks31: 37/38, the failure is `jets`: **98 % worst frame, limit 97 %, one dropped frame**.
Evidence: main 5f60f11 measures 96-97 % there (passes at the edge); the T3 table alone passes; adding only logic.h's world 3 header passes;
adding logic.c's changes fails even with every world 3 branch compiled out and logicIsW2 as it was (so it is not the added tests), and moving the
world 3 code to the end of the file, consolidating hooks to one test per frame, and removing initialised data changed nothing: a layout /
phase effect of about one raster line on a blitter-bound frame (`agk profile`: 313 more line samples over 818 frames). Not paid for.
Options: find one real line of saving in a world 1 frame (Tier 2 can look at the crash's typing and the draw), or the coordinator moves
the test's cap to 98 %.

## Not done / for the next slices
- **amiga-game**: nest tell frames and fire (SPEC 13.6: 26/22/18/16/14), lodge people and barracks logic at heights, trucks/AA on benches,
  shadow on terrain, radar for 4096 px (the pilot uses world 2's 4608 mapping), the "altitude above the floor" refactor of the world-1
  rules, enemy helicopter, winch ledges, M8/M9 maps, the tunnel (portal BOB + rock roof), wind. Hall of fame mission index for 7-9.
- **amiga-hw**: the cloud-ceiling strip (a second ring on lines 44-84, ~6.7 KB, `BK_CLOUDS` is in the table) and the M8 tunnel roof; a
  per-mission list layout (M7 has no ceiling, M8/M9 do: `copperMission` patches colours only, so changing between a ceiling and a no-ceiling
  mission needs a list rewrite); real-hardware / WinUAE proof of the PF2 switch at line 129/130.
- **Observation (Tier 2)**: a 40-character card line (`THE FIRST SCREENS: A PASS, A PAD, A NEST`) left its right half on screen after the
  card; the pilot's lines are now short. The cause was not chased.
- The far range is stored as 640 px (a 320 px loop twice: a fetch of 336 px starts anywhere in the first loop): 10,080 B, not STYLE-world3's
  5,040 B. The arena target still holds (the pilot is 45.5 KB).
- The executable grew 7.5 KB (251.7 -> 259.2 KB): AROS' boot window is the thing to watch (see the gate's boot test).
