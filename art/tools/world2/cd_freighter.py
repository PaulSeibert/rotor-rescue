"""Slice D: the bulk freighter (freighter.txt, 4 sinking stages) and the sink spray (sink_spray.txt).
`python cd_freighter.py <art/world2 dir>`

Geometry (bow LEFT, bridge aft = RIGHT, one facing). Canvas 128 x 36; the BOB's last row (35) sits on BOAT_BOTTOM_Y 248
(row r = y 213 + r). Hull bottom outline (the waterline) = row 33 (y 246), foam rows 34-35. PIVOT = waterline centre, x 63.5, row 33.
The ship pitches about the pivot: deck edge row at x is  yd(x) = 19 - round(d * (x - 63.5) / 63.5)  with d = 0, 2, 5, 8 px for
stages 0-3 (bow DOWN d, stern UP d). Stage 3: the deck is 6 px above the water at the bow (row 27 vs row 33).
"""
import os
import sys

from cdlib import Cv, write_txt
from cd_ship import hsh

W, H = 128, 36
WL = 33
D0 = 19
PX = 63.5
DIPS = (0, 2, 5, 8)


def lift(x, d):
    return round(d * (x - PX) / PX)


def yd(x, d):
    return D0 - lift(x, d)


def xbow(h):
    """stem x at height h above the water (the stem is raked: further forward higher up)"""
    return 1 + round(9 * (1 - min(max(h, 0), 10) / 10) ** 1.5)


def xstern(h):
    return 126 - round(3 * (1 - min(max(h, 0), 22) / 22))


def hullmask(d):
    cells = {}
    for y in range(0, WL + 1):
        h = WL - y
        for x in range(xbow(h), xstern(h) + 1):
            if y >= yd(x, d):
                cells[(x, y)] = True
    return cells


