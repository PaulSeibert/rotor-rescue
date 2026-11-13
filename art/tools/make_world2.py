#!/usr/bin/env python3
"""Builds the World 2 "Coral Islands" production art (missions 4-6: Reef Run, Trade Winds, Typhoon Strait) into art/world2/.

The art is drawn by the generators in art/tools/world2/ (the approved pilot's code plus the cloud and facing changes),
from the Retro Diffusion raws in art/world2/raw/ (far islands, palms). This writes it in the formats agk's converter takes,
as art/disk_art.toml describes them:

  art/world2/*.txt            text art: PF2 bands and the tile bank (pens 1-7, `bitmap`), PF1 BOBs (letters, `bob`)
  art/world2/palettes/*.txt   the per-mission PF2 palettes, agk palette-file format (clouds, far, sea, island x 3 missions)
  art/world2/palettes.toml    per mission: sky top and low (haze H), deep sea C, sky dots, AGA colours
  art/world2/tilemap.bin      the 288-column map (one byte per column: an index into terrain_tiles.txt's frames)
  art/world2/tilemap.txt      the same, readable, with the island table
  art/world2/world2_art.toml  the asset table (disk_art.toml's format) to merge into art/disk_art.toml at integration

    ~/amiga-tools/venv-img/bin/python art/tools/make_world2.py            # write the files
    ~/amiga-tools/venv-img/bin/python art/tools/make_world2.py --check    # exit 1 if they are stale
    ~/amiga-tools/venv-img/bin/python art/tools/make_world2.py --convert  # also run agk's converter on them

Needs numpy and PIL (the img venv); --convert also needs agk's harness (as make_disk_art.py).
"""
import os
import re
import shutil
import sys
import tempfile
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "world2"))
import numpy as np                                     # noqa: E402
from w2compose import *                                # noqa: E402,F401
import w2bobs as B                                     # noqa: E402

LETTERS = "KOSWRYB"
PILOT_CHIP, PILOT_DISK = 77440, 32.9 * 1024            # the approved pilot's total (coral-islands-sizes.txt)
HULL_CANVAS_W, HULL_BASE, HULL_OX = 170, 42, 4         # w2bobs.destroyer's canvas


# ------------------------------------------------------------------------------------------------ the frame sets
def boat_sets():
    """every directional BOB, as {asset: (frames, comment)}; left-facing first, then right-facing (drawn facing right and
    re-lit, see w2bobs.C): the light stays top left in both. Also the crop offsets the destroyer's mounts need."""
    sets = {}
    p = []
    for flip in (False, True):
        fr, _ = B.common([B.patrol(w, flip) for w in (0, 1)])
        p += fr
    sets["patrol_boat"] = (p, "Patrol boat, 2 wake frames x 2 facings: frames 0-1 face left, 2-3 face right (re-lit, not mirrored).")
    g = []
    for flip in (False, True):
        fr, _ = B.common([B.gunboat(s, w, flip, crop=False) for s, w in (("idle", 0), ("tell1", 1), ("tell2", 0), ("fire", 1))])
        g += fr
    sets["gunboat"] = (g, "Gunboat, 4 states x 2 facings: frames 0-3 face left (idle, tell 1, tell 2, fire), 4-7 face right.\n"
                          "The tell is the barrel rising (8, 28, 42, 42 deg) and the red mast lamp; the flash comes with tell 2 and fire.")
    hulls, hoff = [], []
    for flip in (False, True):
        fr, off = B.common([B.destroyer(flip=flip)])
        hulls += fr
        hoff.append(off)
    assert (hulls[0].w, hulls[0].h) == (hulls[1].w, hulls[1].h)
    sets["destroyer_hull"] = (hulls, "Destroyer hull, bridge, funnels, AA mounts; frame 0 faces left, frame 1 faces right.\n"
                                     "Cut into three BOB sections when blitted: bow 64 / mid 64 / stern 32 px (the bow is at the right for frame 1).")
    t, toff = [], []
    for flip in (False, True):
        fr, off = B.common([B.turret(s, flip) for s in ("idle", "tell1", "tell2", "fire")])
        t += fr
        toff.append(off)
    sets["destroyer_turret"] = (t, "Destroyer turret, 4 states x 2 facings: frames 0-3 face left (idle, tell 1, tell 2, fire), 4-7 right.\n"
                                   "One art, two mounts (forward and aft); the tell is the barrels rising and the housing glow.")
    return sets, hoff, toff


