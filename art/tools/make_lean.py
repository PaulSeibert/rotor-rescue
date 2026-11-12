#!/usr/bin/env python3
"""ROTOR RESCUE: the chopper's front-view lean frames (poses CHOP_LEAN1 and CHOP_LEAN2).

Our own art, made from our own front-view frames (art/chopper.txt frames 8-9) by column
and row shear, the way STYLE.md makes poses ("derived by column shear, never rotation"),
then touched up by the rules below. The right side goes low; art.toml's mirror = true makes
the lean to the left.

    python3 art/tools/make_lean.py      # rewrites frames 12-15 of art/chopper.txt (idempotent)

Frames 12-13: CHOP_LEAN1 (slight: rotor ends 1 px off level), 14-15: CHOP_LEAN2 (full: 2 px).
"""
import os

here = os.path.dirname(os.path.abspath(__file__))
path = os.path.join(here, '..', 'chopper.txt')

W, H = 32, 20
BASE_FRAMES = 12     # side x3, turn, front, wreck: 2 rotor phases each


def load():
    text = open(path).read()
    head, *frames = text.split('\nframe\n')
    frames = [f.strip('\n').split('\n') for f in frames]
    return head, frames[:BASE_FRAMES]


def lean(src, drop, shift, lift):
    """src: a front-view frame (20 rows of 32 chars); returns it banked to the right.
    drop: rows the rotor's right end sits below its left; shift: px the mast moves right
    (the body moves half of that); lift: rows the left skid is raised and the right lowered."""
    out = [['.'] * W for _ in range(H)]
    dyb = int(drop)              # the whole view sits lower by the tilt: room for the raised end
    dxb = shift // 2
    cx = 15.5
    # rotor (row 0): tilted by column shear
    for x in range(W):
        if src[0][x] != '.':
            y = dyb + int(round((x - cx) / cx * drop))
            out[y][x] = src[0][x]
    # mast (rows 1-2) and the body (rows 3-12): rigid, the mast leaning further
    for y in range(1, 13):
        dx = shift if y < 3 else dxb
        for x in range(W):
            if src[y][x] != '.' and 0 <= x + dx < W:
                out[y + dyb][x + dx] = src[y][x]
    # the mast joins a tilted rotor: fill the column under the rotor's centre
    # skids: legs run out diagonally from the body's foot to the bar
    yb = 12 + dyb
    for side, x0, step in ((0, 10 + dxb, -1), (1, 21 + dxb, 1)):
        n = 3 - lift if side == 0 else 3 + lift
        x = x0
        for i in range(n):
            out[yb + 1 + i][x] = 'K'
            x += step
        xe = x - step                  # the foot of the leg
        ys = yb + 1 + n
        if side == 0:
            row = (xe - 1, 'KDDDK')
        else:
            row = (xe - 3, 'KDDDK')
        for i, c in enumerate(row[1]):
            if 0 <= row[0] + i < W:
                out[ys][row[0] + i] = c
    return out


def close_gaps(g):
    """A step leaves a diagonal-only join in an outline; fill each with the outline pixel
    (K) so every outline stays 4-connected (STYLE.md section 4). (The rotor is not an
    outline: it is M and D, left alone.)"""
    for y in range(H - 1):
        for x in range(W - 1):
            a, b, c, d = g[y][x], g[y][x + 1], g[y + 1][x], g[y + 1][x + 1]
            if a == 'K' and d == 'K' and b == '.' and c == '.':
                g[y][x + 1] = 'K'
            elif b == 'K' and c == 'K' and a == '.' and d == '.':
                g[y][x] = 'K'
    return g


def main():
    head, frames = load()
    new = []
    for drop, shift, lift in ((1.0, 1, 0), (2.0, 2, 1)):
        for phase in range(2):
            new.append(lean(frames[8 + phase], drop, shift, lift))
    out = head
    for f in frames:
        out += '\nframe\n' + '\n'.join(f)
    for f in new:
        out += '\nframe\n' + '\n'.join(''.join(r) for r in f)
    open(path, 'w').write(out + '\n')
    for i, f in enumerate(new):
        print('frame', 12 + i)
        print('\n'.join(''.join(r) for r in f))


if __name__ == '__main__':
    main()
