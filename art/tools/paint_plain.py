#!/usr/bin/env python3
"""Paints the plain's two source strips (docs/plain-spec.md): art/plain_far.txt, art/plain_floor_src.txt.

Hand-placed pixel art written as code: the strips are tiny, so every mesa, the lake, the wash, the
road and the scrub are placed by number here (deterministic). Run, then make_plain.py:
    python3 art/tools/paint_plain.py && python3 art/tools/make_plain.py
The floor is drawn in "world" px (one 576 px period) and projected: world x on row j is
column x * W_j / 576, so every feature leans toward the vanishing point as the contract says.
"""
import math
import os
import random
import sys

here = os.path.dirname(os.path.abspath(__file__))
art = os.path.join(here, '..')
sys.path.insert(0, here)

FAR_ROWS, FLOOR_LINES, PERIOD = 10, 19, 576
MTN_ROWS = 50
BAYER = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]


def row_w(j):
    return 8 * (FLOOR_LINES - 1 + j)


def read_rows(name):
    lines = open(os.path.join(art, name)).read().split('\n')
    return [l for l in lines[lines.index('frame') + 1:] if l.strip()]


def write_art(name, comment, pal_file, rows):
    out = ['# ' + c for c in comment]
    out += ['colors', '  .  transparent']
    for l in open(os.path.join(art, pal_file)).read().split('\n'):
        p = l.split()
        if len(p) >= 2 and not l.startswith('#') and p[0] != '0':
            out.append('  %s  %s' % (p[0], p[1]))
    out += ['frame'] + [''.join(r) for r in rows]
    open(os.path.join(art, name), 'w').write('\n'.join(out) + '\n')


# ---------------------------------------------------------------- blue noise
def blue_noise(w, h, seed, sigma=1.6, rad=5):
    """A tileable blue-noise rank mask in [0,1): the last phase of void-and-cluster (each new point goes
    into the biggest void of a gaussian energy field, on a torus). Any threshold of it is an even scatter
    with no row or column period, so no dither line can form."""
    rng = random.Random(seed)
    en = [rng.random() * 1e-6 for _ in range(w * h)]
    ker = [(dx, dy, math.exp(-(dx * dx + dy * dy) / (2 * sigma * sigma)))
           for dy in range(-rad, rad + 1) for dx in range(-rad, rad + 1)]
    rank = [0.0] * (w * h)
    for n in range(w * h):
        i = en.index(min(en))
        rank[i] = n / float(w * h)
        x, y = i % w, i // w
        for dx, dy, k in ker:
            en[((y + dy) % h) * w + (x + dx) % w] += k
        en[i] = 1e9
    return rank


# ---------------------------------------------------------------- far strip
def smooth_noise(rng, period, octaves):
    """periodic 1-D noise, summed octaves: [(cycles, amplitude)]"""
    out = [0.0] * period
    for cyc, amp in octaves:
        ph = rng.random() * 6.2832
        ph2 = rng.random() * 6.2832
        for x in range(period):
            t = 6.2832 * cyc * x / period
            out[x] += amp * (0.65 * math.sin(t + ph) + 0.35 * math.sin(2 * t + ph2))
    return out


