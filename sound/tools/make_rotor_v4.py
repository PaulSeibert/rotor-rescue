#!/usr/bin/env python3
"""Previews for rotor v4 (Paul's playtest, 2026-11-14: "too tinny or harsh in flight ... a low thump, huey-like").

    mkdir -p /tmp/rr_old && git archive 9886ad1 src | tar -x -C /tmp/rr_old        # the build Paul played (v3)
    gcc -I /tmp/rr_old/src -o /tmp/rotor_sched_before sound/tools/rotor_sched.c /tmp/rr_old/src/logic.c /tmp/rr_old/src/hiscore.c
    gcc -I src -o /tmp/rotor_sched_after sound/tools/rotor_sched.c src/logic.c src/hiscore.c
    python3 sound/tools/make_rotor_v4.py OUTDIR

Both are Paula playing the loop at the game's period and volume a frame (make_rotor.render: a sample-and-hold
of the 8-bit bytes, as the chip does it, no filter: what WinUAE's unfiltered output does):
  before  v3 as Paul played it: one-rate loop (4 passes of 1200 at period 591: sample-rate lines at 6 kHz),
          the v3 slap
  after   v4: the loop at twice the rate (period 296 at hover: lines at 12 kHz), the softer, lower slap,
          descending settles a little lower
Files: {before,after}-{hover,cruise,climb,descend,lift-empty,lift-l16,cruise-dmg,cruise-l16,flight}.wav
plus {before,after}-flight-with-effects.wav (the same flight with the effects as the game plays them).
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.expanduser("~/amiga-game-kit/harness"))
import make_rotor as mr                      # noqa: E402
import make_rotor_v3 as v3                   # noqa: E402  (its effects() and mix())

KINDS = {   # name: (scheduler kind, scheduler states)
    "hover": "hover", "cruise": "cruise", "climb": "climb", "descend": "descend", "lift-empty": "lift",
    "lift-l16": "lift@l16", "cruise-dmg": "cruise@dmg", "cruise-l16": "cruise@l16", "flight": "flight",
}


def sched(binary, kind):
    r = subprocess.run([binary, kind], capture_output=True, text=True, check=True)
    return [tuple(int(x) for x in ln.split()) for ln in r.stdout.split("\n") if ln.strip()]


def mix_stage(rotor, stage, fx):
    out = [float(v) for v in rotor]
    for t, name in stage:
        at = int(t * v3.RATE)
        for i, v in enumerate(fx[name]):
            if at + i < len(out):
                out[at + i] += v
    return [max(-32768, min(32767, int(v))) for v in out]


def main():
    outdir = sys.argv[1]
    os.makedirs(outdir, exist_ok=True)
    loops = {"before": (mr.make_new()[0], 1), "after": (mr.make_v4()[0], mr.OS4)}
    fx = v3.effects()
    stage = [(1.0, "boom"), (4.0, "tankfire"), (7.0, "jetwarn"), (7.0, "jet"), (9.0, "tankfire"), (12.0, "board"), (12.3, "board")]
    for which, (loop, os_) in loops.items():
        binary = f"/tmp/rotor_sched_{which}"
        for name, kind in KINDS.items():
            s = sched(binary, kind)
            wav = mr.render(loop, s, os_=os_)
            mr.write_wav16(os.path.join(outdir, f"{which}-{name}.wav"), wav)
            pers = [p for p, _ in s]
            vols = [v for _, v in s]
            print(f"{which} {name}: {len(s)} frames, logic period {min(pers)}-{max(pers)} "
                  f"({mr.CLOCK * 10 // (max(1, min(pers)) * mr.PASS) / 10:.1f}-{mr.CLOCK * 10 // (max(pers) * mr.PASS) / 10:.1f} thumps/s), volume {min(vols)}-{max(vols)}")
        s = sched(binary, "flight")
        wav = mix_stage(mr.render(loop, s, os_=os_), stage, fx)
        mr.write_wav16(os.path.join(outdir, f"{which}-flight-with-effects.wav"), wav)


if __name__ == "__main__":
    main()
