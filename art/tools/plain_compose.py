#!/usr/bin/env python3
"""Composites the plain's bands the way the game's copper and bitmaps do (lines 24-255), per mission,
at any camera offset, for the mock-ups (plain_mockup.py) and the seam check (plain_check.py).
Pure Python (no PIL, no numpy): runs under WSL's python3 and Windows python alike.

Layers (docs/plain-spec.md s1): sky gradient (line-dithered, logic.c ditherFill) | mountains play band
(camera/4, lines 120-179, mountain palette) | floor (row j = line 180+j, camera (18+j)/72, land palette,
its ground gradient H -> C under it) | hills (camera/2, rows 2-26 at lines 199-223) | ground (camera, 224-255).
Not drawn: clouds, sprites (not part of the plain).
compose() returns (rgb, tag): rgb[line - 24][x] a 12-bit colour (0xRGB), tag[line - 24][x] 0 for the bare
backdrop (sky or ground gradient) or band * 8 + pen for a band pixel.
"""
import os

here = os.path.dirname(os.path.abspath(__file__))
art = os.environ.get('PLAIN_ART_DIR') or os.path.join(here, '..')

PLAY_TOP, SKY_LINES, FLOOR_Y, FLOOR_LINES, PERIOD = 24, 172, 180, 19, 576
MISSIONS = [
    dict(name='Desert Dusk', sky=(0x214, 0xE75),
         mtn=[0, 0x413, 0x524, 0x635, 0x746, 0x857, 0xA68, 0xC9A],
         land=[0, 0x310, 0x521, 0x742, 0x963, 0xB84, 0xDA6, 0xEC8]),
    dict(name='Mountain Night', sky=(0x001, 0x235),
         mtn=[0, 0x012, 0x123, 0x234, 0x345, 0x457, 0x79A, 0xBDE],
         land=[0, 0x010, 0x121, 0x232, 0x343, 0x354, 0x576, 0x798]),
    dict(name='Coastal Dawn', sky=(0x237, 0xFB8),
         mtn=[0, 0x546, 0x657, 0x768, 0x879, 0x98A, 0xBAB, 0xEDD],
         land=[0, 0x131, 0x242, 0x363, 0x585, 0x9A6, 0xCC8, 0xEEB]),
]
BAND_MTN, BAND_FLOOR, BAND_HILLS, BAND_GROUND = 1, 2, 3, 4


def read_rows(name):
    lines = open(os.path.join(art, name)).read().split('\n')
    return [l for l in lines[lines.index('frame') + 1:] if l.strip()]


def dither_fill(a, b, n):
    """logic.c ditherFill: a line-dithered gradient a (line 0) .. b (line n-1), 12-bit colours."""
    out = [0] * n
    nn = n - 1
    for s in (0, 4, 8):
        ca, cb = (a >> s) & 15, (b >> s) & 15
        d = 2 * (cb - ca)
        sgn = 1
        if d < 0:
            d, sgn = -d, -1
        q, r = 0, nn // 2
        for y in range(n):
            h = 2 * ca + sgn * q
            out[y] |= ((h >> 1) + ((h & 1) & (y & 1))) << s
            r += d
            while r >= nn:
                r -= nn
                q += 1
    return out


def _cdiv(a, b):
    q = abs(a) // abs(b)
    return q if (a >= 0) == (b > 0) else -q


def sky_color(m, idx):
    """logic.c logicSkyColor: the plain gradient (the haze H is this at line FLOOR_Y)."""
    a, b = m['sky']
    n = SKY_LINES - 1
    out = 0
    for s in (0, 4, 8):
        ca, cb = (a >> s) & 15, (b >> s) & 15
        out |= (ca + _cdiv((cb - ca) * idx + n // 2, n)) << s
    return out


class Art:
    def __init__(self, far=None, floor=None, hills=None, ground=None):
        self.mtn = read_rows('mountains.txt')[:50] + (far or read_rows('plain_far.txt'))
        self.floor = floor or read_rows('plain_floor_src.txt')
        self.hills = hills or read_rows('hills.txt')
        self.ground = ground or read_rows('ground.txt')


def compose(m, cam, A):
    sky = dither_fill(m['sky'][0], m['sky'][1], SKY_LINES)
    gg = dither_fill(sky_color(m, FLOOR_Y - PLAY_TOP), m['land'][6], FLOOR_LINES)
    C = m['land'][6]
    rgb, tag = [], []
    for line in range(PLAY_TOP, 256):
        if line < FLOOR_Y:
            bg = sky[line - PLAY_TOP]
        elif line < FLOOR_Y + FLOOR_LINES:
            bg = gg[line - FLOOR_Y]
        else:
            bg = C
        r = [bg] * 320
        t = [0] * 320
        src = None
        if 120 <= line < 180:
            src, band, pal, off = A.mtn[line - 120], BAND_MTN, m['mtn'], cam // 4
        elif FLOOR_Y <= line < FLOOR_Y + FLOOR_LINES:
            j = line - FLOOR_Y
            c = cam % PERIOD
            src, band, pal, off = A.floor[j][:8 * (18 + j)], BAND_FLOOR, m['land'], (c + c * j // 18) // 4
        elif 199 <= line < 224:
            src, band, pal, off = A.hills[line - 199 + 2], BAND_HILLS, m['land'], cam // 2
        elif line >= 224:
            src, band, pal, off = A.ground[line - 224], BAND_GROUND, m['land'], cam
        if src:
            w = len(src)
            for x in range(320):
                ch = src[(x + off) % w]
                if ch != '.':
                    p = int(ch)
                    r[x] = pal[p]
                    t[x] = 0 if (band in (BAND_HILLS, BAND_GROUND) and p == 6) else band * 8 + p   # pen 6 is C, the backdrop's own colour
        rgb.append(r)
        tag.append(t)
    return rgb, tag
