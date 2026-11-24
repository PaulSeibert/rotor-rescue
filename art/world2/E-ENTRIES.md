# Slice E art (the mission 6 strait destroyer set piece): converter entries, chip, geometry

Branch `w2-art-e` (off main 5f60f11). Hand-drawn text-art, no Retro Diffusion spend. Nothing is wired in. Only the seven PF1 pens. One facing only (bow LEFT, as `destroyer_hull.txt` frame 0); the right-facing frames are NOT drawn (see "Facings").
Regenerate: `python art/tools/world2/e_art.py art/world2` ; checks: `python art/tools/world2/e_check.py art/world2` (PASS) ; previews: `python art/tools/world2/e_preview.py art/world2 <out dir>` (Windows Python + Pillow; run from `art/tools/world2` or put it on the path).

## world2_art.toml lines (paste after `[w2_destroyer_turret]`)

```toml
[w2_destroyer_hull_e]                  # 2 frame(s) 154 x 44: 0 = the hull as played, 1 = listing. REPLACES [w2_destroyer_hull]
source = "world2/hull_list.txt"
kind = "bob"

[w2_aa_gun]                  # 3 frame(s) 12 x 12: idle, tell, puff
source = "world2/aa_gun.txt"
kind = "bob"

[w2_turret_hit]                  # 2 frame(s) 32 x 19: smoking, dead
source = "world2/turret_hit.txt"
kind = "bob"

[w2_barrage]                  # 3 frame(s) 24 x 12: the barrage tell
source = "world2/barrage_tell.txt"
kind = "bob"
```
Drop `[w2_destroyer_hull]` from the build (it has both facings, 10,560 B, and its frame 0 still carries the painted AA gun). The existing `[w2_destroyer_turret]` (8 frames, both facings, 3,648 B) and `[w2_whitecap]` stay as they are; see the chip table for the one-facing turret lever.

## Chip (the converter's real format: rows x frames x ceil(w/16) x 2 x 6, no shift word, verified against world2x.bin)

| BOB | size | frames | formula | bytes |
|---|---|---|---|---|
| `w2_destroyer_hull_e` | 154 x 44 | 2 | 44 x 2 x 10 x 12 | **10,560** |
| `w2_aa_gun` | 12 x 12 | 3 | 12 x 3 x 1 x 12 | **432** |
| `w2_turret_hit` | 32 x 19 | 2 | 19 x 2 x 2 x 12 | **912** |
| `w2_barrage` | 24 x 12 | 3 | 12 x 3 x 2 x 12 | **864** |
| new in this job (without the hull) | | | | 2,208 |
| `w2_destroyer_turret` (existing, left frames 0-3 only) | 32 x 19 | 4 | 19 x 4 x 2 x 12 | 1,824 (8 frames, both facings: 3,648) |
| `w2_whitecap` (existing, Typhoon) | 34 x 10 | 3 | 10 x 3 x 3 x 12 | 1,080 |

**Slice E set total: 10,560 + 432 + 912 + 864 + 1,824 + 1,080 = 15,672 bytes** (with both turret facings still in the file: 17,496). The handoff's estimate was about 8.2 KB: the difference is the list frame (+5,280, a second full 154 x 44 hull) and the new BOBs (+2,208). The old two-facing hull file cost 10,560 too, so using `hull_list` instead of `destroyer_hull` is cost neutral against what is in the toml today; it is +5,280 against the "one facing" plan.
Against the a500 margin (116 KB free after slice D, floor 96, so 20 KB): E leaves about 4.3 KB, below the 12 KB the slice D note said triggers the per-mission set lever (SLICE2-HANDOFF, "If slice E leaves less than 12 KB over 96, take it"). Cheaper options if wanted: drop the list frame and let code do the "hull down" with smoke and the dead turrets only (-5,280); keep the turret's 4 left frames only by splitting `destroyer_turret.txt` (-1,824); the whitecaps are Typhoon (mission 6) only anyway.

## Decision: the painted AA guns
The old hull painted a small AA gun at x 43-53, y 21-29 (and another under the aft turret barrel, hidden by the turret). The four AA BOBs carry their own barrels, so **`hull_list.txt` frame 0 is `destroyer_hull.txt` frame 0 with that one painted mount cleared (57 px, x 41-53, y 21-29, replaced by plain deck and a K outline row)**; every other pixel is identical (`e_check.py` proves it). Use frame 0 of `hull_list.txt` instead of the old frame 0. The painted aft gun under the aft turret is left alone: the turret overlay covers it and it reads as the turret's pedestal.

