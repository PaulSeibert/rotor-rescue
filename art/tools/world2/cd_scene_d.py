"""Slice D previews: the freighter in its 4 stages on the world 2 backdrops with the chopper over the hold, the sink spray at the bow.
`python cd_scene_d.py <art/world2 dir> <out dir>`"""
import os
import sys

from PIL import Image

import cd_preview as P

FX, FY = 96, 213            # freighter top-left: last row (35) = y 248
BOWX = FX + 12


def scene(art, m, stage, with_chopper=True):
    bg = P.backdrop(m)
    fr = P.load_art(os.path.join(art, "freighter.txt"))[stage]
    sp = P.load_art(os.path.join(art, "sink_spray.txt"))
    colors, frames = P.read_txt(os.path.join(art, "..", "chopper.txt"))
    heli = P.sprite_img(colors, frames[0])
    P.paste(bg, fr, FX, FY)
    if stage >= 1:
        P.paste(bg, sp[stage - 1], BOWX - 12, FY + 33 - 11 + 1)
    if with_chopper:
        deck_y = FY + 19                                  # level deck edge at the pivot
        cx = FX + 64 - 16
        cy = deck_y - 20 - 4
        P.paste(bg, heli, cx, cy)
    return bg


def build(art, out):
    os.makedirs(out, exist_ok=True)
    box = (FX - 6, 186, FX + 134, 256)
    for m in ("m4", "m5", "m6"):
        ims = [P.crop4(scene(art, m, s), box) for s in range(4)]
        P.sheet(ims, 2).save(os.path.join(out, f"d_freighter_{m}_x4.png"))
    # true size sequence: the four stages side by side at 1x and 2x (the tell the player reads), on all three missions
    rows1, rows2 = [], []
    for m in ("m4", "m5", "m6"):
        ims = [scene(art, m, s).crop(box) for s in range(4)]
        w = Image.new("RGB", (sum(i.width for i in ims) + 6 * 5, ims[0].height + 12), (40, 40, 48))
        for k, i in enumerate(ims):
            w.paste(i, (6 + k * (i.width + 6), 6))
        rows1.append(w)
    one = Image.new("RGB", (rows1[0].width, sum(r.height for r in rows1)), (40, 40, 48))
    y = 0
    for r in rows1:
        one.paste(r, (0, y))
        y += r.height
    one.save(os.path.join(out, "d_freighter_sequence_1x.png"))
    one.resize((one.width * 2, one.height * 2), Image.NEAREST).save(os.path.join(out, "d_freighter_sequence_2x.png"))
    # the ship alone, x6, no chopper, on mission 5 (stages 0-3)
    items = []
    fr = P.load_art(os.path.join(art, "freighter.txt"))
    for f in fr:
        bg = Image.new("RGB", (f.width + 8, f.height + 6), P.rgb12(P.MISSIONS["m5"]["C"]))
        bg.paste(f, (4, 3), f)
        items.append(bg.resize((bg.width * 6, bg.height * 6), Image.NEAREST))
    P.sheet(items, 1).save(os.path.join(out, "d_freighter_x6.png"))
    sp = P.load_art(os.path.join(art, "sink_spray.txt"))
    items = []
    for f in sp:
        bg = Image.new("RGB", (f.width + 4, f.height + 4), P.rgb12(P.MISSIONS["m5"]["C"]))
        bg.paste(f, (2, 2), f)
        items.append(bg.resize((bg.width * 12, bg.height * 12), Image.NEAREST))
    P.sheet(items, 3).save(os.path.join(out, "d_sink_spray_x12.png"))


if __name__ == "__main__":
    build(sys.argv[1], sys.argv[2])
