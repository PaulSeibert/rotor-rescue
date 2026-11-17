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
| fire 2, or X | turn: left, front, right, front... (always allowed, never times out) |
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
- **World 2, Coral Islands (4-6):** (*v1.11: the display, section 11 "World 2's display". v1.13: the gameplay slice, section 11 "World 2's gameplay": islands, pads and their guide, barracks and people, water, patrol boats and gunboats, the way in from mission 3 and out after mission 6. Not built yet: wind, the winch, radar sites, the timed freighter, the destroyer. v1.16: jets, section 6. Until the winch is, missions 5 and 6 are mission 4's islands with more gunboats and four barracks of 16.*) bright sea in morning, midday and storm dusk. Sky; clouds /8; distant volcanic islands /4;
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
- **Winch** (world 2 M5 on): hover in a 24 x 20 px box above a ledge, deck or roof; 8 people take 8 s on normal; the climb is
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
| *radar site* (world 2 M5 on) | 4; a bomb kills | static (island, peak or roof) | no shot. While alive, its 1200 px coverage zone (red on the HUD radar and the strip map) is where jets may appear; destroying it ends jets there for the mission. Outside any live zone no jets appear (world 1 has no radar: today's rule). The jet budget per trip stays the cap | the dish turns | 150 |
| *searchlight tower* (world 4 M10 on) | 4 | beam sweeps in 90 frames | the beam must rest on you; then a flare, then nearby AA and guns fire a volley of 2-3 shots at your lit position, each with its own tell and marker | the beam, the chopper flashing, the flare 12 frames later; beam dwell 60 / 50 / 40 / 34 / 28 frames | 100 |
| *enemy helicopter* (world 3 M9 on) | 4 (cannon 1 each; a bomb 3) | follows your height at 0.6 px/frame, 3 px/frame horizontal, keeps 80-160 px away, never within 56 | a burst of 3 shots at 1.5 px/frame, 40-frame gap between bursts; 1 helicopter at a time on skill 1-3 | nose swings to you, nose and rotor glow blinks; 24 / 20 / 16 / 14 / 12 frames | 200 |
| *destroyer* (world 2 M6 set piece) | two turrets of 6 HP, 4 AA | slow, patrols the strait | turret shells; a barrage at 40 s | turret turns and flashes; 28 / 24 / 20 / 18 / 16 frames | 400 |
| *gunship* (world 4 M12 finale) | 24 (cannon); a bomb 4; core exposed below 50% | strafes the roof line, always reachable | a 5-shot arc, a marked rocket volley, calls the AA | big nose flash, rocket marker; 30 / 26 / 22 / 18 / 16 frames | 1000 |

- **Jets in world 2** (v1.16; Paul's playtest, 2026-11-14: mission 4 at skill 3, "nobody firing at me, not enough things to dodge,
  no airplanes"; studio tuning, pending Paul's playtest; the numbers are the studio's proposal, flagged in "Open questions" 15). World 1's jets, with
  the same fairness rules and the same code (`jetsUpdate`), come to world 2, scaled by mission and skill so that the low skills stay gentle:
  - **When** (`s_pW2JetsFrom`, `s_pW2JetsTrip` in `logic.c`; a *trip* is one delivery home, `g->trip`): the trip a mission's jets begin on, and the
    most jets that may appear between one landing on the base pad and the next (the budget, as world 1's `s_pJetsTrip`; 255 = none at that skill):

    | | skill 1 | skill 2 | skill 3 (and GODMODE) | skill 4 | skill 5 |
    |---|---|---|---|---|---|
    | mission 4, Reef Run: from trip / jets a trip | none | none | 1 / **3** | 1 / 4 | 0 / 5 |
    | mission 5, Trade Winds | none | 2 / 2 | 0 / 4 | 0 / 5 | 0 / 6 |
    | mission 6, Typhoon Strait | 3 / 1 | 1 / 3 | 0 / 5 | 0 / 6 | 0 / 6 |

    So mission 4 at skill 3, Paul's playtest, has its first trip quiet (the pads and the water are new) and from the second trip home 3 jets a trip;
    skill 1-2 never see one in mission 4 and skill 1 none in mission 5; budgets never fall with the skill or the mission (`testW2JetsOnlyFromTheAllowedSkillAndMission`).
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
    while it flashes its warning (the radar there already carries the boats on row 4). The mission card's brief is unchanged (it names what the mission *starts* with).
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
| **world 2** jets: from trip / jets a trip, mission 4 (v1.16, section 6, "Jets in world 2") | none | none | 1 / 3 | 1 / 4 | 0 / 5 |
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
| 2 | the rotor loop, always (a 4800-byte looped blade thump; period 591 = 6 kHz at hover) | silent |
| 0, 1, 3 | effects: `sfx_channels = [1, 3]`; **no music in play** | music |

- Music plays on 0, 1, 3 only (the player's mask excludes 2). Effects borrow 1 and 3.
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
  from scratch (`sound/tools/make_rotor.py`, mirrored bit for bit by `rotorSynth` in `main.c`; no samples). Why the
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
  - **The program**: the unused ACE code was in the executable (the tile and scroll buffers, pak files, chunky and fix-point maths: 77 KB);
    `-ffunction-sections -fdata-sections` and `-Wl,--gc-sections` (CMakeLists.txt, `-D AGK_GC_SECTIONS=OFF` for `agk profile`, which maps code by
    object file) drop it: 325,364 -> 248,464 bytes with all of this gameplay in it (332,536 without the link change). AROS's first picture
    1976 (2347): `tests/memory.agk` now asserts 2100. The new setup code is compiled for size (`COLD`, `-Os`: `bobsBind`, `padCompose`, the
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
- **Memory:** executable 318.2 KB in slow RAM (phase 0 and songs pay-back; 425.6 KB at `b9d5987`); `expect-min-chip-free 64` (KB) on every
  profile, and 96 in `tests/memory.agk`. Kickstart 3.1's chip RAM was the binding limit when it dipped below 64 KB; earlier
  builds measured lowest chip free 95 / 88 / 155 KB (a500 / ks31 / aros); at v1.2 85 / 83 / 158 KB (a1200 1075 KB); at
  v1.6 73 / 66 / 149 KB (a1200 1058 KB); after the pay-back 119 / 153 / 159 KB (a1200 1153 KB). While the program fills
  the 512K slow RAM, every KB it grows is a KB of chip RAM: watch `fast=` in the `AGK mem` lines (49 KB a500, 8 KB ks31).
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
| deep rotor thump by ear | listened to on speakers and on headphones in WinUAE: a heavy whop-whop at 5 a second, faster with speed and climb, a louder spin-up at lift-off, full not thin, no clicks at the loop; with the A500's LED filter on and off | Paul | unproven (agk records it; previews in `sound-preview/rotor`; nobody has judged it by ear) |
| initials typed on a real keyboard (v1.10) | WinUAE (and a real A500 if possible), a PC keyboard through the host-to-Amiga key map: a score that ranks, then letters with and without Shift, digits on the row and on the keypad, Space, Backspace, Return; Esc ends the entry with the score on the table and the game carries on to the title (never quits); P and M in the middle of an entry type or do nothing, never pause or mute; German-layout Y/Z swap noted (US positions); the stick still works and mixes with the keys | Paul, WinUAE with a real keyboard | **unproven** (vAmiga injects raw codes: `tests/hiscorekeys.agk`) |
| attract loop and demo | left alone 12 s + 8 s: the hall of fame, then the demo plays the same trip and ends with 16 aboard; fire returns to the title | Paul in WinUAE | scenario-tested in vAmiga only |
| boot time from a real floppy | fire-to-play measured with a stopwatch on a real A500 or WinUAE at floppy speed | Paul | **not measured** |
| damage and its blink (decided by Paul, 2026-11-11) | WinUAE, play: take one hit and see the smoke, the slower flight, DAMAGED in the HUD and 2 s of blinking; take a second hit during the blink (nothing happens) and after it (the chopper is lost); land on the pad and see it repaired | Paul | unproven by a person (scenarios check state, the first picture and the HUD pixels) |
| the jet's tell | WinUAE, play at normal: the nose-and-engine flash is noticed before the missile; the missile never comes from beyond the screen edge | Paul | unproven by a person |
| shell and missile speeds | WinUAE, play: shells (1.5 px/frame) and missiles (2.5) read as dodgeable but still threatening; the tank gap 85 and the jet gap 40 keep the game hard enough | Paul | unproven; the compensation is a guess |
| the tank quota | WinUAE, GODMODE 5 or normal: after 10 kills in a mission QUOTA 10 shows on the HUD and further tanks score nothing | Paul | scenario-tested in vAmiga only |
| world 2's display (v1.11) | WinUAE A500 (Kickstart 1.3 and 3.1, cycle-exact) and A1200: Ctrl+L, W, 4 / 5 / 6, fly the whole map both ways at full speed: no strip column popping in at either edge, no tear or wrong colour on lines 179-180 and 207-208 (the copper's palette and pointer switches), the sea's rows lean and shear smoothly, no seam between the sea and the strip, the islands appear whole under the card; on a real floppy, how long the card's load takes | Paul | **unproven**: vAmiga only (`tests/world2.agk`, `tests/world2pal.agk`; copper timing by a COLOR00 probe, `W2_COP_PROBE`) |
| world 2's gameplay (v1.13) | WinUAE A500 (Kickstart 1.3 and 3.1, cycle-exact) and A1200, Ctrl+L, W, 4 at skills 1, 3 and 5: the card's strip map reads; the pad's H, chevrons and lamps read at 1x from landing height and the landing feels forgiving (6 px tolerance, the nudge on skill 1-3, a 48 px pad on skill 4-5); the splash and the soft stop at the water; a gunboat's barrel rising and its lamp flashing are noticed 12-22 frames before the shell on a real display; the people come out six at a time without the rescue feeling slow; the unloading run into the hangar; no stutter with a pad, a barracks, six people and a boat on screen (real A500 timing: the game's meter is vAmiga's); the whole of missions 4-6 on the floppy with the card's load (how long it takes) | Paul | **unproven**: vAmiga only (`tests/w2play.agk`, `tests/w2boats.agk`) |
| world 2's jets (v1.16) | WinUAE A500 (Kickstart 1.3 and 3.1, cycle-exact) and A1200, Ctrl+L, W, 4 / 5 / 6 at skills 1 to 5: the first trip of mission 4 at skill 3 is quiet, from the second a jet is announced (the radar blip flashes white, the two-beep alert) well before it is on screen, its nose flashes before its missile, a landed chopper on a pad is not bombed before its grace, jets and boats take turns, the jet reads against the sky and the sea in all three missions, the skill 1-2 flights stay free of jets in mission 4, and (WinUAE's smaller chip) the game boots and plays world 2 with no `AGK ERR`; Paul says whether "3 jets a trip from the second" is enough for "not enough things to dodge" | Paul | **unproven**: vAmiga only (`tests/w2jets.agk`, `tests/chipwinuae.agk`, `testW2Jet*`); the squeeze is vAmiga's model of WinUAE's chip, not WinUAE's |
| the program without the unused ACE code (v1.13) | the game on a real A500 and on WinUAE: nothing the linker dropped was in use (the title, a mission, the hall of fame, the demo, both worlds, the music, all effects; `-Wl,--gc-sections` can only drop what nothing references, but a code path the scenarios never take is the risk) | Paul, WinUAE | **unproven**: all 4 profiles pass their scenarios; no real hardware |
| the approved new art (landed 567e6c5) | the scrolled bands show no seam at 1x on a real display; the palette reads on all three missions | Paul after the art merge | pending: Paul on WinUAE/real display |

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
15. **World 2's jets** (v1.16, studio proposal, flagged for Paul): is mission 4 at skill 3, from the second trip with 3 jets a trip one at a time, enough "airplanes" for "not enough things to dodge"
    (the alternatives: from the first trip, or 4-5 a trip; each is one number in `s_pW2JetsFrom` / `s_pW2JetsTrip`)? Should the brief on the mission card say "AND JETS" (it would change every world 2 card golden and says
    nothing true for the first trip)? Should skills 1-2 in mission 4 see none at all (the proposal) or one a trip?
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
