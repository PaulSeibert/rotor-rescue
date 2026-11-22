# Rotor Rescue: content plan for a commercial-length game

**Status: approved by Paul, 2026-11-12** ("approved; proceed"), with the answers in section 10. Branch `content-plan`.
The approved text of sections 11.1-11.3 is now in `docs/SPEC.md` v1.5 (sections 1, 2, 4-7, 9-12). Where this plan and
SPEC.md differ, SPEC.md wins. Nothing here is built yet.

Every number is labelled: **[M]** measured from the repo, build or latest test logs; **[D]** derived from
measured numbers; **[E]** estimated (a studio guess, to be checked); **[S]** taken from SPEC.md as written.

Scope status: Paul approved on 2026-11-12 his four ideas (with the studio's take) and all eight studio
additions. They are decided scope in this plan, not options. What is left open is numbers, order and names
(section 10).

---

## 0. One-page summary

**What it becomes.** Twelve missions in four worlds, about 80 minutes for a good first-time player [E].
Today: three missions on one flat 3072 px battlefield that differ only in palette and numbers, about 20 minutes [S].

| world | missions | place | what is new | set piece |
|---|---|---|---|---|
| 1 Border Desert (today's) | 1-3 | flat desert, 3072 px | missile trucks and AA guns, the "clear the landing zone, then land" loop | the border convoy (M3) |
| 2 Coral Islands | 4-6 | sea, islands, 4608 px | water and boats, small island pads with a landing guide, wind gusts, ship-deck winch, radar sites, a burning/sinking rescue | the strait destroyer (M6) |
| 3 Highlands | 7-9 | mountains, 4096 px | terrain you route through (passes, valleys, a low cloud ceiling, a tunnel), barracks at four heights up to summit pads, ledge winch rescues | the summit helicopter duel (M9) |
| 4 Harbour City | 10-12 | night port and rooftops, 3584 px | rooftop pads and winches between towers, searchlights that call in fire, fires, enemy helicopters | the lit gauntlet (M10) and the gunship finale (M12) |

**The five design moves.**
1. **Terrain becomes a height map.** Today the ground is flat (`GROUND_Y` 236 [M]) and the landing code already asks
   a function, `floorAt(x)`, what the floor is (roofs already work that way [M]). Making that a per-world height map
   gives islands, mountains, summit pads and rooftops in one stroke.
2. **A new "terrain strip" on screen** (a 1:1 scrolling strip in the lower part of the second playfield, streamed a
   column at a time). It draws the height map. This is the one big hardware item (amiga-hw, L) and it is spiked first.
3. **Ground threats join the existing fairness rules, not replace them.** Missile trucks, AA guns, gunboats and the
   rest use the same ground-grace table as jets, a shared "one landed-chopper attack at a time" rule, a tell of 12
   frames or more (most 20-36), and a visible impact marker. Barracks sites get a garrison you can clear before you
   land (section 4).
4. **Each world teaches one thing in its first mission** (a dip in pressure), then ramps. A set piece sits near the
   middle of each world and the finale ends the game.
5. **Content streams from the floppy.** The program must not grow (AROS boot window has 62 frames, about 6 KB, of
   spare [S]); each world is a packed file of 150 KB or less that is loaded into one shared chip arena. Two disks,
   no world split across them, one swap during a full game.

**Big risks (details in 9).**
- Chip RAM: only **81 KB free in play on the A500 profile** against a floor of 64 KB [M]. A world's art has to replace the
  previous world's, never add to it.
- The copper list is 465 of 496 entries used [S]: a terrain strip, a cloud-ceiling strip and searchlights do not fit
  without enlarging it.
- Frame budget: worst frame 80% today [S]. Terrain streaming, more enemies and long maps need a measured budget.
- Scope: this is about 15 weeks of work [E, plus or minus 40%], with art as the long pole.

**Order of work.** Spikes and mock-ups, then engine foundations with world 1 unchanged (every existing golden must stay
identical), then the ground-threat loop in world 1, then world 2 as the vertical slice, then 3, then 4 and the ending.
A playtest gate after each world.

---

## 1. Goal and design pillars

**Goal.** A complete, commercial-length Amiga rescue game: about 80 minutes for a good player [E], twelve missions, four
distinct battlefields, a real animated ending and credits, on two floppies, running at 50 fps on a 1 MB A500.

**What we keep (Paul loves it, nothing here may weaken it).** The new perspective plain; the explosions; the rotor
and its sound; the damage blink; the polish features (attract loop, hall of fame, mission card, demo). The chopper's
handling and hitbox, the cabin of 16, the first-hit-damages rule and the ground-grace rules from the 2026-11-12 playtest [S].

**Pillars for the full game** (1-3 are today's, 4-6 are new). Inspirations are the genre and its mechanics only: side-view
helicopter rescue, terrain flying, naval and air defence. No other game's levels, names or art.

| # | pillar | in practice |
|---|---|---|
| 1 | **Land to save** | the rescue needs you low and still; the risk is where you land |
| 2 | **Every threat has a tell** | 12 frames or more on every skill, for every new enemy too; shells and missiles stay slow |
| 3 | **A500 at 50 fps** | the chopper, camera and parallax update every frame |
| 4 | **Read the ground** | the terrain is the level: you plan a route, a landing and a retreat from what you can see |
| 5 | **Prepare, then land** | threats to a landed chopper are visible and clearable beforehand, never a surprise |
| 6 | **One new idea per world** | the first mission of a world teaches it with less pressure; the rest use it |

**Not:** a fuel-limited game; a game with long stretches where you cannot land; a scrolling shooter with bosses as the
main event; a game where terrain kills you (terrain is a wall to route around, never a death, see 6).

**Design rule for length.** Every stretch of flight has a place to land within about 45 seconds [E]: a pad, flat
ground, or a ledge. No mission is a transit across nothing.

---

## 2. The world list

All four worlds share the engine: sky gradient, a stack of parallax bands, the perspective floor, the terrain strip
(new), and the chopper and BOBs on top. Palettes obey the standing rules: 7 colours per band, dark to light, one
bitmap serves a world's missions, the copper swaps palettes.

### World 1 - Border Desert (missions 1-3), exists today

| item | plan |
|---|---|
| theme and palette | dusk, night, dawn [S]. **Rename** missions 2 and 3 to Desert Night and Desert Dawn: "Mountain" and "Coastal" will be wrong once worlds 2 and 3 exist [S: names are in `logic.c`] |
| battlefield | 3072 px, four barracks at x 420, 1080, 1700, 2240, base on the right [S]. Unchanged length, flat |
| parallax and floor | as shipped: clouds /8, mountains /4 plus far strip, perspective floor (19 lines), hills /2, ground /1 [S] |
| mechanic it introduces | "clear the landing zone, then land" (section 4). Each barracks gets a placed garrison |
| new enemies | missile truck (M2), AA gun site (M3) |
| set piece | M3 "border convoy" at about 50%: a column of trucks, tanks and AA escorts crosses on the road; destroying the lead truck stops it; reward: a repair crate (repairs the damage mid-trip) and a bonus |
| rest beats | the base, the flight home, and the first 8-14 s of each mission (today's quiet start); after each cleared garrison a 4-8 s stretch with no spawns |
| risk | none to the look: this world's art and goldens do not change except where trucks and AA appear |

### World 2 - Coral Islands (missions 4-6)

| item | plan |
|---|---|
| theme and palette | bright tropical sea. Morning (turquoise sky, warm light), midday haze, storm dusk (grey-green, rain). New band palettes per mission, same slot rules |
| battlefield | 4608 px [E], about 14 screens: a chain of islands from the enemy end to your base island on the right. Sea gaps of 300-700 px between islands. A free "field pad" mid-map (see 10, Q6) |
| parallax and floor | sky; clouds /8; **distant volcanic islands** /4 plus a far strip; **the sea is the perspective floor** (lines 180-207, re-cut, wave rows in perspective; the copper adds a 1-px shimmer by time) ; hills band dropped; **terrain strip** from line 208 (48 rows: island ground, beach, surf; island height at most about 40 px). Behind the strip the COLOR00 gradient is the sea |
| mechanic it introduces | **islands and pads.** Water is not landable (touching it is a soft stop with a splash, no damage). Each barracks sits on an island next to a **landing pad** of limited size (section 4.4). **Wind gusts** from M5. **Winch** from M5 (ship deck) |
| new enemies | gunboat (M4), radar site (M5), destroyer (set piece M6), AA gun and missile truck reused on islands |
| set piece | M6 "the strait destroyer" at about 50%: a destroyer with two gun turrets and AA guards the strait between the island groups. Disable both turrets (or sink it) to open the way. Anti-stall: at 40 s it fires a barrage (30-frame tell); at 90 s its turrets overheat and stay open |
| rest beats | sea crossings between islands (no spawns for the first 5 s after leaving an island; wind and spray only), the base island, the mission start |
| risk | water shimmer and the re-cut floor are copper work (S-M); small pads are the new feel (needs the Paul playtest) |

**Jets in world 2 (SPEC v1.16, built; Paul's playtest, 2026-11-14: mission 4 at skill 3 had no airplanes).** The world 1 jets, with the same warning, tell and
fairness rules, fly in world 2 from the first trip of mission 4 at skill 3 (3 a trip, one at a time, the first one 10 s into the flight; decided 2026-11-17), scaled by mission and skill (SPEC 6, "Jets in world 2": skill 1-2 in
mission 4 and skill 1 in mission 5 have none; mission 6 has the most). They stay out of the base's approach (left of x 3800) and take turns with the boats (R3). The radar site
of mission 5 will later limit *where* they may appear (its coverage zone, section 4.4) and the per-trip budget stays the cap [S]. The jet is world 1's own art, a copy in the
world 2 set: +1,152 bytes of chip (a500 chip free 98 -> 97 KB in play), the squeeze test `tests/chipwinuae.agk` keeps WinUAE's smaller chip honest.

### World 3 - Highlands (missions 7-9)

| item | plan |
|---|---|
| theme and palette | alpine dawn, bright noon, a snow-storm dusk. Cold blues, rock browns, snow whites; each world keeps its own 7-colour band palettes |
| battlefield | 4096 px [E]. A winding range: foothills, a high pass, a plateau, a ridge with a tunnel. The map is a route you plan, not a straight line |
| parallax and floor | sky; far range /4 (above line 130); **terrain strip** from line 130 to 255 (125 rows, about 17 KB ring [E]) holding the mountains you fly through; optionally a **cloud-ceiling strip** at 1:1 (lines 44-84, about 5 KB [E]) in the stretches with a cloud ceiling. Behind the terrain strip the COLOR00 haze gradient reads as valley depth. The perspective floor shows only in M7's low foothills (a meadow/scree floor) and is not drawn where the strip is tall. Honest limit: where mountains are tall, they replace the hills and plain, they do not sit in front of them |
| mechanic it introduces | **terrain shapes the route**: passes (a gap you must be at the right height for), valleys (low and safe from jets, but boxed in), a **low cloud ceiling** (you cannot climb; ceiling is a height map too), and a **tunnel** (ceiling and floor both rock). **Barracks at four heights**: valley floor, a mid shelf, a plateau, a **summit pad**. **Winch ledges** where you cannot land. Wind in the passes |
| new enemies | enemy helicopter (M9), radar site on a peak, truck and AA on slopes (flat segments only), mountain-top AA nests |
| set piece | M9 "summit duel" at about 50%: an enemy helicopter ace contests the summit while you winch from the ledge. Anti-stall: at 40 s it goes reckless (fires in 3-shot bursts with a 20-frame tell, flies low); at 90 s it is forced to hover over the ridge, in range |
| rest beats | a valley with no spawns after each pass (4-8 s), the cloud-ceiling stretch is jet-free, the summit pad after a clear |
| risk | the tall terrain strip is the biggest hardware item (section 6). Vertical room is limited to 192 px [M: `CEIL_Y` 44 to `GROUND_Y` 236]; mountains up to about 110 px high leave 80 px of sky. No vertical scrolling in the base plan (Q5) |

### World 4 - Harbour City (missions 10-12)

| item | plan |
|---|---|
| theme and palette | night port, a burning block at the blue hour, a pink-orange last light. Lights: sodium amber, searchlight white-blue, fire orange. Lit windows are 2-colour dither |
| battlefield | 3584 px [E], dense and tight: dockside cranes, warehouses, towers 60-120 px tall with canyon streets between them, rooftop pads, a harbour mouth |
| parallax and floor | sky glow; far skyline /4; mid skyline /2 where the strip is low; **terrain strip** (lines 120-255, about 17 KB ring [E]) is the near cityscape; **the perspective floor is a road and quay** in the low gaps; harbour water between docks. Searchlights: sky beams (see section 6 for the hardware options) |
| mechanic it introduces | **rooftop landing and winching between towers**; **searchlights** that sweep and call in fire; **fires** (timed rescues); the world is tight, so wind and enemy helicopters matter |
| new enemies | searchlight tower, rooftop AA, enemy helicopters (several), the finale gunship |
| set pieces | M10 "the lit gauntlet" (three searchlight towers and a crane AA nest cover the docks; clear or sneak). M12 "last light": the gunship finale (section 5) |
| rest beats | the harbour water crossings (the lights are off over the water), dark alleys the beams cannot reach, the base |
| risk | the art is the largest of the four. Rooftop pads and tall towers need the same height-map code as world 3 and no vertical scrolling in the base plan |

---

## 3. The mission table

"Sites" = barracks you shoot open and land beside, or winch spots. All missions have 64 people so scoring and ranks
stay simple [E]. Rank thresholds become percentages. Minutes are for a good first-time player including a death or
two [E]; a clean run is about 40% of that [D, from today's 2-minute clean run [S]].

| # | world | name | people / sites | new element | pressure (M1 = 1.0) [E] | minutes [E] |
|---|---|---|---|---|---|---|
| 1 | 1 | Desert Dusk | 64 / 4 barracks | (as today) | 1.0 | 6 |
| 2 | 1 | Desert Night | 64 / 4 | missile truck, garrisons, the clear-then-land loop | 1.25 | 6 |
| 3 | 1 | Desert Dawn | 64 / 4 | AA gun site; the convoy set piece; jet pairs | 1.5 | 7 |
| 4 | 2 | Reef Run | 64 / 4 island barracks | water, islands, small pads with the landing guide, gunboats | 1.1 | 5.5 |
| 5 | 2 | Trade Winds | 64 / 3 barracks + 1 ship deck (16) | wind gusts, the ship-deck winch, radar sites | 1.4 | 6.5 |
| 6 | 2 | Typhoon Strait | 64 / 3 barracks + 1 sinking freighter (16, timed) | time pressure, the destroyer set piece | 1.7 | 7 |
| 7 | 3 | Foothills | 64 / 4 barracks at 4 heights | terrain strip, slopes, passes, barracks at varied heights | 1.2 | 6 |
| 8 | 3 | High Pass | 64 / 3 barracks + 1 summit pad (16) | cloud ceiling, summit pad, wind in passes, radar peaks | 1.6 | 7 |
| 9 | 3 | Cliff Ledges | 64 / 2 barracks (32) + 4 ledge winches (8 each) | ledge winches, the tunnel, the summit helicopter duel | 1.9 | 7.5 |
| 10 | 4 | Night Docks | 64 / 4 rooftop and dock barracks | rooftop pads, searchlights, the lit gauntlet | 1.5 | 6 |
| 11 | 4 | Burning Block | 64 / 2 barracks (32) + 4 rooftop winches (8 each), fires timed | fire time pressure, enemy helicopters in packs | 1.9 | 6.5 |
| 12 | 4 | Last Light | 64 / 2 rooftop barracks (32) + HQ roof (32, two trips) | the gunship finale, the evacuation | 2.2 | 8 |

Total about 80 minutes [E] (inside the 60-90 target). World totals: 19 / 19 / 20.5 / 20.5 minutes.

**Difficulty step.** Within a world the pressure rises about +25% a mission; the first mission of a new world drops
back (1.1 / 1.2 / 1.5) because the player is learning a mechanic. Across worlds the end-of-world pressure grows about
+25% (1.5, 1.7, 1.9, 2.2). Skills 1-5 keep their meaning (tables in section 7 of SPEC).
Upgrades (below) never change the tables.

**Pacing targets from the design playbook, adapted to a rescue game.** The playbook numbers are from vertical
shooters that scroll forward; a rescue game is trips between sites on a map you choose the order of. So:

| playbook target | adapted for Rotor Rescue |
|---|---|
| threat on screen mean 2.5-3, p90 6 | counted as live enemy units plus shots; target mean 2-3 in a world's first mission, p90 6; measured per world by the pacing checker, not guessed |
| waves 5-8 s apart | **encounters** (a garrison, an event, a roaming spawn) reached 5-8 s apart in flight on average; one gap in five of 10-20 s; spawns by map position, not timer (fixes today's gap [S]) |
| rest beats 4-8 s per zone, about every 60-80 s | a rest after each cleared site and each sea/valley crossing; 4-8 s with no spawns and no shots; at least one every 80 s |
| quiet 5-7 s before a set piece, set piece at about 50% | the set piece sits between the 2nd and 3rd site, with the preceding 5-7 s flight clear |
| level-to-level +50% enemies, +80% fire | +25% a mission inside a world (the maps are longer and the first mission of each world is a dip) and +50% at the end of each world to the start of the next teaching dip; the exact curve comes from the checker |
| enemy fire 30/min first zone to 100/min late | per mission: 30/min (first site) rising to about 70/min late, lower than a shooter because the player is often landed [E] |
| tell at least 12 frames, shots 1-1.5 px/frame | kept, with longer tells for lethal weapons (section 4.4) |
| boss anti-stall 30-45 s, weak point by 90 s | kept for the destroyer, the helicopter duel and the gunship |
| time to first threat 3-6 s | not adopted (it conflicts with "a quiet start" already in the game [S]); first threat 8-14 s in world 1, 6-10 s later |

The checker is `wavescript`'s pacing check [M: technique exists, `techniques/wavescript`] applied to set pieces and
garrison scripts; a trip bot in the style of `tools/jetsim.c` [M] measures the rest.

**Upgrades between missions (approved).** A hangar screen after each mission: spend **merit** (10 per person saved plus
medals for zone clears and set pieces) on four upgrades, each of two levels:

| upgrade | effect | note |
|---|---|---|
| armour | the first hit of a trip is absorbed (smoke, no slow-down); refills on the pad | the second hit still destroys |
| winch | winching 30% / 60% faster | |
| seats | cabin 18 / 20 | a winch site plus a barracks fits in one trip |
| cannon | 4 shots live (not 3) / and a repeat 8 frames (not 10) | |

Rules: tuned for no upgrades, so upgrades are comfort and expression, not required; no upgrade touches a tell, a shot
speed or the fairness rules; upgrades reset on game over; GODMODE and ghost mode ignore them; skill 1 starts with level 1 of armour (Q4).

---

## 4. Ground threats and how they stay fair (Paul's idea 3)

### 4.1 The rules (these join the existing ones; none is removed)

| # | rule | detail |
|---|---|---|
| R1 | **Ground grace, shared** | One table for every weapon that can hit a landed chopper (jets, trucks, gunboats, garrison guns): landed this long before it may be targeted: never (skill 1), never (2), 6 s (3), 4 s (4), 3 s (5) [S: today's jet table]. GODMODE uses skill 3 |
| R2 | **Take-off grace** | 2 s after lift-off no new landed-or-low attack starts [S: `JET_TAKEOFF_GRACE`] |
| R3 | **One landed-chopper attack at a time** | A global token across jets, trucks and gunboats: on skill 1-3 one attack on a landed or hovering chopper at a time, the next waits at least 3 s; on skill 4-5 two, 3 s apart [S: today's jet pair rule, extended] |
| R4 | **Tell of at least 12 frames, longer for lethal ones** | by skill, section 4.4 table. A unit test per weapon, like `testNoShellWithoutTell` [S] |
| R5 | **The impact is marked** | the truck's missile is a lob with a blinking marker on the ground from launch; flak shows a ranging puff before it bursts. You can read where it lands |
| R6 | **Slow weapons, long flights** | missiles 1.25 px/frame on the larger axis for ground-launched lobs (flight about 80-100 frames), shells 1.5 [S], flak burst radius small and visible |
| R7 | **Not from off screen, not point-blank** | same as jets [S]: a ground weapon fires only when it is on screen and the target is 56 px or more away |
| R8 | **Sight no longer than weapon range** | [S] |
| R9 | **Terrain is not a weapon** | bumping a wall stops you and costs speed; it never damages |

### 4.2 The loop: clear the landing zone, then land

- Each barracks (and winch site) has a **garrison**: 1-3 placed units (a missile truck, an AA gun, a tank, a gunboat
  offshore) within about 300 px, placed by the map, **visible from a distance** (the world is not scrolling vertically,
  so you see them as you approach).
- A garrison is fixed. You clear it with cannon or bomb before you land. Killing all of it is a **zone clear**: +100
  points and a medal, and the roaming spawns will not come within 400 px of a cleared site while you are landed there.
- If you land without clearing, R1 applies: on skill 1-2 the garrison cannot attack you while landed, but still
  attacks you in the air; on skill 3-5 it starts attacking after the dwell time. So the choice is real and the penalty
  is paid in time you chose to spend.
- This makes the threat to a landed chopper prepared-for, not cheap: you saw it, it had a tell, it had a marker, and
  you had a way to remove it.

### 4.3 What each new ground enemy does to a landed chopper

| enemy | airborne | low / hover | landed (after the R1 dwell) |
|---|---|---|---|
| missile truck | no | lobs at your position (marker) | lobs; you lift off in time because the flight is 80-100 frames and the marker shows from frame 1 |
| AA gun | flak at your altitude (ranging puff) | cannot depress; **does not fire at a chopper below 24 px above ground** | cannot hit you: this is the counterplay (fly low, land) |
| gunboat | no | shells (tank-like) | shells under R1, short of its minimum range |
| tank | no | shells | shells (as today [S]; tanks are already part of the base game and keep their behaviour) |
| jet | missile | bombs | bombs after dwell [S] |

### 4.4 The enemy roster (proposed SPEC section 6)

Existing: tank, jet, air mine, barracks [S]. New ones below. Tells are frames by skill 1 / 2 / 3 / 4 / 5.
Today's tank is 22 / 18 / 16 / 14 / 12 and the jet 24 / 20 / 16 / 14 / 12 [S].

| enemy | role and where | HP | movement | fire pattern | tell (look) | tell frames (1-5) | score | counterplay |
|---|---|---|---|---|---|---|---|---|
| **Missile truck** (W1 M2 on) | area denial against a landed or hovering chopper; escorts convoys | 3 (cannon); bomb kills | drives 0.4 px/frame on flat ground, stops to fire; slopes: only on flat segments | lobbed missile at your position when you are landed or below 30 px; 1 launch, then 150 frames reload; blast radius 20 px | stops, rails rise, launcher flashes; **the ground marker blinks from launch** for the flight | 36 / 32 / 28 / 24 / 20 | 75 | lift off or move 30 px; shoot it first; bomb |
| **AA gun site** (W1 M3 on) | denies altitude; guards pads and peaks | 3; bomb kills | static | flak burst at your position when you are above 24 px, within 260 px; gap 120 / 100 / 85 / 70 / 60 frames; max 1 burst live (skill 1-3), 2 (skill 4-5); radius 18 px | barrel swings, muzzle flash, then a **ranging puff** appears at the aim point and the burst follows | 26 / 22 / 18 / 15 / 12 | 100 | fly under 24 px; hover above it and bomb; keep moving |
| **Gunboat** (W2 M4 on) | the sea's tank; guards sea gaps and island shores | 3; bomb kills (20 px) | 0.6 px/frame along its sea segment, stays on water | shell like a tank's (1.5 px/frame), 85-frame gap on normal; range 220 px, minimum 56 | turret swings up, flashes (as the tank) | 22 / 18 / 16 / 14 / 12 | 100 | rise above its elevation, kill it, cross out of range |
| **Radar site** (W2 M5 on) | strategic: calls jets; covers a stretch of map | 4; bomb kills | static, on an island, a peak or a roof | none. While alive, its coverage zone (1200 px, shown red on the HUD radar and strip map) is where jets may appear. Destroying it ends jets in that stretch for the mission. Outside any live zone no jets appear (World 1 has no radar: today's rule) | the dish turns; the radar blip on your HUD shows the zone | n/a (no shot) | 150 | destroy it, or avoid the zone, or fight jets |
| **Searchlight tower** (W4 M10 on) | calls fire onto you at night | 4 | beam sweeps 90 frames per sweep; may pause on you | the beam must rest on you 40 frames ("lit": the chopper flashes); then a flare pops (12 frames later) and the nearby AA and guns fire at your lit position: a volley of 2-3 flak or shells, all with their own markers | the beam, the flash, the flare pop | beam 60 / 50 / 40 / 34 / 28 plus flare 12 | 100 | break out of the beam, shoot the tower (the beam goes dark), fly low |
| **Enemy helicopter** (W3 M9 on) | an air threat that duels at your height | 4 (cannon 1 each; bomb 3) | follows your height at 0.6 px/frame, 3 px/frame horizontally, keeps 80-160 px away, will not come within 56 px | a burst of 3 shots 1.5 px/frame, 40 frame gap between bursts, 1 helicopter at a time on skill 1-3 | nose swings to you and the nose-and-rotor glow blinks | 24 / 20 / 16 / 14 / 12 | 200 | break the line of height, outpace it, shoot it (it turns slowly) |
| **Destroyer** (W2 M6 set piece) | a sea set piece | two turrets of 6 HP each + 4 AA | slow; patrols the strait | turret shells with a tell of 28 / 24 / 20 / 18 / 16; barrage at 40 s | the turret turns, flashes | 28 / 24 / 20 / 18 / 16 | 400 | take out the turrets from outside AA range; anti-stall rules |
| **Gunship** (W4 M12 finale) | the boss | 24 (cannon), bomb 4; core exposed below 50% | cruises and strafes along the roof line; reachable at all times | a 5-shot arc, a rocket volley (marked lobs), calls the AA | big flash on the nose; rocket marker | 30 / 26 / 22 / 18 / 16 | 1000 | weak point opens at 30-45 s; fully open by 90 s |
| **Fire** (W4, W2 M6) | a timer on a rescue | n/a | grows through 4 stages | none | visible flames, a bar on the HUD | n/a | n/a | rescue faster; the timer is at least 90 s on normal |

Timers on rescue sites: at most two a mission, never in World 1; generous (90 s on normal; 120 / 105 / 90 / 75 / 60 by
skill); shown on the preview card and on the HUD. Losing a site only loses the people still there.

**Wind gusts** (W2 M5 on, W3, W4): a gust applies 0.25 px/frame sideways for 3 s every 12-20 s; the tell is 50
frames before: the windsock turns, spray or snow streaks, a rising howl. No gusts while landed; none at all on skill 1; within
100 px of a barracks on skill 2-3. The pad logic gives a 6 px landing tolerance that is reduced by wind never (it stays 6).
**Built 2026-11-21 (SPEC v1.21, slice B):** direction +x, 3 s = 150 frames, a gap of 600-1000 frames, the first not before 18 s into a mission, none while landed or in the 2 s after take-off, a landing cancels it; the tell is the windsock only (no howl: no sample is drawn, none added); one sock beside each island's pad.

**Winch rescue** (W2 M5 on): at a ledge, a ship deck or a rooftop you cannot land on, hover in a 24 x 20 px box above
the site for the climb: 8 people take 8 s on normal (the climb is paused, not reset, if you drift out). A rope and ladder
are drawn from the skids. Counterplay is to clear the site first (section 4.2); a winch hover is a landed-class state
for R1 and R3 (it counts as a stationary target after the dwell).

**Landing aids** (W2 on): pad width by skill 96 / 80 / 64 / 56 / 48 px [E]; touch-down tolerance of 6 px outside the
pad edge; on skill 1-3 an assist that nudges you 0.5 px/frame toward the pad's centre during the last 12 px of descent;
the pad shows two blinking lamps (green once your skids are over it, red when not), your shadow falls on it, and the
HUD shows a PAD tick. Non-landable surfaces (slopes, water, narrow ledges) are floors you stop on but do not board from.

---

## 5. The finale and the ending (proposed SPEC section 10)

### 5.1 The finale (M12 "Last Light")
- Two rooftop sites (32 people), then the HQ roof with 32 more (two trips of 16), guarded by the gunship.
- Preceding quiet: 5-7 s of flight with only the base to see.
- The gunship (section 4.4) patrols the roof line. Phase 1 (0-40 s): strafes and calls AA; you rescue while it harasses
  you (R1 and R3 apply to its attacks on a landed chopper). Phase 2 (at 40 s): it launches marked rockets (30-frame tell) and drops to roof height. Phase 3 (at 90 s at the latest): its core is exposed and it hovers in range.
- Reward: a final bonus, the ending.

### 5.2 The animated ending (closes SPEC gap)
A 25-35 s sequence built from what the engine already does (scrolling bands, sprites, copper sky), plus two large stills:
the chopper flying home over the sea at sunrise, a crowd at the base, a final shot of the hangar and flag.

| outcome | saved of 768 | sequence |
|---|---|---|
| **Full rescue** | 90% or more | a sunrise flight, the crowd cheers and waves, the flag raised, a fanfare, then the credits roll over the dawn |
| **Partial** | 60-89% | dusk, a quieter return, empty seats, the names of those saved scroll |
| **Costly** | under 60% | night, a lone chopper, a memorial and a silent beat; the credits roll over the night sea |

(A game over with no lives left is not an ending: it goes to the hall of fame and the title, as today [S].)

### 5.3 Credits
- A credits screen (the `modcredits` pattern [M]): code, art, music and sound credits; the art tools and models used
  (Retro Diffusion for the bands, our own hand-finish [S]); every third-party music or art with artist and licence.
- Music today is our own and plays outside play only [S]. Plan: a theme per world shown on its world card (our own, ~20 KB
  each [E]), an ending theme and a credits theme. If third-party modules are used, they are credited with their licence.
- Credits are reachable from the title (a page in the attract loop) and after the ending.

---

## 6. Tech feasibility per world

Kit note: the `perspectivefloor` technique is still being captured in the kit [M: no directory under `techniques/` yet], so the plan relies on this game's own `logicFloorScroll` and `docs/plain-spec.md`.

What the engine does **today** [M from the code, with line numbers at `main` 88e9bfd]:
- **The terrain is flat.** `GROUND_Y` 236 is the only floor; `FEET_Y` 240 is where people and tanks stand (`logic.h`).
- **Landing** is a function: `floorAt(g, x, &isGround)` in `logic.c` returns the floor. It knows two exceptions:
  barracks roofs and the hangar roof (not landable) [M]. Landing happens when the skids reach that floor and `isGround`.
  **This is the natural hook for a height map.** Bombs also use it.
- **The map is not stored.** There is no tile map. A world is rules plus four looping 320 px parallax bands that
  repeat; barracks positions are one array, `s_pBarracksX = {420, 1080, 1700, 2240}`; tanks, mines and jets spawn by
  timers [M, S].
- **The camera** moves in x only; `y` is a screen coordinate. Flyable height is 192 px: `CEIL_Y` 44 to `GROUND_Y` 236 [M].
- **A few rules use absolute heights** and must become "altitude above the local floor": a tank fires when your skids
  are y > 150, mines never go below y 200, the shadow is drawn at `SHADOW_Y`, people stand at `FEET_Y` [M, S].
- **The copper list** is 496 entries allocated, 465 used [S]. **The frame** has a worst case of 80% [S].
- **The radar** draws the whole battlefield at `ROUTE_SCALE` 12 into 256 px [M]; a longer map needs a per-world scale.

### 6.1 The new pieces

Tier: **hw** = amiga-hw (copper, blitter, sprite DMA, timing, assembly); **game** = amiga-game (logic in C, tools,
integration, tests); **art** = amiga-art. Size: S about 1-3 days, M about 3-8 days, L about 8-15 days [E].

| # | piece | tier | size | notes |
|---|---|---|---|---|
| T1 | World data and mission table (world, sites, palettes, lengths, garrisons); `MISSION_COUNT` 3 to 12 behind a flag | game | M | one record per mission; goldens unchanged for world 1 |
| T2 | **Per-world asset arena and loader**: world file (packed bands, BOB sheets, terrain tiles, map) loaded from floppy into one reused chip arena | game (loader, via `loading` technique) + hw (arena sizing) | M | the central memory rule (section 7) |
| T3 | **Band layout table**: the copper builder reads each world's band lines, heights, speeds and palettes from a table instead of constants | hw | M | needed for every world; refactor first with world 1 as the test |
| T4 | **Height map terrain logic**: `floorAt` / `ceilAt` from a per-world map (columns of 16 px, height, flags landable / wall), slope handling, wall bump, enemies and people follow terrain, the altitude-above-floor refactor, shadow on terrain, shots stop on terrain | game | L | many call sites; unit tests first; golden-neutral on a flat map |
| T5 | **Terrain strip renderer**: a 1:1 ring buffer in PF2 streamed one 16 px column at a time (about 1 blit per 5 frames at 3 px/frame), the copper switches PF2 to it at the world's top line, COLOR00 haze gradient behind it | hw | L (spike M) | uses the `scrolling` and `longmap` ideas; PF2 is one bitmap per line so the strip replaces hills and plain on its lines (section 2, world 3 note); **spike first** |
| T6 | **Terrain tools**: height map plus tile ids to packed map (`tools/mkterrain.py`), a preview/viewer, tile bank checks (top-10 coverage, one-offs) | game | M | designer-facing; playbook says "the editor first" |
| T7 | **Water**: re-cut perspective floor as sea, shimmer by time via BPLCON1 in the floor lines; sea gradient | hw | S-M | the floor infrastructure exists (`logicFloorScroll`) [S] |
| T8 | Boats: gunboat, destroyer (turrets as parts) | game M + art M | M | on-water movement over sea segments |
| T9 | **Pads and the landing guide**: pad objects, tolerance, assist, lamps, HUD tick | game | S-M | art S |
| T10 | **Barracks at varied heights**: barracks BOB y from the map, people run along terrain, roofs follow | game | S | falls out of T4 |
| T11 | **Winch**: hover box, rope and ladder BOB from the skids, climbing people, pause rule | game M | M | art S-M; no new chopper sprite poses needed |
| T12 | **Wind gusts** | game | S | art S (sock, spray) |
| T13 | **Radar sites and jet zones** | game | S-M | the jet budget per trip [S] stays as the cap |
| T14 | **Time-pressure rescues** (fire, sinking) | game M + art M | M | HUD bar |
| T15 | **Ground threats**: missile truck, AA gun, flak, markers, the shared token, garrisons, zone clear | game | M | art M; tell and fairness tests per weapon |
| T16 | **Enemy helicopters and the gunship** | game | M-L | art M-L (gunship: a static body with moving parts) |
| T17 | **Cloud-ceiling strip and `ceilAt`**, **tunnel** | hw (strip) + game | M | ceiling strip is a 1:1 copy of T5 for the top lines; the tunnel is a ceiling and floor in the one terrain strip |
| T18 | **Searchlights and night beams** | hw | L | beams are the "copper showcase"; the copper list is nearly full (465 of 496) so this needs a bigger list (about 3 KB chip more [E]) or a fallback of beam BOBs/sprites; spike first |
| T19 | **City world**: skyline strip art, rooftop pads, canyon streets, level scripts | art L + game M | L | no vertical scroll in the base plan (Q5) |
| T20 | **Long maps**: per-world radar scale, 16-bit positions (already int16 up to 32767 [M]), spawn by map position | game | S-M | |
| T21 | **Scripted set pieces with `wavescript`** and its pacing checker | game | M | not used today [S] |
| T22 | **World cards, route map across 12 missions, disk-swap screens, save file** (`missionmap`, `loading`, `hiscore` ideas) | game | M | |
| T23 | **Animated ending, three outcomes, credits screen** (`modcredits`) | game M + art L | L | closes SPEC gap 8 [S] |
| T24 | **Upgrades hangar screen and the four upgrades** | game | M | art S |
| T25 | **Program diet**: move resident title/world art and samples out of the executable to disk; keep the executable at 430 KB or less | hw + game | M | for the AROS boot window and chip headroom |
| T26 | Perf budget for terrain, more units, long maps; 512K option kept open | hw | M | measured, not guessed |

### 6.2 Feasibility by world (honest)

| world | what works now | what is new work | verdict |
|---|---|---|---|
| 1 Desert | everything | T15 (trucks, AA, garrisons), T21 (convoy) | low risk |
| 2 Islands | the perspective floor as a sea, the bands, `floorAt` hook | T3, T4, T5 (low strip), T7, T8, T9, T11, T12, T13, T14 | medium; the terrain strip is the risk, but islands are low (48 rows) so it is the easy case of T5 |
| 3 Highlands | the hook, the band stack | T4, T5 (tall), T17 (ceiling strip and tunnel), T10, winch ledges | high: the tall strip is the technical heart (about 125 rows of PF2 at 3 planes, 17 KB, plus its streaming) |
| 4 City | all of the above | T18 (beams), T19, T16 | highest on art and on beams; the city's code is the others' code |

Honest limits: terrain in PF2 means no hills or plain behind it on its lines; mountains up to about 110 px; no vertical
scrolling; the city's towers are limited to the same height; searchlight beams over buildings may have to be BOBs, not
copper (fallback agreed up front).

---

## 7. The memory and disk plan

### 7.1 Measured facts
| fact | value | |
|---|---|---|
| world | 3072 px, 4 barracks, flat | [M] |
| flyable height | 192 px (44 to 236) | [M] |
| executable | 420,832 B on disk; text and read-only data 397,540 B; bss 34,100 B | [M] `build/first`, `first.map` |
| files on the floppy | `dpslogo.bin` 37,456; `titlelogo.bin` 14,464; `titlelogo8.bin` 25,664; `plainfloor.bin` 4,560 B = 82,144 B | [M] |
| used on disk | 503 KB of the floppy's 880 KB; **free about 377 KB** (the ADF is 901,120 B; filesystem overhead ignored) | [D] |
| chip RAM lowest free in play | a500 **81 KB**, a500-ks31 79 KB, a500-aros 154 KB, a1200 1,070 KB; the test asserts at least 64 KB | [M] latest `perf` serial logs (profile order read from the values; SPEC shows 85 / 83 / 158 earlier) |
| slow RAM free on the a500 profile | about 3 KB | [M] (same logs) |
| memory the game needs | 1 MB (512K chip + 512K slow); a 512K machine fails with error 103, 29,904 bytes short for the executable alone | [S] |
| AROS boot | first picture at frame 2938 of a 3000 window, so **62 frames, about 6 KB of executable growth** | [S] |
| copper list | 496 allocated, 465 used | [S] |
| frame | worst 80%, average 35% | [S] |

### 7.2 The rules
1. **The executable does not grow.** New code goes in, but it is paid for by moving resident art and sound to disk
   (T25), so the AROS first picture stays inside its window. If it cannot, the harness window for the AROS profile may be
   raised (a test setting, not a game limit); this is a decision for the coordinator, not Paul.
2. **One chip arena** is loaded per world and freed on leaving it. The arena contains: the world's four to six band
   bitmaps, its BOB sheets, its terrain tiles and ring buffer, its map. It must fit in what World 1 uses today plus
   at most 15 KB (the 81 to 64 KB margin) [D]. Sizing per world:

| world arena | parts (estimated) | chip [E] |
|---|---|---|
| 1 | bands 25 KB (5 bands of 320 px, 3 planes), floor 4.6 KB, BOBs | as shipped (stays in the executable until T25) |
| 2 | bands 20 KB, sea floor 5 KB, terrain tiles 12 KB, ring 4 KB, BOBs (boats, pads, gunboat, flak, wind) 20 KB | about 61 KB |
| 3 | far range 7 KB, ceiling strip 5 KB, terrain ring 17 KB, tile bank 25 KB, floor 4.6 KB, BOBs 15 KB | about 74 KB |
| 4 | skyline 15 KB, terrain ring 17 KB, tile bank 25 KB, road floor 4.6 KB, BOBs (helicopters, gunship, towers, beams) 25 KB | about 87 KB |

The estimates for worlds 3 and 4 are over the margin. Three levers, in order: (a) T25 moves World 1's art out of the
executable, so the arena can be sized to the biggest world and the slow RAM is used for the unpacked source;
(b) shrink the tile banks (16 px columns, 100 to 150 columns of art, flips); (c) accept a lower chip floor (the 64 KB
assertion was chosen for Kickstart 3.1; it can drop to about 40 KB if the profiles still boot [E]). Measuring this is the
first spike, before any art is made.
3. **Per-world load: 150 KB packed or less.** At 11-15 KB/s [playbook], 150 KB loads in 10-14 s, behind the world card [E].
4. **Slow RAM is nearly full (3 KB free [M])**: the loaded world's unpacked source and the maps live in chip or are
   decoded straight into place; anything else needs the T25 diet.

### 7.3 Two disks (no world spans a disk)

| disk | contents | size [E] |
|---|---|---|
| **Disk 1 (boot)** | executable (421 KB) + title and logos (82 KB) + world 1 (resident, or 150 KB if moved by T25) + world 2 (150 KB) + hall of fame, save | about 650-800 KB of 880 |
| **Disk 2** | world 3 (150 KB) + world 4 (150 KB) + ending (60 KB) + credits and themes (80 KB) | about 440 KB of 880 |

If worlds 3 and 4 come in over 150 KB each, or music grows past the 100 KB budget, the fallback is **three disks**
(one world per disk after the first); the plan does not plan for it but does not rule it out. A hard-drive install
works without change because files are loaded by name through AmigaDOS (the `loading` technique [M]).

**Swap points.** After mission 6 ends (World 2 complete), "INSERT DISK 2" on the world card before world 3 loads; the
game checks the volume name, never opens a requester. At game over or after the ending, "INSERT DISK 1" before the hall
of fame is written (it lives on disk 1; a write-protected disk is handled as today [S]). One swap in a full run, two with
the hall of fame.

**Single drive, no memory expansion:** one swap for a normal run. **AROS:** all loads happen after the first picture,
so the boot window is unaffected, but the test profiles need a world 3 and 4 image to test against (a second ADF in the harness).

**Continue.** A checksummed save file (like the hall of fame: a world, mission, lives, merit, upgrades; about 24 bytes)
written at each world card, so a long game can be resumed (Q3).

### 7.4 Chip use against the 1 MB target
The game keeps its **1 MB minimum** (512K chip + 512K slow) [S]; a 512K machine is not a goal of this plan, but the arena
design is the route to it later: the executable shrinks and the world is loaded on demand. Required: lowest chip free
in play at least 64 KB on the a500 and a500-ks31 profiles, as asserted today [S], re-measured per world.
World 2 with its jets (v1.16): a500 97 KB free in play (98 before the jet's 1,152 bytes; the gates assert 96), 74 KB with WinUAE's 23 KB less chip
modelled (`tests/chipwinuae.agk`, Ctrl+L Q). Radar, windsock and destroyer for missions 5 and 6 still need the levers of STYLE-world2 section 6 (pad widths by skill -3,040
at skill 3, one destroyer facing -5,280, a mission's own set) [M].

**Update, 2026-11-21 (SPEC v1.20, the slice 2 branch `w2-slice2`).** A bigger lever than those three was sitting in the program: the boot splash's DPS logo (37,456 bytes) and its two copper lists, 47 KB of chip, were held for the whole game.
Freed at world 2's load they give world 2 **147,384 bytes free in play on the a500 (was 99,544: +47,840)**, world 1 unchanged at 105 KB [M: `AGK w2 chipbytes`, `tests/w2chip.agk`]. The sets still to come (radar 5,616, windsock 480, destroyer 5,280 and turret 1,824 for one facing, whitecaps 1,080, plus the ship deck and the freighter
once drawn, about 3-5 KB each [E]) add up to about 22-26 KB, so the floor stays at 96 KB for the a500 and 64 KB under the squeeze with 20 KB or more to spare; none of the three plan levers is needed. If it is not enough once the deck and the freighter are drawn, the options with numbers are the pad widths by
skill (-3,040 at skill 3, -4,144 at 4-5, not at skill 1) and the title logo and its lists freed in play (26 KB, a reload at the title). The floor is never lowered without Paul.

---

## 8. The art plan

The standing rules apply to every world: **no visible bands** (the seams between band switches show only the sky
gradient or the shared colour C); the **perspective-floor standard** (each line at its own speed; flat or
perspective-drawn features only; art contract style in `docs/plain-spec.md`); **no flat-topped cut-offs** (every
silhouette stays inside its band or strip with transparent margin; crest heights vary; peaks are pointed or irregular,
never ending at the top row of a strip); outlines on what moves; light from the top left.

### 8.1 Gate order for every world
1. **STYLE addendum** (`docs/STYLE.md`, which should now be added; SPEC open question 6): palette slots, band palettes,
   the terrain tile rules (16 px columns, how tiles join, top-edge treatment), enemy silhouette notes, the prompt base.
2. **A reference mock-up screen** in the game's palette and a scrolled strip at 8-16 camera offsets, shown to Paul for approval.
3. **A pilot set**: the bands, 6 to 10 terrain tiles, the world's hero objects (barracks variant, pad, the new enemies).
4. Paul approves the pilot. Only then the full pass.
5. Seam and band checks (`parallax_check.py`-style, 12 offsets per mission) and a 3x view at the real size.

### 8.2 Per world

| world | addendum content | pilot | mock-up for Paul |
|---|---|---|---|
| 1 | trucks, AA gun, flak puffs, markers, pad lamps (small) | truck, AA gun, marker, flak | one mission-2 night shot with a garrison |
| 2 | sea floor in perspective, island tiles (sand, palm, rock, surf), reef, pad with lamps, gunboat, destroyer, windsock, spray | sea floor strip, 8 island tiles, the pad, the gunboat | the island approach with the landing guide, at three camera offsets |
| 3 | rock and snow tiles (slopes 0/15/30/45 degrees, ledges, summits, tunnel mouth), the cloud ceiling, haze behind the strip | 10 terrain tiles, a summit pad, a ledge, the far range | a pass, a summit pad and the cloud ceiling together; a check of the strip's top edge for flat tops |
| 4 | the skyline strip, rooftop tiles, lit windows, canyon streets, searchlights, fires, the helicopter and gunship | 10 tiles, rooftop pad, searchlight tower, helicopter | a night dock with a beam |
| ending | three key stills and the crowd | stills | storyboard (cards) first |

### 8.3 Sources and licences
Retro Diffusion for the bands (as the first pilot [S]), our own hand-finish for everything that moves, no other game's
art as input. Generation costs are logged per world as in the first pilot ($1.72 of $2.27 [S]). Each world's generation
log goes in `NOTES.md`; credits list the tools.

---

## 9. The production plan

### 9.1 Order of work
The rules are TBD: small commits, behind a disabled flag, merged within one or two days.

| phase | work | exit gate |
|---|---|---|
| **0 Spikes and mock-ups** (about 1-2 wk [E]) | (a) hw spike: the terrain strip (tall and low) and the band table (T3, T5) on a throwaway map; (b) hw spike: chip arena sizing and the copper list size; (c) art mock-ups for worlds 2 and 3 for Paul; (d) a paper level for M4 (map, sites, garrisons) | the strip streams without a dropped frame; the arena numbers confirm or change section 7 |
| **1 Foundations, world 1 unchanged** (about 1.5 wk) | T1, T2, T3, T4's flat-map case, T20, T25, the altitude refactor | **every existing golden unchanged** on all four profiles; chip and AROS numbers recorded |
| **1b Ground-threat loop in world 1** (about 1 wk) | T15 (truck, AA, garrisons, tokens, markers), T21 for the convoy | playtest gate A: Paul on WinUAE: do the garrisons read as fair? |
| **2 World 2, the vertical slice** (about 4 wk) | M4 first: T5 (low), T7, T8, T9; then M5 (wind, winch, radar), M6 (timers, destroyer) | playtest gate B: M4-M6 end to end with a world card, a pad guide, a swap |
| **3 World 3** (about 3 wk) | T4 slopes and walls, T5 (tall), T17, T10, ledge winches, helicopter, the tunnel | playtest gate C |
| **4 World 4, finale, ending** (about 3.5 wk) | T18, T19, T16 gunship, T23, T24 | playtest gate D |
| **5 Disks, polish, release** (about 2 wk) | T22, the second disk image, the save, credits, AROS, perf, Prove It list | all gates green; a release candidate for Paul |

About 15 weeks in all [E, plus or minus 40%]. Art and code run in parallel; art is the long pole (worlds 3 and 4).

**Phase 2, slice 2 (missions 5 and 6 finished), order of work (SPEC v1.20, section 11 "World 2, slice 2"; branch `w2-slice2`).** Each slice is merged-quality on its own (unit tests red first, a scenario, goldens re-recorded in one call after every diff is read, the four-profile gate):
A radar sites and the jet zone rule (zones red on the HUD radar and the card's strip map); B wind gusts and the windsock; C the ship-deck winch rescue (mission 5: needs a ship deck and a rope drawn first); D the timed freighter (mission 6: needs the freighter drawn; at most two timed sites a mission,
90 s on normal, 120 / 105 / 90 / 75 / 60 by skill); E the strait destroyer set piece with its anti-stall (barrage at 40 s, turrets open by 90 s) and the zone bonus. Mission 4 and world 1 do not change. The memory budget is in section 7.4 (the boot splash's 47 KB freed at world 2's load pays for all five).

**Why world 2 is the vertical slice.** It exercises every new thing in its easiest form: the terrain strip at its lowest, the
sea (cheapest floor), pads (Paul's #1 and #4), boats (#3), the first winch and wind, a world card, a swap. World 3's
tall terrain is the riskiest technology, so its spike is in Phase 0 rather than waiting until the slice is done.

### 9.2 Milestones and playtest gates
Each gate is a build Paul plays, with a short observation list (feel of pads, tells readable, length of the mission,
whether the garrison loop is fun). Complaints become tuning tickets, as for jets [S].

### 9.3 Risks

| risk | likelihood | impact | mitigation |
|---|---|---|---|
| terrain strip does not fit the copper list, bus or frame | medium | high | Phase 0 spike; fall back to a lower strip and a smaller map |
| chip arena over budget for worlds 3 and 4 | high | high | T25 diet; smaller tile banks; a lower chip floor |
| AROS boot window | high | low (a test setting) | T25; decide the harness window |
| mountains look like floating cut-outs (flat tops, bands) | medium | high | mock-up approval, haze gradient, a top-edge check script |
| searchlight beams cost too much copper | high | medium | BOB fallback; world 4 ships without the copper version if needed |
| the shared fairness rules become too complicated to tune | medium | medium | one token, one table; unit tests per weapon; ghost-mode pacing bot |
| 80 min of play with 3 lives is too hard | medium | high | Q3: lives refill by world, continue file |
| scope: twelve missions plus eight studio additions | high | high | the phase gates; features ship behind flags; world 4's city can lose its beam effect and fires without losing the world |
| art cost and consistency across worlds | medium | medium | the gate order above; the first pilot's process worked |

---

## 10. Open questions for Paul: answers (2026-11-12)

Paul approved the plan with the studio's recommended answers. **Decided by Paul** means he chose it; **studio default**
means the plan's own proposal stands until a playtest gate says otherwise.

| # | question | answer | status |
|---|---|---|---|
| 1 | twelve missions in four worlds | yes, 12 missions in 4 worlds | decided by Paul |
| 2 | world order | desert, islands, mountains, city | decided by Paul |
| 3 | lives and continues | lives refill to 3 at each world start, plus a continue (a save at each world card) | decided by Paul |
| 4 | upgrades: the four, the currency, skill 1 starting armour | armour, winch, seats, cannon; merit from people saved plus medals; skill 1 starts with armour level 1 | studio default, revisit at a playtest gate |
| 5 | vertical scrolling | none in the base plan; peaks and towers stay on screen (about 110 px of 192) | decided by Paul |
| 6 | a mid-map field pad (repairs, no unloading) on maps over 4000 px | yes | studio default, revisit at a playtest gate |
| 7 | terrain contact | speed loss and a spark only, no damage | decided by Paul |
| 8 | sea contact | a soft stop and a splash, no damage | decided by Paul |
| 9 | world and mission names; renaming today's missions 2 and 3 | the names in section 3; Desert Night and Desert Dawn | studio default, revisit at a playtest gate |
| 10 | ending thresholds, tone, a villain | 90% and 60%; as in 5.2; no named villain | studio default, revisit at a playtest gate |
| 11 | music | own themes on world cards, ending and credits; none in play; third-party only with artist and licence credited | studio default, revisit at a playtest gate |
| 12 | the tunnel | kept (world 3) | studio default, revisit at a playtest gate |
| 13 | memory | stay at 1 MB; raising the AROS test window slightly is allowed if needed | decided by Paul |
| 14 | disks | two disks with one swap; a hard-drive install as a later extra | decided by Paul |
| 15 | scoring caps | zone-clear bonus 100; the tank quota stays 10; trucks, AA and boats pay for their first 6 kills a mission | studio default, revisit at a playtest gate |

---

## 11. Proposed SPEC text

> **APPROVED by Paul, 2026-11-12; applied to `docs/SPEC.md` v1.5.** This is the draft as written; the SPEC text is the
> contract and has been adjusted for fit (it keeps the shipped description of world 1 under 4.6).

### 11.1 Section 4. Structure and content budget (proposed)

**Four worlds, twelve missions.** Each mission has 64 people (the finale has two trips at its last site). A mission ends
when everyone is saved or lost. Worlds are separate battlefields with their own parallax, terrain and enemies.

| mission | world | name | length (px) | sites | new element |
|---|---|---|---|---|---|
| 1 | Border Desert | Desert Dusk | 3072 | 4 barracks | (as before) |
| 2 | Border Desert | Desert Night | 3072 | 4 barracks | missile truck, garrisons |
| 3 | Border Desert | Desert Dawn | 3072 | 4 barracks | AA gun, convoy set piece |
| 4-6 | Coral Islands | Reef Run, Trade Winds, Typhoon Strait | 4608 | 4 sites | water, island pads, gunboats, wind, winch, radar, timers, destroyer |
| 7-9 | Highlands | Foothills, High Pass, Cliff Ledges | 4096 | 4-6 sites | height-map terrain, ceiling, tunnel, summit pads, ledge winches, helicopter duel |
| 10-12 | Harbour City | Night Docks, Burning Block, Last Light | 3584 | 4-6 sites | rooftop pads, searchlights, fires, helicopters, gunship finale |

- **Terrain:** each world carries a height map in columns of 16 px: surface height, flags (landable, wall, water, tunnel),
  and a ceiling for the ceiling and tunnel stretches. `floorAt` and `ceilAt` read it. Terrain never damages.
- **Streaming:** each world is a packed file of at most 150 KB, loaded into one chip arena at its world card.
  The executable does not grow (target 430 KB or less).
- **Disks:** two. Disk 1: boot, title, worlds 1 and 2, hall of fame. Disk 2: worlds 3 and 4, the ending, credits.
  One swap at the end of mission 6.
- **Tile statistics:** per world about 100-150 terrain columns (16 px), top-10 columns cover 55% or more of the map,
  at most 40 one-offs; map about 1.2 KB a world [E].
- **Pads and sites** have a width by skill (96 / 80 / 64 / 56 / 48 px), a 6 px tolerance and a landing guide.
- **Ground threats** obey the rules R1-R9 of section 6 below.

### 11.2 Section 6. Enemies and bosses (proposed)

The table of section 4.4 above (tank, jet, mine, barracks, missile truck, AA gun, gunboat, radar site, searchlight tower,
enemy helicopter, destroyer, gunship, fire), with:
- the **shared ground-grace table**: never / never / 6 s / 4 s / 3 s, for every weapon that can hit a landed chopper;
- the **landed-chopper token**: one attack at a time on skills 1-3, two on skills 4-5, at least 3 s apart;
- **garrisons**: fixed units near each site; clearing them is a zone clear;
- **tells** in frames by skill for each enemy as in section 4.4; a unit test per weapon: no shot, lob, flak or rocket without its tell;
- **Set pieces and bosses**: M3 convoy, M6 destroyer, M9 helicopter duel, M10 lit gauntlet, M12 gunship; all with
  anti-stall escalation at 30-45 s and the weak point open by 90 s.

### 11.3 Section 10. Presentation (proposed)

| item | state |
|---|---|
| ending | an animated ending in three outcomes (90% / 60% of 768 saved), 25-35 s each, then the credits |
| credits | a credits screen (code, art, music, tools, third-party licences), reachable from the title and the ending |
| mission flow | a world card (loading, disk request, the world's theme) and a mission preview card with a strip map for the whole world, a route map across the twelve missions, and one line of brief |
| between missions | a hangar screen for upgrades |
| continue | a save at each world card |
| everything else | as shipped: boot logo, title, attract loop, hall of fame, options |

---

*End of the plan.*

**Built 2026-11-21 (SPEC v1.22, slice C, mission 5):** one ship deck (16 people, in place of the third island's barracks), box 24 x 20 above the flat deck edge, 50 frames a person on every skill, paused not reset, landed-class for R1-R3 and the gusts; the art is `art/world2/deck_ship.txt`, `winch_rope.txt`, `winch_hook.txt`, `man_climb.txt`.
