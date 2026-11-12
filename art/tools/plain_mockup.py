#!/usr/bin/env python3
"""Mock-ups of the plain for approval (needs PIL: Windows python).
    python plain_mockup.py OUT_DIR [suffix]
writes OUT_DIR/first-plain-mockup<suffix>.png (mission 1 full screen at 2x, then missions 2 and 3's
plain crops) and OUT_DIR/first-plain-scroll<suffix>.png (8 camera offsets, real per-line speeds).
"""
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from PIL import Image, ImageDraw
import plain_compose as pc


def to_image(rgb, y0, y1):
    im = Image.new('RGB', (320, y1 - y0))
    px = im.load()
    for y in range(y0, y1):
        row = rgb[y - pc.PLAY_TOP]
        for x in range(320):
            c = row[x]
            px[x, y - y0] = ((c >> 8 & 15) * 17, (c >> 4 & 15) * 17, (c & 15) * 17)
    return im


def main():
    out = sys.argv[1]
    suf = sys.argv[2] if len(sys.argv) > 2 else ''
    A = pc.Art()
    S = 2
    ims = []
    rgb, _ = pc.compose(pc.MISSIONS[0], 0, A)
    ims.append(to_image(rgb, pc.PLAY_TOP, 256).resize((320 * S, (256 - pc.PLAY_TOP) * S), Image.NEAREST))
    for m in pc.MISSIONS[1:]:
        rgb, _ = pc.compose(m, 0, A)
        ims.append(to_image(rgb, 120, 232).resize((320 * S, 112 * S), Image.NEAREST))
    H = sum(i.height for i in ims) + 4 * len(ims) + 4
    sheet = Image.new('RGB', (320 * S, H), (12, 12, 16))
    y = 0
    for i in ims:
        sheet.paste(i, (0, y))
        y += i.height + 4
    sheet.save(os.path.join(out, 'first-plain-mockup%s.png' % suf))
    # scroll: mission 1, 8 offsets, lines 140-226
    cams = [0, 37, 72, 109, 144, 181, 216, 253]
    ims = []
    for cam in cams:
        rgb, _ = pc.compose(pc.MISSIONS[0], cam, A)
        im = to_image(rgb, 140, 226).resize((320 * S, 86 * S), Image.NEAREST)
        ImageDraw.Draw(im).text((4, 2), 'camera %d' % cam, fill=(255, 255, 255))
        ims.append(im)
    sheet = Image.new('RGB', (320 * S, sum(i.height + 4 for i in ims)), (12, 12, 16))
    y = 0
    for i in ims:
        sheet.paste(i, (0, y))
        y += i.height + 4
    sheet.save(os.path.join(out, 'first-plain-scroll%s.png' % suf))


main()
