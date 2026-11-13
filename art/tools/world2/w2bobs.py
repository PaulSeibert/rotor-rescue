"""PF1 BOBs for the Coral Islands pilot, drawn with a tiny canvas DSL (letters K O S W R Y B = the fixed PF1 colours
1-7). Proportions were checked against the Retro Diffusion roughs (raw/*), the pixels are hand-built here, then the
outline rule (1 px 0x111, 4-connected, outside the shape) is applied. Light from the top left."""
import math
import numpy as np
from w2lib import *

LET = {"K": 0x111, "O": 0x564, "S": 0x998, "W": 0xFEC, "R": 0xD42, "Y": 0xFC3, "B": 0x36B}


class C:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.a = [["."] * w for _ in range(h)]

    def px(self, x, y, c):
        x, y = int(x), int(y)
        if 0 <= x < self.w and 0 <= y < self.h:
            self.a[y][x] = c

    def get(self, x, y):
        return self.a[y][x] if 0 <= x < self.w and 0 <= y < self.h else "."

    def rect(self, x0, y0, x1, y1, c):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.px(x, y, c)

    def poly(self, pts, c):
        ys = [p[1] for p in pts]
        n = len(pts)
        for y in range(int(min(ys)), int(max(ys)) + 1):
            xs = []
            for i in range(n):
                (x0, y0), (x1, y1) = pts[i], pts[(i + 1) % n]
                if (y0 <= y + 0.5 < y1) or (y1 <= y + 0.5 < y0):
                    xs.append(x0 + (y + 0.5 - y0) * (x1 - x0) / (y1 - y0))
            xs.sort()
            for i in range(0, len(xs) - 1, 2):
                for x in range(int(math.floor(xs[i] + 0.5)), int(math.floor(xs[i + 1] + 0.5))):
                    self.px(x, y, c)

    def line(self, x0, y0, x1, y1, c, th=1):
        n = int(max(abs(x1 - x0), abs(y1 - y0))) + 1
        for i in range(n + 1):
            t = i / max(1, n)
            x = x0 + (x1 - x0) * t
            y = y0 + (y1 - y0) * t
            for k in range(th):
                self.px(round(x), round(y) + k, c)

    def ell(self, cx, cy, rx, ry, c):
        for y in range(int(cy - ry - 1), int(cy + ry + 2)):
            for x in range(int(cx - rx - 1), int(cx + rx + 2)):
                if ((x + .5 - cx) / rx) ** 2 + ((y + .5 - cy) / ry) ** 2 <= 1.0:
                    self.px(x, y, c)

    def outline(self):
        o = [row[:] for row in self.a]
        for y in range(self.h):
            for x in range(self.w):
                if self.a[y][x] != ".":
                    continue
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    if self.get(x + dx, y + dy) not in (".", "K") or (self.get(x + dx, y + dy) == "K" and False):
                        o[y][x] = "K"
                        break
        self.a = o
        return self

    def img(self):
        im = Image.new("RGBA", (self.w, self.h), (0, 0, 0, 0))
        p = im.load()
        for y in range(self.h):
            for x in range(self.w):
                ch = self.a[y][x]
                if ch != ".":
                    p[x, y] = rgb12(LET[ch]) + (255,)
        return im

    def copy(self):
        c = C(self.w, self.h)
        c.a = [r[:] for r in self.a]
        return c

    def stamp(self, other, x0, y0):
        for y in range(other.h):
            for x in range(other.w):
                if other.a[y][x] != ".":
                    self.px(x0 + x, y0 + y, other.a[y][x])

    def crop(self):
        ys = [y for y in range(self.h) if any(ch != "." for ch in self.a[y])]
        xs = [x for x in range(self.w) if any(self.a[y][x] != "." for y in range(self.h))]
        c = C(max(xs) - min(xs) + 1, max(ys) - min(ys) + 1)
        for y in range(c.h):
            for x in range(c.w):
                c.a[y][x] = self.a[y + min(ys)][x + min(xs)]
        return c

    def mirror(self):
        c = C(self.w, self.h)
        c.a = [r[::-1] for r in self.a]
        return c

    def dump(self):
        return "\n".join("".join(r) for r in self.a)


