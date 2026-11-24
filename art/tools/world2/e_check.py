"""Gates for the slice E art: `python e_check.py <art/world2 dir>` (exit 1 on a hard failure).

Hard: frame counts and sizes, only the 7 PF1 pen letters, hull_list frame 0 = destroyer_hull.txt frame 0 outside the cleared
AA mount box (the existing art is not otherwise touched), the waterline rows 41-43 identical in both hull frames, the list
(bow lower, stern higher, enough pixels differ), the four AA mounts: little overlap with the hull and a hull row under
the gun's base, AA frames differ in silhouette, turret hit frames differ from idle and keep the idle housing's bottom
row, the barrage has no outline pen, grows frame by frame, and red / yellow only where drawn. Soft (printed): the
largest solid area of one pen, open edges.
"""
import os
import sys

from cdlib import read_txt
from e_art import AA_SLOTS, CLEAR_BOX, TURRET_MOUNTS, BODY_ROWS, HULL_H, HULL_W, clean_hull, list_offsets, cv_from

PENS = set("KOSWRYB.")


def load(art, name):
    return read_txt(os.path.join(art, name))[1]


def largest_solid(frame, pen):
    h, w = len(frame), len(frame[0])
    best = (0, 0, 0)
    for y0 in range(h):
        for x0 in range(w):
            if frame[y0][x0] != pen:
                continue
            maxw = w - x0
            for y1 in range(y0, h):
                run = 0
                while run < maxw and frame[y1][x0 + run] == pen:
                    run += 1
                maxw = run
                if maxw == 0:
                    break
                if maxw * (y1 - y0 + 1) > best[0]:
                    best = (maxw * (y1 - y0 + 1), maxw, y1 - y0 + 1)
    return best


def sil(f):
    return {(x, y) for y, r in enumerate(f) for x, ch in enumerate(r) if ch != "."}