def body(c, d):
    cells = hullmask(d)
    for (x, y) in cells:
        r = y - yd(x, d)
        ybt = WL - 3 - lift(x, d)                 # the boot-topping top row rides with the hull
        if y >= ybt and y <= WL - 1:
            k = y - ybt                                 # rows below the band's top edge
            col = "R"
            if k == 0 and hsh(x, y, 7) < 30:
                col = "O"                                # worn top edge of the paint
            elif k > 0 and k % 6 == 0:
                col = "K" if hsh(x, y, 8) > 28 else "R"   # weld line every 5 rows, broken
            elif (x + 5 * (k // 6)) % 19 == 0:
                col = "K"                                 # plate seams, staggered per band
            elif hsh(x, y, 11) < 7:
                col = "O"                                 # scuffs
        elif r == 0:
            col = "K" if (hsh(x, y) > 12 or x < 4 or x > 124) else "S"
        elif r == 1:
            col = "W" if hsh(x, y) > 25 else "S"
        elif r == 2:
            col = "S" if hsh(x, y) > 30 else "W"
        elif r in (6, 11):
            col = "K" if hsh(x, y) > 16 else "O"
        else:
            col = "O"
            if hsh(x, y, 14) < 3:
                col = "S"                                   # stone specks
            elif r in (5, 10) and hsh(x, y, 15) < 40:
                col = "K"                                   # weld shadow under the seam
        c.set(x, y, col)
    # plates: vertical seams per band, lit top-left corner, rivet
    for (r0, r1, off, pitch) in ((3, 5, 7, 20), (7, 10, 17, 19), (12, 22, 3, 23)):
        xs = off
        while xs < W:
            for r in range(r0, r1 + 1):
                y = yd(xs, d) + r
                if (xs, y) in cells and c.get(xs, y) in ("O", "S") and y < WL - 4 - lift(xs, d):
                    c.set(xs, y, "K")
            y0 = yd(xs + 1, d) + r0
            if (xs + 1, y0) in cells and y0 < WL - 3 - lift(xs + 1, d):
                c.set(xs + 1, y0, "S")
                if hsh(xs, r0, 4) > 40:
                    c.set(xs + 2, y0, "S")
                if r1 > r0 and c.get(xs + 1, y0 + 1) == "O":
                    c.set(xs + 1, y0 + 1, "S")
            xs += pitch
    # rust trails (red-brown) from scuppers
    for sx, r0, ln in ((19, 3, 3), (47, 7, 4), (71, 3, 3), (92, 12, 4), (35, 12, 3), (57, 7, 2), (108, 7, 3)):
        for k in range(ln):
            y = yd(sx, d) + r0 + k
            if c.get(sx, y) == "O":
                c.set(sx, y, "R" if k < ln - 1 else "S")
    # outline: any hull pixel touching the sea or the sky, except along the waterline
    for (x, y) in cells:
        if y == WL:
            continue
        for dx, dy in ((1, 0), (-1, 0), (0, -1)):
            if (x + dx, y + dy) not in cells and c.get(x + dx, y + dy) == ".":
                c.set(x, y, "K")
    for x in range(W):                               # the waterline: dashed outline, the sea shows between
        if (x, WL) in cells:
            c.set(x, WL, "K" if x % 2 == 0 else "R")
        if (x, WL) in cells and x % 2 == 1 and (x - 1, WL) in cells:
            c.set(x, WL + 1, "K") if hsh(x, WL) < 55 else None


def put_rel(c, d, x, rel, col, only=False):
    """a deck-gear pixel at rel rows above the deck edge at column x (rel -1 = the row just above the edge)"""
    y = yd(x, d) + rel
    if only:
        c.only(x, y, col)
    else:
        c.set(x, y, col)


def box(c, d, x0, x1, top, bot, fill, edge="K", lit="W"):
    """a gear box on the deck: rows top..bot above the edge (negative), outlined, lit left and top"""
    for x in range(x0, x1 + 1):
        for rel in range(top, bot + 1):
            put_rel(c, d, x, rel, fill)
        put_rel(c, d, x, top, edge)
    for rel in range(top, bot + 1):
        put_rel(c, d, x0, rel, edge)
        put_rel(c, d, x1, rel, edge)
    if x1 - x0 > 3:
        for x in range(x0 + 1, x1 - 1, 2):
            put_rel(c, d, x, bot, lit if bot > top + 1 else fill)


def gear(c, d):
    # bow: windlass and a short mast, then hatch 1 and the crane
    box(c, d, 5, 10, -2, -1, "S")
    for rel in range(-6, 0):
        put_rel(c, d, 8, rel, "K")
    put_rel(c, d, 7, -5, "K")
    put_rel(c, d, 9, -5, "K")
    put_rel(c, d, 8, -6, "S")
    box(c, d, 14, 31, -2, -1, "S")                   # hatch 1, closed
    for x in (19, 24, 29):
        put_rel(c, d, x, -1, "K")
    put_rel(c, d, 15, -1, "W")
    put_rel(c, d, 16, -1, "W")
    # crane: king post, a boom rising forward (up-left) and the hook line
    for rel in range(-8, 0):
        put_rel(c, d, 37, rel, "K")
        put_rel(c, d, 38, rel, "S")
    put_rel(c, d, 36, -1, "K")
    put_rel(c, d, 39, -1, "K")
    for k in range(0, 12):                            # boom: 10 px forward, 7 up
        put_rel(c, d, 37 - k, -8 - (k * 6) // 11, "K")
        put_rel(c, d, 38 - k, -8 - (k * 6) // 11, "K") if k < 2 else None
    for rel in range(-13, -4):                         # hook line from the boom tip
        put_rel(c, d, 26, rel, "K")
    put_rel(c, d, 26, -4, "S")
    put_rel(c, d, 25, -4, "K")
    put_rel(c, d, 27, -4, "K")
    # the OPEN hold amidships: dark opening, folded covers at the ends; nothing taller than 2 px over x 44-82
    for x in range(44, 83):
        put_rel(c, d, x, -2, "K")
        put_rel(c, d, x, -1, "K")
    for x in range(50, 77):
        if hsh(x, 3) > 70:
            put_rel(c, d, x, -1, "O")
        if hsh(x, 5) > 82:
            put_rel(c, d, x, -1, "S")
    for x in range(44, 50):
        put_rel(c, d, x, -1, "S")
        put_rel(c, d, x, -2, "K")
    for x in range(77, 83):
        put_rel(c, d, x, -1, "S")
    put_rel(c, d, 45, -1, "W")
    put_rel(c, d, 46, -1, "W")
    put_rel(c, d, 78, -1, "W")
    # hatch 3 aft of the hold
    box(c, d, 86, 98, -2, -1, "S")
    for x in (91, 95):
        put_rel(c, d, x, -1, "K")
    put_rel(c, d, 87, -1, "W")


def bridge(c, d):
    # house x 102-124: three decks, wheelhouse with windows, funnel aft (x 117-122); rows are relative to the deck edge
    for x in range(102, 125):                                   # main house, rel -1..-4
        for rel in range(-4, 0):
            put_rel(c, d, x, rel, "W")
        put_rel(c, d, x, -4, "K")
        put_rel(c, d, x, -1, "S")
    for rel in range(-4, 0):
        put_rel(c, d, 102, rel, "K")
        put_rel(c, d, 124, rel, "K")
    for x in range(104, 123, 3):
        put_rel(c, d, x, -3, "B")
        put_rel(c, d, x + 1, -3, "B")
    put_rel(c, d, 113, -3, "Y")
    put_rel(c, d, 114, -3, "Y")
    for rel in (-3, -2):
        put_rel(c, d, 123, rel, "S")
    # bridge block x 104-115, rel -5..-7, wing overhang one px each side at the wheelhouse
    for x in range(104, 116):
        for rel in range(-7, -4):
            put_rel(c, d, x, rel, "W")
        put_rel(c, d, x, -7, "K")
    for rel in range(-7, -4):
        put_rel(c, d, 104, rel, "K")
        put_rel(c, d, 115, rel, "K")
    for x in range(106, 114):
        put_rel(c, d, x, -6, "B")
    put_rel(c, d, 107, -6, "S")
    put_rel(c, d, 111, -6, "Y")
    for x in range(105, 115):
        put_rel(c, d, x, -5, "S")
    # wheelhouse top rel -8..-9
    for x in range(106, 114):
        put_rel(c, d, x, -8, "W")
        put_rel(c, d, x, -9, "K")
    put_rel(c, d, 106, -8, "K")
    put_rel(c, d, 113, -8, "K")
    for x in range(107, 113):
        put_rel(c, d, x, -8, "B")
    put_rel(c, d, 108, -8, "S")
    # mast
    put_rel(c, d, 110, -10, "K")
    # funnel x 117-122, rel -5..-10: black cap, red, white band
    for x in range(117, 123):
        for rel in range(-10, -4):
            put_rel(c, d, x, rel, "R")
        put_rel(c, d, x, -10, "K")
        put_rel(c, d, x, -8, "W")
    for rel in range(-10, -4):
        put_rel(c, d, 117, rel, "K")
        put_rel(c, d, 122, rel, "K")
    put_rel(c, d, 118, -9, "Y")
    put_rel(c, d, 118, -7, "Y")
    put_rel(c, d, 121, -7, "K")
    put_rel(c, d, 121, -9, "K")


PUFF_S = (".WWS.",
          "WWSSS",
          ".SSS.")
PUFF_M = ("..WWS..",
          ".WWWSS.",
          "WWWSSSS",
          ".SSSSS.")
PUFF_L = ("...WWWS...",
          ".WWWWSSS..",
          "WWWWSSSSS.",
          "WWWSSSSSSS",
          ".SSSSSSSS.")
WISP = ("W.S",
        ".SS",
        "S..")


def stamp(c, pat, x, y):
    """pattern top-left at (x, y); only onto transparent pixels; a few K flecks on the dark underside"""
    for dy, row in enumerate(pat):
        for dx, ch in enumerate(row):
            if ch != ".":
                c.only(x + dx, y + dy, ch)
    h, w = len(pat), len(pat[0])
    for dx in range(w):
        for dy in range(h):
            if pat[dy][dx] != "." and (dy == h - 1 or pat[dy + 1][dx] == ".") and hsh(x + dx, y + dy, 13) < 35:
                c.only(x + dx, y + dy + 1, "K")


def steam(c, d, stage):
    """steam from the forward hatch and the funnel's lee side: a wisp at stage 2, puffs at stage 3 (the funnel's own top
    is too close to the frame top at stage 3 to carry any)"""
    if stage < 2:
        return
    hx = 90
    base = yd(hx, d) - 3
    if stage == 2:
        stamp(c, PUFF_S, hx, base - 3)
    else:
        stamp(c, PUFF_M, hx - 1, base - 4)
        stamp(c, PUFF_S, hx + 5, base - 7)
        stamp(c, WISP, hx + 9, base - 10)
        top = max(yd(124, d) - 10, 1)
        stamp(c, PUFF_S, 123, top + 1)


def bowwash(c, d, stage):
    """foam climbing the stem at the bow and washing over the foredeck as the bow goes under"""
    if stage == 0:
        pts = ((1, WL), (3, WL + 1), (9, WL + 1), (12, WL + 2))
    elif stage == 1:
        pts = ((2, WL - 1), (3, WL), (5, WL), (2, WL + 1), (8, WL + 1), (13, WL + 2), (4, WL + 2), (10, WL + 2))
    elif stage == 2:
        pts = ((4, WL - 3), (5, WL - 3), (3, WL - 2), (4, WL - 2), (6, WL - 2), (2, WL - 1), (3, WL - 1), (5, WL - 1),
               (7, WL - 1), (9, WL - 1), (1, WL), (4, WL), (8, WL), (10, WL), (3, WL + 1), (6, WL + 1), (11, WL + 1),
               (2, WL + 2), (13, WL + 2), (8, WL + 2), (16, WL + 1))
    else:
        pts = ((6, WL - 7), (7, WL - 7), (5, WL - 6), (6, WL - 6), (8, WL - 6), (4, WL - 5), (5, WL - 5), (7, WL - 5),
               (9, WL - 5), (3, WL - 4), (4, WL - 4), (6, WL - 4), (8, WL - 4), (10, WL - 4), (12, WL - 4), (2, WL - 3),
               (5, WL - 3), (7, WL - 3), (9, WL - 3), (11, WL - 3), (14, WL - 3), (1, WL - 2), (3, WL - 2), (6, WL - 2),
               (8, WL - 2), (10, WL - 2), (13, WL - 2), (17, WL - 2), (2, WL - 1), (4, WL - 1), (7, WL - 1), (11, WL - 1),
               (15, WL - 1), (19, WL - 1), (1, WL), (5, WL), (9, WL), (12, WL), (18, WL), (3, WL + 1), (8, WL + 1),
               (14, WL + 1), (20, WL + 1), (2, WL + 2), (6, WL + 2), (11, WL + 2), (17, WL + 2), (22, WL + 2))
    for k, (x, y) in enumerate(pts):
        c.set(x, y, "W" if k % 3 != 1 else "S")


def sea_flecks(c, d, stage):
    for x, y, col in ((24, 34, "S"), (30, 35, "W"), (42, 34, "W"), (55, 35, "S"), (66, 34, "S"), (75, 35, "W"), (88, 34, "S"),
                      (97, 35, "S"), (108, 34, "W"), (115, 35, "S"), (121, 34, "W"), (124, 35, "S")):
        c.set(x, y, col)
    for x in range(100, 126):
        if hsh(x, 5) < 40:
            c.set(x, 35 if stage < 3 else 34, "S" if hsh(x, 6) > 40 else "W")


def freighter(stage):
    d = DIPS[stage]
    c = Cv(W, H)
    body(c, d)
    gear(c, d)
    bridge(c, d)
    sea_flecks(c, d, stage)
    bowwash(c, d, stage)
    steam(c, d, stage)
    return c


# ---- sink spray: 24 x 12, 3 frames: the bottom row (11) is the waterline; foam and steam grow with the stage
def spray(stage):
    c = Cv(24, 12)
    def fl(pts):
        for k, (x, y) in enumerate(pts):
            c.set(x, y, "W" if k % 3 != 1 else "S")
    base = [(2, 11), (5, 11), (9, 11), (13, 11), (17, 11), (21, 11)]
    if stage == 0:
        fl(base + [(3, 10), (7, 10), (12, 10), (18, 10), (6, 9), (15, 9)])
    elif stage == 1:
        fl(base + [(1, 10), (3, 10), (4, 10), (7, 10), (8, 10), (11, 10), (13, 10), (16, 10), (19, 10), (22, 10),
                   (3, 9), (7, 9), (12, 9), (17, 9), (5, 8), (9, 8), (14, 8), (8, 7)])
    else:
        fl(base + [(0, 10), (1, 10), (3, 10), (4, 10), (6, 10), (7, 10), (9, 10), (10, 10), (12, 10), (13, 10), (15, 10),
                   (16, 10), (18, 10), (19, 10), (21, 10), (22, 10), (2, 9), (4, 9), (5, 9), (8, 9), (10, 9), (13, 9),
                   (15, 9), (18, 9), (20, 9), (3, 8), (6, 8), (9, 8), (12, 8), (16, 8), (19, 8)])
        stamp(c, PUFF_M, 3, 3)
        stamp(c, PUFF_S, 13, 2)
        stamp(c, WISP, 9, 0)
    return c


def build(outdir):
    write_txt(os.path.join(outdir, "freighter.txt"),
              "Bulk freighter 128 x 36 (slice D, the timed sinking freighter, mission 6): 4 frames = the sinking stages 0 level, 1 bow down 2 px,\n"
              "2 bow down 5 px with a wisp of steam, 3 bow down 8 px (deck 6 px above the water at the bow, awash foam, steam). One facing, bow left, bridge aft.\n"
              "The BOB's last row (35) sits on BOAT_BOTTOM_Y 248 (row r = y 213 + r); the waterline outline is row 33 (y 246), PIVOT = (63.5, 33).\n"
              "The ship pitches about the pivot: deck edge row = 19 - round(d (x - 63.5) / 63.5), d = 0 / 2 / 5 / 8. The open hold is x 44-82 (2 px coaming),\n"
              "the hover box is x 52-76 (24 px) centred on x 64 where the deck moves at most 1 px between stages.",
              [freighter(s) for s in range(4)])
    write_txt(os.path.join(outdir, "sink_spray.txt"),
              "Sinking spray 24 x 12, 3 frames (stages 1-3): foam and steam along the waterline, the bottom row (11) is the water; it grows with the stage.\n"
              "Meant for the bow (centre x about 12 from the bow tip) but free to place anywhere along the hull.",
              [spray(s) for s in range(3)])


if __name__ == "__main__":
    build(sys.argv[1] if len(sys.argv) > 1 else ".")
