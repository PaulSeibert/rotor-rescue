#!/usr/bin/env python3
"""The plain between the mountains and the hills (docs/plain-spec.md, SPEC 8).

Sources (painted; --placeholder writes procedural stand-ins for both):
  art/plain_far.txt        320 x 10, the far strip: mountain palette (mtn_pal.txt),
                           loops at 320 with the mountains, x for x under their row 49
  art/plain_floor_src.txt  288 x 19, the floor, one row per game line 180-198, land
                           palette (land_pal.txt); row j uses columns 0 .. W_j - 1,
                           W_j = 8 (18 + j) (144 .. 288), the rest stays '.'
Writes (what art.toml converts; never edit these):
  art/mountains_play.txt   the mountains' rows 0-49 with the far strip below: the play
                           band (60 rows), camera / 4
  disk/plainfloor.bin      640 x 19, 3 interleaved planes (4560 bytes; agk packs it to
                           plainfloor.bin.pk): row j repeated every W_j px, so the copper
                           can start a fetch anywhere in the row's loop. On the floppy, not
                           in the program: the game loads it during the splash (AROS loads
                           the program at ~16 frames a KB against a 3000-frame boot window)
  build/art/preview/plain_floor.png   what the floor's 640 px rows hold (mission 1)
and checks the contract (rows 0 and 18 empty, pens 1-7, nothing past W_j, the far
strip's foot in haze). Deterministic.
    python3 art/tools/make_plain.py [--placeholder]
"""
import os
import struct
import sys
import zlib

FAR_ROWS, FLOOR_LINES, PERIOD = 10, 19, 576
MTN_ROWS = 50                       # mountains.txt rows shown in play (lines 120-169)
OUT_W = 640
BAYER = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]
here = os.path.dirname(os.path.abspath(__file__))
art = os.path.join(here, '..')


def row_w(j):
    return 8 * (FLOOR_LINES - 1 + j)


def read_art(name):
    lines = open(os.path.join(art, name)).read().split('\n')
    i = lines.index('frame')
    return lines[:i], [l for l in lines[i + 1:] if l.strip()]


def colors_block(pal_file):
    out = ['colors', '  .  transparent']
    for l in open(os.path.join(art, pal_file)).read().split('\n'):
        p = l.split()
        if len(p) >= 2 and not l.startswith('#') and p[0] != '0':
            out.append('  %s  %s' % (p[0], p[1]))
    return out


def write_art(name, comment, pal_file, rows):
    text = ['# ' + c for c in comment] + colors_block(pal_file) + ['frame'] + [''.join(r) for r in rows]
    open(os.path.join(art, name), 'w').write('\n'.join(text) + '\n')


def placeholder_far():
    # the mountains' rows 50-59 fading into the haze (ordered dither), with
    # simple block mesas standing in it, their feet dithered away too
    _, mtn = read_art('mountains.txt')
    rows = []
    for y in range(FAR_ROWS):
        keep = (FAR_ROWS - 1 - y) * 16 // (FAR_ROWS - 1)
        rows.append([c if c != '.' and BAYER[y % 4][x % 4] < keep else '.' for x, c in enumerate(mtn[MTN_ROWS + y])])
    for x0, w, h in ((18, 22, 5), (96, 14, 4), (150, 30, 6), (232, 18, 5), (284, 12, 3)):
        top = 8 - h
        for y in range(top, FAR_ROWS):
            inset = 0 if y > top else 1                       # a step in from the edges at the top
            for x in range(x0 + inset, x0 + w - inset):
                if y >= 8 and (BAYER[y % 4][x % 4] >= (8 if y == 8 else 4)):
                    continue                                    # the foot: 50 %, then 25 %
                c = '6' if y == top else '5' if x == x0 + inset else '2' if x == x0 + w - 1 - inset else '3'
                rows[y][x % 320] = c
    return rows


def placeholder_floor():
    # Drawn in "world" px (one period, 576 camera px) and projected: world x on
    # row j is screen x * W_j / 576. Flat features only: scrub dots (one line
    # high), a dry lake (an ellipse in plan, so it shears as perspective says),
    # a road with dashes across rows 11-12. Rows 0 and 18 empty.
    rows = [['.'] * 288 for _ in range(FLOOR_LINES)]

    def put(j, wx, c):
        x = int(wx * row_w(j) / PERIOD) % row_w(j)
        rows[j][x] = c

    seed = 12345

    def rnd(n):
        nonlocal seed
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
        return (seed >> 8) % n

    # the dry lake: plan ellipse centred at world x 300, rows 4-8
    for j in range(4, 9):
        hw = 110 * (1 - ((j - 6) / 2.6) ** 2) ** 0.5
        for wx in range(int(300 - hw), int(300 + hw)):
            put(j, wx, '7')
        put(j, 300 - hw, '5')
        put(j, 300 + hw, '5')
    # the road: row 11 the surface (pen 4) with dark dashes, row 12 its shadow edge
    for wx in range(PERIOD):
        put(11, wx, '2' if wx % 48 < 12 else '4')
        if wx % 8 < 4:
            put(12, wx, '1')
    # scrub: dots of two pixels, denser towards the viewer; a fleck of light
    # every so often on the far rows (enough detail to see each line move)
    for j in range(1, FLOOR_LINES - 1):
        if j in (11, 12):
            continue
        n = 10 + 3 * j
        for _ in range(n):
            wx = rnd(PERIOD)
            if 4 <= j <= 8 and abs(wx - 300) < 120:
                continue
            c = '7' if j < 4 else '13'[rnd(2)] if j > 12 else '23'[rnd(2)]
            put(j, wx, c)
            put(j, wx + PERIOD / row_w(j), c)        # 2 px wide
    return rows