## Geometry (hull frame = top-left origin, left facing; the BOB's last row sits on y 248 like every sea BOB, so the hull top-left is (hull x, 205))

**The 4 AA guns**: top-left of the 12 x 12 BOB, in hull px. The last row (row 11) is the gun's K base line; it sits on a hull outline row.

| slot | top-left (x, y) | stands on | list-frame y offset |
|---|---|---|---|
| A forward roof | (27, 12) | the forward housing roof, K row y 24 (x 21-39) | +3 |
| B forecastle | (42, 17) | the deck, K row y 29 (the cleared mount) | +2 |
| C bridge wing | (75, 6) | the bridge's aft wing roof, K row y 18 (x 76-83) | +1 |
| D funnel catwalk | (99, 4) | spans the gap between the funnel tops, K row y 16 | 0 |

Muzzle (flak spawn) in BOB px: idle (2, 3), tell (3, 2), puff (2, 1): use hull (slot x + 3, slot y + 1) for all frames. The BOB centre for a hit box is (6, 7). Frames: 0 idle, 1 tell (barrel up, red tip and red lamp), 2 puff (muzzle flash, drawn for the 4-6 frames after the shot). The tell is a silhouette change (barrel angle and a longer red tip), not only colour (STYLE-world2 gate). All four guns can be drawn with the same frame at once (the preview does).

**Turrets** (unchanged mounts, from world2_art.toml): left facing, aft turret top-left (122, 11), forward turret (-7, 11). The hit frames use the same top-left and the same housing (cols 16-31, rows 10-18; the housing's bottom row 18 is the idle one, so it keeps sitting on the mount). Muzzle at turret px (0, 12) -> hull (122, 23) aft, (-7, 23) forward; housing centre for hits turret px (24, 14) -> hull (146, 25) aft, (17, 25) forward. `turret_hit.txt`: frame 0 smoking (barrel bent, flames R/Y in a crater, a smoke column over the housing), frame 1 dead (charred, barrel hanging, a wisp). Draw order: hull, turret state, AA, then the chopper.

**Hull list** (frame 1 of `hull_list.txt`): the bow sinks 4 px and the stern rises 2 px (pitch around x 110), blocks above the deck (forward housing, bridge, funnels, aft block) move rigidly, the hull side is sheared by columns, rows 41-43 (bottom edge, ripple row, foam) are identical in both frames, so the waterline does not move. Black smoke streams aft from both funnels (baked in). When the hull lists, draw the turret and AA BOBs with these y offsets from their normal top-left: forward turret +3, aft turret -1, AA A +3, B +2, C +1, D 0 (from `e_art.list_offsets`). Both turrets are in the dead frame in that state (the design: list when both turrets are down); they may be drawn smoking first and dead at the list.

**Barrage tell** (`barrage_tell.txt`, 24 x 12, water effect: no outline, a cream fringe lit from the top left): the ranging marker is drawn where a shell will land; its centre is the BOB top-left + (11.5, 5.5). Frame by time over the 30 frame tell: frames 0-9 -> 0 (a small broken ring and a pit), 10-19 -> 1 (the ring opens, blue boil, red ticks), 20-29 -> 2 (full double ring, red ticks on four sides, yellow flash). Place each marker with its top between y 228 and 238 (the strip's sea, in front of the hull's waterline row y 246-248) at the landing x minus 12, three markers across the strait is what the preview shows (offsets about -30, 0, +30 px around the chopper's x). It must only be drawn over water: the strait is all water (2208-2816).

## Facings
Only the left facing is drawn. The destroyer "patrols the strait" in the plan: if it turns round, the right facing needs: the hull (frame 0 and the list frame redrawn bow-right with the light kept top left, STYLE-world2 forbids a plain mirror: +10,560), the turret states (+912), the AA gun (+432, barrel up-right, light still top left) and the slot table mirrored (x -> 154 - x - 12). If it holds one heading (as the plan for one crossing says), nothing more is needed. Open question for Paul in the report.

## What the previews show (art/tools/world2/e_preview.py; Pillow mock-ups of the mission 4 / 5 / 6 backdrops, not game screenshots)
`e_story_1x.png` / `e_story_2x.png` (per mission: intact, AA tell + turret tell 2, AA puff + turret fire, turret smoking, both dead and the hull listing), `e_aa_<m>_x4.png` (three AA frames on the hull), `e_turrets_list_<m>_x4.png` (smoking / dead / both dead / list), `e_barrage_<m>_x3.png`, `e_turret_hit_x8.png`, `e_aa_barrage_x12_m5/m6.png`, `e_hull_x5.png`.
