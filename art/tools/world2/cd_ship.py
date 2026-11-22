"""Slice C: the moored cargo ship (deck_ship.txt), the winch rope / ladder / hook (winch_rope.txt, winch_hook.txt)
and the climbing man (man_climb.txt). Pure Python, deterministic: `python cd_ship.py <art/world2 dir>`.

Ship geometry (bow LEFT, bridge and funnel aft = RIGHT, one facing, the ship does not move):
  canvas 124 x 30, the BOB's last row (29) sits on BOAT_BOTTOM_Y 248, so row r is at y = 219 + r;
  deck top edge = row 8 (y 227, inside the brief's 226-228), hull bottom outline = row 27 (y 246), foam rows 28-29.
"""
import os
import sys

from cdlib import Cv, write_txt

W, H = 124, 30
DK = 8      # deck edge row
WL = 27     # hull bottom (waterline) row


def hsh(x, y, k=0):
    """deterministic 0..99 noise (no ordered pattern)"""
    v = (x * 73856093) ^ (y * 19349663) ^ (k * 83492791)
    v ^= v >> 13
    v = (v * 1274126177) & 0xFFFFFFFF
    return (v >> 8) % 100


def xl(r):
    t = (r - DK) / (WL - DK)
    return 3 + round(11 * t ** 1.5)


def xr(r):
    t = (r - DK) / (WL - DK)
    return 120 - round(5 * t ** 1.2)


# plate grid: horizontal seam rows and the vertical seams of each plate row (offset, pitch)
SEAMS = (14, 19)
BANDS = ((11, 13, 4, 19), (15, 18, 13, 17), (20, 22, 0, 21))   # (first row, last row, x offset, pitch)


def hull(c):
    for r in range(DK, WL + 1):
        a, b = xl(r), xr(r)
        for x in range(a, b + 1):
            if r == DK:
                c.set(x, r, "K" if (hsh(x, r) > 13 or x in (a, b) or x < 24 or x > 85) else "S")
            elif x == a or x == b:
                c.set(x, r, "K")
            elif r == WL:
                c.set(x, r, "K" if x % 2 == 0 else "O")
            elif r == 9:
                c.set(x, r, "W" if hsh(x, r) > 20 else "S")
            elif r == 10:
                c.set(x, r, ("W" if hsh(x, r) < 22 else "S") if x < 64 else ("S" if hsh(x, r) > 25 else "B"))
            elif r in SEAMS:
                c.set(x, r, "K" if hsh(x, r) > 14 else "S")
            elif r in (23, 24):
                c.set(x, r, "R" if hsh(x, r) > 6 else "K")
            elif r in (25, 26):
                c.set(x, r, "O" if hsh(x, r) > 20 else "K")
            else:
                c.set(x, r, "B")
    # plates: vertical seams, a lit top-left corner and a rivet on each plate
    for (r0, r1, off, pitch) in BANDS:
        xs = off
        while xs < W:
            for r in range(r0, r1 + 1):
                if xl(r) + 2 < xs < xr(r) - 2:
                    c.set(xs, r, "K")
            if xl(r0) + 2 < xs + 1 < xr(r0) - 3:
                c.set(xs + 1, r0, "S")
                c.set(xs + 2, r0, "S" if hsh(xs, r0, 1) > 30 else "W")
                if r1 > r0:
                    c.set(xs + 1, r0 + 1, "S")
                c.set(xs + 4, r0 + 1, "W" if hsh(xs, r0, 2) > 45 else "B")
            xs += pitch
    # plate texture: stone specks on the lit upper-left of each plate, a dark weld shadow under every seam
    for r in range(11, 23):
        if r in SEAMS:
            continue
        for x in range(xl(r) + 2, xr(r) - 1):
            if c.get(x, r) == "B":
                if hsh(x, r, 5) < 3:
                    c.set(x, r, "S")
                elif r + 1 in SEAMS and hsh(x, r, 6) < 40:
                    c.set(x, r, "K")
    # rust streaks under scuppers: olive-brown trails from a seam
    for sx, y0, ln in ((24, 11, 3), (43, 15, 4), (60, 11, 3), (79, 15, 3), (97, 20, 2), (33, 20, 3), (70, 20, 2)):
        for k in range(ln):
            c.set(sx, y0 + k, "O")
        c.set(sx + 1, y0, "O")
    # anchor and hawse at the bow, a few portholes aft
    c.rect(11, 12, 13, 14, "K")
    c.set(11, 12, "S")
    c.set(12, 12, "S")
    for px in (91, 96, 101, 106, 111):
        c.set(px, 12, "Y")
        c.set(px, 13, "K")


