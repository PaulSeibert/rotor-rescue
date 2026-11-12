# The plain: art contract

For amiga-art. The engine is in (branch `perspective-floor`, SPEC section 8 and 11, decided by Paul,
2026-11-12); the art in the repo is a procedural placeholder (`make_plain.py --placeholder`). Paint the
two source files below, run the tool, and the game shows them. Anything that changes a size, a line or
a palette slot is engine work: ask amiga-hw.

Paul's brief: "as if there was a plain of some sort between the hills and the more distant mountains".
Content he picked: **scrub and flats, distant mesas, a dry lake or riverbed, a road or rail line.**

## 1. Where it is (game lines, in play)

| lines | what | speed (of the camera) | bitmap | palette (PF2 pens 1-7) |
|---|---|---|---|---|
| 120-169 | the mountains (`mountains.txt` rows 0-49) | 1/4 | the mountains band | mountains |
| **170-179** | **far strip** (`plain_far.txt`, 10 rows) | 1/4, one block | the same band, below them | mountains |
| **180-198** | **floor** (`plain_floor_src.txt`, 19 rows) | line j: (18 + j) / 72, 1/4 to 1/2 | its own | land |
| 199-223 | the hills (`hills.txt` rows 2-26) | 1/2 | hills | land |
| 224-255 | the ground | 1 | ground | land |

`mountains.txt` rows 50-75 are no longer shown in play (the title still uses rows 0-33 through
`title_mtn.txt`). The title screen keeps its old layout and shows no plain.

## 2. Files and the build

