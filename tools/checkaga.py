#!/usr/bin/env python3
"""Checks an A1200 screenshot of Rotor Rescue, pixel by pixel in 24-bit colour,
against what the AGA code should show:

  - the sky (game lines 24-195): COLOR00 in 24 bits on every line, as
    logicSkyColor24() in src/logic.c computes it (the mission's two colours
    are read from logic.c). A pixel that isn't the sky must be a 12-bit
    colour (x17 per channel: the playfields, sprites, text - all OCS colours).
    A pixel showing the sky's high nibbles only (the 12-bit colour the copper
    writes before setting LOCT) means the low nibbles came late: an error.
  - with --logo, the title logo (disk/titlelogo8.bin, 8 planes, pens 32-255 in
    24 bits) at its place, with the sky through pen 0. Mismatches that are
    12-bit colours are counted as sprites in front (the weather), anything
    else is an error.

    python3 tools/checkaga.py SHOT.png --mission M [--logo] [--last-line Y] [--min-sky-lines N]

--last-line: the last sky line to check (the title's mountains start at 162).
--min-sky-lines: how many lines must show some sky (default 100; lines the
mountains cover from edge to edge are reported, not counted).
Prints the counts; exit 1 on any error. Needs Pillow (~/amiga-tools/venv-img).
"""
import os
import re
import struct
import sys

from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
PLAY_TOP, SKY_END, SKY_LINES = 24, 196, 172
LOGO_W, LOGO_H, LOGO_X, LOGO_Y, LOGO_FIRST = 192, 129, 64, 27, 32


def sky_table(mission):
    src = open(os.path.join(ROOT, 'src', 'logic.c')).read()
    top = [int(v, 16) for v in re.search(r's_pSkyTop\[MISSION_COUNT\] = \{([^}]*)\}', src).group(1).split(',')]
    low = [int(v, 16) for v in re.search(r's_pSkyLow\[MISSION_COUNT\] = \{([^}]*)\}', src).group(1).split(',')]
    n = SKY_LINES - 1
    out = []
    for t in range(SKY_LINES):
        c = []
        for s in (8, 4, 0):
            a, b = ((top[mission] >> s) & 15) * 17, ((low[mission] >> s) & 15) * 17
            num = (b - a) * t
            c.append(a + ((num + n // 2) // n if num >= 0 else -((-num + n // 2) // n)))
        out.append(tuple(c))
    return out


def is12(c):
    return all(v % 17 == 0 for v in c)


def hi_only(c):
    return tuple((v >> 4) * 17 for v in c)


def main():
    a = sys.argv[1:]
    shot = a[0]
    mission = int(a[a.index('--mission') + 1]) if '--mission' in a else 0
    last = int(a[a.index('--last-line') + 1]) if '--last-line' in a else SKY_END - 1
    img = Image.open(shot).convert('RGB')
    assert img.size == (320, 256), f'screenshot is {img.size}'
    px = img.load()
    sky = sky_table(mission)
    errors, late, other, exact_min, notes = 0, 0, 0, 999, []
    sky_lines, covered = 0, []
    min_sky = int(a[a.index('--min-sky-lines') + 1]) if '--min-sky-lines' in a else 100
    logo = None
    if '--logo' in a:
        d = open(os.path.join(ROOT, 'disk', 'titlelogo8.bin'), 'rb').read()
        ncol = 256 - LOGO_FIRST
        hi = struct.unpack(f'>{ncol}H', d[:2 * ncol])
        lo = struct.unpack(f'>{ncol}H', d[2 * ncol:4 * ncol])
        pal = [tuple(((h >> s & 15) << 4) | (l >> s & 15) for s in (8, 4, 0)) for h, l in zip(hi, lo)]
        offs, rb = 4 * ncol, LOGO_W // 8
        logo = {}
        for y in range(LOGO_H):
            for x in range(LOGO_W):
                pen = 0
                for p in range(8):
                    if d[offs + (y * 8 + p) * rb + (x >> 3)] & (0x80 >> (x & 7)):
                        pen |= 1 << p
                logo[(LOGO_X + x, LOGO_Y + y)] = pal[pen - LOGO_FIRST] if pen else None
    logo_ok = logo_sprite = logo_bad = 0
    for y in range(PLAY_TOP, last + 1):
        want_sky = sky[y - PLAY_TOP]
        exact = 0
        for x in range(320):
            got = px[x, y]
            want = want_sky
            if logo is not None and (x, y) in logo:
                if logo[(x, y)] is not None:
                    want = logo[(x, y)]
                    if got == want:
                        logo_ok += 1
                    elif is12(got):
                        logo_sprite += 1
                    else:
                        logo_bad += 1
                        if len(notes) < 12:
                            notes.append(f'logo ({x},{y}): want {want}, got {got}')
                    continue
            if got == want:
                exact += 1
                if logo is not None and (x, y) in logo:
                    logo_ok += 1
            elif got == hi_only(want) and got != want:
                late += 1
                if len(notes) < 12:
                    notes.append(f'late low nibbles at ({x},{y}): got {got}, want {want}')
            elif not is12(got):
                errors += 1
                if len(notes) < 12:
                    notes.append(f'not sky, not 12-bit at ({x},{y}): got {got}, sky {want}')
            else:
                other += 1
        if exact:
            exact_min = min(exact_min, exact)
            sky_lines += 1
        else:
            covered.append(y)   # (the mountains cover a whole line lower down)
    print(f'{shot}: sky lines {PLAY_TOP}-{last} (mission {mission}): {sky_lines} lines with sky, at least {exact_min} '
          f'exact 24-bit sky pixels on each, {other} 12-bit pixels in front, {late} late-low-nibble pixels, {errors} errors'
          + (f'; lines {covered[0]}-{covered[-1]} ({len(covered)}) all covered' if covered else ''))
    if sky_lines < min_sky:
        errors += 1
        print(f'  only {sky_lines} lines show sky (want at least {min_sky})')
    if logo is not None:
        print(f'  logo: {logo_ok} pixels exact (of {LOGO_W * LOGO_H}), {logo_sprite} 12-bit pixels in front, {logo_bad} wrong')
    for n in notes:
        print('  ' + n)
    sys.exit(1 if errors or late or logo_bad else 0)


if __name__ == '__main__':
    main()