def deckhouse(c):
    # aft deckhouse x 86-116 rows 4-7, wheelhouse x 92-104 rows 1-3, funnel x 108-114 rows 0-3
    c.rect(86, 4, 116, 7, "W")
    for x in range(86, 117):
        c.set(x, 4, "K" if hsh(x, 4) > 22 else "S")
    for r in (5, 6, 7):
        c.set(86, r, "K")
        c.set(116, r, "K")
    for x in range(87, 116):                    # shade: the lower and the aft side
        c.set(x, 7, "S")
    for r in (5, 6):
        c.set(115, r, "S")
    for x in (89, 93, 97, 101, 105, 109, 112):  # windows: blue, a lit yellow one or two
        c.set(x, 5, "B")
        c.set(x + 1, 5, "B")
    c.set(97, 5, "Y")
    c.set(98, 5, "Y")
    c.set(109, 5, "Y")
    # wheelhouse
    c.rect(92, 1, 104, 3, "W")
    for x in range(92, 105):
        c.set(x, 1, "K")
    for r in (2, 3):
        c.set(92, r, "K")
        c.set(104, r, "K")
    c.set(103, 3, "S")
    c.set(103, 2, "S")
    for x in range(93, 103):
        c.set(x, 2, "B")
    c.set(94, 2, "S")
    c.set(95, 2, "S")
    c.set(100, 2, "Y")
    # radar bar and mast
    c.set(97, 0, "K")
    c.set(98, 0, "K")
    c.set(99, 0, "K")
    c.set(98, 1, "K")
    # funnel: black cap, red body, yellow band, outline both sides
    c.rect(108, 0, 114, 3, "R")
    c.rect(108, 0, 114, 0, "K")
    for r in range(1, 4):
        c.set(108, r, "K")
        c.set(114, r, "K")
    for x in range(109, 114):
        c.set(x, 2, "Y")
    c.set(109, 2, "W")
    c.set(109, 1, "Y")
    c.set(113, 3, "K")
    c.set(113, 1, "K")


def deckgear(c):
    # raised forecastle x 5-22 (rows 5-7), a foremast with a derrick boom and a hatch behind it; x 28-84 stays bare
    c.rect(5, 5, 22, 7, "W")
    for x in range(5, 23):
        c.set(x, 5, "K")
    c.set(5, 6, "K")
    c.set(5, 7, "K")
    c.set(22, 6, "K")
    c.set(22, 7, "K")
    for x in range(6, 22):
        c.set(x, 7, "S")
    for x in (8, 12, 16, 19):
        c.set(x, 6, "B")                         # forecastle ports
    c.set(13, 6, "Y")
    # mast with cross-tree and a boom to the hatch
    for y in range(0, 5):
        c.set(14, y, "K")
    c.set(13, 1, "K")
    c.set(15, 1, "K")
    c.set(14, 0, "S")
    c.line(14, 3, 25, 6, "K")
    c.set(14, 2, "S")
    # hatch
    c.rect(26, 6, 33, 7, "S")
    for x in range(26, 34):
        c.set(x, 6, "K")
    c.set(26, 7, "K")
    c.set(33, 7, "K")
    c.set(27, 7, "W")
    c.set(28, 7, "W")
    for bx in (118,):
        c.set(bx, 7, "K")
        c.set(bx, 6, "K")
    # stern rail posts
    for x in (117, 119):
        c.set(x, 6, "K")


