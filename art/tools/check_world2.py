#!/usr/bin/env python3
"""Seam, solid and cloud-base check for World 2 (Coral Islands): plain_check.py's four tests (run, period-2/4 dither,
hard edge, solid row) adapted to this world's layout, plus the bare-seam, flat-top and straight-base tests.
3 missions x 16 camera offsets.  Ported from the approved pilot's icheck.py, with the straight-base test added.

    ~/amiga-tools/venv-img/bin/python art/tools/check_world2.py [--verbose] [--pilot-clouds] [--assets]

  --pilot-clouds  run against the pilot's cloud band (one straight base line per cloud); it must FAIL: that is the
                  red test for the cloud change
  --assets        also check the generated production files in art/world2 (pens, frame sizes, facings)
Exit 1 when anything fails.
"""
import os
import sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "world2"))
import numpy as np
from w2compose import *

LIMIT = 64
STRONG = 5
SOLID_LIMIT = 0.85
CAMS = [i * 283 for i in range(16)]
# lines where a band's edge or a palette switch happens: they get the run / dither / edge scans
EDGES = list(range(69, 76)) + list(range(106, 123)) + list(range(163, 184)) + list(range(200, 213))
SOLID = range(165, 256)
SOLID_EXEMPT = (179, 180, 207)          # bare by contract: far row 9, floor row 0 (palette switch), floor row 27 (palette switch)
# lines that must show only the backdrop (sky or sea gradient): the clouds' margins, the blank rows, far-band top rows,
# the palette-switch rows
BARE = list(range(72, 75)) + list(range(109, 120)) + [120, 121, 122, 179, 180, 207]


def longest_const(t):
    best = cur = 1
    for x in range(1, 320):
        cur = cur + 1 if t[x] == t[x - 1] and t[x] else 1
        best = max(best, cur)
    return best


def longest_period(t, P):
    best = cur = 0
    for x in range(P, 320):
        if t[x] == t[x - P]:
            cur += 1
        else:
            cur = 0
        if cur + P > best:
            w = t[x - cur - P + 1:x + 1]
            if len(set(w)) > 1 and all(w[i] == w[i + P] for i in range(len(w) - P)) and not all(c == w[0] for c in w[:P]):
                best = cur + P
    return best


def strong_run(a, b):
    best = cur = 0
    for x in range(320):
        d = sum(abs(((int(a[x]) >> s) & 15) - ((int(b[x]) >> s) & 15)) for s in (0, 4, 8))
        cur = cur + 1 if d >= STRONG else 0
        best = max(best, cur)
    return best


def top_runs(band, maxrow):
    """longest run of columns whose first drawn row is the same (a flat top), and the first drawn row overall"""
    tops = [next((y for y in range(band.shape[0]) if band[y, x]), 99) for x in range(band.shape[1])]
    best = cur = 1
    for x in range(1, len(tops)):
        cur = cur + 1 if tops[x] == tops[x - 1] and tops[x] < maxrow else 1
        best = max(best, cur)
    return best, min(tops)


# ---- straight-base test (the cloud band must not read as a line)
BASE_RUN_MAX = 10        # px: longest run of bottom-edge pixels on one row (a lobe bottom is 5-7 px)
BASE_ROW_SHARE = 0.22    # no single row may carry more than this share of all bottom-edge pixels
BASE_SPREAD_MIN = 10     # rows between the highest and the lowest cloud base
BASE_DIAG_MAX = 9        # px: longest straight diagonal of the lowest drawn pixel per column
BASE_MIN_CLOUDS = 4


def cloud_base_stats(band):
    """band: pen array (rows x 320), looping in x. Returns (longest run of bottom-edge pixels on one row, busiest row's share
    of all bottom-edge pixels, spread of the clouds' base rows, longest diagonal, number of clouds, base rows of the clouds).
    A bottom-edge pixel is drawn with nothing drawn directly under it. A cloud is a run of drawn columns (gaps under 3 px
    are merged); its base row is the row with most of its bottom-edge pixels."""
    h, w = band.shape
    drawn = band > 0
    under = np.zeros_like(drawn)
    under[:-1] = drawn[1:]
    edge = drawn & ~under
    # longest run of bottom-edge pixels on one row, looping in x
    best = 0
    for y in range(h):
        row = np.concatenate([edge[y], edge[y]])
        cur = 0
        for v in row:
            cur = cur + 1 if v else 0
            best = max(best, min(cur, w))
    total = int(edge.sum())
    per_row = edge.sum(axis=1)
    share = float(per_row.max()) / total if total else 0.0
    # clouds as column spans
    cols = drawn.any(axis=0)
    if cols.all():
        spans = [(0, w)]
    else:
        start = int(np.argmin(cols))                    # a gap column: unroll from there
        order = [(start + i) % w for i in range(w)]
        spans, cur = [], None
        gap = 0
        for i, x in enumerate(order):
            if cols[x]:
                if cur is None:
                    cur = [i, i]
                else:
                    cur[1] = i
                gap = 0
            else:
                gap += 1
                if cur is not None and gap >= 3:
                    spans.append(tuple(cur))
                    cur = None
        if cur is not None:
            spans.append(tuple(cur))
        spans = [[order[a] for a in range(s0, s1 + 1)] for s0, s1 in spans]
    bases = []
    if cols.all():
        bases.append(int(per_row.argmax()))
    else:
        for xs in spans:
            pr = edge[:, xs].sum(axis=1)
            bases.append(int(pr.argmax()))
    spread = (max(bases) - min(bases)) if bases else 0
    # straight diagonal: lowest drawn pixel per column, constant non-zero step
    low = np.array([int(np.nonzero(drawn[:, x])[0].max()) if drawn[:, x].any() else -1 for x in range(w)])
    diag = cur = 0
    prev_d = None
    for x in range(1, 2 * w):
        a, b = low[(x - 1) % w], low[x % w]
        if a < 0 or b < 0:
            cur, prev_d = 0, None
            continue
        d = int(b - a)
        if d != 0 and d == prev_d:
            cur += 1
        else:
            cur = 1 if d != 0 else 0
        prev_d = d
        diag = max(diag, min(cur + 1, w))
    return best, share, spread, diag, len(bases), sorted(bases)


