"""Slice E art generator: the strait destroyer set piece (mission 6).

Hand-drawn text-art, no Retro Diffusion. Writes into the given art/world2 dir:
  aa_gun.txt        12 x 12, 3 frames (idle, tell, puff): the four AA mounts on the hull
  turret_hit.txt    32 x 19, 2 frames (smoking, dead), left facing like destroyer_turret.txt
  hull_list.txt     154 x 44, 2 frames: 0 = the hull as played (destroyer_hull.txt frame 0 with the
                    painted AA mount at x 43-53 cleared: the BOBs carry the guns), 1 = listing
  barrage_tell.txt  24 x 12, 3 frames (no outline, water effect)
`python e_art.py <art/world2 dir>`

The AA gun, smoke puffs and the two turret states are drawn here as rows of letters and a few helpers
(`outline`, `puff`); the hull's list is the base frame sheared by columns (a pitched hull is drawn by
columns, as the freighter was), the waterline rows 41-43 stay where they are.
"""
import os
import sys

from cdlib import Cv, read_txt, write_txt

# ---- geometry shared with e_check.py / e_preview.py -------------------------------------------------
HULL_W, HULL_H = 154, 44
CLEAR_BOX = (41, 21, 53, 29)           # x0, y0, x1, y1 of the painted forward AA gun (cleared in frame 0)
# AA mounts: top-left of the 12 x 12 BOB on the hull frame (left-facing). All four stand on a K outline row.
AA_SLOTS = {"A": (27, 12), "B": (42, 17), "C": (75, 6), "D": (99, 4)}
TURRET_MOUNTS = {"aft": (122, 11), "fwd": (-7, 11)}   # unchanged from world2_art.toml
LIST_PIVOT_X = 110
LIST_BOW = 4                              # px the bow ends sink at the hull's left end; the stern rises as much
BODY_ROWS = 41                            # rows 0-40 move with the list; 41-43 (hull bottom edge, ripple row, foam) stay
DECK_ROW = 29                             # the K outline row under every block (deck line is row 30)


def list_dy(x):
    """hull list: px to add to y at hull column x (+ = down). Bow (x 0) sinks, stern (x 153) rises."""
    return round(LIST_BOW * (LIST_PIVOT_X - x) / LIST_PIVOT_X)


def cv_from(rows):
    h, w = len(rows), len(rows[0])
    assert all(len(r) == w for r in rows), "ragged rows"
    c = Cv(w, h)
    for y, r in enumerate(rows):
        for x, ch in enumerate(r):
            c.set(x, y, ch)
    return c


def outline(c, skip="Kwyr."):
    """1-px K outline (4-neighbour) around every pixel that is not K / flash / empty; on empty pixels only"""
    todo = []
    for y in range(c.h):
        for x in range(c.w):
            if c.get(x, y) != ".":
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                if c.get(x + dx, y + dy) not in skip:
                    todo.append((x, y))
                    break
    for x, y in todo:
        c.set(x, y, "K")


def finish(c):
    """flash pens (lower case: drawn without an outline) become their pen letters"""
    for y in range(c.h):
        for x in range(c.w):
            ch = c.get(x, y)
            if ch in "wyr":
                c.set(x, y, ch.upper())


def puff(c, cx, cy, rx, ry, light=True):
    """an outlined smoke puff like smoke.txt: K rim, S body, O shade lower right, one W glint top left"""
    for y in range(cy - ry, cy + ry + 1):
        for x in range(cx - rx, cx + rx + 1):
            d = ((x - cx) / (rx + 0.3)) ** 2 + ((y - cy) / (ry + 0.3)) ** 2
            if d > 1.0:
                continue
            inner = ((x - cx) / (rx - 0.7 + 0.3)) ** 2 + ((y - cy) / max(ry - 0.7, 0.4) ** 1) ** 2 if rx > 1 else 2
            ring = d > (0.62 if rx >= 3 else 0.5)
            if ring:
                c.set(x, y, "K")
            else:
                sh = ((x - cx) / rx + (y - cy) / ry)
                c.set(x, y, "O" if sh > 0.35 else "S")
    if light and rx >= 3:
        c.set(cx - rx + 2, cy - ry + 1, "W")


