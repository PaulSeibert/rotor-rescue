"""Previews of the slice C/D art on the world 2 backdrops of missions 4, 5, 6, with the chopper for scale.

Backdrop = the documented pens (docs/STYLE-world2.md section 3): sky gradient lines 24-179 to the haze H, floor lines
180-207 a gradient H to C with perspective wave dashes, strip lines 208-255 flat C with ripples (island pens 6, 7, 1).
`python cd_preview.py <art/world2 dir> <out dir>`
"""
import os
import random
import sys

from PIL import Image

from cdlib import RGB, read_txt

SW, SH = 320, 256


def rgb12(h):
    return (((h >> 8) & 15) * 17, ((h >> 4) & 15) * 17, (h & 15) * 17)


MISSIONS = {
    "m4": dict(top=0x28B, H=0xCFE, C=0x179, sea=(0x034, 0x057, 0x17A, 0x39C, 0x6BD, 0x8DE, 0xFFF),
               isl=(0x023, 0x142, 0x4A3, 0x975, 0xED9, 0x8DE, 0xFFF)),
    "m5": dict(top=0x5AD, H=0xDFF, C=0x1AB, sea=(0x035, 0x057, 0x17A, 0x3AC, 0x5BE, 0x9EF, 0xFFF),
               isl=(0x023, 0x153, 0x5B3, 0x985, 0xEDA, 0x9EF, 0xFFF)),
    "m6": dict(top=0x233, H=0x8A9, C=0x254, sea=(0x022, 0x033, 0x145, 0x367, 0x588, 0x9BB, 0xDEE),
               isl=(0x012, 0x131, 0x252, 0x764, 0xB97, 0x9BB, 0xDEE)),
}


def lerp(a, b, t):
    return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3))


def backdrop(m, seed=3):
    p = MISSIONS[m]
    img = Image.new("RGB", (SW, SH), (0, 0, 0))
    px = img.load()
    top, H, C = rgb12(p["top"]), rgb12(p["H"]), rgb12(p["C"])
    for y in range(24):
        for x in range(SW):
            px[x, y] = (0x11, 0x11, 0x33)           # HUD stand-in
    for y in range(24, 180):
        col = lerp(top, H, (y - 24) / 155)
        for x in range(SW):
            px[x, y] = col
    for y in range(180, 208):
        col = lerp(H, C, (y - 180) / 27)
        for x in range(SW):
            px[x, y] = col
    for y in range(208, 256):
        for x in range(SW):
            px[x, y] = C
    rnd = random.Random(seed)
    sea = [rgb12(v) for v in p["sea"]]
    for j in range(1, 27):                            # perspective dashes
        y = 180 + j
        ln = max(2, int(1.5 + j * 0.75))
        n = 5 + int(j * 0.9)
        for _ in range(n):
            x = rnd.randrange(0, SW)
            for k in range(ln):
                if j < 9:
                    px[(x + k) % SW, y] = lerp(H, sea[4], 0.35)
                else:
                    px[(x + k) % SW, y] = sea[4]
                    if (x + k + 1) < SW and y + 1 < 208:
                        px[(x + k + 1) % SW, y + 1] = sea[2]
    isl = [rgb12(v) for v in p["isl"]]
    for y in range(209, 256):                         # strip water ripples
        for _ in range(7):
            x = rnd.randrange(0, SW)
            ln = rnd.randrange(3, 3 + (y - 208) // 5 + 3)
            c = isl[5] if rnd.random() < 0.7 else (isl[0] if rnd.random() < 0.6 else isl[6])
            for k in range(ln):
                if x + k < SW:
                    px[x + k, y] = c
    return img


def sprite_img(colors, rows, scale=1):
    h, w = len(rows), len(rows[0])
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    px = im.load()
    for y, r in enumerate(rows):
        for x, ch in enumerate(r):
            if ch != "." and colors.get(ch):
                px[x, y] = colors[ch] + (255,)
    return im


def load_art(path):
    colors, frames = read_txt(path)
    return [sprite_img(colors, f) for f in frames]


def paste(bg, im, x, y):
    bg.paste(im, (x, y), im)


def crop4(img, box, scale=4):
    c = img.crop(box)
    return c.resize((c.width * scale, c.height * scale), Image.NEAREST)


def sheet(images, cols, pad=6, bg=(40, 40, 48)):
    w = max(i.width for i in images)
    h = max(i.height for i in images)
    rows = (len(images) + cols - 1) // cols
    out = Image.new("RGB", (cols * (w + pad) + pad, rows * (h + pad) + pad), bg)
    for n, im in enumerate(images):
        out.paste(im, (pad + (n % cols) * (w + pad), pad + (n // cols) * (h + pad)))
    return out