def barrel(c, x0, y0, ang_deg, length, face=-1):
    """a 2-px thick gun barrel from the pivot (x0, y0), angle above horizontal, pointing left (face -1)"""
    a = math.radians(ang_deg)
    x1 = x0 + face * length * math.cos(a)
    y1 = y0 - length * math.sin(a)
    c.line(x0, y0, x1, y1, "S", th=2)
    return x1, y1


# ----------------------------------------------------------------------------------------- patrol boat
def patrol(wake=0):
    c = C(46, 18)
    ox = 3
    c.poly([(ox + 0, 8), (ox + 40, 9), (ox + 37, 14), (ox + 8, 14)], "O")
    c.rect(ox + 1, 8, ox + 39, 8, "S")                       # lit deck edge
    c.rect(ox + 4, 9, ox + 36, 9, "S")
    for x in range(ox + 9, ox + 36):                        # red stripe
        c.px(x, 12, "R")
    for x in range(ox + 9, ox + 37, 2):                     # shadow row dither
        c.px(x, 13, "K")
    c.rect(ox + 14, 4, ox + 27, 7, "O")                     # cabin
    c.rect(ox + 14, 4, ox + 27, 4, "S")
    c.rect(ox + 14, 5, ox + 14, 7, "S")
    for x in range(ox + 17, ox + 26, 2):
        c.px(x, 6, "Y")
    c.line(ox + 22, 4, ox + 22, 0, "K")                    # mast
    c.px(ox + 23, 1, "S")
    c.rect(ox + 5, 6, ox + 9, 7, "O")                       # bow gun
    c.rect(ox + 5, 6, ox + 9, 6, "S")
    c.line(ox + 5, 6, ox + 1, 5, "S")
    c.rect(ox + 30, 7, ox + 35, 7, "S")                     # stern rail
    c.outline()
    # bow wave and wake (cream), 2 frames
    pts = [(ox + 0, 14), (ox + 1, 15), (ox + 2, 14), (ox + 4, 15), (ox + 6, 15)]
    for x, y in pts:
        c.px(x, y, "W")
    wk = [(ox + 38, 15), (ox + 40, 15), (ox + 42, 16)] if wake == 0 else [(ox + 39, 15), (ox + 41, 16), (ox + 43, 15)]
    for x, y in wk:
        c.px(x, y, "W")
    for x in range(ox + 10, ox + 36, 5):
        c.px(x + wake, 15, "S")
    return c


