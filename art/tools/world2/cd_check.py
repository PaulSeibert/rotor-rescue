"""Gates for the slice C/D art: `python cd_check.py <art/world2 dir>` (exit 1 on a hard failure).

Hard: same frame size in a file, only the 7 PF1 pen letters, the deck stays flat and free over the hover box, the freighter stages
differ, a flat pen area stays small (largest solid rectangle of one pen <= 6 x 4 outside the foam), no row carries more
than 85 % of its pixels in one pen on the hull rows. Soft (printed): outline gaps.
"""
import math
import os
import sys

from cdlib import read_txt

PENS = set("KOSWRYB.")


def load(art, name):
    colors, frames = read_txt(os.path.join(art, name))
    return frames


def largest_solid(frame, pen):
    h, w = len(frame), len(frame[0])
    best = (0, 0, 0)
    for y0 in range(h):
        for x0 in range(w):
            if frame[y0][x0] != pen:
                continue
            # grow the widest rectangle starting here
            maxw = w - x0
            for y1 in range(y0, h):
                run = 0
                while run < maxw and frame[y1][x0 + run] == pen:
                    run += 1
                maxw = run
                if maxw == 0:
                    break
                area = maxw * (y1 - y0 + 1)
                if area > best[0]:
                    best = (area, maxw, y1 - y0 + 1)
    return best


def row_dom(frame):
    worst = (0, 0, "")
    for y, row in enumerate(frame):
        px = [ch for ch in row if ch != "."]
        if len(px) < 24:
            continue
        for pen in "KOSWRYB":
            share = px.count(pen) / len(px)
            if share > worst[0]:
                worst = (share, y, pen)
    return worst


def top_rows(frame, x0, x1):
    out = []
    for x in range(x0, x1 + 1):
        ys = [y for y in range(len(frame)) if frame[y][x] != "."]
        out.append(min(ys) if ys else None)
    return out


def main(art):
    bad = 0

    def fail(msg):
        nonlocal bad
        bad += 1
        print("FAIL", msg)

    files = {"deck_ship.txt": (124, 30, 2), "winch_rope.txt": (8, 40, 3), "winch_hook.txt": (8, 12, 1),
             "man_climb.txt": (16, 12, 2), "freighter.txt": (128, 36, 4), "sink_spray.txt": (24, 12, 3)}
    for name, (w, h, n) in files.items():
        fr = load(art, name)
        if len(fr) != n:
            fail(f"{name}: {len(fr)} frames, expected {n}")
        for i, f in enumerate(fr):
            if len(f) != h or any(len(r) != w for r in f):
                fail(f"{name} frame {i}: not {w} x {h}")
            if any(ch not in PENS for r in f for ch in r):
                fail(f"{name} frame {i}: a letter outside the PF1 pens")
        print(f"ok   {name}: {n} frames {w} x {h}")

    # solids and row dominance
    for name in ("deck_ship.txt", "freighter.txt"):
        for i, f in enumerate(load(art, name)):
            worst = (0, None)
            for pen in "OSWRYB":
                a, ww, hh = largest_solid(f, pen)
                if a > worst[0]:
                    worst = (a, f"{pen} {ww} x {hh}")
            area, desc = worst
            tag = "ok  "
            if area > 40:
                tag = "WARN"
            print(f"{tag} {name} frame {i}: largest solid area {desc} ({area} px)")
            dom = row_dom(f)
            tag = "ok  " if dom[0] <= 0.85 else "WARN"
            print(f"{tag} {name} frame {i}: busiest row {dom[1]} is {dom[0]*100:.0f} % pen {dom[2]}")

    # the deck ship's deck: flat and bare over x 34-85 (hover box), top outline on row 8 in every frame
    for i, f in enumerate(load(art, "deck_ship.txt")):
        tops = top_rows(f, 34, 85)
        if set(tops) != {8}:
            fail(f"deck_ship frame {i}: deck/free span x 34-85 is not flat on row 8: {sorted(set(tops))}")
        else:
            print(f"ok   deck_ship frame {i}: x 34-85 (52 px) flat on row 8 (y 227), nothing above")
        if f[27][14:116].count(".") > 0:
            fail(f"deck_ship frame {i}: hull bottom row 27 has a gap")
    # the freighter: the deck edge in the hold zone, per stage, and the pivot
    fr = load(art, "freighter.txt")
    stage_tops = []
    for i, f in enumerate(fr):
        tops = top_rows(f, 52, 76)
        stage_tops.append(tops)
        print(f"ok   freighter stage {i}: hover box x 52-76 top rows {min(tops)}..{max(tops)} (coaming top; rise {max(tops)-min(tops)} px across 24 px)")
        if max(tops) - min(tops) > 3:
            fail(f"freighter stage {i}: hover box not flat enough")
    ref = min(stage_tops[0])
    for i in range(1, 4):
        mid = stage_tops[i][12]
        print(f"ok   freighter stage {i}: coaming top at the box centre moves {mid - stage_tops[0][12]:+d} px from stage 0")
    # bow deck height above the water at stage 3 (deck edge column near x 6-8 vs waterline row 33)
    for i, f in enumerate(fr):
        ys = [y for y in range(36) if f[y][12] != "." and f[y][12] == "K"]
        bowtop = min(y for y in range(36) if f[y][12] != ".")
        xmin = min(x for x in range(128) for y in range(36) if f[y][x] == "K" and y <= 33 - 3)
        tip = min(y for y in range(36) if f[y][xmin] != ".")
        print(f"ok   freighter stage {i}: deck edge at x 12 is row {bowtop} ({33 - bowtop} px above the waterline); at the bow tip x {xmin} row {tip} ({33 - tip} px)")
    # stages differ
    for i in range(3):
        diff = sum(1 for y in range(36) for x in range(128) if fr[i][y][x] != fr[i + 1][y][x])
        sil = sum(1 for y in range(36) for x in range(128) if (fr[i][y][x] == ".") != (fr[i + 1][y][x] == "."))
        tag = "ok  " if sil >= 40 else "FAIL"
        print(f"{tag} freighter stage {i} -> {i+1}: {diff} px differ, silhouette differs by {sil} px")
        if sil < 40:
            bad += 1
    # outline gaps (soft)
    for name in ("deck_ship.txt", "freighter.txt", "man_climb.txt"):
        for i, f in enumerate(load(art, name)):
            h, w = len(f), len(f[0])
            gaps = 0
            for y in range(h):
                for x in range(w):
                    if f[y][x] in "OSWRYB":
                        for dx, dy in ((1, 0), (-1, 0), (0, -1)):
                            nx, ny = x + dx, y + dy
                            if 0 <= nx < w and 0 <= ny < h and f[ny][nx] == ".":
                                gaps += 1
                                break
            print(f"info {name} frame {i}: {gaps} px with an open edge (foam / steam / flecks / outline gaps)")
    print("RESULT:", "PASS" if bad == 0 else f"{bad} FAILURES")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "."))