def main(art):
    bad = 0

    def fail(msg):
        nonlocal bad
        bad += 1
        print("FAIL", msg)

    files = {"aa_gun.txt": (12, 12, 3), "turret_hit.txt": (32, 19, 2), "hull_list.txt": (154, 44, 2), "barrage_tell.txt": (24, 12, 3)}
    F = {}
    for name, (w, h, n) in files.items():
        fr = load(art, name)
        F[name] = fr
        if len(fr) != n:
            fail(f"{name}: {len(fr)} frames, expected {n}")
        for i, f in enumerate(fr):
            if len(f) != h or any(len(r) != w for r in f):
                fail(f"{name} frame {i}: not {w} x {h}")
            if any(ch not in PENS for r in f for ch in r):
                fail(f"{name} frame {i}: a letter outside the PF1 pens")
        print(f"ok   {name}: {n} frames {w} x {h}")

    # ---- the hull: the existing art is untouched outside the cleared mount
    old = load(art, "destroyer_hull.txt")[0]
    h0, h1 = F["hull_list.txt"]
    x0, y0, x1, y1 = CLEAR_BOX
    diff_out = [(x, y) for y in range(HULL_H) for x in range(HULL_W)
                if old[y][x] != h0[y][x] and not (x0 <= x <= x1 and y0 <= y <= y1)]
    if diff_out:
        fail(f"hull_list frame 0 differs from destroyer_hull.txt frame 0 outside the cleared box at {diff_out[:5]}")
    else:
        d = sum(1 for y in range(y0, y1 + 1) for x in range(x0, x1 + 1) if old[y][x] != h0[y][x])
        print(f"ok   hull_list frame 0 = destroyer_hull.txt frame 0 except {d} px in the cleared AA mount box x {x0}-{x1} y {y0}-{y1}")
    for y in range(BODY_ROWS, HULL_H):
        if h0[y] != h1[y]:
            fail(f"hull_list: waterline row {y} differs between the frames")
    else:
        print(f"ok   hull_list: waterline rows {BODY_ROWS}-{HULL_H - 1} identical in both frames")
    # the list: the red boot-topping stripe (base row 38) at x 60 sinks, at x 140 it rises
    def red_row(f, x):
        ys = [y for y in range(30, HULL_H) if f[y][x] == "R"]
        return ys[0] if ys else None
    r60a, r60b = red_row(h0, 60), red_row(h1, 60)
    r140a, r140b = red_row(h0, 140), red_row(h1, 140)
    print(f"info red stripe row: x 60 {r60a} -> {r60b}, x 140 {r140a} -> {r140b}")
    if r60b is None or r60b - r60a < 2:
        fail("hull_list: the hull side does not sink by at least 2 px at x 60")
    if r140b is None or r140b - r140a > -1:
        fail("hull_list: the stern does not rise by at least 1 px at x 140")
    changed = len(sil(h0) ^ sil(h1))
    print(f"{'ok  ' if changed >= 200 else 'FAIL'} hull_list: the silhouette differs from the intact hull by {changed} px (tilt + smoke)")
    if changed < 200:
        bad += 1

    # ---- AA gun
    aa = F["aa_gun.txt"]
    for i, f in enumerate(aa):
        base = f[11]
        if base[2:11].count("K") != 9:
            fail(f"aa_gun frame {i}: the base line (row 11, cols 2-10) is not all K")
    for i, nm in ((1, "tell"), (2, "puff")):
        dif = len(sil(aa[0]) ^ sil(aa[i]))
        print(f"{'ok  ' if dif >= 6 else 'FAIL'} aa_gun {nm}: silhouette differs from idle by {dif} px")
        if dif < 6:
            bad += 1
    if "R" not in "".join(aa[1]) or "Y" not in "".join(aa[2]) or "R" in "".join(aa[0]) or "Y" in "".join(aa[0]):
        fail("aa_gun: red must be in the tell only and yellow in the puff only (idle has neither)")
    else:
        print("ok   aa_gun: red only in the tell, yellow only in the puff")
    base = cv_from(h0)
    for k, (sx, sy) in AA_SLOTS.items():
        for i, f in enumerate(aa):
            ov = sum(1 for y in range(12) for x in range(12) if f[y][x] != "." and base.get(sx + x, sy + y) != ".")
            left = any(base.get(sx + x, sy + 12) != "." for x in (2, 3, 4))
            right = any(base.get(sx + x, sy + 12) != "." for x in (8, 9, 10))
            if ov > 6:
                fail(f"aa slot {k} frame {i}: {ov} px of the gun lie on hull art")
            if not (left and right):
                fail(f"aa slot {k}: the base is not carried at both ends (hull under cols 2-4 and 8-10 at y {sy + 12})")
        ov0 = sum(1 for y in range(12) for x in range(12) if aa[0][y][x] != "." and base.get(sx + x, sy + y) != ".")
        sup = sum(1 for x in range(2, 11) if base.get(sx + x, sy + 12) != ".")
        print(f"ok   aa slot {k} at hull ({sx}, {sy}): {ov0} px over hull art, base row on hull y {sy + 12} with {sup}/9 columns supported")

    # ---- turret hit
    idle = load(art, "destroyer_turret.txt")[0]
    for i, nm in enumerate(("smoking", "dead")):
        f = F["turret_hit.txt"][i]
        dif = len(sil(idle) ^ sil(f))
        print(f"{'ok  ' if dif >= 40 else 'FAIL'} turret {nm}: silhouette differs from idle by {dif} px")
        if dif < 40:
            bad += 1
        if f[18][16:31] != idle[18][16:31]:
            fail(f"turret {nm}: the housing's bottom row 18 changed (it must keep sitting on the mount)")
    smoking, dead = F["turret_hit.txt"]
    if "Y" not in "".join(smoking) or "R" not in "".join(smoking):
        fail("turret smoking: no flames (R/Y)")
    if "Y" in "".join(dead):
        fail("turret dead: still burning (Y)")
    print("ok   turret: smoking has flames (R/Y), dead has none; the housing bottom row is the idle one in both")

    # ---- barrage
    bt = F["barrage_tell.txt"]
    counts = [sum(1 for r in f for ch in r if ch != ".") for f in bt]
    print(f"info barrage pixel counts per frame: {counts}")
    if any("K" in "".join(f) for f in bt):
        fail("barrage: an outline pen K (water effects have a cream fringe, no outline)")
    if not (counts[0] < counts[1] < counts[2]):
        fail("barrage: the frames do not grow")
    if "R" in "".join(bt[0]) or "Y" in "".join(bt[0]) or "Y" in "".join(bt[1]) or "Y" not in "".join(bt[2]) or "R" not in "".join(bt[1]):
        fail("barrage: red must start at frame 1 and yellow at frame 2")
    else:
        print("ok   barrage: no outline, grows 0 < 1 < 2, red from frame 1, yellow in frame 2")

    # ---- soft: solid areas
    for name in files:
        for i, f in enumerate(F[name]):
            worst = (0, "")
            for pen in "OSWRYB":
                a, ww, hh = largest_solid(f, pen)
                if a > worst[0]:
                    worst = (a, f"{pen} {ww} x {hh}")
            print(f"{'ok  ' if worst[0] <= 40 else 'WARN'} {name} frame {i}: largest solid area {worst[1]} ({worst[0]} px)")
    print("RESULT:", "PASS" if bad == 0 else f"{bad} FAILURES")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "."))