def foot_edge(rng):
    """Per column, where the mountain body stops (s, in far rows) and how deep its dissolve runs (d).
    Low foothill swells, ragged scree tongues pointing down and haze bays where it lifts away; no
    plateau of equal heights, so no row is the edge for more than a few columns."""
    n = 320
    s = [4.0 + v for v in smooth_noise(rng, n, [(2, 1.3), (5, 1.0), (11, 0.7), (23, 0.35)])]
    for _ in range(6):                                   # scree tongues: long lit slope left, short right
        x0, wl, wr, ln = rng.randrange(n), rng.randrange(8, 16), rng.randrange(4, 9), rng.uniform(1.0, 2.0)
        for dx in range(-wl, wr + 1):
            f = 1 - (-dx / wl if dx < 0 else dx / wr)
            s[(x0 + dx) % n] += ln * f ** 1.5 * (0.75 + 0.5 * rng.random())
    for _ in range(6):                                   # haze bays
        x0, w, dp = rng.randrange(n), rng.randrange(10, 26), rng.uniform(2.5, 4.5)
        for dx in range(-w, w + 1):
            s[(x0 + dx) % n] -= dp * (0.5 + 0.5 * math.cos(3.1416 * dx / w)) ** 1.3
    for x in range(n):                                   # ragged: a little white noise, so nothing is a plateau
        s[x] += rng.uniform(-0.45, 0.45)
    s = [min(6.6, max(0.0, v)) for v in s]
    dd = [1.6 + 1.1 * (0.5 + 0.5 * v) + 0.55 * max(0.0, s[x] - 4.0)         # the deeper the foot, the longer it takes to fade
          for x, v in enumerate(smooth_noise(rng, n, [(3, 0.6), (9, 0.5)]))]
    return s, dd


def weighted(rng, pens):
    """pens: [(pen, weight)] -> one pen"""
    t = rng.random() * sum(w for _, w in pens)
    for p, w in pens:
        t -= w
        if t < 0:
            return p
    return pens[-1][0]


def wander(rng, period, seed_cycles=((3, 0.3), (7, 0.25), (15, 0.2))):
    """per-x acceptance weight around 1: the density of a scatter that changes along x"""
    return [1.0 + v for v in smooth_noise(rng, period, list(seed_cycles))]


def paint_far():
    rng = random.Random(4172)
    mtn = read_rows('mountains.txt')
    base = mtn[MTN_ROWS - 1]                       # row 0 continues row 49 x for x
    s, dd = foot_edge(rng)
    bn = blue_noise(320, FAR_ROWS, 77)
    rows = [['.'] * 320 for _ in range(FAR_ROWS)]
    for x in range(320):
        for y in range(FAR_ROWS - 1):               # row 9 stays empty: the palette switch falls on haze
            dist = y - s[x]                          # < 0 inside the body
            if dist < 0:
                cover = 1.0
            else:
                cover = max(0.0, 1.0 - dist / dd[x]) ** 1.5 * 0.92
            if y == FAR_ROWS - 2:
                cover *= 0.3                         # row 8 stays thin
            if bn[y * 320 + x] >= cover:
                continue
            haze = min(1.0, max(0.0, (dist + 1.5) / (dd[x] + 1.5)))   # hazier toward the edge's tip, not toward the bottom
            haze = min(1.0, max(0.0, haze ** 0.8 + rng.uniform(-0.15, 0.15)))
            lift = int(round(4.4 * haze))
            bx = int(base[(x + rng.choice((-1, 0, 0, 1))) % 320])      # a column or so off: no hairline streaks
            rows[y][x] = str(min(5, max(1, bx + lift)))
    # dust: short flecks of foothill haze scattered over the foot's last rows, so the haze between the foot and
    # the floor is not a flat band (every line at least ~17 % drawn); pens 4-6 only (no bright speckle at night)
    wx = wander(random.Random(8), 320)
    for y, target in ((4, 0.0), (5, 0.17), (6, 0.19), (7, 0.2), (8, 0.22)):
        order = sorted(range(320), key=lambda x: bn[y * 320 + x])
        n = sum(c != '.' for c in rows[y])
        for x in order:
            if n >= target * 320:
                break
            if rows[y][x] != '.' or rng.random() > 0.6 * wx[x]:
                continue
            ln = 1 + (rng.random() < 0.45) + (rng.random() < 0.2)
            c = weighted(rng, [('5', 55), ('4', 30), ('6', 15)])
            for d in range(ln):
                if rows[y][(x + d) % 320] == '.':
                    rows[y][(x + d) % 320] = c
                    n += 1
    return rows