# ----------------------------------------------------------------------------------------- gunboat
def gunboat(state="idle", wake=0):
    """state: idle | tell1 | tell2 | fire. Barrel pivot at (20, 11). 64 x 30."""
    c = C(64, 38)
    ox = 13
    base = 30
    c.poly([(ox + 0, base - 8), (ox + 50, base - 7), (ox + 47, base), (ox + 7, base)], "O")      # hull
    c.rect(ox + 1, base - 8, ox + 49, base - 8, "S")
    c.rect(ox + 3, base - 7, ox + 46, base - 7, "S")
    for x in range(ox + 8, ox + 47):
        c.px(x, base - 4, "R")
    for x in range(ox + 8, ox + 48, 2):
        c.px(x, base - 1, "K")
    c.rect(ox + 2, base - 11, ox + 15, base - 9, "O")        # forecastle block
    c.rect(ox + 2, base - 11, ox + 15, base - 11, "S")
    c.rect(ox + 2, base - 10, ox + 2, base - 9, "S")
    # bridge
    c.rect(ox + 20, base - 17, ox + 34, base - 9, "O")
    c.rect(ox + 20, base - 17, ox + 34, base - 17, "S")
    c.rect(ox + 20, base - 16, ox + 21, base - 9, "S")
    c.rect(ox + 22, base - 20, ox + 31, base - 18, "O")      # upper deck
    c.rect(ox + 22, base - 20, ox + 31, base - 20, "S")
    for x in range(ox + 24, ox + 34, 2):
        c.px(x, base - 14, "Y")
    for x in range(ox + 23, ox + 31, 2):
        c.px(x, base - 19, "Y")
    c.line(ox + 27, base - 21, ox + 27, base - 26, "K")      # mast
    c.line(ox + 25, base - 24, ox + 29, base - 24, "S")
    # funnel
    c.rect(ox + 37, base - 14, ox + 41, base - 9, "O")
    c.rect(ox + 37, base - 14, ox + 41, base - 14, "K")
    c.rect(ox + 37, base - 13, ox + 37, base - 9, "S")
    c.rect(ox + 38, base - 12, ox + 40, base - 12, "R")
    # turret (forecastle, pivot at the block's top-left)
    px_, py_ = ox + 9, base - 12
    lift = {"idle": 0, "tell1": 0, "tell2": 1, "fire": 0}[state]
    c.rect(ox + 5, py_ - 4 - lift, ox + 13, py_ - lift, "O")
    c.rect(ox + 5, py_ - 4 - lift, ox + 13, py_ - 4 - lift, "S")
    c.rect(ox + 5, py_ - 3 - lift, ox + 5, py_ - lift, "S")
    ang = {"idle": 8, "tell1": 28, "tell2": 42, "fire": 42}[state]
    tipx, tipy = barrel(c, ox + 6, py_ - 2 - lift, ang, 14)
    c.outline()
    if state in ("tell2", "fire"):                           # muzzle flash (cream core, yellow, red spark)
        fx, fy = round(tipx), round(tipy)
        for dx, dy, ch in [(0, 0, "W"), (-1, 0, "W"), (0, -1, "W"), (-2, 0, "Y"), (-1, -1, "Y"), (-1, 1, "Y"), (0, -2, "Y"),
                           (-3, 0, "Y"), (-2, -2, "R"), (-2, 2, "R"), (-4, 0, "R"), (0, 1, "Y")]:
            c.px(fx + dx, fy + dy, ch)
        if state == "fire":
            for dx, dy in [(-4, -3), (-5, -1), (-3, -4), (-6, 0)]:
                c.px(fx + dx, fy + dy, "S")
    if state in ("tell1", "tell2"):                          # the red warning lamp on the mast blinks
        c.px(ox + 27, base - 26, "R")
        if state == "tell2":
            c.px(ox + 26, base - 26, "R")
            c.px(ox + 28, base - 26, "R")
    # bow wave and wake
    for x, y in [(ox + 0, base + 1), (ox + 2, base + 2), (ox + 4, base + 1), (ox + 6, base + 2), (ox + 8, base + 2)]:
        c.px(x, y, "W")
    for x, y in ([(ox + 49, base + 2), (ox + 52, base + 2), (ox + 54, base + 3)] if wake == 0 else [(ox + 50, base + 2), (ox + 53, base + 3), (ox + 56, base + 2)]):
        c.px(x, y, "W")
    for x in range(ox + 12, ox + 46, 6):
        c.px(x + wake * 2, base + 2, "S")
    return c.crop()


# ----------------------------------------------------------------------------------------- radar site
def radar(frame=0, lamp=True):
    """base pad + hut + a dish on a mast; frame 0-3 turns the dish (wide, mid, thin, mid-flipped). 34 x 40"""
    c = C(34, 40)
    # concrete base (a low block with a lit left face)
    c.poly([(2, 36), (6, 31), (28, 31), (32, 36), (32, 38), (2, 38)], "S")
    c.rect(2, 36, 32, 36, "W")
    c.rect(2, 37, 32, 38, "S")
    for x in range(4, 31, 2):
        c.px(x, 38, "O")
    # hut
    c.rect(18, 26, 28, 31, "O")
    c.rect(18, 26, 28, 26, "S")
    c.rect(18, 27, 18, 31, "S")
    c.rect(24, 28, 26, 31, "K")
    c.px(20, 29, "Y")
    c.px(21, 29, "Y")
    # mast and gantry
    c.rect(11, 14, 12, 31, "S")
    c.rect(11, 14, 11, 31, "W")
    c.rect(8, 30, 15, 31, "O")
    c.line(9, 31, 11, 24, "O")
    c.line(14, 31, 12, 24, "O")
    # dish (a tilted ellipse seen at a turning angle), centre (12, 10)
    wd = [10, 7, 3, 7][frame]
    c.ell(12, 9, wd, 6, "S")
    # lit rim top-left
    for x in range(int(12 - wd), int(12 + wd)):
        for y in range(2, 17):
            if c.get(x, y) == "S" and (c.get(x - 1, y) == "." or c.get(x, y - 1) == "."):
                c.px(x, y, "W")
    # inner dish shade (right/bottom)
    for x in range(int(12 - wd), int(12 + wd)):
        for y in range(2, 17):
            if c.get(x, y) == "S" and (c.get(x + 1, y) == "." or c.get(x, y + 1) == "."):
                c.px(x, y, "O")
    if frame == 3:
        for y in range(2, 17):
            for x in range(0, 34):
                pass
    c.line(12, 9, 19, 5, "O")                                   # feed arm
    c.rect(19, 4, 20, 5, "S")
    c.outline()
    if lamp:
        c.px(12, 1, "R")
        c.px(11, 1, "R") if frame % 2 == 0 else None
    return c