def main():
    verbose = "--verbose" in sys.argv
    W = World(pilot_clouds="--pilot-clouds" in sys.argv)
    fails = 0
    for mi, m in enumerate(MISSIONS):
        worst = dict(run=(0, 0, 0), dither2=(0, 0, 0), dither4=(0, 0, 0), edge=(0, 0, 0))
        solid = {}
        bare_bad = []
        for cam in CAMS:
            img, tag = compose(W, mi, cam, wisps=False)
            arr = np.array(img.convert("RGB"))
            c12 = np.zeros((256, 320), np.int32)
            c12[:] = ((arr[:, :, 0].astype(int) + 8) // 17 << 8) | ((arr[:, :, 1].astype(int) + 8) // 17 << 4) | ((arr[:, :, 2].astype(int) + 8) // 17)
            for line in EDGES:
                t = [int(v) for v in tag[line]]
                for key, v in (("run", longest_const(t)), ("dither2", longest_period(t, 2)), ("dither4", longest_period(t, 4))):
                    if v > worst[key][0]:
                        worst[key] = (v, line, cam)
                if line + 1 in EDGES:
                    v = strong_run(c12[line], c12[line + 1])
                    if v > worst["edge"][0]:
                        worst["edge"] = (v, line, cam)
            for line in SOLID:
                if line in SOLID_EXEMPT:
                    continue
                row = list(c12[line])
                f = max(row.count(c) for c in set(row)) / 320.0
                if f > solid.get(line, (0, 0))[0]:
                    solid[line] = (f, cam)
            for line in BARE:
                if (tag[line] != 0).any():
                    bare_bad.append((line, cam))
            if (tag[208] != 0).any() and (tag[208] % 8 < 6)[tag[208] != 0].any():
                bare_bad.append((208, cam))
        sl = max(solid, key=lambda l: solid[l][0])
        failing = sorted(l for l in solid if solid[l][0] > SOLID_LIMIT)
        worst["solid"] = (int(solid[sl][0] * 100), sl, solid[sl][1])
        bad = [k for k, v in worst.items() if v[0] > (LIMIT if k != "solid" else SOLID_LIMIT * 100)]
        if bare_bad:
            bad.append("bare")
        fails += len(bad)
        # flat tops: far islands, clouds
        fr, fmin = top_runs(W.far, 40)
        cr, cmin = top_runs(W.clouds, 36)
        if fr > 24 or fmin < 3:
            bad.append("far-flat-top"); fails += 1
        if cr > 24 or cmin < 3:
            bad.append("cloud-flat-top"); fails += 1
        brun, bshare, bspread, bdiag, ncl, bases = cloud_base_stats(W.clouds)
        base_bad = []
        if brun > BASE_RUN_MAX:
            base_bad.append("base run %d px > %d" % (brun, BASE_RUN_MAX))
        if bshare > BASE_ROW_SHARE:
            base_bad.append("one row carries %d%% of the base > %d%%" % (bshare * 100, BASE_ROW_SHARE * 100))
        if bspread < BASE_SPREAD_MIN:
            base_bad.append("base heights span %d rows < %d" % (bspread, BASE_SPREAD_MIN))
        if bdiag > BASE_DIAG_MAX:
            base_bad.append("straight diagonal %d px > %d" % (bdiag, BASE_DIAG_MAX))
        if ncl < BASE_MIN_CLOUDS:
            base_bad.append("%d clouds < %d" % (ncl, BASE_MIN_CLOUDS))
        if base_bad:
            bad.append("cloud-straight-base"); fails += 1
        print("%-15s %s" % (m["name"], "  ".join(("%s %d%% (line %d, cam %d)" if k == "solid" else "%s %d px (line %d, cam %d)") % ((k,) + v) for k, v in worst.items())))
        print("%-15s bare-seam lines violating: %d | far-island flat top run %d cols, first drawn row %d | cloud flat top run %d, first row %d -> %s" %
              ("", len(bare_bad), fr, fmin, cr, cmin, "FAIL " + ",".join(bad) if bad else "ok"))
        print("%-15s cloud base: longest flat edge %d px, busiest row %d%%, base rows %s (spread %d), diagonal %d px, %d clouds -> %s" %
              ("", brun, bshare * 100, bases, bspread, bdiag, ncl, "FAIL: " + "; ".join(base_bad) if base_bad else "ok"))
        if verbose and failing:
            print("   solid rows over 85%%: %s" % ", ".join("%d(%d%%)" % (l, solid[l][0] * 100) for l in failing))
        if verbose and bare_bad:
            print("   bare violations:", bare_bad[:10])
    print("checked %d camera offsets x %d edge lines + %d solid lines x 3 missions, limit %d px, solid <= %d %%" % (len(CAMS), len(EDGES), len(SOLID), LIMIT, int(SOLID_LIMIT * 100)))
    if "--assets" in sys.argv:
        import check_world2_assets
        fails += check_world2_assets.run()
    print("RESULT: %s" % ("FAIL (%d)" % fails if fails else "PASS"))
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
