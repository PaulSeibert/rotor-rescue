#!/usr/bin/env python3
"""Review pictures for the World 2 production art: per mission a 2x mock-up on the game's real layout, a scroll strip
(8 camera positions) and an asset sheet, plus the whole 4608 px map. Built from the generators (the same code that
make_world2.py writes the files from), so what you see is what is in art/world2/.

    ~/amiga-tools/venv-img/bin/python art/tools/review_world2.py [OUTDIR]     # default C:\\Users\\Public\\claude-code\\art-pilot
writes OUTDIR/coral-islands-prod-{mockup,scroll,assets}-{m4,m5,m6}.png and coral-islands-prod-world-m4.png
"""
import os
import sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "world2"))
sys.path.insert(0, HERE)
import numpy as np                                     # noqa: E402
from PIL import ImageDraw                              # noqa: E402
from w2compose import *                                # noqa: E402,F401
import w2bobs as B                                     # noqa: E402
import make_world2 as MW                               # noqa: E402

OUTDIR = next((a for a in sys.argv[1:] if not a.startswith("--")), "/mnt/c/Users/Public/claude-code/art-pilot")
TAGS = ["m4", "m5", "m6"]

F5 = {
    'A': ['01110', '10001', '10001', '11111', '10001', '10001', '10001'], 'B': ['11110', '10001', '10001', '11110', '10001', '10001', '11110'],
    'C': ['01111', '10000', '10000', '10000', '10000', '10000', '01111'], 'D': ['11110', '10001', '10001', '10001', '10001', '10001', '11110'],
    'E': ['11111', '10000', '10000', '11110', '10000', '10000', '11111'], 'F': ['11111', '10000', '10000', '11110', '10000', '10000', '10000'],
    'G': ['01111', '10000', '10000', '10011', '10001', '10001', '01111'], 'H': ['10001', '10001', '10001', '11111', '10001', '10001', '10001'],
    'I': ['11111', '00100', '00100', '00100', '00100', '00100', '11111'], 'K': ['10001', '10010', '10100', '11000', '10100', '10010', '10001'],
    'L': ['10000', '10000', '10000', '10000', '10000', '10000', '11111'], 'M': ['10001', '11011', '10101', '10101', '10001', '10001', '10001'],
    'N': ['10001', '11001', '10101', '10101', '10011', '10001', '10001'], 'O': ['01110', '10001', '10001', '10001', '10001', '10001', '01110'],
    'P': ['11110', '10001', '10001', '11110', '10000', '10000', '10000'], 'R': ['11110', '10001', '10001', '11110', '10100', '10010', '10001'],
    'S': ['01111', '10000', '10000', '01110', '00001', '00001', '11110'], 'T': ['11111', '00100', '00100', '00100', '00100', '00100', '00100'],
    'U': ['10001', '10001', '10001', '10001', '10001', '10001', '01110'], 'V': ['10001', '10001', '10001', '10001', '01010', '01010', '00100'],
    'W': ['10001', '10001', '10001', '10101', '10101', '11011', '10001'], 'Y': ['10001', '10001', '01010', '00100', '00100', '00100', '00100'],
    '0': ['01110', '10011', '10101', '10101', '10101', '11001', '01110'], '1': ['00100', '01100', '00100', '00100', '00100', '00100', '01110'],
    '2': ['01110', '10001', '00001', '00110', '01000', '10000', '11111'], '3': ['11110', '00001', '00001', '01110', '00001', '00001', '11110'],
    '4': ['10010', '10010', '10010', '11111', '00010', '00010', '00010'], '5': ['11111', '10000', '11110', '00001', '00001', '10001', '01110'],
    '6': ['01110', '10000', '11110', '10001', '10001', '10001', '01110'], '7': ['11111', '00001', '00010', '00100', '01000', '01000', '01000'],
    '8': ['01110', '10001', '10001', '01110', '10001', '10001', '01110'], '9': ['01110', '10001', '10001', '01111', '00001', '00001', '01110'],
    ' ': ['00000'] * 7,
}


