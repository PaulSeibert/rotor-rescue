# World 2, slice 2: handoff (branch `w2-slice2`, off main 5f60f11)

Spec: `docs/SPEC.md` section 11 "World 2, slice 2" (v1.20); plan: `docs/CONTENT-PLAN.md` sections 2-4, 7.4, 9.1. Playtest: `J:\amiga\play\PLAYTEST-w2s2\`.

## Built

**Slice A, done (radar sites and the jet zone rule).** `tRadar`, `s_pRadarPlace` (mission 5: x 1608, 3416; mission 6 adds 440), `logicJetZone` (jets START only inside a live zone, missions with sites only), `logicRadarZones`, the HUD ground line in red and white site dots, the card's strip map (rows 1-2), cannon (4 hits) and bomb (kills) damage, 150 points, `AGK radar down=N radar zones=K` on serial. Tests: `testW2Radar*`, `testW2JetsStartOnlyInsideALiveZone`, `testW2JetDelayCountsInsideZonesOnly`, `testW2OneSiteDownLeavesTheOthersZone`, `testW2Mission4KeepsItsJetsEverywhere` (638 checks failed against stubs first); scenario `tests/w2radar.agk`; `tests/w2chip.agk`.

**Memory.** The boot splash's 47 KB (DPS logo and its two copper lists) are freed at world 2's load: world 2 chip free 99,544 -> 147,384 bytes on the a500 (`AGK w2 chipbytes`); the radar BOB (5,616) is in its own block `s_pW2X` (`disk/world2x.bin`, `SETX` in `art/tools/make_world_disk.py`, allocated in `worldLoad(1)`, freed in `worldLoad(0)`), now 141,768. Boot, title and world 1 are as before (105 KB).

## How to add the next BOB (slices B, C, D, E)
1. Add it to `SETX` in `art/tools/make_world_disk.py` (the art is already converted from `art/world2/world2_art.toml`: windsock, destroyer hull, turret, whitecap, bow spray; the deck, rope and freighter are NOT drawn yet: amiga-art), run `python3 art/tools/make_world_disk.py` (it rewrites `disk/world2x.bin` and `src/world2_art.h`).
2. `logic.h`: a `BOB_*` after `BOB_RADAR`, its size in `logic.c`'s three tables; `main.c`: `BOB_W2X(BOB_X, X)` in `bobsBind`'s world 2 branch and `TPL_MAX` + its frames (now 62 for 33 + 28 shared).
3. Chip: only slice-2 BOBs go in `WORLD2X_BYTES`; check `AGK w2 chipbytes` and keep `tests/w2chip.agk`'s 130 KB.

## Slice B, done (wind gusts and the windsocks; SPEC v1.21)
`windUpdate` (missions 5-6, called with `boatsUpdate`, zero cost in world 1), `logicGustsOn`, `gustTimer/gustTell/gustOn/gustFx/gustSeed` (own xorshift: the game's LFSR is untouched), `EV3_GUSTTELL/EV3_GUST` and serial `AGK gust tell`, `AGK gust start`. 0.25 px a frame toward +x for 150 frames after a 50 frame tell, gaps 600-1000 frames, first tell 900+ frames into play, none while landed (a landing cancels it) or in the 2 s take-off grace, none on skill 1, skill 2-3 not within 100 px of a barracks (a push in progress is held there), one at a time. `BOB_SOCK` (10 x 20, 2 frames, in `SETX`, +480 bytes) beside each pad (`SOCK_DX` 60), limp / whipping in the tell / full in the gust, not drawn on the card (the card's first picture is the heaviest of the game). NO new sound (none drawn, none reused). Tests: `testW2Gust*`, `testW2Mission4AndWorld1HaveNoWind`, `testW2SockIsDrawnAndWhips` (24 checks red first), `tests/w2wind.agk`.

## Slices C, D, E (need art first)
- C ship-deck winch (mission 5): hover box 24 x 20 above the deck, 8 people in 8 s on normal, paused not reset when you drift out; counts as a landed-class state for the ground-grace rules; needs a ship deck, rope and ladder drawn.
- D timed freighter (mission 6): at most two timed sites a mission, 90 s on normal, 120 / 105 / 90 / 75 / 60 by skill, a HUD bar and the preview card; needs a freighter (listing and sinking stages).
- E strait destroyer set piece (mission 6, between the second and fourth island, 2208-2816 is already free of jets): two turrets of 6 HP, 4 AA, tell 28 / 24 / 20 / 18 / 16, barrage at 40 s (30-frame tell), turrets open at 90 s; one facing of hull (5,280) and turret (1,824) plus whitecaps (1,080); and the zone bonus (all radar sites down: bonus and a HUD line).

## For the art agent: slices C and D (what the code will need; sizes in px, PF1 pens 1-7, three planes + mask, <= 16 px wider than the art for the shift word; see `docs/STYLE-world2.md` and `art/world2/*.txt` for the format and the look)
**C, the ship deck (mission 5; 16 people a mission replace one barracks' worth; at 8 a winch hover).**
- `deck_ship.txt`: a moored cargo ship in profile, hull about 112 x 30, waterline on `BOAT_BOTTOM_Y` (248, like the gunboat), a FLAT open deck whose top row is at y 226-228 (the chopper hovers 24 x 20 px above it) with a bridge and a funnel at one end only (so the hover box over the deck is free), one frame idle, plus 1 frame "bow wake" (like `patrol_boat.txt`): 2 frames, one facing (the ship does not move). Light top left, the world 2 rules (no flat colour area, no full-width row, no ordered dither).
- `winch_rope.txt`: the rope and ladder hanging from the skids: 8 x 40, 3 frames (rope hanging, ladder swaying left, ladder swaying right) plus a short 8 x 12 "hook" frame for the first moment (a person climbing is the existing `BOB_HOSTAGE` 16 x 12; add 2 frames of a person climbing, `MAN_CLIMB0/1`, 16 x 12, arms up).
- `deck_lamp` : reuse the pad lamp (yellow inside the box, red outside); no new art. The "hover box" is shown by those two lamps on the deck's ends, 24 px apart.
- Tells: the box lamps blink as the pad's; the rope drops 24 px over 12 frames when the hover starts; people on deck wave (reuse `MAN_WAVE0/1`).
**D, the sinking freighter (mission 6; timed: 90 s on normal, 120 / 105 / 90 / 75 / 60 by skill).**
- `freighter.txt`: a bulk freighter about 128 x 34 with a high bridge aft, ONE flat open hold area amidships (24 px free for the hover box), 4 frames of its sinking, one facing: level, listing by 2 px at the bow (stage 1), 5 px (stage 2), nearly awash (stage 3, the deck 6 px above water at the bow and a little steam). Each stage is a quarter of the timer. The deck line must stay flat enough for the hover box (it tilts: the deck top rises 0 / 2 / 5 / 8 px at the stern while the bow dips; keep the pivot at the waterline centre).
- `sink_spray.txt`: 24 x 12, 3 frames of foam / steam puffs along the waterline that grow with the stage (the tell that time is running out); the existing smoke BOB can be reused for steam if chip is tight.
- HUD: a timer bar on the card-style HUD line is code, not art (pen 5 red as the zone bars).
- Both ships: one small deck lamp pair at the hover box (reuse the pad lamps); no new sound is planned (the plan's alert would need a sample).
- Budget: deck ship 2 frames 112 -> 128 wide x 30 high is about 2 x 30 x 16 B x 2 (image + mask) = 3.8 KB; freighter 4 frames about 128 x 34 -> 4 x 34 x 20 B x 2 = 5.4 KB; people / lamps reuse. The slice 2 block has about 120 KB of the a500's chip left after B (`tests/w2chip.agk` asserts 130 KB at the load: lower it to the measured value minus 2 KB when art lands, say so in the SPEC).

## Budgets and traps found in slice A
- The frame that starts a mission (Ctrl+L, W, n inside world 2, and mission to mission) is the heaviest picture of the game: 94 % on the a500 at baseline. Nothing may be added to it: the map rows 1-2 come from overlays made at the world load and are shown in place of the image rows (drawing them at the card cost 43 %), the HUD zones wait six pictures. `tests/world2pal.agk` is the guard.
- `tests/jets.agk` sits at its own limit (97 %) in world 1: any new per-frame code in the shared paths flips it; the HUD radar's row table was removed to pay for slice A's few instructions.
- The wait-serial pattern is literal text, `expect-serial` a regex checked after the run; `expect-no-serial` looks at the whole log (use `A[\s\S]*B` order patterns).
- Native: see the report: a new data file (`world2x.bin.pk`) and nothing else.