# ----------------------------------------------------------------------------------------- island barracks
def barracks(state="closed"):
    """a beach barracks on stilts, white walls, olive roof, sandbags; 52 x 32. closed = blue lamp, open = red + dark door"""
    c = C(52, 32)
    # stilts / porch posts
    for x in (6, 15, 24, 33, 42):
        c.rect(x, 24, x + 1, 29, "K")
    c.rect(4, 23, 47, 24, "O")                               # floor beam
    c.rect(4, 23, 47, 23, "S")
    # walls
    c.rect(6, 12, 45, 22, "W")
    c.rect(6, 12, 6, 22, "W")
    for x in range(8, 45, 6):
        c.rect(x, 14, x + 3, 17, "K")                       # louvred windows
        c.rect(x, 14, x + 3, 14, "S")
        for y in (15, 17):
            for xx in range(x, x + 4, 2):
                c.px(xx, y, "O")
    c.rect(6, 22, 45, 22, "S")
    c.rect(43, 12, 45, 22, "S")                              # right wall in shade
    # door
    dk = "K" if state == "open" else "O"
    c.rect(24, 16, 29, 22, dk)
    if state == "open":
        c.rect(25, 18, 28, 22, "R")
        c.px(26, 19, "Y")
        c.px(27, 20, "Y")
    else:
        c.rect(24, 16, 29, 16, "S")
        c.px(28, 20, "Y")
    # roof: thatch (olive) with lit left slope
    c.poly([(2, 12), (10, 3), (41, 3), (49, 12)], "O")
    c.rect(10, 3, 41, 3, "S")
    for y in range(4, 12):
        xl = 10 - (y - 3)
        c.px(xl, y, "S")
        c.px(xl + 1, y, "S")
    for x in range(12, 46, 4):                                # thatch rows
        c.px(x, 6, "S")
        c.px(x + 2, 8, "K")
        c.px(x + 1, 10, "S")
    c.rect(3, 11, 48, 12, "O")
    c.rect(3, 11, 20, 11, "S")
    c.rect(26, 11, 48, 12, "K") if False else None
    # marker lamp on the ridge pole
    c.line(25, 3, 25, 0, "K")
    lamp = "R" if state == "open" else "B"
    c.rect(24, 0, 26, 1, lamp)
    # sandbags in front
    for i, x in enumerate(range(3, 47, 5)):
        c.rect(x, 26, x + 4, 28, "S")
        c.rect(x, 26, x + 1, 26, "W")
        c.px(x + 4, 28, "O")
        c.px(x + 3, 28, "O")
    c.rect(3, 29, 50, 29, "O")
    c.outline()
    return c