def cloud(c, ells, dark=False, seed=0):
    """a connected smoke mass (union of ellipses (cx, cy, rx, ry)) with one K outline, lit top left.
    light: S body, W rim top left, O rim lower right.  dark: O body, S rim top left, K rim lower right (burning oil)."""
    mask = set()
    for (cx, cy, rx, ry) in ells:
        for y in range(int(cy - ry - 1), int(cy + ry + 2)):
            for x in range(int(cx - rx - 1), int(cx + rx + 2)):
                if ((x - cx) / (rx + 0.45)) ** 2 + ((y - cy) / (ry + 0.45)) ** 2 <= 1.0:
                    mask.add((x, y))
    body, hi, sh = ("O", "S", "K") if dark else ("S", "W", "O")
    for (x, y) in mask:
        ul = (x - 1, y) not in mask or (x, y - 1) not in mask
        lr = (x + 1, y) not in mask or (x, y + 1) not in mask
        if ul and not lr:
            pen = hi
        elif lr and not ul:
            pen = sh
        else:
            pen = body
        c.set(x, y, pen)
    for (cx, cy, rx, ry) in ells:                      # one glint per big puff, inside the lit rim
        if rx >= 3 and (cx - rx + 2, cy - ry + 1) in mask:
            c.set(cx - rx + 2, cy - ry + 1, hi if dark else "W")
    ring = set()
    for (x, y) in mask:
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            if (x + dx, y + dy) not in mask:
                ring.add((x + dx, y + dy))
    for (x, y) in ring:
        c.only(x, y, "K")


# ---- AA gun -----------------------------------------------------------------------------------------
# letters: S stone, O olive, W cream, R red, Y yellow; lower case w y r = flash pens, no outline.
AA_IDLE = [
    "............",
    "............",
    ".SS.........",
    "..SS........",
    "...SS.......",
    "....SS......",
    "...SSSSSW...",
    "...SOOOOOO..",
    "...SOOOOOO..",
    "....OOOOO...",
    "..SSSSSSSSS.",
    "............",
]
AA_TELL = [
    "............",
    "...rr.......",
    "...rr.......",
    "...SS.......",
    "....SS......",
    "....SS......",
    "...SSSSSW...",
    "...SOOOOOO..",
    "...SORROOO..",
    "....OOOOO...",
    "..SSSSSSSSS.",
    "............",
]
AA_PUFF = [
    "..w.w.......",
    ".wyyyw......",
    "..wyw.......",
    "...SS.......",
    "....SS......",
    "....SS......",
    "...SSSSSW...",
    "...SOOOOOO..",
    "...SOOOOOO..",
    "....OOOOO...",
    "..SSSSSSSSS.",
    "............",
]


def aa_frames():
    out = []
    for rows in (AA_IDLE, AA_TELL, AA_PUFF):
        c = cv_from(rows)
        outline(c)
        finish(c)
        out.append(c)
    return out


# ---- turret hit / dead ------------------------------------------------------------------------------
def load_turret_idle(art):
    colors, frames = read_txt(os.path.join(art, "destroyer_turret.txt"))
    return cv_from(frames[0])


def sag_barrel(c, col_end, drop):
    """bend the barrel down: columns left of col_end move down by up to `drop` rows (rows 9-14 are the barrel
    where the housing does not own the column: housing cols 16+, shaft joins at cols 14-15)"""
    src = cv_from(c.rows())
    for x in range(0, col_end):
        dy = round(drop * (col_end - x) / col_end)
        for y in range(9, 19):
            c.set(x, y, ".")
        for y in range(9, 19):
            ch = src.get(x, y)
            if ch != ".":
                c.set(x, y + dy, ch)


