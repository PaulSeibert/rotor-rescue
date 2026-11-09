#!/usr/bin/env python3
"""The ROTOR RESCUE title logo for the Amiga: the wordmark on its winged badge
(art/src/rotor_rescue_title_source.png, 512x512 RGBA) trimmed to its opaque
area and scaled to W px wide, in 4 bitplanes: colour 0 is see-through (the
sky's copper gradient shows round it), 7 pens keep one colour for the whole
picture and 8 more are reloaded by the copper on every line where the picture
needs it, so ROTOR's blue and RESCUE's gold keep their gradients. A light
ordered (Bayer) dither mixes neighbouring colours within a line.

Writes:
  disk/titlelogo.bin        the picture for the floppy (agk packs it): the 7
                            fixed colours, ROW_REGS colours per line (words),
                            then 4 interleaved bitplanes (W / 8 bytes a plane)
  src/title_logo.h          its sizes and offsets, for main.c
  art/title_logo_amiga.png  how it looks on the Amiga (2x, over a sky), for the docs

Plain Python 3, no libraries:
    python3 art/tools/make_title_logo.py
"""
import os
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from make_dps_logo import save_png, to12, r12, rgb, dist, nearest, kmeans  # noqa: E402

ROOT = os.path.join(HERE, '..', '..')
SRC = os.path.join(ROOT, 'art', 'src', 'rotor_rescue_title_source.png')

W = 192            # multiple of 16: 12 words a line
PLANES = 4
FIXED = 7          # pens 1-7: one colour for the whole picture
ROW_REGS = 8       # pens 8-15: reloaded line by line
FIRST_ROW_REG = 1 + FIXED
ALPHA_MIN = 128    # source alpha (0-255, area averaged) from which a pixel is solid
DITHER = 0.6       # 0: none; 1: a full ordered mix between the two nearest colours
BAYER = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]


