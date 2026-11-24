"""Slice E previews: the destroyer with its new states on the world 2 backdrops of missions 4, 5, 6, chopper for scale.
`python e_preview.py <art/world2 dir> <out dir>`
Pillow mock-ups of the backdrops (cd_preview.backdrop), not game screenshots. The hull sits with its last row on y 248.
"""
import os
import sys

from PIL import Image

import cd_preview as P
from e_art import AA_SLOTS, TURRET_MOUNTS, list_offsets, clean_hull, load_hull0

HX, HY = 70, 248 - 44 + 1             # hull top-left on the 320 x 256 screen


class Art:
    def __init__(self, art):
        L = lambda n: P.load_art(os.path.join(art, n))
        self.hull_old = L("destroyer_hull.txt")
        self.hull = L("hull_list.txt")          # 0 clean, 1 list
        self.tur = L("destroyer_turret.txt")    # 0-3 left: idle, tell1, tell2, fire
        self.hit = L("turret_hit.txt")          # 0 smoking, 1 dead
        self.aa = L("aa_gun.txt")
        self.off = list_offsets(clean_hull(load_hull0(art)))
        self.bar = L("barrage_tell.txt")
        colors, frames = P.read_txt(os.path.join(art, "..", "chopper.txt"))
        self.heli = P.sprite_img(colors, frames[0])


def compose(A, m, hull=0, fwd=("idle", 0), aft=("idle", 0), aa=0, listing=False, heli=None, barrage=None, bg=None):
    """fwd / aft: ('idle', i) | ('hit', i); aa: frame index or None; heli: (x, y) or None; barrage: [(x, y, frame)]"""
    b = (bg or P.backdrop(m)).copy()
    hl = A.hull[1 if listing else 0]
    off = A.off if listing else {k: 0 for k in "A B C D fwd aft".split()}
    P.paste(b, hl, HX, HY)
    for name, st in (("fwd", fwd), ("aft", aft)):
        if st is None:
            continue
        mx, my = TURRET_MOUNTS[name]
        dy = off[name]
        im = (A.tur if st[0] == "idle" else A.hit)[st[1]]
        P.paste(b, im, HX + mx, HY + my + dy)
    if aa is not None:
        for k, (sx, sy) in AA_SLOTS.items():
            dy = off[k]
            P.paste(b, A.aa[aa], HX + sx, HY + sy + dy)
    for (bx, by, f) in (barrage or []):
        P.paste(b, A.bar[f], bx, by)
    if heli:
        P.paste(b, A.heli, heli[0], heli[1])
    return b


BOX = (HX - 14, 176, HX + 154 + 14, 256)


def x4(img, box=BOX, s=4):
    return P.crop4(img, box, s)


def stack(images, pad=6, bg=(40, 40, 48)):
    w = max(i.width for i in images)
    h = sum(i.height for i in images) + pad * (len(images) + 1)
    out = Image.new("RGB", (w + 2 * pad, h), bg)
    y = pad
    for i in images:
        out.paste(i, (pad, y))
        y += i.height + pad
    return out


def build(art, out):
    os.makedirs(out, exist_ok=True)
    A = Art(art)
    heli = (HX + 60, 176 + 8)
    for m in ("m4", "m5", "m6"):
        # AA states (the three frames, all four mounts)
        ims = [x4(compose(A, m, aa=k, heli=heli)) for k in range(3)]
        stack(ims).save(os.path.join(out, f"e_aa_{m}_x4.png"))
        # turret states: smoking forward / idle aft; dead forward / smoking aft; both dead + list
        s1 = compose(A, m, fwd=("hit", 0), aft=("idle", 0), aa=0, heli=heli)
        s2 = compose(A, m, fwd=("hit", 1), aft=("hit", 0), aa=0, heli=heli)
        s3 = compose(A, m, fwd=("hit", 1), aft=("hit", 1), aa=0, heli=heli)
        s4 = compose(A, m, fwd=("hit", 1), aft=("hit", 1), aa=0, listing=True, heli=heli)
        stack([x4(s) for s in (s1, s2, s3, s4)]).save(os.path.join(out, f"e_turrets_list_{m}_x4.png"))
        # the barrage: three frames on the water ahead of the bow (two markers each, the chopper over them), the destroyer behind
        ims = []
        for f in range(3):
            sc = compose(A, m, aa=0, heli=(26, 186), barrage=[(6, 232, f), (36, 238, f), (20, 224, f)])
            ims.append(P.crop4(sc, (0, 176, 320, 256), 3))
        stack(ims).save(os.path.join(out, f"e_barrage_{m}_x3.png"))

    # true size and 2x, the whole story in a row per mission
    rows = []
    for m in ("m4", "m5", "m6"):
        sc = [compose(A, m, aa=0, heli=heli),
              compose(A, m, fwd=("idle", 2), aa=1, heli=heli),
              compose(A, m, fwd=("idle", 3), aa=2, heli=heli),
              compose(A, m, fwd=("hit", 0), aa=0, heli=heli),
              compose(A, m, fwd=("hit", 1), aft=("hit", 1), aa=0, listing=True, heli=heli)]
        crops = [s.crop(BOX) for s in sc]
        w = Image.new("RGB", (crops[0].width * 3 + 24, crops[0].height * 2 + 18), (40, 40, 48))
        for k, c in enumerate(crops):
            w.paste(c, (6 + (k % 3) * (c.width + 6), 6 + (k // 3) * (c.height + 6)))
        rows.append(w)
    one = stack(rows)
    one.save(os.path.join(out, "e_story_1x.png"))
    one.resize((one.width * 2, one.height * 2), Image.NEAREST).save(os.path.join(out, "e_story_2x.png"))
    # single frames at x12 on mission 5's sea and mission 6's sea
    items = []
    for m in ("m5", "m6"):
        C = P.rgb12(P.MISSIONS[m]["C"])
        for im in A.aa + A.bar:
            bgc = Image.new("RGB", (im.width + 4, im.height + 4), C)
            bgc.paste(im, (2, 2), im)
            items.append(bgc.resize((bgc.width * 12, bgc.height * 12), Image.NEAREST))
    P.sheet(items[:6], 3).save(os.path.join(out, "e_aa_barrage_x12_m5.png"))
    P.sheet(items[6:], 3).save(os.path.join(out, "e_aa_barrage_x12_m6.png"))
    items = []
    for im in A.hit + A.tur[:1]:
        bgc = Image.new("RGB", (im.width + 4, im.height + 4), P.rgb12(P.MISSIONS["m6"]["C"]))
        bgc.paste(im, (2, 2), im)
        items.append(bgc.resize((bgc.width * 8, bgc.height * 8), Image.NEAREST))
    P.sheet(items, 3).save(os.path.join(out, "e_turret_hit_x8.png"))
    hl = []
    for im in A.hull:
        bgc = Image.new("RGB", (im.width + 4, im.height + 4), P.rgb12(P.MISSIONS["m6"]["C"]))
        bgc.paste(im, (2, 2), im)
        hl.append(bgc.resize((bgc.width * 5, bgc.height * 5), Image.NEAREST))
    stack(hl).save(os.path.join(out, "e_hull_x5.png"))


if __name__ == "__main__":
    build(sys.argv[1], sys.argv[2])
