#!/usr/bin/env python3
"""Does World 3's terrain strip stream without popping or a missing column? (SPEC 13; the hardware pilot's proof in vAmiga.)

Takes screenshots of a flight (`agk test -p a500 w3pilot` leaves build/agk/<profile>/w3pilot/flyN.screen.png), finds for each the
camera x at which the art pass's own raster of the strip (art/world3 `w3compose.Terrain`, the same columns the converter packs into
disk/world3.bin) matches the picture best, and reports the mismatching pixels of lines 143-255 (the chopper's and the scenery's boxes
left out). A column that was not drawn, a stale column or a seam shows as a block of mismatches at a 16 px edge.

    python tools/check_w3strip.py [PROFILE] [SHOT ...]       (Windows Python: Pillow and numpy; exit 1 on a mismatch over 0.5 %)
"""
import os
import sys

import numpy as np
from PIL import Image

here = os.path.dirname(os.path.abspath(__file__))
root = os.path.normpath(os.path.join(here, ".."))
sys.path.insert(0, os.path.join(root, "art", "tools"))
import make_world3_disk as M   # noqa: E402

STRIP_Y, ROWS = 130, 126
CHECK_FROM = 143


def expected_strip(art):
    ter, objects = M.pilot_terrain(art)
    sys.path.insert(0, os.path.join(art, "tools"))
    import w3lib as L   # noqa: E402
    pal = [(0, 0, 0)] + [L.rgb12(c) for c in L.MISSIONS[0]["strip"]]
    return ter.strip, np.array(pal, np.uint8), objects, L


def main(argv):
    profile = argv[0] if argv else "a500"
    art = M.find_art([])
    strip, pal, objects, L = expected_strip(art)
    base = os.path.join(root, "build", "agk", profile, "w3pilot")
    shots = argv[1:] or ["start", "fly1", "fly2", "fly3", "fly4"]
    cols_total = strip.shape[1]
    bad_total = 0
    for name in shots:
        path = os.path.join(base, name + ".screen.png")
        im = np.array(Image.open(path).convert("RGB"))
        part = im[CHECK_FROM:256]                       # 113 x 320 x 3
        best = None
        for cam in range(0, cols_total - 320):
            win = strip[CHECK_FROM - STRIP_Y:, cam:cam + 320]
            m = win > 0
            exp = pal[win]
            diff = (exp != part).any(axis=2) & m
            n = int(diff.sum())
            if best is None or n < best[0]:
                best = (n, cam)
        n, cam = best
        win = strip[CHECK_FROM - STRIP_Y:, cam:cam + 320]
        m = win > 0
        exp = pal[win]
        diff = (exp != part).any(axis=2) & m
        # the scenery's and the chopper's boxes are not the strip: leave them out
        for o in objects:
            if o["kind"] in ("pad", "nest", "lodge", "hangar"):
                x0 = o["x"] - cam
                w = o.get("w", 64) if o["kind"] == "pad" else (30 if o["kind"] == "nest" else 64 if o["kind"] == "hangar" else 52)
                diff[:, max(0, x0):max(0, min(320, x0 + w))] = False
        # the chopper flies in the upper part: its box is at most 40 x 30 px, in these rows only when it is low
        total = int(m.sum())
        bad = int(diff.sum())
        pct = 100.0 * bad / max(1, total)
        cols_bad = sorted({int(x) // 16 for x in np.nonzero(diff.any(axis=0))[0]})
        print("%-6s cam %4d: %5d of %6d rock pixels differ (%.2f %%)%s" % (name, cam, bad, total, pct,
              ("; 16 px columns %s" % cols_bad) if bad else ""))
        if pct > 0.5:
            bad_total += 1
    print("check_w3strip:", "FAIL" if bad_total else "ok")
    return 1 if bad_total else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
