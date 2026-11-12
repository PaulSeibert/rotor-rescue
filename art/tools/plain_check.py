#!/usr/bin/env python3
"""The seam check for the plain (docs/plain-spec.md s5): no background row may read as a line.
For each mission and each of 16 camera offsets it composes the screen (plain_compose.py) and scans
lines 160-205 (the far strip, the 179/180 palette switch, the floor, the 198/199 hills seam):
  1. run     the longest run of one band pen on a row;
  2. dither  the longest run of a regular period-2 or period-4 pattern of band pens / bare backdrop
             (a Bayer-like row), counted only where the pattern really alternates;
  3. edge    on each pair of adjacent lines, the longest run of columns whose colours differ strongly
             (what the eye reads as a hard horizontal edge); the smooth sky and ground gradients
             are below the threshold.
Anything over LIMIT px fails. Exit 1 on a failure.
    python3 art/tools/plain_check.py [--limit 64] [--verbose]      (PLAIN_ART_DIR=... checks another set)
"""
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import plain_compose as pc

Y0, Y1 = 160, 227
ROAD = range(190, 194)      # the road (floor rows 11-12) is a deliberate full-width feature, not a band seam
EDGES = list(range(165, 183)) + list(range(194, 203)) + list(range(215, 227))   # lines at the band seams: mountain foot, 179/180 palette switch, floor/hills, hills' foot/ground
# (the floor's interior, the dry lake and the road, are features Paul approved, not seams)
CAMS = [i * 36 for i in range(16)]
STRONG = 5      # sum of the channel steps (4-bit units) that counts as an edge


def longest_const(t):
    best = cur = 1
    for x in range(1, 320):
        cur = cur + 1 if t[x] == t[x - 1] and t[x] else 1
        best = max(best, cur)
    return best


def longest_period(t, P):
    """longest run where t[x] == t[x - P] holds, over a stretch that is not constant and not all backdrop"""
    best = cur = 0
    for x in range(P, 320):
        if t[x] == t[x - P]:
            cur += 1
        else:
            cur = 0
        if cur + P > best:
            w = t[x - cur - P + 1:x + 1]
            if len(set(w)) > 1 and any(w[i] != w[i + 1] for i in range(len(w) - 1)):
                # a period-P pattern that alternates (not a constant run, which `run` reports)
                if all(w[i] == w[i + P] for i in range(len(w) - P)) and not all(c == w[0] for c in w[:P]):
                    best = cur + P
    return best


def strong_run(a, b):
    best = cur = 0
    for x in range(320):
        d = sum(abs(((a[x] >> s) & 15) - ((b[x] >> s) & 15)) for s in (0, 4, 8))
        cur = cur + 1 if d >= STRONG else 0
        best = max(best, cur)
    return best


def main():
    limit = 64
    if '--limit' in sys.argv:
        limit = int(sys.argv[sys.argv.index('--limit') + 1])
    verbose = '--verbose' in sys.argv
    A = pc.Art()
    fails = 0
    for m in pc.MISSIONS:
        worst = dict(run=(0, 0, 0), dither2=(0, 0, 0), dither4=(0, 0, 0), edge=(0, 0, 0))
        for cam in CAMS:
            rgb, tag = pc.compose(m, cam, A)
            for line in range(Y0, Y1):
                if line not in EDGES:
                    continue
                t = tag[line - pc.PLAY_TOP]
                for key, v in (('run', longest_const(t)), ('dither2', longest_period(t, 2)),
                               ('dither4', longest_period(t, 4))):
                    if v > worst[key][0]:
                        worst[key] = (v, line, cam)
                if line + 1 in EDGES:
                    v = strong_run(rgb[line - pc.PLAY_TOP], rgb[line + 1 - pc.PLAY_TOP])
                    if v > worst['edge'][0]:
                        worst['edge'] = (v, line, cam)
        bad = [k for k, v in worst.items() if v[0] > limit]
        fails += len(bad)
        print('%-15s %s' % (m['name'], '  '.join('%s %d px (line %d, cam %d)' % ((k,) + v) for k, v in worst.items())),
              '-> FAIL ' + ','.join(bad) if bad else '-> ok (all <= %d px)' % limit)
    print('checked %d camera offsets x %d lines x 3 missions, limit %d px' % (len(CAMS), Y1 - Y0, limit))
    sys.exit(1 if fails else 0)


main()
