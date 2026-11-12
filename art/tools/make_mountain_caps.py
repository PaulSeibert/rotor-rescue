#!/usr/bin/env python3
"""Give every summit of art/mountains.txt a pointed top inside the band.

The pilot's mountains band (320x76) had its tallest summits cut flat: row 3, the
first row with any pixel, is 57 pixels wide, so the band's top edge sliced the
mountains. This finds each flat-topped summit (a run of columns that all reach
the band's highest row), cuts a pointed cap into it - a point on APEX_ROW with
three rows of empty sky above, widening SLOPE px per row each side until it
meets the mountain's own flank - and relights the new edge (lit left slope,
light crest), so no summit touches the band's top edge. Idempotent.
    python3 art/tools/make_mountain_caps.py
"""
import os
APEX_ROW = 3   # the point is on this row; rows 0..APEX_ROW-1 stay empty sky
SLOPE = 3.0    # px of half-width gained per row below the point
MIN_RUN = 4    # a flat run narrower than this is already a point
here = os.path.dirname(os.path.abspath(__file__))
path = os.path.join(here, '..', 'mountains.txt')
L = open(path).read().split('\n')
i = L.index('frame')
head = L[:i + 1]
rows = [list(l) for l in L[i + 1:] if l.strip()]
H, W = len(rows), len(rows[0])
top = [next((y for y in range(H) if rows[y][x] != '.'), H) for x in range(W)]
best = min(top)
runs, x = [], 0
while x < W:
    if top[x] <= best:
        s = x
        while x < W and top[x] <= best: x += 1
        runs.append((s, x - 1))
    else: x += 1
# a notch of up to 3 columns one or two rows down does not split a summit's flat
runs = [r for r in runs if r[1] - r[0] + 1 >= MIN_RUN]
print('flat summits (columns):', runs)
apex = [(s + e) / 2 for s, e in runs]
def cap(x):
    return min(APEX_ROW + max(abs(x - a) - 0.5, 0) / SLOPE for a in apex)
region = set()
for s, e in runs:
    for x in range(s, e + 1): region.add(x)
    for step, x in ((1, e + 1), (-1, s - 1)):
        while 0 <= x < W and cap(x) > top[x]:
            region.add(x); x += step
cut = set()
for x in region:
    for y in range(H):
        if rows[y][x] != '.' and y < cap(x) - 1e-6:
            rows[y][x] = '.'; cut.add((x, y))
# relight the new edge: left-facing edge pixels pen 6, the crest (open above) pen 5
for x in region:
    for y in range(min(H, 30)):
        c = rows[y][x]
        if c == '.': continue
        if x > 0 and rows[y][x - 1] == '.':
            if c < '6': rows[y][x] = '6'
        elif y > 0 and rows[y - 1][x] == '.':
            if c < '5': rows[y][x] = '5'
open(path, 'w').write('\n'.join(head + [''.join(r) for r in rows]) + '\n')
tops = [next(y for y in range(H) if rows[y][x] != '.') for x in range(W)]
print('highest row now', min(tops), 'in', tops.count(min(tops)), 'columns; widest first 3 rows:',
      [sum(c != '.' for c in rows[y]) for y in range(min(tops), min(tops) + 3)])
