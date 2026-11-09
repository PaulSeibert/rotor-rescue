#!/usr/bin/env python3
"""The sky dots' sprite shapes -> src/sky_gfx.h (python3 tools/mkclouds.py).

16 x 3 on sprite channel 6 (colours 29-31 per mission): a sunlit wisp, or two
stars for the night. (The clouds themselves are a PF2 band: art/tools/draw.py.)
"""
import math
import os

CLOUD_H = [12, 10, 14]


def cloud(shape, h):
    """Puffs (circles) on a flat base; shading by height inside the cloud."""
    puffs = {
        0: [(8, 7, 5), (16, 5, 6), (24, 7, 5)],
        1: [(10, 5, 5), (20, 5, 6)],
        2: [(7, 9, 5), (15, 6, 7), (24, 8, 6)],
    }[shape]
    img = [[0] * 32 for _ in range(h)]
    for y in range(h):
        for x in range(32):
            inside = any((x - cx) ** 2 + ((y - cy) * 1.3) ** 2 <= r * r for cx, cy, r in puffs)
            if not inside or y > h - 2 and (x < 3 or x > 28):
                continue
            # light from above: top rows lit, underside dark; a checker dither at the seams
            t = y / (h - 1)
            v = 3 if t < 0.35 else 2 if t < 0.75 else 1
            if abs(t - 0.35) < 0.08 or abs(t - 0.75) < 0.08:
                v = v if (x + y) & 1 else max(1, v - 1) if t > 0.5 else min(3, v + 1)
            img[y][x] = v
    return img


def wisp():
    rows = ["....1122221.....",
            "..1223333332211.",
            ".....112211....."]
    return [[int(c) if c != "." else 0 for c in r] for r in rows]


def stars():
    rows = ["....2.......1...",
            "...232..........",
            "....2.......1..."]
    return [[int(c) if c != "." else 0 for c in r] for r in rows]


def words(img, x0):
    """Sprite lines for 16 px from column x0: DATA (bit 0), DATB (bit 1) per line."""
    out = []
    for row in img:
        a = b = 0
        for i in range(16):
            v = row[x0 + i]
            a |= (v & 1) << (15 - i)
            b |= ((v >> 1) & 1) << (15 - i)
        out += [a, b]
    return out


def arr(name, ws):
    return f"static const UWORD {name}[{len(ws)}] = {{" + ", ".join(f"0x{w:04x}" for w in ws) + "};"


def main():
    lines = ["// Made by tools/mkclouds.py - don't edit.", "#ifndef _SKY_GFX_H_", "#define _SKY_GFX_H_"]
    lines.append(arr("s_pWisp", words(wisp(), 0)))
    lines.append(arr("s_pStars", words(stars(), 0)))
    lines.append("#endif")
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "sky_gfx.h")
    open(out, "w").write("\n".join(lines) + "\n")
    print("wrote", out)


if __name__ == "__main__":
    main()