def hud_text(mask, x, y, s):
    for ch in s:
        for r, row in enumerate(F5.get(ch, F5[' '])):
            for c, b in enumerate(row):
                if b == '1':
                    for dx in (0, 1):
                        mask[(x + c + dx, y + r)] = 1
        x += 8


def hud(pad_ok):
    im = Image.new("RGBA", (320, 24))
    px = im.load()
    for y in range(24):
        for x in range(320):
            c = HUD_BG[y]
            if 2 <= y <= 21 and 58 <= x < 224 and y >= 12:
                c = 0x011
            px[x, y] = rgb12(c) + (255,)
    mask = {}
    hud_text(mask, 1, 3, "SAVED 12 ABOARD 05 LOST 00 SCORE 001250")
    hud_text(mask, 1, 14, "HELI 3")
    hud_text(mask, 229, 14, "PAD")
    hud_text(mask, 270, 14, "SK 3")
    for (x, y) in mask:
        if (x + 1, y + 1) not in mask and 0 <= x + 1 < 320 and y + 1 < 24:
            px[x + 1, y + 1] = rgb12(0x111) + (255,)
    for (x, y) in mask:
        row = y - 3 if y < 12 else y - 14
        px[x, y] = rgb12(HUD_INK[max(0, min(7, row))]) + (255,)
    lamp = 0xFC3 if pad_ok else 0xD42
    for dx in range(3):
        for dy in range(3):
            px[257 + dx, 15 + dy] = rgb12(lamp) + (255,)
    for x in range(60, 222):
        px[x, 20] = rgb12(0x235) + (255,)
    for (a, b) in [(62, 82), (100, 138), (150, 178), (196, 220)]:
        for x in range(a, b):
            px[x, 20] = rgb12(0xED9) + (255,)
            px[x, 21] = rgb12(0x975) + (255,)
    for (x, c) in [(68, 0x36B), (110, 0x36B), (160, 0x36B), (208, 0xD42), (90, 0xD42), (146, 0xD42)]:
        for dy in (18, 19):
            px[x, dy] = px[x + 1, dy] = rgb12(c) + (255,)
    for x in range(120, 124):
        px[x, 16] = rgb12(0xFC3) + (255,)
    px[121, 15] = rgb12(0xFC3) + (255,)
    return im


def load_frames(name):
    """the game's own art (art/NAME.txt) as RGBA frames"""
    cols, frames, cur, mode = {}, [], None, None
    for line in open(os.path.join(ROOT, "art", name + ".txt")):
        line = line.rstrip("\n")
        if line.startswith("#") or not line.strip():
            continue
        if line.strip() == "colors":
            mode = "c"
        elif line.strip() == "frame":
            mode, cur = "f", []
            frames.append(cur)
        elif mode == "c":
            p = line.split()
            if len(p) >= 2:
                cols[p[0]] = None if p[1] == "transparent" else int(p[1], 16)
        elif mode == "f":
            cur.append(line)
    out = []
    for fr in frames:
        im = Image.new("RGBA", (max(len(r) for r in fr), len(fr)), (0, 0, 0, 0))
        for y, r in enumerate(fr):
            for x, ch in enumerate(r):
                if cols.get(ch) is not None:
                    im.putpixel((x, y), rgb12(cols[ch]) + (255,))
        out.append(im)
    return out


def label(im, text, color=(255, 255, 255)):
    im = im.copy()
    d = ImageDraw.Draw(im)
    d.text((3, 2), text, fill=color)
    return im


# ------------------------------------------------------------------------------------------------ mock-up
class Art:
    """the production frame sets as images, by name"""
    def __init__(self):
        self.sets, self.mounts = MW.bob_sets()

    def img(self, name, i):
        return self.sets[name][0][i].img()


