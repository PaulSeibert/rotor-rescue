#!/usr/bin/env python3
"""ROTOR RESCUE's art (by Paul Ross Seibert), drawn as text art into art/*.txt:
the rescue chopper (an attached 15-colour hardware sprite), every BOB on
playfield 1 and the three parallax bands of playfield 2 (the title logo is
art/tools/make_title_logo.py).
Deterministic: the same script always draws the same pixels.

    python3 art/tools/draw.py
"""
import math
import os
import random

here = os.path.dirname(os.path.abspath(__file__))
art = os.path.join(here, '..')

# ---------------------------------------------------------------- helpers

def grid(w, h, c='.'):
    return [[c] * w for _ in range(h)]


def from_rows(rows, w=None):
    w = w or max(len(r) for r in rows)
    assert all(len(r) <= w for r in rows), [len(r) for r in rows]
    if len(set(len(r) for r in rows)) > 1: print('ragged rows', [len(r) for r in rows])
    return [list(r.ljust(w, '.')) for r in rows]


def paste(dst, src, x0, y0, transparent='.'):
    for y, row in enumerate(src):
        for x, c in enumerate(row):
            if c != transparent and 0 <= y + y0 < len(dst) and 0 <= x + x0 < len(dst[0]):
                dst[y + y0][x + x0] = c


def mirror(g):
    return [r[::-1] for r in g]


