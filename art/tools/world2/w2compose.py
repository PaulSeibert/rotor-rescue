"""Coral Islands: builds every background layer and composes the play screen like the copper and bitmaps do.
Lines: HUD 0-23 | sky gradient 24-179 (copper, line-dithered) | clouds 72-111 (cam/8) | far islands 120-179
(cam/4; rows 0-49 + far strip 50-59) | sea floor 180-207 (28 rows, row j at (28+3j)/112 of the camera, sea pens)
| terrain strip 208-255 (48 rows, 1:1, island pens) | PF1 BOBs and the chopper on top.
Palette loads (copper, in PF2-blank rows): clouds at 24, far at 113, sea at 179 (far row 9 / floor row 0 empty),
island at 207 (floor row 27 empty; strip row 0 holds pens 6 and 7 only, which both palettes share)."""
import math
import numpy as np
from w2lib import *
import w2sea as isea, w2tiles as itiles, w2bobs as ibobs
from w2tiles import TW, TH, WATER_MARKS, WATER_GLINT, WATER_CREST, water_cycle, island, water_layer, to_tiles, put, lock_palm, crop_palm, smooth

M0 = MISSIONS[0]


# ------------------------------------------------------------------------------------------ tile bank
def boulder(a, x, yb, w, h, seed):
    r = rng(seed)
    for yy in range(h):
        for xx in range(w):
            nx = (xx + .5 - w / 2) / (w / 2)
            ny = (yy + .5 - h) / h
            if nx * nx + (ny * ny) <= 1.0 and ny <= 0:
                v = 4
                if nx + ny * 1.2 < -0.55:
                    v = 5
                if nx + ny * 0.6 > 0.55 or yy >= h - 1:
                    v = 2
                a[yb - h + yy + 1, x + xx] = v
    for k in range(w):                                  # a surf/sand shadow under it
        if a[yb + 1, x + k] in (5, 4):
            a[yb + 1, x + k] = 4