def turret_mounts(hoff, toff):
    """where a turret frame's top-left goes on the hull frame, per facing and mount (px). The turret's 15 px housing sits
    on the hull's turret bases (forward: hull canvas x ox+8..ox+22, aft: ox+138..ox+150) with its foot on the base's
    second row."""
    ox, base = HULL_OX, HULL_BASE
    out = {}
    for flip in (False, True):
        hx0, hy0 = hoff[1 if flip else 0]
        tx0, ty0 = toff[1 if flip else 0]
        for nm, (b0, b1) in (("forward", (ox + 8, ox + 22)), ("aft", (ox + 138, ox + 150))):
            centre = (b0 + b1) / 2.0
            hx = HULL_CANVAS_W - 1 - centre if flip else centre              # base centre in final hull-canvas px
            # housing in the turret canvas: x 18..32 (unflipped) or 1..15 (flipped), centre 25 or 8; foot row 17
            hcx = 8 if flip else 25
            ox_canvas = int(round(hx - hcx))
            oy_canvas = (base - 14) - 17
            out[("right" if flip else "left", nm)] = (ox_canvas + tx0 - hx0, oy_canvas + ty0 - hy0)
    return out


def bob_sets():
    sets, hoff, toff = boat_sets()
    bf, _ = B.common(B.barracks_set())
    sets["barracks"] = (bf, "Island barracks, 52 x 32: 0 intact (closed, blue lamp), 1 shot up, 2-3 open and burning (flicker).\n"
                            "The engine's four barracks frames; the pilot drew closed and open, the other two are derived from them.")
    lit, dark = B.pad_parts("Y"), B.pad_parts("O")
    sets["pad_cap"] = ([lit["cap_l"], lit["cap_r"], dark["cap_l"], dark["cap_r"]],
                       "Landing pad end caps 8 x 46 (pylon with beacon): 0 left lit, 1 right lit, 2 left dark, 3 right dark.")
    sets["pad_slab"] = ([lit["chev_r"], lit["chev_l"], lit["plain"], dark["chev_r"], dark["chev_l"], dark["plain"]],
                        "Landing pad slab units 8 x 16: lit chevron right, chevron left, plain; then the dark phase of the same.")
    sets["pad_centre"] = ([lit["centre"], dark["centre"]], "Landing pad centre 16 x 16 (the H): lit, dark.")
    sets["pad_lamp"] = ([lit["lamp_off"], lit["lamp_on"]], "Pad status lamps 5 x 5: red (off the pad), yellow (over the pad). No green exists in PF1.")
    sets["radar"] = (B.common([B.radar(i) for i in range(4)])[0], "Radar site 34 x 40: dish turning (wide, mid, thin, mid); the red lamp blinks on even frames.")
    sets["windsock"] = (B.common([B.windsock(i) for i in range(2)])[0], "Windsock 16 x 22: limp, full (the gust tell: it fills 50 frames before).")
    sets["splash"] = ([B.splash(i) for i in range(5)], "Water splash 24 x 24, 5 frames (spike, column, crown, droplets, ring); no outline, a white fringe on the sea.")
    sets["bow_spray"] = ([B.wake_bow(i) for i in range(2)], "A boat's bow spray 12 x 8, 2 frames.")
    sets["whitecap"] = ([B.whitecap(i) for i in range(3)], "Storm whitecap 34 x 10, 3 frames (Typhoon Strait only).")
    for name, (frames, _) in sets.items():
        assert len({(f.w, f.h) for f in frames}) == 1, "%s: frames differ in size %s" % (name, [(f.w, f.h) for f in frames])
    return sets, turret_mounts(hoff, toff)


# ------------------------------------------------------------------------------------------------ writers
def txt_head(comment):
    return "".join("# %s\n" % l for l in comment.split("\n"))


def bob_text(frames, comment):
    out = [txt_head(comment), "colors\n  .  transparent\n"]
    for ch in LETTERS:
        out.append("  %s  0x%03X\n" % (ch, B.LET[ch]))
    for f in frames:
        out.append("frame\n")
        out += ["".join(r) + "\n" for r in f.a]
    return "".join(out)


def band_text(frames, pal, comment):
    out = [txt_head(comment), "colors\n  .  transparent\n"]
    out += ["  %d  0x%03X\n" % (i + 1, c) for i, c in enumerate(pal)]
    for a in frames:
        out.append("frame\n")
        out += ["".join("." if v == 0 else str(v) for v in row) + "\n" for row in a]
    return "".join(out)


