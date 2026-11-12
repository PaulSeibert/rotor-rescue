#!/usr/bin/env python3
"""art/title_mtn.txt: the title screen's mountain strip, 320x34.

The title shows only the top 34 rows of the mountains band (lines 162-195), so
the range would end in a hard line where the hills start. This takes those 34
rows from art/mountains.txt and fades the bottom ROWS_FADE of them into the
sky with an ordered (Bayer 4x4) dither, so the strip ends in haze, never in a
cut-off peak. Deterministic; run it after changing art/mountains.txt.
    python3 art/tools/make_title_mtn.py
"""
import os
H, ROWS_FADE = 34, 12
BAYER = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]
here = os.path.dirname(os.path.abspath(__file__))
src = open(os.path.join(here, '..', 'mountains.txt')).read().split('\n')
i = src.index('frame')
head = [l for l in src[:i] if not l.startswith('# Mountains')]
rows = [l for l in src[i + 1:] if l.strip()][:H]
out = []
for y, row in enumerate(rows):
    f = y - (H - ROWS_FADE)               # 0.. into the fade zone
    if f >= 0:
        keep = (ROWS_FADE - 1 - f) * 16 // (ROWS_FADE - 1)   # 16..0 of 16 levels
        row = ''.join(c if c == '.' or BAYER[y % 4][x % 4] < keep else '.' for x, c in enumerate(row))
    out.append(row)
text = ['# The title screen\'s mountain strip, 320x34: rows 0-33 of mountains.txt, the lowest',
        '# 12 rows dithered away into the sky (art/tools/make_title_mtn.py makes this file).'] + head + ['frame'] + out
open(os.path.join(here, '..', 'title_mtn.txt'), 'w').write('\n'.join(text) + '\n')
