# ROTOR RESCUE: style addendum, World 2 "Coral Islands" (missions 4-6)

Addendum to the game's `docs/STYLE.md` (the approved art-pilot STYLE.md rules all apply: light from the top left, 1-px 0x111 outline on
everything that moves or must be read, bands without outline, locked PF1 colours, no band edge visible). Status: **approved 2026-11-13** (Paul), with one change: the cloud band (section 2).
The production art is in `art/world2/`, built by `art/tools/make_world2.py`, checked by `art/tools/check_world2.py`. Source of truth for the contract is `docs/plain-spec.md` (the `perspectivefloor` technique had not landed in
the kit when this was written, so the plain-spec geometry is generalised below).
Generators: `art/tools/world2/` (ported bit-identical from the pilot's `i*.py`), RD raws in `art/world2/raw/`; review pictures `coral-islands-prod-*.png`.

## 1 Look

Bright tropical diorama: turquoise sea in perspective, pale volcanic islands lost in warm haze, cumulus lit from the top left, sand-and-jungle islands
in front, and a crisp olive-and-cream enemy cast on top. The sea is the floor of the world: it is the first thing the player sees, so it carries the
perspective cue (wave dashes shrink and thin out toward the horizon, never a ruled line, never a dither row). Mission moods are palette swaps only:
**M4 Reef Run** morning (azure to cream, warm light), **M5 Trade Winds** midday haze (washed, very pale horizon), **M6 Typhoon Strait** storm dusk
(grey-green, dark slate clouds with a warm dusk rim, rain as BOB streaks, whitecaps). Night is not in the plan for World 2.

## 2 Screen layout (game lines; HUD 0-23 unchanged)

| lines | layer | speed (camera) | bitmap | PF2 palette loaded |
|---|---|---|---|---|
| 24-179 | sky: copper COLOR00 gradient, line-dithered; sprite wisps (ch 6) | n/a | none | n/a |
| 72-111 | clouds, 40 rows, rows 0-2 and 37-39 empty; **no straight base** (see below), belly dissolves by blue noise | 1/8 | 320 x 40 loop | clouds (load at line 24) |
| 112-119 | blank PF2 rows (palette switch) | n/a | none | far islands loaded at 113 |
| 120-169 | far volcanic islands, band rows 0-49 (rows 0-2 empty, apexes pointed) | 1/4 | 320 x 60 loop, one band with the far strip | far |
| 170-179 | far strip: island feet dissolve into haze, sparse far-sea glitter; row 9 (line 179) empty | 1/4 | same band | far |
| 180-207 | **sea floor in perspective**, 28 rows; row 0 (line 180) empty = haze H; row 27 (line 207) empty | row j: (28 + 3j) / 112, i.e. 1/4 to 0.97 | 640 x 28 loop per row W_j = 4 (28 + 3j) px | sea (load at 179, in the empty row) |
| 208-255 | **terrain strip**, 48 rows of 16 px tile columns streamed into a ring buffer; row 0 holds pens 6 and 7 only | 1 | 352 x 48 ring | island (load at 207, in the empty floor row) |
| all | PF1: BOBs; sprites: chopper | 1 | | fixed |

Backdrop (COLOR00): sky gradient lines 24-179 ending in the haze **H**; lines 180-207 a line-dithered gradient **H to C** (the sea's depth);
lines 208-255 flat **C** (deep water, shows through every transparent strip pixel). C is the one colour all three seams share.

### Cloud band (changed at approval, 2026-11-13)
The pilot cut every cloud on one flat base, an 88 px straight edge, which read as a band. Each cloud is now a ring of overlapping lobes with
its own wandering, scalloped underside; the belly pens follow each column's own bottom and dissolve by blue noise. Six masses with bases
between rows 11 and 27: a big low bank, a narrow tower, a small high bank, a mid bank, a high wisp. Same pens, same top-left light, rows
0-2 and 37-39 still empty (the seams). `check_world2.py` enforces it: longest run of bottom-edge pixels on one row <= 10 px, no row carrying
more than 22 % of all base pixels, >= 10 rows between the highest and lowest base, no straight diagonal over 9 px, >= 3 clouds.

### Perspective sea (the rule for drawing it)
Horizon at line 170.7 (so line 180 moves at 1/4 and line 208 at 1: speed is linear in screen y, as 1/z is for a flat sea). Think of the sea as a
strip of 448 world px; world x on row j is drawn at column x (28 + 3j) / 112. A wave crest is a world-length dash of 6-22 px: 1.5-5 px wide at row 1,
6-21 px at row 26. Rows 1-8 are one pixel high, haze-coloured (pens 4/5, almost the gradient, low contrast); rows 9-26 add a shadow pixel (pen 2/3) one
px down and right of the crest; pen 7 glints sit on the lit left end of the near crests only. Every row is 22-29 % drawn (the solid rule asks 15 %),
density wanders along x (three sines that divide 448), no two rows share phase. Near rows never exceed 29 % coverage or the sea turns to scales.
Strip water continues at 1:1: ripples 3-8 px (top) to 8-14 px (bottom), pens 6 (crest), 7 (rare glint), 1 (half the troughs).

### Seams (all proven in `icheck.py`)
- bands meet only in sky or sea gradient: the clouds' top and bottom three rows, the blank rows, the far band's top three rows, line 179, line 180
  and line 207 are bare (no pixel) at 16 camera offsets in 3 missions;
- the far strip dissolves by blue noise (a 64 x 64 void-and-cluster map, wandering start row, pens lift by up to 2 toward the haze), never a Bayer row;
- the sea pens 6 and 7 equal the island pens 6 and 7 in every mission, so strip row 0 may carry them across the 207/208 palette switch;
- islands in the strip stand on tile columns that all end in neutral columns (no vegetation or palm touches column 0 or 15), so any order joins;
- **biome change to the next world** is not in this world; between islands the "connector" is water, with reef tiles and surf rings (blue-noise placed).

## 3 Palettes (12-bit OCS, pens 1-7 dark to light; AGA extras are 24-bit and limited to gradients)

Fixed PF1 and chopper colours are unchanged. PF2 per mission:

| | M4 Reef Run (morning) | M5 Trade Winds (midday haze) | M6 Typhoon Strait (storm dusk) |
|---|---|---|---|
| sky top to low (H) | 28B to CFE | 5AD to DFF | 233 to 8A9 |
| C (deep sea, flat) | 179 | 1AB | 254 |
| clouds | 59B 7BC 9CD BDE DEE FEE FFF | 7AC 9BD BCE DDE EEF FFF FFD | 112 223 334 445 667 889 CA9 |
| far islands | 357 468 579 68A 8AB ACC CEE | 69A 7AB 8BC ACD BDD CEE DFF | 233 344 455 566 677 788 899 |
| sea (floor) | 034 057 17A 39C 6BD 8DE FFF | 035 057 17A 3AC 5BE 9EF FFF | 022 033 145 367 588 9BB DEE |
| island (strip) | 023 142 4A3 975 ED9 8DE FFF | 023 153 5B3 985 EDA 9EF FFF | 012 131 252 764 B97 9BB DEE |
| sky dots (29-31) | 9CD EFE FFF | BDE FFF FFE | 445 889 BBB |
| AGA sky top, low | 2488CC, CCFFEE | 55AADD, DDFFFF | 112D2D, 88AA99 |
| AGA C | 1A7C9A | 18AABB | 1F4F4A |

On AGA the sky and the H-to-C sea gradient are 24-bit per line (`logicSkyColor24`); the 12-bit pens do not change, and the
far bands scroll in quarter pixels, so sub-pixel motion only ever moves pixels that already sit on sky, haze or C. AGA-only extras (optional, copper):
the 1-px sea shimmer as BPLCON1 phase by line and time (the plan's T7), and 24-bit rain tint on the M6 gradient.

Pen roles. **Island pens**: 1 deep teal shade (trough, jungle base, rock shade), 2 dark green, 3 light green, 4 sand shade and rock, 5 sand,
6 shallow-water crest (**shared with the sea palette**), 7 foam white (shared). **Sea pens**: 1-2 troughs, 3-4 body, 5 crest, 6-7 shared glints.
**Cloud pens**: 7 lit top-left, 6 lit, 5 body, 4 shade, 3 flat belly. **Far pens**: islands are drawn dark (pens 1-4 in the RD source) and re-lit
by the build: lifted two pens (aerial perspective), +1 on a left edge, -1 on a right edge, haze lifts up to two more at the foot.

Checked collisions: a pen must not equal the backdrop gradient on a row where it covers most of the row (the solid check caught M5's pen 5 equal to
the gradient at line 197; it was changed from 6CD to 5BE). Re-run `icheck.py` after any palette edit.

### Copper and palette budget (estimate, needs amiga-hw to confirm)
World 1 uses 465 of 496 entries [S]. World 2 relative to it: hills band dropped (about -17), floor 28 lines instead of 19 (+9 lines x 4 = +36), a fourth
palette load (+8), the strip's bitplane-pointer switch (+8): **about 500 entries, about 4 over the allocation**. Fixes in order: enlarge the list by 16 entries
(64 bytes chip), or shorten the floor to 24 rows (the strip stays at 208). The three missions differ only in table data (no new copper structure).
Palette loads per frame: clouds 7 + far 7 + sea 7 + island 7 = 28 colour moves, all in PF2-blank rows.

## 4 Terrain tile kit (the strip)

- **Tile = a 16 px wide, 48 row column** (3 planes = 288 bytes). The map is one byte per column (4608 px = 288 columns = 288 bytes). No 2-D tiles: an island is a
  run of columns, which suits the streaming of one column per 5 frames.
- Pilot bank: **36 columns = 10,368 bytes of chip** (the plan's budget is 12 KB, so 1.6 KB headroom): water x 8, flat island x 4,
  beach end left x 2 variants x 2 columns, beach end right x 2 x 2, palms x 3 groups x 2 columns, scrub x 2, boulders x 2 groups x 2, reef x 2, pad bed x 2.
  Limit for the production pass: **40 columns (11.5 KB)** per world; a world at 40 needs the reuse check (top-10 columns cover 50 % of the map).
- Rows: 0-18 sea behind (ripple texture, never bare), 19-26 jungle fringe (dark mounds with a lit top-left, a dark base row), 27-35 the walking track
  (feet on row 32 = y 240, landing floor row 28 = y 236), 36-40 beach in shade (wet sand), 41-45 surf ring (pens 6, 7), 46-47 shallow water. Palms stand
  with their base on row 30-31 and reach row 3; nothing but water touches row 0.
- Join rules: every column ends in neutral columns (water ripples may run to the edge, they are random, so nothing lines up); the beach ends are two columns
  (32 px) of slope; wind-rippled sand puts a dash of the other sand pen on every sand row (>= 20 %), so a long island is never a flat colour; pebbles are 2-px clusters.
- Pads are **BOBs on a flat pad-bed column** (so the pad can be 48-96 px wide by skill, built from 8-px units). Barracks, boats and anything the player must read are PF1
  BOBs with the 1-px outline.
- Tile rule for flips: shading is baked; **no mirrored tiles** (the light would flip); right beaches are their own tiles.

## 5 BOB set and tells

| object | size | frames | notes |
|---|---|---|---|
| island barracks | 52 x 32 | closed (blue lamp), open (red lamp, lit door); the game adds shot-up and burning | stilts, white walls, olive thatch roof, sandbags |
| pad kit | cap 8 x 46, slab units 8 / 16 x 16 | lit and dark phase | far edge lit, front face in shade; chevrons point to a lit H; edge lights along the rim; two pylons with a beacon |
| pad lamps | 5 x 5 | red (off pad), yellow (over pad) | **no green exists in PF1**: the plan's green lamp becomes yellow, and the HUD PAD tick is yellow/red too; a true green would need sprite channel 6 (costs the wisps) |
| patrol boat | 43 x 17 | 2 wake frames x 2 facings | low cabin, bow gun |
| gunboat | 54 x 32 | idle, tell 1 (barrel 28 deg, mast lamp red), tell 2 (barrel 42 deg, flash, 3 lamps), fire (flash and smoke) | the tell is the barrel rising and the lamp; frames by skill 22 / 18 / 16 / 14 / 12 |
| radar site | 34 x 40 | dish 4 frames (wide, mid, thin, mid) + a blinking red lamp | on a concrete base with a hut |
| destroyer | 154 x 44 | hull in three BOB sections (bow 64 / mid 64 / stern 32); turrets are separate parts: idle, tell 1, tell 2, fire | bridge, mast and radar, two funnels, two AA mounts; the turret tell is 28 / 24 / 20 / 18 / 16 frames |
| windsock | 16 x 22 | limp, full | the gust tell: it fills 50 frames before |
| splash, bow spray, whitecap | 24 x 24, 12 x 8, 34 x 10 | 5, 2, 3 | water touch = soft stop with a splash; whitecaps only M6 |

Boats and the destroyer have **two facings drawn, not mirrored**: the right-facing set is drawn bow-right from the same code with the lit rims kept on the left of every block, so the light stays top left (`w2bobs.C(flip=True)`; `check_world2.py --assets` fails a plain mirror and any right-facing frame lit from the right). Barracks: the engine's four frames (intact, shot up, burning x2); the pilot drew closed and open, shot-up and the flames are derived from them.

## 6 Chip and disk (pilot measurement; the production numbers are in section 9)

| part | chip bytes |
|---|---|
| terrain tile bank (36 columns) | 10,368 |
| tile map | 288 |
| terrain ring 352 x 48, 3 planes | 6,336 |
| clouds 320 x 40 | 4,800 |
| far islands + strip 320 x 60 | 7,200 |
| sea floor 640 x 28 | 6,720 |
| BOBs (3 planes + mask, word-aligned plus a shift word, both facings of boats and the destroyer, 4 barracks frames, 2 pad phases) | 41,728 |
| **total** | **77,440 (75.6 KB)** |

Plan estimate was 61 KB; **the pilot is 16.4 KB over it** (the plan's rule is World 1's arena plus at most 15 KB; World 1's arena is not measured here, so this is a risk, not a verdict). Levers, in
order: do not mirror the destroyer (3.9 KB), do the pad's dark phase by recolour (2.4 KB), one wake frame for the patrol boat (0.5 KB), let the ring be 336 px wide (0.2 KB),
share the gunboat's fire frame with tell 2 (1.2 KB): about 8 KB. The estimated **packed disk size is about 33 KB** (zlib -9 plus 12 % for LHA on the planar data, BOBs at 55 %),
far below the 150 KB per-world limit, so loading is not the limit, chip RAM is. Not counted: the chopper, hostages, shots, explosions (already resident), the HUD, music.

## 7 Retro Diffusion

Every request carries `input_palette` = the exact pens of the target layer (the 1-px strips; PF1 for objects, the mission palette for bands) and **never** other games' art.
Keep the base wording of the first pilot and add the water words:

- band base: `16-bit era pixel art, side view parallax background layer, crisp hand-pixelled clusters, limited palette, light from the upper left, ...`
- sprite base: `16-bit era pixel art game sprite, side view, crisp hand-pixelled, bold 1 pixel black outline, light from the upper left, limited palette, ...`

| asset | prompt_style | size | prompt (after the base) | result in the pilot |
|---|---|---|---|---|
| far islands | `rd_plus__environment` + `tile_x` | 320 x 64 | `a distant archipelago of low rounded tropical islands and two tall jungle covered volcanic cones, hazy blue-green, irregular silhouettes, the islands sit in calm sea at the bottom, pale empty sky above, no sun, no text` | used: silhouettes only (flat dark mass, plateau tops), re-lit and re-hazed by `icompose.far_band`, apexes carved |
| clouds | `rd_plus__environment` + `tile_x` | 320 x 64 | `two long bands of fluffy tropical cumulus ...` | **rejected** (filled the width, teal outlines); cumulus are built procedurally (circles with top-left lighting) |
| palms | `rd_plus__low_res` n=3 | 32 x 32 | `a pair of short coconut palm trees seen whole, each tree about twenty pixels tall, curved brown trunks, drooping green fronds, a little sand at the foot, small and compact, margin around the trees, nothing else` | used (2 of 3 locked to the island pens, the RD sand mound removed); the 32 x 48 first try was too tall for the 48-row strip |
| boats | `rd_plus__low_res` n=2 | 48 x 24, 64 x 32 | `an enemy gunboat in profile facing left, olive drab hull with a raised bridge, one large turret with a long gun barrel, red stripe, stone grey details, nothing else` | proportion references only (fuzzy 4-6 colour clusters, wrong facing); the sprites are built on the DSL in `ibobs.py` |
| destroyer | `rd_pro__default` | 160 x 48 | `an enemy destroyer warship in profile facing left, long low grey hull with a sharp bow, two gun turrets with long barrels, a tall bridge and a mast, anti aircraft guns, ...` | proportion reference (the bow had no hull); hand-built |
| radar | `rd_plus__low_res` n=2 | 32 x 40 | `a radar station on a small concrete base, a large round radar dish on a tower, ...` | shape reference |

Post-process as in the first pilot: `lock` to the slot colours (no dither), stray report empty, remove orphans, outline rule for PF1 objects, check at 1x and 4x on the real background.
Rule of thumb learned: RD is good for **silhouettes and ideas** (islands, palms, hull proportions) and not for the final pixels at 16-64 px; the kit pieces are drawn in code against those references.
Spend for this pilot: **$1.08** (balance $3.57 to $2.49).

## 8 QA for this world (added to STYLE section 9)

- [ ] `icheck.py` passes in all three missions (run, period-2/4, hard edge <= 64 px; no row more than 85 % one colour on lines 165-255 except 179, 180 and 207; bare-seam lines; no flat top over 24 columns, nothing in the first three rows)
- [ ] 3 x 3 repeat of every water and flat column shows no seam; an island run joins in any order
- [ ] every pen in `sea` and `island` differs from the backdrop gradient on the rows it dominates
- [ ] pad: 48 / 64 / 96 px built from 8-px units; the lit H and chevrons read at 1x from the chopper's height; the dark phase is clearly different from the lit one
- [ ] gunboat and destroyer turret: the tell frame differs from idle in silhouette (barrel angle), not only in colour
- [ ] chip total within the arena (section 6), re-measured with `isizes.py` after every asset change

## 9 Production (2026-11-13)

Frame sets are one size per asset (a text-art file needs it), cropped to the union of their frames so the pivot and the waterline stay registered:
patrol boat 43 x 17 (4), gunboat 61 x 30 (8), destroyer hull 154 x 44 (2), turret 32 x 19 (8), barracks 51 x 31 (4), pad caps 8 x 46 (4),
slab units 8 x 16 (6), centre 16 x 16 (2), lamps 5 x 5 (2), radar 33 x 39 (4), windsock 10 x 20 (2), splash 24 x 24 (5), bow spray 12 x 8 (2),
whitecap 34 x 10 (3). Chip total **78,720 B (76.9 KB)** against the pilot's 77,440 B (75.6 KB): +1,280 B, from the common frame boxes and
the pad parts kept uncropped on the 8 px grid. Packed disk 34.1 KB by the pilot's flat 55 % rule (pilot 32.9 KB); 17 KB with each set's own
zlib -9 + 12 %. `make_world2.py --convert` runs agk's converter over `art/world2/world2_art.toml`.