def turret_hit(art):
    t = load_turret_idle(art)
    # ---- smoking: the barrel bent down, the housing scorched with a flickering crater, a smoke column
    s = cv_from(t.rows())
    sag_barrel(s, 11, 4)
    crater = {
        (22, 11): "K", (23, 11): "K", (24, 11): "K", (25, 11): "K",
        (21, 12): "K", (22, 12): "R", (23, 12): "Y", (24, 12): "R", (25, 12): "K",
        (21, 13): "K", (22, 13): "Y", (23, 13): "W", (24, 13): "Y", (25, 13): "R", (26, 13): "K",
        (22, 14): "K", (23, 14): "R", (24, 14): "R", (25, 14): "K",
        (23, 15): "K", (24, 15): "K",
    }
    for (x, y), ch in crater.items():
        s.set(x, y, ch)
    for x, y in ((18, 11), (19, 11), (28, 12), (29, 13), (28, 14), (27, 15), (20, 16), (27, 16), (18, 15)):
        s.set(x, y, "K")
    s.set(19, 14, "O")
    cloud(s, [(23, 8, 2, 1), (24, 5, 3, 2), (26, 2, 4, 2)])
    # ---- dead: charred housing (K with a few O flecks, S kept on the top-left rim), the barrel hanging dark with a lit top edge
    d = cv_from(t.rows())
    sag_barrel(d, 16, 4)
    for y in range(10, 19):
        for x in range(16, 32):
            ch = d.get(x, y)
            if ch == "O":
                h = (x * 7 + y * 13 + x * y) % 11
                d.set(x, y, "O" if h in (0, 4, 8) else "K")
            elif ch == "S":
                if y == 11 and x < 23:
                    d.set(x, y, "S")
                elif x <= 17:
                    d.set(x, y, "S")
                else:
                    d.set(x, y, "S" if (x * 5 + y * 3) % 7 == 0 else "K")
    src = cv_from(d.rows())
    for y in range(9, 19):
        for x in range(0, 16):
            if src.get(x, y) == "S":
                up = src.get(x, y - 1)
                d.set(x, y, "S" if up in ".K" and (x + y) % 5 != 0 else "O")
    for x, y in ((21, 11), (22, 11), (22, 12), (23, 12), (23, 13), (24, 13)):
        d.set(x, y, "K")
    d.set(24, 11, "O")
    d.set(23, 14, "R")
    cloud(d, [(24, 7, 1, 1), (25, 4, 2, 2)])
    return [s, d]


# ---- hull: clean frame 0 and the list frame ---------------------------------------------------------
def load_hull0(art):
    colors, frames = read_txt(os.path.join(art, "destroyer_hull.txt"))
    return cv_from(frames[0])


def clean_hull(h0):
    c = cv_from(h0.rows())
    x0, y0, x1, y1 = CLEAR_BOX
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            c.set(x, y, "K" if y == y1 else ".")
    return c


def runs_above_deck(base):
    """column runs (x0, x1) that carry structure above the deck (rows 0-28): they move as rigid blocks in the list"""
    cols = [x for x in range(HULL_W) if any(base.get(x, y) != "." for y in range(0, DECK_ROW))]
    runs, s, p = [], cols[0], cols[0]
    for x in cols[1:]:
        if x != p + 1:
            runs.append((s, p))
            s = x
        p = x
    runs.append((s, p))
    return runs