def foam(c, wake):
    """waterline foam rows 27-29; frame 1 has the bow wake and a longer stern wash"""
    a, b = xl(WL), xr(WL)
    for x in range(a + 1, b):
        if x % 2 == 1:
            c.set(x, 28, "K")
    for x, y, col in ((18, 29, "S"), (26, 28, "W"), (33, 29, "S"), (47, 29, "S"), (52, 28, "W"), (66, 29, "S"),
                      (81, 29, "S"), (88, 28, "W"), (99, 29, "S"), (110, 28, "S")):
        c.set(x, y, col)
    if wake == 0:
        for x, y, col in ((9, 28, "W"), (11, 29, "W"), (6, 29, "S"), (14, 29, "W"), (117, 28, "W"), (119, 29, "S"), (121, 29, "W")):
            c.set(x, y, col)
    else:
        # bow wave: a crest climbing the stem, foam trailing aft
        for x, y, col in ((6, 26, "W"), (7, 26, "W"), (5, 27, "W"), (6, 27, "S"), (7, 27, "W"), (8, 27, "W"),
                          (3, 28, "W"), (4, 28, "W"), (5, 28, "S"), (6, 28, "W"), (8, 28, "W"), (9, 28, "W"),
                          (1, 29, "W"), (2, 29, "S"), (4, 29, "W"), (5, 29, "W"), (7, 29, "S"), (9, 29, "W"),
                          (11, 29, "W"), (12, 28, "W"), (15, 29, "W"), (22, 28, "S"), (118, 28, "W"), (120, 28, "W"),
                          (117, 29, "W"), (119, 29, "W"), (122, 29, "S"), (121, 28, "S"), (123, 29, "W"), (30, 29, "S"),
                          (38, 28, "S"), (44, 29, "W"), (58, 28, "S"), (72, 29, "W"), (93, 29, "W"), (105, 29, "W")):
            c.set(x, y, col)


def deck_ship(wake):
    c = Cv(W, H)
    hull(c)
    deckhouse(c)
    deckgear(c)
    foam(c, wake)
    return c


# ---- the winch rope and ladder: 8 x 40, 3 frames; the top row is the skid hook-up point (centre x = 3.5)
RW, RH = 8, 40


def rope_plain():
    """frame 0: a hanging rope with knots and a weighted end: 'KWK'-wide, lit left"""
    c = Cv(RW, RH)
    for y in range(RH):
        c.set(2, y, "K")
        c.set(3, y, "W" if y % 4 != 3 else "S")
        c.set(4, y, "S")
        c.set(5, y, "K")
    for y in range(5, RH, 8):                  # knots
        c.rect(2, y, 5, y + 1, "K")
        c.set(3, y, "W")
        c.set(3, y + 1, "S")
        c.set(4, y, "S")
    # shackle at the top
    c.rect(1, 0, 6, 0, "K")
    c.set(3, 0, "S")
    c.set(4, 0, "S")
    # the weighted tip
    c.rect(2, 37, 5, 39, "K")
    c.set(3, 37, "S")
    c.set(4, 37, "O")
    c.set(3, 38, "O")
    c.set(4, 38, "O")
    return c


