#!/usr/bin/env python3
"""The worlds' display art on the floppy (SPEC 4.3, 11; docs/STYLE-world2.md).

World 1's bands live in disk/diskart.bin (make_disk_art.py), loaded at boot into the art's
chip block. World 2 (missions 4-6) reuses that block's band region (the world arena): its
art is loaded there at its mission card, and world 1's bands are loaded back from their own
file when the game returns to world 1. This writes:
  disk/world1.bin    world 1's bands (clouds, mountains, title mountains, hills, ground):
                     byte for byte diskart.bin's band region (DISK_ART_CLOUDS_OFFS up to
                     DISK_ART_HANGAR_OFFS), for the reload
  disk/world2.bin    world 2's display art, the parts one after another (8-byte aligned):
                     the clouds (640 x 40) and the far islands (640 x 60) as agk's converter
                     makes a wrap_x band; the sea floor widened to 768 px (each row is its
                     loop W_j = 4 (28 + 3 j) repeated: a row's fetch of 336 px starts
                     anywhere in its first loop, W_j up to 436 px, so the row needs
                     W_j + 336 px); the terrain tile bank (36 columns of 16 x 48, 3
                     interleaved planes, tile i at rows 48 i ..); the tile map (288 bytes)
  src/world2_art.h   the parts' offsets and sizes, and the per-mission palettes from
                     art/world2/palettes.toml (12-bit pens, the AGA 24-bit gradients)
agk packs both onto the floppy (world1.bin.pk, world2.bin.pk).

    python3 art/tools/make_world_disk.py           # after make_disk_art.py or a world 2 edit
    python3 art/tools/make_world_disk.py --check   # exit 1 if disk/ or src/world2_art.h is stale
"""
import os
import re
import shutil
import struct
import sys
import tempfile
import tomllib

here = os.path.dirname(os.path.abspath(__file__))
root = os.path.normpath(os.path.join(here, "..", ".."))
sys.path.insert(0, os.path.join(os.path.expanduser("~"), "amiga-game-kit", "harness"))
from agk import art as agkart   # noqa: E402

PARTS = ["w2_clouds", "w2_far", "w2_sea", "w2_tiles"]
# World 2's BOBs, loaded after the display art in the same file (the "set"): the art's name, the
# frames kept (the converter's frames in the art file: patrol boat 0, 2, 4, 5 = one wake a facing and the shot tell a facing; the
# gunboat's idle, tell 1 and tell 2 a facing, its fire frame being tell 2 held; the pad's lit
# phase only), the game's name, and whether it has a mask: the pad's parts are only the pieces
# main.c puts the whole pad together from at the card (one BOB a pad, not fifteen: padCompose),
# so they have none. The levers of docs/STYLE-world2.md section 6.
SET = [("w2_barracks", None, "W2_BARRACKS", True), ("w2_patrol_boat", [0, 2, 4, 5], "PATROL", True),
       ("w2_gunboat", [0, 1, 2, 4, 5, 6], "GUNBOAT", True), ("w2_pad_cap", [0, 1], "PAD_CAP", False),
       ("w2_pad_slab", [0, 1, 2], "PAD_SLAB", False), ("w2_pad_centre", [0], "PAD_CENTRE", False),
       ("w2_pad_lamp", None, "PAD_LAMP", True), ("w2_splash", None, "SPLASH", True),
       ("w2_jet", None, "W2_JET", True)]   # (v1.16: world 1's jet art again, the 4 frames: left, right, flash left, flash right)
# World 2's slice 2 BOBs (missions 5 and 6: SPEC 11, "World 2, slice 2"): a file of their own, loaded into a chip block of its
# own that worldLoad allocates when world 2 loads (after the boot splash's 47 KB are freed) and frees when the game leaves
# world 2, so the boot-time block, the title and world 1 stay as they were.
SETX = [("w2_radar", None, "RADAR", True)]   # slice A: the radar site, 4 frames, the dish turning: wide, half, thin, half
PAD_ROWS = 46
PAD_WIDTHS = (48, 64, 80, 96)   # the pad's widths on the 16 px grid: one composed BOB each (chip, 3 planes, no mask)
SEA_LINES = 28
SEA_W = 768                  # every row's loop + the 336 px fetch (row 27: 436 + 332)
MISSIONS = ["m4_reef_run", "m5_trade_winds", "m6_typhoon_strait"]


def c_name(name):
    return "".join(p.capitalize() for p in name.split("_"))


def array(src, name):
    m = re.search(r"static const UWORD s_pArt%s\[\] = \{(.*?)\};" % c_name(name), src, re.S)
    if not m:
        raise SystemExit(f"make_world_disk: no data for [{name}] in the converted art")
    return [int(v, 16) for v in re.findall(r"0x[0-9A-Fa-f]+", m.group(1))]


