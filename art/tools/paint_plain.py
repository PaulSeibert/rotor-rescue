#!/usr/bin/env python3
"""Paints the plain's two source strips (docs/plain-spec.md): art/plain_far.txt, art/plain_floor_src.txt.

Hand-placed pixel art written as code: the strips are tiny, so every mesa, the lake, the wash, the
road and the scrub are placed by number here (deterministic). Run, then make_plain.py:
    python3 art/tools/paint_plain.py && python3 art/tools/make_plain.py
The floor is drawn in "world" px (one 576 px period) and projected: world x on row j is
column x * W_j / 576, so every feature leans toward the vanishing point as the contract says.
"""
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


# ---------------------------------------------------------------- far strip
# (x of the left edge, top-face width, rows tall, talus: widens one px each side every 2 rows
#  below the cliff, 'butte' keeps vertical walls)
MESAS = [   # (left edge x, top-face width, top row y, talus skirt?); every one runs down into the haze
    (8, 26, 2, 1), (46, 9, 1, 0), (62, 17, 4, 1), (104, 6, 1, 0), (116, 30, 2, 1),
    (160, 12, 3, 1), (186, 5, 1, 0), (196, 4, 3, 0), (212, 28, 3, 1), (254, 14, 2, 1),
    (284, 18, 4, 1),
]


def paint_far():
    mtn = read_rows('mountains.txt')
    base = mtn[MTN_ROWS - 1]                       # row 0 continues row 49 x for x
    lift = [0, 1, 2, 3, 4, 4, 5, 5, 6, 6]          # the foot lightens toward the haze
    keep = [16, 11, 7, 5, 4, 4, 4, 5, 6, 3]        # Bayer coverage /16 of the mountains' foot (rows 8, 9: the haze rule)
    rows = [['.'] * 320 for _ in range(FAR_ROWS)]
    for y in range(FAR_ROWS):
        for x in range(320):
            if BAYER[y % 4][x % 4] < keep[y]:
                rows[y][x] = str(min(5, max(int(base[x]), int(base[x]) + lift[y])))   # pen 5 at most: night's 6-7 are bright
    cap = {8: 8, 9: 4}                              # rule 3: the foot's coverage cap on rows 8, 9
    for x0, w, top, talus in MESAS:
        h = FAR_ROWS - top
        for r in range(h):
            y = top + r
            grow = max(0, r - 3) if talus else 0     # a slope skirt at the base
            xa, xb = x0 - grow, x0 + w + grow       # [xa, xb)
            if r == 0:
                xa, xb = xa + 2, xb - 2             # the top face: corners chipped
            elif r == 1:
                xa, xb = xa + 1, xb - 1
            for x in range(xa, xb):
                if y in cap and BAYER[y % 4][x % 4] >= cap[y]:
                    rows[y][x % 320] = '.'          # the foot dissolves
                    continue
                lit = x - xa                          # columns from the lit (left) edge
                shade = xb - 1 - x                    # columns from the shaded (right) edge
                if r == 0:
                    c = 6 if shade > 0 else 4
                elif lit == 0:
                    c = 5
                elif shade == 0:
                    c = 2
                elif shade <= 2 and w > 5:
                    c = 3
                elif w > 14 and (x - x0) % 9 == 4 and r > 1:
                    c = 3                            # a crevice down the wall
                else:
                    c = 4 if (r + x) % 6 else 5     # a few strata flecks
                if y >= 7:
                    c = min(5, c + (y - 6))         # lifts into the haze at the foot
                rows[y][x % 320] = str(c)
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

    def bay(j, x, k):                                # Bayer test, k/16 coverage
        return BAYER[j % 4][x % 4] < k

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
    for wx in range(PERIOD):
        dash = wx % 36 < 12
        put(11, wx, '6' if dash else '3', 'road')
        if wx % 4 < 3:
            put(12, wx, '5', 'road')
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
    return rows


def main():
    write_art('plain_far.txt', [
        "The plain's far strip, 320x10 (game lines 170-179): the mountains' foot fading into the haze,",
        'with small distant mesas and buttes standing in it (lit from the top left, feet dissolving).',
        'Mountain palette (PF2 9-15). Loops at 320 with the mountains. Painted by art/tools/paint_plain.py.'],
        'mtn_pal.txt', paint_far())
    write_art('plain_floor_src.txt', [
        "The plain's perspective floor, 288x19: row j is game line 180 + j, uses columns",
        '0 .. 8 (18 + j) - 1 and loops there. Land palette (PF2 9-15). Dry lake, wash, road with posts,',
        'scrub and flats, drawn in world px and projected (docs/plain-spec.md s3). By art/tools/paint_plain.py.'],
        'land_pal.txt', paint_floor())


main()
