#!/usr/bin/env python3
"""Rotor Rescue's jet pacing from a pacing run's serial log (tools/mkpacing.py).

    agk run -f tools/pacing1.agk -o build/pacing1
    python3 tools/pace.py build/pacing1/serial.txt [more logs...]

The game prints a sample every 16 frames in play:
  pace=frame pskill ptrip jets (on screen) tells (jets in their flash) jatk (jet attack
  runs started so far) eshots / jshots (enemy shots / the jet ones: missiles, bombs) pland
Reported per log (docs/SPEC.md section 5):
  attacks/min     jet attack runs started (a flash begun) per minute of play
  jets on screen  mean, and the share of time at least one is
  pairs           attack starts less than 3 s after the one before (a pair: two jets together)
  min gap         the shortest time between two attack starts
  landed          the share of time the chopper sat on the ground
  landed, attacked  the share of landed time with a jet flash or a jet shot (missile, bomb) up
  landed, any fire  the same counting tank shells too
  first attack    seconds from touching down to the first jet attack run started while
                  landed, per landing (none: it left or the run ended first)
"""
import re
import sys

STEP = 16 / 50   # seconds between samples


def load(path):
    rows = []
    for line in open(path, errors="replace"):
        if "pace=" not in line:
            continue
        d = {k: int(v) for k, v in re.findall(r"(\w+)=(-?\d+)", line)}
        if "pace" in d and "jatk" in d:
            rows.append(d)
    return rows


def report(path):
    r = load(path)
    if len(r) < 2:
        print(f"{path}: no pace samples")
        return
    minutes = len(r) * STEP / 60
    attacks = r[-1]["jatk"] - r[0]["jatk"]
    starts = []                       # time (s) of each attack start, to the sample
    for i in range(1, len(r)):
        for _ in range(max(0, r[i]["jatk"] - r[i - 1]["jatk"])):
            starts.append(i * STEP)
    gaps = [b - a for a, b in zip(starts, starts[1:])]
    pairs = sum(1 for x in gaps if x < 3)
    jets = [x["jets"] for x in r]
    landed = [x for x in r if x["pland"]]
    att = sum(1 for x in landed if x["tells"] or x["jshots"])
    anyf = sum(1 for x in landed if x["tells"] or x["eshots"])
    firsts, t0, base = [], None, None
    for i, x in enumerate(r):
        if x["pland"] and t0 is None:
            t0, base = i, x["jatk"]
        elif not x["pland"]:
            if t0 is not None and not any_hit(firsts, t0):
                firsts.append((t0, None))
            t0 = None
        if t0 is not None and x["jatk"] > base and not any_hit(firsts, t0):
            firsts.append((t0, (i - t0) * STEP))
    if t0 is not None and not any_hit(firsts, t0):
        firsts.append((t0, None))
    f = ", ".join("none" if s is None else f"{s:.1f}s" for _t, s in firsts)
    print(f"{path}: skill {r[0]['pskill']}, trip {r[0]['ptrip']}, {minutes:.1f} min of play")
    print(f"  attacks/min      {attacks / minutes:5.1f}   ({attacks} attack runs)")
    print(f"  jets on screen   {sum(jets) / len(jets):5.2f} mean, {100 * sum(1 for j in jets if j) / len(jets):.0f}% of the time at least one, "
          f"{sum(1 for j in jets if j > 1) * 100 / len(jets):.0f}% two")
    print(f"  pairs            {pairs} attack starts under 3 s after the one before; min gap "
          f"{min(gaps):.1f} s" if gaps else "  pairs            0 (fewer than two attacks)")
    print(f"  landed           {100 * len(landed) / len(r):5.0f}% of the time")
    if landed:
        print(f"  landed, attacked {100 * att / len(landed):5.1f}%   (a jet flash or a jet missile/bomb up)")
        print(f"  landed, any fire {100 * anyf / len(landed):5.1f}%   (tank shells too)")
    print(f"  first attack     {f or 'no landing'}   (from touching down)")


def any_hit(lst, t0):
    return any(t == t0 for t, _s in lst)


for p in sys.argv[1:]:
    report(p)