| file | size | who | what |
|---|---|---|---|
| `art/plain_far.txt` | 320 x 10 | **you paint** | the far strip; loops at 320 px |
| `art/plain_floor_src.txt` | 288 x 19 | **you paint** | the floor; row j uses columns 0 .. W_j - 1 (table below), the rest stays `.` |
| `art/mountains_play.txt` | 320 x 60 | the tool | `mountains.txt` rows 0-49 + `plain_far.txt`: what `art.toml` `[mountains]` converts |
| `disk/plainfloor.bin` | 640 x 19, 3 planes | the tool | each floor row repeated every W_j px, interleaved planes (4560 bytes): on the floppy, loaded during the splash (in the program it would push AROS's boot past its window) |
| `build/art/preview/plain_floor.png` | 640 x 19 | the tool | a preview of those rows in mission 1's colours |

```sh
python3 art/tools/make_plain.py        # checks the contract, writes the two generated files
agk art                                # previews in build/art/preview/ (mountains.png; the tool wrote plain_floor.png)
agk test --only floor --only parallax  # the per-line scroll and the bands, on every profile
```

Format: the band text art (as `mountains.txt`, `hills.txt`): a `colors` block, `frame`, one character
per pixel, `.` transparent, `1`-`7` the pens. **The pen number is what counts**: the colours in the
`colors` block are mission 1's (for the previews) and the copper swaps in each mission's palette
(section 4). A PNG works as a start: `agk art-export` makes text art of it; keep pens 1-7 dark to light.
`make_plain.py` stops with a message when a rule in sections 5-6 that it can check is broken.

## 3. The floor's geometry: one row per line, each its own loop

Floor row j (0-18) is game line 180 + j. It moves at **(18 + j) / 72** of the camera (row 0 with the
mountains, row 18 with the hills; linear down the screen, which is what a flat plain does: 1/z is
linear in screen y). Its loop is **W_j = 8 (18 + j) px**: every row repeats after the same **576 px of
camera**, so a feature drawn across rows stays one feature at every scroll position.

| row j | line | speed | W_j (px) | | row j | line | speed | W_j (px) |
|---|---|---|---|---|---|---|---|---|
| 0 | 180 | 0.250 | 144 (empty) | | 10 | 190 | 0.389 | 224 |
| 1 | 181 | 0.264 | 152 | | 11 | 191 | 0.403 | 232 |
| 2 | 182 | 0.278 | 160 | | 12 | 192 | 0.417 | 240 |
| 3 | 183 | 0.292 | 168 | | 13 | 193 | 0.431 | 248 |
| 4 | 184 | 0.306 | 176 | | 14 | 194 | 0.444 | 256 |
| 5 | 185 | 0.319 | 184 | | 15 | 195 | 0.458 | 264 |
| 6 | 186 | 0.333 | 192 | | 16 | 196 | 0.472 | 272 |
| 7 | 187 | 0.347 | 200 | | 17 | 197 | 0.486 | 280 |
| 8 | 188 | 0.361 | 208 | | 18 | 198 | 0.500 | 288 (empty) |
| 9 | 189 | 0.375 | 216 | | | | | |

**Perspective, the one rule for drawing:** think of the plain as a strip of ground 576 px long (world
px). A point at world x on row j is drawn at **column x * W_j / 576** (= x * (18 + j) / 72). So a
feature is narrower on the far rows than on the near ones by the same ratio (a 100 px lake is 25 px
wide on row 0 and 50 px on row 18), and its left and right edges lean toward the vanishing point. Drawn
that way, it shears as the camera moves exactly as real ground does. (The placeholder in
`make_plain.py` draws its lake, road and scrub with this formula: copy it.)

## 4. Palette per mission (no new slots)

Floor pixels use PF2 pens 1-7 loaded with the **land** palette; the far strip's use the **mountain**
palette. Transparent floor pixels show **COLOR00, the plain's ground**: a line-dithered copper gradient
from **H** (the sky's own colour on line 180: the haze) on row 0 to **C** (land pen 6) on row 18, and
C below the floor (it shows between the hill crests). Transparent far-strip pixels show the sky.

| mission | far strip pens 1-7 (mountains) | floor pens 1-7 (land) | H (row 0) | C (row 18 down) |
|---|---|---|---|---|
| 1 Desert Dusk | 413 524 635 746 857 A68 C9A | 310 521 742 963 B84 DA6 EC8 | D65 | DA6 |
| 2 Mountain Night | 012 123 234 345 457 79A BDE | 010 121 232 343 354 576 798 | 235 | 576 |
| 3 Coastal Dawn | 546 657 768 879 98A BAB EDD | 131 242 363 585 9A6 CC8 EEB | EA8 | CC8 |

(On the A1200 the ground gradient is 24-bit; the pens are the same.) Keep the bright pens (6-7) and
the dark ones (1-2) for small accents on the far rows: far away, contrast is low (the haze). Use the
whole range on the near rows.

## 5. Seams: the edges fall in the shared colour, never a hard line

1. **Floor row 0 is empty** (`.` only). It is the haze H, and the land palette loads on it (the tool
   checks).
2. **Floor row 18 is empty.** It is C, and so is everything the hills don't cover below it: the
   floor's near edge meets the hills in one colour (the tool checks).
3. **The far strip's foot is haze:** row 8 at most 50 % covered, row 9 at most 25 %, ordered (Bayer)
   dither, no run longer than 4 px on row 9 (the tool checks the coverage). Below it, row 0 of the
   floor is the haze, so the strip has no bottom edge.
4. **The far strip continues the mountains:** its row 0 is directly under `mountains.txt` row 49, x
   for x (same band, same speed, same palette), so paint the two as one picture: the mountains' foot
   fades into the haze inside the strip (the placeholder fades rows 50-59 of the old foot with a 4x4
   Bayer ramp over the 10 rows).
5. **The far rows fade in:** rows 1-3 sparse (a few light flecks), density and contrast rising toward
   the viewer. No solid horizontal line anywhere: a row filled edge to edge with one pen looks still
   when it scrolls and reads as a stripe.
6. **Line dither:** COLOR00 is line-dithered on OCS, like the sky. Don't fight it with a horizontal
   pattern of the same 2-line period.

## 6. What goes where

| | far strip (rows 0-9, one speed) | floor rows 1-15 | floor rows 16-17 |
|---|---|---|---|
| mesas, buttes, anything taller than one line | **yes** (up to ~8 px, feet in haze, rule 3) | **no**: it shears | no |
| scrub, rocks, tufts | yes | yes: one line high (1 x 2 px or so), the plain's texture | yes, one line high |
| dry lake, riverbed, salt flat | no | **yes**, drawn in perspective (section 3) | no |
| road or rail line | no | **yes**: along a row (constant depth), 1-2 rows; give it marks (dashes, sleepers, posts every few px) or it won't be seen to move | no |
| anything crossing into row 18 | | no (rule 2) | |

A lake or road may span rows only if drawn with the section 3 formula; then it shears like ground.

## 7. Optional: the plain behind the hill crests

Between the hill crests (lines 199-218) COLOR00 shows C, flat. The hills scroll at 1/2, the floor's
near speed, so near-plain texture (land pens, flat, one line high) may be painted into `hills.txt`'s
transparent pixels on rows 2-21. It scrolls with the hills; keep it sparse next to row 2 (the seam with
floor row 18, which is C).

## 8. Budget (don't grow it without amiga-hw)

- Chip RAM: the floor is 640 x 19 x 3 planes (4.6 KB); the far strip replaced 26 mountain rows the play
  band no longer shows (3.8 KB), so the plain costs about 0.7 KB of chip. The floor's art is on the
  floppy (1.2 KB packed), not in the program. Kickstart 3.1's chip floor is 64 KB free.
- Copper: 4 entries a floor line (7 on AGA), filled in when the camera moves.
- Sizes are fixed by the engine: 10 far rows, 19 floor rows, W_j as in section 3, 288-wide source.