def pad_img(art, width, lit=True):
    off = 0 if lit else 3
    cap = [art.img("pad_cap", i) for i in ((0, 1) if lit else (2, 3))]
    slab = [art.img("pad_slab", off + i) for i in range(3)]       # chevron right, chevron left, plain
    centre = art.img("pad_centre", 0 if lit else 1)
    n = (width - 32) // 8 // 2
    out = Image.new("RGBA", (width, 46), (0, 0, 0, 0))
    x = 0
    out.alpha_composite(cap[0], (x, 0))
    x += 8
    for i in range(n):
        out.alpha_composite(slab[0] if i % 2 == 0 else slab[2], (x, 30))
        x += 8
    out.alpha_composite(centre, (x, 30))
    x += 16
    for i in range(n):
        out.alpha_composite(slab[1] if (n - 1 - i) % 2 == 0 else slab[2], (x, 30))
        x += 8
    out.alpha_composite(cap[1], (x, 0))
    return out


def destroyer_img(art, facing, states=("idle", "idle")):
    """hull with its two turrets; a forward turret's barrels overhang the bow, so the picture is the union"""
    f = 1 if facing == "right" else 0
    hull = art.img("destroyer_hull", f)
    pad = 24
    out = Image.new("RGBA", (hull.width + 2 * pad, hull.height + pad), (0, 0, 0, 0))
    out.alpha_composite(hull, (pad, pad))
    ti = {"idle": 0, "tell1": 1, "tell2": 2, "fire": 3}
    for mount, st in zip(("forward", "aft"), states):
        mx, my = art.mounts[(facing, mount)]
        out.alpha_composite(art.img("destroyer_turret", f * 4 + ti[st]), (mx + pad, my + pad))
    return out.crop(out.getbbox())


def scene(W, art, mi, cam, kind):
    scr, tag = compose(W, mi, cam)
    s = scr
    ch = load_frames("chopper")
    hostage, shadow, shell = load_frames("hostage"), load_frames("shadow"), load_frames("shell")

    def blit(spr, wx, y):
        s.alpha_composite(spr, (int(wx - cam), int(y)))

    def sea(spr, wx, bottom):
        blit(spr, wx, bottom - spr.height)
    if kind == "m4":                                       # barracks, pad, a right-facing gunboat telling at the chopper
        blit(art.img("barracks", 0), 132, 212)
        blit(art.img("windsock", 1), 308, 220)
        blit(pad_img(art, 64), 240, 198)
        for x, fr in [(196, 2), (206, 3), (216, 1), (226, 5)]:
            blit(hostage[fr % len(hostage)], x, 229)
        sea(art.img("gunboat", 6), 4 + cam, 243)
        blit(art.img("splash", 2), 74 + cam, 222)
        blit(shell[0], 66 + cam, 205)
        blit(art.img("pad_lamp", 1), 262, 238)
    elif kind == "m5":                                     # radar site, open barracks, left-facing patrol boat and right-facing one
        blit(art.img("barracks", 1), 132, 212)
        blit(art.img("radar", 1), 196, 205)
        blit(art.img("windsock", 0), 308, 220)
        blit(pad_img(art, 48), 262, 198)
        sea(art.img("patrol_boat", 0), 20 + cam, 241)
        sea(art.img("patrol_boat", 3), 60 + cam, 249)
        sea(art.img("bow_spray", 1), 28 + cam, 246)
        blit(art.img("pad_lamp", 0), 236, 238)
    else:                                                  # the destroyer telling, whitecaps, a burning barracks
        blit(art.img("barracks", 2), 176, 212)
        blit(pad_img(art, 64, lit=False), 254, 198)
        sea(destroyer_img(art, "right", ("tell1", "tell2")), -14 + cam, 247)
        for i, (x, y) in enumerate(((150, 190), (232, 196), (120, 200))):
            blit(art.img("whitecap", i), x + cam, y)
        blit(art.img("windsock", 1), 308, 220)
    cx = 246
    blit(shadow[2 % len(shadow)], 258, 239)
    s.alpha_composite(ch[2 % len(ch)].transpose(Image.FLIP_LEFT_RIGHT), (cx, 200))
    s.alpha_composite(hud(kind != "m6"), (0, 0))
    return s


# ------------------------------------------------------------------------------------------------ sheet
def row_img(imgs, k, bg, pad=6):
    h = max(i.height for i in imgs) * k
    w = sum(i.width * k + pad for i in imgs) + pad
    o = Image.new("RGBA", (w, h + 2 * pad), (28, 30, 38, 255))
    x = pad
    for i in imgs:
        z = zoom(i, k)
        o.alpha_composite(z, (x, pad + (h - z.height)))
        x += z.width + pad
    return o