# ----------------------------------------------------------------------------------------- landing pad
def pad_parts(g="Y"):
    """pad kit in 8 px units; the slab is 16 rows deep, drawn in the walking track's perspective (far edge lit,
    front face in shade). cap_l / cap_r carry a lit pylon with a beacon; chev_r / chev_l are chevrons pointing in;
    centre is the lit H (16 px); lamp_on / lamp_off are 5x5 status lamps (yellow = over the pad, red = not).
    g = guide colour: Y lit, O dark (the blink)."""
    P = {}

    def slab(w, left=False, right=False):
        c = C(w, 16)
        c.poly([(0, 4), (3 if left else 0, 2), (w - (3 if right else 0), 2), (w, 4), (w, 15), (0, 15)], "S")
        c.rect(0, 2, w, 2, "W")
        c.rect(0, 13, w, 15, "O")
        for x in range(0, w, 2):
            c.px(x, 12, g if g == "Y" else "S")
        return c
    for name, f in [("chev_r", ">"), ("chev_l", "<")]:
        c = slab(8)
        for k in range(4):
            xs = (1 + k) if f == ">" else (5 - k)
            for dx in (0, 1):
                c.px(xs + dx, 3 + k, g)
                c.px(xs + dx, 10 - k, g)
        if g == "Y":
            c.px(1 if f == ">" else 5, 3, "W")
        P[name] = c.outline()
    c = slab(16)
    for y in range(3, 11):
        for x in (3, 4, 11, 12):
            c.px(x, y, g)
    for y in (6, 7):
        for x in range(5, 11):
            c.px(x, y, g)
    if g == "Y":
        c.px(3, 3, "W")
        c.px(11, 3, "W")
    P["centre"] = c.outline()
    c = slab(8)
    for x in (3, 4):
        c.px(x, 6, "W")
        c.px(x, 7, "W")
    P["plain"] = c.outline()
    for side in ("l", "r"):
        c = C(8, 46)
        s2 = slab(8, left=(side == "l"), right=(side == "r"))
        c.stamp(s2, 0, 30)
        px0 = 1 if side == "l" else 5
        c.rect(px0, 8, px0 + 1, 32, "S")
        c.rect(px0, 8, px0, 32, "W")
        c.rect(px0 - 1, 29, px0 + 2, 32, "O")
        for y in (13, 18, 23):
            c.px(px0 + 1, y, "R")
            c.px(px0, y + 1, "W")
        c.rect(px0 - 1, 4, px0 + 2, 7, "O")
        c.rect(px0, 5, px0 + 1, 6, "Y" if g == "Y" else "O")
        P["cap_" + side] = c.outline()
    for nm, col in [("lamp_off", "R"), ("lamp_on", "Y")]:
        c = C(5, 5)
        c.rect(1, 1, 3, 3, col)
        if col == "Y":
            c.px(1, 1, "W")
        P[nm] = c.outline()
    return P


def pad(width, lit=True):
    """assembled pad of `width` px (48..96 in 8 px steps), 46 rows; the slab is rows 30-45"""
    P = pad_parts("Y" if lit else "O")
    n_side = (width - 32) // 8 // 2
    c = C(width, 46)
    x = 0
    c.stamp(P["cap_l"], x, 0)
    x += 8
    for i in range(n_side):
        c.stamp(P["chev_r"] if i % 2 == 0 else P["plain"], x, 30)
        x += 8
    c.stamp(P["centre"], x, 30)
    x += 16
    for i in range(n_side):
        c.stamp(P["chev_l"] if (n_side - 1 - i) % 2 == 0 else P["plain"], x, 30)
        x += 8
    c.stamp(P["cap_r"], x, 0)
    return c


