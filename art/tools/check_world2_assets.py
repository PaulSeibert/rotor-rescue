"""Asset checks for art/world2 (run by check_world2.py --assets): the files are current, BOB frames are one size and use only the
PF1 pens, a facing pair is re-lit (not a plain mirror) with the light on the left in both, the tile bank fits the budget."""
import os
import sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "world2"))
import numpy as np
from w2lib import ART


def frames(name):
    rows, cur = [], None
    out = []
    for line in open(os.path.join(ART, name + ".txt")):
        line = line.rstrip("\n")
        if line.startswith("#") or not line.strip():
            continue
        if line == "frame":
            cur = []
            out.append(cur)
        elif cur is not None:
            cur.append(line)
    return out


def rim(fr):
    """(lit, shade): stone beside olive with the stone on the left / on the right, in every row"""
    lit = shade = 0
    for r in fr:
        for a, b in zip(r, r[1:]):
            lit += a == "S" and b == "O"
            shade += a == "O" and b == "S"
    return lit, shade


def flag(fr):
    """(pixels left of the mast, pixels right of it) of the masthead pennant: the non-outline pixels in the rows above the cabin
    roof (the first row with a long run of outline), the mast being the column that is outline in most of those rows"""
    top = next(i for i, r in enumerate(fr) if "K" * 8 in r)
    rows = fr[:top]
    mast = max(range(len(rows[0])), key=lambda x: sum(r[x] == "K" for r in rows))
    near = 6                                                     # the pennant is 3 px long; the tell's muzzle lamp (far at the bow) is not the flag
    left = sum(ch not in ".K" for r in rows for ch in r[max(0, mast - near):mast])
    right = sum(ch not in ".K" for r in rows for ch in r[mast + 1:mast + 1 + near])
    return left, right


def run():
    bad = 0

    def fail(msg):
        nonlocal bad
        bad += 1
        print("  asset FAIL: " + msg)
    import make_world2
    files, sets, W = make_world2.outputs()
    for rel, data in files.items():
        data = data.encode() if isinstance(data, str) else data
        p = os.path.join(ART, rel)
        if not os.path.exists(p) or open(p, "rb").read() != data:
            fail("%s is stale: run make_world2.py" % rel)
    for name, (fs, _) in sets.items():
        fr = frames(name)
        if len(fr) != len(fs) or len({(len(f), len(f[0])) for f in fr}) != 1:
            fail("%s: frame count or size" % name)
        if set("".join("".join(f) for f in fr)) - set(".KOSWRYB"):
            fail("%s: a pen outside the PF1 set" % name)
    for name, per in (("patrol_boat", 2), ("gunboat", 4), ("destroyer_hull", 1), ("destroyer_turret", 4)):
        fr = frames(name)
        left, right = fr[:per], fr[per:]
        for i, (a, b) in enumerate(zip(left, right)):
            if [r[::-1] for r in a] == b:
                fail("%s frame %d: the right-facing frame is a plain mirror" % (name, i))
            la, sa = rim(a)
            lb, sb = rim(b)
            if name != "destroyer_turret" and (lb < sb or la < sa):
                fail("%s frame %d: light is not from the left (lit rims %d/%d, shade %d/%d)" % (name, i, la, lb, sa, sb))
    pb = frames("patrol_boat")                                   # the mast flag streams toward the stern, away from the heading
    for i, f in enumerate(pb):
        left, right = flag(f)
        face_left = i in (0, 1, 4)                                        # frames 0-1 face left, so the stern (and the flag) is to the right
        toward, away = (right, left) if face_left else (left, right)
        if toward < 3 or away:
            fail("patrol_boat frame %d (%s-facing): mast flag must stream toward the stern, >= 3 px and none toward the bow (stern side %d px, bow side %d px)" %
                 (i, "left" if face_left else "right", toward, away))
    # the shot tell (missions 5 on): frame 4 faces left, 5 faces right, drawn by the same generator as the idle frames
    if len(pb) < 6:
        fail("patrol_boat: no tell frames (4 = left, 5 = right); %d frames" % len(pb))
    else:
        for tell, idle, name in ((4, 0, "left"), (5, 2, "right")):
            t, d = pb[tell], pb[idle]
            h, w = len(t), len(t[0])
            bow = range(0, 13) if name == "left" else range(w - 13, w)       # the bow gun and its raised barrel: top rows, 13 columns at the bow
            gun = [(x, y) for y in range(0, 9) for x in bow]
            diff = sum(t[y][x] != d[y][x] for x, y in gun)
            lamp = sum(t[y][x] in "RY" for x, y in gun) - sum(d[y][x] in "RY" for x, y in gun)
            if diff < 8:
                fail("patrol_boat tell frame %d (%s) differs from idle %d in only %d px of the gun area (need >= 8)" % (tell, name, idle, diff))
            if lamp < 4:
                fail("patrol_boat tell frame %d (%s): the muzzle lamp needs >= 4 more red/yellow px than idle (has %d)" % (tell, name, lamp))
            rest = sum(t[y][x] != d[y][x] for y in range(h) for x in range(w) if (x, y) not in set(gun))
            if rest:
                fail("patrol_boat tell frame %d (%s): %d px differ from idle outside the gun area (only the gun and lamp may move)" % (tell, name, rest))
        if [r[::-1] for r in pb[4]] == pb[5]:
            fail("patrol_boat tell frames: the right-facing tell is a plain mirror")
        l4, s4 = rim(pb[4])
        l5, s5 = rim(pb[5])
        if l5 < s5 or l4 < s4:
            fail("patrol_boat tell frames: light is not from the left")
    n = len(W.bank)
    if n > 40:
        fail("tile bank %d columns > 40" % n)
    if max(W.strip.max(), 0) > 7:
        fail("strip pen > 7")
    print("assets: %d BOB sets, tile bank %d columns, facing pairs re-lit -> %s" % (len(sets), n, "FAIL" if bad else "ok"))
    return bad
