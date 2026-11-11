#!/usr/bin/env python3
"""Checks an OCS screenshot of Rotor Rescue's sky line by line against the
line-dithered gradient that logicSkyColorDither() in src/logic.c computes (the
mission's two colours are read from logic.c, the dither is done again here):
on every sky line (game lines 24-195) the colour most pixels show must be that
line's table colour, where that line shows enough sky.

    python3 tools/checksky.py SHOT.screen.png --mission M [--last-line Y] [--min-sky-lines N]

Prints the number of lines checked, the half-step lines (where a channel
alternates with its neighbour) and any line whose sky isn't its table value;
exit 1 on any. --table prints the table instead. Needs Pillow
(~/amiga-tools/venv-img).
"""
import os
import re
import sys
from collections import Counter

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
PLAY_TOP, SKY_END, SKY_LINES = 24, 196, 172


def sky_table(mission):
    src = open(os.path.join(ROOT, 'src', 'logic.c')).read()
    top = [int(v, 16) for v in re.search(r's_pSkyTop\[MISSION_COUNT\] = \{([^}]*)\}', src).group(1).split(',')]
    low = [int(v, 16) for v in re.search(r's_pSkyLow\[MISSION_COUNT\] = \{([^}]*)\}', src).group(1).split(',')]
    n = SKY_LINES - 1
    out = []
    for line in range(SKY_LINES):
        c = 0
        for s in (0, 4, 8):
            a, b = (top[mission] >> s) & 15, (low[mission] >> s) & 15
            num = 2 * (b - a) * line
            h = 2 * a + ((num + n // 2) // n if num >= 0 else -((-num + n // 2) // n))
            c |= ((h >> 1) + ((h & 1) & (line & 1))) << s
        out.append(c)
    return out


def rgb12(p):
    return ((p[0] // 17) << 8) | ((p[1] // 17) << 4) | (p[2] // 17)


def main():
    args = sys.argv[1:]
    mission = int(args[args.index('--mission') + 1]) if '--mission' in args else 0
    table = sky_table(mission)
    if '--table' in args:
        for y, c in enumerate(table):
            print(PLAY_TOP + y, f'{c:03X}')
        return 0
    from PIL import Image
    last = int(args[args.index('--last-line') + 1]) if '--last-line' in args else SKY_END - 1
    need = int(args[args.index('--min-sky-lines') + 1]) if '--min-sky-lines' in args else 60
    im = Image.open(args[0]).convert('RGB')
    checked = half = 0
    errors = []
    for gy in range(PLAY_TOP, last + 1):
        i = gy - PLAY_TOP
        want = table[i]
        prev = table[i - 1] if i else want
        nxt = table[i + 1] if i + 1 < SKY_LINES else want
        row = Counter(rgb12(im.getpixel((x, gy))) for x in range(320))
        # what a wrong sky would show: a neighbouring line's colour (the copper
        # a line late or early, or no dither)
        wrong = sum(row[c] for c in {prev, nxt} if c != want)
        if row[want] >= 20:
            checked += 1
            half += want != prev and prev == nxt
        elif wrong >= 20:
            errors.append(f'line {gy}: {wrong} px show a neighbouring line\'s colour, none {want:03X}')
    print(f'{checked} sky lines checked, {half} on a half step (alternating with both neighbours)')
    for e in errors:
        print('ERR', e)
    if checked < need:
        print(f'ERR only {checked} lines show enough sky (need {need})')
        return 1
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main())