def pal_text(pal, name, num, m):
    out = ["# World 2 %s palette, mission %d %s (%s). PF2 pens 1-7, dark to light; 0 is transparent.\n" % (name, num, m["name"], m["tag"]),
           "0  0x000  transparent\n"]
    out += ["%d  0x%03X  %s%d\n" % (i + 1, c, name[0], i + 1) for i, c in enumerate(pal)]
    return "".join(out)


SLUG = ["m4_reef_run", "m5_trade_winds", "m6_typhoon_strait"]


def outputs():
    W = World()
    files = {}
    m0 = MISSIONS[0]
    # ---- bands
    bank_names = list(W.bank.keys())
    files["terrain_tiles.txt"] = band_text([W.bank[n] for n in bank_names], m0["isl"],
        "Terrain tile bank: %d columns of 16 x 48 stacked (frame i = tile i; the order is tilemap.txt's index). PF2 pens 1-7 =\n"
        "the island palette (mission 4's colours here, the copper swaps each mission's). Shading is baked: no mirrored tiles." % len(bank_names))
    files["clouds.txt"] = band_text([W.clouds], m0["clouds"],
        "Clouds band 320 x 40 (lines 72-111, camera / 8). Rows 0-2 and 37-39 empty. Six masses with their own bases (rows 11-27).")
    files["far_islands.txt"] = band_text([W.far], m0["far"],
        "Far volcanic islands (rows 0-49) + far strip (rows 50-59), 320 x 60 (lines 120-179, camera / 4). Rows 0-2 and 59 empty.")
    wmax = max(len(r) for r in W.floor)
    src = np.zeros((28, wmax), np.uint8)
    full = np.zeros((28, 640), np.uint8)
    for j in range(28):
        r = W.floor[j]
        src[j, :len(r)] = r
        full[j] = np.tile(r, 640 // len(r) + 1)[:640]
    files["sea_floor_src.txt"] = band_text([src], m0["sea"],
        "Sea floor source, 28 rows x %d: row j uses columns 0 .. W_j-1 with W_j = 4 (28 + 3 j); the rest stays '.'. Rows 0 and 27 empty." % wmax)
    files["sea_floor.txt"] = band_text([full], m0["sea"],
        "Sea floor 640 x 28 (lines 180-207): each row of sea_floor_src.txt repeated every W_j px, as the plain's floor.\nGenerated; row j scrolls at (28 + 3 j) / 112 of the camera.")
    # ---- BOBs
    sets, mounts = bob_sets()
    for name, (frames, comment) in sets.items():
        files[name + ".txt"] = bob_text(frames, comment)
    # ---- the map
    idx = {n: i for i, n in enumerate(bank_names)}
    files["tilemap.bin"] = bytes(idx[n] for n in W.names)
    lines = ["# Tile map: 288 columns of 16 px = 4608 px, one byte each (index into terrain_tiles.txt). Same map in all three missions.\n",
             "# index  tile\n"]
    lines += ["%2d  %s\n" % (i, n) for i, n in enumerate(bank_names)]
    lines.append("# islands, in tile columns: first column, count, the 4 flat tiles under the barracks, the pad bed (pad width in px)\n")
    for o in W.objs:
        lines.append("island first=%d columns=%d barracks=%s pad=%s pad_px=%s%s\n" % (o["first_col"], o["columns"], o["barracks_col"], o["pad_col"], o["pad_px"],
                                                                                    " base" if o["base_island"] else ""))
    lines.append("# the map: tile index per column, 16 columns a line (first column number on the left)\n")
    for i in range(0, 288, 16):
        lines.append("%3d: %s\n" % (i, " ".join("%2d" % idx[n] for n in W.names[i:i + 16])))
    files["tilemap.txt"] = "".join(lines)
    # ---- palettes
    toml = ["# World 2 per-mission palettes (12-bit 0xRGB; the copper loads them, the band text art carries mission 4's for previews).\n",
            "# Pens 1-7 dark to light. Sky: a line-dithered copper gradient 'top' to 'low' (low = the haze H) on lines 24-179,\n",
            "# then H to C on lines 180-207 (the sea's depth), flat C on 208-255. AGA: the same gradients in 24 bits (aga_*).\n"]
    for mi, m in enumerate(MISSIONS):
        for key, nm in (("clouds", "clouds"), ("far", "far"), ("sea", "sea"), ("isl", "island")):
            files["palettes/%s_%s.txt" % (SLUG[mi], nm)] = pal_text(m[key], nm, mi + 4, m)
        toml += ["\n[%s]\nname = \"%s\"\nmood = \"%s\"\n" % (SLUG[mi], m["name"], m["tag"]),
                 "sky_top = \"0x%03X\"\nsky_low_haze_H = \"0x%03X\"\ndeep_sea_C = \"0x%03X\"\n" % (m["sky"][0], m["sky"][1], m["C"]),
                 "sky_dots = [%s]\n" % ", ".join('"0x%03X"' % c for c in m["dots"]),
                 "aga_sky_top = \"0x%06X\"\naga_sky_low = \"0x%06X\"\naga_deep_sea_C = \"0x%06X\"\n" % (m["aga"]["sky"][0], m["aga"]["sky"][1], m["aga"]["C"])]
        for key in ("clouds", "far", "sea", "isl"):
            toml.append("%s = [%s]\n" % (key, ", ".join('"0x%03X"' % c for c in m[key])))
    files["palettes.toml"] = "".join(toml)
    # ---- the asset table
    t = ["# World 2 (Coral Islands) art, in art/disk_art.toml's format. Not merged into disk_art.toml yet (integration is a separate step):\n",
         "# paste these tables there (the paths are relative to art/, as disk_art.toml's are). `make_world2.py --convert` runs agk's converter on this file.\n",
         "# The bands' `palette` is mission 4's; missions 5 and 6 load their own from palettes/ (copper moves, no new art).\n",
         "[art]\ndepth = 3\n"]

    def band(name, src, pal, wrap, note):
        t.append("\n[%s]                  # %s\nsource = \"world2/%s\"\nkind = \"bitmap\"\ndepth = 3\npalette = \"world2/palettes/%s\"\n%s" %
                 (name, note, src, pal, "wrap_x = 320\n" if wrap else ""))
    band("w2_tiles", "terrain_tiles.txt", SLUG[0] + "_island.txt", False, "%d tile columns 16 x 48 stacked (frame i = tile i), PF2 island pens" % len(bank_names))
    band("w2_clouds", "clouds.txt", SLUG[0] + "_clouds.txt", True, "320 x 40, camera / 8")
    band("w2_far", "far_islands.txt", SLUG[0] + "_far.txt", True, "320 x 60, camera / 4: the far islands + the far strip")
    band("w2_sea", "sea_floor.txt", SLUG[0] + "_sea.txt", False, "640 x 28, a loop per row (W_j = 4 (28 + 3 j))")
    for name, (frames, comment) in sets.items():
        t.append("\n[w2_%s]                  # %d frame(s) %d x %d\nsource = \"world2/%s.txt\"\nkind = \"bob\"\n" % (name, len(frames), frames[0].w, frames[0].h, name))
    t.append("\n# Destroyer turret mounts: the turret frame's top-left on the hull frame, in px (one turret art, two mounts).\n")
    for (facing, mount), (mx, my) in sorted(mounts.items()):
        t.append("# %s-facing, %s turret: x %d, y %d\n" % (facing, mount, mx, my))
    t.append("# Pad: assemble from 8 px units on a 46-row canvas: cap (rows 0-45), then slab units at row 30 (cap, chevron or plain units\n"
             "# pointing in, the 16 px centre H, units, cap): 48, 64, 80 or 96 px; lit and dark phases blink.\n")
    files["world2_art.toml"] = "".join(t)
    return files, sets, W


# ------------------------------------------------------------------------------------------------ sizes
def planar(a, planes=3):
    h, w = a.shape
    wb = (w + 15) // 16 * 2
    out = bytearray()
    for y in range(h):
        for p in range(planes):
            bits = np.pad(((a[y] >> p) & 1).astype(np.uint8), (0, wb * 8 - w))
            out += np.packbits(bits).tobytes()
    return bytes(out)


def sizes(files, sets, W):
    """chip bytes as the pilot counted them (planar, 3 planes for bands; BOBs 3 planes + mask, width in words + 1 shift word)
    and a packed-disk estimate (zlib -9 + 12 % for LHA on the planar data)."""
    rows = []

    def add(name, raw, frames=None, chip=None):
        rows.append((name, chip if chip is not None else len(raw), int(len(zlib.compress(raw, 9)) * 1.12), frames))
    add("terrain tile bank (%d columns of 16 x 48)" % len(W.bank), b"".join(planar(W.bank[n]) for n in W.bank))
    add("tile map (288 columns, 1 byte each)", files["tilemap.bin"])
    rows.append(("terrain ring buffer 352 x 48 (320 + 2 x 16 px), 3 planes", 352 // 8 * 48 * 3, 0, None))
    add("clouds band 320 x 40", planar(W.clouds))
    add("far islands + far strip 320 x 60", planar(W.far))
    fl = np.zeros((28, 640), np.uint8)
    for j in range(28):
        r = W.floor[j]
        fl[j] = np.tile(r, 640 // len(r) + 1)[:640]
    add("sea floor 640 x 28", planar(fl))
    pens = {ch: i + 1 for i, ch in enumerate(LETTERS)}
    for name, (frames, _) in sets.items():
        raw = b""
        chip = 0
        for f in frames:
            a = np.array([[0 if ch == "." else pens[ch] for ch in r] for r in f.a], np.uint8)
            raw += planar(a) + planar((a > 0).astype(np.uint8) * 7, 1)
            chip += ((f.w + 15) // 16 + 1) * 2 * f.h * 4
        rows.append((name + " %d x %d" % (frames[0].w, frames[0].h), chip, int(len(zlib.compress(raw, 9)) * 1.12), len(frames)))
    return rows


def report(rows):
    print("%-62s %8s %8s" % ("component", "chip B", "disk B"))
    for n, chip, disk, fr in rows:
        print("%-62s %8d %8d%s" % (n, chip, disk, "   (%d frames)" % fr if fr else ""))
    chip = sum(r[1] for r in rows)
    disk = sum(r[2] for r in rows)
    pilot_rule = sum(r[2] if r[3] is None else int(r[1] * 0.55) for r in rows)      # the pilot's estimate: BOBs at 55 % of chip
    print("TOTAL chip %d B = %.1f KB (pilot %d B = %.1f KB, %+d B)" % (chip, chip / 1024.0, PILOT_CHIP, PILOT_CHIP / 1024.0, chip - PILOT_CHIP))
    print("      packed disk about %.1f KB by the pilot's rule (BOBs at 55 %% of chip; pilot 32.9 KB), %.1f KB with each BOB set's own zlib -9 + 12 %%" %
          (pilot_rule / 1024.0, disk / 1024.0))
    return chip, disk


# ------------------------------------------------------------------------------------------------ agk's converter
def convert(scratch):
    sys.path.insert(0, os.path.join(os.path.expanduser("~"), "amiga-game-kit", "harness"))
    from agk import art as agkart
    tmp = tempfile.mkdtemp(prefix="world2-art-")
    try:
        art = os.path.join(tmp, "art")
        os.makedirs(art)
        for f in os.listdir(os.path.join(ROOT, "art")):
            if f not in ("art.toml", "disk_art.toml", "world2"):
                os.symlink(os.path.join(ROOT, "art", f), os.path.join(art, f))
        shutil.copytree(os.path.join(ROOT, "art", "world2"), os.path.join(art, "world2"))
        shutil.copy(os.path.join(art, "world2", "world2_art.toml"), os.path.join(art, "art.toml"))
        agkart.build(tmp, scratch)
    finally:
        shutil.rmtree(tmp)
    return open(os.path.join(scratch, "art.c")).read(), open(os.path.join(scratch, "art.h")).read()


def main():
    check = "--check" in sys.argv
    files, sets, W = outputs()
    stale = []
    for rel, data in files.items():
        data = data.encode() if isinstance(data, str) else data
        full = os.path.join(ART, rel)
        old = open(full, "rb").read() if os.path.exists(full) else None
        if old == data:
            continue
        stale.append(rel)
        if not check:
            os.makedirs(os.path.dirname(full), exist_ok=True)
            with open(full, "wb") as f:
                f.write(data)
    print("art/world2: %d files%s" % (len(files), " (%d %s)" % (len(stale), "stale" if check else "written") if stale else ", up to date"))
    report(sizes(files, sets, W))
    if "--convert" in sys.argv and not check:
        scratch = os.path.join(ROOT, "build", "world2-art")
        os.makedirs(scratch, exist_ok=True)
        _, hdr = convert(scratch)
        got = re.findall(r"#define ART_(W2_[A-Z0-9_]+?)_FRAMES (\d+)", hdr)
        print("agk converter ok, frames: %s" % ", ".join("%s=%s" % m for m in got))
        print("  previews: build/world2-art/preview/")
    if check and stale:
        print("make_world2: out of date: %s - run make_world2.py" % ", ".join(stale))
        sys.exit(1)


if __name__ == "__main__":
    main()
