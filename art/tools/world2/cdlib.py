"""Shared helpers for the slice C/D art (deck ship, winch rope, climbing man, freighter, sink spray).

Text-art files are in art/world2/*.txt: a `colors` block (letter, hex) then `frame` blocks of equal size.
Pens are the PF1 pens 1-7 (the same 7 letters as the gunboat / radar): K 0x111 outline, O 0x564 olive,
S 0x998 stone, W 0xFEC cream, R 0xD42 red, Y 0xFC3 yellow, B 0x36B blue.
"""
import os

HEADER = """colors
  .  transparent
  K  0x111
  O  0x564
  S  0x998
  W  0xFEC
  R  0xD42
  Y  0xFC3
  B  0x36B
"""
PENS = "KOSWRYB"
RGB = {"K": (0x11, 0x11, 0x11), "O": (0x55, 0x66, 0x44), "S": (0x99, 0x99, 0x88), "W": (0xFF, 0xEE, 0xCC),
       "R": (0xDD, 0x44, 0x22), "Y": (0xFF, 0xCC, 0x33), "B": (0x33, 0x66, 0xBB)}


class Cv:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.g = [["."] * w for _ in range(h)]

    def set(self, x, y, c):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.g[y][x] = c

    def get(self, x, y):
        if 0 <= x < self.w and 0 <= y < self.h:
            return self.g[y][x]
        return "."

    def only(self, x, y, c):
        """set only on a transparent pixel"""
        if self.get(x, y) == ".":
            self.set(x, y, c)

    def rect(self, x0, y0, x1, y1, c):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.set(x, y, c)

    def line(self, x0, y0, x1, y1, c):
        dx, dy = abs(x1 - x0), -abs(y1 - y0)
        sx, sy = (1 if x0 < x1 else -1), (1 if y0 < y1 else -1)
        err = dx + dy
        while True:
            self.set(x0, y0, c)
            if x0 == x1 and y0 == y1:
                break
            e2 = 2 * err
            if e2 >= dy:
                err += dy
                x0 += sx
            if e2 <= dx:
                err += dx
                y0 += sy

    def rows(self):
        return ["".join(r) for r in self.g]


def write_txt(path, comment, frames):
    """frames: list of Cv (equal size) -> text-art file, LF line ends"""
    w, h = frames[0].w, frames[0].h
    out = []
    for line in comment.strip().splitlines():
        out.append("# " + line)
    out.append(HEADER.rstrip("\n"))
    for f in frames:
        assert (f.w, f.h) == (w, h), "frame size mismatch"
        out.append("frame")
        out.extend(f.rows())
    with open(path, "w", newline="\n") as fh:
        fh.write("\n".join(out) + "\n")


def read_txt(path):
    """-> (colors {letter: (r,g,b) or None}, [rows-list per frame])"""
    colors, frames, cur, mode = {}, [], None, None
    for raw in open(path).read().splitlines():
        if raw.startswith("#"):
            continue
        s = raw.strip()
        if s == "colors":
            mode = "c"
            continue
        if s == "frame":
            cur = []
            frames.append(cur)
            mode = "f"
            continue
        if mode == "c" and s:
            p = s.split()
            if p[1] == "transparent":
                colors[p[0]] = None
            else:
                v = int(p[1], 16)
                colors[p[0]] = (((v >> 8) & 15) * 17, ((v >> 4) & 15) * 17, (v & 15) * 17)
        elif mode == "f" and raw:
            cur.append(raw)
    return colors, frames
