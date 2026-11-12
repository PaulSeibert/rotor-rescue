# Rotor Rescue: game spec (retroactive)

Written after the fact, on 2026-11-11, from the code at `main` (2c0c4f9, "Goldens: the dithered
sky, the tank tell, quarter-pixel bands, the new screens") and from the repo's own tests and
commit messages. It describes the game as it is: where a Desert Palms playbook target isn't met,
the section says **Gap vs playbook** in plain words. It does not change the game. From now on it
is the contract everyone works from: change the design, change this file in the same commit.

Sources, used throughout: `src/logic.c` / `src/logic.h` (rules, tuning, tables), `src/main.c`
(display, sound, input, text), `src/hiscore.c`, `sound/sound.toml`, `art/art.toml`,
`art/palette.txt`, `agk.toml`, `tests/*.agk`, `tests/unit/test_logic.c`, and the art pilot's
`~/art-pilot/first/STYLE.md` and `NOTES.md` (approved 2026-11-11). References like `logic.c:454`
are line numbers at 2c0c4f9. "Derived" means worked out from the code's tables, not measured in
a run. "Landing" means in flight on another branch when this was written (see section 8).

## 1. Pitch

You fly a rescue chopper along one long, flat battlefield, 3072 px wide (about ten screens).
Four barracks, spread out between the enemy border fence and your base on the far right, hold 16
hostages each. Blow a barracks open with the cannon (two shots) or a bomb (one), land beside it so
the people can run aboard, then fly the cabin full (16) home through tank shells, jets and air
mines, and land on the pad so they run into the hangar. One hit and the chopper is gone with
everyone aboard. 64 people, three missions (Desert Dusk, Mountain Night, Coastal Dawn), three
choppers for the whole game. A side-view rescue shooter in the 8-bit helicopter-rescue tradition,
built from scratch: our own code, art, music and sound.

**Pillars**
1. **Land to save:** the rescue needs you low and still, which is where the tanks are. The risk
   is the game; flying is just how you get there.
2. **Every shell has a tell:** a tank stops, swings its barrel up and flashes before it fires
   (12-22 frames by skill, 16 on normal). Jets are heard and seen on the radar before they appear.
3. **A500 at 50 fps:** player, camera and parallax update every frame on a stock A500.

**Not:** a scrolling-shooter with waves or bosses; a game with lives by score; a puzzle of
routes (there is one battlefield, repeated three times with harder numbers).

## 2. Target machine

| machine | what it gets |
|---|---|
| **A500 OCS, PAL 50 Hz, Kickstart 1.3** | the whole game. README and `agk.toml` state **1 MB (512K chip + 512K slow)**: the 425.6 KB program lives in slow RAM. 50 fps, worst frame 81% (section 11). |
| 512K chip, no expansion | **not supported** (see Gap). Not tested here. |
| **A1200 AGA** (found at boot: Lisa's DENISEID low byte `$F8`, `agaDetect` in `main.c`) | a 24-bit copper sky on all 172 sky lines (4 MOVEs a line via BPLCON3 LOCT), a 224-colour title logo (8 planes, `titlelogo8.bin`), quarter-pixel parallax (BPLCON1 35 ns bits) for the clouds, mountains and hills. The game itself is the OCS game. |
| Kickstart 3.1, AROS ROM | tested profiles `a500-ks31`, `a500-aros`. AROS loads the program at about 10 frames a KB; the first picture arrives by frame ~2990 against the harness's 3000-frame boot window (`b9d5987`). |

**Gap vs playbook:** the playbook's master is a 512K A500 that boots and plays complete. This
game needs the 1 MB expansion; its 425.6 KB executable alone is most of an unexpanded machine's
RAM. Moving data to the floppy (as Rotor Raider does) is the route; not planned.

## 3. Core loop and feel

**Controls** (one-button joystick in port 2 plays everything; a second button makes turning quicker)

| input | verb |
|---|---|
| stick left / right | fly: accelerate to 3 px/frame with inertia |
| stick up / down | climb / descend; descend onto the ground to land; up to take off |
| stick held against the facing | turn, through facing the screen (side, front, other side) |
| fire, facing left or right | cannon (6 px/frame, repeat 10 frames, 3 shots live, 36 frames of life) |
| down + fire in the air | turn to face the screen |
| fire, facing the screen | drop a bomb (airborne only) |
| left / right, facing the screen | a sidestep: 2.5 px/frame, full speed in ~5 frames |
| fire 2, or X | turn: left, front, right, front... |
| keys | arrows, Space fire, X turn, P pause (in play), M music, Esc quit |
| Ctrl+L, then 0-5 | difficulty (section 7); Ctrl+L, T / H: debug dumps (parallax fine scroll, hall-of-fame file read-back) |
| fire on the mission card | skip it |

**Feel in numbers** (`logic.h:241-266`, `logic.c:714-827`)
- Side flight: `VX_MAX` 48 = 3 px/frame; `VX_ACCEL` 2 and `VX_DRAG` 1 (1/16 px per frame each), so
  24 frames to full speed and 48 to a stop: a heavy machine.
- Head-on sidestep: max 2.5 px/frame, accel 8 (5 frames), reversal 12 (~9 frames), drag 5 (~8 frames
  to a stop).
- Vertical: up max 1.75 px/frame, down max 2 px/frame, accel 3, released decel 2. Take-off starts at
  0.4 px/frame.
- Turn: 6 frames per step (side to front to side = 12); the stick must push against the facing 5
  frames to start a turn, 20 frames from the front (so you can strafe while bombing).
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
- **No shield and no invulnerability frames:** one hit destroys the chopper and everyone aboard. Then
  70 frames of crash, 150 frames of the LOST state (3 s), respawn on the pad.

**Fun at three scales**
- 10 s: land under a tank's tell and lift off as the barrel flashes; shoot a jet you heard coming.
- 1 min: a trip: shoot a barracks open, land, 16 run aboard, fly home, they run into the hangar.
- 10 min: three missions of four trips, one rank for everyone saved.

**Scoring** (`logic.c`)
| event | points |
|---|---|
| a person reaches the hangar | 100 (max 6400 a mission) |
| barracks opened | 25 |
| tank | 50 |
| jet | 100 |
| air mine | 75 |
| mission cleared | +10 per person saved |

Rank from people saved over the game (`logicRank`): under 40 ROOKIE, 40 PILOT, 90 CAPTAIN, 140 HERO,
180 LEGEND (of 192). Kept on death: score, saved count, the opened barracks and who is still free.
Lost on death: everyone aboard. No extra lives by score (**Gap vs playbook**, see 7).

## 4. Structure and content budget

One battlefield, three times. World 3072 px wide: your base on the right (pad x 2800-2940, hangar
x 2960, start x 2880), the enemy border fence at x 2560 (enemy ground units stay left of it), the
four barracks at x 420, 1080, 1700, 2240 (far to near; 2460, 1800, 1180 and 640 px from the start).

| mission | sky (top, horizon) | weather | first tank after | tanks at the start (normal) | jets on from | mines on from |
|---|---|---|---|---|---|---|
| 1 Desert Dusk | 0x214, 0xE75 | dust motes | 700 frames (14 s) | 1 | after trip 1 | after trip 2 |
| 2 Mountain Night | 0x001, 0x235 | rain (4 px/frame, slanted) | 550 frames (11 s) | 2 | the start | after trip 1 |
| 3 Coastal Dawn | 0x237, 0xFB8 | sea spray rising | 400 frames (8 s) | 3 | the start | the start |

(Normal skill: jets and mines switch on when trips + mission index reach 1 and 2; the tank cap is
`1 + trip + mission`, at most 5. Section 7 gives all skills.) Game lines (`main.c:75-82`): HUD 0-23, clouds band y72
(40 rows, camera / 8), mountains y120 (76 rows, / 4), a gap row at 196 where the land palette loads,
hills y197 (27 rows, / 2), ground y224 (32 rows, / 1). The title shows no cloud band.

- **No tile map and no per-mission load:** the world is rules plus four looping parallax bands, all
  resident. Only boot loads from the floppy (logos, hall of fame).
- **Disk:** one 880 KB floppy (`first.adf`, 901,120 bytes). Files: the executable, `dpslogo.bin`
  (22.2 KB packed), `titlelogo.bin` (10.4 KB packed), `titlelogo8.bin` (18.8 KB packed), and the
  hall of fame file `rrscores` (56 bytes) written by the game. About 477 KB of 880 KB used, derived.
- **Boot to play:** the DPS logo and title load during the black splash. Time from fire to play on
  a real floppy is **not measured** (open question 1); the AROS profile's first picture is at frame
  ~2990 (60 s of emulated time), its worst case.
- **Tile statistics:** n/a (parallax bands, no map). The art pilot's one-off budget is 40 feature
  stamps per mission band (section 8).

**Gap vs playbook:** no per-world load budget applies (nothing streams), but a 20 s fire-to-play
target is unproven; the preview card (2.4 s, section 10) is the only "level intro".

## 5. Pacing

**Not measured.** The playbook's threat metrics (mean, p90, quiet share, shots a minute) have not
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
| fire ramp ~30/min first zone to ~100/min late | per tank in range: a shell every 110-141 frames on normal (24 a minute) at mission 1 trip 0; `-20` frames a mission and `-8` a trip, floor 40-71 frames (54 a minute per tank). Up to 5 tanks, 6 shells live | plausible, **not measured** |
| next level +50% enemies | tank cap +1 a mission, spawn gap -40 frames, shell gap -20, jet rest -30 | yes by tables |

**Length:** a bot-assisted trip to the nearest barracks and home takes 1452 frames (29 s,
`tests/rescue.agk`). Pure flight for four round trips is at least 12,160 px / 3 px a frame = 4053
frames (81 s); with unloading (10 frames a person) a clean mission is about 2 minutes (derived).

**Gap vs playbook:** the game has no pacing contract at all. There is no wave table to check, no
set piece, no checker run. Until it is measured, treat the enemy numbers in 6 and 7 as the spec and
do not call the pacing "tuned".

## 6. Enemies and bosses

| enemy | HP | movement | fire / threat | tell | score |
|---|---|---|---|---|---|
| tank | 2 (cannon 1 each; a bomb within 22 px kills) | spawns off screen (cam -40 or +screen +40), drives at the chopper 0.5 px/frame, stays between x 16 and 2536 | a shell when the chopper is low (skids y > 150) and within 200 px: aimed with the chopper's half velocity, error +- (mask+1)/2 px, 2-4 px/frame, life 100 frames; first shell 150 frames after spawning | stops, barrel up, flash blinks every 4 frames, 12-22 frames by skill (16 normal) | 50 |
| jet | 1 (cannon only) | from 130-255 px off screen (lead by skill), mostly ahead of where you fly (3 of 4), 4 px/frame, follows your height at 0.5 px/frame | a missile (5 px/frame, life 60 frames) only if on screen 18-40 frames (by skill; 24 on normal), you are airborne, 56-200 px ahead and within 14 px of its height; a bomb if you are landed within 24 px; ram kills | heard (a sound on spawn), on the radar, minimum 56 px ahead (`JET_AHEAD_MIN`), then the on-screen delay. No flash. | 100 |
| air mine | 1 (cannon only) | spawns off screen at y 54-117, homes at 0.5 px/frame in x and 0.25 in y when within 300 px, never below y 200 | contact | none (it is visible and slow) | 75 |
| barracks | 2 (cannon 1 each; a bomb within 30 px does 2) | static; releases 1 person per 8 frames after a 20-frame delay | none; roofs stop bombs and the chopper | burning frames | 25 |

- **Tell units:** tanks, section 7 table. A unit test (`testNoShellWithoutTell`) and a scenario
  (`tests/tell.agk`: no `AGK sfx tankfire` without a `tank tell` before it) enforce it for every
  shell.
- **Shell and missile speed. Gap vs playbook** (1-1.5 px/frame): shells 2-4 px/frame, the jet
  missile 5 px/frame (6 before `e46d2a1`). Long tells and the 56 px minimum make them readable; a
  slower shell is a tuning ticket.
- **No fire from off screen, no point-blank:** a tank fires only inside 200 px and low; a jet needs
  18-40 on-screen frames and 56 px of room. **Gap:** the jet's "ahead < 200 px" gate can place it
  up to ~40 px beyond the screen edge once it has been seen (the chopper is 104-216 px from the
  screen's left edge), which the on-screen test (`seen`) does not recheck; no unit test covers it.
- **Sight no longer than weapon range:** tanks drive at the chopper from any distance but shoot
  only inside 200 px; mines see 300 px and have no weapon.
- **Path scripts:** none. Movement is per-unit code in `logic.c` (no `wavescript`).
- **Bosses and set pieces: none. Gap vs playbook:** no boss, no mid-mission set piece, no
  anti-stall escalation (there is nothing to stall: a mission ends when all 64 are saved or lost).
  The nearest thing is the ramp in the trips: tanks +1 a trip, jets after trip 1, mines after trip 2.

## 7. Difficulty

Ctrl+L then a key, within two seconds; it stays until changed. Shown at the right of the score
bar. Tables in `logic.c:454-468`. Index 0 is GODMODE.

| | 1 easiest | 2 easy | **3 normal** | 4 hard | 5 hardest |
|---|---|---|---|---|---|
| tanks at once (`1 + trip + mission +`) | -1 | -1 | 0 | +1 | +2 |
| shell gap (frames, `110 - 20*mission - 8*trip +`) | +60, floor 60 | +30, floor 50 | 0, floor 40 | -15, floor 32 | -30, floor 26 |
| aim error (px, +-) | 32 | 32 | 16 | 8 | 4 |
| tank tell (frames) | 22 | 18 | 16 | 14 | 12 |
| jets from (trip + mission >=) | 3 | 2 | 1 | 1 | 0 |
| mines from | 4 | 3 | 2 | 1 | 1 |
| spawn gap (`220 - 40*mission +`; every skill floored at 60) | +120 | +60 | 0 | -40 | -70 |
| jet rest between passes (`160 - 30*mission +`; floored at 40) | +80 | +40 | 0 | -30 | -50 |
| jet starts this far off screen (px) | 255 | 220 | 180 | 150 | 130 |
| jet on screen before its first shot (frames) | 40 | 30 | 24 | 20 | 18 |

- **Never changes:** the battlefield, barracks positions and hit points, people, cabin 16, 3 lives,
  the chopper's handling and hitbox, every tell existing, one-hit death.
- **The ramp:** within a mission by trip (tanks +1, shell gap -8, jets after the first trip, mines
  after the second); across missions by mission (tanks +1, spawn gap -40, shell gap -20, jet rest -30,
  quiet start 700 / 550 / 400 frames).
- **Hero devices: none.** No shield, no smart bomb, no extra lives by score, no power-ups.
  **Gap vs playbook:** one hit ends a chopper and every rescue in progress; the only forgiveness is
  the long tells and the safe base. The playbook asks for at least one hero device.
- **Respawn:** in place at the pad, landed, after a crash (70 frames) and a 150-frame LOST screen.
  Enemy shots, jets and mines are cleared; jets cool 150 frames; tanks stay. The camera pans back.
  Lives (3) are for the whole game and are not refilled between missions. People still free stay
  free; open barracks stay open. No checkpoints and no continue: game over goes to the hall of
  fame, then the title.
- **Exploit rules (read from the code, none unit-tested):**
  - The base is safe: no tank spawns while the chopper is at x 2760 or more, tanks cannot pass
    x 2536, and a tank shoots only within 200 px, so the pad (x 2800+) cannot be shot.
  - Flying high is safe from tanks (they need skids y > 150), but not from jets and mines.
  - **Open:** tanks respawn on a timer while you are left of x 2760, 50 points each; a tank-farming
    loop (and mines at 75) is possible for score. It makes no progress and is not closed.
- **Testing aids:** GODMODE (key 0: plays like normal, nothing destroys the chopper, no hi-score;
  its jets use the easiest lead and delay, see open question 5), Ctrl+L 1-5, the serial heartbeat
  (`state`, `x`, `landed`, `aboard`, `saved`, `trip`, `lives`), and the scenario checkpoints
  `boarding` and `rescued`.

## 8. Art direction / style guide

The approved direction is `~/art-pilot/first/STYLE.md` (sections 1-10, pilot approved on mock-ups
2026-11-11) and its `NOTES.md`. **Landing:** the art integration is being committed to `main`
as this is written (seamless parallax bands, the mountain fix, new sprites). This section describes
the approved direction; the shipped 1.0 look (drawn by `art/tools/draw.py`) is what `main` still
has until that lands. Update this section after the merge, and decide
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
| 9-15 | the parallax bands, reloaded by the copper per band and per mission. Each band is drawn in indices 1-7 dark to light so one bitmap serves all three palettes |

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

**Animation** (`logic.h`, art pilot section 6): chopper 6 poses x 2 rotor phases, mirrored for
left; person 8 frames (stand, 2 run right, 2 run left, 2 wave, down); tank 10 (2 each way, 2 fire,
2 aim, 2 flash); jet 2; barracks 4; explosion 5; mine 2; shadow 4 sizes x 2 dither phases; flag 2.
Rule: one drawn frame per 4-6 px of travel, held at least 2 game frames.

**Sizes** (`logic.c:14-31`): hangar 64x40, fence 16x32, barracks 48x28, tank 32x14, person 16x12,
jet 32x12, mine 16x12, shot 16x3, bomb 16x6, shell 16x4, explosion 32x32, flag 16x24, shadow 32x4.

**Sources (honest list)**
- 1.0 art: scripts only, `art/tools/draw.py` (our own drawing code), logos from the user's source
  pictures (`art/src/*.png`). No other games' art.
- Approved pilot (**landing**): Retro Diffusion (`rd_plus__environment` + `tile_x` for the bands,
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
| 2 | the rotor loop, always (a 3008-byte looped chop at period 428, 8287 Hz) | silent |
| 0, 1, 3 | effects: `sfx_channels = [1, 3]`; **no music in play** | music |

- Music plays on 0, 1, 3 only (the player's mask excludes 2). Effects borrow 1 and 3.
- **Rotor:** period 428 minus speed/4 in 1/16 px units (416 at top speed: about 3% higher in pitch than the hover); volume 38
  plus speed/4 (50 at top speed, +4 climbing); landed 24 (16 on the card); 0 when paused or dead. The loop is
  synthesised at startup after the splash (it pushed AROS past its boot window when done earlier).
- **Priorities** (higher wins): crash 5; open 4; boom 3, start 3; tankfire 2, board 2, saved 2, jet
  2; cannon 1, bomb 1, land 1. Explosions and the crash beat weapons; important effects are short
  (0.08-0.8 s). `soundPlay` order in `playSounds` takes crash/open/boom as one choice per frame.
- **Gap vs playbook:** no panning by screen X (effects go to whichever of 1 and 3 is free); no
  cap on effects per frame beyond priority. Rotor Raider has both.
- **Music:** one tune per state, all our own, synthesised sampled instruments (`sound/samples/`
  bass, hat, kick, lead, snare from `sound/tools/make_samples.py`): `theme` for the title, card,
  hall of fame and initials; `fanfare` for mission cleared and the ending; `gameover`. **Gap:** no
  music in play (the rotor owns the sound); no boss or ending theme (none needed: no boss).
  No third-party music.

## 10. Presentation

| item | state |
|---|---|
| boot | Desert Palms Software logo: 288x242, 16 colours with 4 reloaded per line (about 320 colours), fades in and out, fire skips; its 47 KB is kept until quitting (freeing it froze the title for ~6 frames, `670227d`) |
| title | the ROTOR RESCUE logo (192x129, 4 planes, 16 colours with 8 reloaded per line, ~800 changes) over the sky; A1200: a 224-colour version; the theme; author and copyright lines; hi-score in the score bar |
| attract loop | **yes:** title 600 frames (12 s), then the hall of fame 400 frames (8 s), then a recorded demo of the first trip (1453 input frames, 29 s, DEMO in the score bar, no score kept), then the title; fire ends it |
| options | difficulty by Ctrl+L, no menu |
| mission flow | a preview card before each mission (120 frames, fire skips): MISSION n OF 3, its name, a strip map of the battlefield (enemy ground red, fence, four barracks, pad, hangar, a "you are here" arrow), "64 HELD IN 4 BARRACKS" and a one-line brief (TANKS ON PATROL / ...AND JETS / ...AND AIR MINES). No start banner |
| in play | HUD: SAVED / ABOARD / LOST / score; choppers left; a radar of the whole battlefield |
| end of mission | MISSION COMPLETE, SAVED n OF 64, 250 frames, then the next card |
| game over | GAME OVER, total saved, rank, score; 300 frames, fire skips after 100; then the hall of fame if the score ranks |
| high scores | a hall of fame of 5 (initials, score, mission reached, rank), saved to the floppy as `rrscores` (56 bytes, checksummed, read at boot); name entry with the stick, 30 s limit; defaults DPS 5000, PRS 4000, ACE 3000, AGK 2000, RTR 1000. A write-protected disk is found with `Info()` before any write; the table still shows with "DISK WRITE-PROTECTED: NOT SAVED". Never brings up a requester |
| ending | ALL MISSIONS COMPLETE: the tally and a rank. **Gap vs playbook:** no animated ending |
| credits | the author and publisher lines on the title. **Gap:** no credits screen (no third-party music, but no art credits either; the pilot's tools are not listed) |

## 11. Tech plan

- **Display:** PAL lores 320x256, a dual playfield of 6 planes. PF1 (planes 1/3/5, colours 1-7):
  lines 0-23 the HUD bitmap, below it a double-buffered frame buffer for every BOB, drawn by the
  blitter from a queue while the CPU runs the game. PF2 (2/4/6, colours 9-15): four looping
  parallax bands, each with its own fine scroll and palette. The sky above the cloud band (line 72) runs
  with 3 planes to leave bus to the CPU; 6-plane DMA starts at `CLOUDS_Y` (README still says "the top
  120 lines", from before the cloud band). DDFSTRT `$30` (a fetch word early for
  fine scroll; costs sprite 7). One raw copper list per buffer, rebuilt each frame; 496 entries
  allocated, 465 used (`b9d5987`). The title has its own list (the logo as a 4-plane section).
- **Sprites:** 0-3 the chopper (32 px, 15 colours, attached pairs, a pose per frame); 4-5 weather
  (13 items stacked down each channel's DMA list, slid by the CPU); 6 the far sky's wisps or stars
  (behind both playfields, priority code `0x1B`); 7 lost to the fine-scroll fetch.
- **BOBs:** 13 types, `DRAW_MAX` 96 a frame. Limits: 3 player shots, 6 enemy shots, 5 tanks, 2 jets,
  3 mines, 6 explosions.
- **Frame budget (A500):** `tests/perf.agk` (a whole trip: 16 boarding, 16 running into the hangar)
  asserts `expect-max-load 83`; the heaviest picture measures 81%, about 19% short of a dropped
  frame (`e6cc691`). No dropped frames. The average load is not recorded. Target (average <= 70%,
  worst < 100%): the worst is met; the average is unmeasured.
- **Memory:** executable 425.6 KB in slow RAM (`b9d5987`); `expect-min-chip-free 64` (KB) on every
  profile. Kickstart 3.1's chip RAM was the binding limit when it dipped below 64 KB; earlier
  builds measured lowest chip free 95 / 88 / 155 KB (a500 / ks31 / aros).
- **Kit techniques used:** `blitq` (blit queue), `copw` (raw copper writer), `text`, `pack` (the
  packed logos), `perf`, ACE's ptplayer for the music mask; our own splash and title copper
  pictures. Not used: `wavescript`, `longmap`, `modcredits`, `agahud`.
- **A1200 extras** (chosen at runtime, A500 unchanged): the 24-bit sky (4 MOVEs a line, all 172
  lines, also on the title); the 224-colour title logo (8 planes at 1x fetch, palette loaded in the
  vertical blank in banks of 32); quarter-pixel parallax (mountains 0.75 px/frame, hills 1.5, at
  full speed, instead of 0,1,1,1 and 1,2,1,2); colours 29-31 written by the copper (a CPU write
  between a line's two LOCT MOVEs would set only the low nibbles); FMODE 0, BPLCON3 `$0C00`,
  BPLCON4 `$0011` set in every list (Kickstart 3.1 leaves BPLCON3 at 0, which gave 1.0 wrong band
  colours on an A1200). Not tried on real hardware (section 12).

## 12. Production

- **Order of work (as it happened):** the engine and a first trip (1.0, 2026-11-04) -> smooth
  camera, shadow, weather, far sky (11-08, 11-09) -> HUD polish, the rotor sound, jets you see
  coming -> A1200 extras (11-09) -> the tank tell, dithered sky, quarter-pixel bands, mission card,
  hall of fame and demo (11-11) -> this spec.
- **Milestones:** playable core done; one complete mission done; three missions done; polish
  (art pilot landing, 2026-11-11); release v1.0 shipped (zip with ADF, README, GIF; no `CREDITS.txt`
  yet, none needed). Attract loop and hall of fame done.
- **Playtests:** outside playtests: none recorded. Complaints go in as tuning tickets.

**Acceptance criteria** (state as recorded in the commit messages at 2c0c4f9; **not re-run for this spec**)
- [x] `agk unit` passes (54 tests in `tests/unit/test_logic.c`)
- [x] `agk test` passes on a500, a500-ks31, a500-aros, a1200 (14 scenario files, per-profile goldens where intended)
- [x] no dropped frames in `tests/perf.agk` (max load 83% asserted, 81% measured)
- [ ] the pacing checker passes section 5's targets: **no checker exists** for this game
- [x] tells on every tank shell, unit-tested and scenario-tested (`testNoShellWithoutTell`, `tests/tell.agk`)
- [ ] boss anti-stall: **n/a** (no boss); no set piece
- [ ] the 512K master boots: **no**, needs 1 MB (section 2)
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
| new rotor whop by ear | listened to on speakers: deep thump and slap, no tinny hiss, no clicks at the loop | Paul | unproven (agk records it; nobody has judged it by ear) |
| attract loop and demo | left alone 12 s + 8 s: the hall of fame, then the demo plays the same trip and ends with 16 aboard; fire returns to the title | Paul in WinUAE | scenario-tested in vAmiga only |
| boot time from a real floppy | fire-to-play measured with a stopwatch on a real A500 or WinUAE at floppy speed | Paul | **not measured** |
| the approved new art (**landing**) | the scrolled bands show no seam at 1x on a real display; the palette reads on all three missions | Paul after the art merge | pending the merge |

## Open questions for Paul

1. Fire-to-play time from a real floppy is not measured (425.6 KB executable plus logos). What is
   the budget, and does a 1 MB requirement stand?
2. Should the game get a hero device (a shield, a respawn blink, or a hit that doesn't take the
   cabin)? The playbook asks for one; today one hit loses everything aboard.
3. Is a pacing run wanted (a scripted GODMODE trip, as Rotor Raider's `tools/pacing.agk`)? Section 5
   is derived only.
4. Slow the tank shell and jet missile toward 1-1.5 px/frame? (2-4 and 5 today.)
5. GODMODE (key 0) is documented as "plays like normal", but its jets use the easiest lead (255 px)
   and on-screen delay (40 frames) because the tables' index 0 differs from index 3 (`logic.c:464-465`).
   Intended?
6. Add `docs/STYLE.md` (a copy of the pilot's) after the art lands?
7. Close the tank-farming loop (a score exploit), or leave it?
8. Add a credits screen and an animated ending, as the playbook's presentation list asks?

## Version history

| version | date | change |
|---|---|---|
| v1.0 | 2026-11-11 | retroactive: the game as it is at `main` 2c0c4f9, with the approved new art noted as landing |
