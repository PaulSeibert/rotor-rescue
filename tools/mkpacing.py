#!/usr/bin/env python3
"""Writes tools/pacing<skill>.agk: the pacing scenario for one difficulty.

    python3 tools/mkpacing.py 1 3          # tools/pacing1.agk, tools/pacing3.agk
    agk run -f tools/pacing1.agk -o build/pacing1
    python3 tools/pace.py build/pacing1/serial.txt

Each starts from the 'boarding' checkpoint (tests/rescue.agk: landed by the open
barracks), switches the chopper to ghost (Ctrl+L, G: nothing destroys it, so the
run lasts, but the difficulty's own tables apply), sets the difficulty and moves
the schedule on to the trip where jets are on (Ctrl+L, N, once per trip), then
CYCLES times: sits landed LANDED frames (boarding), takes off, flies left and
back for about 900 frames, lands again. About 2 minutes of play with the defaults.
"""
import sys

JETS_FROM = {1: 3, 2: 2, 3: 1, 4: 1, 5: 0}   # trips + missions before jets (src/logic.c)
CYCLES, LANDED = 3, 1200   # (the emulator runs ~40 frames a second and a run may last 300 s)

KEYS = {0: 0x0A, 1: 1, 2: 2, 3: 3, 4: 4, 5: 5}


def ctrl_l(key, echo):
    # (wait 5 dropped keys: the game polls the keyboard once a frame and the harness is quick)
    return ["key 0x63", "key 0x28", "wait 10", f"key 0x{key:02X}", "wait 30", f'expect-serial "{echo}"']


def build(skill):
    out = [f"# Pacing at difficulty {skill}: written by tools/mkpacing.py, run with tools/pace.py (see there).",
           "start-at boarding", "wait 5"]
    out += ctrl_l(0x24, "ghost=1")          # ghost
    out += ctrl_l(KEYS[skill], f"difficulty={skill}")   # difficulty
    for n in range(1, JETS_FROM[skill] + 1):
        out += ctrl_l(0x36, f"trip={n}")     # next trip
    out += ["wait 20"]
    for _ in range(CYCLES):
        out += [f"wait {LANDED}",
                "press up 40", "press left 300", "press right 300", "press left 100", "press right 100",
                "press down 90", "wait 10"]
    out += ["wait 100", 'expect-no-serial "AGK ERR"']
    return "\n".join(out) + "\n"


for a in sys.argv[1:] or ["1", "3"]:
    open(f"tools/pacing{a}.agk", "w").write(build(int(a)))
    print(f"tools/pacing{a}.agk")
