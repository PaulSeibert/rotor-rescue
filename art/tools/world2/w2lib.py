"""Shared helpers for the World 2 "Coral Islands" art: palettes and layout (from the approved pilot's ipals.py), pen arrays
(numpy uint8, 0 = transparent, 1-7 = PF2 pens) <-> images and agk text art, blue noise.
Layout lines: HUD 0-23, sky 24-179 (copper gradient), clouds 72-111, far islands 120-169 + far strip 170-179,
sea floor 180-207 (28 rows), terrain strip 208-255 (48 rows)."""
import os, sys
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
OUT = os.path.join(ROOT, "build", "world2") + os.sep       # scratch pictures
RAW = os.path.join(ROOT, "art", "world2", "raw") + os.sep  # Retro Diffusion raws the art is built from
ART = os.path.join(ROOT, "art", "world2") + os.sep         # the production assets


def rgb12(c):
    return (((c >> 8) & 15) * 17, ((c >> 4) & 15) * 17, (c & 15) * 17)


def to12(rgb):
    r, g, b = rgb[:3]
    return ((r + 8) // 17 << 8) | ((g + 8) // 17 << 4) | ((b + 8) // 17)


def _dist(a, b):
    dr, dg, db = a[0] - b[0], a[1] - b[1], a[2] - b[2]
    return 3 * dr * dr + 4 * dg * dg + 2 * db * db


def nearest(rgb, pal_rgb):
    best, bi = None, 0
    for i, p in enumerate(pal_rgb):
        d = _dist(rgb, p)
        if best is None or d < best:
            best, bi = d, i
    return bi


def zoom(im, k):
    return im.resize((im.width * k, im.height * k), Image.NEAREST)


# the game's fixed PF1 colours 1-7 (art/palette.txt): outline, olive drab, stone, cream white, red, yellow, blue
PF1 = [0x111, 0x564, 0x998, 0xFEC, 0xD42, 0xFC3, 0x36B]
HUD_BG = [0x88A, 0x558, 0x346, 0x235, 0x235, 0x234, 0x224, 0x224, 0x224, 0x124, 0x123, 0x123,
          0x123, 0x113, 0x113, 0x113, 0x113, 0x112, 0x112, 0x112, 0x112, 0x112, 0x99B, 0x001]
HUD_INK = [0xFFF, 0xFFE, 0xFFD, 0xFEB, 0xFD9, 0xFC7, 0xEB6, 0xDA5]

PLAY_TOP, SKY_LINES = 24, 172
CLOUDS_Y, FAR_Y, FARSTRIP_Y, SEA_Y, SEA_ROWS, STRIP_Y, STRIP_ROWS = 72, 120, 170, 180, 28, 208, 48
FEET_Y = 240

MISSIONS = [
  dict(name="Reef Run", tag="morning", sky=(0x28B, 0xCFE), C=0x179,
       clouds=[0x59B, 0x7BC, 0x9CD, 0xBDE, 0xDEE, 0xFEE, 0xFFF],
       far=[0x357, 0x468, 0x579, 0x68A, 0x8AB, 0xACC, 0xCEE],
       sea=[0x034, 0x057, 0x17A, 0x39C, 0x6BD, 0x8DE, 0xFFF],
       isl=[0x023, 0x142, 0x4A3, 0x975, 0xED9, 0x8DE, 0xFFF],
       dots=[0x9CD, 0xEFE, 0xFFF],
       aga=dict(sky=(0x2488CC, 0xCCFFEE), C=0x1A7C9A)),
  dict(name="Trade Winds", tag="midday haze", sky=(0x5AD, 0xDFF), C=0x1AB,
       clouds=[0x7AC, 0x9BD, 0xBCE, 0xDDE, 0xEEF, 0xFFF, 0xFFD],
       far=[0x69A, 0x7AB, 0x8BC, 0xACD, 0xBDD, 0xCEE, 0xDFF],
       sea=[0x035, 0x057, 0x17A, 0x3AC, 0x5BE, 0x9EF, 0xFFF],
       isl=[0x023, 0x153, 0x5B3, 0x985, 0xEDA, 0x9EF, 0xFFF],
       dots=[0xBDE, 0xFFF, 0xFFE],
       aga=dict(sky=(0x55AADD, 0xDDFFFF), C=0x18AABB)),
  dict(name="Typhoon Strait", tag="storm dusk", sky=(0x233, 0x8A9), C=0x254,
       clouds=[0x112, 0x223, 0x334, 0x445, 0x667, 0x889, 0xCA9],
       far=[0x233, 0x344, 0x455, 0x566, 0x677, 0x788, 0x899],
       sea=[0x022, 0x033, 0x145, 0x367, 0x588, 0x9BB, 0xDEE],
       isl=[0x012, 0x131, 0x252, 0x764, 0xB97, 0x9BB, 0xDEE],
       dots=[0x445, 0x889, 0xBBB],
       aga=dict(sky=(0x112D2D, 0x88AA99), C=0x1F4F4A)),
]

def sky_line(m, i):
    a, b = m["sky"]; n = SKY_LINES - 1; out = 0
    for s in (8, 4, 0):
        x, y = (a >> s) & 15, (b >> s) & 15
        v = x + (y - x) * i / n; lo = int(v); fr = v - lo
        thr = 0.5 if fr < 0.25 or fr > 0.75 else (0.0 if i & 1 else 1.0)
        out |= min(15, lo + (1 if fr > thr else 0)) << s
    return out

def grad(a, b, n, y0=0):
    """line-dithered gradient a..b over n lines (12-bit), like logic.c ditherFill (approx)"""
    out = []
    for i in range(n):
        c = 0
        for s in (8, 4, 0):
            x, y = (a >> s) & 15, (b >> s) & 15
            v = x + (y - x) * i / max(1, n - 1); lo = int(v); fr = v - lo
            thr = 0.5 if fr < 0.25 or fr > 0.75 else (0.0 if (i + y0) & 1 else 1.0)
            c |= min(15, lo + (1 if fr > thr else 0)) << s
        out.append(c)
    return out

def H(m):   # the sky colour on the last gradient line (the horizon haze)
    return m["sky"][1]


import w2noise
BN = w2noise.get()
rng = np.random.default_rng

def bn(x, y):
    """blue-noise threshold at integer coords (arrays ok), periodic 64"""
    return BN[np.asarray(y) % 64, np.asarray(x) % 64]

def pens_to_img(a, pal):
    """a: 2D uint8 pen array -> RGBA with pal = 7 colours (pens 1-7)."""
    h, w = a.shape
    lut = np.zeros((8, 4), np.uint8)
    for i, c in enumerate(pal):
        lut[i + 1] = rgb12(c) + (255,)
    return Image.fromarray(lut[a], "RGBA")

def save_txt(path, rows, pal, comment=""):
    """agk text art for a PF2 band: pens 1-7 as digits, . transparent"""
    with open(path, "w") as f:
        for l in comment.split("\n"):
            f.write("# " + l + "\n")
        f.write("colors\n  .  transparent\n")
        for i, c in enumerate(pal):
            f.write("  %d  0x%03X\n" % (i + 1, c))
        f.write("frame\n")
        for r in rows:
            f.write("".join("." if v == 0 else str(v) for v in r) + "\n")

def load_txt(path):
    lines = open(path).read().split("\n")
    rows = [l for l in lines[lines.index("frame") + 1:] if l.strip()]
    return [np.array([0 if ch == "." else int(ch) for ch in r], np.uint8) for r in rows]
