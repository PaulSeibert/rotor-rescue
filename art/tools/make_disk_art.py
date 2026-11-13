#!/usr/bin/env python3
"""The art that lives on the floppy, not in the program (content plan T25, SPEC 11).

art/disk_art.toml lists it in art.toml's format (the chopper and the parallax bands). This
converts it with agk's own art converter, so the pixels are exactly what `agk art` would
make, and writes:
  disk/diskart.bin    one file, the parts one after another:
                      the chopper's sprite frames: ART_CHOPPER_FRAMES x ART_CHOPPER_PARTS
                      parts of ART_CHOPPER_H rows, 2 words a row (plane 0, plane 1): the
                      sprite data without its control words (10240 bytes);
                      then each band: its bitmap's interleaved planes (row, plane, 80
                      bytes), byte for byte what agk's artNAMECreate() would put in chip;
                      then each BOB: its image, then its mask (the mask in all 3
                      planes), as artNAMECreate() and artNAMECreateMask() would
  src/disk_art.h      the assets' ART_* sizes, each part's offset and size in the file,
                      and the chopper's colours (artChopperApplyColors)
  build/art-disk/preview/*.png   what they look like (look after every change)
agk packs it onto the floppy (diskart.bin.pk); main.c loads it during the splash into one
chip block (the chopper's frame store and the bands' bitmaps).
Why: AROS loads the program at about 10 frames a KB against the harness's 3000-frame boot
window, and on the A500 profiles the program fills the 512K slow RAM, so every KB of art in
the program is also a KB of chip RAM (what doesn't fit in slow RAM spills into chip).

    python3 art/tools/make_disk_art.py           # after editing chopper.txt or a band
    python3 art/tools/make_disk_art.py --check   # exit 1 if disk/ or src/disk_art.h is stale
"""
import os
import re
import shutil
import struct
import sys
import tempfile

here = os.path.dirname(os.path.abspath(__file__))
root = os.path.normpath(os.path.join(here, "..", ".."))
sys.path.insert(0, os.path.join(os.path.expanduser("~"), "amiga-game-kit", "harness"))
from agk import art as agkart   # noqa: E402

BANDS = ["clouds", "mountains", "title_mtn", "hills", "ground"]
BOBS = ["hangar", "fence", "barracks", "tank", "hostage", "jet", "mine", "shot", "bomb", "shell", "boom",
        "shadow", "flag", "smoke"]


def c_name(name):
    return "".join(p.capitalize() for p in name.split("_"))


def array(src, name):
    m = re.search(r"static const UWORD s_pArt%s\[\] = \{(.*?)\};" % c_name(name), src, re.S)
    if not m:
        raise SystemExit(f"make_disk_art: no data for [{name}] in the converted art")
    return [int(v, 16) for v in re.findall(r"0x[0-9A-Fa-f]+", m.group(1))]


def convert(out_dir):
    """agk's converter on a scratch project whose art.toml is disk_art.toml"""
    tmp = tempfile.mkdtemp(prefix="disk-art-")
    try:
        os.makedirs(os.path.join(tmp, "art"))
        for f in os.listdir(os.path.join(root, "art")):
            if f not in ("art.toml", "disk_art.toml"):
                os.symlink(os.path.join(root, "art", f), os.path.join(tmp, "art", f))
        shutil.copy(os.path.join(root, "art", "disk_art.toml"), os.path.join(tmp, "art", "art.toml"))
        agkart.build(tmp, out_dir)
    finally:
        shutil.rmtree(tmp)
    return open(os.path.join(out_dir, "art.c")).read(), open(os.path.join(out_dir, "art.h")).read()


def define(hdr, name):
    m = re.search(r"#define %s (\d+)" % name, hdr)
    if not m:
        raise SystemExit(f"make_disk_art: no {name} in the converted art.h")
    return int(m.group(1))


def bob_parts(src, hdr, name):
    """A BOB's image and mask, each as its chip bitmap holds them (3 planes,
    interleaved: row, plane, words), from art.c's rows of 3 planes + the mask"""
    up = name.upper()
    rows = define(hdr, f"ART_{up}_H") * define(hdr, f"ART_{up}_FRAMES")
    wpr = define(hdr, f"ART_{up}_BITMAP_W") // 16
    words = array(src, name)
    if len(words) != rows * 4 * wpr:
        raise SystemExit(f"make_disk_art: [{name}]'s data isn't 3 planes + a mask a row")
    img, mask = [], []
    for y in range(rows):
        row = words[y * 4 * wpr:(y + 1) * 4 * wpr]
        img += row[:3 * wpr]
        mask += row[3 * wpr:] * 3
    return struct.pack(">%dH" % len(img), *img), struct.pack(">%dH" % len(mask), *mask)


def outputs(src, hdr):
    files = {}
    parts = []
    for name in ["chopper"] + BANDS:
        parts.append((name, struct.pack(">%dH" % len(array(src, name)), *array(src, name))))
    for name in BOBS:
        img, mask = bob_parts(src, hdr, name)
        parts += [(name, img), (name + "_mask", mask)]
    # (each part starts on 8 bytes, like an AllocMem block)
    parts = [(n, d + b"\0" * (-len(d) % 8)) for n, d in parts]
    files[os.path.join("disk", "diskart.bin")] = b"".join(d for _, d in parts)
    defines = [l for l in hdr.splitlines() if l.startswith("#define ART_") and
               re.match(r"#define ART_(CHOPPER|%s)_" % "|".join(b.upper() for b in BANDS + BOBS), l)]
    colors = re.search(r"void artChopperApplyColors\(UWORD \*pPalette\) \{(.*?)\n\}", src, re.S).group(1)
    h = ["// Generated by art/tools/make_disk_art.py from art/disk_art.toml - do not edit.",
         "// The art on the floppy (disk/*.bin): its sizes, and the chopper's colours.",
         "#ifndef _DISK_ART_H_", "#define _DISK_ART_H_", "", "#include <ace/types.h>", ""]
    h += defines
    h += ["", "// disk/diskart.bin: the parts one after another (offset, bytes); one file, one",
          "// AmigaDOS open (on AROS an open of a floppy file costs about a second)"]
    offs = 0
    for name, data in parts:
        stem = name.replace("_", "").upper()
        h.append(f"#define DISK_ART_{stem}_OFFS {offs}")
        h.append(f"#define DISK_ART_{stem}_BYTES {len(data)}")
        offs += len(data)
    h.append(f"#define DISK_ART_BYTES {offs}")
    h += ["", "/** Set the chopper's 15 attached-sprite colours (slots 17-31). */",
          "static inline void artChopperApplyColors(UWORD *pPalette) {" + colors.rstrip(), "}", "",
          "#endif // _DISK_ART_H_", ""]
    files[os.path.join("src", "disk_art.h")] = "\n".join(h).encode()
    return files


def main():
    check = "--check" in sys.argv
    out_dir = os.path.join(root, "build", "art-disk")
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
    print(f"  previews: {os.path.relpath(os.path.join(out_dir, 'preview'), root)}/")
    if check and stale:
        print("make_disk_art: out of date - run python3 art/tools/make_disk_art.py")
        sys.exit(1)


if __name__ == "__main__":
    main()