def define(hdr, name):
    m = re.search(r"#define %s (\d+)" % name, hdr)
    if not m:
        raise SystemExit(f"make_world_disk: no {name}")
    return int(m.group(1))


def convert(out_dir):
    """agk's converter on a scratch project whose art.toml is world2_art.toml's display parts"""
    toml = open(os.path.join(root, "art", "world2", "world2_art.toml")).read()
    keep = []
    for block in re.split(r"(?m)^(?=\[)", toml):
        head = re.match(r"\[(\w+)\]", block)
        if head is None or head.group(1) == "art" or head.group(1) in PARTS or head.group(1) in [n for n, _, _, _ in SET + SETX]:
            keep.append(block)
    tmp = tempfile.mkdtemp(prefix="world-art-")
    try:
        os.makedirs(os.path.join(tmp, "art"))
        for f in os.listdir(os.path.join(root, "art")):
            if f not in ("art.toml", "disk_art.toml"):
                os.symlink(os.path.join(root, "art", f), os.path.join(tmp, "art", f))
        open(os.path.join(tmp, "art", "art.toml"), "w").write("".join(keep))
        agkart.build(tmp, out_dir)
    finally:
        shutil.rmtree(tmp)
    return open(os.path.join(out_dir, "art.c")).read(), open(os.path.join(out_dir, "art.h")).read()


def pixels(words, w, h):
    """interleaved 3-plane rows (row, plane, words) -> h rows of w pens"""
    wpr = w // 16
    out = []
    for y in range(h):
        row = [0] * w
        for p in range(3):
            for k in range(wpr):
                v = words[(y * 3 + p) * wpr + k]
                for b in range(16):
                    if v & (0x8000 >> b):
                        row[k * 16 + b] |= 1 << p
        out.append(row)
    return out


def planar(rows, w):
    wpr = w // 16
    words = []
    for row in rows:
        for p in range(3):
            for k in range(wpr):
                v = 0
                for b in range(16):
                    if row[k * 16 + b] & (1 << p):
                        v |= 0x8000 >> b
                words.append(v)
    return struct.pack(">%dH" % len(words), *words)


def sea(src, hdr):
    w, h = define(hdr, "ART_W2_SEA_BITMAP_W"), define(hdr, "ART_W2_SEA_H")
    if (w, h) != (640, SEA_LINES):
        raise SystemExit(f"make_world_disk: the sea is {w} x {h}, not 640 x {SEA_LINES}")
    rows = pixels(array(src, "w2_sea"), w, h)
    wide = []
    for j, row in enumerate(rows):
        loop = 4 * (28 + 3 * j)
        for x in range(loop, w):
            if row[x] != row[x - loop]:
                raise SystemExit(f"make_world_disk: sea row {j} doesn't repeat every {loop} px (x {x})")
        wide.append([row[x % loop] for x in range(SEA_W)])
    for j in (0, SEA_LINES - 1):
        if any(wide[j]):
            raise SystemExit(f"make_world_disk: sea row {j} must be empty (the palette switches)")
    return planar(wide, SEA_W)


def bob_parts(src, hdr, name, keep):
    """A BOB's image and mask as the chip bitmaps hold them (as make_disk_art.py: 3 interleaved planes,
    the mask in all 3), only the frames in keep, stacked in that order"""
    up = name.upper()
    h, frames = define(hdr, f"ART_{up}_H"), define(hdr, f"ART_{up}_FRAMES")
    wpr = define(hdr, f"ART_{up}_BITMAP_W") // 16
    data = array(src, name)
    if len(data) != h * frames * 4 * wpr:
        raise SystemExit(f"make_world_disk: [{name}]'s data isn't 3 planes + a mask a row")
    img, mask = [], []
    for f in (keep if keep is not None else range(frames)):
        if f >= frames:
            raise SystemExit(f"make_world_disk: [{name}] has no frame {f}")
        for y in range(f * h, (f + 1) * h):
            row = data[y * 4 * wpr:(y + 1) * 4 * wpr]
            img += row[:3 * wpr]
            mask += row[3 * wpr:] * 3
    return struct.pack(">%dH" % len(img), *img), struct.pack(">%dH" % len(mask), *mask)


def words(src, name):
    a = array(src, name)
    return struct.pack(">%dH" % len(a), *a)