def ladder_sway(direction):
    """a rope ladder (rails 'W/S' + 'K', rungs every 5 rows): the top stays on the skid, the foot swings 2 px to one side;
    the 1-px steps sit on rung rows (y 12 and 27) so a rung bridges each step"""
    c = Cv(RW, RH)
    for y in range(RH):
        step = 0 if y < 13 else (1 if y < 28 else 2)
        x0 = 2 + direction * step
        for rx in (x0, x0 + 3):
            c.set(rx, y, "W" if (y // 3) % 2 == 0 else "S")
            c.set(rx + 1, y, "K")
        if y % 5 == 2:
            prev = x0 - direction if y in (12, 27) else x0       # the rung at a step spans both positions
            for x in range(min(x0, prev), max(x0, prev) + 4):
                c.set(x, y, "O")
            c.set(x0, y, "W")
            c.set(x0 + 3, y, "W")
            for x in range(x0 + 1, x0 + 3):
                c.set(x, y + 1, "K")
    return c


def hook():
    """8 x 12: the short first-moment rope and a steel J hook (stone, lit cream on the left)"""
    c = Cv(8, 12)
    for y in range(0, 5):
        c.set(2, y, "K")
        c.set(3, y, "W" if y % 2 == 0 else "S")
        c.set(4, y, "S")
        c.set(5, y, "K")
    pix = {(3, 5): "S", (4, 5): "S",                          # thimble
           (3, 6): "W", (4, 6): "S", (3, 7): "W", (4, 7): "S", (3, 8): "W", (4, 8): "S",
           (3, 9): "S", (4, 9): "S", (5, 9): "S", (6, 9): "S", (6, 8): "S", (6, 7): "W"}
    for (x, y), col in pix.items():
        c.set(x, y, col)
    for (x, y) in list(pix):
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            c.only(x + dx, y + dy, "K")
    return c


# ---- the climbing man, 16 x 12 (same palette and drawing as art/hostage.txt)
MAN_CLIMB0 = """....K.KKK.......
...KWKKWWK......
...KWKWWWKK.....
...KBKBBBKWK....
....KBBBBBKK....
.....KBBBK......
.....KBBBK......
.....KSSSK......
...KKSSKSK......
..KSSSK.KSK.....
..KKKK..KSK.....
........KKK.....
"""
def man(txt, mirror=False):
    c = Cv(16, 12)
    for y, row in enumerate(txt.strip("\n").splitlines()):
        for x, ch in enumerate(row):
            c.set(14 - x if mirror else x, y, ch)
    return c


def build(outdir):
    write_txt(os.path.join(outdir, "deck_ship.txt"),
              "Cargo deck ship 124 x 30 (slice C, the ship-deck winch rescue, mission 5): frame 0 idle, frame 1 bow wake. One facing, bow left,\n"
              "bridge and funnel aft (right). The BOB's last row sits on BOAT_BOTTOM_Y 248 (row r = y 219 + r); the flat deck edge is row 8 (y 227),\n"
              "bare of gear from x 24 to x 84 (the 24 x 20 hover box); hull bottom outline row 27 (waterline y 246), foam rows 28-29.",
              [deck_ship(0), deck_ship(1)])
    write_txt(os.path.join(outdir, "winch_rope.txt"),
              "Winch rope 8 x 40, 3 frames: 0 the plain rope hanging (knots, weighted tip), 1 the rope ladder swaying left, 2 swaying right.\n"
              "The top row is the skid hook-up (rope centre x 3.5 in frame 0, 2.5 / 3.5 in the ladder frames); the code may clip the bottom rows\n"
              "to drop the rope 24 px over 12 frames. The 8 x 12 hook for the first moment is winch_hook.txt.",
              [rope_plain(), ladder_sway(-1), ladder_sway(+1)])
    write_txt(os.path.join(outdir, "winch_hook.txt"),
              "Winch hook 8 x 12, 1 frame: the short rope and its hook for the first moment of a winch (before the rope drops).",
              [hook()])
    write_txt(os.path.join(outdir, "man_climb.txt"),
              "A person climbing the winch ladder, 16 x 12, 2 frames (MAN_CLIMB0 / MAN_CLIMB1), arms up, same drawing and pens as hostage.txt.\n"
              "Append to hostage.txt as frames after the existing 12 (or keep as its own BOB): the hands are at x 2-3 / 11-12.",
              [man(MAN_CLIMB0), man(MAN_CLIMB0, True)])


if __name__ == "__main__":
    build(sys.argv[1] if len(sys.argv) > 1 else ".")