def build_bank():
    bank = {}
    for i in range(8):
        bank["water%d" % i] = water_layer(TW, 100 + i)
    for i in range(4):
        bank["flat%d" % i] = island(TW, 200 + i)
    for v in range(2):
        t = to_tiles(island(32, 300 + v, left_slope=32))
        bank["shoreL%d_0" % v], bank["shoreL%d_1" % v] = t
        t = to_tiles(island(32, 320 + v, right_slope=32))
        bank["shoreR%d_0" % v], bank["shoreR%d_1" % v] = t
    pair = crop_palm(lock_palm(RAW + "palm2_5201_1.png"))
    one = crop_palm(lock_palm(RAW + "palm2_5201_2.png"))
    for nm, spr, seed, flip in [("palmA", pair, 400, False), ("palmB", one, 401, False), ("palmC", one[:, ::-1], 402, True)]:
        c = island(32, seed)
        put(c, spr, (32 - spr.shape[1]) // 2, 31 - spr.shape[0])
        bank[nm + "_0"], bank[nm + "_1"] = to_tiles(c)
    for nm, seed in [("bush0", 500), ("bush1", 501)]:
        c = island(TW, seed)
        for k in range(2):
            x0, rx, ry = 1 + k * 7, 4, 7 if nm == "bush0" else 6
            for yy in range(24 - ry, 27):
                for xx in range(x0, x0 + 2 * rx):
                    nx = (xx + .5 - (x0 + rx)) / rx
                    ny = (yy + .5 - 24) / ry
                    if nx * nx + ny * ny <= 1.0 and 2 <= xx <= 13:
                        v = 2
                        if nx + ny < -0.4:
                            v = 3
                        if ny > 0.5 or nx > 0.7:
                            v = 2
                        c[yy, xx] = v
        bank[nm] = c
    for v, seed in enumerate((600, 601)):
        c = island(32, seed)
        boulder(c, 5 + 7 * v, 36 if v == 0 else 34, 9, 6, seed)
        boulder(c, 20 - 5 * v, 38, 6, 4, seed + 7)
        bank["rock%d_0" % v], bank["rock%d_1" % v] = to_tiles(c)
    for v, seed in enumerate((700, 701)):                 # reef: rocks in the water with surf rings
        c = water_layer(TW, seed, finish=False)
        boulder(c, 3 + 4 * v, 40, 7, 4, seed)
        for x in range(1, 15):
            for y in (41, 42):
                if c[y, x] == 0 or c[y, x] in WATER_MARKS:
                    if (x + y * 2 + v) % 3 != 0:
                        c[y, x] = WATER_GLINT if (x + y) % 2 else WATER_CREST
        water_cycle(c, seed)
        bank["reef%d" % v] = c
    for v, seed in enumerate((800, 801)):
        bank["padbed%d" % v] = island(TW, seed, vegetation=True, ground_detail=True)
    return bank


def world_map(bank):
    """4608 px = 288 columns. Returns names[288] and the object table (world px)."""
    rs = rng(77)
    names = ["water%d" % int(rs.integers(0, 8)) for _ in range(288)]
    objs = []
    interiors = ["flat0", "flat1", "flat2", "flat3", "bush0", "bush1", "flat0", "flat2"]

    def lay(col, tiles):
        for i, t in enumerate(tiles):
            names[col + i] = t

    def build(col, ncol, barracks=None, pad=None, base=False, seed=0):
        r = rng(seed)
        t = ["shoreL%d_0" % (seed % 2), "shoreL%d_1" % (seed % 2)]
        inner = ncol - 4
        row = []
        i = 0
        while i < inner:
            ch = r.uniform()
            if ch < 0.24 and i + 2 <= inner:
                row += [["palmA_0", "palmA_1"], ["palmB_0", "palmB_1"], ["palmC_0", "palmC_1"]][int(r.integers(0, 3))]
                i += 2
            elif ch < 0.32 and i + 2 <= inner:
                row += ["rock%d_0" % int(r.integers(0, 2)), "rock%d_1" % int(r.integers(0, 2))]
                i += 2
            else:
                row.append(interiors[int(r.integers(0, len(interiors)))])
                i += 1
        row = row[:inner]
        t += row + ["shoreR%d_0" % (seed % 2), "shoreR%d_1" % (seed % 2)]
        lay(col, t)
        objs.append(dict(first_col=col, columns=ncol, base_island=base,
                         barracks_col=None if barracks is None else col + barracks,
                         pad_col=None if pad is None else col + pad[0], pad_px=None if pad is None else pad[1]))
        if barracks is not None:                        # clear flat ground under the barracks (4 tiles) and the pad
            for k in range(4):
                names[col + barracks + k] = "flat%d" % ((k + seed) % 4)
        if pad is not None:
            pc, pw = pad
            for k in range(pw // 16 + 2):
                names[col + pc + k] = "padbed%d" % (k % 2)

    # island chain, enemy end (left) to the base island (right)
    build(4, 28, barracks=4, pad=(10, 64), seed=1)      # A: x 128-512 (the mock-up island)
    build(66, 38, barracks=9, pad=(19, 64), seed=2)     # B
    build(120, 30, barracks=8, pad=(17, 56), seed=3)    # C
    build(184, 40, barracks=9, pad=(20, 96), seed=4)    # D
    build(250, 36, base=True, pad=(6, 80), seed=5)      # base island
    for c in (47, 52, 110, 160, 171, 236):
        names[c] = "reef%d" % (c % 2)
    return names, objs


def render_strip(bank, names):
    return np.concatenate([bank[n] for n in names], axis=1)


# ------------------------------------------------------------------------------------------ bands
def far_band():
    """60 rows x 320: far volcanic islands (RD roughs 'far2' reshaded: aerial perspective, lit left, haze foot) + far strip"""
    im = Image.open(RAW + "far2_5102_0.png").convert("RGBA")
    pal = [rgb12(c) for c in M0["far"]]
    raw = np.zeros((64, 320), np.uint8)
    px = im.load()
    for y in range(64):
        for x in range(320):
            raw[y, x] = nearest(px[x, y][:3], pal) + 1
    # sky = the lightest pen connected to the top rows
    sky_pen = int(np.bincount(raw[:3].ravel()).argmax())
    land = np.zeros((64, 320), bool)
    for x in range(320):
        y = 0
        while y < 44 and raw[y, x] == sky_pen:
            y += 1
        land[y:44, x] = True            # everything under the first non-sky pixel down to the horizon (row 43)
    band = np.zeros((60, 320), np.uint8)
    foot = 41
    for x in range(320):
        for y in range(foot + 1):
            if land[y, x] and raw[y, x] != sky_pen:
                p = raw[y, x]
                q = min(6, p + 2)
                l_edge = (x < 2 or not land[y, x - 2]) or (y > 0 and not land[y - 1, x])
                r_edge = (x > 316 or not land[y, (x + 1) % 320])
                if l_edge:
                    q = min(6, q + 1)
                elif r_edge:
                    q = max(2, q - 1)
                band[y + 12, x] = q
    # no flat tops: carve every plateau (a run of >= 5 columns within 1 row of its local minimum) into a pointed apex
    tops = np.array([next((y for y in range(60) if band[y, x]), 60) for x in range(320)])
    x = 0
    while x < 320:
        if tops[x] < 40:
            x1 = x
            while x1 + 1 < 320 and abs(tops[x1 + 1] - tops[x]) <= 1:
                x1 += 1
            if x1 - x + 1 >= 5:
                cx = (x + x1) / 2.0
                tmin = tops[x:x1 + 1].min()
                for xx in range(x, x1 + 1):
                    lim = tmin + int(round(1.15 * abs(xx - cx))) - 1
                    for y in range(60):
                        if y < lim:
                            band[y, xx] = 0
            x = x1 + 1
        else:
            x += 1
    # haze foot: dissolve with blue noise from a wandering start row to row 58; lift pens by up to 2
    for x in range(320):
        r0 = 44 + 3.5 * math.sin(x * 2 * math.pi / 320 * 3 + 0.8) + 2.5 * math.sin(x * 2 * math.pi / 320 * 7 + 2.1)
        for y in range(40, 60):
            if band[y, x]:
                t = (y - r0) / (59 - r0)
                if t > 0:
                    keep = 1 - smooth(t * 1.15)
                    if bn(x, y) > keep:
                        band[y, x] = 0
                    else:
                        band[y, x] = min(6, band[y, x] + int(t * 2.5))
    # far sea glitter in the far strip (rows 50-58), blue-noise sparse, low contrast (pens 5/6 of the haze palette)
    for y in range(50, 59):
        dens = 0.27 + 0.02 * (y - 50)
        for x in range(320):
            dx = dens * (1 + 0.8 * math.sin(x * 2 * math.pi / 320 * 4 + y))
            if band[y, x] == 0 and bn(x * 3 + y * 5, y * 7 + x) < dx and bn(x + 17, y + 31) < 0.7:
                band[y, x] = 5 if bn(x, y + 9) < 0.7 else 6
                if bn(x + 1, y) < 0.5 and band[y, (x + 1) % 320] == 0:
                    band[y, (x + 1) % 320] = band[y, x]
    band[:3] = 0
    band[59] = 0
    return band


def cloud_band_pilot(seed=9):
    """(the approved pilot band, kept so the checker can prove it fails: every cloud is cut on one straight base) 40 rows x 320 cumulus, lit top-left (pens 7 lit .. 3 belly), belly dissolves by blue noise; rows 0-2 and 37-39 empty"""
    r = rng(seed)
    a = np.zeros((40, 320), np.uint8)
    spec = [(40, 31, 70), (150, 27, 90), (250, 33, 54), (320 + 70, 28, 48)]
    for cx0, yb, w in spec:
        circles = []
        n = max(4, w // 12)
        for i in range(n):
            t = i / (n - 1)
            rad = 5 + 6 * math.sin(math.pi * t) ** 0.8 + r.uniform(-1, 2)
            cx = cx0 - w / 2 + w * t
            cy = yb - rad * 0.75
            circles.append((cx, cy, rad))
        for y in range(2, yb + 1):
            for xx in range(int(cx0 - w / 2 - 14), int(cx0 + w / 2 + 14)):
                best = None
                for (cx, cy, rad) in circles:
                    d = math.hypot(xx + .5 - cx, y + .5 - cy)
                    if d <= rad:
                        lit = ((xx + .5 - cx) * -0.6 + (y + .5 - cy) * -0.8) / rad
                        if best is None or cy - rad < best[0]:
                            best = (cy - rad, lit, d / rad)
                if best:
                    lit = best[1]
                    v = 7 if lit > 0.5 else 6 if lit > 0.15 else 5 if lit > -0.25 else 4 if lit > -0.6 else 3
                    if y > yb - 4:
                        v = 3 if y > yb - 2 else min(v, 4)
                    a[y, xx % 320] = v
        for y in range(yb - 3, yb + 1):                    # belly dissolves
            for xx in range(int(cx0 - w / 2 - 14), int(cx0 + w / 2 + 14)):
                x = xx % 320
                if a[y, x] and bn(x, y) > 1 - (yb - y + 1) * 0.28 - 0.1 * math.sin(x * 0.31):
                    a[y, x] = 0
    a[:3] = 0
    a[37:] = 0
    return a


# Cloud specs: centre x, base row (the lowest a lobe may reach), width, smallest and largest lobe radius, lobe-size
# profile exponent, base wobble (rows) and its period (px). The silhouettes differ on purpose: a tall tower, a big
# three-domed cumulus, a low humped bank, a mid-height puff and a cluster of three small high puffs. The bases sit
# 12 rows apart and every lobe has its own bottom, so no straight line shows across the loop (check_world2.py).
CLOUDS = [
    dict(cx=40, base=33, w=96, rmin=5.0, rmax=12.0, p=0.9, wob=3.0, per=61, stack=0),    # big low bank, base sags to the right
    dict(cx=128, base=29, w=40, rmin=5.0, rmax=9.5, p=0.7, wob=1.5, per=23, stack=3),    # narrow tower
    dict(cx=186, base=20, w=44, rmin=3.5, rmax=6.0, p=0.9, wob=1.6, per=29, stack=0),    # small high bank
    dict(cx=240, base=26, w=60, rmin=4.5, rmax=9.5, p=0.8, wob=3.0, per=43, stack=1),    # mid bank, base rises to the right
    dict(cx=292, base=15, w=26, rmin=3.0, rmax=4.2, p=0.9, wob=1.2, per=17, stack=0),    # high wisp
]


def cloud_band(seed=11):
    """40 rows x 320 cumulus lit top-left (pens 7 lit .. 3 belly). Every cloud is a ring of overlapping circles whose
    bottoms are their own (lobed, wandering), so there is no straight base; the belly pens follow each column's own
    bottom and dissolve by blue noise. Rows 0-2 and 37-39 are empty (the band's seams)."""
    r = rng(seed)
    a = np.zeros((40, 320), np.uint8)
    for k, cl in enumerate(CLOUDS):
        cx0, yb, w = cl["cx"], cl["base"], cl["w"]
        ph = r.uniform(0, 6.28)
        circles = []
        n = max(4, int(w / (cl["rmax"] * 0.8)))
        for i in range(n):
            t = i / (n - 1) if n > 1 else 0.5
            rad = cl["rmin"] + (cl["rmax"] - cl["rmin"]) * math.sin(math.pi * t) ** cl["p"] + r.uniform(-0.8, 1.0)
            cx = cx0 - w / 2 + w * t + r.uniform(-1.5, 1.5)
            lb = yb + cl["wob"] * math.sin(2 * math.pi * (cx - cx0) / cl["per"] + ph) - r.uniform(0, 1.5)
            circles.append((cx, lb - rad, rad))
        # fill the underside between big lobes with small ones so the base is scalloped, not stepped
        for i in range(n * 2):
            cx = cx0 - w / 2 + 3 + (w - 6) * r.uniform()
            rad = r.uniform(2.2, 3.6)
            lb = yb + cl["wob"] * math.sin(2 * math.pi * (cx - cx0) / cl["per"] + ph) - r.uniform(0.5, 2.5)
            circles.append((cx, lb - rad, rad))
        for s in range(cl["stack"]):                       # towers: smaller domes stacked above the middle
            rad = cl["rmax"] * (0.85 - 0.18 * s)
            cx = cx0 + r.uniform(-4, 4)
            circles.append((cx, yb - cl["rmax"] * 1.3 - s * rad * 0.9, rad))
        x0, x1 = int(cx0 - w / 2 - 14), int(cx0 + w / 2 + 14)
        cov = {}                                            # (x, y) -> pen before the belly
        for y in range(3, 37):
            for xx in range(x0, x1):
                best = None
                for (cx, cy, rad) in circles:
                    d = math.hypot(xx + .5 - cx, y + .5 - cy)
                    if d <= rad:
                        lit = ((xx + .5 - cx) * -0.6 + (y + .5 - cy) * -0.8) / rad
                        if best is None or cy - rad < best[0]:
                            best = (cy - rad, lit)
                if best:
                    lit = best[1]
                    cov[(xx, y)] = 7 if lit > 0.5 else 6 if lit > 0.15 else 5 if lit > -0.25 else 4 if lit > -0.6 else 3
        bottom = {}
        for (xx, y) in cov:
            bottom[xx] = max(bottom.get(xx, 0), y)
        for (xx, y), v in cov.items():
            d = bottom[xx] - y                              # rows above this column's own bottom
            if d < 2:
                v = 3
            elif d < 4:
                v = min(v, 4)
            if d < 3:                                       # the belly dissolves by blue noise
                keep = 1 - (3 - d) * 0.30 - 0.12 * math.sin(xx * 0.31 + k)
                if bn(xx, y) > keep:
                    continue
            a[y, xx % 320] = v
    a[:3] = 0
    a[37:] = 0
    return a


def sea_rows():
    return isea.sea_floor()


class World:
    def __init__(self, pilot_clouds=False):
        self.bank = build_bank()
        self.names, self.objs = world_map(self.bank)
        self.strip = render_strip(self.bank, self.names)
        self.far = far_band()
        self.clouds = cloud_band_pilot() if pilot_clouds else cloud_band()
        self.floor = sea_rows()
        # strip row 0 may hold only shared pens 6/7
        self.strip[0] = np.where(self.strip[0] >= 6, self.strip[0], 0)


# ------------------------------------------------------------------------------------------ composing
def colorize(a, pal):
    lut = np.zeros((8, 3), np.uint8)
    for i, c in enumerate(pal):
        lut[i + 1] = rgb12(c)
    return lut[a]


def compose(W, mi, cam, mock_objects=None, wisps=True):
    """returns (RGB image 320x256 (PIL), pen-tag array [256][320] (0 backdrop, else band*8+pen))"""
    m = MISSIONS[mi]
    img = np.zeros((256, 320, 3), np.uint8)
    tag = np.zeros((256, 320), np.int32)
    sky = grad(m["sky"][0], m["sky"][1], 156)
    sea = grad(H(m), m["C"], 28, y0=180)
    for y in range(24, 256):
        c = sky[y - 24] if y < 180 else (sea[y - 180] if y < 208 else m["C"])
        img[y, :] = rgb12(c)

    def put_row(y, row, off, pal, band):
        w = len(row)
        xs = (np.arange(320) + off) % w
        v = row[xs]
        mask = v > 0
        lut = np.array([(0, 0, 0)] + [rgb12(c) for c in pal], np.uint8)
        img[y, mask] = lut[v[mask]]
        tag[y, mask] = band * 8 + v[mask]

    for r in range(40):
        put_row(CLOUDS_Y + r, W.clouds[r], cam // 8, m["clouds"], 1)
    for r in range(60):
        put_row(FAR_Y + r, W.far[r], cam // 4, m["far"], 2)
    for j in range(SEA_ROWS):
        s, wj = isea.floor_geom(j)
        c = cam % 448
        put_row(SEA_Y + j, W.floor[j], (c * (28 + 3 * j)) // 112, m["sea"], 3)
    for r in range(48):
        put_row(STRIP_Y + r, W.strip[r], cam, m["isl"], 4)
    out = Image.fromarray(img, "RGB").convert("RGBA")
    if wisps:
        px = out.load()
        for (yy, xx) in [(27, 90), (42, 300), (60, 20)]:
            for i in range(14):
                if 0 <= xx + i < 320:
                    px[xx + i, yy + 24] = rgb12(m["dots"][1] if 3 < i < 10 else m["dots"][0]) + (255,)
    return out, tag