def world1():
    hdr = open(os.path.join(root, "src", "disk_art.h")).read()
    a, b = define(hdr, "DISK_ART_CLOUDS_OFFS"), define(hdr, "DISK_ART_BYTES")
    order = ["CLOUDS", "MOUNTAINS", "TITLEMTN", "HILLS", "GROUND", "FENCE", "FENCEMASK", "BARRACKS", "BARRACKSMASK", "TANK",
             "TANKMASK", "JET", "JETMASK", "MINE", "MINEMASK", "FLAG", "FLAGMASK"]
    offs = [define(hdr, f"DISK_ART_{n}_OFFS") for n in order]
    if offs != sorted(offs) or offs[-1] + define(hdr, "DISK_ART_FLAGMASK_BYTES") != b:
        raise SystemExit("make_world_disk: diskart.bin's bands and world 1's own BOBs are not the one run at its end")
    art = open(os.path.join(root, "disk", "diskart.bin"), "rb").read()
    if len(art) != define(hdr, "DISK_ART_BYTES"):
        raise SystemExit("make_world_disk: disk/diskart.bin is stale (run make_disk_art.py)")
    return art[a:b]


def hexes(vals, width):
    return ", ".join(f"0x{v:0{width}X}" for v in vals)


def outputs(src, hdr):
    files = {}
    w1 = world1()
    files[os.path.join("disk", "world1.bin")] = w1
    tilemap = open(os.path.join(root, "art", "world2", "tilemap.bin"), "rb").read()
    tiles = define(hdr, "ART_W2_TILES_FRAMES")
    if define(hdr, "ART_W2_TILES_H") != 48 or define(hdr, "ART_W2_TILES_W") != 16:
        raise SystemExit("make_world_disk: a tile is a 16 x 48 column")
    if len(array(src, "w2_tiles")) != tiles * 48 * 3:
        raise SystemExit("make_world_disk: the tile bank isn't 3 interleaved planes of 16 px")
    if len(tilemap) != 288 or max(tilemap) >= tiles:
        raise SystemExit("make_world_disk: tilemap.bin must be 288 columns of tile indices")
    parts = [("clouds", words(src, "w2_clouds")), ("far", words(src, "w2_far")), ("sea", sea(src, hdr)),
             ("tiles", words(src, "w2_tiles")), ("map", tilemap)]
    parts = [(n, d + b"\0" * (-len(d) % 8)) for n, d in parts]
    display = sum(len(d) for _, d in parts)
    set_parts = []
    for name, keep, cname, isMask in SET:
        img, mask = bob_parts(src, hdr, name, keep)
        set_parts.append((cname, img + b"\0" * (-len(img) % 8)))
        if isMask:
            set_parts.append((cname + "MASK", mask + b"\0" * (-len(mask) % 8)))
    files[os.path.join("disk", "world2.bin")] = b"".join(d for _, d in parts + set_parts)
    setx_parts = []
    for name, keep, cname, isMask in SETX:
        img, mask = bob_parts(src, hdr, name, keep)
        setx_parts.append((cname, img + b"\0" * (-len(img) % 8)))
        if isMask:
            setx_parts.append((cname + "MASK", mask + b"\0" * (-len(mask) % 8)))
    files[os.path.join("disk", "world2x.bin")] = b"".join(d for _, d in setx_parts)
    pal = tomllib.load(open(os.path.join(root, "art", "world2", "palettes.toml"), "rb"))
    h = ["// Generated by art/tools/make_world_disk.py from art/world2/ - do not edit.",
         "// World 2 (Coral Islands, missions 4-6): the display art on the floppy (disk/world2.bin,",
         "// loaded into the world arena and the BOBs' chip at the mission card) and its per-mission palettes.",
         "#ifndef _WORLD2_ART_H_", "#define _WORLD2_ART_H_", "", "#include <stdint.h>", ""]
    for n in ("W2_CLOUDS", "W2_FAR"):
        for k in ("W", "H", "BITMAP_W"):
            h.append(f"#define ART_{n}_{k} {define(hdr, f'ART_{n}_{k}')}")
    h.append(f"#define ART_W2_CLOUDS_LOOP_W {define(hdr, 'ART_W2_CLOUDS_LOOP_W')}")
    h.append(f"#define ART_W2_FAR_LOOP_W {define(hdr, 'ART_W2_FAR_LOOP_W')}")
    h += [f"#define ART_W2_SEA_BITMAP_W {SEA_W}   // 640 px of art, widened: row j repeats every 4 (28 + 3 j) px",
          f"#define ART_W2_SEA_H {SEA_LINES}",
          f"#define ART_W2_TILE_COUNT {tiles}",
          "#define ART_W2_TILE_H 48", "#define ART_W2_MAP_COLS 288", ""]
    h.append(f"#define WORLD1_BANDS_BYTES {len(w1)}   // disk/world1.bin = diskart.bin's band region")
    h.append("// disk/world2.bin: the parts one after another (offset, bytes)")
    offs = 0
    for name, data in parts:
        h.append(f"#define WORLD2_{name.upper()}_OFFS {offs}")
        h.append(f"#define WORLD2_{name.upper()}_BYTES {len(data)}")
        offs += len(data)
    h.append(f"#define WORLD2_DISPLAY_BYTES {offs}")
    h.append("// then world 2's BOBs: the set (each: its size, the frames kept, where its image and its mask begin)")
    for name, keep, cname, isMask in SET:
        up = name.upper()
        h.append(f"#define ART_{cname}_W {define(hdr, f'ART_{up}_W')}")
        h.append(f"#define ART_{cname}_H {define(hdr, f'ART_{up}_H')}")
        h.append(f"#define ART_{cname}_FRAMES {len(keep) if keep is not None else define(hdr, f'ART_{up}_FRAMES')}")
        h.append(f"#define ART_{cname}_BITMAP_W {define(hdr, f'ART_{up}_BITMAP_W')}")
    for name, data in set_parts:
        h.append(f"#define WORLD2_{name}_OFFS {offs}")
        offs += len(data)
    h.append(f"#define WORLD2_BYTES {offs}")
    h.append("// the pads' composed BOBs (padCompose): chip past the file's end, one a width, 3 planes, 46 rows, no mask")
    comp = 0
    for w in PAD_WIDTHS:
        h.append(f"#define WORLD2_PAD{w}_OFFS {offs + comp}")
        comp += (w // 8 * 3 * PAD_ROWS + 7) & ~7
    h.append(f"#define WORLD2_PAD_COMPOSED_BYTES {comp}")
    h.append("// slice 2's BOBs: disk/world2x.bin, in a chip block of their own (worldLoad allocates it at world 2's load)")
    for name, keep, cname, isMask in SETX:
        up = name.upper()
        h.append(f"#define ART_{cname}_W {define(hdr, f'ART_{up}_W')}")
        h.append(f"#define ART_{cname}_H {define(hdr, f'ART_{up}_H')}")
        h.append(f"#define ART_{cname}_FRAMES {len(keep) if keep is not None else define(hdr, f'ART_{up}_FRAMES')}")
        h.append(f"#define ART_{cname}_BITMAP_W {define(hdr, f'ART_{up}_BITMAP_W')}")
    xoffs = 0
    for name, data in setx_parts:
        h.append(f"#define WORLD2X_{name}_OFFS {xoffs}")
        xoffs += len(data)
    h.append(f"#define WORLD2X_BYTES {xoffs}")
    h += ["", "// Per mission (4, 5, 6): PF2 pens 1-7 (index 0 unused), dark to light; the sky gradient",
          "// top to H (lines 24-179), the sea's H to C (180-207), C under the strip; AGA: 24 bits"]

    def tab(key, name):
        rows = ["\t{0x000, " + hexes([int(v, 16) for v in pal[m][key]], 3) + "}," for m in MISSIONS]
        return [f"static const uint16_t {name}[3][8] = {{"] + rows + ["};"]
    h += tab("clouds", "g_pW2CloudPal") + tab("far", "g_pW2FarPal") + tab("sea", "g_pW2SeaPal") + tab("isl", "g_pW2IslPal")
    h.append("static const uint16_t g_pW2SkyDotPal[3][3] = {" +
             ", ".join("{" + hexes([int(v, 16) for v in pal[m]["sky_dots"]], 3) + "}" for m in MISSIONS) + "};")
    for key, name, w in (("sky_top", "g_pW2SkyTop", 3), ("sky_low_haze_H", "g_pW2SkyH", 3), ("deep_sea_C", "g_pW2SeaC", 3),
                         ("aga_sky_top", "g_pW2SkyTop24", 6), ("aga_sky_low", "g_pW2SkyH24", 6),
                         ("aga_deep_sea_C", "g_pW2SeaC24", 6)):
        t = "uint16_t" if w == 3 else "uint32_t"
        h.append(f"static const {t} {name}[3] = {{" + hexes([int(pal[m][key], 16) for m in MISSIONS], w) + "};")
    for m in MISSIONS:
        if pal[m]["sea"][5:7] != pal[m]["isl"][5:7]:
            raise SystemExit(f"make_world_disk: {m}: sea pens 6-7 must equal the island's (strip row 0)")
    h += ["", "#endif // _WORLD2_ART_H_", ""]
    files[os.path.join("src", "world2_art.h")] = "\n".join(h).encode()
    return files


def main():
    check = "--check" in sys.argv
    out_dir = os.path.join(root, "build", "art-world2")
    files = outputs(*convert(out_dir))
    stale = []
    for path, data in files.items():
        full = os.path.join(root, path)
        old = open(full, "rb").read() if os.path.exists(full) else None
        if old == data:
            continue
        stale.append(path)
        if not check:
            with open(full, "wb") as f:
                f.write(data)
    for path, data in files.items():
        print(f"  {path}: {len(data)} bytes{' (stale)' if check and path in stale else ''}")
    if check and stale:
        print("make_world_disk: out of date - run python3 art/tools/make_world_disk.py")
        sys.exit(1)


if __name__ == "__main__":
    main()
