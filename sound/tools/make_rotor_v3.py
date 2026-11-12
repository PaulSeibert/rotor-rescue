#!/usr/bin/env python3
"""Previews for rotor v3 (Paul's playtest, 2026-11-12: damaged and loaded were irritating).

    gcc -I src -o /tmp/rotor_sched_after sound/tools/rotor_sched.c src/logic.c          # this build
    gcc -I <main>/src -o /tmp/rotor_sched_before sound/tools/rotor_sched.c <main>/src/logic.c   # before
    python3 sound/tools/make_rotor_v3.py OUTDIR

Same renderer as v2 (make_rotor.render: Paula playing the loop at the game's period and volume a
frame), plus the effects involved, mixed as the game plays them (sound.toml's own synthesis, at
the effect's volume; each is a staged moment, the same before and after, so only the rotor differs).
Files: {before,after}-{cruise,lift}-{empty,dmg,l8,l16,dl16}.wav
  cruise: 5 s at 1.5 px/frame; lift: 2 s idling, lift off, fly right 6 s (the spin-up included).
"""
import os
import subprocess
import sys
import tomllib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.expanduser("~/amiga-game-kit/harness"))
import make_rotor as mr                      # noqa: E402
from agk import sound as ks                  # noqa: E402

RATE = 44100
STATES = ("empty", "dmg", "l8", "l16", "dl16")
BASES = ("cruise", "lift")
# staged effects: (seconds, name); jetwarn and jet come together, as in main.c's playSounds
STAGE = {
    "empty": [],
    "dmg": [(0.3, "boom"), (2.0, "tankfire"), (3.5, "jetwarn"), (3.5, "jet")],
    "l8": [(1.5, "tankfire"), (3.5, "jetwarn"), (3.5, "jet")],
    "l16": [(1.5, "tankfire"), (3.5, "jetwarn"), (3.5, "jet")],
    "dl16": [(0.3, "boom"), (2.0, "tankfire"), (3.5, "jetwarn"), (3.5, "jet")],
}


def sched(binary, base, state):
    kind = base if state == "empty" else f"{base}@{state}"
    r = subprocess.run([binary, kind], capture_output=True, text=True, check=True)
    return [tuple(int(x) for x in ln.split()) for ln in r.stdout.split("\n") if ln.strip()]


def effects():
    with open(os.path.join(HERE, "..", "sound.toml"), "rb") as f:
        toml = tomllib.load(f)
    out = {}
    for name, spec in toml["sfx"].items():
        data = ks.synth_sfx(name, spec)
        pcm = [(b - 256 if b > 127 else b) * 256 * int(spec.get("volume", 64)) / 64 for b in data]
        step = ks.SFX_RATE / RATE
        out[name] = [pcm[int(i * step)] for i in range(int(len(pcm) / step))]
    return out


def mix(rotor, state, fx):
    out = [float(v) for v in rotor]
    for t, name in STAGE[state]:
        at = int(t * RATE)
        for i, v in enumerate(fx[name]):
            if at + i < len(out):
                out[at + i] += v
    return [max(-32768, min(32767, int(v))) for v in out]


def main():
    outdir = sys.argv[1]
    os.makedirs(outdir, exist_ok=True)
    loop, _ = mr.make_new()
    fx = effects()
    for which in ("before", "after"):
        binary = f"/tmp/rotor_sched_{which}"
        for base in BASES:
            for state in STATES:
                s = sched(binary, base, state)
                pers = [p for p, _ in s]
                vols = [v for _, v in s]
                wav = mix(mr.render(loop, s), state, fx)
                mr.write_wav16(os.path.join(outdir, f"{which}-{base}-{state}.wav"), wav)
                print(f"{which} {base} {state}: {len(s)} frames, period {min(pers)}-{max(pers)} "
                      f"(max {mr.CLOCK * 10 // (max(1, min(pers)) * mr.PASS) / 10:.1f} thumps/s), volume {min(vols)}-{max(vols)}")


if __name__ == "__main__":
    main()
