# Rotor Rescue: game spec (retroactive)

Written after the fact, on 2026-11-11, from the code at `main` (2c0c4f9, "Goldens: the dithered
sky, the tank tell, quarter-pixel bands, the new screens") and from the repo's own tests and
commit messages. It describes the game as it is: where a Desert Palms playbook target isn't met,
the section says **Gap vs playbook** in plain words. It does not change the game. From now on it
is the contract everyone works from: change the design, change this file in the same commit.

**v1.5 (2026-11-12): approved by Paul, the commercial content plan** (`docs/CONTENT-PLAN.md`): twelve missions in
four worlds. Sections 4, 6 and 10 now carry the approved full-game design; sections 1, 2, 5, 7, 9, 11 and 12 are touched
only where it requires. **Not built yet.** Text marked *shipped* describes the game at `main` (world 1) and stays true
for it; text marked *approved, not built* is the contract for work to come. Where a number is "studio default" Paul has
not chosen it: revisit at a playtest gate.

Sources, used throughout: `src/logic.c` / `src/logic.h` (rules, tuning, tables), `src/main.c`
(display, sound, input, text), `src/hiscore*.c` (the kit's `hiscore` technique, v1.10), `sound/sound.toml`, `art/art.toml`,
`art/palette.txt`, `agk.toml`, `tests/*.agk`, `tests/unit/test_logic.c`, and the art pilot's
`~/art-pilot/first/STYLE.md` and `NOTES.md` (approved 2026-11-11). References like `logic.c:454`
are line numbers at 2c0c4f9. "Derived" means worked out from the code's tables, not measured in
a run. "Landing" means in flight on another branch when this was written (see section 8).

## 1. Pitch

You fly a rescue chopper along one long, flat battlefield, 3072 px wide (about ten screens).
Four barracks, spread out between the enemy border fence and your base on the far right, hold 16
hostages each. Blow a barracks open with the cannon (two shots) or a bomb (one), land beside it so
the people can run aboard, then fly the cabin full (16) home through tank shells, jets and air
mines, and land on the pad so they run into the hangar. The first hit only damages the chopper (smoke,
slower, everyone aboard kept); a second hit while damaged destroys it with everyone aboard. 64 people, three missions (*shipped:* Desert Dusk, Mountain Night, Coastal Dawn; v1.5 grows this to twelve missions in four worlds, section 4), three
choppers for the whole game. A side-view rescue shooter in the 8-bit helicopter-rescue tradition,
built from scratch: our own code, art, music and sound.

**Pillars**
1. **Land to save:** the rescue needs you low and still, which is where the tanks are. The risk
   is the game; flying is just how you get there.
2. **Every shell has a tell:** a tank stops, swings its barrel up and flashes before it fires
   (12-22 frames by skill, 16 on normal). Jets are heard and seen on the radar before they appear.
3. **A500 at 50 fps:** player, camera and parallax update every frame on a stock A500.

**Not:** a scrolling-shooter with waves or bosses; a game with lives by score; a game with fuel or long stretches where
you cannot land. *Shipped:* one battlefield repeated three times with harder numbers. *Approved, not built (v1.5):* four
worlds with their own battlefields, see section 4.

**Pillars for the full game (approved, not built)** - 4. **Read the ground:** the terrain is the level; you plan a route,
a landing and a retreat from what you see. 5. **Prepare, then land:** every threat to a landed chopper is visible,
telegraphed and clearable beforehand. 6. **One new idea per world:** the first mission of a world teaches it with less
pressure, then it ramps. Terrain never kills: it is a wall to route around.

## 2. Target machine

| machine | what it gets |
|---|---|
| **A500 OCS, PAL 50 Hz, Kickstart 1.3** | the whole game. README and `agk.toml` state **1 MB (512K chip + 512K slow)**: the 248.5 KB program (v1.13: 325 KB before the unused ACE code was linked out) lives in slow RAM. 50 fps, worst frame 80% (section 11). |
| 512K chip, no expansion | **does not boot: AmigaDOS says "Unable to load first: Error code 103"** (no free store). Measured 2026-11-11 on vAmiga with an A500 OCS, Kickstart 1.3 and no slow RAM (decision E, decided by Paul, 2026-11-11: measure only, nothing shrunk); the numbers are under the table. |
| **A1200 AGA** (found at boot: Lisa's DENISEID low byte `$F8`, `agaDetect` in `main.c`) | a 24-bit copper sky on all 172 sky lines (4 MOVEs a line via BPLCON3 LOCT), a 224-colour title logo (8 planes, `titlelogo8.bin`), quarter-pixel parallax (BPLCON1 35 ns bits) for the clouds, mountains and hills. The game itself is the OCS game. |
| Kickstart 3.1, AROS ROM | tested profiles `a500-ks31`, `a500-aros`. AROS loads the program at about 5 frames a KB; the first picture arrives at frame 2590 against the harness's 3000-frame boot window (2983 before phase 0's memory pay-back moved the chopper, the BOBs and the bands to a floppy file, section 11). `tests/memory.agk` keeps it at about 2692 or less (the game prints `bootvbl`, CIA-A's TOD count at the first picture, which trails the harness's frames by the ROM's start-up: AROS 102, Kickstart 3.1 138, Kickstart 1.3 333). |

**The 512K measurement** (vAmiga, scheme `A500_OCS_1MB` with `mem set SLOW_RAM 0`, a local profile
(the kit's profiles all have more than 512K), the build at `design-decisions`, 2026-11-11). It never
gets as far as `AGK ready`: the DOS reports error 103 when it loads `first` (the screen is on the
CLI with that message at frame 800, and still at 3000).
- **Free store with nothing loaded:** read from Exec's memory list after the failed load: one
  region, 512K chip (`$8E8-$7E800`), **400,944 bytes free** in nine chunks, the largest 400,536
  bytes. Kickstart 1.3 and its CLI use about 123 KB of the 512K.
- **What the executable asks for:** five hunks, all "any memory": 396,560 (code and data), 440,
  33,780 (BSS), 40 and 28 = **430,848 bytes**. LoadSeg takes the first hunk (it fits) and fails on
  the 33,780-byte one: **29,904 bytes short** for the executable alone.
- **What it would need to play:** on the 1 MB machine (512K chip + 512K slow) the lowest free store
  during play is 85 KB chip + 4 KB slow (`AGK mem`), so the game uses about 834 KB there
  (1,048,576 less 89 KB free less the ~123 KB the OS takes): the 430,848-byte executable plus about
  403 KB of allocations (`main.c`: frame buffers, HUD, copper lists, logos, the rotor wave and so
  on). Derived, to the nearest KB: 2.1 times what a 512K machine has free.
- **So it needs 1 MB.** A 512K master would have to load in under 400 KB and run in that, which
  means streaming most of the data from the floppy, not a trim. Nothing was shrunk for this test.

**Gap vs playbook:** the playbook's master is a 512K A500 that boots and plays complete. This
game needs the 1 MB expansion (measured above); moving data to the floppy (as Rotor Raider does) is
the route; not planned.

**Memory rules for the full game (approved, not built; Paul, 2026-11-12):** the game stays at **1 MB** (512K chip + 512K
slow). The executable does not grow (target 430 KB or less, new code is paid for by moving resident art and sound to disk).
Each world is a packed file of 150 KB or less loaded from the floppy into one reused chip arena. Lowest chip free in play
stays at 64 KB or more on the a500 and a500-ks31 profiles, re-measured per world. The AROS profile's boot window may be
raised slightly in the test harness if the program needs it; that is a test setting, not a game limit.
**Phase 0 memory pay-back (2026-11-12, branch `phase0-memory`):** the chopper's frames, the 14 BOBs and the 5 parallax
bands left the program for one floppy file (`diskart.bin`, 74.5 KB, 23 KB packed), and the chopper's sprite DMA reads two
small blocks per copper buffer instead of two resident sets of all 32 frames. The program went from 424,432 to 348,940
bytes. On the A500 profiles the program had filled the 512K slow RAM (0 KB free), so allocations spilled into chip RAM:
every KB taken out of the program came back as chip. (v1.8: the songs left the program too, 348,940 -> 318,184 bytes, now as floppy files; section 9. Chip free stays as below, slow RAM free 49 -> 70 KB on a500, 8 -> 29 KB on a500-ks31.) Lowest chip free in play (`perf`, `memory`), before -> after:
a500 73 -> 119 KB, a500-ks31 66 -> 153 KB, a500-aros 149 -> 159 KB, a1200 1058 -> 1153 KB; slow RAM free 0 -> 49 KB
(a500), 0 -> 8 KB (a500-ks31). First picture (harness frames from power-on): a500 2640 -> 2196, a500-ks31 2202 -> 1820,
a500-aros 2983 -> 2590, a1200 1788 -> 1468. `tests/memory.agk` asserts at least 96 KB of chip free in play on every profile
and the AROS margin.

## 3. Core loop and feel

**Controls** (one-button joystick in port 2 plays everything; a second button makes turning quicker)

| input | verb |
|---|---|
| stick left / right | fly: accelerate to 3 px/frame with inertia |
| stick up / down | climb / descend; descend onto the ground to land; up to take off |
| stick held against the facing | turn, through facing the screen (side, front, other side) |
| fire, facing left or right | cannon (6 px/frame, repeat 10 frames, 3 shots live, 36 frames of life) |
| fire, facing left or right | the cannon, always: taps, a held fire, any volley, whatever the stick is doing (Paul's playtest, 2026-11-12; studio proposal) |
| stick held down 20 frames with no fire in them, then fire | turn to face the screen: the bomb stance (the press turns, it doesn't shoot). Firing at any time restarts the 20 frames, so a volley never gets there |
| fire, facing the screen | drop a bomb (airborne only) |
| up + fire, facing the screen | turn back to the side you came from: the cannon (so: bomb the tank, up + fire, shoot the jet) |
| left / right, facing the screen | a sidestep: 2.5 px/frame, full speed in ~5 frames |
| fire 2, or X | turn: left, front, right, front... (always allowed, never times out). From the side it is the quick way to the bomb stance: fire 2, then fire, bombs at once, with no 20-frame hold (the stance stays until turned: fire 2 again, or up + fire) |
| keys | arrows, Space fire, X turn, P pause (in play), M music, Esc quit. While the hall-of-fame initials are up the keyboard types instead: only the stick's button is fire, and Space, P, M and Ctrl+L are not read; Esc ends the entry and keeps the score (section 10) |
| Ctrl+L, then 0-5 | difficulty (section 7); Ctrl+L, T / H: debug dumps (parallax fine scroll, hall-of-fame file read-back) |
| fire on the mission card | skip it |

**Feel in numbers** (`logic.h:241-266`, `logic.c:714-827`)
- Side flight: `VX_MAX` 48 = 3 px/frame; `VX_ACCEL` 2 and `VX_DRAG` 1 (1/16 px per frame each), so
  24 frames to full speed and 48 to a stop: a heavy machine.
- Head-on sidestep: max 2.5 px/frame, accel 8 (5 frames), reversal 12 (~9 frames), drag 5 (~8 frames
  to a stop).
- Vertical: up max 1.75 px/frame, down max 2 px/frame, accel 3, released decel 2. Take-off starts at
  0.4 px/frame.
- Turn (Paul's playtest, 2026-11-12; studio proposal, pending his playtest): eased, no snaps. Side to front is
  the three-quarter view for 5 frames (`TURN_STEP_FRAMES`), then a **lean** frame for 6 (`LEAN_FRAMES`)
  toward the way the nose is swinging, then (going on to the other side) the three-quarter view
  for 5 and the side: left to right = 16 frames, side, 3/4, lean right, 3/4, side. Leaning left is
  the mirror. Every pose of the sequence is shown, in order. The stick must push against the facing 5
  frames to start a turn, 20 frames from the front (so you can strafe while bombing).
- Head-on strafe (Paul's playtest, 2026-11-12; studio proposal): the sidestep keeps its speed (2.5 px/frame,
  5 frames to full: unchanged, e46d2a1) and gains an eased lean into the travel: front, slight lean (from 0.75
  px/frame), full lean (from 1.75), one pose every 3 frames (`LEAN_STEP_FRAMES`) and back the same way, so a
  5-frame ramp shows every pose and never snaps; a reversal passes the upright pose, never lean to lean.
- Front-facing hover sways gently (front 24 frames, slight lean one way 24, front 24, slight lean the other 24: 96
  frames); a sidestep leans into the move (slight at 0.75 px/frame, full at 1.75).
- Bomb stance times out: a stance entered by the stick (down, then fire) turns back to the side 75 frames
  after the last bomb; a stance entered with X stays until you turn it.
- Facing is separate from moving: the chopper keeps its velocity through a turn.
- Camera: locked to the chopper's pixel, with a 56 px look-ahead that glides (accel 1/8 px per frame
  squared, max 2 px/frame) and a scroll speed that changes by at most 1 px a frame; a long pan after
  a crash brakes to arrive (max 12 px/frame). Everything updates at 50 Hz.
- Cannon: 6 px/frame plus the chopper's velocity; tilted forward at speed it points down a little.
  Bomb: leaves at the chopper's half speed, 1 px/frame down, gravity 3/16 px per frame squared,
  life 120 frames; barracks roofs stop it. Bombs are unlimited; cannon shots and bombs share the 3 player-shot
  slots.
- People: run to a landed chopper at 1.25 px/frame (they hear it from 112 px), walk to the hangar at
  1 px/frame, wander at 0.5 px/frame around their barracks. Cabin 16. Boarding is a contact within 3 px.

**Hitbox (hero syndrome)**
- The chopper's sprite is 32 x 20; its hitbox is 26 x 13 (x-13..x+13, y-13..y, `logic.c:987`):
  about 3 px less each side, 53% of the sprite area. **Gap vs playbook** (25-50%): slightly large.
- Enemy shots are 4 x 4, mines 12 x 12 (a 14 x 12 target for the cannon), the jet 28 x 10 for a ram
  and 32 x 12 for the cannon. The cannon's box is 6 x 3.
- **Damage instead of instant loss** (decided by Paul, 2026-11-11; the hero device). The first hit **damages** the
  chopper: it trails smoke (a puff every 3 frames in flight; every 6 landed or hovering, under half a pixel a
  frame, where they would pile up on one spot; the puffs go with the chopper in the explosion that ends it:
  v1.9, frame budget), flies about 20 % slower (`VX_MAX` 48 to 38, the sidestep 40 to 32, climb
  28 to 22, descent 32 to 26), everyone aboard is kept, and the HUD's right slot reads DAMAGED in
  place of the skill. The hit is followed by `INVULN_FRAMES` = 100 frames (2 s) in which nothing
  hurts the chopper (shots and mines pass through, a ram does nothing) and it blinks (4 frames
  visible, 4 hidden). A **second hit while damaged destroys it** with the old loss rules: everyone
  aboard is lost, 70 frames of crash, 150 frames of the LOST state (3 s), respawn on the pad.
  **Landing on the home pad (x 2800-2940) repairs it**; so does a respawn. GODMODE ignores hits as
  before and is never damaged.

**Fun at three scales**
- 10 s: land under a tank's tell and lift off as the barrel flashes; shoot a jet you heard coming.
- 1 min: a trip: shoot a barracks open, land, 16 run aboard, fly home, they run into the hangar.
- 10 min: three missions of four trips, one rank for everyone saved.

**Scoring** (`logic.c`)
| event | points |
|---|---|
| a person reaches the hangar | 100 (max 6400 a mission) |
| barracks opened | 25 |
| tank | 50 for the first `TANK_QUOTA` (10) kills of a mission, then 0 (decided by Paul, 2026-11-11); the HUD's left slot shows QUOTA 10 for 4 s when the quota is reached |
| jet | 100 |
| air mine | 75 |
| mission cleared | +10 per person saved |

Rank from people saved over the game (`logicRank`): under 40 ROOKIE, 40 PILOT, 90 CAPTAIN, 140 HERO,
180 LEGEND (of 192). Kept on death: score, saved count, the opened barracks and who is still free.
Lost on death (the second hit): everyone aboard. No extra lives by score (**Gap vs playbook**, see 7).

## 4. Structure and content budget

*Approved, not built (v1.5).* **Four worlds, twelve missions.** Each mission has 64 people (the finale's last site is two
trips of 16). A mission ends when everyone is saved or lost. Worlds are separate battlefields with their own parallax,
terrain, enemies and music. World 1 is the game as shipped (4.2). Order: desert, islands, mountains, city (decided by Paul).

| mission | world | name (studio default) | length px | sites (people) | new element | pressure (M1 = 1.0, estimated) | minutes (estimated) |
|---|---|---|---|---|---|---|---|
| 1 | Border Desert | Desert Dusk | 3072 | 4 barracks (64) | as shipped | 1.0 | 6 |
| 2 | Border Desert | Desert Night | 3072 | 4 barracks (64) | missile truck, garrisons, the clear-then-land loop | 1.25 | 6 |
| 3 | Border Desert | Desert Dawn | 3072 | 4 barracks (64) | AA gun site; the border convoy set piece; jet pairs as shipped | 1.5 | 7 |
| 4 | Coral Islands | Reef Run | 4608 | 4 island barracks (64) | water, islands, small pads with the landing guide, gunboats | 1.1 | 5.5 |
| 5 | Coral Islands | Trade Winds | 4608 | 3 barracks + ship deck (48 + 16) | wind gusts, the ship-deck winch, radar sites | 1.4 | 6.5 |
| 6 | Coral Islands | Typhoon Strait | 4608 | 3 barracks + sinking freighter (48 + 16, timed) | time pressure; the destroyer set piece | 1.7 | 7 |
| 7 | Highlands | Foothills | 4096 | 4 barracks at 4 heights (64) | height-map terrain, slopes, passes, barracks at varied heights | 1.2 | 6 |
| 8 | Highlands | High Pass | 4096 | 3 barracks + summit pad (48 + 16) | cloud ceiling, summit pad, wind in passes, radar peaks | 1.6 | 7 |
| 9 | Highlands | Cliff Ledges | 4096 | 2 barracks (32) + 4 ledge winches (8 each) | ledge winches, the tunnel, the summit helicopter duel | 1.9 | 7.5 |
| 10 | Harbour City | Night Docks | 3584 | 4 rooftop and dock barracks (64) | rooftop pads, searchlights, the lit gauntlet | 1.5 | 6 |
| 11 | Harbour City | Burning Block | 3584 | 2 barracks (32) + 4 rooftop winches (8 each), fires timed | fire time pressure, enemy helicopters in packs | 1.9 | 6.5 |
| 12 | Harbour City | Last Light | 3584 | 2 rooftop barracks (32) + HQ roof (32, two trips) | the gunship finale, the evacuation | 2.2 | 8 |

About 80 minutes for a good first-time player (estimated; a clean run is about 40% of that). Missions 2 and 3 are renamed
from "Mountain Night" and "Coastal Dawn" (studio default). Pressure is a planning index, defined by the pacing checker once it exists.

**4.1 Worlds and what each introduces** (approved, not built)
- **World 1, Border Desert (missions 1-3):** flat desert, dusk / night / dawn, the shipped parallax and perspective plain.
  New: missile trucks and AA guns, garrisons at each barracks, the border convoy (a column crosses the road at about 50%;
  destroying the lead truck stops it; reward: a repair crate and a bonus).
- **World 2, Coral Islands (4-6):** (*v1.11: the display, section 11 "World 2's display". v1.13: the gameplay slice, section 11 "World 2's gameplay": islands, pads and their guide, barracks and people, water, patrol boats and gunboats, the way in from mission 3 and out after mission 6. Not built yet: wind, the winch, the timed freighter, the destroyer (radar sites: v1.20, slice A). v1.16: jets, section 6. Until the winch is, missions 5 and 6 are mission 4's islands with more gunboats and four barracks of 16.*) bright sea in morning, midday and storm dusk. Sky; clouds /8; distant volcanic islands /4;
  **the sea is the perspective floor** (re-cut to lines 180-207, wave rows in perspective, a 1 px shimmer by time); hills
  dropped; terrain strip from line 208 (48 rows, island height at most about 40 px). Water is not landable (touching it is a soft
  stop with a splash, no damage; decided by Paul). Pads (4.4), wind gusts, ship-deck winch, gunboats, radar sites, a timed
  sinking freighter. Set piece, M6 about 50%: **the strait destroyer** (two turrets of 6 HP, AA guards; anti-stall at 40 s
  a barrage, at 90 s the turrets overheat open). A field pad mid-map repairs damage (studio default, maps over 4000 px).
- **World 3, Highlands (7-9):** alpine dawn, noon, snow-storm dusk. Far range /4 above line 130; **terrain strip lines 130-255**
  (125 rows, about 17 KB ring); an optional 1:1 **cloud-ceiling strip** (lines 44-84) in the cloud-ceiling stretches; a COLOR00
  haze gradient behind the strip reads as valley depth; the perspective floor shows only in M7's low foothills. **The terrain
  shapes the route:** passes, valleys, a low cloud ceiling, a tunnel (studio default: kept). Barracks at four heights:
  valley, mid shelf, plateau, summit pad. Ledge winches where you cannot land. Set piece, M9 about 50%: **the summit duel**
  with an enemy helicopter (anti-stall: reckless at 40 s, forced to hover in range by 90 s).
- **World 4, Harbour City (10-12):** night port, a burning block at the blue hour, last light. Far skyline /4; the near cityscape
  is the terrain strip (lines 120-255); the perspective floor is a road or quay in the low gaps. Rooftop pads and winches between
  towers, searchlights (copper beams, with a beam-sprite fallback), fires, enemy helicopters. Set pieces: M10 **the lit gauntlet**
  (three searchlight towers and a crane AA nest) and M12 **the gunship finale** (section 6).
- **Rest beats and set pieces:** a rest of 4-8 s with no spawns after each cleared site and each sea or valley crossing, at
  least one every 80 s; a set piece between the second and third site, with 5-7 s of clear flight before it.
- **Design rule:** every stretch of flight has a place to land within about 45 s (a pad, flat ground or a ledge). No fuel.

**4.2 The terrain** (approved, not built)
- Each world carries a **height map** in columns of 16 px: surface height, flags (landable, wall, water, tunnel) and a ceiling for
  the ceiling and tunnel stretches. `floorAt` and `ceilAt` read it (`floorAt` already decides roofs; the hook exists). A
  slope or ledge is a floor you stop on but cannot board from, like a roof today. **Terrain never damages:** a wall bump
  costs speed and a spark (decided by Paul).
- Rules that use absolute heights today (a tank fires when skids y > 150; mines never go below y 200; the shadow at `SHADOW_Y`;
  people at `FEET_Y`) become altitude above the local floor.
- **No vertical scrolling** (decided by Paul): flyable height stays 192 px (`CEIL_Y` 44 to `GROUND_Y` 236); peaks and towers
  stay on screen, at most about 110 px.
- **Terrain strip:** a 1:1 ring buffer in PF2, streamed one 16 px column at a time, replacing the hills and plain on its lines;
  the copper band layout comes from a per-world table. Spiked before any world art is made.
- Per world about 100-150 terrain columns; top-10 columns cover 55% or more of a map, at most 40 one-offs; a map is about 1.2 KB
  (estimated).

**4.3 Streaming and disks** (approved, not built)
- Each world is one packed file of at most 150 KB, loaded behind its world card into one chip arena freed on leaving the world.
- **Two disks** (decided by Paul): Disk 1 boot, executable, title, worlds 1 and 2, hall of fame, save. Disk 2 worlds 3 and 4, the
  ending and credits. No world spans disks. One swap in a normal run, after mission 6 ("INSERT DISK 2"; the volume name is
  checked, no requester); "INSERT DISK 1" before the hall of fame is written after game over or the ending. A third disk is the
  fallback only if worlds run over 150 KB. A hard-drive install is a later extra (files are loaded by name).
- **Continue:** a checksummed save file (world, mission, lives, merit, upgrades; about 24 bytes) is written at each world card.
- The long maps need a per-world radar scale (the HUD radar is 256 px wide; today `ROUTE_SCALE` is 12).

**4.4 Pads and landing aids** (approved; *pads, the guide and water built, v1.13*; wind, winch and timed rescues not built)
- Pad width by skill 96 / 80 / 64 / 56 / 48 px (studio default), on the art's 16 px grid (32 + 16 n: 48, 64, 80 or 96) and cut down to the island's bed (64, 64, 56, 96 px, the base 80): so 96 / 96 / 80 / 64 / 48 on the widest bed and 48 px everywhere on skill 4-5. GODMODE plays as skill 3. Touch-down tolerance 6 px outside the edge. On skill 1-3 an
  assist nudges you 0.5 px/frame toward the pad's centre during the last 12 px of descent (within 20 px beyond the tolerance). Two lamps blink over the pad's ends (**yellow** once your skids are over the pad, red when not: PF1 has no green) once you are within 150 px, the shadow falls on the pad (and is not drawn over water). *Not built:* the HUD's PAD tick. **Landing is on pads only** in world 2: the beach, a barracks roof and the sea are floors you stop on, not land on. **Water** is a soft stop with a splash: the skids stop at the surface (y 236), one splash a touch (a new one after lifting 6 px), no damage, no life lost.
- **Wind gusts** (world 2 M5 on): 0.25 px/frame sideways for 3 s every 12-20 s; tell 50 frames before (windsock, spray or snow
  streaks, a rising howl). None while landed; none on skill 1; none within 100 px of a barracks on skill 2-3.
- **Winch** (world 2 M5 on; the box is 24 x 20 px on skill 5 and larger on skill 1-4, with a grace, a capture ring and a lamp that says "slow down": v1.44, section 11 "Winch and loading polish"): hover in a 24 x 20 px box above a ledge, deck or roof; 8 people take 8 s on normal; the climb is
  paused, not reset, if you drift out. A winch hover counts as a landed state for the ground-grace rules (section 6).
- **Timed rescues:** at most two a mission, never in world 1; 90 s on normal (120 / 105 / 90 / 75 / 60 by skill), shown on the
  preview card and the HUD; losing a site loses only the people still there.

**4.5 The upgrades** (approved; currency and list are studio defaults)
A hangar screen after each mission: spend **merit** (10 per person saved, plus medals for zone clears and set pieces) on four
upgrades of two levels each. Armour (the first hit of a trip is absorbed; refills on the pad; the second hit still destroys),
winch speed (30% / 60% faster), seats (cabin 18 / 20), cannon (4 shots live; repeat 8 frames). Tuned for no upgrades; none touches a
tell, a shot speed or a fairness rule; upgrades reset on game over; GODMODE ignores them. Skill 1 starts with armour level 1.

**4.6 World 1 as shipped** (still the contract for missions 1-3)

One battlefield, three times. World 3072 px wide: your base on the right (pad x 2800-2940, hangar
x 2960, start x 2880), the enemy border fence at x 2560 (enemy ground units stay left of it), the
four barracks at x 420, 1080, 1700, 2240 (far to near; 2460, 1800, 1180 and 640 px from the start).

| mission | sky (top, horizon) | weather | first tank after | tanks at the start (normal) | jets on from | mines on from |
|---|---|---|---|---|---|---|
| 1 Desert Dusk | 0x214, 0xE75 | dust motes | 700 frames (14 s) | 1 | after trip 1 | after trip 2 |
| 2 Mountain Night | 0x001, 0x235 | rain (4 px/frame, slanted) | 550 frames (11 s) | 2 | the start | after trip 1 |
| 3 Coastal Dawn | 0x237, 0xFB8 | sea spray rising | 400 frames (8 s) | 3 | the start | the start |

(Normal skill: jets and mines switch on when trips + mission index reach 1 and 2; the tank cap is
`1 + trip + mission`, at most 5. Section 7 gives all skills.) Game lines in play (`main.c`): HUD 0-23, clouds band y72
(40 rows, camera / 8), mountains y120 (50 rows, / 4) and below them the plain's far strip y170 (10 rows,
the same band and speed), the plain's perspective floor y180-198 (19 lines, each at its own speed from / 4
to / 2; the land palette loads at the end of line 179), hills y199 (their rows 2-26, / 2), ground y224
(32 rows, / 1). The title keeps the old layout: no clouds, its own mountain strip, a gap row at 196, hills
y197.

- **No tile map and no per-mission load:** the world is rules plus four looping parallax bands, all
  resident once loaded. Only boot loads from the floppy (logos, the art, hall of fame).
- **Disk:** one 880 KB floppy (`first.adf`, 901,120 bytes). Files: the executable (318.2 KB), `dpslogo.bin`
  (22.2 KB packed), `titlelogo.bin` (10.4 KB packed), `titlelogo8.bin` (18.8 KB packed), `plainfloor.bin`
  (the plain's floor, 1.2 KB packed), `diskart.bin` (the chopper's frames, the BOBs and the bands, 74.5 KB,
  about 23 KB packed: `art/disk_art.toml`, `art/tools/make_disk_art.py`), the three songs `m_theme.mod.pk` (17.9 KB), `m_fanfare.mod.pk` (5.3 KB)
  and `m_gameover.mod.pk` (0.7 KB), 33 KB unpacked (`sound/sound.toml` `disk = true`), and the
  hall of fame file `rrscores` (56 bytes) written by the game. About 420 KB of 880 KB used, derived. (v1.11: plus
  `world2.bin` (14.6 KB packed) and `world1.bin` (14.9 KB packed, world 1's bands again for the way back from world 2;
  `art/tools/make_world_disk.py`), section 11.)
- **Boot to play:** the DPS logo loads before the first picture; the art (in a frame of its own),
  then the title logo, then the songs (a frame each: fanfare, game over, theme) load during the black splash and the theme starts as the last one is in. Time from fire to play on
  a real floppy is **not measured** (open question 1); the AROS profile's first picture is at frame
  2590 (52 s of emulated time), its worst case. The art's load makes the splash about 190 frames
  longer on Kickstart 1.3 (130 on AROS) while the program loads about 450 (390) faster, so the title
  comes sooner overall; the scenarios wait up to 1000 frames for `AGK t0` (was the harness's 500).
- **Tile statistics:** n/a (parallax bands, no map). The art pilot's one-off budget is 40 feature
  stamps per mission band (section 8).

**Gap vs playbook:** no per-world load budget applies (nothing streams), but a 20 s fire-to-play
target is unproven; the preview card (2.4 s, section 10) is the only "level intro".


## 5. Pacing

**Jet pacing is measured (v1.3, "Jet pacing" below); the rest is not.** The playbook's threat metrics (mean, p90, quiet share, shots a minute) have not
been taken for this game: no `wavescript` or pacing run exists, and the spawns are timers and tables,
not a wave script. Everything below is derived from the code. Measuring it (a scripted trip in
GODMODE, as Rotor Raider's `tools/pacing.agk`) is open work (open question 3).

| playbook target | Rotor Rescue (derived) | met? |
|---|---|---|
| threat mean 2.5-3 | unknown. Tanks on screen at once: 1 + trip + mission (normal, cap 5); enemy shots on screen at most 6 (`ESHOT_MAX`); jets 2 (`JET_MAX`), mines 3 (`MINE_MAX`) | unknown |
| waves every 5-8 s | no waves. Spawn attempts every `220 - 40*mission + rand(0..127)` frames on normal: 4.4-6.9 s (mission 1), 3.6-6.2 s (2), 2.8-5.4 s (3); each tries one tank (if under the cap) and, once mines are on, a mine (a 1 in 2 roll in `enemiesSpawn`) | in range, by timer not row |
| spawn by map row, by time only when stopped | by time, off screen, only while the chopper is left of x 2760 (tanks, mines left of 2560, jets left of 2680): the base is a safe zone by construction | **gap** (time while the screen scrolls) |
| time to first threat 3-6 s | the first tank 14 s / 11 s / 8 s after the card; mission 1's jets need one trip home first | **gap**: slow start by design, "a quiet start" (`logic.c:414`) |
| rest beats, 4-8 s per zone | the base, the flight home, and the whole mission-start are quiet; nothing scripted | unknown |
| set piece at ~50%, quiet before it | none | **gap** (see 6) |
| fire ramp ~30/min first zone to ~100/min late | per tank in range: a shell every 85-116 frames on normal (31 a minute) at mission 1 trip 0 (base gap 85, was 110: slower shells are paid for in fire rate); `-20` frames a mission and `-8` a trip, floor 40-71 frames (54 a minute per tank). Up to 5 tanks, 6 shells live | plausible, **not measured** |
| next level +50% enemies | tank cap +1 a mission, spawn gap -40 frames, shell gap -20, jet rest -30 | yes by tables |

**Jet pacing** (Paul's playtest, 2026-11-12; studio tuning, pending Paul's playtest). Paul's playtest of 2026-11-12: "the jets, when the player is picking up
captives and is on the ground, even on the level 1 difficulty, they are too quick to bomb the player on the ground and it seems
unfair. also, two jets come together and the player has trouble transitioning from bombing to missiles. game seems just a bit too
hard still." Rules (section 6 has the mechanics, section 7 the tables):

- **Ground grace.** A landed chopper (boarding) is not bombed: never on skill 1-2 (the jets overfly it); on skill 3 only after
  6 s landed without a break, skill 4 after 4 s, skill 5 after 3 s. After take-off a 2 s grace: no jet starts an attack run.
  GODMODE uses skill 3's numbers.
- **One jet at a time.** Skill 1-2, and skill 3 in missions 1-2: one jet out at a time, the next waits until the first has gone
  (this lowers the jets' frequency). Skill 3 in mission 3, and skill 4-5: pairs, but the second jet's attack run starts at least
  3 s after the first's. (Read as: "on skill 1-3 one jet; on 4-5 or in mission 3 pairs" with skill 1-2 never paired in mission 3,
  so that skill 1 stays the easiest: studio decision, pending Paul's playtest.)
- **A warning, then time to switch weapons.** Every jet is announced the moment it appears: its radar blip flashes white and a
  short two-beep alert plays (effect `jetwarn`, priority 3, on the effect channels 1/3 like the others). No attack run starts
  until 2 s after that (it appears 384 px off screen, 100 frames of flight from being whole on screen; **a jet from behind** 576 px off screen and 3 s, see "Jets from behind"), and the jet's flash (tell)
  is longer on the low skills (24 / 20 / 16 / 14 / 12). The flash may begin up to three quarters of its length before the whole jet is on
  screen (else a long tell leaves a jet from the front only about 12 frames to begin one); the missile or bomb still needs the whole jet on screen at its end.
- **Jet pacing, measured** (`tools/pace.py`, ghost mode, 2.4 min of play, 39-40 % of it landed; before = `main` 71e762b plus the
  instrumentation, after = this branch; skill 1 at trip 3, skill 3 at trip 1):

  | | attack runs a minute | jets on screen (mean, share of time) | landed, a jet flash or shot up | first attack after touching down | runs under 3 s after another |
  |---|---|---|---|---|---|
  | skill 3 before | 26.8 | 0.61, 46 % | 13.7 % | 9.3 s, 1.0 s | 48 of 64 |
  | skill 3 after | 9.2 | 0.26, 26 % | 8.0 % | 10.2 s, 6.1 s (6 s dwell) | 8 of 22 (one jet's follow-up shots; two jets never closer than 2.9 s) |
  | skill 1 before | 14.1 | 0.65, 55 % | 18.3 % | 13.4 s, 0.3 s | 22 of 34 |
  | skill 1 after | 3.3 | 0.24, 24 % | 0.0 % (never, by design) | none | 0 of 8 |

  The coordinator's target was about half of before (12-15 and 6-8 a minute). Two tuning passes reached 9.2 and 3.3: one jet at a time
  gives at most about 11 passes a minute (3.8 s of flight, 40-70 frames of rest), and skill 1's jets cannot attack the 40 % of the run
  spent landed. Per minute airborne skill 1 is about 5.5 and skill 3 about 15. Left for Paul's playtest to say if it is toothless.
- Original wording of the measurement setup: `tools/pacing1.agk`, `tools/pacing3.agk` and `tools/pace.py` (ghost mode: the difficulty's tables, an
  immortal chopper; three cycles of 24 s landed and about 18 s airborne at the first jets-on trip): see the table in "Jet pacing,
  measured" below. Not measured with a person: that is the playtest.

**Jets from behind and the last barracks** (Paul's playtest, 2026-11-12; studio tuning, pending Paul's playtest). Paul: "the jets still come in from
behind too quickly" and "on the last barracks, the far one, about to clear a level, the jets are brutal, even on difficulty level 1."

- **From behind, measured** (`tools/jetbehind.c`, ghost mode, a jet forced to appear behind a right-facing chopper; frames from its appearing). A jet comes
  from behind in 1 of 4 (`r & 3` in `jetsUpdate`), flying 4 px/frame. Before: 384 px lead, 100-frame warning. A **hovering** chopper was
  rammed after 130 frames (2.6 s) and, on skill 3-5, had a missile at 119; one that **stopped** after 1.2 s had a missile at 181-183; one that
  **turned round** to face it at 1.2 s had its first attack run at 103-120 and its missile at 132-140 (2.6-2.8 s), with 0.4 s to see it,
  decide and turn (a turn is 17 frames). A chopper cruising on at 3 px/frame is overtaken at 1 px/frame: first run after 383 frames,
  ram after 525 (skill 1-3), no problem. After: `JET_LEAD_BEHIND` 576 px and `JET_WARN_BEHIND` 150 frames (3 s) for a
  from-behind jet (from ahead stays 384 px and 100 frames). Hover: ram at 178 frames, no missile; stop at 1.2 s: first run at 207-213, missile 229-231;
  turn round at 1.2 s: first run at 150, missile at 166 or none, contact at 173; cruise: first run after 559-565. The blip and chirp are as
  before (the moment it appears, 576 px off screen: the radar holds it at the edge). Unit test `testJetFromBehindHasTime`: over skills 1, 3 and 5 and
  hover / cruise / stop / turn round, no attack run and no ram within 150 frames of a from-behind jet's appearing.
- **The last barracks, reproduced** (`tools/jetsim.c`: a bot flies whole trips on the real rules in ghost mode, base - barracks - land, open, board - base,
  nearest barracks first, 300 seeds; fast on the host, so the numbers are the bot's, not a person's: compare trips, not absolutes). **Cause, three parts:**
  (1) **Jets come by time in the air, and the far trip is the longest**: the nearest barracks is a 21 s trip, the far one 45 s; jets appear about one every 5 s of flight
  whatever the position (a jet rests only 40-103 frames between passes), so jets a trip rose with the trip: 4.6 / 7.5 / 10.5 (skill 3, mission 1), 4.0 / 9.0 / 13.5 / 17.9
  (skill 3, mission 3). No table or timer is indexed by barracks; position matters only through distance. (2) **A long straight cruise lines the jet up**: attack runs a
  minute in the air are 0.3-1 near the base and 2-4.6 over the far half of the map (a jet follows your height at 0.5 px/frame and shoots when level; short, busy
  hops never line it up). (3) **On skill 1 in mission 1 the first jet of the game arrives on the last trip**: `jetsFrom` is 3, so with the nearest
  barracks first the far one is the debut and the climax at once, 8.9 jets and 1.45 attack runs in that one trip (skill 1, mission 1: 0 / 0 / 0 / 8.9). Not the cause: the tanks
  (they grow by trip too, and are near the chopper 20-58 % of a trip, but the bot took at most 4.4 shell hits a minute and none at the far barracks on skill 1; not changed here), the mission timers (none), anti-stall (none), the time airborne (the same share per minute).
- **The fix: a jet budget per trip** (`s_pJetsTrip`, `logicJetsTripMax`, `g->jetsTrip`): at most 2 / 3 / 5 / 6 / 6 jets appear between one landing on the home pad and the next
  (skill 1-5; GODMODE 5 = normal), then none until the next trip. A budget, not a rest, so the first jets of a trip are as the fairness pass left them and a long
  flight stops adding. Each is about 1.4 x the early trip's own count (skill 3, mission 3: 4.0 early, 6 cap), so far / early is at most 1.5.
- **Before and after** (bot, 300 seeds each; trip 0 is the nearest barracks, trip 3 the far one; jets a trip / attack runs a trip):

  | | before: jets a trip | after | before: attack runs | after |
  |---|---|---|---|---|
  | skill 1, mission 1 | 0 / 0 / 0 / **8.9** | 0 / 0 / 0 / **2.0** | 0 / 0 / 0 / 1.45 | 0 / 0 / 0 / 0.41 |
  | skill 1, mission 3 | 0 / 4.8 / 7.2 / **9.8** | 0 / 2.0 / 2.0 / **2.0** | 0 / 0.09 / 0.81 / 1.49 | 0 / 0.09 / 0 / 0.37 |
  | skill 3, mission 1 | 0 / 4.6 / 7.5 / **10.5** | 0 / 4.1 / 4.9 / **4.9** | 0 / 0.11 / 0.81 / 1.35 | 0 / 0.39 / 0.09 / 0.62 |
  | skill 3, mission 3 | 4.0 / 9.0 / 13.5 / **17.9** | 4.1 / 4.8 / 4.6 / **4.4** | 0.28 / 0.63 / 1.28 / 2.44 | 0.26 / 0.28 / 0.12 / 0.66 |
  | skill 5, mission 3 | 4.7 / 9.5 / 14.5 / **19.7** | 4.7 / 5.7 / 5.7 / **5.5** | 1.18 / 0.52 / 1.79 / 2.75 | 1.14 / 0.24 / 0.02 / 0.85 |

  Far trip against the earliest trip that has jets: skill 3, mission 3 from 4.4 x to 1.07 x; skill 3, mission 1 from 2.3 x to 1.2 x; skill 1, mission 3 from 2.0 x to 1.0 x (skill 1, mission 1 has
  no early jets: the far trip is the debut, at 2 jets instead of 8.9). Skill 1 is the calmest at every trip (2 jets a trip, jets from trip + mission 3 as before). The
  budget is the part Paul's playtest should judge: it makes the far barracks a trip of the same size as the others, and the flight home after the budget is used up is clear of jets.
  `testJetTripBudget` (the budget is used up and no more, the pad refills it, skill 1 the smallest, GODMODE = normal).

**Length:** a bot-assisted trip to the nearest barracks and home takes 1452 frames (29 s,
`tests/rescue.agk`). Pure flight for four round trips is at least 12,160 px / 3 px a frame = 4053
frames (81 s); with unloading (10 frames a person) a clean mission is about 2 minutes (derived).

**Pacing for the full game (approved, not built).** The playbook's numbers come from vertical shooters; adapted here: threat
on screen mean 2-3 in a world's first mission, p90 6; **encounters** (a garrison, an event, a roaming spawn) reached 5-8 s apart in
flight on average, spawned by map position, not timer; a rest of 4-8 s after each cleared site and each sea or valley crossing,
at least one every 80 s; a set piece at about 50% with 5-7 s clear before it; +25% pressure a mission inside a world, with
a dip at the first mission of each world; enemy fire about 30 a minute in the first zone to about 70 a minute late (lower than
a shooter, the player is often landed); tell at least 12 frames, shots 1-1.5 px/frame; set-piece and boss anti-stall at 30-45 s
and the weak point open by 90 s. Not adopted: time to first threat 3-6 s (the quiet start stays, 8-14 s in world 1, 6-10 s later).
Tooling: `wavescript` and its pacing checker for set pieces and garrisons; a trip bot (as `tools/jetsim.c`) for the rest.

**Gap vs playbook:** the game has no pacing contract at all. There is no wave table to check, no
set piece, no checker run. Until it is measured, treat the enemy numbers in 6 and 7 as the spec and
do not call the pacing "tuned".

## 6. Enemies and bosses

| enemy | HP | movement | fire / threat | tell | score |
|---|---|---|---|---|---|
| tank | 2 (cannon 1 each; a bomb within 22 px kills) | spawns off screen (cam -40 or +screen +40), drives at the chopper 0.5 px/frame, stays between x 16 and 2536 | a shell when the chopper is low (skids y > 150) and within 200 px: aimed with the chopper's half velocity, error +- (mask+1)/2 px, 1.5 px/frame (24/16, was 2-4), life 140 frames; first shell 150 frames after spawning | stops, barrel up, flash blinks every 4 frames, 12-22 frames by skill (16 normal) | 50 |
| jet | 1 (cannon only) | from 384 px off screen (100 frames of flight from being whole on screen; 576 px and 150 frames when from behind), mostly ahead of where you fly (3 of 4), at most `s_pJetsTrip` (section 7) a trip, 4 px/frame, follows your height at 0.5 px/frame | **An attack run starts** when its flash (tell) begins; a missile follows only if the jet is **fully on screen**, you are airborne, 56-200 px ahead and within 14 px of its height; a bomb if you are landed within 24 px (also fully on screen). **Never before** its warning is 2 s old (`JET_WARN_FRAMES` 100), never within 2 s after a take-off (`JET_TAKEOFF_GRACE`), never at a landed chopper on skill 1-2 and not before the dwell time on 3-5 (section 7), on skill 1-3 not while another jet is out, and a second jet's run never within 3 s (`JET_PAIR_GAP` 150) of the first's. The gap between one jet's missiles is 40 frames with the tell inside it; a ram hurts you | **warned** (a flashing white radar blip, a two-beep alert, the jet's scream) the moment it appears, then a **flash** (nose and engine glow, blinking) for the tell, 12-24 frames by skill (section 7); aborted (no missile or bomb) if at the end of the tell the jet is not fully on screen, you have landed, climbed out of its height, are closer than 56 px, or (a bomb) have not been landed long enough | 100 |
| air mine | 1 (cannon only) | spawns off screen at y 54-117, homes at 0.5 px/frame in x and 0.25 in y when within 300 px, never below y 200 | contact | none (it is visible and slow) | 75 |
| barracks | 2 (cannon 1 each; a bomb within 30 px does 2) | static; releases 1 person per 8 frames after a 20-frame delay | none; roofs stop bombs and the chopper | burning frames | 25 |
| *missile truck* (approved, not built; world 1 M2 on) | 3 (cannon); a bomb kills | drives 0.4 px/frame on flat ground, stops to fire | a lobbed missile at your position when you are landed or below 30 px; 1.25 px/frame, flight 80-100 frames, blast radius 20 px; 150 frames reload | stops, rails rise, launcher flashes; **a ground marker blinks from launch**; 36 / 32 / 28 / 24 / 20 frames by skill | 75 |
| *AA gun site* (world 1 M3 on) | 3; a bomb kills | static | flak burst at your position when you are more than 24 px above the ground, within 260 px; gap 120 / 100 / 85 / 70 / 60 frames; 1 burst live (skill 1-3), 2 (4-5); radius 18 px; **cannot fire at a chopper below 24 px** | barrel swings, muzzle flash, a **ranging puff** at the aim point; 26 / 22 / 18 / 15 / 12 frames | 100 |
| *gunboat* (world 2 M4 on; *built, v1.13*) | 3 (cannon 1 each); a bomb within 30 px kills | 0.6 px/frame along its sea stretch, braced while it tells | a shell like a tank's (1.5 px/frame, aimed with the chopper's drift and the tank's aim error by skill), the tank's gap (85 on normal, less by mission and skill), only at a chopper that is **low (skids y > 150)**, 56-220 px away, with the **whole boat on screen**, landed only after the ground dwell (R1), not for 2 s after take-off (R2), one tell at a time 150 frames apart (R3); the chopper may have gone when the tell ends: then no shell | barrel rises and the lamp flashes, blinking tell 1 / tell 2; 22 / 18 / 16 / 14 / 12 frames (16 for GODMODE) | 100 |
| *patrol boat* (world 2 M4 on; *built, v1.13; armed from M5, v1.15*) | 2 (cannon 1 each); a bomb kills | 0.8 px/frame along its stretch, braced while it tells | **mission 4: none** (unarmed scenery and a target). **Missions 5 and 6: the gunboat's attack, weaker**: a shell like a gunboat's (same speed, aim error, range, dwell and one-tell-at-a-time rules; the bow gun is the muzzle, 19 px from its centre), a tell `PATROL_TELL_EXTRA` (4) frames longer and the gap to its next shell `PATROL_COOL_EXTRA` (30) frames longer than a gunboat's on the same skill; only at a chopper that is **low**, 56-220 px away, the **whole boat on screen**, landed only after the ground dwell (R1), not for 2 s after take-off (R2), one tell at a time with the gunboats (R3) | the bow gun swings up and the muzzle lamp lights (one frame a facing, held until the shot); 26 / 22 / 20 / 18 / 16 frames by skill (20 for GODMODE) | 50 |
| *radar site* (world 2 M5 on; *built, v1.20 slice A*) | 4 (cannon 1 each); a bomb within 30 px kills | static (island), box 34 x 38 on the ground | no shot, no tell: it never attacks. While alive, its 1200 px coverage zone (cut at the map's left edge and at x 3800; red on the HUD radar's ground line and on the card's strip map) is where jets may START; destroying it ends jets there for the mission; a jet already in the air flies on. In a mission with sites, outside every live zone no jet starts (mission 4 and world 1 have none and keep today's rule). The per-trip jet budget stays the cap. Sites: mission 5 x 1608 and 3416, mission 6 those and 440 (SPEC 11, "World 2, slice 2") | the dish turns (4 frames, 8 pictures each) | 150 |
| *searchlight tower* (world 4 M10 on) | 4 | beam sweeps in 90 frames | the beam must rest on you; then a flare, then nearby AA and guns fire a volley of 2-3 shots at your lit position, each with its own tell and marker | the beam, the chopper flashing, the flare 12 frames later; beam dwell 60 / 50 / 40 / 34 / 28 frames | 100 |
| *enemy helicopter* (world 3 M9 on) | 4 (cannon 1 each; a bomb 3) | follows your height at 0.6 px/frame, 3 px/frame horizontal, keeps 80-160 px away, never within 56 | a burst of 3 shots at 1.5 px/frame, 40-frame gap between bursts; 1 helicopter at a time on skill 1-3 | nose swings to you, nose and rotor glow blinks; 24 / 20 / 16 / 14 / 12 frames | 200 |
| *destroyer* (world 2 M6 set piece; *built, v1.24 slice E*) | two turrets of 6 HP, 4 AA (no HP, air only) | station-keeping in the strait (one facing, bow left) | turret shells (shared ground-threat rules); flak above y 212; a barrage at 40 s, then every 30 s; the turrets open at 90 s | turret turns and flashes; 28 / 24 / 20 / 18 / 16 frames; AA 26 / 22 / 18 / 15 / 12; barrage 30 | 400 (150 + 150 + 100) |
| *gunship* (world 4 M12 finale) | 24 (cannon); a bomb 4; core exposed below 50% | strafes the roof line, always reachable | a 5-shot arc, a marked rocket volley, calls the AA | big nose flash, rocket marker; 30 / 26 / 22 / 18 / 16 frames | 1000 |

- **Jets in world 2** (v1.16; Paul's playtest, 2026-11-14: mission 4 at skill 3, "nobody firing at me, not enough things to dodge,
  no airplanes"; studio tuning, pending Paul's playtest; the numbers are the studio's proposal, flagged in "Open questions" 15). World 1's jets, with
  the same fairness rules and the same code (`jetsUpdate`), come to world 2, scaled by mission and skill so that the low skills stay gentle:
  - **When** (`s_pW2JetsFrom`, `s_pW2JetsTrip` in `logic.c`; a *trip* is one delivery home, `g->trip`): the trip a mission's jets begin on, and the
    most jets that may appear between one landing on the base pad and the next (the budget, as world 1's `s_pJetsTrip`; 255 = none at that skill):

    | | skill 1 | skill 2 | skill 3 (and GODMODE) | skill 4 | skill 5 |
    |---|---|---|---|---|---|
    | mission 4, Reef Run: from trip / jets a trip | none | none | 0 / **3** | 0 / 4 | 0 / 5 |
    | mission 5, Trade Winds | none | 2 / 2 | 0 / 4 | 0 / 5 | 0 / 6 |
    | mission 6, Typhoon Strait | 3 / 1 | 1 / 3 | 0 / 5 | 0 / 6 | 0 / 6 |

    So mission 4 at skill 3, Paul's playtest, has jets from its first trip, 3 a trip (decided by the coordinator on Paul's behalf, 2026-11-17: the first trip must already have
    something to dodge; it was "from the second trip" in v1.16); skill 1-2 never see one in mission 4 and skill 1 none in mission 5. Budgets never fall with the skill or the mission,
    and a jet never begins later on a higher skill (`testW2JetsOnlyFromTheAllowedSkillAndMission`; skill 4 in mission 4 moved from trip 1 to trip 0 to stay in step with skill 3).
  - **A late first jet:** in world 2 a mission's first jet comes after `W2_JET_FIRST` (500 frames, 10 s) of flying left of `W2_JET_SAFE_X` (world 1: 100 frames), then its 2 s warning, so the
    first trip has its quiet take-off and the pads and water first and then something to dodge; the take-off grace, the ground grace and the warning stay as they are
    (`testW2FirstTripJetComesLate`: the first jet is announced between frame 500 and 900 of a hover in the open, on every mission and skill that has jets from trip 0, and the budget is used up in that trip).
  - **How many at once:** the world 1 tables (`s_pJetsMax`, `s_pJetsMaxM3`): one jet out at a time on skill 1-3 and pairs on skill 4-5 in missions 4 and 5, pairs from
    skill 3 (and GODMODE) up in mission 6 (it takes world 1's mission 3 row); the second jet's attack run at least `JET_PAIR_GAP` (3 s) after the first's.
  - **Warning, tell, attack:** exactly world 1's (section 5, "Jet pacing"): a jet appears `JET_LEAD` (384 px) off screen ahead (576 px, `JET_WARN_BEHIND` 150 frames,
    when 1 in 4 comes from behind), its radar blip flashes and the two-beep alert plays at once, no attack run until `JET_WARN_FRAMES` (100) frames after,
    then the nose-and-engine flash for `s_pJetTell` (24 / 20 / 16 / 14 / 12 frames by skill; 16 for GODMODE), and a missile only if the whole jet is on screen and
    the chopper is airborne, 56-200 px ahead and within 14 px of its height; a bomb only on a landed chopper under it after the ground dwell.
  - **Fairness (R1, R2, R3):** never at a landed chopper on skill 1-2 and not before the dwell on 3-5 (6 s / 4 s / 3 s; `s_pLandDwell`, the table the boats share), none
    starts an attack run in the 2 s after a take-off (`JET_TAKEOFF_GRACE`), and **one attack at a time across jets and boats**: a jet's tell holds a boat's off and a
    boat's tell a jet's, for `BOAT_ATTACK_GAP` (3 s) (the boats' `g->atkWait`, the jet marks it `ATK_JET`; world 1 has no boats and does not use it).
  - **Where:** while the chopper is left of `W2_JET_SAFE_X` (3800: the base island's approach, 336 px before its pad), so none appears in the intro, while the chopper sits at the base or
    on its way in; jets already in the air when it passes that x stay. They fly the whole map over sea and islands at the chopper's height (0.5 px/frame
    follow, clamped to the play area), so a hovering chopper over a pad is as visible a target as over the water; a bomb falls on a landed chopper (a pad is the only place it lands).
    There are no radar sites yet: when the radar site (mission 5 on) is built, its coverage zone limits where jets may appear (row above) and this budget stays the cap.
  - **Scoring and HUD:** a jet is shot down by the cannon (1 HP; a bomb on its way down hits one too) for 100, as in world 1. Its blip is on the HUD radar's row 1, white and double-width
    while it flashes its warning (the radar there already carries the boats on row 4). **The mission card's brief** says "AND JETS" where jets can appear in that mission at the skill in force (`logicJetsPossible`, `logicBrief`; decided 2026-11-17 so the card is honest, shown with the skill in force at the mission start):
    mission 4 `PATROL BOATS, GUNBOATS AND JETS` (otherwise `PATROL BOATS AND GUNBOATS`), mission 5 `GUNBOATS AND JETS GUARD THE LANES` (`GUNBOATS GUARD THE SEA LANES`), mission 6
    `GUNBOATS AND JETS HOLD THE STRAIT` (`GUNBOATS HOLD THE STRAIT`): one line, at most 33 of the card's 40 columns, no chip (a string in the program). So on skill 1-2 in mission 4 and
    skill 1 in mission 5 the line is unchanged; elsewhere it names jets even when the mission's first trip has none (mission 5 at skill 2, mission 6 at skill 1-2 begin on a later trip)
    (`testW2BriefSaysJetsExactlyWhenTheyCanAppear`).
  - **Art:** world 1's jet (`art/jet.txt`, 32 x 12, 4 frames, PF1 pens 1-7: the pens are the same in both worlds) is a copy in world 2's set (`BOB_W2_JET`, `[w2_jet]` in
    `world2_art.toml`); no recolour was needed: it reads against every mission's sky and sea. Chip: +1,152 bytes (section 11).
- **Tell units:** tanks and jets, section 7 table. A unit test (`testNoShellWithoutTell`) and a
  scenario (`tests/tell.agk`: no `AGK sfx tankfire` without a `tank tell` before it) enforce it for
  every shell; `testNoMissileWithoutTell` does the same for every jet missile (decided by Paul, 2026-11-11).
- **Shell and missile speed** (decided by Paul, 2026-11-11): shells 1.5 px/frame on the larger axis (a slanted shot up to
  2.1 on the diagonal; was 2-4), the jet missile 2.5 px/frame (was 5). The playbook asks 1-1.5.
  Because slower shots are easier to dodge, the difficulty is paid back in fire timing, not speed:
  the tank's base shell gap is 85 frames (was 110) and the jet's gap between missiles 40 including its tell (was 50). **Not
  measured** with a person; a tuning ticket if it plays too hard or too soft.
- **No fire from off screen, no point-blank:** a tank fires only inside 200 px and low; a jet fires
  only while fully on screen (`jetFullyOnScreen`, its 32 px box inside the screen, at the start
  and at the end of its tell, with 4 px a frame allowed for in the missile's room; unit-tested, `testJetNeverFiresOffScreen`), 18-40 on-screen frames
  in, with 56 px of room. (Before, "ahead < 200 px" could place a jet up to ~40 px beyond the screen
  edge once it had been seen, `logic.c:1107` at 17ece43.)
- **Sight no longer than weapon range:** tanks drive at the chopper from any distance but shoot
  only inside 200 px; mines see 300 px and have no weapon.
**Rules for every weapon that can hit a landed or low chopper (approved, not built; they join section 5's jet rules and remove none):**
- **R1 shared ground grace:** the jet table is shared by jets, trucks, gunboats and garrison guns: landed this long before it may be targeted: never (skill 1), never (2), 6 s (3), 4 s (4), 3 s (5); GODMODE uses skill 3. (Tanks keep their shipped behaviour.)
- **R2 take-off grace:** 2 s after lift-off no new landed-or-low attack starts.
- **R3 one attack at a time:** a global token across jets, trucks and gunboats: one attack on a landed or hovering chopper at a time on skill 1-3, two on skill 4-5, at least 3 s apart.
- **R4 the tell:** 12 frames or more on every skill, longer for lethal weapons (the tables in this section); a unit test per weapon: no shot, lob, flak or rocket without its tell.
- **R5 the impact is marked:** a truck's missile has a blinking ground marker from launch; flak shows a ranging puff before it bursts.
- **R6 slow weapons:** ground-launched lobs 1.25 px/frame (flight 80-100 frames); shells 1.5; the jet missile 2.5 after its long tell.
- **R7** not from off screen, not point-blank (56 px). **R8** sight no longer than weapon range. **R9** terrain is not a weapon.
- **Garrisons:** each barracks and winch site has 1-3 placed units within about 300 px, visible from afar, fixed. Clearing them all is a **zone clear** (+100 points and a medal); roaming spawns then stay 400 px away while you are landed there. If you land without clearing, R1 applies: on skill 1-2 the garrison cannot attack you while landed, but still attacks you in the air; on skill 3-5 it starts after the dwell time.
- **A landed chopper:** the AA gun cannot hit it (it cannot depress below 24 px: the counterplay); the truck lobs after the dwell, you lift off in time because the marker shows from frame 1.

- **Path scripts:** none. Movement is per-unit code in `logic.c` (no `wavescript`).
- **Bosses and set pieces** (approved, not built): world 1 M3 the border convoy; world 2 M6 the strait destroyer; world 3 M9
  the summit duel; world 4 M10 the lit gauntlet and M12 the gunship. Each has anti-stall escalation at 30-45 s and its weak point
  open by 90 s, a 5-7 s quiet before it, and drops a reward (a repair crate, a medal). *Shipped:* none; the ramp is the trips.
  Terrain, pads, winch and wind are in section 4.

## 7. Difficulty

Ctrl+L then a key, within two seconds; it stays until changed. Shown at the right of the score
bar. Tables in `logic.c` (`s_p*[DIFFICULTY_MAX + 1]`). Index 0 is GODMODE and **equals index 3 in
every table** (decided by Paul, 2026-11-11; before, its jets used the easiest lead 255 px and delay 40). A unit test
(`testGodmodeTablesEqualNormal`) walks every per-difficulty table (the registry `logicDiffTables`)
and fails on any difference.

| | 1 easiest | 2 easy | **3 normal** | 4 hard | 5 hardest |
|---|---|---|---|---|---|
| tanks at once (`1 + trip + mission +`) | -2 (was -1) | -1 | 0 | +1 | +2 |
| shell gap (frames, `85 - 20*mission - 8*trip +`, plus 0-31) | +90, floor 75 (was +60, 60) | +30, floor 50 | 0, floor 40 | -15, floor 32 | -30, floor 26 |
| aim error (px, +-) | 32 | 32 | 16 | 8 | 4 |
| tank tell (frames) | 22 | 18 | 16 | 14 | 12 |
| jets from (trip + mission >=) | 3 | 2 | 1 | 1 | 0 |
| mines from | 4 | 3 | 2 | 1 | 1 |
| spawn gap (`220 - 40*mission +`; every skill floored at 60) | +120 | +60 | 0 | -40 | -70 |
| jet rest between passes (`20 - 30*mission +` with one jet at a time, `160 - 30*mission +` with pairs; floored at 40; plus 0-63 frames) | +100 (was +80) | +60 (was +40) | 0 | -30 | -50 |
| jet starts this far off screen (px) | 384 on every skill (it was 255 / 220 / 180 / 150 / 130) |||||
| jet on screen before its first shot | replaced by the 2 s warning (100 frames from appearing) on every skill (was 40 / 30 / 24 / 20 / 18 frames) |||||
| jet tell, the flash before a missile or bomb (frames; was 22 / 18 / 16 / 14 / 12; its own table now, the tank's is unchanged) | 24 | 20 | 16 | 14 | 12 |
| landed this long before a jet may bomb the chopper | never | never | 6 s (300 frames) | 4 s (200) | 3 s (150) |
| jets out at once (missions 1-2 / mission 3) | 1 / 1 | 1 / 1 | 1 / 2 | 2 / 2 | 2 / 2 |
| jets that may appear in one trip (base pad to base pad; Paul's playtest, 2026-11-12, section 5) | 2 | 3 | 5 | 6 | 6 |
| a jet from behind: lead (px) and warning (frames) | 576, 150 on every skill (from ahead 384, 100) |||||
| **world 2** jets: from trip / jets a trip, mission 4 (v1.16, section 6, "Jets in world 2"; v1.17: skill 3-4 from trip 0) | none | none | 0 / 3 | 0 / 4 | 0 / 5 |
| mission 5 | none | 2 / 2 | 0 / 4 | 0 / 5 | 0 / 6 |
| mission 6 | 3 / 1 | 1 / 3 | 0 / 5 | 0 / 6 | 0 / 6 |

- **Jet fairness** (Paul's playtest, 2026-11-12; studio tuning, pending Paul's playtest): the jet rows above, and section 5's rules. Also by the way, a jet's cool-down was
  8 bits and wrapped for skill 1's rest of 240-303 frames (it was often 0-47 instead); it is 16 bits now. `testNoBombOnLandedChopper`,
  `testTakeoffGrace`, `testOneJetAtATime`, `testPairOffset`, `testWarningBeforeAttack`, `testJetTellTable` and `testSkillOneEasier`
  hold the rules.
- **Never changes:** the battlefield, barracks positions and hit points, people, cabin 16, 3 lives (refilled per world from v1.5),
  the chopper's handling and hitbox, every tell existing, the second hit's death (the first only damages).
- **The ramp:** within a mission by trip (tanks +1, shell gap -8, jets after the first trip, mines
  after the second); across missions by mission (tanks +1, spawn gap -40, shell gap -20, jet rest -30,
  quiet start 700 / 550 / 400 frames).
- **Hero device: damage** (decided by Paul, 2026-11-11, section 3): the first hit costs smoke, 20 % speed and the rest
  of the trip until the pad, not the cabin; 2 s of blinking invulnerability follow. No shield,
  no smart bomb, no extra lives by score, no power-ups.
- **Respawn:** in place at the pad, landed, after a crash (70 frames) and a 150-frame LOST screen. Lives: *shipped* 3 for the whole game; *approved, not built (decided by Paul, v1.5):* **lives refill to 3 at the start of each world, and there is a continue** (a save at each world card: world, mission, lives, merit, upgrades). In a world, lives are not refilled between missions.
  Enemy shots, jets and mines are cleared; jets cool 150 frames; tanks stay. The camera pans back.
  (Shipped: lives are for the whole game; see the bullet head for v1.5.) People still free stay
  free; open barracks stay open. *Shipped:* no checkpoints and no continue: game over goes to the hall of
  fame, then the title (v1.5 adds the world-card save, section 4.3).
- **Exploit rules (read from the code, none unit-tested):**
  - The base is safe: no tank spawns while the chopper is at x 2760 or more, tanks cannot pass
    x 2536, and a tank shoots only within 200 px, so the pad (x 2800+) cannot be shot.
  - Flying high is safe from tanks (they need skids y > 150), but not from jets and mines.
  - **Closed** (decided by Paul, 2026-11-11): tanks respawn on a timer while you are left of x 2760, and used to pay 50
    points each without end. Now only the first `TANK_QUOTA` (10) tank kills of a mission score
    (`tankKills`, reset at each mission start, not at a respawn; cannon and bomb alike); later tanks
    still die and still threaten, for 0. At the quota the HUD's left slot reads QUOTA 10 for 200
    frames. 10 is about what a fair mission kills (about 2-3 a trip over four trips; derived, not
    measured). **Open:** mines still pay 75 each, endlessly (3 at once, a 1 in 2 roll per spawn): a
    smaller loop, not closed here.
- **Testing aids:** ghost (Ctrl+L, G: nothing destroys the chopper at any difficulty, for the pacing runs), Ctrl+L N (the next trip's
  schedule), GODMODE (key 0: plays like normal, nothing damages or destroys the chopper, no
  hi-score), Ctrl+L 1-5, the serial heartbeat
  (`state`, `x`, `landed`, `aboard`, `saved`, `trip`, `lives`), and the scenario checkpoints
  `boarding` and `rescued`.

**Difficulty for the full game (approved, not built).** Skills 1-5 keep their meaning and their tables; new weapons use the same
indices.

| | 1 | 2 | **3** | 4 | 5 |
|---|---|---|---|---|---|
| missile truck tell (frames) | 36 | 32 | 28 | 24 | 20 |
| AA flak ranging puff (frames) | 26 | 22 | 18 | 15 | 12 |
| gunboat tell | 22 | 18 | 16 | 14 | 12 |
| enemy helicopter tell | 24 | 20 | 16 | 14 | 12 |
| destroyer turret tell | 28 | 24 | 20 | 18 | 16 |
| gunship tell | 30 | 26 | 22 | 18 | 16 |
| searchlight beam dwell | 60 | 50 | 40 | 34 | 28 |
| AA gap between bursts (frames) | 120 | 100 | 85 | 70 | 60 |
| pad width (px) | 96 | 80 | 64 | 56 | 48 |
| timed rescue (s) | 120 | 105 | 90 | 75 | 60 |
| wind gusts | none | not near barracks | not near barracks | yes | yes |
| landed-chopper attacks at once | 1 | 1 | 1 | 2 | 2 |

All values are studio defaults, revisit at a playtest gate. Within a world the pressure rises about +25% a mission; the first
mission of a world dips (section 4's index). Zone-clear bonus 100; the tank quota of 10 stays, and trucks, AA guns and boats pay
for their first 6 kills a mission only (studio default).

## 8. Art direction / style guide

The approved direction is `~/art-pilot/first/STYLE.md` (sections 1-10, pilot approved on mock-ups
2026-11-11) and its `NOTES.md`. **Landed** on `main` in 567e6c5/065a44d (2026-11-11): the pilot
sprites, seamless hazed parallax bands (the sky gradient now runs to line 195), pointed summits
(`art/tools/make_mountain_caps.py`: the first opaque row of the mountain band is 5 px, was 57),
and the title's hazed mountain strip (`art/tools/make_title_mtn.py`, 320x34, 8 KB chip). Paul
approved the result on 2026-11-11. `art/tools/draw.py` now only writes the palettes. Still open:
whether a copy of STYLE.md joins the repo as `docs/STYLE.md` (open question 6).

**Look:** a painted dusk diorama: hazy, layered landscape bands behind a crisp, hard-outlined
foreground cast. One sun, top left: slopes, roofs, fuselages and tanks lit from the upper left,
shadows to the bottom right. Chunky 2+ pixel clusters, no single-pixel noise inside shapes, ordered
(Bayer) dither only for gradients and smoke, a black outline on everything that moves; bands have
no outline.

**Palette (OCS 12-bit, dual playfield: PF1 colours 0-7, PF2 9-15, sprites 17-31)**

| slots | use |
|---|---|
| 0 | the sky: COLOR00, a copper gradient per line (172 lines, 24-195), line-dithered on OCS; the HUD panel per line |
| 1 | 0x111 outline, darkest shadow, HUD drop shadow |
| 2-7 | 0x564 olive drab (enemy paint, roofs), 0x998 stone, 0xFEC cream white (lit edges, skin), 0xD42 red (fire, open-barracks marker), 0xFC3 yellow, 0x36B blue (hostages, closed-barracks marker). Locked for every BOB and the HUD |
| 17-27 | the chopper, attached sprites on channels 0-3: 111 445 778 AAB EEE D22 811 38D BEF F70 FE4 |
| 25-27 (shared pair) | the weather on sprite channels 4-5 (pale blue, orange, yellow) |
| 29-31 | sky wisps or stars on sprite channel 6, per mission (A45 E86 FCA / 335 99B FFF / DA9 FDB FFE) |
| 9-15 | the parallax bands, reloaded by the copper per band and per mission. Each band is drawn in indices 1-7 dark to light so one bitmap serves all three palettes. The plain uses no new slots: its far strip is in the mountains' palette, its floor in the land palette |
| 0 on lines 180-198 | the plain's ground under the floor: a line-dithered gradient per mission from H (the sky's own colour at line 180, the haze) to C (land pen 6); C from line 199 down, so the hills stand on the plain's colour, not the sky's |

Band palettes (7 colours, dark to light): clouds dusk 312 423 534 745 A55 D86 FB8, night 011 112 123
234 346 68A 9BD, dawn 245 356 568 89A CAA FCB FEE; mountains dusk 413 524 635 746 857 A68 C9A, night
012 123 234 345 457 79A BDE, dawn 546 657 768 879 98A BAB EDD; hills and ground dusk 310 521 742 963
B84 DA6 EC8, night 010 121 232 343 354 576 798, dawn 131 242 363 585 9A6 CC8 EEB (`logic.c:45-62`).
Sky: dusk 214 to E75, night 001 to 235, dawn 237 to FB8.
**AGA:** the sky is 24-bit; bands and BOBs stay 12-bit; the title logo is 224 colours.

**Seamless bands (the user's rule: a band boundary is never visible).** The copper switches PF2's
bitplane pointers at lines 72, 112, 120, 196/197 and 224. Both sides of every switch show only the
sky (transparent PF2 over the unbroken COLOR00 gradient) or **C**, the land palette's pen 6 (0xDA6
dusk, 0x576 night, 0xCC8 dawn), used by the hills and the ground. Mountain bases haze into the sky
over 12 rows, cloud undersides over 10, hill bases dither into C over 7 rows; each silhouette stays
inside its band with at least 2 transparent rows of margin; crest heights vary (no flat run over
about 24 px). `parallax_check.py` (the pilot) asserts every pixel on each boundary line pair is the
line's sky colour or C at 12 camera offsets for each mission: last run PASS. On AGA the quarter-pixel
shift only moves pixels already sky or C, so it cannot open a seam. **There are no horizontal biome
transitions:** the border is the fence and the ground band is the same everywhere.

**The plain: a perspective floor between the mountains and the hills** (decided by Paul, 2026-11-12).
The band of flat sky colour between the mountains' foot and the hills (lines 192-198 solid, and the sky
between the hill crests down to 218) read as too solid and showed the banding. In its place, a plain:
scrub and flats, distant mesas, a dry lake or riverbed, a road or rail line.
- **Far strip** (lines 170-179, 10 rows): the mountains' foot fading into haze, with small mesas. It is
  the bottom of the mountains band (the same bitmap rows, speed / 4, palette, AGA quarter pixels), so
  anything taller than a line may stand here without shearing.
- **Floor** (lines 180-198, 19 lines): every line scrolls at its own speed, rising linearly down the
  screen (speed is proportional to 1/z, and 1/z is linear in screen y on a flat plain): line 180 at
  the mountains' camera / 4, line 198 at the hills' camera / 2, `(18 + j) / 72` of the camera for
  floor line j. Row j loops every `8 (18 + j)` px (144-288), so every row repeats after the same 576 px
  of camera travel and a flat feature drawn in perspective stays one feature. Only flat, one-line-high
  or perspective-drawn features go here; anything taller shears.
- **Seams:** floor row 0 and row 18 are transparent (the haze at the far edge, C at the near one); the
  far strip's last two rows dither into haze. The full art contract (sizes, pens, rules, files) is
  `docs/plain-spec.md`.
- **Hills:** they start at their row 2 (rows 0-1 were empty) at line 199; their transparent pixels now
  show C (the plain's near colour) instead of the sky. Optional for the art pass: near-plain texture
  baked into the hills' empty pixels, which scroll at / 2, the floor's near speed.

**Animation** (`logic.h`, art pilot section 6): chopper 8 poses (+ slight and full lean, front view) x 2 rotor phases, mirrored for
left; person 8 frames (stand, 2 run right, 2 run left, 2 wave, down); tank 10 (2 each way, 2 fire,
2 aim, 2 flash); jet 2; barracks 4; explosion 5; mine 2; shadow 4 sizes x 2 dither phases; flag 2.
Rule: one drawn frame per 4-6 px of travel, held at least 2 game frames.

**Sizes** (`logic.c:14-31`): hangar 64x40, fence 16x32, barracks 48x28, tank 32x14, person 16x12,
jet 32x12, mine 16x12, shot 16x3, bomb 16x6, shell 16x4, explosion 32x32, flag 16x24, shadow 32x4.

**Sources (honest list)**
- 1.0 art: scripts only, `art/tools/draw.py` (our own drawing code), logos from the user's source
  pictures (`art/src/*.png`). No other games' art.
- Approved pilot (**landed** 2026-11-11): Retro Diffusion (`rd_plus__environment` + `tile_x` for the bands,
  `rd_pro__default` with our own chopper as the reference, `rd_animation__vfx` for the explosion,
  `rd_plus__low_res` as a rough for the tank), then locked to the exact palette slots, orphans
  removed, mirrored where it lit from the right, hand-finished and outlined. 19 calls, $1.72 of a
  $2.27 cap, OpenAI/Gemini none (`NOTES.md` generation log). The barracks, jet, hostage and turn
  views failed in Retro Diffusion and are hand-authored.
- **Sign-off:** the user approved the pilot on 2026-11-11.

## 9. Audio

**Channel policy** (`sound/sound.toml`, `main.c:24`, `main.c:1664-1769`). Paula plays 0 and 3 left,
1 and 2 right.

| channel | in play | title, card, hall of fame, jingles |
|---|---|---|
| 2 | the rotor loop, always (a 4800-byte looped blade thump, v4: stored at twice the rate, so Paula's period is half the logic's, 296 = 12 kHz at hover) | silent |
| 0, 1, 3 | effects: `sfx_channels = [1, 3]`; **no music in play** | music |

- Music plays on 0, 1, 3 only (the player's mask excludes 2). Effects borrow 1 and 3.
- **Rotor channel claim (v1.42, Paul's playtest of the integration build, 2026-11-24: "after dropping off the first set of passengers the
  rescuees-entering-the-HQ sound persists, a high-pitched sound, and replaces the low Huey chop-chop; it never returns for the rest of the game").**
  *Cause, found in ACE's `ptplayerSfxPlay` (automatic channel choice, which `soundPlay` uses for every effect):* `sfx_channels = [1, 3]` and
  `ptplayerSetMusicChannelMask(0x5)` only steer it while a song's silence counts are fresh. In play there is no song (`musicFor` stops it at lift-off), the counts
  are stale from the title theme (channels 1 and 3 positive, 0 and 2 zero), and when **both effect channels are busy and the new effect is not of higher priority than
  either** the choice falls to the last idle channel with the best count: channel **2**. Two `saved` effects (11 frames each, 6 apart) plus a `land`,
  `board`, `boom` or cannon shot in the same stretch is enough, which is why it came "sometimes", at a drop-off. That effect starts on the rotor's channel (new
  pointer, length and period) and when it ends ptplayer leaves the channel on its idle loop (the effect's first words); the game only ever wrote the
  period and volume of channel 2 after boot, so the rotor's wave was never put back, and `rotorUpdate`'s volume made whatever was there audible: the 530/795 Hz
  "saved" arpeggio looping for the rest of the game. (The v4 note below, "nothing steals the rotor's channel", was measured with trips in which
  two effects never overlapped a third; it was true of those runs only.)
  *Fix:* the rotor is now also **claimed in ptplayer**: `rotorCreate` plays the loop as a looped effect of the highest priority on channel 2
  (`ptplayerSfxPlayLooped`, volume 0; the wave has its idle word first, 4,802 bytes of chip, +2), so the automatic choice never picks it (a busy channel is
  skipped, and no priority beats a looped one), and `musicFor` claims it again after `soundMusicStop` (`ptplayerStop` frees every channel's claim). `rotorUpdate`
  still writes only period and volume. Sound, pitch and every rotor number are unchanged. Channel 0 stays an idle effect channel in play, as before.
  *Proof:* `tests/hqsound.agk` (debug key Ctrl+L, J: three `saved` in one picture, an idle rotor) with `sound/tools/check_rotor_back.py` on its recording
  (the harness's `expect-sound` is not enough: the stuck effect is audible too). Red on the build before: 36-39 % of the power above 1 kHz and 0-2 % in
  the rotor's 30-150 Hz body after the burst, 6 of 6 windows; fixed: 1-7 % and 88-99 %, 0 of 6.
- **Rotor, v2 (Paul's playtest, 2026-11-12; studio tuning, pending Paul's playtest):** "especially when cruising, still too annoying
  and irritating; a low, satisfying chopper thump, like an arcade Huey." Cruise and top speed were the problem (8.4 thumps a second at
  top speed, volume up to 56), so: the thump rate is capped at **6.5 a second flat out** (period 455;
  was 351: 8.4) and rises 45/16 (2.8) of a period unit per 1/16 px of speed (shifts: a divide would pull in the 68000 maths library) (5.6 a second at 1.5 px/frame, 5.9 at 2, 6.5 at 3; was 5 per
  unit), climbing still 4 per unit but never below 455; the pitch follows the rate on Paula, so it now rises 30 % hover to top speed
  (was 68 %). The **volume stays near the hover level**: 40 plus speed * 11 / 128 (44 flat out, was speed/3: 56), climbing +3. The loop has more low body and
  a softer crack: the 55 Hz resonator is kicked harder (1500, was 1100) and rings 85 ms (was 70), the 230 Hz one is softer (2000, was
  2800), the slap is a quarter less (strength 96/84/92/78, was 128/112/122/104); energy 82 % at 40-100 Hz (was 65 %), 12 % at 100-250 Hz (was
  26 %). The take-off spin-up (3 less per frame for 100 frames, up to 300, and +10 volume) is unchanged. Unit tests
  `testRotorRateFollowsSpeedAndClimb` (6-6.5 flat out, 5.5-6 at cruise, no more than 6.5 climbing flat out, volume at most hover + 5). Previews:
  `sound-preview/rotor-v2/` (hover, cruise, full, take-off; `v1-previous-*` is the 2026-11-12 morning version).
- **Rotor v4 (Paul's playtest, 2026-11-14, WinUAE: "the rotor sound in flight is still really bad: too tinny or harsh in flight; it sounds
  like an alarm bell - it should be a low thump that is huey-like and satisfying, chop-chop-chop, and slight pitch / tempo adjustments for
  different states. I liked the v3 sounds, it seemed like I was not hearing them"; branch `rotor-sound`, pending Paul's ears).**
  *What he heard, measured.* The game was played in WinUAE with Paul's own `first.uae` settings (no sound keys: defaults) from his
  `first.adf` of 2026-11-14 15:47, with real key events and the default playback device recorded (WASAPI loopback; the windows were named
  `RRDBG-rotor-*`), through a scripted flight (idle, lift-off, hover, cruise, climb, descend, cannon) and a scripted whole rescue; and in
  vAmiga (`agk run -f sound/capture/rotorcap.agk`, `rotorcapw2.agk` for world 2, `tests/rescue.agk`; WinUAE: `sound/tools/winuae_capture.py`). Results:
  - **v3 is what plays.** Period and volume in the serial lines are the unit-tested numbers (hover 591/40 = 5.0 thumps a second, cruise 6.2,
    climb 6.1, idle 3.4, loaded and damaged lower), the thump rate measured in the recordings agrees (5.0 / 6.2 / 6.2, idle 3.5), and the waveform
    of one blade pass in WinUAE is the synthesised loop through a sample-and-hold. The A500 profile, world 2 (a floppy load first) and
    the title music on or off make no difference: **music is off during flight** (`musicFor` stops it at lift-off), so there is nothing to
    mix with.
  - **Nothing steals the rotor's channel.** `ptplayerSetMusicChannelMask(0x5)` keeps effects on 1 and 3 and `ptplayerSetChannelsForPlayer(0xB)`
    keeps the player off 2; across a whole rescue in WinUAE (16 boards, 16 saves, cannon, barracks) the 5-a-second comb never drops out, restarts
    or changes channel; `ptplayerStop` leaves 2 alone. (Checked because the loop is started once, at boot, outside ACE's DMA bookkeeping.)
  - **What is wrong with v3 in the player's ears** (A-weighted, IEC 61672, as shares of the total, rotor channel, real WinUAE play):
    the loop's slap and mid band count for 27-36 % at 250 Hz-1 kHz and about 8 % at 1-3 kHz in cruise, climb and hover: "the crack", not
    the thump, is most of what the ear gets (the 40-120 Hz body is 84 % of the *power* but weighs almost nothing in A, and a laptop speaker
    plays none of it). On top, Paula's sample-and-hold has **no filter in WinUAE** and v3 stores the loop at 6.0 kHz at hover (period 591;
    4.2 kHz idling, 7.8 flat out), so its sampling-rate lines (5.9-6.1 kHz hovering) are bright lines in the spectrogram that **glide** with
    every speed change (10 kHz to 4 kHz through the spin-up, 5.7-8 kHz in cruise and climb), stand steady at idle (4.2 and 8.4 kHz) and wobble
    with the damaged pitch: 19 dB over their surroundings in the vAmiga hover. A gliding whistle in the keenest part of hearing over a
    5-a-second thump is what an alarm sounds like. (vAmiga and the v3 preview WAVs have the same lines; the preview WAVs are mono and have
    no effects, which may be why they sounded right: not established.)
  - **Effects, not the rotor, are the other alarm-like thing in the recordings** and were left alone (a design call, below): `board` (square
    880/1320 Hz, one a person, up to 16 in a row), `saved` (a pulse25 arpeggio 1047-1568 Hz, 16 in a row at the hangar) and `jetwarn` (square,
    1.8-2.3 kHz) weigh 45-70 % in A at 1-3 kHz and run 3-7 dB louder than the rotor (rms -17..-20 dBFS against -23): bell-like. Paula channel 2
    is the right channel only (the rotor is 14 dB lower in the left: WinUAE's 70 % stereo separation).
  *The fix, in the loop and the rules, not in the approved numbers* (empty hover 591/40 and cruise 501/42 are pinned bit for bit by
  `testRotorApprovedEmptyStaysExactly`, which still passes unchanged):
  - **The loop is made at twice the rate** (`ROTOR_OS` 2, two passes of 2400 samples, the same 4800 bytes of chip RAM): Paula's period is the logic's
    divided by two (`logicRotorHwPeriod`, (p+1)/2: hover 296, idle 423, flat out 228), so every thump rate in this section is unchanged (to 0.2 %) and
    the sampling lines sit at 12 kHz hovering (8.4 idling, 15.6 flat out), 6 dB weaker and out of the ears' keenest range. The serial state line
    prints `rotorHw=`. The loop is made by `logicRotorSynth` (logic.c, unit-tested; `make_rotor.py --check` compares it with its Python mirror byte for byte).
  - **A softer, lower slap** (two low-pass poles, about 120-450 Hz, strength 70/60 of 128 against 96/84/92/78 at 250 Hz-1.2 kHz) and the 230 Hz resonator
    kicked less than a third as hard: the same three resonators (55, 130, 230 Hz), the body unchanged (40-120 Hz = 84 % of the power, 120-250 Hz = 12 %),
    power above 300 Hz 0.15 % (was 0.51 %); A-weighted 250 Hz-1 kHz 12 % and above 1 kHz 2 % at hover in the loop (was 27 % and 11 %). Measured in the real
    WinUAE play (250 Hz-1 kHz and 1-3 kHz, v3 then v4): hover 26 % and 7.9 % became 12 % and 1.0 % (world 1, clean 2.5 s windows; world 2: 26/7.9 became 11/1.1),
    the flight out to the barracks 35 % and 9.7 % became 19 % and 1.6 %, the flight home with 16 aboard 29 % and 7.4 % became 7.5 % and 1.1 %; in the vAmiga flight
    every state fell from 25-35 % and 7-8 % to 9-20 % and 1 %, and the 5-9 kHz share from 0.8-1.6 % to 0.1 %. What is left of the sampling lines: at idle (period 845)
    the image sits at 8.4 kHz, 3 % in A (v3: 4.2 and 8.4 kHz, 13 %), at a volume of 16-26.
  - **States** (small; never faster than the approved ones except the lively spin-up): *descending* settles the pitch a little lower (period + vy/2, up to +16 =
    2.7 %: 4.9 a second flat down), the *take-off spin-up* of the empty chopper peaks at **7.8 a second** (`ROTOR_PER_MIN_BOOST` 380; was 300 = 9.8 with a
    climb: a buzz; the v3 text's "9.8-11.8" was the unslewed target, play showed 7.8-9.8), a loaded or damaged lift-off stays calm (6.5 at most), loaded and damaged are lower
    and heavier (v3), climbing quickens to 6.1 (v2), hover 5.0, idle 3.4.
  - Unit tests (red on v3 first): `testRotorHardwarePeriodIsOversampled`, `testRotorLoopIsDeepAndSoft` (a DFT of the loop: body, power over 300 Hz, A-weighted mid
    and high shares at hover, idle and flat out), `testRotorDescendSettlesLower`, `testRotorEmptyTakeoffIsNoBuzz`; `tests/rotor.agk` expects `rotorHw=` (red on v3);
    `sound/tools/check_rotor_audio.py` measures a recording (v3's vAmiga hover: 26.9 % mid, 10.9 % high, a 19 dB line at 6.06 kHz = FAIL; v4: 10.2 %, 1.6 %, 11 dB = PASS).
  - Not decided by ear (nobody has listened to v4 or to its previews, `C:\Users\admin\Downloads\rotor-v4`); if it is still not Huey enough the levers are the slap
    strength (70/60), the 230 Hz kick (600), a rotor on two channels (0 and 2) so it is not in one ear, and `board`/`saved`/`jetwarn` softer (Paul's call).
- **Rotor v3 (Paul's playtest, 2026-11-12, later: "when damaged, the sound is very annoying when flying, and the sound when
  cruising with passengers is not good and irritating as well"; studio tuning, pending Paul's playtest).** What the code and
  the emulator showed (a500, scripted trip, serial `rotorPer`/`rotorVol` and every `AGK sfx`): the rotor did **not** know about
  damage or passengers at all. Empty and 16 aboard cruise at the same period 456 and volume 44 flat out, and damage only
  caps the speed at 2.4 px/frame (period ~485), so the cruise itself was the approved v2. What was left that is
  irritating: (1) **the take-off spin-up** (`rotorBoost`, unchanged in v2): 100 frames of 3 less period a frame down to 300
  = **11.8 thumps a second** at volume +10 (measured 9.8-11.3 a second, volume 48-50, for the first 2 s of every flight),
  and every loaded trip, every repair on the pad and every barracks leaves with it, so the cruise "with passengers" opens
  with a buzz; (2) the damaged state had no sound of its own beyond the single hit boom, and the sfx channels let one
  effect repeat as fast as its events (boarding 16 people is 16 `board` blips 8-10 frames apart, jets and explosions in
  clusters). Now (`logicRotorTick`, logic.c; previews `sound-preview/rotor-v3/`):
  - **The approved empty hover and cruise are bit for bit unchanged** (periods and volumes identical for hover, cruise, full and
    take-off with nothing aboard and no damage; the empty chopper's lively take-off is untouched).
  - **Loaded: lower and heavier, never higher.** Period + 2 per person aboard (16: +32, 6 %), volume + aboard/4 (16: +4); the spin-up for
    a loaded or hurt chopper is 1 per frame, not 3 (and +boost/20, not /10) and never below period 455, so it lifts off at 6.5 thumps a second
    at the most (was 11.8), no buzz.
  - **Damaged: a laboured, lower rotor, not an alarm.** Period + 32 plus a slow wobble of 0-15 (two triangles of 64 and 21 frames, so it is irregular
    and slow, at most 3 units a frame), volume as for the load. Cruise 5.0-5.6 thumps a second instead of 5.6-6.5; it is easy to tell
    from the healthy one, with no repeating cue. The "hurt" cue at the moment of the hit stays the single boom, plus the smoke and
    the DAMAGED HUD slot.
  - **No effect repeats like a machine gun:** `logicSfxAllow` (main.c `playSounds`) holds back an effect that played less than `logicSfxGap`
    frames ago: boom 8, jetwarn 50, jet 50, tankfire 12, saved 6, board 6, land 12 (crash, open, cannon, bomb, start are not gated: priority and
    the fire rate decide those).
  - Unit tests `testRotorApprovedEmptyStaysExactly` (591/40 hover, 501/42 cruise), `testRotorLoadedAndDamagedNeverFasterThanEmpty` (at 0,
    16, 32, 38, 48 sixteenths of a pixel: loaded 8/16, damaged, both: period above the empty one, at most 90 above, volume 0-5 above),
    `testRotorHeavyLiftoffIsCalm` (at most 6.5 a second, volume steps at most 2 a frame, at most 62), `testRotorDamagedWobbleIsGentle`
    (6-24 units of wobble, steps at most 3, never below the empty cruise + 32), `testSfxGapsWhileDamaged`.
  - Not decided by ear yet: Paul listens in WinUAE. If the empty take-off's 9.8-11.8 a second is also too much, the same
    fix applies to it (`ROTOR_PER_MIN_BOOST` 455 and the 3 per frame); left alone because Paul approved hover and cruise only.
- **Rotor (decided by Paul, 2026-11-12: a deep blade thump; numbers below are v1, see v2 above):** a low, heavy whop-whop-whop like a Huey, made
  from scratch (`sound/tools/make_rotor.py`, mirrored bit for bit by `logicRotorSynth` in `logic.c` (v4; `rotorSynth` in `main.c` before); no samples). Why the
  loop of 6ab2ff8 was still tinny: its blade pass was 752 samples at 8287 Hz, so **11 thumps a second**, a
  buzz, not a whop; its 75 Hz thump was 40 ms long and its noise slap was louder (raw peak 229 against 190) and
  sits at 300 Hz-1 kHz where the ear and small speakers are most sensitive, so the slap, not the thump, was what
  you heard; and it played at volume 38 of 64. Now: the loop is 4 blade passes of `ROTOR_PASS` = 1200 samples
  (4800 bytes of chip RAM, was 3008), each a thump of three resonators (55 Hz 70 ms, 130 Hz 35 ms, 230 Hz 20 ms:
  the body that small speakers reproduce) plus a slap a quarter of its level, peak 124 of 127. Energy: 65 % at
  40-100 Hz, 26 % at 100-250 Hz, under 3 % above 250 Hz. The Paula period sets the thump rate
  (`logicRotorPeriod`, logic.c, unit tested): 5 a second at hover (period 591), 3.5 idling on the skids (845),
  8.4 at top speed (5 less per 1/16 px: 351), climbing quickens it (4 per climb unit, floor 330 = 8.95 a
  second), descending does not. Volume (`logicRotorVolume`): 40 at hover plus speed/3 (56 at top speed) and 4
  climbing; landed 26 (16 on the preview card); 0 when paused or dead. **Take-off:** for 100 frames after lift-off
  the period is 3 less per frame left (up to 300) and the volume up to 10 more, so it spins up from the idle to
  about 7.8 thumps a second and +8 louder in half a second, then settles to the hover over 2 s; the period
  slews (16 a frame down, 3 up) and the volume (2 up, 1 down), so neither jumps. The loop is synthesised at
  startup after the splash (it pushed AROS past its boot window when done earlier); same work as before
  (12000 steps).
- **Priorities** (higher wins): crash 5; open 4; boom 3, start 3; tankfire 2, board 2, saved 2, jet
  2; cannon 1, bomb 1, land 1. Explosions and the crash beat weapons; important effects are short
  (0.08-0.8 s). `soundPlay` order in `playSounds` takes crash/open/boom as one choice per frame.
- **Gap vs playbook:** no panning by screen X (effects go to whichever of 1 and 3 is free); no
  cap on effects per frame beyond priority. Rotor Raider has both.
- **Music from the floppy (v1.8):** the three songs are files, not program data (`disk = true`). `songsLoad` (main.c) reads one a frame at boot after the title
  logo, last the theme, which starts then; all three stay in memory (samples in chip, patterns in fast RAM, as the program had them: chip free is
  unchanged, 119 / 153 / 159 KB on a500 / ks31 / aros), and nothing loads later: a load stops Paula's music and freezes the picture for seconds, so
  there is no moment in play, on a card or on the title where one would be quiet. Our own loader, not the kit's `soundMusicLoad` (that put the whole
  file, patterns too, in chip: 10 KB less chip free). The cost is time at boot: the title and its music come 50-200 harness frames (1-4 s of
  emulated time) later, the first picture 50-150 sooner. `tests/songs.agk`, `tests/memory.agk`.
- **Music:** one tune per state, all our own, synthesised sampled instruments (`sound/samples/`
  bass, hat, kick, lead, snare from `sound/tools/make_samples.py`): `theme` for the title, card,
  hall of fame and initials; `fanfare` for mission cleared and the ending; `gameover`. **Gap:** no
  music in play (the rotor owns the sound); no boss or ending theme (none needed: no boss).
  No third-party music.
  *Approved, not built (v1.5):* a theme per world, played on its world card (our own, about 20 KB each), an ending theme and a credits theme; still no music in play. Third-party music only with artist and licence recorded and credited (`modcredits`). Music budget about 100 KB, on disk.

## 10. Presentation

| item | state |
|---|---|
| boot | Desert Palms Software logo: 288x242, 16 colours with 4 reloaded per line (about 320 colours), fades in and out, fire skips; its 47 KB is kept until quitting (freeing it froze the title for ~6 frames, `670227d`) |
| title | the ROTOR RESCUE logo (192x129, 4 planes, 16 colours with 8 reloaded per line, ~800 changes) over the sky; A1200: a 224-colour version; the theme; author and copyright lines; hi-score in the score bar |
| attract loop | **yes:** title 600 frames (12 s), then the hall of fame 400 frames (8 s), then a recorded demo of the first trip (1453 input frames, 29 s, DEMO in the score bar, no score kept), then the title; fire ends it |
| options | difficulty by Ctrl+L, no menu |
| mission flow | a preview card before each mission (120 frames, fire skips): MISSION n OF 3, its name, a strip map of the battlefield (enemy ground red, fence, four barracks, pad, hangar, a "you are here" arrow), "64 HELD IN 4 BARRACKS" and a one-line brief (TANKS ON PATROL / ...AND JETS / ...AND AIR MINES). No start banner |
| world card and route map (approved, not built) | a world card before each world (loading behind it, the disk request, its theme), a preview card with a strip map of the whole world, and a route map across the twelve missions (`missionmap`) |
| between missions (approved, not built) | a hangar screen for the upgrades (section 4.5), the merit total, the continue save |
| in play | HUD: SAVED / ABOARD / LOST / score; choppers left; a radar of the whole battlefield |
| first-use prompts (v1.19; Paul's playtest, 2026-11-17: "can't figure out how to bomb on rotor rescue with keyboard controls"; approved by Paul) | F-35's rules (`HINT_*`, `hintUpdate`): a timed line in the message layer's first play line (y 96, centred, the message pen: under the HUD, above the chopper's usual height, over the sky; the HUD's two slots are 8 and 7 letters and the line is full). **HINT_BOMB**: a tank or a closed barracks on screen, ahead of the chopper's cannon within 160 px (or under it, up to 24 px behind), the chopper flying (not landed, not dead, not the demo, not on a card): `HOLD DOWN, THEN FIRE: BOMB` for 50 frames, then `BTN 2, THEN FIRE: BOMB` for 50, then the first again (150 frames in all). **HINT_STANCE**: while the chopper is facing the screen in flight: `FIRE: BOMB   UP AND FIRE: GUNS` (up to 150 frames, gone when it turns back). At most 4 shows of each kind a game, a kind's next show 500 frames after its last began, **off for good once 3 bombs have been dropped** (the demo's don't count). The counts survive a lost chopper and the next mission and reset on a new game (`logicStartGame`, the debug start from the title). Boats don't trigger it (the cannon sinks them). It changes no gameplay and costs no chip: it types into the message line that cards and game over already use (`msgUpdate`, 2 letters a picture); serial `AGK hint=bomb`, `AGK hint=stance`, `AGK hint=none`. `tests/hint.agk`, `testHint*` |
| end of mission | MISSION COMPLETE, SAVED n OF 64, 250 frames, then the next card |
| game over | GAME OVER, total saved, rank, score; 300 frames, fire skips after 100; then the hall of fame if the score ranks |
| high scores | a hall of fame of 5 (initials, score, mission reached, rank), saved to the floppy as `rrscores` (56 bytes, checksummed, read at boot); name entry **with the stick or typed on the keyboard** (v1.10, the kit's `hiscore` technique, a DPS requirement): stick as before (up/down letter, repeating every 8 frames; fire the next; left back one), keyboard A-Z, 0-9 (the digit row and the keypad), Space, Backspace (blanks the place before the cursor), Return or keypad Enter (confirms at once: places not reached stay as they were) and **Esc (ends the entry at once and keeps the score with the initials so far, exactly what the 30 s timeout does; it never discards the score and never quits the game from the entry; Paul, 2026-11-13)**; raw keycodes, so Shift makes no difference; the two can be mixed in one entry; 30 s limit, restarted by any key. Entry is a state of its own: the game's keys (M, P, Ctrl+L, Space as fire) are not read in it, so typing an M does not mute the music; Esc is read by the entry only (it ends it), and the game's quit is a fresh press (`keyUse`), so the Esc that ended the entry, still held, does not quit from the table. The file is unchanged (magic `RRH1`, same 56 bytes, so old disks' tables read as they are; a name may now hold digits, which the kit's first copy refused to read back). The table lines are the kit's `hiRow`: `- 1  ABC  012345  MSN 2  CAPTAIN  -` (35 columns; was 31 with `M2`), rebuilt only when the table or the new entry changes; defaults DPS 5000, PRS 4000, ACE 3000, AGK 2000, RTR 1000. A write-protected disk is found with `Info()` before any write; the table still shows with "DISK WRITE-PROTECTED: NOT SAVED". Never brings up a requester |
| ending | *shipped:* ALL MISSIONS COMPLETE: the tally and a rank. *Approved, not built (v1.5):* an **animated ending of 25-35 s in three outcomes**, by people saved of 768: **90% or more** a sunrise flight home, the crowd waving, the flag raised, the fanfare, credits over the dawn; **60-89%** dusk, empty seats, the names of those saved scroll; **under 60%** night, a lone chopper, a memorial and a silent beat. Built from the engine's bands, sprites and copper sky plus two large stills (studio default thresholds and tone). A game over with no lives left is not an ending: it goes to the hall of fame, as before |
| credits | *shipped:* the author and publisher lines on the title. *Approved, not built (v1.5):* a **credits screen** (code, art, music, sound, the art tools used, third-party licences; the `modcredits` pattern), a page in the attract loop and after the ending |

## 11. Tech plan

- **Display:** PAL lores 320x256, a dual playfield of 6 planes. PF1 (planes 1/3/5, colours 1-7):
  lines 0-23 the HUD bitmap, below it a double-buffered frame buffer for every BOB, drawn by the
  blitter from a queue while the CPU runs the game. PF2 (2/4/6, colours 9-15): four looping
  parallax bands, each with its own fine scroll and palette. The sky above the cloud band (line 72) runs
  with 3 planes to leave bus to the CPU; 6-plane DMA starts at `CLOUDS_Y` (README still says "the top
  120 lines", from before the cloud band). DDFSTRT `$30` (a fetch word early for
  fine scroll; costs sprite 7). One raw copper list per buffer, rebuilt each frame; 496 entries
  allocated, 465 used (`b9d5987`). The title has its own list (the logo as a 4-plane section).
- **The perspective floor** (decided by Paul, 2026-11-12; section 8): lines 180-198 of PF2, one copper
  block per line at the line's start (WAIT, COLOR00 the plain's ground, BPLCON1 the line's fine scroll,
  BPL2MOD the step to the next line's start: the modulo moves all three PF2 planes with one MOVE, so
  a line costs 4 entries, 7 on AGA with its 24-bit COLOR00). At the end of line 179 the copper points
  PF2 at floor row 0 and loads the land palette (row 0 is transparent, so the load can't show). The
  far strip is the mountains band's last 10 rows: no copper of its own. Per frame, only when the
  camera moves, `logicFloorScroll` (`logic.c`) writes each line's BPLCON1 and modulo straight into
  the back list: one 16 x 16 multiply a frame, then an add, a mask and a table lookup a line (no
  divide, no per-line wrap: each row's loop is its speed times 576 px). The same code serves the
  A1200 in quarter pixels (a mask keeps or drops BPLCON1's 35 ns bits). The per-mission ground
  gradients are made at startup. The sky's COLOR00 changes stop at line 179 (they stay on lines the
  mountains cover: COLOR00 is also the overscan border beside the picture). The floor's art (640 x 19, 3 planes, 4.6 KB
  chip) is a floppy file, `plainfloor.bin` (1.2 KB packed), loaded with the title logo during the
  splash: in the program it pushed AROS's first picture to frame 2992 of its 3000.
- **World 2's display** (v1.11, the first slice of world 2; `docs/STYLE-world2.md` section 2; *built*, behind the
  debug keys Ctrl+L, W, then 1-6 = missions 1-6, until the gameplay slice adds world 2's barracks, pads and enemies).
  `g->world` (0 desert, 1 islands) with `g->mission` the mission within the world (0-2), so missions 4-6 are world 1
  with missions 0-2, and every per-mission table keeps three rows. In world 2 the logic has the 4608 px map
  (`logicWorldW`), the start on the base island's pad (x 4136), no barracks, enemies or weather yet, and the radar at
  x / 24; `logicTerrainFloorY` (the tile map: island tiles ground at `GROUND_Y`, water and reef not landable) is the
  hook for the gameplay slice. The title, the hall of fame and a new entry are world 1's (`setState`).
  - **Lines** (game lines): 24-179 sky, a COLOR00 MOVE on every line (top to H), the clouds at 72 (/8) and the far
    islands with their strip at 120-179 (/4, one band) exactly as world 1's clouds and mountains; the far palette at
    113; line 179 the sea palette at its start, PF2 to sea row 0 at `$D8`; **the sea** 180-207, a block a line
    (WAIT, COLOR00 the H-to-C gradient, BPLCON1, BPL2MOD: 4 entries, 7 on AGA), line j at (28 + 3 j) / 112 of the
    camera (`logicSeaScroll`: one 16 x 16 multiply a frame, no divide; unit-tested against the exact value at every
    camera); line 207 the island palette after its block, PF2 to the strip at `$D8`; **the terrain strip** 208-255,
    1:1, with C under it (the gradient's last colour). No WAIT passes line 255 (no `$FFDF` wrap).
  - **Copper timing, measured** (a red/green/blue COLOR00 probe, `W2_COP_PROBE`, first pixel in the full frame):
    line 180's block is done before the line's first pixel on a500 (x 0) and in the left border on a1200 (x 13,
    as world 1 since review A2); the island palette at 207 is done by x 0 (a500) or runs to x 93 (a1200) on the
    sea's empty last row, where no PF2 pen shows; the strip's switch at 207 `$D8` is done before line 208's first
    pixel on both.
  - **Copper lists**, sized per world from the layout, both buffers at the larger (measured, `copper_used`): OCS
    world 1 596 (v1.12; 560 before), world 2 629 (every mission) of **661** allocated (was 633: +224 bytes of chip
    for both lists; the art pilot's estimate was "about 500 of 496" from the old 465 count); AGA world 1 1127, world 2
    1187 of **1339** (was 1278). Both worlds' lists have a sky MOVE on every line (v1.12: world 1's had one only where
    the colour changed), so a world's three missions share one layout: a new mission in the same world patches its
    colours into both lists (`copperMission`: the sky, the floor's or sea's ground, the PF2 palettes) instead of
    writing them again; only a world's load writes them, with the bitplanes off. That includes world 1's load at
    the title after a game in world 2: the title writes the game lists in those two dark pictures, so the next game's
    start and the attract loop's hall of fame find them ready (`tests/w2over.agk`: 4 dropped pictures on the A500,
    worst 247 %, before; none after).
  - **The sea's art** is 768 px wide on the floppy (`make_world_disk.py` widens the 640 px art by its own loops):
    row j loops every 4 (28 + 3 j) px, up to 436, and a row's 336 px fetch starts anywhere in its first loop.
  - **Sea animation** (v1.31; Paul's native playtest, 2026-11-24: "the sea layer could use some animation to show
    movement in the water, to make it more interesting and 3D appearing"; world 2 only, world 1's floor is
    unchanged; `tests/unit/test_logic.c` `testSeaPhase`, `testSeaDrift`, `testSeaPens`, `testSeaCopper`,
    `testSeaSwell`, `testSeaCameraAlone`, `tests/w2sea.agk`). Two effects, both deterministic from the game frame
    counter alone (`g->frame`; no LFSR, no timer, so the goldens and the native parity gate stay exact), both
    written into the copper values the sea already has: **no new copper entry, no chip byte, no bitplane or
    blitter work.** The art (`sea_floor.txt`, `world2.bin`) is untouched.
    - **The swell** (a per-line drift of the near water): the phase is `(frame >> 2) & 31` (`logicSeaPhase`: a step
      every 4 frames, a cycle of 128 frames, so the 16-bit counter wraps 65535 -> 0 without a jump). Sea line j
      moves sideways by `A_j sin(2 pi (phase - j) / 32)` quarter pixels (`logicSeaDrift`, the table `s_pSeaDrift`
      generated by `art/tools/make_sea_drift.py`), `A_j = (28 + 3 j) / 7` quarters times a ramp: 3.8 px at line 206
      (j 26), the camera's own 1/z ratio, so the near water swings the most and the sea reads as depth. Lines 181-191
      (j 1-11), the far water, stay still (calm at a distance: the texture thins with distance; and each line moved
      costs a copper write), line 192 moves a sixth of its amplitude, line 197 (j 17) and nearer all of it. The
      `- j` makes the wave run toward the player, one cycle across the lines, so neighbouring lines never differ by
      more than 3 quarters (under 1 px: the water shears, it does not tear); the bare rows 180 and 207 stay still.
      It is added to each line's left edge (`logicSeaScrollSwell`: the camera's `(28 + 3 j) / 112` scroll, unchanged,
      plus the drift, wrapped into the row's own loop, so a row never leaves its 768 px art at any camera): the OCS
      list writes whole pixels (BPLCON1 and the modulo), the AGA list quarter pixels.
    - **The shimmer** (a colour ramp): pens 4-7 of the sea palette lighten by 0, 1 or 2 steps a channel (clamped at
      15) on a triangle of 32 steps, a step every 8 frames (a cycle of 256 frames, a divisor of 65536), pen k a
      quarter of a cycle after pen k - 1 (`logicSeaPen`, `logicSeaPenLevel`, `logicSeaLighten`): a swell of light runs
      from the far crests (pen 4) through the mid crests (5) to the near glints (6, 7) toward the player, and no
      row is ever lit as a whole (every line is 22-29 % drawn). The four palette MOVEs after line 179's WAIT are
      patched in place (`seaPens`, per list, from a table of the three colours each pen has, built with the
      gradients at world 2's first load); pens 1-3 and the island palette are never touched, so strip row 0 and the
      island seam are as before; a pen is never darker than the mission's own colour (M4 pen 7 and M5 pen 7 are
      0xFFF and stay).
    - **Rules that keep the frame budget** (the swell costs CPU: the copper list is in chip RAM and the lines are
      written under 6-plane DMA, about 0.5 % of a frame a line). (1) Moving camera: the sea's lines are written
      anyway; `logicSeaScrollSwell` adds the near lines' drift (about 3 % of a frame more than the camera alone).
      (2) Still camera (a hovering or landed chopper): only the swell moves, `logicSeaSwell` rewrites the near
      lines (12-26) from the base edges the last scroll stored (`s_pSeaBase`, valid only for the camera it was
      stored at), every 4th picture per list (about +8 % of a frame in that picture, +4 % on average), and the pens
      every 8th, **but only in a picture that queued at most `SEA_BLIT_MAX` (14) blits**: a hovering chopper in
      the quiet has 11-13 (measured on the a500, the same blits on every profile), a fight 19-23, so the busy pictures, which are the frame
      budget's worst, skip the swell and take it up in the next quiet one. The blit count is a function of the
      game state, like the frame counter (not of timing), so the screen stays deterministic. (3) A list just
      written or patched (a world's load, a mission's start: copperMission, copperWriteList) holds the swell and
      the shimmer for 12 pictures (`s_ubSeaHold`, `SEA_NO_SWELL`: `logicSeaScroll`, the camera alone, exactly the
      old routine): the mission-start pictures are 94-97 % already. The lists are double buffered, so each
      remembers the phase and the pen step it shows (`s_pSeaPhase`, `s_pSeaPenStep`). The clouds and the far
      islands still move only with the camera. Ctrl+L, S holds the swell still (tests: `tests/world2.agk`
      measures the camera's scroll of every layer with it).
    - **What it costs** (measured, a500, the game's own meter, main 5f60f11 against this branch; copper lists 629 of
      661 OCS entries and 1187 of 1339 AGA as before, chip free in play 97 KB lowest on the a500 (floor 96) and
      74 KB in the squeeze run (floor 64) as before, the program +2.7 KB of fast RAM (the 896-byte table and the
      code)): hover in the quiet (`tests/w2sea.agk`) average 37 -> 41 %, worst 38 -> 46 %; `tests/w2play.agk` worst
      82 -> 82 %, average 43 -> 46 %; `tests/world2.agk` worst 64 -> 63 %, average 35 -> 38 %; `tests/world2pal.agk`
      (the mission-start picture) worst 94 -> 97 %; `tests/w2over.agk` worst 69 -> 73 %; no dropped frame in any
      world 2 scenario; world 1 (`perf`, `jets` 97 % then, 92 % since v1.40) unchanged to the point. The extra average is not paid for by
      a saving elsewhere (the budget is protected by the blit gate and the hold, not by a cheaper routine; see
      the handoff).
    - **Options measured and not taken** (Paul's list): (c) whitecap or crest BOBs: the slice E table counts
      1,080 bytes of chip for three whitecaps, a blit each a picture, no art for the sea band yet, and PF1 sits in
      front of the boats' wakes; not needed for the effect. (d) glints on a sprite: channel 6 is the far sky's,
      4-5 are the weather's (colours 25-27 are the weather's, not white); a free pair would need its colours
      moved and `SEA_LINES` SPRxPOS MOVEs: 28 + 2 pointer entries of the 32 the OCS list has free (629 of 661),
      nothing left for later slices; the next best if Paul wants more sparkle. (e) a vertical bob of the horizon
      or the near lines: every line's WAIT would move, and the lines above are the far strip's own, so a copper
      timing change in the most loaded part of the list; not done. A whole-sea swell (lines 1-11 too) was built
      first and measured: +6 % average and +14 % in the swell's pictures, 12 points more worst load in
      `tests/w2play.agk` (82 -> 94, over its 85) and a dropped frame at the mission start; the far water is calm
      for that reason.
  - **Base ocean animation** (v1.43; Paul's queued item: "animate the base ocean layer", the water of the terrain
    strip, lines 208-255, where the islands and ships stand; world 2 only; `tests/unit/test_logic.c` `testBaseWave`,
    `tests/w2base.agk`). The sea above the strip moves already (v1.31); the strip's water was still. **No new copper
    entry, no chip byte, no bitplane or blitter work, no swell of the strip's lines** (a swell would shear the islands,
    which are in the same bitmap; there is no clear water below them to swell). It is **colour cycling of the strip's
    three water pens**: the art draws every water stroke (a horizontal run of pixels) in one of the pens 1, 6 and 7
    (the three "classes"), chosen by the stroke's row on a perspective phase (`u = 2.2 ln(1 + y / 12)`: a wave about 5
    rows long at the strip's top, about 25 at its foot, so the water is farther away at the top) plus a random
    +-0.3 of a wave a stroke, so no row is a band and no two neighbours match; the island's own pixels use only pens
    2-5 (the bush's and boulder's dark underside and the pebbles: pen 1 -> pen 2; the generator,
    `art/tools/world2/w2tiles.py`, tells a water pixel from an island pixel by who drew it, not by colour). The copper
    then lets the three pens' colours run through a wave that is the sum of three slow movements, every cycle a divisor
    of 65536 so the 16-bit counter wraps with no jump (`logicBaseWave`, `logicBaseColor`, `logicBaseWaveTab`): **the
    swell**, a crest of light rolling **toward the player** (32 steps of 2 frames, a cycle of 64; pen 1 is class 0, pen 6
    class 1 and pen 7 class 2, 11 and 21 steps on); **a counter-wave** a third as strong running the other way, slower (32
    steps of 8 frames, 256; so the crests do not all move one way at one speed: in the water they cross); and **the
    breathing** (32 steps of 16 frames, 512): how deep the dark goes, 3 to 6 sixteenths of the way from pen 1 to pen 6,
    so the whole strip slowly deepens and clears. A colour is the dark, through the mission's light (pen 6), to near-white
    (pen 7: 13/16 of the way, never above it). The average colour of the three pens is nearly constant (the strip never
    flashes as a whole: the mean luminance of open strip water moves +-6 % over the cycle, measured). Crest density
    by depth is in the art: a stroke's class jitter grows from +-0.2 of a wave at the top to +-0.4 at the foot, and the wave is
    5 rows long at the top and 25 at the foot. Drifting foam is the wave's white peak: only pen 7 reaches near-white, a
    fifth of the strokes at any time. The three palette MOVEs of the island palette after line 207's WAIT are patched in
    place (`basePens`, per list, when the step moved: 3 words from a 4 x 17 table per mission built at world 2's first
    load, `s_pBaseTab`, 408 bytes of fast RAM; the first build computed the colours in full each picture and cost 2 to 5
    points of the worst picture, w2play 85 -> 90 against its 85 and w2frtr over its 98, so it was changed to the table and to the sea swell's
    rule: **only in a picture that queued at most `SEA_BLIT_MAX` blits**, and not in a list's first pictures). **The
    two lists keep in step (Paul's playtest, 2026-11-25: "some flicker when flying around").** The first rule let a
    busy picture's list keep an older step and then jump to the frame's step in its next quiet picture: the two copper
    lists, shown in turn, then differed by up to 14 steps and the pens jumped up to 9 levels of a channel in one picture
    (the wave itself moves 3 at most a step), measured on the a500 in `tests/w2play.agk` (people boarding, boats: 31 % of
    the pictures queue more than 14 blits, runs of 22 busy pictures; a list was up to 29 steps behind the frame). The
    rule now (`logicBasePenKey`): **a list written goes to the other list's step, or one step past it when the two are
    level, never past the frame's step**, so the two lists never differ by more than one step, the size of the wave's own
    step. The wave therefore runs at its own pace in quiet pictures and slower in busy ones (about a quarter of its pace
    through the boarding in `w2play`; it never stops while any picture is quiet) and stays behind the frame counter once
    it has fallen behind (invisible: a slow cycle with no beat to keep). Writing the pens in every picture instead was
    measured and rejected: 3 MOVEs and the lookups cost 2 points of the worst a500 picture (`w2play` 85 -> 87 against its
    85). `bspread` in the status line is the largest difference between the lists' steps after a write; `tests/w2play.agk`
    expects 1 at most (and `testBaseFlicker` the same on the measured blit counts). Deterministic from the frame counter
    and the blit count, as the sea's (`g->frame`; Ctrl+L, S holds it, with the sea's). Chip RAM: none (the tile bank is
    the same size, art only).
    *Not done:* a swell of the strip's lines (above); shore foam or wave rings as BOBs (a blit a picture, chip, Retro
    Diffusion art): not needed, the cycling reads as swell (RD spend: none).
    *Frame limit and load time* (decided by Paul 2026-11-26): the `w2play` limit goes 85 -> 88. a500 worst picture: main 84, the
    ocean +1, the rotor channel claim (v1.42) +1 = 86, deterministic (priced one component at a time, each disabled in turn).
    The a500 world-2 load takes 402 ticks on this branch against 369 on main (+33, a500 only: a500-ks31 287 -> 288 = +1; the
    +33 has been there since the first `main.c` commit of the branch; the mechanism is unproven, and a500 load ticks shift by
    tens with any `main.c` change). The scenarios' 'AGK world 2 loaded' waits are 525 = 402 + 30 % slack (they were 400, then 700
    on this branch); goldens depend on that boot-relative clock, which is why the a500 and ks31 pictures differ outside the strip.
  - **The strip**: a pointer-creep ring (the spike's), 3 separate planes of 44-byte rows (2,734 bytes each, 8.2 KB),
    22 tile columns kept drawn (both pictures' fetches); a column coming into view is 3 queued blits (A -> D, 48 words
    a plane) from the 36-tile bank, one every ~5 pictures at 3 px a frame. After a camera jump (a mission's start)
    the 22 columns are drawn 4 a picture while both lists point the strip's lines at the blank row, so the islands
    appear whole under the mission card (a whole span in one picture was 135 % in the spike); after a world's
    load, all at once in the load's picture.
  - **Memory**: `disk/world2.bin` (42,720 bytes, 14.6 KB packed: the clouds, the far islands, the sea, the tile
    bank, the map) loads at the mission card into the **world arena**, the band region of the floppy art's chip block
    (46,320 bytes: world 1's clouds, mountains, title mountains, hills and ground); `disk/world1.bin` (the same
    region from `diskart.bin`, 14.9 KB packed) loads it back when the game returns to world 1. The strip's 8.2 KB is
    allocated while in world 2. The bitplanes and sprites are off from the load's picture until both lists are the
    new world's (2 pictures), so no half-loaded picture shows. Chip free in play in world 2, lowest (`tests/world2.agk`, asserted 96): a500 110 KB, a500-ks31 145 KB,
    a500-aros 151 KB, a1200 1157 KB (world 1 unchanged: 118 / 153 / 159 / 1165). The program is the same size as
    before (323.8 KB: the copper writer, `genericCreate` and `copperWriteList` compiled for size pay for world 2's
    code); AROS's first picture 2346 (2349 before; `tests/memory.agk` asserts 2350); world 2's gradients are made at
    its first load, not at boot.
  - **Frame** (the game's own meter, a500 and a500-ks31 alike): the flight across the whole map at full speed
    (`tests/world2.agk`, asserted: no dropped frames, worst 85) worst 46 % in flight, 63 % in the card's window
    after the load, average 28-37 %; mission changes and the way back to world 1 (`tests/world2pal.agk`, no
    dropped frames) worst 76 %. A strip column is about 1 % of a picture (3 blits). World 2 has no BOBs yet: the
    gameplay slice's objects come on top of these numbers. World 1 unchanged (`tests/perf.agk` 67 %).
  - **The copper writer** writes whole longs (`cwFastWait`, `cwFastMove`: the same bits as ACE's bitfield setters,
    which read-modify-write chip RAM a field at a time). A whole game list still costs most of an A500 frame (the sky
    loop, the CPU in slow RAM under 6-plane DMA; measured in world 2 before its mission patch: 200-277 % pictures,
    2-4 dropped at each mission card). **World 1's mission cards** (v1.12, `tests/w1card.agk`, asserted: no dropped
    frames, worst 90): the cards of missions 2 and 3 dropped 4 pictures each, worst 250 %, on a500 and a500-ks31
    (both lists written again); patched, worst 81-82 % (the patch is ~17 % of that picture, the rest the mission's
    start and the route card), a500-aros 63 %, a1200 27 %. The game's start from the title writes nothing (the lists
    are mission 1's), worst 57 %; world 2's load 60 % and the way back to world 1 62 % (both lists written with the
    bitplanes off, pictures not counted). Same pictures as the full rewrite (pixel-compared).
- **World 2's gameplay** (v1.13; `docs/CONTENT-PLAN.md` sections 2-4; *built*; unit tests `testW2*`, scenarios `tests/w2play.agk`, `tests/w2boats.agk`).
  - **The land**: `g_pIslands` (`logic.c`) is the tile map's five "island" lines (span, barracks centre, pad centre, widest bed); a unit test
    checks every column against `logicTerrainFloorY` on `art/world2/tilemap.bin`, so `logicOnWater` (the logic has no map: it lives in the arena)
    and the tile classes cannot drift apart. Barracks x 160, 1232, 2080, 3120 (mission 4's four islands); pads 256, 1392, 2220, 3312 and the
    base 4136; the hangar 4212 (past the widest pad's zone). `floorAt` is the old hook: barracks and hangar roofs, then a pad (`logicPadAt`: the
    pad's width, cached per skill in `g->padPx` and cut to the bed, plus the 6 px) is ground, anything else on the island or the sea a floor
    to stop on. The base pad is the unloading, the repair and the jet budget's reset (`onBasePad`).
  - **People**: they wander about half way from the barracks to its pad (`tBarracks.wx`), hear a landed chopper within 160 px (`W2_CALL_RANGE`:
    a pad is up to 192 px from its barracks), at most `W2_OUT_MAX` (6) are out on the beach at once (the rest come out as those board or die),
    and unloading runs at 2 px a frame into the hangar (a person on screen is about 1.5 % of an A500 frame: 16 on a beach beside a pad
    dropped frames, measured in `tests/w2play.agk`).
  - **Boats** (`tBoat`, `BOAT_MAX` 6, placed by `s_pBoatPlace` per mission, slot kept when a lower skill leaves a gunboat out): mission 4 has
    two patrol boats and gunboats in the gaps west of islands 2 and 3 (the second from skill 2); missions 5 and 6 have more gunboats (skill 1 /
    3 / 2 / 4 extras); the patrol boat is unarmed in mission 4 and armed in 5 and 6 (`logicPatrolArmed`, `logicPatrolTell`). Rules above (section 6); tests: `testW2BoatsArePlaced`, `testW2BoatTellTable`, `testW2GunboatTellsBeforeItShoots` (every
    shell after the table's tell, braced), `testW2GunboatFairness` (high chopper, off screen, point-blank, out of sight, take-off grace),
    `testW2LandedChopperHasGrace` (dwell by skill, never on 1-2), `testW2OneBoatAttackAtATime`, `testW2BoatsCanBeShot`, and for the patrol boats `testW2PatrolArmedFromMission5`, `testW2PatrolUnarmedInMission4`,
    `testW2PatrolTellsThenShoots`, `testW2PatrolFairness`; scenarios `tests/w2patrol.agk` (mission 5: the tell, then the shell) and `tests/w2patrol4.agk` (mission 4: none). Boats are on the radar
    (row 4: gunboat red, patrol boat stone).
  - **The way through the game**: mission 3's clear goes on to world 2's first mission (lives refill to 3, the score is kept); world 2's third
    clear is the end (`GS_VICTORY`, "ALL MISSIONS COMPLETE") until world 3 exists. The hall of fame's `reached` is the mission 1-6 (7 = all done:
    `g_sHiConfig` max 7, the file format unchanged, an old table reads as it was). The card: "MISSION 4 / REEF RUN / 64 HELD ON 4 ISLANDS / PATROL
    BOATS AND GUNBOATS" and a strip map of the 4608 px at an 18th (the islands, barracks, pads, hangar, the base ours, "you are here"), made at
    the world's load (making it costs about two frames: not in a card's picture; the boats are in the brief, not on the map).
  - **Memory** (measured): world 2's BOBs are `disk/world2.bin`'s tail, 20,704 bytes after the 42,720 of display art: the island barracks (4 frames),
    patrol boat (4: a wake a facing, and from v1.15 the shot tell a facing; +1,088 bytes), gunboat (6: idle, tell 1, tell 2 a facing, the fire frame is tell 2 held), the pad's parts (lit
    phase only, no masks: they are put together at the load), lamps, splash. They load over the arena's spare 3,600 bytes and **world 1's own
    BOBs' chip** (fence, barracks, tank, jet, mine, flag, 9,792 bytes, loaded back from `world1.bin`, now 56,112 bytes, on the way home), and the
    art block grew by 11,352 bytes for the rest (6,376 for the set, 4,976 for the pads' four composed widths, one BOB each: **a pad is 1 BOB**, not
    15, which cost 30 points of frame). `diskart.bin`'s layout is now chopper, the BOBs both worlds use, the bands, world 1's BOBs. Chip free
    in play, lowest (`tests/memory.agk`, `tests/w2play.agk`, asserted 96): world 1 a500 107 KB, a500-ks31 142, a500-aros 148, a1200 1223; world 2 a500 99 KB (was 110: 11 KB
    for the set; 14 KB was spare), **98 KB with the patrol boat's tell frames (v1.15; a500 98, a500-ks31 132, the same in missions 4, 5 and 6 as they all load one set today)**. **Per-mission sets, measured** (`tools` sizes of the art as drawn, 48,240 bytes in all): mission 5 adds the radar
    (5,616) and windsock (480), mission 6 the destroyer (one facing 5,280, both 10,560), its turret (4 frames 1,824, 8: 3,648) and the
    whitecaps (1,080): a set of 31.8 KB (M5) or 33.9 KB (M6) with the pads' composites, against 25.7 KB today: chip free would be about **92 KB (M5) and 90 KB
    (M6) on the a500, below the asserted 96**. (v1.15: the patrol boat stays in M5/M6 and is armed there, with its tell frames, +1.1 KB: the projection is about 91 KB (M5) and 89 KB (M6).) Levers left: compose only the pad widths the skill uses (1.9 KB at skill 3 for 5.0), one destroyer facing, load a mission's own set at its card (the loader is `bobsBind`), or lower the floor (SPEC 2: 64).
  - **Jets** (v1.16; section 6, "Jets in world 2"; `testW2Jet*`, `tests/w2jets.agk`, `tests/chipwinuae.agk`): the jet is the set's last BOB (`BOB_W2_JET`, world 1's art as it is: 576 + 576 bytes
    for the 4 frames and their masks, 1,152 bytes of the floppy file and of the art block's chip, `WORLD2_EXTRA` 7,592 -> 8,744), 4 more blit templates (`TPL_MAX` 54 -> 58: 28 shared + 29 world 2's = 57), the radar's
    jet blips on world 2's 4608 px, the jets' table, the safe x and the shared attack token in `logic.c`. **Chip free in play, lowest on the a500: 98 -> 97 KB** (the gate asserts 96: a margin of about 1 KB), and
    **74 KB with the squeeze below**. Levers (docs/STYLE-world2.md section 6) in use: none new for the jet, it costs only its own frames; the ones already taken stay (the gunboat's fire frame, one wake frame, the pad's
    lit phase, the pad as one BOB a width). Left, with their bytes (measured sizes, 48,240 as drawn): compose only the pad widths the skill uses (skill 3: widths 48 and 64 only, 1,936 of 4,976 bytes, **-3,040**;
    skill 2 -1,656; skill 4-5 -4,144; skill 1 none, and a mid-mission skill change, Ctrl+L, would need the pads composed again), one destroyer facing (-5,280 image and mask, mission 6), a mission's own set loaded at its
    card (`bobsBind`, -the radar and destroyer parts when a mission lacks them), the floor of 96 KB lowered (SPEC 2 keeps 64 as the game's own). Missions 5 and 6's radar, windsock, destroyer and whitecaps still need them
    (section 11, "Per-mission sets": about 91 / 89 KB before, 1.1 KB less with the jet).
  - **Chip allocations are all checked, and a squeeze test reproduces WinUAE's smaller chip** (v1.16; lessons from Rotor Raider's blank screen, whose unmade BOBs wiped the frame on WinUAE, which has about 23 KB less chip
    free than vAmiga at the same point): `main.c` makes every chip allocation through a check (`CHIP_CHECK`: the art block, the plain's floor, the frame buffers, the HUD, the blank row, the title's memory, the chopper's
    sprite blocks, the sky and weather lists, world 2's terrain ring; `AGK ERR out of chip RAM: <what>` on serial and a halt for the essentials, the rotor's wave degrades to no rotor sound); the BOBs of this game are
    bitmaps inside the art block, so none can be "unmade" on its own (the block is the one allocation), and none is drawn before it is made. **Ctrl+L, Q** (test hook) takes `CHIP_SQUEEZE_KB` (23) of chip away, **Ctrl+L, E**
    four times that; `tests/chipwinuae.agk` plays world 2 from the title with the squeeze (jets, the HUD, the drawn jet BOB, no `AGK ERR`, the lowest chip free 74 KB against the game's floor of 64).
  - **World 2, slice 2: missions 5 and 6 finished** (v1.20; `docs/CONTENT-PLAN.md` sections 2-4, 7, 9 phase 2; branch `w2-slice2`; the numbers are the studio's proposal until Paul's playtest, except where the plan already fixes them).
    Built in five slices, each merged-quality (unit tests red first, a scenario, the four-profile gate); mission 4 and world 1 never change (every existing golden stays unless a diff is explained by the slice):

    | slice | what | art it needs | chip it adds (a500 bytes, image and mask) |
    |---|---|---|---|
    | A | **radar sites** (missions 5 and 6) and the **jet zone rule**; the zones red on the HUD radar and the card's strip map | the radar, 4 frames, as drawn (`art/world2/radar.txt`) | 5,616 |
    | B | **wind gusts** and the **windsock** | the windsock, 2 frames, as drawn | 480 |
    | C | the **ship-deck winch rescue** (mission 5) | deck ship, rope, hook, climbing person (drawn, `w2-art-cd`) | 7,632 (measured, v1.22) |
    | D | the **timed freighter** (mission 6): at most two timed sites a mission, 90 s on normal, 120 / 105 / 90 / 75 / 60 by skill | freighter (4 sinking stages) and sink spray (drawn, `w2-art-cd`) | 14,688 (measured, v1.23) |
    | E | the **strait destroyer** set piece (mission 6) with its anti-stall, and the **zone bonus** | the destroyer's hull (with the listing frame), turret (one facing), turret damage, AA gun and barrage marker (drawn, `w2-art-e`) and the whitecaps; one facing only | 10,928 (measured, v1.24; mission 6's part) |

    *Radar sites* (slice A): `tRadar` (`RADAR_MAX` 3), placed per mission by `s_pRadarPlace` (mission 4: none; mission 5: x 1608 on the second island's east end and x 3416 on the fourth's, beside its pad; mission 6: those and x 440 on the first; all on flat or bush tiles, off the pads), each on its island's ground,
    4 HP (a cannon hit 1, a bomb kills), a box 34 x 40 standing on `FEET_Y`, 150 points, no shot and no tell (it never attacks: the plan's roster, section 4.4); a destroyed site is gone (no wreck frame is drawn: an art follow-up). Its **zone** is the stretch `x - 600 .. x + 600` (1200 px, `RADAR_ZONE_HALF`), cut at the map's left edge and at `W2_JET_SAFE_X` (3800): mission 5's zones are 1008-2208 and 2816-3800, mission 6 adds 0-1040, so the sea between the second and fourth island, 2208-2816, is the strait, free of jets (mission 6's destroyer will sit there, slice E).
    **The jet zone rule** (`logicJetZone`): in a mission with radar sites, a jet may START (appear) only while the chopper is inside the zone of a LIVE site, so outside every live zone no jet appears; destroying a site removes its zone for the mission (a jet already in the air flies on, as the base-approach rule already lets it).
    Where two zones overlap, either site alive is enough. Missions without sites (world 1, mission 4) keep today's rule exactly. The per-trip jet budget, the first-jet delay, the warning, the tell, the ground grace and the take-off grace are untouched: the zone only decides where the next jet may start,
    so a player who keeps out of the zones meets fewer jets than the budget and, once every site is down, none. The first-jet delay (`W2_JET_FIRST`, 500 frames) counts only while the chopper is inside a live zone (as it already counted only left of `W2_JET_SAFE_X`): the first jet of mission 5 comes 10 s after the chopper first spends time in a zone, so on a normal trip it is met over the second island, not at the base. The base island's approach is never a zone, so what the HUD and the card show red is exactly where a jet can start.
    *The HUD radar* draws each live zone as a red bar (pen 5, `0xD42`) on its bottom (ground) line and a white dot (pen 3, two px, row 6) at each standing site; both go when the site dies (redrawn only on a change, as the barracks are, and not in the first six pictures of a card, because the picture that starts a mission is already the heaviest of the game: 94 % on the a500 at baseline, 96 % with slice A, `tests/world2pal.agk`). *The card's strip map* draws the mission's zones as a red line over the islands (row 1) and a tick at each site (row 2): rows 1-2 of the map are made for each of the world's three missions at the world's load (192 bytes each, `routeOverlays`), and the card's first picture copies the mission's own two rows to the screen in place of the image's (`routeShow`: the same longs, nothing extra; drawing them row by row at the card cost 43 % of a frame in the 6-plane display).
    *Memory* (the levers; a500, measured with `AGK w2 chipbytes` at the end of the world's load): 99,544 bytes free in play (97 KB) against the test floor of 96 KB (98,304: a margin of 1,240 bytes), and the sets of missions 5 and 6 need more (slice A alone 5,616; all five slices about 22-26 KB). **The new sets live in a block of their own** (`disk/world2x.bin`, `WORLD2X_*` in `world2_art.h`, loaded into `s_pW2X`, which `worldLoad(1)` allocates and `worldLoad` frees again for any other world): growing the art block at boot instead took the title and world 1 from 105 KB to 99 KB, and the splash is still there while the boot-time floor is measured. The plan's levers are each smaller than that, pad widths by skill cannot help skill 1 (all four widths are in use) and would need
    a new composition when Ctrl+L changes the skill, and a mission's own set needs the art block allocated again at every card. The lever used is one the code already held: **the boot splash's 47 KB** (the DPS logo's 37,456 bytes and its two copper lists, loaded for the boot fade) stayed allocated for the whole game (`main.c`: "until the game quits"). They are now freed **at the title, right after the splash** (v1.40, `genericProcess`: `AGK splash freed`, one OS trip behind a black screen). This branch first freed them when world 2 first loaded (`worldLoad(1)`: the display is off, a floppy load runs there with the OS on anyway); that free was removed when the branches were merged (v1.41: the free at the title is the same memory earlier, so world 1 gets it too), never needed again (the splash plays once, at boot). **Measured: world 2's chip free in play 99,544 -> 147,384 bytes (97 -> 143 KB, +47,840) with the splash freed, and 141,768 (138 KB) with slice A's radar in its block (-5,616)**; world 1 and the title are unchanged (105 KB); no frame cost (the load frame is skipped by the perf meter as before).
    No floor is lowered: the gates keep 96 (`tests/memory.agk` and the world 2 scenarios) and 64 with the squeeze (`tests/chipwinuae.agk`); `tests/w2chip.agk` holds the new figure (at least 130 KB at the world's load) so a later slice cannot spend it unnoticed. If a later slice does not fit even then, the build stops there and Paul chooses (the options with numbers:
    pad widths by skill -3,040 at skill 3 and -4,144 at skills 4-5; the title logo and its copper lists freed in play, 26 KB, reloaded from the floppy on return to the title; a mission's own set).
    *Native layer* (listed, not built): a new BOB and its templates (`TPL_MAX`), a pen 5 bar on the HUD and a line on the card's map need nothing from the chipset model beyond what the world 2 BOBs already use; the new data is a second floppy file, `world2x.bin` (the radar frames, 5,616 bytes), and the HUD's pen 5 bar and the card's two map rows are plain 3-plane drawing; no new input. Nothing for the kit until slice B's gust (a sideways force: logic only) and slice C's winch (hover box: logic only).
    **Slice B: wind gusts and the windsock** (v1.21; world 2 missions 5 and 6 only; the numbers are the plan's, CONTENT-PLAN 4.4, the fairness rules are the studio's, pending Paul's playtest):
    - **A gust** pushes the chopper sideways, toward +x (the base side: the sock art points right), 0.25 px a frame (`GUST_PUSH` 4 sixteenths, its own sub-pixel accumulator, not through `vx`: stick, landing tolerance and the pad guide are untouched) for `GUST_FRAMES` 150 (3 s): at most 37.5 px a gust, bounded. A gust is the only wind: one at a time.
    - **Tell** (`GUST_TELL_FRAMES` 50, 12 frames is the floor): the windsocks whip (frame limp / full every 4 frames) for the 50 frames before the push, then stay full while it blows. Serial `AGK gust tell`, `AGK gust start`. **No new sound** (the plan's rising howl needs a sample that is not drawn: deliberately none, and no existing effect reused: an alarm-like effect would read as a weapon, see the rotor v4 notes); the sock is the tell, as in the plan's "windsock turns".
    - **Timing:** the next gust's tell begins `GUST_GAP_MIN` .. `GUST_GAP_MAX` (600 .. 1000 frames, 12-20 s, from the game's LFSR) after the last gust ended, the first one after the mission's start: never before `GUST_FIRST` (900 frames, 18 s of play, plus a gap's spread of 0-400) into a mission, so the card and take-off are quiet; the wait counts in play, landed or not. A gust (tell or push) **never starts while the chopper is landed or inside the 2 s take-off grace** (`airGrace`), and a landing during the tell or the push cancels what is left of it (a landed chopper is never pushed); it then waits its next gap.
    - **By skill** (`logicGustsOn`): none on skill 1; skill 2-3 (and GODMODE) not while the chopper is within 100 px of a barracks (`GUST_BARRACKS_CLEAR`: neither the tell nor the push starts there, and a push in progress is held (no sideways movement, its time still runs) while it is there); skill 4-5 anywhere (barracks included). Mission 4 never has wind.
    - **Windsocks** (`BOB_SOCK`, `art/world2/windsock.txt`, 10 x 20, 2 frames: limp, full): one on each island's pad side, `SOCK_DX` (60) px right of the pad's centre (clear of a 96 px pad's edge), standing on the ground, in missions 5 and 6 only, not on the mission card (its first picture is already the heaviest of the game: 96 % on the a500, 101 % with a sock, measured). 480 bytes of chip in the slice 2 block (`world2x.bin`), `TPL_MAX` 62 -> 64. Measured: world 2 chip free on the a500 141,768 -> 141,288 bytes (-480).
    - Tests: `testW2Gust*` (timing range and first-gust floor, the tell lead, push size and direction, skill table, barracks rule, landed / take-off grace, one at a time, mission 4 and world 1 untouched, the sock drawn and whipping), scenario `tests/w2wind.agk`.
    **Slice C: the ship-deck winch rescue** (v1.22; mission 5 only; art from `w2-art-cd` a68fa57, `art/world2/CD-ENTRIES.md`; the numbers are the plan's, CONTENT-PLAN 4.4 "Winch rescue", the exposure rules are the studio's, pending Paul's playtest):
    - **The site.** Mission 5 is "3 barracks + 1 ship deck" (CONTENT-PLAN section 3): the third island's barracks (slot 2, x 2080) is replaced by a moored cargo ship in the sea gap between the second and third island, hull 124 x 30 (`BOB_DECK`, 2 frames: the bow wake, alternating every 14 frames; one facing; it never moves), its left edge at x 1730 (`DECK_X`), the waterline on `BOAT_BOTTOM_Y`, the deck's flat edge at y 227 (`DECK_Y`), the sea gap's gunboat slot 2 (x 1690-1890) left empty in mission 5 so the ship is not sailed through. It is inside the first radar site's zone (1008-2208): shooting that site quiets the stretch (the jets), the deck is the plan's "island-less site". 16 people wait on it (hostage slots 32-47, state INSIDE, three of them drawn waving at x 28 / 76 / 82 of the hull while they last). Mission 4 and 6 have no deck (mission 6 keeps its interim until slice D).
    - **The hover box** is 24 x 20 px: x 1777 .. 1800 (hull x 47-70), the skids' y 207 .. 227 (`DECK_BOX_*`). The deck is a floor you cannot land on: over the hull the skids stop at y 227 (the barracks-roof rule: not ground, no boarding), so the box's bottom is the deck. Two lamps (the pad lamp, `BOB_PAD_LAMP`) stand at hull x 42 and 71, y 222: yellow while the chopper is in the box, red while not, drawn once it is within 150 px.
    - **The winch** (`g->winch`): the chopper alive in the box, 12 frames of hover sets the rope (it drops 24 px over 12 frames, hook shown the first 4 frames, ladder swaying after); while it stays in the box one person climbs the rope every `DECK_PERSON_FRAMES` (50: 8 people in 8 s on normal, 16 in 16 s; the same on every skill, the plan only speeds it with the winch upgrade, not built) and boards (`EV_BOARD`, the existing sound, `aboard` + 1, `AGK winch board=N`). Out of the box the climb is **paused, not reset** (the person's progress and the count are kept; the rope is drawn up at once); it resumes in the box. A full cabin (`CABIN_MAX` 16) or an empty deck ends it. The people count for the mission and the unloading like any others (score 100 each at the hangar, 10 a person at the clear); no extra score for the deck (it has no garrison: no zone clear).
    - **Exposure rules** (the plan: "a winch hover is a landed-class state for R1 and R3"): `g->winch` counts as landed (the jets and boats are run with `chop.landed` set for that picture, so world 1's code is untouched and costs nothing) for the ground grace R1 (boats and jets: never on skill 1-2, 6 / 4 / 3 s of hovering on skill 3 / 4 / 5 before a shell or a bomb may be aimed at it; `landFrames` counts the hover), R2 (leaving the winch starts the 2 s take-off grace) and R3 (one attack at a time, the shared token); a jet's tell cannot end in a missile at a winching chopper (it is a bomb target, as a landed one). **No gust** starts or blows while it winches (landed-class). Jets can still start in a live radar zone (radar rules unchanged). Skill 1-2: a hovering chopper is never aimed at (as when landed).
    - **HUD and card:** the deck's slot is the HUD radar's third barracks dot at the deck's hover x (the slot's barracks is "open": the dot is red); the card's strip map draws a small ship (blue hull, cream house) in place of the third hut at the deck's x (the map is made for every mission of the world at the world's load: the whole image now, three of 960 bytes, `routeCreate`, `logicRouteMapFor`; the card shows its mission's, `routeShow`). The HUD radar's barracks dots and sites are redrawn six pictures into a card, not in its first (the barracks of a new mission are closed again: with the deck's slot open in mission 5 that redraw made the mission switch picture 100 % on the a500 against 97 %).
    - **Chip** (the real converter sizes, `world2x.bin`, rows x frames x ceil(w/16) x 2 x 6): deck 5,760, rope 1,440, hook 144, climbing man 288 = **7,632 bytes**; world 2 on the a500 141,288 -> 133,656 B (130.5 KB free). `tests/w2chip.agk` lowered from 130 KB to **128 KB** (measured minus 2 KB, so slice D's freighter (13,824 + 864 by the same sums) is expected to take it below that: it will be lowered or the sets loaded per mission then). Floors 96 / 64 not touched.
    - Tests: `testW2Deck*` (placement, the box, the rope, the climb rate and pause, full cabin, R1-R3 and the gust, floor, draw, map), scenario `tests/w2deck.agk`.
    **Slice D: the sinking freighter** (v1.23; mission 6 only; art `art/world2/freighter.txt`, `sink_spray.txt`, geometry in `art/world2/CD-ENTRIES.md`; the plan fixes the timer numbers, CONTENT-PLAN 4.4 "Timers on rescue sites", the rest is the studio's, pending Paul's playtest):
    - **The site.** Mission 6 is "3 barracks + 1 sinking freighter (16, timed)" (CONTENT-PLAN section 3): the third island's barracks (slot 2) is replaced by a bulk freighter, 128 x 36 (`BOB_FRTR`, 4 frames = the sinking stages), its left edge at x 1728 (`FRTR_X`: the same sea gap as mission 5's ship, 1664-1920, the gap's gunboat left out in mission 6 too; the radar site at 1608 covers it), its waterline row at y 246, 16 people aboard (slots 32-47; three drawn waving, x 86 / 92 / 98 of the hull, aft of the hold). **One timed site in mission 6** (the plan allows two a mission, never in world 1; the destroyer, slice E, is a set piece, not a timed site).
    - **The timer** starts when play starts and runs while the game is in play (not on the card or while the chopper is down), `SINK_SECONDS`: **120 / 105 / 90 / 75 / 60 s on skill 1-5 (90 for GODMODE)**, the plan's table; it is fixed when the mission starts. Each sinking stage is a quarter of it (`frtrStage`: 0 level, 1 bow down 2 px, 2 five px with a puff of steam, 3 nearly awash with three: the stage is the BOB frame; stage 1-3 also draw the spray, `BOB_SPRAY` frame stage - 1 at the bow). A stage change prints `AGK sink stage=N`. **At the end it sinks:** the people still aboard are **lost** (`killed` goes up by their number, so the mission's `saved + killed` still reaches 64 and the clear and the rank count them as dead: the plan says "Losing a site only loses the people still there"; no score is taken), the winch ends, a boom marks it, the hull is gone, `AGK sunk lost=N`. People already in the cabin are safe. Once everyone is off (or lost) the timer and its bar stop.
    - **The hover box** is as slice C's (24 x 20 px, hold coaming x 52-75 of the hull, climb 1 person per 50 frames, paused not reset out of the box, 12 frames of hook then rope, the same lamps (x 47 / 76) and rules) but **follows the tilt**: the skids' floor (the coaming's top over the box) is y 230 / 230 / 230 / 229 for stages 0-3 (the art's coaming rows 17 / 17 / 16-18 / 15-18, `FRTR_FLOOR`), the box is the 20 px above it, and over the hull the skids stop on that floor (not a landing). A box that drifts down with the stage never closes on a chopper that is in it (it is at most 1 px).
    - **Exposure rules:** exactly slice C's (landed-class for R1-R3 through the winch, no gusts during a winch, jets in a live radar zone as before); the timer is the new pressure, nothing shoots harder. Skill 1-2 are never aimed at while winching, and have the longest timers.
    - **HUD and card:** the freighter's slot is the HUD radar's third barracks dot (red) at the box's x; **a red bar (pen 5) on the two rows under the radar (HUD rows 22-23, x 64 .. 255) shrinks with the time left** (192 px = the whole timer) while people remain aboard. The card's second line says what is out there: mission 5 "48 ON 3 ISLANDS, 16 ON A SHIP", mission 6 "FREIGHTER SINKS IN 90 SECONDS" (the seconds of the skill in force at the card); the strip map shows the ship at the freighter's x as in mission 5.
    - **Chip** (the converter's real sizes): freighter 13,824 + spray 864 = **14,688 bytes** added to `world2x.bin` (now 28,416 B); world 2 on the a500 133,656 -> 118,968 B (116 KB free, floors 96 / 64 untouched, margin 20 KB over 96). **The per-mission lever was measured and NOT taken:** mission 6 does not need the deck set (5,760 of it) and mission 5 does not need the freighter set (14,688), but loading a mission's set at its card costs a floppy read and a template re-crop in the mission-switch picture, already 97 % on the a500; the chip it would give (14.7 KB in mission 5, 5.8 KB in mission 6) is not needed against 96 KB. If slice E's destroyer art (8,184) and anything later leave less than 12 KB of margin over 96, load per mission then. `tests/w2chip.agk` floor lowered 128 -> **114 KB** (measured 116 minus 2), said here.
    - **A real mission 5 -> 6 transition** is now played (`tests/w2clear.agk`, the test hook Ctrl+L, K "all rescued": mission clear, the card): the picture that starts a mission after a clear has no dropped frame.
    - Tests: `testW2Freighter*` (timer by skill, quarter stages, box by stage, winch at every stage, the sinking loses exactly the people left and nobody else, the mission still completes, nothing after), `tests/w2frtr.agk`, `tests/w2clear.agk`.
    **Slice E: the strait destroyer and the zone bonus** (v1.24; mission 6 only for the destroyer; art `art/world2/hull_list.txt`, `destroyer_turret.txt` (left frames), `turret_hit.txt`, `aa_gun.txt`, `barrage_tell.txt`, `whitecap.txt`, geometry in `art/world2/E-ENTRIES.md`; the plan fixes the HP, the tell table, the 40 s barrage and the 90 s opening (CONTENT-PLAN 4.4 "Destroyer", 3), the rest is the studio's, pending Paul's playtest). Decided by Paul, 2026-11-24: the hull edit is accepted (frame 0 of `hull_list.txt`, the old painted forward AA mount cleared); ONE FACING (bow left; the strait is crossed one way); the list frame is kept; the AA guns have no destroyed state and no HP.
    - **The site.** One destroyer lies in the sea between the third island (ends x 2400) and the fourth (starts 2944): hull left edge `DEST_X` 2560, 154 x 44, waterline row on `BOAT_BOTTOM_Y` (top y `DEST_TOP` 205), so it spans x 2560-2713 inside the jet-free strait of slice A (2208-2816) and across the route to the freighter, the first and the second island (every trip there crosses it). It does not move (one facing is drawn, and a patrol would need the other): "slow, patrols the strait" is a station-keeping ship, the set piece. Mission 6's two gunboat slots in the strait (x 2440-2680 and 2680-2910) are left empty so a gunboat is not sailed through the hull; the rest of mission 6's boats are as before. Mission 4, mission 5 and world 1 have no destroyer, no new event, no new cost.
    - **Two turrets and four AA guns.** Turrets (`BOB_TURRET` 32 x 19, mounts forward hull x -7 and aft x 122, y 216): 6 HP each (`DEST_HP`); a cannon hit is 1, a bomb exploding within 30 px of the housing is 3; they pay 150 each. A turret at 2 HP or less is **smoking** (`BOB_THIT` frame 0), at 0 it is **dead** (frame 1, charred, it never fires again). Four AA guns (`BOB_AAGUN` 12 x 12, mounts hull (27, 12), (42, 17), (75, 6), (99, 4)) have no HP and cannot be shot: they are fired at the air only and go quiet with the ship (below).
    - **Turret shells** go under the shared ground-threat rules, exactly as a gunboat's: a turret may take the chopper as its target when the chopper is low (skids below y 150), within `BOAT_RANGE` 220 px of the housing and not closer than `BOAT_MIN_RANGE` 56 (R7, R8), the turret whole on screen, landed only after the ground dwell (R1: never on skill 1-2, 6 / 4 / 3 s on skill 3 / 4 / 5; GODMODE as skill 3) and not inside the take-off grace (R2); a turret starts its tell only when the shared attack token is free or is its own (R3, `atkWait` / `atkLast`, 3 s between two attackers, jets and boats included). **Tell** `logicDestTell`: **28 / 24 / 20 / 18 / 16 frames by skill 1-5** (GODMODE as normal, 20; the floor is 12): the turret turns to the chopper, braced, its barrel rising (`TURRET_TELL1` / `TURRET_TELL2` blinking every 4 frames); the shell leaves the muzzle (forward turret hull (-7, 23), aft (122, 23)) at 1.5 px a frame, aimed at the chopper's position as the gunboat's, with a muzzle flash (`TURRET_FIRE`, 8 frames) and the gunboat's shot sound; unless the chopper got away meanwhile (no shell: nothing to read it from). Each turret fires at most every `logicDestGap` frames, tell included: 170 / 140 / 120 / 100 / 85 (120 on GODMODE), plus 0-31 random, slower than a gunboat's because a turret's shell is the set piece's own weapon; a dead turret never fires.
    - **AA guns (flak).** Air only (CONTENT-PLAN 4.3: "cannot depress; does not fire at a chopper below 24 px above ground"): a gun may take the chopper when it is **above y 212** (more than 24 px above the sea), not landed and not in the take-off grace, within `AA_RANGE` 180 px (less than the cannon's 216, so a turret can be shot from outside the guns) and not closer than 56 px (a chopper right above a gun is safe from it), the gun on screen. **Tell** `logicAaTell` **26 / 22 / 18 / 15 / 12 frames** from the table (GODMODE 18): the barrel up and its tip red (`AA_TELL`); the last `min(tell, 14)` frames show the **ranging puff** (a smoke puff, `BOB_SMOKE`, on the aim point, fixed where the chopper is when it appears, and the gun's muzzle flash `AA_PUFF` for 5 frames), then the **burst** (`BOB_BOOM`, radius 18 px: the chopper's box 26 x 13 against a 36 x 36 box; a hit is the shells': first hit damages, the second destroys, `chopperDestroy`). A gun rests `logicAaGap` frames after its burst (**120 / 100 / 85 / 70 / 60**, plus 0-15, the plan's table; skill 1 120), no two guns start a tell within 40 frames of each other, and at most `logicAaLive` flak shots (a gun in its barrel phase or a puff) at once: 1 on skill 1-3, 2 on skill 4-5. AA is not under R1-R3, which protect a landed or low chopper (it cannot hit one); it has these limits instead.
    - **The disabled ship.** When both turrets are dead the destroyer is disabled: +100 points (a turret 150 each, the set piece pays 400 in all, the plan's score), the hull draws as `HULL_LIST` (frame 1 of the list art: the bow sinks 4 px, the stern rises 2, black smoke from both funnels; the turret and AA BOBs sit at y offsets +3 forward turret, -1 aft turret, AA +3 / +2 / +1 / 0), both turrets in the dead frame, and the AA guns, the barrage and every attack stop (the way is open: the plan's "disable both turrets to open the way"); flak in the air is dropped. `AGK destroyer down`. The HUD's left slot says **DEST DWN** for 3 s (8 frames after the kill).
    - **Anti-stall (the plan's 40 s and 90 s).** `destClock` counts the frames the chopper is alive and within `DEST_ENGAGE` 360 px of the hull's centre (x 2637) in play; time spent elsewhere does not count and the clock is kept over a lost chopper and the trips of the mission. At **2000 frames (40 s)** and then every **1500 frames (30 s)** while the ship is not disabled the destroyer fires a **barrage**: `BARRAGE_TELL_FRAMES` **30 frames** of tell (ranging markers, `BOB_BARRAGE` 24 x 12, frames 0 / 1 / 2 for tell frames 0-9 / 10-19 / 20-29: a faint ring, the ring opening with red ticks, the full double ring with a flash), three markers at the chopper's x at the start, -36, 0 and +36 px, only where the x is over water (a marker is never put on an island), top y 232; after the tell the markers burst one after the other, `BARRAGE_STAGGER` 4 frames apart (a salvo of 8 frames: each a boom and a splash, `BARRAGE_HALF` 14 px each side, from y 150 down to the sea; a marker is gone once it has burst): a chopper whose box touches one with its skids below y 150 is hit as by a shell (it can fly out of the spread in half a second, or fly above y 150, or land). A barrage does not start while the chopper is landed or in the take-off grace (R1, R2), and it takes the shared attack token for 3 s so a turret does not start a tell in the same moment (a tell already running finishes). At **4500 frames (90 s)** the turrets are **open**: each living turret is cut to 2 HP (smoking, a cannon hit twice or one bomb), they go on firing, and a serial line says so (`AGK destroyer open`). A stalling player is therefore paid back at 40 s, and at 90 s the fight can only be won.
    - **Zone bonus.** In a mission with radar sites (5 and 6) the moment the **last standing site** goes down is a zone clear: **+100 points** once a mission (CONTENT-PLAN 10 q15: "zone-clear bonus 100"), `AGK zone clear`, the HUD's left slot says **ZONE CLR** for 3 s (150 frames, starting 8 frames after the event: the quota line's slot, `quotaKind` picks the text); no medal (there is no medal system yet) and the jets' zones are already gone with the sites (slice A).
    - **Whitecaps (Typhoon only).** `BOB_CAP` 34 x 10, 3 frames: storm foam on the sea of mission 6 at fixed world x over open water (`s_pCapX`), at most two on screen, the frame cycling every 16 frames (0, 1, 2, 1), behind the boats, none in the destroyer's own stretch of the strait: the mood of the storm and a landmark for the strait, not a hazard. The bow spray (`bow_spray.txt`) is not used: nothing here moves.
    - **Pop-in, the whole hull and its pieces.** The BOB blitter does not clip: a BOB is drawn only whole inside the buffer's margins (48 px left of the screen, 48 px right of it), so a 154 px hull would appear and vanish with 106 px still showing. The hull (`BOB_DHULL`, 154 x 44, 2 frames, **no mask**: it is drawn before anything that could be under it, as a plain copy onto the empty buffer, half the blitter words of a cookie cut) is one BOB while its left edge is on the screen between x -48 and 214 (`DEST_WHOLE_MIN`, `DEST_WHOLE_MAX`, asserted against the buffer in `main.c`); beyond that four 48 px pieces (`BOB_DHULL0-3`, 48 / 48 / 48 / 10 px, **views of the same bitmap's columns**, no chip of their own; the bitmap has a zero word after each plane's row for the blitter's extra shift word) are drawn right to left, so that each piece's extra word is the next piece's first, complete: they appear and vanish off screen. `testW2HullIsNeverCutByTheBuffersMargins` walks the hull across the screen and checks every visible column is in a BOB the blitter will draw. (The ship deck and the freighter of slices C and D, 124 and 128 px, still pop in with about 80 px showing: a finding, not changed here: the same views would fix them.)
    - **Memory: the per-mission set (the SPEC v1.23 lever, taken).** Slice E's art as drawn is 15,672 bytes; the hull needs no mask (5,280 less) and the turret is one facing (1,824 of 3,648 kept), so it is **10,928 bytes** in the file (hull 5,808 with its zero word, turret 1,824, turret hit 912, AA 432, barrage 864, whitecaps 1,080), which would still have left 4.6 KB over the 96 KB floor with everything in one block, under the 12 KB the lever was to be taken at. `world2x.bin` is split: the **common set** (radar 5,616, windsock 480, rope 1,440, hook 144, climbing man 288 = 7,968 bytes) is in the block `s_pW2X` allocated at the world's load as before, and a mission's **own part** is a file of its own in a second block `s_pW2M` made when the mission's card starts and freed at the next: `world2x5.bin` (mission 5: the ship deck 5,760) and `world2x6.bin` (mission 6: the freighter and its spray 14,688 and the destroyer's set 10,928 = 25,616). `genericProcess` loads it right after the logic has switched mission and before the picture draws (`missionSetLoad`: the bitplanes off, the same two pictures blank as a world's load, the templates made again over the same slots and cropped, the lists rewritten; world 2's first load does it inside `worldLoad`), so the picture that starts a mission is not slower; mission 4 holds none (its block is freed). `tests/w2chip.agk` plays mission 4 -> 5 -> 6 -> 5 and asserts the chip free after each. **Measured on the a500 (`AGK w2 chipbytes`):** mission 4 139,416 (136 KB), mission 5 133,656 (130 KB, as slice C measured), **mission 6 113,800 (111 KB): 15,496 bytes over the 96 KB floor (4,6 KB without the lever)**; floors 96 / 64 untouched; `tests/w2chip.agk` floors are measured minus 2 KB (134 / 128 / 109 KB), said here: they guard this work, they are not the game's floor. `TPL_MAX` 80 -> 104 (mission 6's 32 template frames over mission 5's 2; +1 KB of program data). The world's load is a little longer (one more file): the scenarios that jump straight to mission 6 wait up to 1500 frames for it.
    - **Frame (vAmiga, a500, the game's meter).** The set piece is the heaviest scene of the game for the blitter: the hull alone (whole, plain copy, erase and draw) is about a third of the picture's blitter words. Measured, `tests/w2dest.agk` (a fight at the strait on skill 1: tells, shells, flak, bursts, two kills): average 62-76 %, worst 96 %, no dropped frame; `tests/w2barrage.agk` (the barrage, the opening, the zone bonus): average 64-78 %, worst 97 % (the picture after the position jump of the test hook), none dropped; mission 6's flight past it (`w2frtr`) worst 97 %. What keeps it there: the hull as one mask-less copy (the first version, four masked pieces, dropped 76 frames of 450), no whitecaps in the strait, the barrage's three bursts staggered 4 frames apart (three explosions on one picture dropped a frame), the bonus line starting 8 frames after the kill, no extra explosion for the disabled ship. The limits asserted are 98 (the scenarios' own; the freighter's flight crosses the strait too). `tests/jets.agk` (world 1, skill 5 crash, limit 97) is layout sensitive: this slice's first main.c put it at 98 with one dropped frame, with no extra instruction on its path; it passes again with the mission-set check moved after `logicUpdate` and the bonus text carried by the quota line's slot.
    - **Native layer:** the new data is two more floppy files (`world2x5.bin.pk`, `world2x6.bin.pk`) and a mission switch that frees and loads a block; the rules are plain C; no new input, no new sound (the shell, the flak burst and the barrage use the gunboat's shot and the explosion).
    - Tests: `testW2Destroyer*` (placement, the tell table and the shell's tell, the ground-threat rules, hit points and states, the disabled ship, the AA's air-only rule, tell and burst, the barrage's clock, tell, markers and blast, the 90 s opening, the draw list, the zone bonus), scenarios `tests/w2dest.agk` (a fight at the strait on skill 1), `tests/w2barrage.agk` (the barrage, the opening, the zone bonus) and `tests/w2chip.agk`; test hooks (Ctrl+L, then; wait a second between two): D to the destroyer, B the barrage in 2 s, Y the opening in 2 s, R every radar site down.
    **Winch and loading polish** (v1.44; branch `w2-freighter-ux`; Paul's playtest of the merged build, 2026-11-25: positioning the chopper over the sinking freighter and the mission 5 ship deck is "a little klugey", and while people load from the freighter "the group neither decrements nor moves". Studio proposal, pending Paul's playtest; slices C and D above are changed only where this block says so; the rope, the 50 frame climb (one person a second), the exposure rules, the timer and the chip are exactly as they were):
    - **Why positioning was awkward (found in the code, not guessed).** (1) The box was 24 x 20 px and the hover limit 0.5 px/frame on every skill, while the chopper's drag is 1/16 px/frame per frame: from cruise (3 px/frame) a stop takes 48 frames and about 70 px of coasting, so a stop inside a 24 px window needs a reverse at the right moment, on skill 1 as on skill 5. (2) A frame out of the box (a 1 px drift, the last of a gust's 0.25 px/frame) pulled the rope up at once and the next hover started the 12 frame drop again: the hover had to be unbroken. (3) The two lamps lied: they turned yellow when the chopper was in the box by position alone, whatever its speed, so the usual state on the way in (in the box, too fast) read "ready" and nothing happened; they also blinked out a quarter of the time. (4) The box was only 20 px tall, so a chopper coming down from cruise altitude had to arrive in the band slowly as well. (The wind was already handled: the gust never starts within 100 px of the site's slot on skill 2-3, none blows while it winches, none on skill 1.)
    - **The winch window by skill** (skill 5 is slice C's numbers, unchanged: "keep skill 5 demanding"; GODMODE plays as 3; `g_pWinchGrow`, `g_pWinchUp`, `g_pWinchTol`, `g_pWinchGrace`, `g_pWinchCatch`, in the difficulty tables test):

      | skill | box width (px) | box height above the floor (px) | hover speed limit (px/frame) | rope kept this many frames out of the box | capture ring (px) | settle |
      |---|---|---|---|---|---|---|
      | 1 | 56 | 36 | 1.5 | 24 | 12 | yes |
      | 2 | 48 | 32 | 1.25 | 18 | 12 | yes |
      | 3 | 40 | 28 | 1.0 | 12 | 8 | yes |
      | 4 | 32 | 24 | 0.75 | 6 | 0 | no |
      | 5 | 24 | 20 | 0.5 | 0 | 0 | no |

      The box stays centred on slice C's box (the radar dot does not move), so it only grows. **Grace:** out of the box (or too fast) the climb is paused as before, but the rope stays down for the grace frames and the hover counts as winching; a person already climbing is not lost; at the end of the grace the rope is drawn up and the hover starts again (12 frames), as before. A full cabin, an empty deck, a dead chopper or a sunk ship end it at once (no grace). **Capture ring (skill 1-3):** within the ring beside the box, at no more than twice the speed limit and inside the box's height, the chopper is nudged into the box by 0.5 px/frame (the pad guide's nudge; inside the box nothing pulls). **Settle (skill 1-3):** once the rope is out the sideways speed loses 1/16 px/frame more than the drag, so a released stick stops in half the time; it is less than the stick's push (2), so it never holds the chopper in place. Skill 4-5: no pull, no settle.
    - **The lamps say what the winch does.** Two lamps at the ends of the (now wider) box, shown within 150 px as before: **red, blinking** (a quarter off) outside the box; **yellow, blinking fast** (4 frames on, 4 off) in the box but too fast or not yet low enough: "slow down"; **yellow, steady** while the winch holds (the hook is falling or the rope is down). No new art: the two existing lamp frames and three blink patterns.
    - **Loading is visible, a person at a time.** The group on deck is drawn as up to `DECK_GROUP_MAX` (3) people waving, in fixed slots aft on the deck (the BOB's left x on the hull: ship 96, 88, 80; freighter 98, 92, 86; the first the far end, the last the end nearest the rope, in front of the deckhouse), standing for all the people waiting: `ceil(waiting * 3 / 16)` of them, so the group visibly thins out as the count falls (16 to 11 waiting: 3, 10-6: 2, 5-1: 1, none: 0), and each person in turn steps out and runs. (Five were built first and dropped frames on the a500, worst 100 % against 88 %: each BOB on this screen costs about 4 % of a frame; three is what the slice C scene already drew.) A person's 50 frames: **the first 22 (`DECK_WALK_FRAMES`) he runs out of the group to the rope's foot** (the run frames, from the slot the group lost, or the near end while the group is full, towards the chopper's x as it is), **the other 28 he climbs the rope** (the climbing frames, from the deck up to the belly), and he comes aboard at the 50th (`EV_BOARD`: **`aboard` + 1, the HUD's ABOARD number goes up by one, `SOUND_SFX_BOARD`**: the existing effect, once per person, played like every effect by `soundPlay`; no new sample, no new channel use, nothing on channel 2 beyond what section 9's rotor claim already allows). The person running or climbing is not in the group (`waiting = deckLeft - 1` while the rope is out). Paused (out of the box past the grace) he stands in the group again and starts his run when the rope is out. The rate is unchanged (one person every `DECK_PERSON_FRAMES`, 16 in 16 s); the loading now shows it.
    - **Cost.** Chip: none (the hostage and climbing frames; no new BOB). Per frame: the same BOB count as slice C (3 people, one of them the runner or the climber; the steady lamp is shown in the quarter of the frames the red blink hides, no more at once than before); the frame budget is checked in `tests/w2deck.agk` and `tests/w2frtr.agk`. Floors 96 / 64 not touched. **Measured (a500, game meter, none dropped):** `w2frtr` worst 98, `w2radar` 96, `w2play` 84, `w2deckslop` 85, `jets` 92; **`w2deck` (skill 1) worst 97 against its own limit of 90 (88 before; it fails on a500 and a500-ks31, passes on aros and a1200): the limit was not loosened, the cause is open** (the BOB count is the same; with the old tables and the new drawing it is 89, with the old speed limit alone 92: the worst frame moves with when the rope starts, but which frame it is has not been found; needs a per-frame trace).
    - **Tests (red first):** unit `testW2WinchTablesBySkill`, `testW2WinchBoxWidthBySkill`, `testW2WinchSpeedLimitBySkill`, `testW2WinchGraceKeepsTheRope`, `testW2WinchCaptureRing`, `testW2WinchSettles`, `testW2DeckLampsSayWhatTheWinchDoes`, `testW2DeckGroupThinsOut`, `testW2DeckPersonRunsToTheRopeThenClimbs`, `testW2DeckLoadingCountsOnePerPerson` (ship and freighter); scenarios `tests/w2deck.agk` (a slow-but-not-0.5 approach that used to fail) and `tests/w2frtr.agk`.
  - **The program**: the unused ACE code was in the executable (the tile and scroll buffers, pak files, chunky and fix-point maths: 77 KB);
    `-ffunction-sections -fdata-sections` and `-Wl,--gc-sections` (CMakeLists.txt, `-D AGK_GC_SECTIONS=OFF` for `agk profile`, which maps code by
    object file) drop it: 325,364 -> 248,464 bytes with all of this gameplay in it (332,536 without the link change). AROS's first picture
    1976 (2347): `tests/memory.agk` asserted 2100 at this slice; raised 2100 -> 2300 on 2026-11-24 (coordinator, pending Paul's veto): the merged build grew ~14 KB (world 3 ~9 KB, sea ~3 KB) and AROS loads ~11 frames per KB; main 7fc7e09 measured 2014, the merged build 2179; the harness hard limit is ~2900-3000. Prove It: AROS boot time is a test-bench number, not a player-facing limit on real Kickstart. The new setup code is compiled for size (`COLD`, `-Os`: `bobsBind`, `padCompose`, the
    routes, `boatsPlace`), and the pad and boat functions called every frame are small, not inlined.
  - **Frame** (`tests/w2play.agk`, asserted: no dropped frames, worst 85; the game's meter, a500): the walk to a pad and back worst **81 %**
    (boarding 73 %, unloading 81 %), flight over islands and boats `tests/world2.agk` worst 63 % (card window 63 %: the same as before this slice,
    the strip map made at the load), average 37-53 %; the game's world-2 numbers grew by the objects (a barracks 3 %, a pad 5 %, a boat 2 %).
    World 1 (`tests/perf.agk`): worst 68 % (67 before; the world and boat tests in the hot paths).
- **Sprites:** 0-3 the chopper (32 px, 15 colours, attached pairs, a pose per frame: its 32 frames sit in
  chip once, in the floppy art's block; sprite DMA reads 4 blocks of 352 bytes, one per copper buffer and
  rotor phase, which one queued blit refills only when that buffer's pose changes); 4-5 weather
  (13 items stacked down each channel's DMA list, slid by the CPU); 6 the far sky's wisps or stars
  (behind both playfields, priority code `0x1B`); 7 lost to the fine-scroll fetch.
- **BOBs:** 13 types, `DRAW_MAX` 96 a frame. Limits: 3 player shots, 6 enemy shots, 5 tanks, 2 jets,
  3 mines, 6 explosions.
- **Frame budget (A500):** `tests/perf.agk` (a whole trip: 16 boarding, 16 running into the hangar)
  asserts `expect-max-load 83`; the heaviest picture measures 80% on a500 and a500-ks31 (the game's
  own perf counter; `agk profile` 81%, average 35%), about 20% short of a dropped frame. It was 82%
  at v1.2 and 85% (profile 86%) after the deep rotor and jet fairness merged; the perf trim won it
  back without changing what is played or heard: last picture's BOBs are erased in the top border
  while the HUD types, the HUD's numbers take one DIVU a digit, and the pacing sample never shares
  a picture with the status line. No dropped frames. Target (average <= 70%, worst < 100%): met.
  v1.9 (the frame-budget pass, measured on a500 and a500-ks31): `tests/perf.agk` 81% -> 70%; the skill-5
  loss among explosions (`tests/jets.agk`, now asserted: no dropped frames, `expect-max-load 97`) 180% with
  11 dropped -> 96% at the crash's first two pictures, 82% before it, the jet's warning 98% -> 57%;
  `tests/hiscore.agk` (asserted, 90) 135% -> 83%. In the 6-plane lines the bus is full and the program
  runs from slow RAM, so CPU instructions and blitter words share one budget: the per-picture BOB loop is
  68000 assembly, the message and HUD text draw only the letters that change, sounds and the serial report
  run after the swap, the wreck's sprite refill is half. A picture waits for the vertical blank after its
  swap (review A3), so an overrun costs a frame, never a torn one. The ~90% target at the crash is not met.
  **v1.40 (the headroom pass, no design change, every golden identical):** `jets` (skill-5 crash, limit 97) had flipped to 98 with a dropped frame four times on unrelated code; it was layout noise on a 97% frame. Real savings (CPU in the 6-plane lines is ~27 bus cycles a line, so instructions are the budget): the people's erase areas merge into one clear (`erasesMerge`/`erasesMergeAsm`, 4 or more people, only below the message rows), the lost chopper's text types from a per-line plan (`msgTypeFast`; LOST/CLEAR/OVER/VICTORY, not the hall of fame, whose 7 planned lines cost it 10 points), `msgCoverAll` stops at the last person, the cover/fix bookkeeping visits only flagged lines, `textType` stops scanning once the budget is spent, `chopperDestroy` stops at the last person aboard. Worst frame, a500 (a500-ks31): jets 97 -> 92 (92); hiscore 85 -> 83 (84); after-numbers also hiscoreesc 83 (84), hiscorekeys 83 (88), perf 65 (66), w2play 82 (83); aros jets 74, a1200 40. Before-numbers other than jets and a500 hiscore were not captured. `tests/jetsload.agk` guards the crash at 94 and checks the typed text against shots recorded from the old build. `jets` keeps 97. Music/sfx interrupts (ptplayer) still add 10-15 lines at random pictures.
- **Memory:** executable 318.2 KB in slow RAM (phase 0 and songs pay-back; 425.6 KB at `b9d5987`); `expect-min-chip-free 64` (KB) on every
  profile, and 96 in `tests/memory.agk`. Kickstart 3.1's chip RAM was the binding limit when it dipped below 64 KB; earlier
  builds measured lowest chip free 95 / 88 / 155 KB (a500 / ks31 / aros); at v1.2 85 / 83 / 158 KB (a1200 1075 KB); at
  v1.6 73 / 66 / 149 KB (a1200 1058 KB); after the pay-back 119 / 153 / 159 KB (a1200 1153 KB). While the program fills
  the 512K slow RAM, every KB it grows is a KB of chip RAM: watch `fast=` in the `AGK mem` lines (49 KB a500, 8 KB ks31).
  **The logo's memory (v1.40):** the boot logo's picture and two copper lists (~46 KB) are freed at the second picture after the splash, in one OS trip before the title's colours (`AGK splash freed`; nothing shows them again). Chip free in world 1 play: a500 105 -> 151 KB, a500-ks31 139 -> 186, a500-aros 145 -> 192, a1200 1214 -> 1261; the largest single piece stays 105 KB (the freed blocks are separate holes). `tests/splashfree.agk` guards 149 KB (a500 151 minus 2: a guard, not the game's floor); the floors stay 96 (`memory.agk`) and 64.
  **The merged build (v1.41, branch `integration`: main 7fc7e09 + w2-slice2 + bomb-hint + w2-sea-anim + w3-pilot; not a design change, no new feel numbers):**
  the five branches were each gated alone on main 5f60f11; together their pictures added up, so the merge paid with real savings and deferrals, never with a
  raised limit or a lowered floor (the limits stay: jets 94 in `jetsload`, 97 in `jets`, 98 in the world 2 scenarios, 90 in the hall of fame, floors 96 / 64).
  Measured per function with the profiler (the map's `.text.NAME` sections): after bomb-hint joined slice 2 the a500 had hiscore 91, w2barrage / w2dest 100 with a dropped frame,
  w2frtr 98-100. Fixes: (1) `msgFixOnly`: letters an erase crossed under a settled key (an explosion over a typed line) are redrawn without the text body's set-up (18-31
  raster lines); (2) the typing plan also serves GS_PLAY (the bomb prompt, one line) and its fast path erases a line that is to be blank by typing spaces over it; letters crossed
  while a text is still typing take the fix path and then the plan (hiscore 91 -> 87: the prompt types in its play part); (3) on a mission card the typing waits ONE picture behind
  a picture that redrew part of the HUD radar strip (the card's 7th and 8th pictures, a barracks opening: 60-90 raster lines), fixes never wait; (4) the freighter's timer bar
  only clears the pixels it lost (a byte at a time) instead of redrawing both rows (pixel-identical); (5) a bomb prompt is a key with something to say, so jets-headroom's
  "a new key with nothing to say settles at once" no longer applies in GS_PLAY while a hint is up (otherwise it never typed: `tests/hint.agk` red, then green).
  World 3 is `g->world` 2 (`logicIsW2` / `logicIsW3`): every world-2-only rule that slice 2 and the sea wrote with `g->world` now asks `logicIsW2` (deck, gusts, radar sites, destroyer, the
  jet and boat attack token, the card map, the mission BOB part); world 3's wall bump moved to its own event byte (`events5`, `EV5_BUMP`) because slice 2 owns `events3` / `events4`; world
  3's BOBs sit after the missions' parts in the BOB enum; world 3 clears the people run so `erasesMerge` and `msgCoverAll` never lean on world 2's draw order there. Known and left alone:
  after a Ctrl+L, W jump from one card to another the card keeps the previous card's map (the buffer's lit flag is only cleared when play starts); the goldens of `w3load` record that.
- **Kit techniques used:** `blitq` (blit queue), `copw` (raw copper writer), `text`, `pack` (the
  packed logos), `perf`, `asmmodule` (the per-picture BOB loop, `src/objblits.s`, its C version kept
  beside it as `OBJ_BLITS_C`), ACE's ptplayer for the music mask; our own splash and title copper
  pictures. Not used: `wavescript`, `longmap`, `modcredits`, `agahud`.
- **A1200 extras** (chosen at runtime, A500 unchanged): the 24-bit sky (4 MOVEs a line, all 172
  lines, also on the title); the 224-colour title logo (8 planes at 1x fetch, palette loaded in the
  vertical blank in banks of 32); quarter-pixel parallax (mountains 0.75 px/frame, hills 1.5, at
  full speed, instead of 0,1,1,1 and 1,2,1,2); colours 29-31 written by the copper (a CPU write
  between a line's two LOCT MOVEs would set only the low nibbles); FMODE 0, BPLCON3 `$0C00`,
  BPLCON4 `$0011` set in every list (Kickstart 3.1 leaves BPLCON3 at 0, which gave 1.0 wrong band
  colours on an A1200). Not tried on real hardware (section 12).

**Planned for the full game (approved, not built; 26 work items in `docs/CONTENT-PLAN.md` section 6 with tier and size).**
- **Per-world arena and loader:** a world file (packed bands, BOB sheets, terrain tiles, map) loaded from the floppy into one reused chip arena (kit `loading`). **Band layout table:** the copper builder reads each world's lines, heights, speeds and palettes from a table (amiga-hw).
- **Terrain strip** (amiga-hw, large; spiked first): a 1:1 PF2 ring buffer streamed a column at a time, a cloud-ceiling strip, the COLOR00 haze behind it; the copper list (465 of 496 entries used) must be enlarged for the strip, the ceiling strip and searchlight beams (about 3 KB chip more, estimated). *v1.11: world 2's low strip is built ("World 2's display" above); the tall strip and the ceiling strip are world 3's.* **Water:** the perspective floor as a sea with a BPLCON1 shimmer.
- **Height-map logic** (amiga-game): `floorAt` / `ceilAt`, wall bump, altitude above the floor, enemies and people follow terrain.
- **Set pieces and garrisons** with `wavescript` and its pacing checker. **Program diet:** resident title and world-1 art and samples move to disk; the executable stays at 430 KB or less. (Phase 0 did the chopper, the BOBs and the bands: 348.9 KB; the songs, 33 KB, followed in v1.8: 318.2 KB. Not moved: the sound effects, 62 KB of samples in the program, which play in the air where nothing can load.)
- Frame budget: terrain streaming, more units and long maps get a measured budget; the target stays average <= 70%, worst < 100%.

## 12. Production

- **Order of work (as it happened):** the engine and a first trip (1.0, 2026-11-04) -> smooth
  camera, shadow, weather, far sky (11-08, 11-09) -> HUD polish, the rotor sound, jets you see
  coming -> A1200 extras (11-09) -> the tank tell, dithered sky, quarter-pixel bands, mission card,
  hall of fame and demo (11-11) -> this spec.
- **Order of work for the full game (approved, not built):** phase 0 spikes and mock-ups (terrain strip, arena and copper list sizes, art mock-ups for worlds 2 and 3) -> phase 1 foundations with world 1 unchanged (every existing golden identical) -> phase 1b the ground-threat loop in world 1 (gate A) -> phase 2 world 2 as the vertical slice (gate B) -> phase 3 world 3 (gate C) -> phase 4 world 4, the finale and the ending (gate D) -> phase 5 disks, save, credits, AROS, perf, release. About 15 weeks, plus or minus 40% (estimated). Each gate is a build Paul plays. Features ship behind disabled flags and merge within a day or two.
- **Art gates for each world:** a `docs/STYLE.md` addendum, a reference mock-up approved by Paul, a pilot set approved by Paul, then the full pass; no visible bands, the perspective-floor standard, no flat-topped cut-offs.
- **Milestones:** playable core done; one complete mission done; three missions done; polish
  (art pilot landed 2026-11-11, approved by Paul); release v1.0 shipped (zip with ADF, README, GIF; no `CREDITS.txt`
  yet, none needed). Attract loop and hall of fame done.
- **Playtests:** outside playtests: none recorded. Complaints go in as tuning tickets.

**Acceptance criteria** (state as recorded in the commit messages at 2c0c4f9; **not re-run for this spec**)
- [x] `agk unit` passes (54 tests in `tests/unit/test_logic.c`)
- [x] `agk test` passes on a500, a500-ks31, a500-aros, a1200 (14 scenario files, per-profile goldens where intended)
- [x] no dropped frames in `tests/perf.agk` (max load 83% asserted, 80% measured)
- [ ] the pacing checker passes section 5's targets: **no checker exists** for this game
- [x] tells on every tank shell, unit-tested and scenario-tested (`testNoShellWithoutTell`, `tests/tell.agk`)
- [ ] boss anti-stall: **n/a** (no boss); no set piece
- [ ] the 512K master boots: **no**, error 103 at load, needs 1 MB (measured, section 2)
- [x] a release zip with README and the demo video (`build/release/first-1.0.zip`); no `CREDITS.txt`

**Prove It** (what each recent feature needs on WinUAE or real hardware, and who runs it. vAmiga is
the test bench, not the player: it ignores AGA BPLCON1's 35 ns bits, records mono audio and has a
writable disk.)

| feature | what proves it | who | state |
|---|---|---|---|
| quarter-pixel parallax on a real AGA machine | an A1200 (or WinUAE A1200 preset) in play: mountains and hills scroll without judder, no seam opens between bands | Paul on real A1200 | **unproven**; the copper values are checked in `tests/parallax.agk`, the pixels cannot be (vAmiga drops the bits) |
| write-protected disk path for the hall of fame | the real floppy or a Gotek with the write-protect tab set, or WinUAE with the ADF read-only: after a high score, "DISK WRITE-PROTECTED: NOT SAVED", no requester, no hang; then writable: "SAVED TO DISK" and the file reads back at the next boot | Paul, real floppy or WinUAE | **unproven** (vAmiga's regression disk is writable) |
| 24-bit sky and 224-colour title on a real A1200 | by eye on real hardware or WinUAE AGA: smooth sky, no steps; `tools/checkaga.py` already checks vAmiga screenshots pixel by pixel | Paul | README: "hasn't been tried on real hardware yet" |
| line-dithered OCS sky | by eye on a CRT and an LCD: half steps read as a smooth gradient, no visible flicker or lines | Paul | unproven (`tools/checksky.py` checks the table, not the look) |
| tank tell (16 frames) readable in play | played: the barrel-up flash is noticed before the shell on a real display | Paul playtest | unproven by a person |
| deep rotor thump by ear | listened to on speakers and on headphones in WinUAE: a heavy whop-whop at 5 a second, faster with speed and climb, a louder spin-up at lift-off, full not thin, no clicks at the loop; with the A500's LED filter on and off | Paul | unproven (agk records it; previews in `sound-preview/rotor`; nobody has judged it by ear; v4, 2026-11-16: the loop made at twice the rate and a softer slap after Paul's 2026-11-14 playtest, WinUAE recordings and previews in `C:\Users\admin\Downloads\rotor-v4`, still **unproven by ear**: listen to before-* and after-* there, then in play at hover, cruise, climb, descend, loaded, damaged and at lift-off) |
| initials typed on a real keyboard (v1.10) | WinUAE (and a real A500 if possible), a PC keyboard through the host-to-Amiga key map: a score that ranks, then letters with and without Shift, digits on the row and on the keypad, Space, Backspace, Return; Esc ends the entry with the score on the table and the game carries on to the title (never quits); P and M in the middle of an entry type or do nothing, never pause or mute; German-layout Y/Z swap noted (US positions); the stick still works and mixes with the keys | Paul, WinUAE with a real keyboard | **unproven** (vAmiga injects raw codes: `tests/hiscorekeys.agk`) |
| attract loop and demo | left alone 12 s + 8 s: the hall of fame, then the demo plays the same trip and ends with 16 aboard; fire returns to the title | Paul in WinUAE | scenario-tested in vAmiga only |
| boot time from a real floppy | fire-to-play measured with a stopwatch on a real A500 or WinUAE at floppy speed | Paul | **not measured** |
| damage and its blink (decided by Paul, 2026-11-11) | WinUAE, play: take one hit and see the smoke, the slower flight, DAMAGED in the HUD and 2 s of blinking; take a second hit during the blink (nothing happens) and after it (the chopper is lost); land on the pad and see it repaired | Paul | unproven by a person (scenarios check state, the first picture and the HUD pixels) |
| the jet's tell | WinUAE, play at normal: the nose-and-engine flash is noticed before the missile; the missile never comes from beyond the screen edge | Paul | unproven by a person |
| shell and missile speeds | WinUAE, play: shells (1.5 px/frame) and missiles (2.5) read as dodgeable but still threatening; the tank gap 85 and the jet gap 40 keep the game hard enough | Paul | unproven; the compensation is a guess |
| the bomb prompt (v1.19) | WinUAE, a new game on the keyboard (arrows, Space, X): fly left towards the first barracks: HOLD DOWN, THEN FIRE: BOMB appears under the score bar, a second later BTN 2, THEN FIRE: BOMB, and it is readable over the sky and clouds; do it (hold Down a second, Fire, Fire): the line changes to FIRE: BOMB   UP AND FIRE: GUNS and, after 3 bombs, nothing more is ever shown in that game; a new game shows it again; no flicker or tearing in the message line, no slowdown when a jet or smoke crosses it; chip free unchanged | Paul | **unproven by a person** (the scenario `tests/hint.agk` and `testHint*` check the rules and the pixels in vAmiga; chip free and the frame budget were measured: no change in chip, `jets` maxload 97 as before) |
| the tank quota | WinUAE, GODMODE 5 or normal: after 10 kills in a mission QUOTA 10 shows on the HUD and further tanks score nothing | Paul | scenario-tested in vAmiga only |
| world 2's display (v1.11) | WinUAE A500 (Kickstart 1.3 and 3.1, cycle-exact) and A1200: Ctrl+L, W, 4 / 5 / 6, fly the whole map both ways at full speed: no strip column popping in at either edge, no tear or wrong colour on lines 179-180 and 207-208 (the copper's palette and pointer switches), the sea's rows lean and shear smoothly, no seam between the sea and the strip, the islands appear whole under the card; on a real floppy, how long the card's load takes | Paul | **unproven**: vAmiga only (`tests/world2.agk`, `tests/world2pal.agk`; copper timing by a COLOR00 probe, `W2_COP_PROBE`) |
| world 2's gameplay (v1.13) | WinUAE A500 (Kickstart 1.3 and 3.1, cycle-exact) and A1200, Ctrl+L, W, 4 at skills 1, 3 and 5: the card's strip map reads; the pad's H, chevrons and lamps read at 1x from landing height and the landing feels forgiving (6 px tolerance, the nudge on skill 1-3, a 48 px pad on skill 4-5); the splash and the soft stop at the water; a gunboat's barrel rising and its lamp flashing are noticed 12-22 frames before the shell on a real display; the people come out six at a time without the rescue feeling slow; the unloading run into the hangar; no stutter with a pad, a barracks, six people and a boat on screen (real A500 timing: the game's meter is vAmiga's); the whole of missions 4-6 on the floppy with the card's load (how long it takes) | Paul | **unproven**: vAmiga only (`tests/w2play.agk`, `tests/w2boats.agk`) |
| the boot logo's memory freed (v1.40) | WinUAE A500 (KS 1.3 and 3.1) and A1200, cold boot: look for any flash or grey screen between the logo's fade-out and the title (the free is one OS trip, about two frames, with the colours still black), then play world 1 with no `AGK ERR` | Paul or anyone with WinUAE | **unproven**: vAmiga only (`tests/splashfree.agk`); the OS trip's look is not visible to the harness |
| world 2's jets (v1.16) | WinUAE A500 (Kickstart 1.3 and 3.1, cycle-exact) and A1200, Ctrl+L, W, 4 / 5 / 6 at skills 1 to 5: in mission 4 at skill 3 the first trip has its quiet take-off and then, about 12 s into the flight, a jet is announced (the radar blip flashes white, the two-beep alert) well before it is on screen, the card says AND JETS at skill 3-5 and not at skill 1-2, its nose flashes before its missile, a landed chopper on a pad is not bombed before its grace, jets and boats take turns, the jet reads against the sky and the sea in all three missions, the skill 1-2 flights stay free of jets in mission 4, and (WinUAE's smaller chip) the game boots and plays world 2 with no `AGK ERR`; Paul says whether "3 jets a trip from the first, the first one late" is enough for "not enough things to dodge" (a playtest check, v1.17) | Paul | **unproven**: vAmiga only (`tests/w2jets.agk`, `tests/chipwinuae.agk`, `testW2Jet*`); the squeeze is vAmiga's model of WinUAE's chip, not WinUAE's |
| the program without the unused ACE code (v1.13) | the game on a real A500 and on WinUAE: nothing the linker dropped was in use (the title, a mission, the hall of fame, the demo, both worlds, the music, all effects; `-Wl,--gc-sections` can only drop what nothing references, but a code path the scenarios never take is the risk) | Paul, WinUAE | **unproven**: all 4 profiles pass their scenarios; no real hardware |
| world 2's animated sea (v1.31) | WinUAE A500 (Kickstart 1.3 and 3.1, cycle-exact) and A1200, Ctrl+L, W, 4 / 5 / 6, hover over the sea and fly the map: the near water sways about 4 px and the swell runs toward the chopper, the glints brighten and fall back, no tear or jump on any line, no seam between the sea and the island strip (lines 207-208), the chopper, its shadow and the boats read over it, nothing flickers on a CRT or LCD; the frame meter stays clear of 100 % in the card's window | Paul in WinUAE (A500 and A1200); a real A500/A1200 shows the BPLCON1 steps on a CRT | **unproven**: vAmiga frames only (`tests/w2sea.agk`: the swell and the pen ramp move the sea's pixels, the same phase gives the same picture on all four profiles) |
| the approved new art (landed 567e6c5) | the scrolled bands show no seam at 1x on a real display; the palette reads on all three missions | Paul after the art merge | pending: Paul on WinUAE/real display |

## 13. World 3 Highlands: the terrain strip and the hardware pilot (v1.30)

Added by the `w3-pilot` branch (the first slice of CONTENT-PLAN phase 3). It is a new section on purpose: it does not
touch sections 1-12 and the version row is v1.30, so it merges beside the other branches' v1.19-v1.24. Status: the **hardware
pilot** is built behind `W3_PILOT` (a debug entry, Ctrl+L, W, 7; mission 7's first screens); everything else in World 3 is
*approved, not built*.

### 13.1 Decisions (Paul, 2026-11-24)
- Peaks stay **100 px** and the flyable height stays **192 px (44-236)**, with **no vertical scrolling**.
- The **tunnel is in mission 8**: a lit rock-roof tunnel (a rock roof hanging from the cloud-ceiling strip, plus a portal BOB).
  There is no dark fade.
- The **cloud ceiling is in M8 and M9 only**; M7 has none.
- The **perspective floor is dropped in World 3** (CONTENT-PLAN sections 2 and 6.2 are amended): the strip is the ground on all its lines.
- The **valley haze** is as drawn (a COLOR00 gradient H to V behind the strip, lines 130-255).
- The M9 enemy helicopter is drawn in one facing and the other is **flipped**; M8 has **no trucks and no AA guns** (the nests carry the ground fire).

### 13.2 Level-design rule: the air gap under the cloud deck
The deck (ceiling strip, lines 44-84) makes `ceilAt` 85 where it is present. A peak 100 px high leaves 51 px of air between the deck
and the crest: less than the chopper (20 px high) can fly through in comfort. **Rule: no stretch with a deck may need the chopper to
pass over terrain higher than 80 px (a gap of at least 70 px).** The route (a pass, a valley, a tunnel) must never force the 50 px
gap. Taller peaks (up to 100 px) are allowed beside a deck stretch only as walls the route goes round or through (a pass lower than
80 px, a tunnel). `logicW3GapOk` checks a height map against it; the pilot map and every future map carry this test.

### 13.3 Display (band table; STYLE-world3 sections 2 and 3)
One table per world drives the copper builder (`g_pBands`, logic.c: the band's first line, its rows, its camera speed as a shift
or 1:1, its palette index; 7 pens per band, dark to light). World 3 in game lines:

| lines | layer | speed | bitmap | palette |
|---|---|---|---|---|
| 24-129 | sky: COLOR00 gradient `sky_top` to H | n/a | none | n/a |
| 44-84 | cloud-ceiling strip (M8, M9 only), 41 rows | 1:1 | ring of 16 px columns | ceiling pens 1-7, loaded at 43 |
| 88-129 | far range, 42 rows | 1/4 | 320 x 42 loop | far pens 1-7, loaded at 87 |
| 130-255 | terrain strip, 126 rows, rows 0-5 empty of rock | 1:1 | ring of 16 px columns, 3 planes | strip pens 1-7, loaded at 129 |
| 130-255 behind | haze gradient H to V | n/a | COLOR00 per line | n/a |

Per-mission pens 1-7 (12-bit): see STYLE-world3 section 3 (the table is the contract: M7 far `345 456 568 78A 9AC DDE FFF`, strip
`112 435 765 C96 9AC DDE FFF`; M8 and M9 as there). Far pens 6, 7 equal strip pens 6, 7 in every mission.

### 13.4 Memory
- The floors stay **96 KB (a500, lowest chip free in play) and 64 KB (with WinUAE's 23 KB taken away)** in worlds 1, 2 and 3: never lowered.
- **One tile bank** for all three missions, **at most 40 top tiles** (the pilot uses 27 of the art's 35); only the palette differs per mission.
  16 rock fills, stored from row 21 (rows 0-20 are never visible: 630 B a fill).
- **The arena** is one chip block made at boot at the size of the biggest world (today 69,832 B: world 2's file and its composed pads).
  Loading a world **replaces** the previous one's art in it, never adds. World 3's content: the pilot is 51,394 B (the file 30,304 B, the
  gradients 2,784 B, the strip's ring 18,306 B); the art pass's levered figures are M8 66,878 B and M9 67,262 B (STYLE-world3 section 13):
  they fit the arena as it is, so **world 3 costs the game no chip**. (The older 62 KB / 71 KB targets were derived from world 2's figures
  before the splash lever below; they are replaced by this derivation.)
- **The ceiling for the arena** (a500, vAmiga; first measured with a `W3_KEEP_SPLASH` switch, removed at the integration, v1.41): the arena is made at boot,
  so every extra byte comes off every world's chip free. The boot splash's 47,840 B are freed at the title (v1.40, merged), so world 1 plays with 151 KB
  (105 KB before), world 3 with 155,592 B and world 2 with 147,384 B before slice 2's sets. **Slice 2 changed the binding world: mission 6's set
  (the freighter and the destroyer) leaves 113,800 B in play (111 KB, `tests/w2chip.agk`), 15.5 KB above the 96 KB floor, so the arena may grow by
  about 15 KB (to about 85 KB) before world 2 mission 6 falls below the floor** (world 1 and world 3 could take about 55 KB more; the per-mission
  lever can move more of mission 6's art to its card). Chip free in play in world 3 (bytes, the merged build): a500 155,592, a500-ks31 190,912,
  a500-aros 197,280, a1200 1,256,520 (the a1200 figure on the merge result; the pilot branch measured 1,287,632 with a smaller program); world 2
  (a500): 139,416 / 133,656 / 113,800 in missions 4 / 5 / 6. With the 23 KB squeeze the pilot measured 132,040 / 167,360 / 173,728 / 1,264,080
  (world 3: a500, ks31, aros, a1200), `tests/w3squeeze.agk` still passes.

### 13.5 Height-map semantics (`floorAt` / `ceilAt`)
A map is one byte per 16 px column: the surface height `h` (px above `GROUND_Y`, 0-100) at the column's left joint, plus a flag per
column (pad). The surface between two joints is the straight line (the tile's profile is its art). Quantised rises per 16 px:
0, 4, 9, 16, 28, 44 (0, 15, 30, 45, 60 and 70 degrees).
- `floorAt(x)` is the highest surface under the skids (x - 10 .. x + 10), as a y; the skids stop there and the chopper never goes
  below it. It lands only where the column is a **pad** (flat by construction, a bench tile); anywhere else the floor is a soft stop
  (no damage, no landing), as water is in world 2.
- **Slopes:** up to 45 degrees (a rise of 16 per 16 px) the chopper slides up the flank as the floor lifts it. **Walls:** a flank
  of 60 degrees or more (a rise of 28 or more) is solid from the side: the chopper's body (half-width 16) stops at it, its vx goes
  to 0, no damage (the wall bump costs speed only), and the bump sets an event for the puff.
- `ceilAt(x)` is the highest y the skids can reach: `CEIL_Y` (44) with no ceiling; under a cloud deck or a tunnel roof, the deck's base
  line plus the chopper's height (20), so its top stops at the base (eased down 3 px a frame, never a pop). The ceiling map is a height
  map too (M8, M9; one base line per 16 px joint, 0 = none).
- A barracks, pad or ledge stands on a bench tile: its y is `floorAt` of its column.

### 13.6 Nest tells by skill
The mountain AA nest raises its barrels (the tell frame: silhouette, a red lamp) before it fires: **26 / 22 / 18 / 16 / 14 frames
at skill 1-5**. The fire frame is the tell held with a muzzle flash.

### 13.7 The pilot (what is built)
`W3_PILOT`: world index 2, mission 7's first screens (foothills with a low pass, a pad on a bench, a nest, the lodge, the pines): a
1:1 terrain strip streamed a 16 px column at a time into a ring in PF2, the far range at 1/4, the haze, the strip fill under the world
card, and the world's art loaded into the shared arena in place of the previous world's (`disk/world3.bin`, `art/tools/make_world3_disk.py`).
Missions 8 and 9 show a "not built" card. No enemy behaviour yet (the nest and lodge are scenery): that is for amiga-game.
Measured in vAmiga (docs/WORLD3-HANDOFF.md has all of it): chip in play as in 13.4 (the world adds none); copper entries 631 (OCS) and
1,333 (AGA) against the lists' 661 and 1,340 (world 2's list is the bigger on the a500, so no growth); frame load 26-47 % average, worst
74 % (the picture after the card's world load); `disk/world3.bin` 30.3 KB, 13 KB packed; the strip streams pixel-exact against the art's
raster at 5 camera offsets (`tools/check_w3strip.py`, a500 and a1200).

## Open questions for Paul

(Numbers are kept from before. Questions 2, 4, 5 and 7 are decided, decided by Paul, 2026-11-11: see v1.2.)

1. Fire-to-play time from a real floppy is not measured (318.2 KB executable plus logos, art and songs). What is
   the budget? (The 1 MB requirement stands: decided by Paul, 2026-11-12, v1.5.)
3. Jet pacing is measured (v1.3, section 5); the tank and mine pacing is not. Is a full pacing run wanted?
9. Jet fairness (v1.3) is studio tuning, pending Paul's playtest: skill 1 landed at a barracks with jets around; a jet pair on
   skill 4; the warning (radar flash and alert). Studio decision: skill 1-2 never get pairs, skill 3 gets them in mission 3. Is that wanted, or should skill 3 stay single there too?
6. Add `docs/STYLE.md` (a copy of the pilot's) after the art lands?
8. Decided in v1.5: a credits screen and an animated ending are in the plan (section 10).
11. ~~**Patrol boats**~~ Decided by Paul, 2026-11-13 (v1.15): patrol boats stay unarmed in mission 4; from mission 5 they fire, with a tell (art drawn: `art/world2/patrol_boat.txt` frames 4 and 5), under the gunboats' fairness rules (tell frames by skill, only low and near choppers, ground grace, dwell, post-take-off grace, one attack at a time, skill scaling), a little weaker (section 6). (Original question, v1.13: (v1.13) are unarmed: their art has no tell frame and a shot needs a tell. Should `amiga-art` draw one (a bow gun flash, 2 frames, about 0.5 KB) so they can shoot, or stay scenery and targets (50 points)?)
12. ~~**People out at once**~~ Decided by Paul, 2026-11-13 (v1.15): keep `W2_OUT_MAX` = 6 people out at once. (Original question, v1.13: world 2's barracks release at most 6 at a time and the unloading run is 2 px a frame, because 16 on a beach beside a pad dropped frames. Does the rescue still feel like a rescue at 6, or is a bigger crowd worth a cheaper pad (art) or a cheaper person (the radar dot, the erase)?)
13. ~~**Pad width on skill 4-5**~~ Decided by Paul, 2026-11-13 (v1.15): keep 48 px pads on skills 4-5 with no nudge, and no HUD PAD tick. (Original question, v1.13: 48 px on every island (the 56 and 48 of the table land on the 16 px grid at 48), 6 px of tolerance, no nudge. Too tight, or the intended edge? And the HUD's PAD tick (not built): wanted, given the lamps?)
14. ~~**Missions 5 and 6**~~ Decided by Paul, 2026-11-13 (v1.15): the mission 5/6 interim (mission 4's islands plus more gunboats) is accepted for the playtest build. (Original question, v1.13: (v1.13) are mission 4's four islands with more gunboats until the winch, the deck, the freighter, wind, radar and the destroyer are built: is that a fair interim for a playtest gate, or should they wait?)
15. ~~**World 2's jets**~~ Decided by the coordinator on Paul's behalf, 2026-11-17 (v1.17), **flagged for Paul's playtest confirmation**: mission 4 at skill 3 gets jets from the first trip (trip 0), 3 a trip, one at a time, the first one late
    (10 s of flying); skill 4 follows from trip 0 (4 a trip) and skill 5 stays 0 / 5; skills 1-2 in mission 4 keep none (gentle, as the table says). The mission card's brief says "AND JETS" where jets can appear at the skill in force
    (it changes the world 2 card goldens). Paul to confirm after a WinUAE playtest: is 3 jets a trip, the first at about 12 s, enough "airplanes" for "not enough things to dodge"? (Each is one number in `s_pW2JetsFrom` / `s_pW2JetsTrip` /
    `W2_JET_FIRST`.) (Original question, v1.16: from the second trip? "AND JETS" on the card? skill 1-2 none or one a trip?)
16. **The strait destroyer** (v1.24, slice E; studio defaults for Paul's playtest, decided by Paul 2026-11-24: the hull edit, one facing, the list frame kept, no destroyed state for the AA): it keeps station (no patrol: one facing is drawn); both turrets down disables the ship and silences the AA and the barrage (the plan says only "disable both turrets to open the way"); the AA is under its own limits (air only, one or two flak shots live) and not under the landed-chopper token; the barrage is 40 s of the chopper within 360 px, then every 30 s, and the turrets open (cut to 2 HP) at 90 s but keep firing; the turret's rest between shells 170 / 140 / 120 / 100 / 85 frames; a turret pays 150, the disabled ship 100 more (400); the zone bonus is +100 when the last radar site falls (no medal yet); the strait's two mission 6 gunboat slots are the destroyer's. Are the guns and the barrage fair and readable? Is the barrage too early or too soon repeated?
10. Studio defaults awaiting a playtest gate (v1.5): the upgrade list and currency (4.5); the field pad on maps over 4000 px; world and mission names; the ending thresholds (90% / 60%) and tone, no named villain; music (own themes on cards, ending, credits); the tunnel; the scoring caps (first 6 kills a mission for trucks, AA and boats).

## Version history

| version | date | change |
|---|---|---|
| v1.0 | 2026-11-11 | retroactive: the game as it is at `main` 2c0c4f9, with the approved new art noted as landing |
| v1.1 | 2026-11-11 | the pilot art landed on `main` (567e6c5, 065a44d): section 8 and the milestones updated |
| v1.2 | 2026-11-11 | decided by Paul, 2026-11-11: A damage instead of instant loss; B shell 1.5 and missile 2.5 px/frame, a jet tell, no off-screen fire; C a tank score quota of 10 a mission; D GODMODE's tables equal normal's, unit-tested; E the 512K test (measurement only, result in section 2). Open questions 2, 4, 5, 7 removed |
| v1.3 | 2026-11-12 | (Paul's playtest, 2026-11-12; studio tuning, pending Paul's playtest): jet fairness (ground grace, one jet at a time, a warning and a longer tell, an easier skill 1), jet pacing measured; section 5, 6, 7 |
| v1.4 | 2026-11-12 | decided by Paul, 2026-11-12: the plain between the mountains and the hills, a true perspective floor (each line its own speed, / 4 to / 2), sections 4, 8, 11; art contract `docs/plain-spec.md`. Art v4 approved by Paul: no mesas, blue-noise wandering foot, perspective texture between and under the hills, no full-width solid or dithered rows (checked by `art/tools/plain_check.py`) |
| v1.5 | 2026-11-12 | approved by Paul: commercial content plan (`docs/CONTENT-PLAN.md`): twelve missions in four worlds (desert, islands, mountains, city), height-map terrain with no vertical scroll, pads and winch and wind, ground threats under shared fairness rules, lives refill per world with a continue, an animated ending and credits, two disks with one swap, 1 MB kept; sections 1, 2, 4-7, 9-12. Not built |
| v1.6 | 2026-11-12 | (Paul's playtest, 2026-11-12; studio proposal, pending his playtest): volleys never become bombs (the bomb stance needs a 20-frame down hold with no fire, section 3), eased turn through front with lean frames and a front-facing sway |
| v1.8 | 2026-11-13 | content plan phase 0, songs (T26), not a design change: the three songs are floppy files loaded a frame each at boot, the theme starting after them (50-200 frames later); program 348,940 -> 318,184 bytes; sections 2, 4, 9, 10, 11; `tests/songs.agk` |
| v1.7 | 2026-11-12 | content plan phase 0 (T25, program diet), not a design change: the chopper's frames, the BOBs and the bands are a floppy file loaded during the splash; the chopper's sprite DMA reads small per-buffer blocks; chip free in play and the boot times in sections 2, 4 and 11; `tests/memory.agk` |
| v1.10 | 2026-11-13 | the hall of fame moves to the kit's `hiscore` technique (`src/hiscore.c/.h`, `hiscore_disk.c`, `hiscore_keys.c`): keyboard initials entry (section 10), the game's keys not read in the entry, the table lines from `hiRow`; file format and floppy save unchanged; `tests/hiscorekeys.agk`, `tests/unit/test_logic.c` |
| v1.9 | 2026-11-13 | the frame-budget pass (section 11): skill-5 crash 180% -> 96%, hall of fame 135% -> 83%, perf 81% -> 70%; hovering damaged, a smoke puff every 6 frames, and the puffs go in the crash's explosion (section 3); the wait for the vertical blank after the swap (review A3); the land palette at the start of line 179 (review A2) |
| v1.11 | 2026-11-13 | world 2's display engine (section 11, "World 2's display"): `g->world`, the sea (28 perspective lines), the terrain strip, the per-mission palettes, the world arena and its loads, copper lists sized per world, the debug keys Ctrl+L, W, 1-6; measured copper timing, chip and frame. Not a design change (STYLE-world2, approved) |
| v1.12 | 2026-11-13 | world 1's mission cards patch the copper lists' colours instead of writing them again (section 11, "Copper lists", "The copper writer"): 4 dropped pictures per card -> none, worst 250 % -> 82 %; world 1's list has a sky MOVE on every line (596 of 661 entries); `tests/w1card.agk`. Not a design change |
| v1.13 | 2026-11-13 | world 2's gameplay slice (section 11, "World 2's gameplay"; sections 4.1, 4.4, 6, 2, 12): islands and pads and their guide, the island barracks and people (at most 6 out at once), water as a soft stop with a splash, patrol boats (unarmed: no tell frame) and gunboats under the shared ground-threat rules, mission 3 on to mission 4 and mission 6 to the end, the card's brief and strip map, the hall of fame's six missions; the BOB set on the floppy over world 1's BOB chip; **the unused ACE code linked out (325 -> 248 KB)**; not built: wind, winch, radar and jets, the timed freighter, the destroyer (missions 5-6 are mission 4's islands with more gunboats until then) |
| v1.14 | 2026-11-13 | decided by Paul: Esc in the hall-of-fame initials ends the entry at once and keeps the score with the initials so far (as the 30 s timeout does); it never discards the score and never quits from the entry. Sections 2 (keys) and 10; the kit's `hiscore` technique the same; `tests/hiscoreesc.agk`, `tests/unit/test_logic.c` testHighScoreEsc |
| v1.15 | 2026-11-13 | decided by Paul, 2026-11-13: open questions 11-14 resolved (patrol boats armed from mission 5 with the tell the art now has; 6 people out at once; 48 px pads on skills 4-5, no nudge, no PAD tick; the mission 5/6 interim accepted for the playtest). Section 6 (the patrol boat row), section 11 (boats, memory: the set gains the 2 tell frames, +1,088 bytes, chip free 98 KB on the a500 in play): `logicPatrolArmed`, `logicPatrolTell` (the gunboat's table + 4), a 30-frame longer rest, the bow gun as muzzle; `tests/w2patrol.agk`, `tests/w2patrol4.agk`, `tests/unit/test_logic.c` testW2Patrol* |
| v1.16 | 2026-11-16 | Paul's playtest, 2026-11-14 ("mission 4 at skill 3: nobody firing, not enough to dodge, no airplanes"; studio tuning, pending his playtest). Also records what `c4b5fbc` (merged in 9886ad1) did and the spec never said: the patrol boat is armed in mission 4 from skill 3 (`logicPatrolArmed`; unarmed on skill 1-2), boats fire faster from skill 3 (`logicBoatGap`, `s_pBoatCool`: 0 / 0 / 0 / -15 / -25 / -30 frames on the tank's pause). Jets come to world 2 (section 6, "Jets in world 2"; section 7 table; section 11: `BOB_W2_JET`, +1,152 bytes of chip, a500 chip free 98 -> 97 KB, `TPL_MAX` 58; open question 15; section 12): mission 4 skill 3 from the second trip, 3 a trip, scaled up by skill and mission, nothing on skill 1-2 in mission 4, one attack at a time across jets and boats. Every chip allocation in `main.c` is checked and a squeeze hook (Ctrl+L, Q / E) with `tests/chipwinuae.agk` reproduces WinUAE's smaller chip |
| v1.17 | 2026-11-17 | decided by the coordinator on Paul's behalf, 2026-11-17 (open question 15; flagged for his WinUAE playtest): world 2's jets begin on the first trip in mission 4 at skill 3 and 4 (`s_pW2JetsFrom` row 4: 0 / 255 / 255 / 0 / 0 / 0; budgets 3 / 4 / 5 unchanged), the first jet of a world 2 mission comes after `W2_JET_FIRST` (500 frames) of flying, and the mission card's brief says AND JETS where jets can appear at the skill in force (`logicJetsPossible`; the world 2 card goldens change on all 4 profiles; no chip). Skill 1-2 stay gentle. Tests: `testW2FirstTripJetComesLate`, `testW2BriefSaysJetsExactlyWhenTheyCanAppear`, the table and monotone checks, `tests/w2jets.agk`. Merge note: `rotor-sound` also bumps the version (expect a docs conflict) |
| v1.18 | 2026-11-16 | rotor v4 (merged after v1.17; numbered v1.16 on its branch `rotor-sound`; Paul's playtest 2026-11-14, studio tuning, pending his ears): the rotor loop is stored at twice the rate (Paula period = the logic's / 2: sampling lines at 12 kHz, the same 4.8 KB of chip), a softer, lower slap, descending settles lower, the empty take-off peaks at 7.8 a second (was 9.8 with a climb); the approved empty hover and cruise numbers unchanged. Section 9 (cause measured in WinUAE and vAmiga), 12; `logicRotorSynth`, `logicRotorHwPeriod`; tests; `sound/tools/check_rotor_audio.py`, `make_rotor_v4.py` |
| v1.19 | 2026-11-17 | Paul's playtest, 2026-11-17 (the Windows native build: "can't figure out how to bomb on rotor rescue with keyboard controls"); he approved teaching it in the game. First-use bomb prompts (section 10, "first-use prompts"; section 3's fire 2 row now says it is the quick way to the stance; section 12): a timed `HOLD DOWN, THEN FIRE: BOMB` / `BTN 2, THEN FIRE: BOMB` line while a tank or a closed barracks is ahead, and `FIRE: BOMB   UP AND FIRE: GUNS` in the stance, F-35's rules (4 shows a kind, 500 frames apart, 150 long, off after 3 bombs, kept through a continue, reset by a new game). No gameplay change; chip free unchanged (a500 105 KB at the lowest on the title and in world 1, 97 KB in world 2, 74 KB squeezed; the program grows 1.8 KB in fast RAM: fast free 127 -> 126 KB). `tests/hint.agk`, `testHint*` in `tests/unit/test_logic.c`; README controls |
| v1.20 | 2026-11-21 | World 2, slice 2 (missions 5 and 6 finished), branch `w2-slice2`: the slice list and build order (section 11, "World 2, slice 2": A radar sites and the jet zone rule, B wind and the windsock, C the ship-deck winch, D the timed freighter, E the strait destroyer and the zone bonus), the jet zone rule (jets start only inside a live radar site's 1200 px zone in missions with sites; mission 4 and world 1 unchanged) and the chip lever: the boot splash's 47 KB are freed when world 2 loads (world 2 chip free in play 99,544 -> 147,384 bytes on the a500; no floor lowered; `tests/w2chip.agk`). Studio proposal pending Paul's playtest. (v1.19 is the bomb prompt on branch `bomb-hint`, not merged: expect a docs conflict in this table.) |
| v1.21 | 2026-11-21 | World 2 slice B (branch `w2-slice2`): wind gusts (0.25 px/frame sideways for 3 s every 12-20 s, 50 frame tell, none on skill 1, not near barracks on skill 2-3, never landed or just after take-off or in the first 18 s) and the windsocks (section 11, "World 2, slice 2"); no new sound; +480 bytes of chip in the slice 2 block. Studio proposal pending Paul's playtest. |
| v1.22 | 2026-11-21 | World 2 slice C (branch `w2-slice2`): the ship-deck winch rescue in mission 5 (section 11, "World 2, slice 2"): a moored cargo ship replaces the third island's barracks, 16 people climb a rope in a 24 x 20 px hover box at 1 a second, paused not reset when you drift out, the winch is a landed-class state for the ground grace, take-off grace, one-attack-at-a-time and the gusts; the card's map now made whole per mission; +7,632 bytes of chip, `tests/w2chip.agk` floor 130 -> 128 KB. Studio proposal pending Paul's playtest. |
| v1.23 | 2026-11-21 | World 2 slice D (branch `w2-slice2`): the sinking freighter in mission 6 (section 11, "World 2, slice 2"): 16 people, a timer of 120 / 105 / 90 / 75 / 60 s by skill split into four sinking stages, the winch box follows the tilt, the people left when it sinks are lost, a pen 5 timer bar under the HUD radar, the card says what is out there; +14,688 bytes of chip (world 2 on the a500 116 KB free; the per-mission set lever measured, not taken), `tests/w2chip.agk` floor 128 -> 114 KB; a real mission clear -> next card scenario (`tests/w2clear.agk`, hook Ctrl+L, K). Studio proposal pending Paul's playtest. |
| v1.24 | 2026-11-24 | World 2 slice E (branch `w2-slice2`): the strait destroyer in mission 6 (section 11, "World 2, slice 2"): two turrets of 6 HP and four AA guns, turret shells under the ground-threat rules with a tell of 28 / 24 / 20 / 18 / 16 frames, flak with a ranging puff, a barrage at 40 s (30 frame tell) and the turrets open at 90 s, the listing hull and 400 points when both turrets are down, the zone bonus (+100 when the last radar site falls), whitecaps in the Typhoon; one facing, decided by Paul; the per-mission BOB set (the SPEC v1.23 lever, taken: mission 6 113,800 bytes of chip free on the a500, 15,496 over the 96 KB floor), `TPL_MAX` 104, the hull whole or in pieces (no pop-in), the hull without a mask (mission 6 113,800 bytes of chip free). Studio proposal pending Paul's playtest. |
| v1.30 | 2026-11-24 | decided by Paul, 2026-11-24 (World 3 questions): peaks 100 px and 192 px flyable, no vertical scroll; the tunnel in mission 8 (lit rock roof and a portal BOB, no dark fade); the cloud ceiling in M8 and M9 only; the perspective floor dropped in World 3; valley haze as drawn; the flipped second enemy-helicopter facing and no trucks or AA guns in M8 accepted. New section 13 (display band table, the 62 / 71 KB arena target, the 40-tile bank limit, `floorAt` / `ceilAt` semantics, the air-gap rule, nest tells by skill, the pilot behind `W3_PILOT`); CONTENT-PLAN sections 2 and 6.2 amended. Branch `w3-pilot`: the band layout table (T3), the height map (T4 slice), the terrain strip (T5), `disk/world3.bin` |
| v1.31 | 2026-11-24 | world 2's sea is animated (Paul's native playtest 2026-11-24: "the sea layer could use some animation, to show movement in the water, more interesting and 3D"; studio decision, pending his playtest; branch `w2-sea-anim`, numbered v1.31 because other branches hold v1.19-v1.24 and v1.30). Section 11, "Sea animation": a per-line swell (the near water, lines 12-26, drifts up to 3.8 px, the camera's own 1/z ratio, a wave running toward the player; the far water is still) and a shimmer of the sea palette's pens 4-7, both pure functions of the frame counter (`logicSeaPhase`, `logicSeaDrift`, `logicSeaPen`; the 16-bit wrap is seamless), written into copper values the list already has: no new copper entry, no chip; budget rules (a blit-count gate for the swell alone, a hold after a list is written) in the section. World 1 unchanged. `logicSeaScroll` takes the phase; `tests/unit/test_logic.c`, `tests/w2sea.agk`; every world 2 golden that shows the sea changes |
| v1.40 | 2026-11-24 | frame headroom for the skill-5 crash (section 11) and the boot logo's 46 KB freed after the splash (section 11, memory): not a design change, no golden changes (jetsload goldens recorded from the old build); `tests/jetsload.agk`, `tests/splashfree.agk`, `testDrawPeopleRunAndGroundFirst`. Other branches use v1.19-v1.24, v1.30, v1.31: expect docs conflicts, see `docs/JETS-HEADROOM-MERGE.md` |
| v1.41 | 2026-11-24 | integration of v1.19-v1.31 with v1.40 (branch `integration`; section 11, "The merged build"): not a design change; the frame fixes above (typing plan in play and for erases, fix path, typing waits behind a radar redraw on a card, the freighter bar's shrink), world 3 kept apart from world 2's rules (`logicIsW2`), the splash free at `worldLoad` removed (the free at the title stands); goldens of 14 world 2 / world 3 scenarios re-recorded (the sea band, card text typing phase, a radar-row dot, w3load's card map); unproven on WinUAE or real hardware: see the PLAYTEST-integration checklist |
| v1.42 | 2026-11-24 | the rotor's Paula channel is claimed in ptplayer (section 9, "Rotor channel claim"): a third effect at once (the HQ drop-off) no longer takes channel 2 and leaves the "saved" arpeggio looping where the rotor was; not a design change, no rotor number changes, +2 bytes of chip; `tests/hqsound.agk`, `sound/tools/check_rotor_back.py`, debug key Ctrl+L, J; `tests/w2patrol.agk` `expect-max-load` 85 -> 90 (decided by Paul, 2026-11-25): the claim adds about 4 points (83 -> 86-87 on the a500 and a500-ks31) to the first 50-frame window after world 2 loads, where `musicFor` stops the theme and re-claims the rotor channel; steady-state load and the average (40) are unchanged and no frame drops; deferring the re-claim was tried and broke the a500 world-2 load wait, so the limit moved instead |
| v1.43 | 2026-11-25 | the base ocean animation (section 11, "Base ocean animation"): the terrain strip water in three colour-cycled pens (swell, counter-wave, breathing), art regenerated (terrain_tiles.txt, disk/world2.bin), 3 palette MOVEs patched in quiet pictures, the two lists kept within one step of each other (flicker fix, `logicBasePenKey`, `bspread`); no chip, no copper entry, no feel numbers; tests/w2base.agk, testBaseWave, testBaseFlicker, tests/w2play.agk bspread; goldens of the strip rows (208-255) re-recorded in world 2 scenarios on all four profiles; `w2play` limit 85 -> 88 by Paul 2026-11-26 (main 84, ocean +1, rotor claim +1); a500 load 369 ticks on main, 402 here, world-2 load waits 525 |
| v1.44 | 2026-11-25 | World 2 winch and loading polish (branch `w2-freighter-ux`; section 11, "Winch and loading polish"; Paul's playtest 2026-11-25): the winch window by skill (box 56 / 48 / 40 / 32 / 24 px wide, 36 / 32 / 28 / 24 / 20 tall, hover speed 1.5 / 1.25 / 1.0 / 0.75 / 0.5 px/frame, rope kept 24 / 18 / 12 / 6 / 0 frames out of the box, a capture ring and a settle on skill 1-3; skill 5 is as before), lamps that show the winch's state (red, fast yellow, steady yellow), and the people on deck loading one at a time (the group thins, a person runs to the rope, climbs, boards: ABOARD and the board sound per person). Same rate, no chip. Studio proposal pending Paul's playtest. (v1.42 is on the branches `hqsound-fix` and `w2-base-ocean`, not merged: expect a docs conflict in this table.) |