def load_rgba(path):
    data = open(path, 'rb').read()
    assert data[:8] == b'\x89PNG\r\n\x1a\n', 'not a PNG'
    pos, idat = 8, b''
    while pos < len(data):
        n, tag = struct.unpack('>I4s', data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        if tag == b'IHDR':
            w, h, depth, ctype, _, _, inter = struct.unpack('>IIBBBBB', body)
            assert depth == 8 and ctype == 6 and not inter, 'need 8-bit RGBA'
        elif tag == b'IDAT':
            idat += body
        pos += 12 + n
    stride = w * 4
    raw = zlib.decompress(idat)
    prev = bytearray(stride)
    px = []
    for y in range(h):
        f = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - 4] if i >= 4 else 0
            b = prev[i]
            c = prev[i - 4] if i >= 4 else 0
            if f == 1:
                line[i] = (line[i] + a) & 255
            elif f == 2:
                line[i] = (line[i] + b) & 255
            elif f == 3:
                line[i] = (line[i] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        px.append([tuple(line[x * 4:x * 4 + 4]) for x in range(w)])
        prev = line
    return w, h, px


def pick(c, pal, x, y):
    """the pen for colour c (0-15 units, floats) at (x, y): the nearest, or
    with the ordered dither its neighbour, as often as c lies towards it"""
    best = sorted(range(len(pal)), key=lambda i: dist(c, pal[i]))[:2]
    i0, i1 = best[0], best[1]
    if DITHER <= 0 or i0 == i1:
        return i0
    a, b = pal[i0], pal[i1]
    ab = [b[k] - a[k] for k in range(3)]
    ll = sum(v * v for v in ab)
    if not ll:
        return i0
    t = sum((c[k] - a[k]) * ab[k] for k in range(3)) / ll      # 0 at a, 1 at b
    t = max(0.0, min(1.0, t)) * DITHER
    return i1 if t > (BAYER[y & 3][x & 3] + 0.5) / 16 else i0


def main():
    sw, sh, src = load_rgba(SRC)
    xs = [x for y in range(sh) for x in range(sw) if src[y][x][3] >= ALPHA_MIN]
    ys = [y for y in range(sh) for x in range(sw) if src[y][x][3] >= ALPHA_MIN]
    bx0, bx1, by0, by1 = min(xs), max(xs) + 1, min(ys), max(ys) + 1
    scale = (bx1 - bx0) / W
    H = int(round((by1 - by0) / scale))
    print(f'source box x {bx0}-{bx1} y {by0}-{by1}, scale 1/{scale:.3f} -> {W}x{H}')

    # area average down to W x H, premultiplied by alpha; solid where the
    # covered alpha reaches ALPHA_MIN (colours in 0-15 units, unrounded)
    img = []
    for y in range(H):
        fy0, fy1 = by0 + y * scale, by0 + (y + 1) * scale
        row = []
        for x in range(W):
            fx0, fx1 = bx0 + x * scale, bx0 + (x + 1) * scale
            r = g = b = a = n = 0.0
            for yy in range(int(fy0), min(sh, int(fy1 + 0.999))):
                wy = min(fy1, yy + 1) - max(fy0, yy)
                if wy <= 0:
                    continue
                line = src[yy]
                for xx in range(int(fx0), min(sw, int(fx1 + 0.999))):
                    wx = min(fx1, xx + 1) - max(fx0, xx)
                    if wx <= 0:
                        continue
                    p = line[xx]
                    wa = wx * wy * p[3]
                    r += p[0] * wa; g += p[1] * wa; b += p[2] * wa; a += wa; n += wx * wy
            if a / n >= ALPHA_MIN:
                row.append((r / a / 17, g / a / 17, b / a / 17))
            else:
                row.append(None)
        img.append(row)

    # the fixed pens: the whole picture's main colours (outline, metal, red...)
    hist = {}
    for row in img:
        for c in row:
            if c is not None:
                k = r12(c)
                hist[k] = hist.get(k, 0) + 1
    fixed = [r12(c) for c in kmeans(list(hist.items()), FIXED)]
    print('fixed:', ' '.join('%X%X%X' % c for c in fixed))

    # per line: ROW_REGS more for what the fixed ones fit badly
    rowpal, prev = [], None
    for y, row in enumerate(img):
        h = {}
        for c in row:
            if c is None:
                continue
            k = r12(c)
            d = nearest(k, fixed)[1]
            if d > 4:
                h[k] = h.get(k, 0) + d
        pts = list(h.items())
        cent = [r12(c) for c in kmeans(pts, ROW_REGS)] if pts else []
        while len(cent) < ROW_REGS:
            cent.append(prev[len(cent)] if prev else fixed[0])
        if prev:
            # keep each register's colour where it can: fewer copper MOVEs
            order, left = [None] * ROW_REGS, list(cent)
            for i in range(ROW_REGS):
                if prev[i] in left:
                    order[i] = prev[i]
                    left.remove(prev[i])
            for i in range(ROW_REGS):
                if order[i] is None:
                    j = min(range(len(left)), key=lambda j: dist(left[j], prev[i]))
                    order[i] = left.pop(j)
            cent = order
        rowpal.append(cent)
        prev = cent

    # pixels -> pens (0: see-through)
    pens = []
    for y, row in enumerate(img):
        pal = fixed + rowpal[y]
        pens.append([0 if c is None else 1 + pick(c, pal, x, y) for x, c in enumerate(row)])

    moves = sum(1 for y in range(1, H) for i in range(ROW_REGS) if rowpal[y][i] != rowpal[y - 1][i])
    lines = sum(1 for y in range(1, H) if rowpal[y] != rowpal[y - 1])
    most = max(sum(1 for i in range(ROW_REGS) if rowpal[y][i] != rowpal[y - 1][i]) for y in range(1, H))
    print(f'{moves} colour changes on {lines} of {H} lines (at most {most} a line)')

    def w12(c):
        return (c[0] << 8) | (c[1] << 4) | c[2]

    # disk/titlelogo.bin: the fixed colours, the per-line colours (words,
    # big-endian), then the bitplanes
    blob = bytearray()
    for c in fixed:
        blob += struct.pack('>H', w12(c))
    rows_offs = len(blob)
    for r in rowpal:
        for c in r:
            blob += struct.pack('>H', w12(c))
    while len(blob) & 3:
        blob.append(0)
    planes_offs = len(blob)
    for y in range(H):
        for p in range(PLANES):
            for xb in range(W // 8):
                v = 0
                for bit in range(8):
                    if (pens[y][xb * 8 + bit] >> p) & 1:
                        v |= 0x80 >> bit
                blob.append(v)
    os.makedirs(os.path.join(ROOT, 'disk'), exist_ok=True)
    open(os.path.join(ROOT, 'disk', 'titlelogo.bin'), 'wb').write(blob)

    hdr = f"""// Generated by art/tools/make_title_logo.py - don't edit.
// The title logo on the floppy (disk/titlelogo.bin -> titlelogo.bin.pk):
// TITLE_LOGO_FIXED colours (words) for colours 1.., TITLE_LOGO_ROW_REGS a
// line for colours TITLE_LOGO_FIRST_ROW_REG.. (colour 0 is see-through); then
// TITLE_LOGO_PLANES interleaved bitplanes, TITLE_LOGO_W x TITLE_LOGO_H.
#ifndef _TITLE_LOGO_H_
#define _TITLE_LOGO_H_
#define TITLE_LOGO_W {W}
#define TITLE_LOGO_H {H}
#define TITLE_LOGO_PLANES {PLANES}
#define TITLE_LOGO_FIXED {FIXED}
#define TITLE_LOGO_ROW_REGS {ROW_REGS}
#define TITLE_LOGO_FIRST_ROW_REG {FIRST_ROW_REG}
#define TITLE_LOGO_CHANGES {moves}        // colour MOVEs after the first line
#define TITLE_LOGO_ROWS_OFFS {rows_offs}
#define TITLE_LOGO_PLANES_OFFS {planes_offs}
#define TITLE_LOGO_BYTES {len(blob)}
#endif
"""
    open(os.path.join(ROOT, 'src', 'title_logo.h'), 'w').write(hdr)

    # the preview: 2x, over a sunset sky like the title's
    out = []
    for y in range(H + 8):
        sky = rgb((max(0, 6 - y // 40), max(0, 3 - y // 60), max(2, 8 - y // 25)))
        row = []
        for x in range(W + 16):
            ly, lx = y - 4, x - 8
            if 0 <= ly < H and 0 <= lx < W and pens[ly][lx]:
                pal = fixed + rowpal[ly]
                row.append(rgb(pal[pens[ly][lx] - 1]))
            else:
                row.append(sky)
        out.append(row)
    big = [[p for p in r for _ in range(2)] for r in out for _ in range(2)]
    save_png(os.path.join(ROOT, 'art', 'title_logo_amiga.png'), (W + 16) * 2, (H + 8) * 2, big)
    print(f'disk/titlelogo.bin {len(blob)} bytes')


if __name__ == '__main__':
    main()
