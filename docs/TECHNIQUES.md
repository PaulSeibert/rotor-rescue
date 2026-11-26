# Rotor Rescue: techniques to reuse

Techniques this game uses that the next game can copy. Each entry says what it is, what it cost, and
what to watch. The engine-side write-up is in the kit (`docs/colourcycle.md`); the rules of the sea and
the strip are in `docs/SPEC.md` section 11.

## Colour-cycled water with phase-assigned pens (base ocean, SPEC v1.43)

**What it is.** The terrain strip's water (lines 208-255, where the islands and ships stand) moves with no
new copper entry and no chip bytes. The water is drawn in three palette pens (1, 6 and 7), and the copper
lets those three colours run through a wave, so the water reads as rolling.

**The art decides which pen each stroke gets.** `art/tools/world2/w2tiles.py` (`water_cycle`) draws each
water stroke (a horizontal run of pixels) in one of the three pens by the row's wave phase:
`u = 2.2 * ln(1 + y / 12)` waves at strip row `y`. A wave is about 5 rows long at the top of the strip and
about 25 at its foot, so the water looks farther away at the top. The class is `int(3u) % 3` plus a random
jitter of +-0.2 to +-0.4 of a wave a stroke (more at the foot), so no row is a band and no two neighbours
match. Strip row 0 uses pens 6 and 7 only (`w2compose.py`, the strip row 0 rule; the palettes switch at
line 207).

Islands never cycle. `island_pens` moves what is left of pens 1, 6 and 7 after the water is marked to pens
2 and 5. The water carries marker values (16, 17, 11) until it is cycled, so the generator tells water from
island by who drew the pixel, not by its colour.

Why the phase matters: a static art file with one palette ramp flashes the whole strip. The phase assignment
is what makes the cycling read as crests travelling.

**The wave (`logicBaseWave`, `logicBaseWaveTab`).** The sum of three movements, computed per class. Every
cycle is a divisor of 65536, so the 16-bit frame counter wraps with no jump:
- **the swell**, a crest of light rolling toward the player: 32 steps of 2 frames, a cycle of 64. Classes 0,
  1 and 2 start 0, 11 and 21 steps on;
- **a counter-wave** a third as strong, running the other way and slower: 32 steps of 8 frames, a cycle of
  256, so the crests do not all move one way at one speed;
- **the breathing**, how deep the dark goes (3 to 6 sixteenths of the way from pen 1 to pen 6): 32 steps of
  16 frames, a cycle of 512. The strip slowly deepens and clears.

A colour is the dark, through the mission's light (pen 6), to near-white (pen 7, never above 13/16 of the
way). Near-white is the crest's peak, the drifting foam: only pen 7 gets there, about a fifth of the strokes
at any time. The mean luminance of the open strip water moves by about +-6 % over a cycle (measured), so the
whole strip never flashes and no banding shows.

**Not done, on purpose:** swelling the strip's lines. The islands share the bitmap with the water, so a swell
would shear them, and there is no clear water below them to swell into.

**Copper cost.** `basePens` rewrites the three existing island-palette MOVEs (pens 1, 6, 7) in each copper
list, and only when the step has moved. No entries are added to the list and no chip bytes are used.

**Memory.** A colour table of 4 depths x 17 levels per mission (`BASE_TAB_N` = 68 words), `s_pBaseTab`:
408 bytes of fast RAM for the three missions. Entries start at 0xFFFF and are filled the first time the wave
needs them, not at the world load.

**Frame budget: what it cost, and the fix.** The first build computed the three colours in full every picture.
On an A500 that cost 2 to 5 points of the worst picture (SPEC v1.43 measured it): `w2play` went 85 -> 90
against its limit of 85, and `w2frtr` went over its limit of 98 (99 % with a dropped frame). The fix has two
parts:
1. the per-mission table, filled lazily (above);
2. gating, the same rule as the sea's swell: the pens are written only in a picture that queued at most
   `SEA_BLIT_MAX` (14) blits, and not in a list's first pictures (`s_ubSeaHold`). The step depends only on the frame
   counter and the blit count, so runs are deterministic and Ctrl+L, S holds the frame, as it does for the sea.
   Writing in every picture was measured again after the fix below and costs 2 points of the worst a500 picture
   (`w2play` 85 -> 87 against 85): the three chip-RAM MOVEs and the lookups are not free, so the gate stays.

**Flicker: gating a double-buffered write needs a rule between the lists (Paul's playtest, 2026-11-25).** The first
gate said "a busy picture takes the step up in the next quiet one". It was wrong with two copper lists: a list that
skips keeps the pens of its last write, and the picture shows its own list, so the water flipped between two phases
every picture, or jumped many steps when a quiet picture caught up. Measured in `tests/w2play.agk` on the a500 (people
boarding, boats): 31 % of the pictures were busy, in runs of 22; the lists were up to 14 steps apart, a list was up to 29
steps behind the frame, and a pen channel jumped 9 levels in one picture (the wave moves 3 a step at most). A skip is
fine; two lists out of step are not. The rule (`logicBasePenKey`): a list written goes to the other list's step, or one
step past it when they are level, never past the frame's step. The lists then differ by one step at most (the wave's own
step), at the cost of a slower wave in busy stretches (about a quarter of its pace through the boarding; it never stops
while a picture is quiet), and the wave falls behind the frame counter for good, which nobody can see in a slow cycle.
A catch-up that jumps to the frame's step is the same mistake as the first gate: do not add one. The metric is the
largest change of a pen channel between two shown pictures (`testBaseFlicker`, on the measured blit counts) and in the
game `bspread`, the lists' largest difference (`tests/w2play.agk` expects 1 at most).

**Test.** `tests/w2base.agk` (synced to the game frame, as `tests/w2sea.agk`): picked strip-water pixels must
differ between frame 288 and frame 352, on the a500 and the a1200 shots. On main before this change every
picked pixel was the same, so the test was red there.

**Watch.**
- After a list is written, the pens wait out that list's first pictures, so the change lands a picture late.
- Any later change to the gate must keep the lists' steps within one of each other (`bspread`), whatever it skips.
- The strip's row 0 is pens 6 and 7 only; a new art pass must keep that rule (the generator enforces it).
- On the a500 the world 2 load waits in the test scenarios went from 400 to 700 ticks (see Open).
- Native port: `s_pBaseStep`, `s_pBaseTab` and the `basePens` patch may need the kit's `rescue.json` rule
  counts updated before a native build.

**Open.** The world 2 load on the a500 measured 350-380 ticks on the build before this change and 400-430
here. The commit blames the floppy read moving with the disk layout; that is not proven. Treat the a500
world-load slack as thin until someone measures it.