def sheet(W, art, mi):
    m = MISSIONS[mi]
    BG = rgb12(m["C"]) + (255,)

    def on(im, bg=BG):
        b = Image.new("RGBA", im.size, bg)
        b.alpha_composite(im)
        return b
    sections = []
    names = list(W.bank.keys())
    tiles = [on(pens_to_img(W.bank[n], m["isl"])) for n in names]
    sections.append(("TERRAIN TILES 16 x 48, %d columns (island pens on the deep sea colour), 3x" % len(tiles),
                     [row_img(tiles[i:i + 12], 3, BG, 4) for i in range(0, len(tiles), 12)]))
    rep = []
    for nm in ("water0", "flat0"):
        t = pens_to_img(W.bank[nm], m["isl"])
        big = Image.new("RGBA", (48, 144))
        for j in range(3):
            for i in range(3):
                big.paste(t, (i * 16, j * 48))
        rep.append(on(big))
    seq = ["shoreL0_0", "shoreL0_1", "flat0", "palmA_0", "palmA_1", "flat1", "rock0_0", "rock0_1", "padbed0", "padbed1", "bush0", "flat2",
           "palmC_0", "palmC_1", "shoreR0_0", "shoreR0_1"]
    run = on(pens_to_img(np.concatenate([W.bank[n] for n in seq], axis=1), m["isl"]))
    sections.append(("3 x 3 REPEAT (water, flat) and an island run, 3x", [row_img(rep, 3, BG, 6), row_img([run], 3, BG, 4)]))
    hz = rgb12(m["sky"][1]) + (255,)
    sky = rgb12(m["sky"][0]) + (255,)
    far2 = Image.new("RGBA", (640, 60), hz)
    clo2 = Image.new("RGBA", (640, 40), rgb12(m["sky"][0]) + (255,))
    for i in range(2):
        far2.alpha_composite(pens_to_img(W.far, m["far"]), (i * 320, 0))
        clo2.alpha_composite(pens_to_img(W.clouds, m["clouds"]), (i * 320, 0))
    fl = Image.new("RGBA", (640, 28), BG)
    for j in range(28):
        row = W.floor[j]
        for x in range(640):
            v = row[x % len(row)]
            if v:
                fl.putpixel((x, j), rgb12(m["sea"][v - 1]) + (255,))
    sections.append(("BANDS looped twice (the join is at x = 320): clouds 320 x 40 | far islands + strip 320 x 60 | sea floor 640 x 28, 2x",
                     [row_img([clo2], 2, BG), row_img([far2], 2, BG), row_img([fl], 2, BG)]))
    pads = [pad_img(art, w) for w in (48, 64, 96)] + [pad_img(art, 64, lit=False), art.img("pad_lamp", 0), art.img("pad_lamp", 1)]
    sections.append(("PADS 48 / 64 / 96 px, the dark blink phase of 64, lamps red / yellow, 3x", [row_img([on(p) for p in pads], 3, BG, 8)]))
    b = [art.img("barracks", i) for i in range(4)] + [art.img("radar", i) for i in range(4)] + [art.img("windsock", i) for i in range(2)]
    sections.append(("BARRACKS intact / shot up / burning x2 | RADAR 4 frames | WINDSOCK limp / full, 3x", [row_img([on(x) for x in b], 3, BG, 8)]))
    pb = [art.img("patrol_boat", i) for i in range(4)]
    sections.append(("PATROL BOAT: left-facing wake 0 / 1, right-facing wake 0 / 1 (right is re-lit, light stays top left), 5x", [row_img([on(x) for x in pb], 5, BG, 8)]))
    gl = [art.img("gunboat", i) for i in range(4)]
    gr = [art.img("gunboat", 4 + i) for i in range(4)]
    sections.append(("GUNBOAT left-facing: idle / tell 1 / tell 2 (flash) / fire; then right-facing, 4x", [row_img([on(x) for x in gl], 4, BG, 8), row_img([on(x) for x in gr], 4, BG, 8)]))
    dl = destroyer_img(art, "left")
    dr = destroyer_img(art, "right")
    sections.append(("DESTROYER 154 x 44 with turrets, left- and right-facing, 3x", [row_img([on(dl)], 3, BG, 8), row_img([on(dr)], 3, BG, 8)]))
    tl = [art.img("destroyer_turret", i) for i in range(4)]
    tr = [art.img("destroyer_turret", 4 + i) for i in range(4)]
    sections.append(("TURRET idle / tell 1 / tell 2 / fire, left then right, 5x", [row_img([on(x) for x in tl + tr], 5, BG, 8)]))
    fx = [art.img("splash", i) for i in range(5)] + [art.img("bow_spray", i) for i in range(2)] + [art.img("whitecap", i) for i in range(3)]
    sections.append(("EFFECTS: splash 5 | bow spray 2 | whitecap 3 (storm), 4x", [row_img([on(x) for x in fx], 4, BG, 8)]))
    sw = []
    for key in ("clouds", "far", "sea", "isl"):
        cell = Image.new("RGBA", (7 * 12, 12))
        for i, c in enumerate(m[key]):
            cell.paste(rgb12(c) + (255,), (i * 12, 0, i * 12 + 12, 12))
        sw.append(cell)
    for c in (m["sky"][0], m["sky"][1], m["C"]):
        sw.append(Image.new("RGBA", (12, 12), rgb12(c) + (255,)))
    sections.append(("PALETTE %s (%s): clouds | far | sea | island | sky top, sky low = haze H, deep sea C, 3x" % (m["name"], m["tag"]), [row_img(sw, 3, BG, 6)]))
    imgs = []
    for title, rows in sections:
        t = label(Image.new("RGBA", (1500, 16), (28, 30, 38, 255)), title, (255, 220, 140))
        imgs += [t] + rows
    Wd = max(i.width for i in imgs)
    H = sum(i.height + 2 for i in imgs)
    out = Image.new("RGBA", (Wd, H), (28, 30, 38, 255))
    y = 0
    for i in imgs:
        out.alpha_composite(i, (0, y))
        y += i.height + 2
    return out.convert("RGB")


