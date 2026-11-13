"""Terrain strip tile kit: 16 px wide x 48 rows, PF2 pens 1-7 (island palette). Built procedurally;
palms are Retro Diffusion sprites locked to the pens. Light from the top left."""
import numpy as np
from w2lib import *
TW, TH = 16, 48
BACK, FRONT = 24, 41          # island back edge (jungle foot) and front edge (sand to surf) rows
MISS = MISSIONS[0]


def water_layer(w, seed, rows=range(TH)):
    """ripple texture (pens 6 crest, 7 glint, 1 trough) for a w-wide canvas, 1-px margins so tiles join anywhere"""
    r = rng(seed)
    a = np.zeros((TH, w), np.uint8)
    for y in rows:
        k = y / 47.0
        cov = 0.19 + 0.05 * k
        Lmin, Lmax = 3 + int(5 * k), 6 + int(8 * k)
        need = cov * w
        got = (a[y] > 0).sum()
        tries = 0
        while got < need and tries < 40:
            tries += 1
            L = min(int(r.integers(Lmin, Lmax + 1)), w - 2)
            x = int(r.integers(0, w - L + 1))
            if (a[y, max(0, x - 1):x + L + 1] > 0).any():
                continue
            a[y, x:x + L] = 6
            if L >= 6 and r.uniform() < 0.5:
                a[y, x] = 7
            if y + 1 < TH and L >= 4 and r.uniform() < 0.75:    # trough: 1 px down and right of the crest
                xs = x + 1
                xe = x + L - (1 if L > 5 else 0)
                if (a[y + 1, xs + 1:xe + 1] == 0).all() and xe < w - 1 and r.uniform() < 0.5:
                    a[y + 1, xs + 1:xe + 1] = 1
            got = (a[y] > 0).sum()
    return a


def smooth(t):
    t = np.clip(t, 0, 1)
    return t * t * (3 - 2 * t)


