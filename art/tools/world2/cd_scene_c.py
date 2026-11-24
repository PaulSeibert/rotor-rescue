"""Slice C previews: deck ship + chopper hovering + rope/ladder + climbing man + people on deck + lamps.
`python cd_scene_c.py <art/world2 dir> <out dir>`"""
import os
import sys

from PIL import Image

import cd_preview as P

SX, SY = 100, 219          # ship's top-left: the BOB's last row (29) is y 248


def chopper(art):
    colors, frames = P.read_txt(os.path.join(art, "..", "chopper.txt"))
    return P.sprite_img(colors, frames[0])


def scene(art, m, ship_frame, rope_frame, man_frame, chopper_dy=-12):
    bg = P.backdrop(m)
    ship = P.load_art(os.path.join(art, "deck_ship.txt"))[ship_frame]
    rope = P.load_art(os.path.join(art, "winch_rope.txt"))[rope_frame]
    man = P.load_art(os.path.join(art, "man_climb.txt"))[man_frame]
    hostage = P.load_art(os.path.join(art, "..", "hostage.txt"))
    lamp = P.load_art(os.path.join(art, "pad_lamp.txt"))
    heli = chopper(art)
    deck_y = SY + 8                                      # deck edge row
    hx = SX + 47                                         # hover box 24 wide: x 47..71 on the ship
    cx = hx + 12 - 16                                    # chopper centred on the box
    cy = deck_y - 20 + chopper_dy                        # box is 20 high above the deck
    # rope from the belly
    rx = cx + 16 - 4
    ry = cy + 17
    clip = rope.crop((0, 0, 8, min(40, deck_y - ry)))
    P.paste(bg, clip, rx, ry)
    P.paste(bg, ship, SX, SY)
    P.paste(bg, heli, cx, cy)
    P.paste(bg, man, rx + 4 - 7 if rope_frame == 0 else rx + 3 - 7, ry + 12)
    # lamps on the deck, 24 px apart
    P.paste(bg, lamp[1], hx - 5, deck_y - 5)
    P.paste(bg, lamp[0], hx + 24, deck_y - 5)
    # people on deck: stand, wave, wave
    for k, (px, f) in enumerate(((SX + 28, 3), (SX + 76, 4), (SX + 82, 0))):
        P.paste(bg, hostage[f], px, deck_y - 12)
    return bg


def build(art, out):
    os.makedirs(out, exist_ok=True)
    full, crops = [], []
    for m in ("m4", "m5", "m6"):
        row = []
        for sf in (0, 1):
            im = scene(art, m, sf, 1 + sf, sf)
            row.append(P.crop4(im, (SX - 6, 186, SX + 130, 256)))
            if sf == 0:
                full.append(im)
        crops.append(P.sheet(row, 2))
    P.sheet(full, 3).save(os.path.join(out, "c_deck_ship_scene_1x.png"))
    for m, c in zip(("m4", "m5", "m6"), crops):
        c.save(os.path.join(out, f"c_deck_ship_{m}_x4.png"))
    # ship alone, x6, on neutral and on each C
    ships = P.load_art(os.path.join(art, "deck_ship.txt"))
    items = []
    for f in ships:
        for m in ("m4", "m6"):
            bg = Image.new("RGB", (f.width + 8, f.height + 6), P.rgb12(P.MISSIONS[m]["C"]))
            bg.paste(f, (4, 3), f)
            items.append(bg.resize((bg.width * 6, bg.height * 6), Image.NEAREST))
    P.sheet(items, 2).save(os.path.join(out, "c_deck_ship_x6.png"))
    # parts sheet x10 on the mission 5 colour
    parts = []
    for name in ("winch_rope", "winch_hook", "man_climb"):
        for f in P.load_art(os.path.join(art, name + ".txt")):
            bg = Image.new("RGB", (f.width + 4, f.height + 4), P.rgb12(P.MISSIONS["m5"]["C"]))
            bg.paste(f, (2, 2), f)
            parts.append(bg.resize((bg.width * 10, bg.height * 10), Image.NEAREST))
    P.sheet(parts, 6).save(os.path.join(out, "c_parts_x10.png"))


if __name__ == "__main__":
    build(sys.argv[1], sys.argv[2])