# ---------------------------------------------------------------- floor
def paint_floor():
    rng = random.Random(1112)
    rows = [['.'] * 288 for _ in range(FLOOR_LINES)]
    owner = [dict() for _ in range(FLOOR_LINES)]     # column -> feature tag, to keep scrub off them

    def col(j, wx):
        return int(round(wx * row_w(j) / PERIOD)) % row_w(j)

    def put(j, wx, c, tag=None, over=True):
        if j <= 0 or j >= FLOOR_LINES - 1:
            return
        x = col(j, wx)
        if not over and rows[j][x] != '.':
            return
        rows[j][x] = c
        if tag:
            owner[j][x] = tag

    def span(j, wa, wb, c, tag=None, thin=None):
        """fill world [wa, wb) on row j, at least one column; thin(x) -> False drops a pixel."""
        xa, xb = col(j, wa), col(j, wb)
        if xb <= xa:
            xb = xa + 1
        for x in range(xa, xb):
            if thin and not thin(x):
                continue
            rows[j][x % row_w(j)] = c
            if tag:
                owner[j][x % row_w(j)] = tag

    fbn = blue_noise(288, FLOOR_LINES, 91)

    def bay(j, x, k):                                # blue-noise test, k/16 coverage (no ordered dither)
        return fbn[j * 288 + x % 288] < k / 16.0

    # --- flats texture: sparse streaks of the slightly darker pen 5 on the near rows, pen 7 glints far
    for j in range(1, FLOOR_LINES - 1):
        n = [3, 3, 3, 4, 4, 4, 5, 5, 5, 6, 6, 7, 7, 8, 8, 8, 8][j - 1]
        for _ in range(n):
            wx = rng.randrange(PERIOD)
            if j < 5:
                put(j, wx, '7' if rng.random() < .6 else '5')
            else:
                ln = 1 + (j - 5) // 3 + rng.randrange(3)      # streaks lengthen toward the viewer
                span(j, wx, wx + ln * PERIOD / row_w(j), '5')

    # --- the dry lake / salt flat: a plan ellipse, centre world 330, rows 3-8, lit rim toward the
    # top-left (pen 7 face, a thin pen 6 'wet' fringe on its far-right shore, a few pen 5 cracks)
    LC, LH = 330, 150
    for j in range(3, 9):
        t = (j - 5.6) / 3.0                                  # -0.87 .. 1.1
        r = max(0.0, 1 - t * t)
        hw = LH * r ** 0.5
        if hw < 6:
            continue
        a, b = LC - hw, LC + hw
        # soft top: the far edge is hazy, dithered 50 %, then 75 %
        cov = {3: 6, 4: 11}.get(j, 16)
        span(j, a, b, '7', 'lake', thin=lambda x, j=j, cov=cov: bay(j, x, cov))
        if cov == 16:
            # shore: the first and last columns darker, like a baked rim
            put(j, a, '5', 'lake')
            put(j, b - 1, '6', 'lake')
        if j in (6, 7):
            for _ in range(5):                              # a few cracks inside
                wx = rng.uniform(a + 20, b - 24)
                span(j, wx, wx + 14, '6', 'lake')
    # a bright strip of the dusk light along the lake's upper half
    span(4, LC - 90, LC - 20, '7', 'lake')

    # --- the dry riverbed (wash): leaves the lake's lower end at world ~420, runs toward the viewer,
    # wandering a little; pale bed (pen 7 dithered), dark bank pixels on its right (shade side)
    path = {9: 418, 10: 424, 11: 428, 12: 421, 13: 412, 14: 408, 15: 414, 16: 424, 17: 436}
    width = {9: 16, 10: 18, 11: 20, 12: 24, 13: 28, 14: 32, 15: 36, 16: 40, 17: 44}
    for j in range(9, 18):
        a, b = path[j] - width[j] / 2, path[j] + width[j] / 2
        span(j, a, b, '7', 'wash')
        put(j, b, '5', 'wash')
        put(j, a - 1, '5', 'wash')
        if j >= 13:
            put(j, b + 3, '4', 'bank')

    # --- the road: row 11 the surface, pen 3, with pale pen 6 dashes; row 12 its lit verge. Runs the full
    # 576 world px (loops). The wash passes under it (a culvert: the road is drawn over).
    vr = random.Random(7)                                   # the verge: runs with irregular gaps, not a solid line
    verge, wx = [None] * PERIOD, 0
    while wx < PERIOD:
        for _ in range(vr.randrange(16, 44)):
            if wx < PERIOD:
                verge[wx] = '5' if vr.random() < 0.88 else '4'
                wx += 1
        wx += vr.randrange(5, 17)
    for wx in range(PERIOD):
        dash = wx % 36 < 12
        put(11, wx, '6' if dash else '3', 'road')
        if verge[wx]:
            put(12, wx, verge[wx], 'road')
    # marker posts: one dark pixel on the far verge (row 10), every 48 world px, and the odd taller pair
    # is NOT possible (anything tall shears), so posts are a pen 1 pixel with a pen 2 'shadow' pixel
    for k in range(PERIOD // 48):
        wx = 24 + k * 48
        put(10, wx, '1', 'post')
        if k % 4 == 2:                                      # the odd pole: a bigger mark, 2 px, on row 10
            put(10, wx + 1.5, '2', 'post')
            put(9, wx, '2', 'post')

    # --- scrub: dots and patches, sparse and tiny far, larger and denser near; kept off the lake,
    # wash and road.  (j: count of tufts, size px, pens)
    for j in range(1, FLOOR_LINES - 1):
        if j in (11, 12):
            continue
        n = int(3 + 0.75 * j ** 1.1)
        pens = ('5', '7') if j < 4 else ('5', '4') if j < 8 else ('4', '5', '3') if j < 14 else ('3', '4', '4', '2')
        for _ in range(n):
            wx = rng.randrange(PERIOD)
            w = 1 if j < 6 else 1 + rng.randrange(2) if j < 12 else 2 + rng.randrange(3)
            x = col(j, wx)
            if any(owner[j].get((x + d) % row_w(j)) for d in range(-1, w + 1)):
                continue
            c = rng.choice(pens)
            for d in range(w):
                rows[j][(x + d) % row_w(j)] = c
            # a patch: a couple of neighbours, tight to the first
            if j >= 6 and rng.random() < 0.45:
                for d in range(rng.randrange(2, 5)):
                    xx = (x + w + 1 + d * 2) % row_w(j)
                    if not owner[j].get(xx):
                        rows[j][xx] = rng.choice(pens)
            # a lit pixel on the left of the bigger tufts (light from the top left)
            if j >= 13 and w >= 3 and not owner[j].get((x - 1) % row_w(j)):
                rows[j][(x - 1) % row_w(j)] = '5'
    # --- the near edge: rows 14-17 thin out toward the hills by a blue-noise keep mask whose density wanders
    # along x (world px), so no row of dots or wash end marks line 198/199 and no feature has a flat foot
    xn = smooth_noise(random.Random(5), PERIOD, [(3, 0.3), (7, 0.25), (15, 0.2)])
    keep = {14: 1.0, 15: 0.85, 16: 0.62, 17: 0.4}
    for j in keep:
        for x in range(row_w(j)):
            if rows[j][x] == '.':
                continue
            wx = x * PERIOD / row_w(j)
            k = min(1.0, max(0.05, keep[j] * (0.75 + xn[int(wx) % PERIOD])))
            if fbn[j * 288 + x] >= k:
                rows[j][x] = '.'
    # --- the floor is never a flat band: every row is at least ~18 % drawn (flats and scrub, wandering density)
    tr = random.Random(21)
    xw = wander(tr, PERIOD)
    for j in range(1, FLOOR_LINES - 1):
        target = 0.19 if j < 5 else 0.2
        w = row_w(j)
        pens = ([('5', 55), ('4', 35), ('7', 10)] if j < 5 else [('5', 40), ('4', 35), ('3', 15), ('7', 10)] if j < 14
                else [('4', 35), ('3', 30), ('5', 20), ('2', 15)])
        order = sorted(range(w), key=lambda x: fbn[j * 288 + x])
        n = sum(c != '.' for c in rows[j][:w])
        for x in order:
            if n >= target * w:
                break
            if rows[j][x] != '.' or owner[j].get(x) or tr.random() > 0.55 * xw[int(x * PERIOD / w) % PERIOD]:
                continue
            ln = 1 + (j >= 6) * (tr.random() < 0.5) + (j >= 12) * (tr.random() < 0.5)
            c = weighted(tr, pens)
            for d in range(ln):
                if rows[j][(x + d) % w] == '.':
                    rows[j][(x + d) % w] = c
                    n += 1
    return rows


def paint_hills():
    """hills.txt rows 18-26 (lines 215-223): the foot of the hills dissolves into the ground (pen 6 = C, the
    backdrop) along a wandering per-column edge with a blue-noise scatter, instead of the old 6/5 line dither
    and three solid rows. The source is the pilot's hills (tools/hills_orig.txt): rows 0-17 are kept."""
    rng = random.Random(2231)
    lines = open(os.path.join(here, 'hills_orig.txt')).read().split(chr(10))
    head = lines[:lines.index('frame') + 1]
    src = [list(l) for l in lines[lines.index('frame') + 1:] if l.strip()]
    s = [19.5 + v for v in smooth_noise(rng, 320, [(3, 0.8), (8, 0.7), (19, 0.45)])]
    for x in range(320):
        s[x] += rng.uniform(-0.5, 0.5)
    dd = [2.0 + 1.2 * (0.5 + 0.5 * v) for v in smooth_noise(rng, 320, [(4, 0.6), (11, 0.5)])]
    bn = blue_noise(320, 9, 33)
    out = [r[:] for r in src]
    for x in range(320):
        pen = '.'
        for y in (21, 20, 19, 18):                   # the hill's own pen at the foot, found from its body
            if src[y][x] not in '.6':
                pen = src[y][x]
                break
        for y in range(18, 27):
            dist = y - s[x]
            cover = 1.0 if dist < 0 else max(0.0, 1.0 - dist / dd[x]) ** 1.4 * 0.9
            if y <= 21 and src[y][x] == '.':
                continue                             # a crest gap stays a gap
            if pen == '.' or bn[(y - 18) * 320 + x] >= cover:
                out[y][x] = '.'
            else:
                out[y][x] = pen
    plain_texture(out, rng)
    open(os.path.join(art, 'hills.txt'), 'w').write(chr(10).join(head + [''.join(r) for r in out]) + chr(10))


def plain_texture(g, rng):
    """Texture for the plain behind and in front of the hills (docs/plain-spec.md s7): every transparent pixel of
    hills rows 2-26 (lines 199-223) may take it. It scrolls at 1/2 like the floor's near edge. Farther (top)
    rows get 1-2 px flecks in pens 5/4 (+ the odd pale 7); nearer rows get larger scrub tufts with a lit left
    pixel and a shadow, small stones and pale or dark patches. Every row stays at least ~17-22 % drawn, with a
    density that wanders along x, so no band of flat colour crosses the screen. Pen 6 is C: never used."""
    H = len(g)
    bn = blue_noise(320, H - 2, 55)
    xw = wander(rng, 320)
    free = lambda y, x: 2 <= y < H and g[y][x % 320] == '.'
    # patches first: pale (7) and dark (3) strips on the middle and near rows, dithered, wandering in length
    for k in range(14):
        y = rng.randrange(6, 25)
        t = (y - 2) / 24.0
        w = int(5 + 12 * t + rng.randrange(6))
        x0 = rng.randrange(320)
        pen = '7' if k % 3 else '3'
        for d in range(w):
            if free(y, x0 + d) and rng.random() < 0.78 - 0.4 * abs(d - w / 2.0) / w:
                g[y][(x0 + d) % 320] = pen
                if pen == '3' and free(y + 1, x0 + d + 1) and rng.random() < 0.5:
                    g[y + 1][(x0 + d + 1) % 320] = '2'
    for y in range(2, H):
        t = (y - 2) / 24.0
        target = 0.17 + 0.05 * t
        pens = ([('5', 62), ('7', 14), ('4', 24)] if t < 0.3 else [('5', 36), ('4', 30), ('7', 12), ('3', 22)] if t < 0.65
                else [('4', 30), ('3', 24), ('5', 22), ('7', 12), ('2', 12)])
        order = sorted(range(320), key=lambda x: bn[(y - 2) * 320 + x])
        n = sum(c != '.' for c in g[y])
        for x in order:
            if n >= target * 320:
                break
            if g[y][x] != '.' or rng.random() > 0.5 * xw[x]:
                continue
            if y >= 20 and rng.random() < 0.09 and free(y + 1, x + 3) and free(y, x + 2):
                # a small stone: lit top, shadow below to the right (light from the top left)
                for dx, dy, c in ((1, 0, '5'), (2, 0, '4'), (0, 1, '4'), (1, 1, '3'), (2, 1, '3'), (3, 1, '1')):
                    if free(y + dy, x + dx):
                        g[y + dy][(x + dx) % 320] = c
                        n += 1 if dy == 0 else 0
                continue
            wmax = 1 + int(4 * t * t + 1.2 * t)
            w = 1 + rng.randrange(wmax)
            c = weighted(rng, pens)
            for d in range(w):
                if free(y, x + d):
                    g[y][(x + d) % 320] = c
                    n += 1
            if t > 0.4 and w >= 3 and free(y, x - 1) and rng.random() < 0.7:
                g[y][(x - 1) % 320] = '5'                    # lit pixel, left
                n += 1
            if t > 0.5 and w >= 2 and free(y + 1, x + w) and rng.random() < 0.5:
                g[y + 1][(x + w) % 320] = '2' if t > 0.75 else '4'    # its shadow, below right


def paint_ground():
    """ground.txt rows 0, 1, 2 and 4 (lines 224-228, 1:1 with the playfield) were solid pen 6: a few small low
    contrast dust pixels (pens 5/7 near C, rarely 4/3) so the foreground does not start with a flat band.
    Sparse and one px: they must not compete with the tanks and people."""
    rng = random.Random(3109)
    lines = open(os.path.join(art, 'ground.txt')).read().split(chr(10))
    f = lines.index('frame') + 1
    xw = wander(rng, 320)
    for y in (0, 1, 2, 4):
        row = ['6'] * 320
        bn = blue_noise(320, 1, 40 + y)
        order = sorted(range(320), key=lambda x: bn[x])
        n = 0
        for x in order:
            if n >= 0.18 * 320:
                break
            if rng.random() > 0.5 * xw[x]:
                continue
            row[x] = weighted(rng, [('5', 40), ('7', 38), ('4', 14), ('3', 8)])
            n += 1
        lines[f + y] = ''.join(row)
    open(os.path.join(art, 'ground.txt'), 'w').write(chr(10).join(lines))


def main():
    paint_hills()
    paint_ground()
    write_art('plain_far.txt', [
        "The plain's far strip, 320x10 (game lines 170-179): the mountains' foot fading into the haze,",
        'dissolving along a wandering edge (swells, scree tongues, haze bays) with blue-noise scatter.',
        'Mountain palette (PF2 9-15). Loops at 320 with the mountains. Painted by art/tools/paint_plain.py.'],
        'mtn_pal.txt', paint_far())
    write_art('plain_floor_src.txt', [
        "The plain's perspective floor, 288x19: row j is game line 180 + j, uses columns",
        '0 .. 8 (18 + j) - 1 and loops there. Land palette (PF2 9-15). Dry lake, wash, road with posts,',
        'scrub and flats, drawn in world px and projected (docs/plain-spec.md s3). By art/tools/paint_plain.py.'],
        'land_pal.txt', paint_floor())


main()
