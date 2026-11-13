"""The sea in perspective: floor (28 rows, line 180-207), far strip, water tiles for the 1:1 strip."""
import numpy as np
from w2lib import *

def floor_geom(j):
    return (28 + 3 * j) / 112.0, 4 * (28 + 3 * j)      # speed of camera, loop width W_j px

def sea_floor(seed=21):
    r = rng(seed)
    rows = []
    for j in range(SEA_ROWS):
        s, W = floor_geom(j)
        row = np.zeros(W, np.uint8)
        if j == 0 or j == SEA_ROWS - 1:       # palette-switch rows: row 0 haze, row 27 empty
            rows.append(row); continue
        t = j / 26.0
        cover = 0.22 + 0.07 * t               # share of the row drawn (the solid rule asks >= 15 %)
        # density wander along world u (periods divide 448 so the row loops)
        ph = r.uniform(0, 6.28, 4)
        def dens(u):
            return 1.0 + 0.55 * (np.sin(2 * np.pi * 2 * u / 448 + ph[0]) + 0.7 * np.sin(2 * np.pi * 5 * u / 448 + ph[1])
                                 + 0.4 * np.sin(2 * np.pi * 9 * u / 448 + ph[2])) / 2.1
        target = cover * W
        drawn = 0; guard = 0
        two_row = j >= 9
        while drawn < target and guard < 3000:
            guard += 1
            u = r.uniform(0, 448)
            if r.uniform() > dens(u) / 1.6:
                continue
            Lw = r.uniform(6, 22) * (1.0 if t > .3 else 1.15)    # world length of the crest
            x0 = int(u * s); n = max(2, int(round(Lw * s)))
            if t < 0.35:   pen_c, pen_g = 4, 5
            elif t < 0.7:  pen_c, pen_g = 5, 6
            else:          pen_c, pen_g = 6, 6
            for k in range(n):
                x = (x0 + k) % W
                if row[x] == 0:
                    row[x] = pen_c; drawn += 1
            if t >= 0.35 and n >= 4:
                row[x0 % W] = pen_g if t < 0.7 else 7      # glint on the lit (left) end
                if t >= 0.7 and n >= 6: row[(x0 + 1) % W] = 7
            if two_row and j + 1 < SEA_ROWS - 1:
                pass
        rows.append(row)
    # second pass: trough shadows, 1 px down-right of crests, on rows >= 9 (darker pens)
    for j in range(9, SEA_ROWS - 2):
        s, W = floor_geom(j); s2, W2 = floor_geom(j + 1)
        nxt = rows[j + 1]
        for x in range(W):
            if rows[j][x] and rows[j][x - 1] == 0 or False:
                pass
        for x in range(W):
            if rows[j][x] >= 5:
                x2 = int(round((x + 1) * W2 / W)) % W2
                if nxt[x2] == 0 and r.uniform() < 0.35:
                    nxt[x2] = 3 if j < 17 else 2
                    if r.uniform() < 0.5:
                        nxt[(x2 + 1) % W2] = 3 if j < 17 else 2
    return rows

if __name__ == "__main__":
    rows = sea_floor()
    for j, r in enumerate(rows):
        print(j, len(r), round((r > 0).mean(), 2))