# ----------------------------------------------------------------------------------------- destroyer
def destroyer(turret_state=("idle", "idle"), flash_aa=False):
    """a strait destroyer, facing left, 160 x 46 (feet at row 41). Cut into three BOB sections for the blitter:
    bow 64 / mid 64 / stern 32. Turret A (forward) and B (aft of the bridge) are separate parts with tell frames."""
    c = C(170, 50)
    base = 42
    ox = 4
    # hull
    c.poly([(ox + 0, base - 12), (ox + 6, base - 12), (ox + 150, base - 10), (ox + 152, base - 11), (ox + 147, base), (ox + 30, base), (ox + 12, base - 3), (ox + 4, base - 8)], "O")
    # sheer line (lit deck edge) and hull plating
    c.rect(ox + 6, base - 12, ox + 150, base - 12, "S")
    c.rect(ox + 0, base - 12, ox + 6, base - 12, "S")
    for x in range(ox + 14, ox + 148, 1):
        c.px(x, base - 9, "S")
    for x in range(ox + 24, ox + 146):
        c.px(x, base - 4, "R")
    for x in range(ox + 20, ox + 148, 2):
        c.px(x, base - 1, "K")
    for x in range(ox + 36, ox + 142, 17):                       # hull number blocks / scuttles
        c.rect(x, base - 7, x + 2, base - 6, "K")
    c.px(ox + 4, base - 9, "S"); c.px(ox + 6, base - 7, "S")
    # forward deck house under turret A (barbette)
    c.rect(ox + 20, base - 17, ox + 38, base - 13, "O")
    c.rect(ox + 20, base - 17, ox + 38, base - 17, "S")
    # bridge superstructure
    c.rect(ox + 54, base - 23, ox + 82, base - 13, "O")
    c.rect(ox + 54, base - 23, ox + 82, base - 23, "S")
    c.rect(ox + 54, base - 22, ox + 56, base - 13, "S")
    for x in range(ox + 58, ox + 81, 3):
        c.px(x, base - 19, "Y"); c.px(x + 1, base - 19, "Y")
    c.rect(ox + 60, base - 30, ox + 74, base - 24, "O")
    c.rect(ox + 60, base - 30, ox + 74, base - 30, "S")
    c.rect(ox + 60, base - 29, ox + 61, base - 24, "S")
    for x in range(ox + 63, ox + 74, 2):
        c.px(x, base - 27, "Y")
    c.line(ox + 67, base - 31, ox + 67, base - 40, "K")           # mast
    c.line(ox + 63, base - 37, ox + 71, base - 37, "S")
    c.line(ox + 65, base - 33, ox + 69, base - 33, "S")
    # radar on the mast top
    c.rect(ox + 64, base - 42, ox + 70, base - 41, "S")
    # funnels
    for fx in (ox + 92, ox + 108):
        c.rect(fx, base - 26, fx + 8, base - 13, "O")
        c.rect(fx, base - 26, fx + 8, base - 26, "K")
        c.rect(fx, base - 25, fx + 1, base - 13, "S")
        c.rect(fx, base - 23, fx + 8, base - 22, "R")
    # AA mounts (two) on the deck
    for ax in (ox + 46, ox + 124):
        c.rect(ax, base - 15, ax + 5, base - 13, "S")
        c.rect(ax + 1, base - 17, ax + 4, base - 16, "O")
        c.line(ax + 1, base - 17, ax - 3, base - 20, "S")
        c.line(ax + 4, base - 17, ax + 8, base - 20, "S")
    # aft deckhouse and rear mast
    c.rect(ox + 130, base - 19, ox + 142, base - 13, "O")
    c.rect(ox + 130, base - 19, ox + 142, base - 19, "S")
    c.rect(ox + 130, base - 18, ox + 131, base - 13, "S")
    c.line(ox + 125, base - 14, ox + 125, base - 30, "K")
    # turret bases (the turrets themselves are separate parts)
    c.rect(ox + 8, base - 15, ox + 22, base - 13, "S")
    c.rect(ox + 138, base - 16, ox + 150, base - 13, "S")
    c.outline()
    # waterline foam: broken, cream and stone
    for x in range(ox + 6, ox + 150):
        if (x * 7 + (x // 5)) % 11 < 4:
            c.px(x, base + 1, "W")
        elif (x * 5) % 13 < 3:
            c.px(x, base + 1, "S")
    for x in range(ox + 0, ox + 14):
        if x % 3 != 1:
            c.px(x, base - 2 + (x // 5), "W")
    return c


def turret(state="idle", mirror=False):
    """a twin-gun turret with long barrels, facing left, 30 x 18. states idle | tell1 | tell2 | fire.
    The tell is the barrels rising and a flash on the housing (frames per skill: 28/24/20/18/16)."""
    c = C(34, 22)
    ox = 18
    lift = {"idle": 0, "tell1": 1, "tell2": 2, "fire": 0}[state]
    c.rect(ox + 0, 13 - lift, ox + 14, 17 - lift, "O")
    c.rect(ox + 2, 11 - lift, ox + 12, 12 - lift, "O")
    c.rect(ox + 2, 11 - lift, ox + 12, 11 - lift, "S")
    c.rect(ox + 0, 13 - lift, ox + 14, 13 - lift, "S")
    c.rect(ox + 0, 14 - lift, ox + 1, 17 - lift, "S")
    ang = {"idle": 6, "tell1": 24, "tell2": 40, "fire": 40}[state]
    t1 = barrel(c, ox + 2, 14 - lift, ang, 17)
    t2 = barrel(c, ox + 2, 17 - lift, ang, 17) if False else t1
    c.line(ox + 2, 13 - lift, round(t1[0]), round(t1[1]) - 2, "S")
    c.outline()
    if state in ("tell2", "fire"):
        fx, fy = round(t1[0]) - 1, round(t1[1]) - 1
        for dx, dy, ch in [(0, 0, "W"), (-1, 0, "W"), (0, -1, "W"), (-2, 0, "Y"), (-1, -1, "Y"), (-1, 1, "Y"), (0, -2, "Y"), (-3, 0, "Y"),
                           (-2, -2, "R"), (-2, 2, "R"), (-4, 0, "R"), (0, 1, "Y"), (-3, -1, "R")]:
            c.px(fx + dx, fy + dy, ch)
        if state == "tell2":
            c.px(ox + 6, 14 - lift, "Y")           # glow on the housing
            c.px(ox + 7, 14 - lift, "Y")
    if state == "tell1":
        c.px(ox + 6, 14 - lift, "R")
    return c


# ----------------------------------------------------------------------------------------- effects
def splash(frame):
    """water splash 5 frames, 24 x 24, no outline (a white fringe against the sea): spike, column, crown, droplets, ring"""
    c = C(24, 24)
    cx = 11
    if frame == 0:
        c.rect(cx, 17, cx + 1, 22, "W")
        c.px(cx, 16, "W")
        c.rect(cx - 3, 22, cx + 4, 22, "S")
    elif frame == 1:
        c.poly([(cx - 2, 22), (cx - 1, 8), (cx + 2, 8), (cx + 3, 22)], "W")
        c.rect(cx + 2, 10, cx + 2, 22, "S")
        c.px(cx - 4, 14, "W"); c.px(cx + 5, 12, "W"); c.px(cx - 3, 9, "S")
        c.rect(cx - 5, 22, cx + 6, 22, "S")
    elif frame == 2:
        c.poly([(cx - 3, 22), (cx - 2, 12), (cx + 3, 12), (cx + 4, 22)], "W")
        for k in range(-7, 8):                                    # the crown
            h = 9 - abs(k) // 2
            if abs(k) > 2:
                c.px(cx + k, 22 - h, "W")
                c.px(cx + k, 22 - h + 1, "W" if k < 0 else "S")
        c.rect(cx + 3, 13, cx + 3, 22, "S")
        c.rect(cx - 8, 22, cx + 9, 22, "S")
    elif frame == 3:
        for (x, y) in [(3, 12), (6, 9), (9, 7), (14, 8), (17, 10), (20, 13), (5, 15), (18, 16), (12, 11)]:
            c.px(x, y, "W")
            c.px(x, y + 1, "S")
        c.rect(cx - 9, 21, cx + 10, 21, "W")
        c.rect(cx - 8, 22, cx + 9, 22, "S")
    else:
        for x in range(cx - 10, cx + 11):
            if (x * 3) % 5 < 3:
                c.px(x, 22, "S")
        for x in range(cx - 7, cx + 8, 2):
            c.px(x, 21, "W")
        c.px(cx - 4, 18, "W"); c.px(cx + 5, 17, "S")
    return c


def wake_bow(frame):
    """a boat's bow spray 2 frames 12 x 8"""
    c = C(12, 8)
    pts = [[(0, 7), (1, 5), (2, 4), (4, 6), (6, 7), (8, 7)], [(0, 7), (2, 6), (3, 3), (5, 5), (7, 7), (9, 6)]][frame]
    for x, y in pts:
        c.px(x, y, "W")
        c.px(x + 1, y, "S") if (x + y) % 2 else None
    return c


def whitecap(frame):
    """a rolling whitecap for the storm (Typhoon Strait): 3 frames 34 x 10, lit left"""
    c = C(34, 10)
    sh = frame * 2
    for x in range(34):
        h = int(3 + 2 * math.sin((x + sh) * 0.45) + 1.2 * math.sin((x + sh) * 0.9))
        top = 8 - h
        for y in range(top, 9):
            c.px(x, y, "W" if y - top < 2 and (x + sh) % 7 < 5 else ("S" if y - top < 3 else "B"))
    return c


def windsock(frame):
    """windsock on a pole 2 frames (limp / full) 16 x 22, 3 hoops red/cream"""
    c = C(16, 22)
    c.line(2, 21, 2, 3, "K", th=1)
    c.px(3, 21, "K"); c.px(1, 21, "K")
    if frame == 0:       # limp
        for i, ch in enumerate("RWRWR"):
            c.rect(3 + i, 4 + (i // 2), 3 + i, 8 + i, ch)
    else:                # streaming
        for i, ch in enumerate("RRWWRRWW"):
            c.rect(3 + i, 3 + i // 4, 3 + i, 7 - i // 4, ch)
    c.px(2, 2, "K")
    return c