def outline(g, edge='K', keep='.K'):
    h, w = len(g), len(g[0])
    out = [r[:] for r in g]
    for y in range(h):
        for x in range(w):
            if g[y][x] != '.':
                continue
            if any(0 <= y + dy < h and 0 <= x + dx < w and g[y + dy][x + dx] not in keep
                   for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                out[y][x] = edge
    return out


def save(name, comment, colors, frames):
    h, w = len(frames[0]), len(frames[0][0])
    with open(os.path.join(art, name + '.txt'), 'w') as f:
        f.write(''.join(f'# {line}\n' for line in comment.split('\n')))
        f.write('# (drawn by art/tools/draw.py - edit that, not this)\n')
        f.write('colors\n')
        f.write('  .  transparent\n')
        for k, v in colors:
            f.write(f'  {k}  {v}\n')
        for fr in frames:
            assert len(fr) == h and all(len(r) == w for r in fr), name
            f.write('frame\n' + '\n'.join(''.join(r) for r in fr) + '\n')
    print(f'{name}: {w}x{h} x{len(frames)}')


# Playfield 1 palette (colours 1-7), shared by every BOB
PF1 = [
    ('K', '0x111'),   # 1 outline
    ('O', '0x564'),   # 2 olive drab
    ('S', '0x998'),   # 3 stone
    ('W', '0xFEC'),   # 4 cream white
    ('R', '0xD42'),   # 5 red
    ('Y', '0xFC3'),   # 6 yellow
    ('B', '0x36B'),   # 7 blue
]

# ---------------------------------------------------------------- the chopper
# 32 x 20, skids on row 16. Side view faces right; mirror = true in art.toml
# makes the left-facing frames.

# Colour registers 17-27 in this order; 28-31 stay free: channel 6 (unattached)
# shows 29-31 - the sky's wisps and stars, set per mission (main.c skyUpdate).
CHOP_COLORS = [
    ('K', '0x111'), ('D', '0x445'), ('M', '0x778'), ('L', '0xAAB'), ('W', '0xEEE'),
    ('R', '0xD22'), ('r', '0x811'), ('B', '0x38D'), ('G', '0xBEF'), ('O', '0xF70'),
    ('Y', '0xFE4'),
]

SIDE = [
    "................................",
    "................................",
    "ROTOR...........................",   # row 2: main rotor (filled in below)
    "................KMMK............",
    "...............KMLLMK...........",
    "..K..........KKKKWWWWKKKK.......",
    ".KMK.......KKWWWWWWWWWWWKBBK....",
    ".KLK.....KKWWWWWWWWWWWWWKBBBBK..",
    "KMLMKKKKKWWWWWWWWWWWWWWWKBBBGBK.",
    "KLWLLLLLLWWWWWWWWWWWWWWWWKBBBGBK",
    "KMLMMMMMMMRRRRRRRRRRRRRRRRKKKKWK",
    ".KMK..KKKKrRRRRRRRRRRRRRRRRRRRWK",
    "..K.......KLLLLLLLLLLLLLLLLLLLK.",
    "...........KKKKKKKKKKKKKKKKKKK..",
    "..............K.......K.........",
    "..............K.......K......K..",
    "..........DDDDDDDDDDDDDDDDDDDDK.",
    "................................",
    "................................",
    "................................",
]
FRONT = [
    "................................",
    "................................",
    "ROTOR...........................",
    "...............KK...............",
    "..............KMMK..............",
    "...........KKKWWWWKKK...........",
    "..........KWBBBBBBBBWK..........",
    ".........KWBBBBGGBBBBWK.........",
    ".........KWBBBBBBBBBBWK.........",
    ".........KWWWWWWWWWWWWK.........",
    ".........KRRRRRRRRRRRRK.........",
    ".........KrRRRRRRRRRRrK.........",
    "..........KLLLLLLLLLLK..........",
    "...........KKKKKKKKKK...........",
    "..........K..........K..........",
    ".........K............K.........",
    "........DDDD........DDDD........",
    "................................",
    "................................",
    "................................",
]
TURN = [
    "................................",
    "................................",
    "ROTOR...........................",
    "...............KK...............",
    "..............KMMK..............",
    "...........KKKKWWWWKKK..........",
    ".K.......KKWWWWWWWKBBBK.........",
    "KMK....KKWWWWWWWWKBBBBBK........",
    "KLKKKKKWWWWWWWWWWKBBGBBK........",
    "KMLLLLLWWWWWWWWWWKBBBBBK........",
    ".KK...KRRRRRRRRRRRRRRRRK........",
    "......KrRRRRRRRRRRRRRRK.........",
    ".......KLLLLLLLLLLLLLK..........",
    "........KKKKKKKKKKKKK...........",
    "..........K......K..............",
    ".........K......K...............",
    "......DDDDDDDDDDDDDDD...........",
    "................................",
    "................................",
    "................................",
]


def rotor(g, phase, x0=1, x1=31, row=2):
    g = [r[:] for r in g]
    g[row] = ['.'] * 32
    if phase == 0:
        for x in range(x0, x1):
            g[row][x] = 'D'
        g[row][16] = 'K'
        g[row][15] = 'K'
    else:
        mid = (x0 + x1) // 2
        half = (x1 - x0) // 3
        for x in range(mid - half, mid + half):
            g[row][x] = 'M'
        g[row][15] = 'K'
        g[row][16] = 'K'
    return g


def tail_rotor(g, phase):
    # rows 5..11 at x 0..3: vertical blur or a cross
    g = [r[:] for r in g]
    if phase == 1:
        g[6][1] = 'D'
        g[10][1] = 'D'
        g[8][0] = 'M'
    return g


def shear(g, k, pivot=16):
    """Tilt nose-down by k px per px (right side lower)."""
    h, w = len(g), len(g[0])
    out = grid(w, h)
    for y in range(h):
        for x in range(w):
            c = g[y][x]
            if c == '.':
                continue
            ny = y + int(round((x - pivot) * k))
            if 0 <= ny < h:
                out[ny][x] = c
    return out


def chopper():
    side = from_rows(SIDE)
    front = from_rows(FRONT)
    turn = from_rows(TURN)
    frames = []
    for pose in range(6):
        for phase in (0, 1):
            if pose < 3:
                g = rotor(side, phase)
                g = tail_rotor(g, phase)
                g = shear(g, (0, 0.09, 0.17)[pose])
            elif pose == 3:
                g = rotor(turn, phase, 3, 29)
            elif pose == 4:
                g = rotor(front, phase, 0, 32)
            else:
                # the wreck: nose down, dark, burning, a stub of rotor
                g = [r[:] for r in side]
                g[2] = list('.' * 12 + 'MMMMKKMMM' + '.' * 11)
                g = shear(g, 0.28)
                rnd = random.Random(7 + phase)
                dark = {'W': 'L', 'R': 'r', 'B': 'D', 'G': 'M', 'L': 'M'}
                g = [[dark.get(c, c) for c in r] for r in g]
                for _ in range(26):
                    x = rnd.randrange(6, 28)
                    col = [y for y in range(20) if g[y][x] != '.']
                    if not col:
                        continue
                    y = col[0] - rnd.randrange(0, 4)
                    if 0 <= y < 20:
                        g[y][x] = rnd.choice('OOY')
            frames.append(g)
    save('chopper', 'The rescue chopper: 32x20, 15 colours (attached sprites, channels 0-3).\n'
         'Frames: pose * 2 + rotor phase; poses side (level, tilt, full tilt), turn,\n'
         'front, wreck. Faces right; art.toml mirror = true adds the left-facing set.',
         CHOP_COLORS, frames)


# ---------------------------------------------------------------- BOBs

def chk(x, y, phase=0):
    """A checkerboard: the 50% dither between two colours."""
    return (x + y + phase) & 1 == 0


def hangar():
    """Corrugated walls lit from the left (the low sun), a curved roof shading
    from a bright crown to dark eaves, the shadow of the eave on the wall, the
    open door with its frame and a lamp, and the red cross on a white board."""
    w, h = 64, 40
    g = grid(w, h)
    # walls: corrugated stone, every 4th column a dark groove, the far right in shade
    for y in range(13, 40):
        for x in range(2, 62):
            c = 'S'
            if x % 4 == 3 or (x >= 56 and x % 2 == 1):
                c = 'O'                                 # grooves, closer at the shaded end
            g[y][x] = c
    # the eave's shadow on the wall, fading
    for x in range(2, 62):
        g[13][x] = 'K'
        g[14][x] = 'K' if chk(x, 14) else g[14][x]
    # the roof: a half-round, bright crown to dark eaves
    for x in range(0, 64):
        dx = (x - 32) / 32.0
        top = int(round(10 - 9 * math.sqrt(max(0, 1 - dx * dx))))
        for y in range(max(0, top), 13):
            d = y - top                                 # depth under the roof line
            side = x / 63.0                             # 0 lit (left) .. 1 shade (right)
            if d <= 1 and side < 0.75:
                c = 'W' if d == 0 or chk(x, y) else 'B'  # the sunlit crown
            elif y >= 11:
                c = 'K' if chk(x, y) else 'B'           # dark eaves
            elif side > 0.7 and chk(x, y):
                c = 'K'                                 # the far side in shade
            else:
                c = 'B'
            g[y][x] = c
        if 0 <= top < h:
            g[top][x] = 'K'
    for x in range(0, 64):
        g[12][x] = 'K'
    # ribs over the roof, catching the light on the lit side
    for x in range(6, 60, 8):
        for y in range(2, 12):
            if g[y][x] in 'BK' and g[y - 1][x] != '.':
                g[y][x] = 'W' if x < 40 else ('S' if chk(x, y) else 'B')
    # big open door, people run in here: a frame, a dark inside with a floor
    for y in range(18, 40):
        for x in range(8, 30):
            g[y][x] = 'K'
    for y in range(34, 39):
        for x in range(9, 29):
            if chk(x, y) and y > 35:
                g[y][x] = 'O'                           # light on the floor inside
    for y in range(17, 40):
        g[y][7] = 'W'
        g[y][30] = 'S'
    for x in range(7, 31):
        g[17][x] = 'W'
    g[16][18] = 'Y'                                     # the lamp over the door
    g[16][19] = 'Y'
    g[15][18] = 'K'
    g[15][19] = 'K'
    # a red cross on a white board, with a dark edge
    for y in range(20, 32):
        for x in range(42, 54):
            g[y][x] = 'W'
    for y in range(20, 32):
        g[y][53] = 'S'
    for x in range(42, 54):
        g[31][x] = 'S'
    for y in range(22, 30):
        for x in range(46, 50):
            g[y][x] = 'R'
    for y in range(24, 28):
        for x in range(44, 52):
            g[y][x] = 'R'
    for y in range(13, 40):
        g[y][1] = 'K'
        g[y][62] = 'K'
    for x in range(1, 63):
        g[39][x] = 'K'
    save('hangar', 'Home base hangar, 64x40.', PF1, [g])


def fence():
    g = grid(16, 32)
    for y in range(4, 32):
        g[y][2] = 'S'
        g[y][13] = 'S'
        g[y][1] = 'K'
        g[y][14] = 'K'
    for y in range(6, 30, 4):
        for x in range(3, 13):
            g[y][x] = 'K'
        for x in range(3, 13, 2):
            g[y + 2][x] = 'K'
    for x in range(0, 16, 3):
        g[3][x] = 'K'
        g[2][x + 1 if x + 1 < 16 else x] = 'K'
    # a striped border marker
    for y in range(12, 20):
        for x in range(4, 12):
            g[y][x] = 'R' if ((x + y) // 2) % 2 else 'W'
    save('fence', 'The border fence, 16x32.', PF1, [g])


def barracks():
    frames = []
    w, h = 48, 28
    for f in range(4):
        rnd = random.Random(100 + f)
        g = grid(w, h)
        roof_top = 4
        for y in range(roof_top, 12):
            inset = 12 - y
            for x in range(inset, w - inset):
                # olive roof: a lit ridge, shading to the eaves, the right end darker
                if y <= roof_top + 1:
                    c = 'S' if chk(x, y) else 'O'
                elif y >= 10:
                    c = 'K' if chk(x, y) else 'O'
                elif x > 36 and chk(x, y):
                    c = 'K'
                else:
                    c = 'O'
                g[y][x] = c
            g[y][inset] = 'K'
            g[y][w - inset - 1] = 'K'
        for x in range(8, 40):
            g[roof_top][x] = 'K'
        for y in range(12, 28):
            for x in range(2, 46):
                # plank walls: a dark seam every 4 rows, shade under the eave
                c = 'S'
                if y % 4 == 1 and chk(x, y):
                    c = 'O'
                if y == 13 or (y == 14 and chk(x, y)):
                    c = 'K'
                g[y][x] = c
            g[y][1] = 'K'
            g[y][46] = 'K'
        for x in range(0, 48):
            g[12][x] = 'K'
            g[27][x] = 'K'
        # door (a frame, a dark inside) and barred windows with sills
        for y in range(16, 27):
            for x in range(20, 28):
                g[y][x] = 'K'
        for y in range(16, 27):
            g[y][19] = 'W'
            g[y][28] = 'O'
        for x in range(19, 29):
            g[15][x] = 'W'
        for wx in (6, 34):
            for y in range(15, 21):
                for x in range(wx, wx + 8):
                    lit = 'Y' if y < 18 or chk(x, y) else 'R'   # lamplight, warmer low down
                    g[y][x] = lit if f < 2 else 'K'
            for x in range(wx, wx + 8, 2):
                for y in range(15, 21):
                    g[y][x] = 'K'
            for x in range(wx - 1, wx + 9):
                g[21][x] = 'W'
                g[14][x] = 'K'
        # sandbags along the front
        for bx in list(range(2, 18, 4)) + list(range(30, 46, 4)):
            for x in range(bx, min(bx + 4, 46)):
                g[24][x] = 'W' if bx < x < bx + 3 else 'S'
                g[25][x] = 'W' if x < bx + 3 else 'K'
                g[26][x] = 'S' if x < bx + 3 else 'K'
        if f == 1:
            # shot up: holes ringed with scorch
            for _ in range(10):
                x, y = rnd.randrange(4, 44), rnd.randrange(6, 24)
                if g[y][x] in 'SO':
                    g[y][x] = 'K'
                    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        if g[y + dy][x + dx] in 'SOW' and rnd.random() < 0.6:
                            g[y + dy][x + dx] = 'K' if chk(x + dx, y + dy) else 'O'
        if f >= 2:
            # burst open: roof blown, flames from the wreck, smoke over them
            for y in range(0, 13):
                for x in range(0, 48):
                    g[y][x] = '.'
            for x in range(2, 46):
                g[12][x] = 'K'
            ph = (f - 2) * 2.1
            for x in range(2, 46):
                hgt = 5 + 3.5 * math.sin(x * 0.55 + ph) + 2.0 * math.sin(x * 1.3 - ph * 1.7)
                hgt = int(max(1, min(11, hgt + rnd.uniform(-1, 1))))
                for y in range(12 - hgt, 12):
                    d = 12 - y                          # height above the wall top
                    t = d / float(hgt)
                    c = 'W' if t < 0.25 else 'Y' if t < 0.55 else 'R'
                    if t > 0.8 and chk(x, y, f):
                        c = 'K'                         # sooty tips
                    g[y][x] = c
                # smoke drifting above the tallest tongues
                top = 12 - hgt
                for y in range(max(0, top - 3), top):
                    if chk(x, y, f) and rnd.random() < 0.55:
                        g[y][x] = 'K'
            for y in range(13, 27):
                for x in range(16, 32):
                    g[y][x] = 'K'
            for y in range(20, 27):
                for x in range(17, 31):
                    if chk(x, y, f) and rnd.random() < 0.5:
                        g[y][x] = 'R'                   # embers glowing inside
        frames.append(g)
    save('barracks', 'Hostage barracks, 48x28: intact, shot up, burning open (2 flicker frames).', PF1, frames)


def tank():
    # lit from above-left: a pale top edge on the turret and hull, olive
    # bodies shading to dark low down, a red star, wheels with bright hubs
    base = from_rows([
        "................................",
        "..............KKKKK.............",
        ".............KSSSOOK............",
        "............KSOOROOOKKKKKKKKKK..",
        "............KOOOOOOKSSSSSSSSWK..",
        "........KKKKKKKKKKKKKKKKKKK.....",
        ".......KSSSSSSSSSSSSSOOOOOOK....",
        "......KOOOOOOOOOOOOOOOOOOOOOK...",
        ".....KKKKKKKKKKKKKKKKKKKKKKKKK..",
        ".....KSKSKSKSKSKSKSKSKSKSKSKK...",
        "......KSWKSWKSWKSWKSWKSWKSWK....",
        ".......KKKKKKKKKKKKKKKKKKKK.....",
        "................................",
        "................................",
    ])
    for x in range(7, 28):                              # the hull's lower half in shade
        if base[7][x] == 'O' and chk(x, 7):
            base[7][x] = 'K' if x > 20 else 'O'
    alt = [r[:] for r in base]
    alt[9] = list(".....KKSKSKSKSKSKSKSKSKSKSKSK...")
    alt[10] = list("......KWSKWSKWSKWSKWSKWSKWSK....")   # the wheels turn
    fire = [r[:] for r in base]
    for x in range(30, 32):
        fire[3][x] = 'Y'
        fire[4][x] = 'Y'
    fire[2][31] = 'W'
    # The tell (the wind-up before a shell): the barrel swings up from level to
    # ~20 degrees and a flash blinks on the turret - the star turns yellow and
    # the muzzle glints white.
    aim = [r[:] for r in base]
    for y in (3, 4):
        for x in range(20, 30):
            aim[y][x] = '.'
    aim[3][19] = 'O'
    aim[4][19] = 'K'
    barrel = []
    for i in range(10):                                  # a 2-px barrel, rising 1 in 3
        x, y = 20 + i, 3 - (i + 1) // 3
        barrel += [(x, y, 'S' if i < 9 else 'W'), (x, y + 1, 'O' if i < 9 else 'S')]
    for x, y, c in barrel:
        aim[y][x] = c
    for x, y, c in barrel:                               # its outline, where there's nothing
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            if 0 <= y + dy < 14 and 0 <= x + dx < 32 and aim[y + dy][x + dx] == '.':
                aim[y + dy][x + dx] = 'K'
    flash = [r[:] for r in aim]
    flash[3][16] = 'Y'                                   # the star lights up
    flash[0][30] = 'W'                                   # the muzzle glints
    flash[0][31] = 'Y'
    flash[1][30] = 'Y'
    flash[1][31] = 'W'
    # right-facing are drawn; left = mirror
    frames = [mirror(base), mirror(alt), base, alt, mirror(fire), fire,
              mirror(aim), aim, mirror(flash), flash]
    save('tank', 'Enemy tank, 32x14: left x2, right x2, firing left, firing right,\n'
         'the tell (barrel up) left, right, the tell flashing left, right.', PF1, frames)


def hostage():
    stand = [
        "......WW........",
        "......WW........",
        ".......B........",
        "......BBB.......",
        ".....B.B.B......",
        ".....W.B.W......",
        "......BBB.......",
        "......K.K.......",
        "......K.K.......",
        "......K.K.......",
        "......K.K.......",
        ".....KK.KK......",
    ]
    run0 = [
        ".......WW.......",
        ".......WW.......",
        "......BB........",
        ".....BBBB.......",
        "....W.BB.W......",
        "......BB........",
        "......BBB.......",
        ".....K..K.......",
        "....K....K......",
        "....K.....K.....",
        "...K......K.....",
        "...K.......K....",
    ]
    run1 = [
        ".......WW.......",
        ".......WW.......",
        "......BB........",
        "......BBB.......",
        ".....BBB.W......",
        ".....WBB........",
        "......BBB.......",
        "......KK........",
        "......K.K.......",
        "......K.K.......",
        ".....K..K.......",
        ".....K...K......",
    ]
    wave0 = [
        "....W.WW.W......",
        "....W.WW.W......",
        ".....BBBB.......",
        "......BB........",
        "......BB........",
        "......BB........",
        "......BBB.......",
        "......K.K.......",
        "......K.K.......",
        "......K.K.......",
        "......K.K.......",
        ".....KK.KK......",
    ]
    wave1 = [
        "......WW........",
        "...W..WW..W.....",
        "....BBBBBB......",
        "......BB........",
        "......BB........",
        "......BB........",
        "......BBB.......",
        "......K.K.......",
        "......K.K.......",
        "......K.K.......",
        "......K.K.......",
        ".....KK.KK......",
    ]
    dead = [
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "......R.........",
        "...WWBBBBKKK....",
        "..RRRRRRRR......",
    ]
    fr = [from_rows(x) for x in (stand, run0, run1, wave0, wave1, dead)]
    for f in (1, 2):                                    # running: hair at the back of the head
        fr[f][0][7] = 'K'
    fr.append(mirror(fr[1]))
    fr.append(mirror(fr[2]))
    save('hostage', 'A hostage, 16x12: stand, run right x2, wave x2, down, run left x2.', PF1, fr)


def jet():
    right = from_rows([
        "................................",
        "...KK...........................",
        "...KSK..........................",
        "...KSSK.............KKKKK.......",
        "....KSSKKKKKKKKKKKKKSBWBBKK.....",
        "RY..KSWWWWSSSSSSSSSSSSSSSSSKK...",
        "RYWKOOOOOOOOOOOOOOOOOOOOOOOOSKK.",
        "RY..KSSSSSSSRRSSSSSSSSSSSSSKK...",
        ".....KKKKKKKSSSSKKKKKKKKKKK.....",
        "...........KSSSK................",
        "...........KKKK.................",
        "................................",
    ])
    for x in range(14, 27):                             # the belly in shade
        if right[7][x] == 'S' and chk(x, 7):
            right[7][x] = 'O'
    save('jet', 'Enemy jet, 32x12: left, right.', PF1, [mirror(right), right])


def mine():
    frames = []
    for f in range(2):
        g = grid(16, 12)
        for y in range(12):
            for x in range(16):
                dx, dy = x - 7.5, y - 5.5
                if dx * dx + dy * dy <= 16:
                    g[y][x] = 'S'
                if dx * dx + dy * dy <= 4:
                    g[y][x] = 'R' if f == 0 else 'Y'
        for (x, y) in ((7, 0), (8, 0), (7, 11), (8, 11), (2, 5), (2, 6), (13, 5), (13, 6),
                       (4, 2), (11, 2), (4, 9), (11, 9)):
            g[y][x] = 'K'
        frames.append(outline(g))
    save('mine', 'Floating air mine, 16x12: blinking.', PF1, frames)


def small():
    shot = from_rows(["YYYYWW..........", "YWWWWWWW........", "YYYYWW.........."])
    save('shot', 'Cannon tracer / missile, 16x3.', PF1, [shot])
    bomb = from_rows([".KK.............", "KSSK............", "KSSK............", "KSSK............",
                      ".KK.............", "..Y............."])
    save('bomb', 'Falling bomb, 16x6.', PF1, [bomb])
    shell = from_rows([".YY.............", "YWWY............", "YWWY............", ".YY............."])
    save('shell', 'Enemy shell, 16x4.', PF1, [shell])


def boom():
    """A flash, a billowing fireball (hot core, ragged red rim), then smoke that
    rises and thins: shapes from smooth noise, colour bands dithered into each
    other so the 7-colour palette reads as a gradient."""
    rnd = random.Random(40)
    # a few lobes around the centre: the billows (angle, strength)
    lobes = [(rnd.uniform(0, 2 * math.pi), rnd.uniform(0.05, 0.11)) for _ in range(6)]

    def edge(ang, f):
        e = 1.0
        for i, (a, s) in enumerate(lobes):
            e += s * math.cos(ang * (i + 3) - a + f * 0.7)
        return e

    frames = []
    for f in range(5):
        g = grid(32, 32)
        ember = random.Random(77 + f)
        r = (4.5, 8.0, 11.0, 12.0, 12.5)[f]
        cy = 15.5 - (0, 0, 0.5, 2.0, 3.5)[f]           # smoke rises
        for y in range(32):
            for x in range(32):
                dx, dy = x - 15.5, (y - cy) * 1.1
                d = math.sqrt(dx * dx + dy * dy)
                ang = math.atan2(dy, dx)
                rr = min(r * edge(ang, f), 14.5)
                if d >= rr:
                    continue
                t = d / rr                              # 0 centre .. 1 rim
                k = BAYER4[y & 3][x & 3] / 16.0
                if f == 0:
                    c = 'W' if t < 0.55 + k * 0.3 else 'Y'
                elif f == 1:
                    c = 'W' if t < 0.3 + k * 0.2 else 'Y' if t < 0.6 + k * 0.2 else 'R'
                elif f == 2:
                    c = 'Y' if t < 0.25 + k * 0.2 else 'R' if t < 0.6 + k * 0.25 else \
                        ('K' if k < 0.5 else 'R')
                elif f == 3:
                    # smoke, embers still glowing in it
                    c = 'S' if t < 0.45 + k * 0.3 else 'K'
                    if t < 0.4 and ember.random() < 0.12:
                        c = 'R' if ember.random() < 0.7 else 'Y'
                    if t > 0.85 and k < 0.4:
                        c = '.'
                else:
                    # thinning smoke: dithered away from the rim inwards
                    if k * 0.9 < t - 0.25:
                        continue
                    c = 'S' if k > 0.5 else 'K'
                g[y][x] = c
        frames.append(g)
    save('boom', 'Explosion, 32x32, 5 frames: flash, fireball x2, smoke x2.', PF1, frames)


def shadow():
    """The chopper's shadow on the track, 32x4: 4 sizes by altitude (on the
    ground .. high up), each in 2 dither phases so the pattern stays put on the
    screen whatever x it's drawn at (logic.c picks frame = size * 2 + (x & 1)).
    Colour 1 (the outline black) dithered over the ground: a soft, see-through
    shadow - a solid core low down, thinner and sparser the higher it flies."""
    # per size: (half width of the core rows, half width of the rim rows, core density)
    sizes = [(13, 9, 3), (11, 7, 2), (8, 5, 2), (5, 3, 1)]
    frames = []
    for half, rim, dens in sizes:
        for phase in (0, 1):
            g = grid(32, 4)
            for y in range(4):
                hw = rim if y in (0, 3) else half
                for x in range(16 - hw, 16 + hw):
                    edge = min(x - (16 - hw), 16 + hw - 1 - x)   # 0 at the ends
                    d = dens if y in (1, 2) else min(dens, 2)
                    if edge < 2:
                        d = min(d, 3 if edge else 2)              # ragged, fading ends
                    # 4 = solid, 3 = a solid row over a checker row, 2 = checker,
                    # 1 = a dotted line (only odd/even x patterns: 2 phases cover them)
                    chk = (x + y + phase) % 2 == 0
                    on = d >= 4 or (d == 3 and (y == 1 or chk)) or (d == 2 and chk) or \
                        (d == 1 and y == 2 and chk)
                    if on:
                        g[y][x] = 'K'
            frames.append(g)
    save('shadow', "The chopper's shadow, 32x4: 4 sizes (low .. high) x 2 dither phases.", PF1, frames)


def flag():
    frames = []
    for f in range(2):
        g = grid(16, 24)
        for y in range(0, 24):
            g[y][1] = 'S'
            g[y][2] = 'K'
        g[0][1] = 'Y'
        for y in range(1, 9):
            for x in range(3, 15):
                wave = int(round(math.sin((x + f * 3) * 0.7)))
                yy = y + wave
                if 1 <= yy < 10:
                    g[yy][x] = 'B' if y < 5 else 'W'
        for y in range(3, 7):
            for x in range(6, 9):
                g[y + int(round(math.sin((x + f * 3) * 0.7)))][x] = 'Y'
        frames.append(g)
    save('flag', 'Home base flag, 16x24, waving.', PF1, frames)


# ---------------------------------------------------------------- PF2 bands
# Indices 1-7 dark to light. art/mtn_pal.txt and art/land_pal.txt give mission
# 1's colours; the copper loads each mission's own (logic.c tables).

MTN_LETTERS = '1234567'
MTN_PAL = ['0x413', '0x524', '0x635', '0x746', '0x857', '0xA68', '0xC9A']
LAND_PAL = ['0x310', '0x521', '0x742', '0x963', '0xB84', '0xDA6', '0xEC8']


def periodic_height(w, rnd, terms):
    phases = [rnd.uniform(0, 2 * math.pi) for _ in terms]
    return [sum(a * math.sin(2 * math.pi * k * x / w + p) for (k, a), p in zip(terms, phases))
            for x in range(w)]


def mountains():
    w, h = 640, 76
    rnd = random.Random(1982)
    g = grid(w, h)
    # far range: hazy, light
    far = periodic_height(w, rnd, [(3, 9), (7, 5), (13, 3), (29, 1.5)])
    near = periodic_height(w, rnd, [(2, 12), (5, 8), (11, 4), (23, 2.5), (47, 1.2)])
    for x in range(w):
        top = int(26 - far[x])
        for y in range(max(0, top), h):
            g[y][x] = '5' if y - top < 2 else '4'
        # far snow
        if top < 18:
            for y in range(max(0, top), min(h, top + 2)):
                g[y][x] = '6'
    for x in range(w):
        top = int(46 - near[x])
        slope = near[(x + 1) % w] - near[x - 1]
        for y in range(max(0, top), h):
            d = y - top
            if d < 2:
                c = '7' if top < 30 else ('5' if slope > 0 else '3')
            elif d < 8:
                c = '3' if slope > 0.15 else '2'
            else:
                c = '2' if (x * 7 + y * 3) % 11 else '1'
            if y > h - 10:
                c = '1'
            g[y][x] = c
    save('mountains', 'Far mountains band, 640x76 (PF2, camera / 4). Loops.',
         list(zip(MTN_LETTERS, MTN_PAL)), [g])


def hills():
    w, h = 640, 27
    rnd = random.Random(64)
    g = grid(w, h)
    hh = periodic_height(w, rnd, [(4, 5), (9, 3), (17, 1.5), (40, 0.8)])
    for x in range(w):
        top = int(13 - hh[x])
        slope = hh[(x + 1) % w] - hh[x - 1]
        for y in range(max(0, top), h):
            d = y - top
            c = ('5' if slope > 0 else '4') if d < 2 else '3' if d < 7 else '2'
            g[y][x] = c
    # scrub and boulders on the slopes
    for _ in range(90):
        x = rnd.randrange(w)
        col = [y for y in range(h) if g[y][x] != '.']
        if not col:
            continue
        y = col[0]
        for dx in range(-1, 2):
            for dy in range(-2, 1):
                if 0 <= y + dy < h and abs(dx) + abs(dy) < 3:
                    g[y + dy][(x + dx) % w] = '2' if rnd.random() < 0.7 else '1'
    save('hills', 'Near hills band, 640x27 (PF2, camera / 2). Loops.',
         list(zip(MTN_LETTERS, LAND_PAL)), [g])


def ground():
    w, h = 320, 32
    rnd = random.Random(7)
    g = grid(w, h, '4')
    for x in range(w):
        g[0][x] = '2'
        g[1][x] = '3'
        for y in range(2, 10):
            g[y][x] = '4' if (x + y * 5) % 13 else '3'
        # the track where everyone stands (FEET_Y 240 = row 16)
        for y in range(10, 20):
            g[y][x] = '6' if 12 <= y <= 17 else '5'
        g[13][x] = '5' if x % 6 < 3 else '6'
        g[16][x] = '5' if (x + 2) % 7 < 3 else '6'
        for y in range(20, h):
            g[y][x] = '3' if y < 26 else '2'
    # rocks and tufts in the foreground
    for _ in range(60):
        x, y = rnd.randrange(w), rnd.randrange(21, h - 1)
        c = rnd.choice('1215')
        g[y][x] = c
        g[y][(x + 1) % w] = c
        if rnd.random() < 0.5:
            g[y - 1][x] = '7' if c == '5' else c
    for _ in range(30):
        x, y = rnd.randrange(w), rnd.randrange(3, 9)
        g[y][x] = '5'
    save('ground', 'Ground band, 320x32 (PF2, camera / 1). Loops.',
         list(zip(MTN_LETTERS, LAND_PAL)), [g])


CLOUD_PAL = ['0x312', '0x423', '0x534', '0x745', '0xA55', '0xD86', '0xFB8']
BAYER4 = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]


def clouds():
    """Far cloud banks, 640x40 (PF2, camera / 8), low in the sky. Loops. Lit from
    below (the sun or moon is low): bright rims on the undersides, darker bodies,
    ragged dithered edges so they melt into the sky gradient behind them.
    (Every row of the band is a line of 6-plane DMA on the A500: keep it short.)"""
    w, h = 640, 40
    rnd = random.Random(1945)
    g = grid(w, h)
    # two layers of banks: (centre row, half thickness, terms of the thickness profile)
    layers = [(11, 6, [(3, 1.0), (7, 0.6), (17, 0.3)]),
              (28, 8, [(2, 1.0), (5, 0.7), (13, 0.35)])]
    for cy, half, terms in layers:
        prof = periodic_height(w, rnd, [(k, a) for k, a in terms])
        lo, hi = min(prof), max(prof)
        drift = periodic_height(w, rnd, [(3, 2.5), (8, 1.0)])
        for x in range(w):
            t = (prof[x] - lo) / (hi - lo)              # 0..1 thickness here
            th = half * (t * 1.6 - 0.45)                # gaps where it's thin
            if th <= 0.6:
                continue
            c = cy + drift[x]
            top, bot = c - th * 0.8, c + th            # flatter tops, deeper bellies
            for y in range(max(0, int(top) - 1), min(h, int(bot) + 2)):
                # distance inside the bank: dithered fade at the edges
                inside = min(y - top, bot - y)
                if inside < 0:
                    continue
                if inside < 1.5 and inside * 10 < BAYER4[y & 3][x & 3]:
                    continue
                u = (y - top) / (bot - top)             # 0 top .. 1 underside
                if u > 0.82:
                    v = 7 if (u > 0.92 or (x + y) & 1) else 6   # the lit rim
                elif u > 0.62:
                    v = 5
                elif u > 0.35:
                    v = 4 if (x * 3 + y) % 5 else 3
                else:
                    v = 3 if u > 0.15 else 2
                if inside < 1.0:
                    v = max(1, v - 2)                   # thin edges darker: depth
                g[y][x] = str(v)
    save('clouds', 'Far cloud banks, 640x40 (PF2, camera / 8). Loops. Lit from below.',
         list(zip(MTN_LETTERS, CLOUD_PAL)), [g])


def palettes():
    with open(os.path.join(art, 'mtn_pal.txt'), 'w') as f:
        f.write('# The mountains band (PF2, colours 9-15). Mission 1; logic.c has the others.\n')
        f.write('0  0x000  transparent\n')
        for i, c in enumerate(MTN_PAL):
            f.write(f'{i + 1}  {c}  m{i + 1}\n')
    with open(os.path.join(art, 'cloud_pal.txt'), 'w') as f:
        f.write('# The clouds band (PF2, colours 9-15). Mission 1; logic.c has the others.\n')
        f.write('0  0x000  transparent\n')
        for i, c in enumerate(CLOUD_PAL):
            f.write(f'{i + 1}  {c}  c{i + 1}\n')
    with open(os.path.join(art, 'land_pal.txt'), 'w') as f:
        f.write('# The hills and ground bands (PF2, colours 9-15). Mission 1; logic.c has the others.\n')
        f.write('0  0x000  transparent\n')
        for i, c in enumerate(LAND_PAL):
            f.write(f'{i + 1}  {c}  l{i + 1}\n')


if __name__ == '__main__':
    chopper()
    hangar()
    fence()
    barracks()
    tank()
    hostage()
    jet()
    mine()
    small()
    boom()
    shadow()
    flag()
    clouds()
    mountains()
    hills()
    ground()
    palettes()