def main():
    os.makedirs(OUTDIR, exist_ok=True)
    W = World()
    art = Art()
    for mi, tg in enumerate(TAGS):
        m = scene(W, art, mi, 0, tg)
        zoom(m, 2).convert("RGB").save(os.path.join(OUTDIR, "coral-islands-prod-mockup-%s.png" % tg))
        cams = [0, 220, 560, 1010, 1500, 2150, 2900, 3700]
        frames = []
        for c in cams:
            im, _ = compose(W, mi, c, wisps=True)
            frames.append(label(im.crop((0, 24, 320, 256)).convert("RGBA"), "%s CAM %d" % (MISSIONS[mi]["name"], c)))
        pad = 6
        o = Image.new("RGB", (2 * 320 + 3 * pad, 4 * 232 + 5 * pad), (20, 20, 24))
        for i, f in enumerate(frames):
            o.paste(f.convert("RGB"), (pad + (i % 2) * (320 + pad), pad + (i // 2) * (232 + pad)))
        o.save(os.path.join(OUTDIR, "coral-islands-prod-scroll-%s.png" % tg))
        sheet(W, art, mi).save(os.path.join(OUTDIR, "coral-islands-prod-assets-%s.png" % tg))
    # the whole map, 1152 px a row (the strip as the player scrolls through it)
    strip = pens_to_img(W.strip, MISSIONS[0]["isl"])
    full = Image.new("RGBA", (4608, 48), rgb12(MISSIONS[0]["C"]) + (255,))
    full.alpha_composite(strip)
    o = Image.new("RGB", (1152, 4 * 54), (20, 20, 24))
    for r in range(4):
        o.paste(full.crop((r * 1152, 0, (r + 1) * 1152, 48)).convert("RGB"), (0, r * 54 + 3))
    o.save(os.path.join(OUTDIR, "coral-islands-prod-world-m4.png"))
    print("wrote review pictures to", OUTDIR)


if __name__ == "__main__":
    main()
