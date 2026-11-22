# World 2, slice 2: handoff (branch `w2-slice2`, off main 5f60f11)

Spec: `docs/SPEC.md` section 11 "World 2, slice 2" (v1.20); plan: `docs/CONTENT-PLAN.md` sections 2-4, 7.4, 9.1. Playtest: `J:\amiga\play\PLAYTEST-w2s2\`.

## Built

**Slice A, done (radar sites and the jet zone rule).** `tRadar`, `s_pRadarPlace` (mission 5: x 1608, 3416; mission 6 adds 440), `logicJetZone` (jets START only inside a live zone, missions with sites only), `logicRadarZones`, the HUD ground line in red and white site dots, the card's strip map (rows 1-2), cannon (4 hits) and bomb (kills) damage, 150 points, `AGK radar down=N radar zones=K` on serial. Tests: `testW2Radar*`, `testW2JetsStartOnlyInsideALiveZone`, `testW2JetDelayCountsInsideZonesOnly`, `testW2OneSiteDownLeavesTheOthersZone`, `testW2Mission4KeepsItsJetsEverywhere` (638 checks failed against stubs first); scenario `tests/w2radar.agk`; `tests/w2chip.agk`.

**Memory.** The boot splash's 47 KB (DPS logo and its two copper lists) are freed at world 2's load: world 2 chip free 99,544 -> 147,384 bytes on the a500 (`AGK w2 chipbytes`); the radar BOB (5,616) is in its own block `s_pW2X` (`disk/world2x.bin`, `SETX` in `art/tools/make_world_disk.py`, allocated in `worldLoad(1)`, freed in `worldLoad(0)`), now 141,768. Boot, title and world 1 are as before (105 KB).

## How to add the next BOB (slices B, C, D, E)
1. Add it to `SETX` in `art/tools/make_world_disk.py` (the art is already converted from `art/world2/world2_art.toml`: windsock, destroyer hull, turret, whitecap, bow spray; the deck, rope and freighter are NOT drawn yet: amiga-art), run `python3 art/tools/make_world_disk.py` (it rewrites `disk/world2x.bin` and `src/world2_art.h`).
2. `logic.h`: a `BOB_*` after `BOB_RADAR`, its size in `logic.c`'s three tables; `main.c`: `BOB_W2X(BOB_X, X)` in `bobsBind`'s world 2 branch and `TPL_MAX` + its frames (now 62 for 33 + 28 shared).
3. Chip: only slice-2 BOBs go in `WORLD2X_BYTES`; check `AGK w2 chipbytes` and keep `tests/w2chip.agk`'s 130 KB.

## Slice B plan (not started): wind gusts and the windsock
CONTENT-PLAN 4.4: a gust is 0.25 px/frame sideways (4 sixteenths) for 3 s (150 frames) every 12-20 s (600-1000 frames, from `logicRandom`); the tell is 50 frames before (the sock fills; the plan also says spray and a howl: no new sample is drawn, so serial `AGK gust tell` and the sock only unless Paul wants a sound); no gusts while landed; none on skill 1; skill 2-3 not within 100 px of a barracks; skill 4-5 anywhere; missions 5-6 only. Direction: the sock art (`art/world2/windsock.txt`, 10 x 20, frames limp / full) points right, so gusts push toward +x. Apply it in `logicChopperUpdate` as one more `subStep` on `c->x` (own sub-pixel accumulator, not through `vx`: the landing tolerance stays 6 px, plan 4.4). Windsocks: one at each island's pad (5), drawn from the set (480 bytes, 2 frames, 2 more templates). Tests first: the timer and its range per seed, the tell lead (50 frames), no gust landed, the skill table, mission 4 untouched, the sideways drift per gust (0.25 px x 150 = 37.5 px), the card brief unchanged. Goldens: only pad-side sock pixels in the world 2 scenarios.

## Slices C, D, E (need art first)
- C ship-deck winch (mission 5): hover box 24 x 20 above the deck, 8 people in 8 s on normal, paused not reset when you drift out; counts as a landed-class state for the ground-grace rules; needs a ship deck, rope and ladder drawn.
- D timed freighter (mission 6): at most two timed sites a mission, 90 s on normal, 120 / 105 / 90 / 75 / 60 by skill, a HUD bar and the preview card; needs a freighter (listing and sinking stages).
- E strait destroyer set piece (mission 6, between the second and fourth island, 2208-2816 is already free of jets): two turrets of 6 HP, 4 AA, tell 28 / 24 / 20 / 18 / 16, barrage at 40 s (30-frame tell), turrets open at 90 s; one facing of hull (5,280) and turret (1,824) plus whitecaps (1,080); and the zone bonus (all radar sites down: bonus and a HUD line).

## Budgets and traps found in slice A
- The frame that starts a mission (Ctrl+L, W, n inside world 2, and mission to mission) is the heaviest picture of the game: 94 % on the a500 at baseline. Nothing may be added to it: the map rows 1-2 come from overlays made at the world load and are shown in place of the image rows (drawing them at the card cost 43 %), the HUD zones wait six pictures. `tests/world2pal.agk` is the guard.
- `tests/jets.agk` sits at its own limit (97 %) in world 1: any new per-frame code in the shared paths flips it; the HUD radar's row table was removed to pay for slice A's few instructions.
- The wait-serial pattern is literal text, `expect-serial` a regex checked after the run; `expect-no-serial` looks at the whole log (use `A[\s\S]*B` order patterns).
- Native: see the report: a new data file (`world2x.bin.pk`) and nothing else.
