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
    n = len(W.bank)
    if n > 40:
        fail("tile bank %d columns > 40" % n)
    if max(W.strip.max(), 0) > 7:
        fail("strip pen > 7")
    print("assets: %d BOB sets, tile bank %d columns, facing pairs re-lit -> %s" % (len(sets), n, "FAIL" if bad else "ok"))
    return bad