def main():
    if '--placeholder' in sys.argv:
        write_art('plain_far.txt', [
            'The plain\'s far strip, 320x10 (game lines 170-179): the mountains band\'s last rows.',
            'Mountain palette (PF2 9-15, mission 1; logic.c has the others). Loops at 320 with the',
            'mountains. PLACEHOLDER from art/tools/make_plain.py --placeholder: the mountains\' foot',
            'faded into haze and block mesas. docs/plain-spec.md is the contract.'], 'mtn_pal.txt', placeholder_far())
        write_art('plain_floor_src.txt', [
            'The plain\'s perspective floor, 288x19: row j is game line 180 + j, uses columns',
            '0 .. 8 (18 + j) - 1 (the rest stays .) and loops there. Land palette (PF2 9-15).',
            'PLACEHOLDER from art/tools/make_plain.py --placeholder. docs/plain-spec.md is the contract.'],
            'land_pal.txt', placeholder_floor())

    problems = []
    _, mtn = read_art('mountains.txt')
    _, far = read_art('plain_far.txt')
    _, floor = read_art('plain_floor_src.txt')
    if len(far) != FAR_ROWS or any(len(r) != 320 for r in far):
        problems.append('plain_far.txt must be 320 x %d' % FAR_ROWS)
    if len(floor) != FLOOR_LINES or any(len(r) != 288 for r in floor):
        problems.append('plain_floor_src.txt must be 288 x %d' % FLOOR_LINES)
    for name, rows in (('plain_far.txt', far), ('plain_floor_src.txt', floor)):
        bad = {c for r in rows for c in r} - set('.1234567')
        if bad:
            problems.append('%s: only . and pens 1-7 (found %s)' % (name, ''.join(sorted(bad))))
    for y, cap in ((FAR_ROWS - 2, 0.5), (FAR_ROWS - 1, 0.25)):
        cover = sum(c != '.' for c in far[y]) / 320.0
        if cover > cap:
            problems.append('plain_far.txt row %d: %.0f%% covered, at most %.0f%% (its foot is haze)' % (y, cover * 100, cap * 100))
    for j in (0, FLOOR_LINES - 1):
        if any(c != '.' for c in floor[j]):
            problems.append('plain_floor_src.txt row %d must be empty (%s)' % (j, 'the haze; the land palette loads on it' if j == 0 else 'C, the seam with the hills'))
    for j, r in enumerate(floor):
        if any(c != '.' for c in r[row_w(j):]):
            problems.append('plain_floor_src.txt row %d: pixels past column %d (its loop)' % (j, row_w(j) - 1))
    if problems:
        print('\n'.join('make_plain: ' + p for p in problems))
        sys.exit(1)

    mhead, _ = read_art('mountains.txt')
    mhead = [l for l in mhead if not l.startswith('#')]
    text = ['# The mountains band as played, 320x60 (lines 120-179): mountains.txt rows 0-49, then the',
            '# plain\'s far strip (plain_far.txt). Made by art/tools/make_plain.py: edit those, not this.'] + \
        mhead + ['frame'] + mtn[:MTN_ROWS] + far
    open(os.path.join(art, 'mountains_play.txt'), 'w').write('\n'.join(text) + '\n')

    out = [[floor[j][u % row_w(j)] for u in range(OUT_W)] for j in range(FLOOR_LINES)]
    blob = bytearray()
    for r in out:                       # interleaved: per row plane 0, 1, 2 (80 bytes each)
        pens = [0 if c == '.' else int(c) for c in r]
        for p in range(3):
            for b in range(OUT_W // 8):
                v = 0
                for k in range(8):
                    v = (v << 1) | ((pens[b * 8 + k] >> p) & 1)
                blob.append(v)
    root = os.path.join(art, '..')
    os.makedirs(os.path.join(root, 'disk'), exist_ok=True)
    open(os.path.join(root, 'disk', 'plainfloor.bin'), 'wb').write(bytes(blob))
    # a preview: the 640 px rows in mission 1's land colours, transparent as a mid ground colour
    pal = [0xC86]
    for l in open(os.path.join(art, 'land_pal.txt')).read().split('\n'):
        q = l.split()
        if len(q) >= 2 and not l.startswith('#') and q[0] != '0':
            pal.append(int(q[1], 16))
    raw = bytearray()
    for r in out:
        raw.append(0)
        for c in r:
            x = pal[0 if c == '.' else int(c)]
            raw += bytes(((x >> 8 & 15) * 17, (x >> 4 & 15) * 17, (x & 15) * 17))

    def chunk(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xFFFFFFFF)
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', OUT_W, FLOOR_LINES, 8, 2, 0, 0, 0)) + \
        chunk(b'IDAT', zlib.compress(bytes(raw))) + chunk(b'IEND', b'')
    prev = os.path.join(root, 'build', 'art', 'preview')
    os.makedirs(prev, exist_ok=True)
    open(os.path.join(prev, 'plain_floor.png'), 'wb').write(png)
    print('make_plain: art/mountains_play.txt (320x%d), disk/plainfloor.bin (%d bytes), build/art/preview/plain_floor.png'
          % (MTN_ROWS + FAR_ROWS, len(blob)))


main()