def list_offsets(base):
    """y offset of each thing that sits on the hull when it lists (+ = down): the AA mounts and the turrets
    (blocks move rigidly: the offset of the run they stand on; B stands on the sheared deck)"""
    runs = runs_above_deck(base)

    def run_dy(x):
        for (a, b) in runs:
            if a <= x <= b:
                return list_dy((a + b) // 2)
        return list_dy(x)

    return {"A": run_dy(AA_SLOTS["A"][0] + 6), "B": list_dy(AA_SLOTS["B"][0] + 6), "C": run_dy(AA_SLOTS["C"][0] + 6),
            "D": run_dy(AA_SLOTS["D"][0] + 6), "fwd": run_dy(24), "aft": run_dy(130)}


def list_hull(base):
    out = Cv(HULL_W, HULL_H)
    runs = runs_above_deck(base)
    # the waterline and foam rows stay
    for y in range(BODY_ROWS, HULL_H):
        for x in range(HULL_W):
            out.set(x, y, base.get(x, y))
    # rigid blocks above the deck (rows 0 .. DECK_ROW): shifted whole by the list at their centre
    run_dy_of = {}
    for (a, b) in runs:
        dy = list_dy((a + b) // 2)
        for x in range(a, b + 1):
            run_dy_of[x] = dy
            for y in range(0, DECK_ROW + 1):
                ch = base.get(x, y)
                if ch != "." and 0 <= y + dy < HULL_H:
                    out.set(x, y + dy, ch)
    # the hull side (rows DECK_ROW + 1 .. BODY_ROWS - 1) and the deck outline between blocks: sheared by columns
    for x in range(HULL_W):
        dy = list_dy(x)
        top = DECK_ROW + 1 if x in run_dy_of else DECK_ROW
        for y in range(top, BODY_ROWS):
            ch = base.get(x, y)
            ny = y + dy
            if ch != "." and ny < BODY_ROWS:
                out.set(x, ny, ch)
        # a gap under a block (the side moved down more than the block): repeat the block's last row
        if x in run_dy_of:
            for y in range(DECK_ROW + 1 + run_dy_of[x], top + dy):
                out.set(x, y, base.get(x, DECK_ROW))
        # at the stern the side moved up: fill between its moved bottom and the static waterline rows
        if dy < 0:
            for y in range(BODY_ROWS + dy, BODY_ROWS):
                out.set(x, y, base.get(x, BODY_ROWS - 1))
    # black smoke from both funnels, streaming aft (right): one connected mass each
    for ells in ([(97, 13, 2, 1), (100, 10, 3, 2)], [(106, 7, 3, 2)], [(113, 5, 4, 2)], [(121, 3, 3, 1)], [(127, 3, 2, 1)],
                 [(113, 13, 2, 1), (116, 10, 3, 2)], [(122, 9, 3, 2)], [(130, 7, 3, 2)], [(137, 6, 3, 1)], [(143, 6, 2, 1)]):
        cloud(out, ells, dark=True)
    return out


# ---- barrage tell -----------------------------------------------------------------------------------
def ring_pixels(cx, cy, rx, ry):
    """the 1-px ring of an ellipse: pixels inside the outer ellipse (r + 0.5) and outside the inner one (r - 0.5)"""
    out = []
    for y in range(12):
        for x in range(24):
            d_out = ((x - cx) / (rx + 0.5)) ** 2 + ((y - cy) / (ry + 0.5)) ** 2
            d_in = ((x - cx) / max(rx - 0.5, 0.1)) ** 2 + ((y - cy) / max(ry - 0.5, 0.1)) ** 2
            if d_out <= 1.0 and d_in > 1.0:
                out.append((x, y))
    return out


def draw_ring(c, cx, cy, rx, ry, gap=0):
    """foam ring lit top left: cream on the upper-left arc, stone across, blue on the lower-right; `gap` drops
    about one pixel in `gap` (hash placed) so it reads as breaking foam"""
    for (x, y) in ring_pixels(cx, cy, rx, ry):
        if gap and (x * 5 + y * 3) % gap == 0:
            continue
        v = ((x - cx) / rx + (y - cy) / ry)
        if v < -0.45:
            pen = "W"
        elif v < 0.35:
            pen = "S"
        else:
            pen = "B"
        c.set(x, y, pen)


def fill_ellipse(c, cx, cy, rx, ry, pen, hole=0):
    """the water boiling inside the ring: a blue ellipse with hash-placed holes (never a flat area)"""
    for y in range(12):
        for x in range(24):
            if ((x - cx) / (rx + 0.5)) ** 2 + ((y - cy) / (ry + 0.5)) ** 2 <= 1.0:
                if hole and (x * 3 + y * 7) % hole == 0:
                    continue
                c.set(x, y, pen)


def barrage_frames():
    """24 x 12 on the water, centre (11.5, 5.5): the ranging marker. The cream fringe is the lit upper-left arc;
    the boil, the red ticks and a yellow flash grow as the 30 frames run out."""
    cx, cy = 11.5, 5.5
    frames = []
    a = Cv(24, 12)
    fill_ellipse(a, cx, cy, 3.0, 1.0, "B", hole=4)
    draw_ring(a, cx, cy, 5.5, 2.3, gap=4)
    a.set(10, 4, "W")
    frames.append(a)
    b = Cv(24, 12)
    fill_ellipse(b, cx, cy, 6.0, 2.4, "B", hole=4)
    draw_ring(b, cx, cy, 8.8, 3.9, gap=5)
    draw_ring(b, cx, cy, 8.0, 3.2, gap=3) if False else None
    for (x, y) in ring_pixels(cx, cy, 7.9, 3.1):          # a second cream row on the lit arc only
        if ((x - cx) / 7.9 + (y - cy) / 3.1) < -0.6 and (x * 5 + y * 3) % 4:
            b.set(x, y, "W")
    for x, y in ((0, 5), (0, 6), (1, 5), (23, 5), (23, 6), (22, 6)):
        b.set(x, y, "R")
    for x, y in ((3, 2), (20, 9), (12, 1)):
        b.set(x, y, "W")
    frames.append(b)
    c = Cv(24, 12)
    fill_ellipse(c, cx, cy, 8.0, 3.4, "B", hole=5)
    draw_ring(c, cx, cy, 11.0, 5.0, gap=0)
    for (x, y) in ring_pixels(cx, cy, 10.0, 4.1):          # lit arc doubled
        if ((x - cx) / 10.0 + (y - cy) / 4.1) < -0.5:
            c.set(x, y, "W")
    for x, y in ((0, 5), (0, 6), (1, 5), (1, 6), (22, 5), (22, 6), (23, 5), (23, 6), (11, 0), (12, 0), (11, 1), (12, 1),
                 (11, 10), (12, 10), (11, 11), (12, 11), (1, 4), (22, 7)):
        c.set(x, y, "R")
    for x in range(8, 16):
        c.set(x, 5, "Y")
    for x in range(9, 15):
        c.set(x, 4, "Y")
        c.set(x, 6, "Y")
    for x, y in ((11, 5), (12, 5), (11, 4), (12, 4)):
        c.set(x, y, "W")
    for x, y in ((5, 1), (18, 10), (4, 9), (19, 2)):
        c.set(x, y, "W")
    frames.append(c)
    return frames


# ---- main -------------------------------------------------------------------------------------------
def main(art):
    aa = aa_frames()
    write_txt(os.path.join(art, "aa_gun.txt"),
              "Destroyer AA gun, 12 x 12, 3 frames: idle, tell (barrel up, red muzzle and lamp), puff (muzzle flash).\n"
              "Four mounts on the hull (see E-ENTRIES.md); same hull pens as the turret; the last row is the K base line.", aa)
    th = turret_hit(art)
    write_txt(os.path.join(art, "turret_hit.txt"),
              "Destroyer turret hit states, 32 x 19, left facing like destroyer_turret.txt: frame 0 smoking (barrel bent,\n"
              "flames in the crater), frame 1 dead (charred, barrel hanging, a wisp). Same mounts as the idle turret.", th)
    h0 = load_hull0(art)
    base = clean_hull(h0)
    lst = list_hull(base)
    write_txt(os.path.join(art, "hull_list.txt"),
              "Destroyer hull, 154 x 44, left facing, 2 frames. Frame 0 = destroyer_hull.txt frame 0 with the painted AA mount\n"
              "at x 41-53, y 21-29 cleared (the AA BOBs carry the guns): use it instead of the original frame 0.\n"
              "Frame 1 = listing, both turrets down: the bow sinks 4 px, the stern rises 2 px (pitched around x 110),\n"
              "the waterline rows 41-43 stay, black smoke streams from both funnels. Generated by e_art.py.", [base, lst])
    bt = barrage_frames()
    write_txt(os.path.join(art, "barrage_tell.txt"),
              "Barrage tell, 24 x 12, 3 frames, drawn on the water (no outline, a cream fringe lit top left):\n"
              "0 a small ring and a pit, 1 the ring opens with red ticks, 2 the full ring, red ticks on four sides, yellow flash.", bt)
    print("wrote aa_gun.txt, turret_hit.txt, hull_list.txt, barrage_tell.txt in", art)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "../../world2")