def island(canvas_w, seed, left_slope=0, right_slope=0, vegetation=True, ground_detail=True, bare=False):
    """paint an island run on a canvas (pens), starting from water. left_slope / right_slope: width in px of the
    beach that rises from / falls to the water at that end (0 = the run continues past the canvas edge)."""
    r = rng(seed)
    W = canvas_w
    a = water_layer(W, seed + 1)
    b = np.full(W, float(BACK))
    f = np.full(W, float(FRONT))
    wob = np.zeros(W)
    for per, amp in [(23, 1.0), (11, 0.7)]:
        wob += amp * np.sin(np.arange(W) * 2 * np.pi / per + r.uniform(0, 6.28))
    for x in range(W):
        lt = smooth((x + 0.5) / left_slope) if left_slope else 1.0
        rt = smooth((W - x - 0.5) / right_slope) if right_slope else 1.0
        t = min(lt, rt)
        b[x] = 37 - (37 - BACK) * t + (wob[x] * 0.6 if t < 1 else 0)
        f[x] = 37 + (FRONT - 37) * t + (wob[x] * 0.5 if t < 1 else 0)
    bi = np.round(b).astype(int)
    fi = np.round(f).astype(int)
    land = np.zeros((TH, W), bool)
    for x in range(W):
        if fi[x] > bi[x] + 1:
            land[bi[x]:fi[x], x] = True
    ring = np.zeros((TH, W), bool)
    for y in range(TH):
        for x in range(W):
            if land[y, x]:
                continue
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    if 0 <= y + dy < TH and 0 <= x + dx < W and land[y + dy, x + dx]:
                        ring[y, x] = True
    a[land] = 0
    for x in range(W):
        for y in range(bi[x], fi[x]):
            if not land[y, x]:
                continue
            d_top = y - bi[x]
            d_bot = fi[x] - 1 - y
            pen = 5
            if d_top < 2 and bi[x] < 31:
                pen = 4
            elif d_bot < 3:
                pen = 4
            a[y, x] = pen
    if ground_detail and not bare:
        # wind-rippled sand: every sand row carries short dashes of the other sand pen (>= 20 % of the row), so no
        # row of the strip is a flat colour; sparse pebbles on top
        for y in range(int(bi.min()) + 1, int(fi.max())):
            cols = [x for x in range(W) if land[y, x]]
            if len(cols) < 4:
                continue
            base = [int(a[y, x]) for x in cols]
            need = max(3, int(round(0.21 * len(cols))))
            got = 0
            tries = 0
            while got < need and tries < 40:
                tries += 1
                L = int(r.integers(2, 5))
                x = int(r.integers(0, W - L + 1))
                if not all(land[y, x + k] for k in range(L)):
                    continue
                b0 = a[y, x]
                if b0 not in (4, 5):
                    continue
                alt = 4 if b0 == 5 else 5
                if any(a[y, x + k] != b0 for k in range(L)):
                    continue
                for k in range(L):
                    a[y, x + k] = alt
                got += L
        for _ in range(W // 8):
            x = int(r.integers(2, W - 4))
            y = int(r.integers(29, 37))
            if land[y, x] and land[y, x + 1] and a[y, x] == 5:
                a[y, x] = 1
                a[y, x + 1] = 4
    if vegetation and not bare:
        x = 1
        while x < W - 2:
            hb = bi[x]
            if hb > 31:
                x += int(r.integers(2, 4))
                continue
            rx = int(r.integers(3, 6))
            ry = int(r.integers(5, 9))
            if x + rx >= W - 1:
                break
            cx, cy = x + rx, hb - 1 + int(r.integers(0, 2))
            for yy in range(cy - ry, cy + 2):
                for xx in range(cx - rx, cx + rx + 1):
                    if not (0 <= xx < W and 0 <= yy < TH):
                        continue
                    nx = (xx - cx) / (rx + 0.5)
                    ny = (yy - cy) / (ry + 0.5)
                    if nx * nx + ny * ny <= 1.0 and yy <= bi[xx] + 1:
                        v = 2
                        if nx + ny < -0.45 and ny < 0.1:
                            v = 3
                        if ny > 0.55 or nx > 0.75:
                            v = 1
                        a[yy, xx] = v
            x += rx * 2 - int(r.integers(0, 3))
        for x in range(W):
            if bi[x] < 31 and a[bi[x], x] in (4, 5):
                a[bi[x], x] = 1 if r.uniform() < 0.7 else 4
    for y in range(TH):
        for x in range(W):
            if ring[y, x] and not land[y, x] and a[y, x] in (0, 6, 7, 1) and y >= 28:
                a[y, x] = 7 if (r.uniform() < 0.45) else 6
    return a


def to_tiles(canvas):
    return [canvas[:, i * TW:(i + 1) * TW].copy() for i in range(canvas.shape[1] // TW)]


def lock_palm(path):
    im = Image.open(path).convert("RGBA")
    pal = [rgb12(c) for c in MISS["isl"]]
    a = np.zeros((im.height, im.width), np.uint8)
    px = im.load()
    for y in range(im.height):
        for x in range(im.width):
            p = px[x, y]
            if p[3] < 128:
                continue
            a[y, x] = nearest(p[:3], pal) + 1
    return a


def put(dst, spr, x0, y0):
    for y in range(spr.shape[0]):
        for x in range(spr.shape[1]):
            v = spr[y, x]
            if v and 0 <= y0 + y < dst.shape[0] and 0 <= x0 + x < dst.shape[1]:
                dst[y0 + y, x0 + x] = v


def crop_palm(a):
    ys, xs = np.nonzero(a)
    a = a[ys.min():ys.max() + 1, xs.min():xs.max() + 1].copy()
    h = a.shape[0]
    for y in range(h - 4, h):
        for x in range(a.shape[1]):
            if a[y, x] in (5, 4):
                a[y, x] = 0
    ys, xs = np.nonzero(a)
    return a[:ys.max() + 1, :]
