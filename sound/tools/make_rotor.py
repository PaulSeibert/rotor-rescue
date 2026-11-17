#!/usr/bin/env python3
"""ROTOR RESCUE's rotor: the loop logic.c's logicRotorSynth() makes at startup, with
the same integer arithmetic (keep the two in step: `--check` compiles logic.c and
compares them), and WAV previews of how it plays on Paula in the game.

v4 (SPEC section 9, Paul's playtest of 2026-11-14): make_v4() is the loop the game plays now: made at
twice the rate (Paula's period is half the logic's, so the sample-rate lines sit at 12 kHz, not 6),
a softer, lower slap. make_new() is v3's loop (one rate, 4 passes of 1200), kept to compare and for
the v3 previews.

    python3 sound/tools/make_rotor.py [OUTDIR]            # the loop + analysis
    python3 sound/tools/make_rotor.py OUTDIR --render     # + the in-game previews
                                                          #   (needs sound/tools/rotor_sched.c
                                                          #    built to /tmp/rotor_sched)

A deep blade thump, like a Huey's whop-whop-whop. A loop of PASSES blade
passes, PASS samples each. On every pass:
  - the thump: a low pressure pulse, three damped resonators kicked together:
    ~55 Hz (the weight, ~85 ms), ~130 Hz (~35 ms) and ~230 Hz (~20 ms, the body
    that small speakers still reproduce). (v2, Paul's playtest of 2026-11-12: more
    low body, a softer slap; the rate now tops out at 6.5 a second.) Their impulse responses start with a
    sharp edge and swell: a thud, not a click.
  - the slap: a short burst of band-passed noise (~250 Hz-1.2 kHz), a quarter
    of the thump's level, 25 ms: the "crack" of the blade tip.
  - a very quiet low rumble between the thumps.
The pass is ROTOR_PASS (logic.h) samples, so the Paula period sets the thump
rate: period 591 (6001 Hz) = 5 a second (hover), 455 = 6.5 (full speed).
Four passes a loop, each a little different, so it doesn't repeat every beat.
"""
import math
import os
import struct
import subprocess
import sys
import wave

CLOCK = 3546895
PASS = 1200                  # samples a blade pass (logic.h ROTOR_PASS)
PASSES = 4
N = PASS * PASSES            # 4800 bytes of chip RAM
HOVER_PERIOD = 591           # 6001 Hz: 5 thumps a second
STRENGTH = (96, 84, 92, 78)   # each pass's slap, /128 (a quarter less than v1: the crack was the irritating part)
SLAP_AT = 10                 # samples after the kick the slap starts
PEAK = 126                   # the loop's peak, of 127


def q14(f0, tau, fs):
    """a damped resonator's two Q14 coefficients: f0 Hz, amplitude decays 1/e in tau s"""
    r = math.exp(-1.0 / (tau * fs))
    w = 2 * math.pi * f0 / fs
    return int(round(2 * r * math.cos(w) * 16384)), int(round(r * r * 16384))


FS = CLOCK / HOVER_PERIOD
RES = (  # (A1, A2, kick): the kicks give each a similar share of the peak
    q14(55, 0.085, FS) + (1500,),     # (v2: longer and harder, the low body)
    q14(130, 0.035, FS) + (2300,),
    q14(230, 0.020, FS) + (2000,),
)


def lfsr_step(s):
    lsb = s & 1
    s >>= 1
    if lsb:
        s ^= 0xB400
    return s


def asr(v, n):               # arithmetic shift right, as the 68000 (and C on it) does
    return v >> n


def make_new(verbose=False):
    """exactly rotorSynth()'s integer loop. A warm-up pass first (the last
    pass's slot, so the resonators' tails are in the first real pass: it loops
    seamlessly), then the loop."""
    lfsr = 0x1D2B
    y1 = [0, 0, 0]
    y2 = [0, 0, 0]
    lp1 = lp2 = rum = 0
    raw = []
    peaks = [0, 0, 0]
    for n in range(PASS + N):
        i = n - PASS if n >= PASS else N - PASS + n      # warm-up = the last pass again
        k = i // PASS
        t = i - k * PASS
        lfsr = lfsr_step(lfsr)
        noise = (lfsr & 0xFF) - 128
        thump = 0
        for j in range(3):
            a1, a2, kick = RES[j]
            y = asr(a1 * y1[j] - a2 * y2[j], 14) + (kick if t == 0 else 0)
            y2[j], y1[j] = y1[j], y
            thump += y
        thump = asr(thump, 4)
        # the slap: noise band-passed (a fast low-pass minus a slow one), enveloped
        lp1 += asr((noise << 4) - lp1, 1)                # ~ 1.2 kHz
        lp2 += asr(lp1 - lp2, 2)                         # ~ 250 Hz
        band = asr(lp1 - lp2, 2)
        ts = t - SLAP_AT
        env = 0 if ts < 0 or ts >= 150 else (ts * 32 if ts < 8 else 256 - asr((ts - 8) * 461, 8))
        slap = asr(asr(band * env, 7) * STRENGTH[k], 7)
        rum += asr((noise << 4) - rum, 6)
        rm = asr(rum, 4)
        peaks = [max(peaks[0], abs(thump)), max(peaks[1], abs(slap)), max(peaks[2], abs(rm))]
        if n >= PASS:
            raw.append(thump + slap + rm)
    peak = max(abs(v) for v in raw) or 1
    scale = (PEAK << 8) // peak
    out = [max(-128, min(127, asr(v * scale, 8))) for v in raw]
    if verbose:
        print(f"raw peak {peak}, scale {scale}; component peaks thump/slap/rumble {peaks}")
    return out, peak


OS4 = 2                      # v4: the loop is stored at this many times the logic's rate
PASS4 = PASS * OS4           # 2400 samples a blade pass as stored
PASSES4 = 2
N4 = PASS4 * PASSES4         # 4800 bytes of chip RAM, as v3's
A1_4 = (32722, 32614, 32396)   # 2 r cos w, Q14, for 12003 Hz (q14(f0, tau, 12003))
A2_4 = (16352, 16306, 16248)   # r^2, Q14
KICK_4 = (750, 1150, 600)
STRENGTH_4 = (70, 60)


def make_v4(verbose=False):
    """exactly logicRotorSynth()'s integer loop (logic.c): a warm-up pass (the last pass again), then the loop"""
    lfsr = 0x1D2B
    y1 = [0, 0, 0]
    y2 = [0, 0, 0]
    lp1 = lp1b = lp2 = rum = 0
    raw = []
    for run in range(PASSES4 + 1):
        k = run - 1 if run else PASSES4 - 1
        for t in range(PASS4):
            lfsr = lfsr_step(lfsr)
            noise = (lfsr & 0xFF) - 128
            thump = 0
            for j in range(3):
                y = asr(A1_4[j] * y1[j] - A2_4[j] * y2[j], 14) + (KICK_4[j] if t == 0 else 0)
                y2[j], y1[j] = y1[j], y
                thump += y
            thump = asr(thump, 4)
            lp1 += asr((noise * 16 - lp1) * 40, 8)
            lp1b += asr((lp1 - lp1b) * 40, 8)
            lp2 += asr((lp1b - lp2) * 12, 8)
            band = asr(lp1b - lp2, 2)
            ts = t - 20
            env = 0 if ts < 0 or ts >= 300 else (ts * 16 if ts < 16 else 256 - asr((ts - 16) * 230, 8))
            slap = asr(asr(band * env, 7) * STRENGTH_4[k], 7)
            rum += asr(noise * 16 - rum, 7)
            if run:
                raw.append(thump + slap + asr(rum, 4))
    peak = max(abs(v) for v in raw) or 1
    scale = (126 << 8) // peak
    out = [max(-128, min(127, asr(v * scale, 8))) for v in raw]
    if verbose:
        print(f"v4 raw peak {peak}, scale {scale}, loop peak {max(abs(v) for v in out)}")
    return out, peak


def check_against_c():
    """compile logic.c (+ hiscore.c) with a host stub, dump logicRotorSynth's bytes, compare with make_v4()"""
    src = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "src")
    prog = os.path.join(os.path.dirname(os.path.abspath(__file__)), "rotor_dump.c")
    exe = "/tmp/rotor_dump_check"
    subprocess.run(["gcc", "-I", src, "-o", exe, prog, os.path.join(src, "logic.c"), os.path.join(src, "hiscore.c")], check=True)
    c = [v - 256 if v > 127 else v for v in subprocess.run([exe], capture_output=True, check=True).stdout]
    py, _ = make_v4()
    same = c == py
    print("logicRotorSynth (C) and make_v4 (Python):", "identical, %d bytes" % len(c) if same else "DIFFER at %d" % next(i for i, (a, b) in enumerate(zip(c, py)) if a != b))
    return 0 if same else 1


def make_old():
    """the loop commit 6ab2ff8 made (3008 samples, period 428), for comparison"""
    pass_, passes = 752, 4
    strength = (128, 112, 124, 104)
    lfsr = 0x1D2B
    y1 = y2 = lp1 = lp2 = rum = 0
    raw = []
    for n in range(2 * pass_ * passes):
        i = n % (pass_ * passes)
        k = i // pass_
        t = i - k * pass_
        lfsr = lfsr_step(lfsr)
        noise = (lfsr & 0xFF) - 128
        y = asr(32617 * y1 - 16285 * y2, 14) + (750 if t == 0 else 0)
        y2, y1 = y1, y
        lp1 += asr((noise << 4) - lp1, 1)
        lp2 += asr(lp1 - lp2, 4)
        band = asr(lp1 - lp2, 4)
        env = t * 16 if t < 16 else (256 - asr(t - 16, 1) if t < 528 else 0)
        env = max(env, 0)
        slap = asr(asr(band * env, 7) * strength[k], 7)
        rum += asr((noise << 4) - rum, 5)
        if n >= pass_ * passes:
            raw.append(asr(y, 6) + slap + asr(rum, 5))
    peak = max(abs(v) for v in raw) or 1
    scale = (118 << 8) // peak
    return [max(-128, min(127, asr(v * scale, 8))) for v in raw]


def band_shares(loop, rate, np):
    a = np.asarray(loop, float)
    a -= a.mean()
    spec = np.abs(np.fft.rfft(a)) ** 2
    f = np.fft.rfftfreq(len(a), 1.0 / rate)
    tot = spec.sum()
    edges = [(0, 40), (40, 100), (100, 250), (250, 1000), (1000, 2000), (2000, 1e9)]
    return [100 * spec[(f >= lo) & (f < hi)].sum() / tot for lo, hi in edges]


def describe(name, loop, period, np):
    rate = CLOCK / period
    a = np.asarray(loop, float)
    sh = band_shares(loop, rate, np)
    print(f"{name}: {len(loop)} bytes, period {period} = {rate:.0f} Hz, peak {abs(a).max():.0f}, rms {a.std():.1f}; "
          f"energy <40 Hz {sh[0]:.0f}%, 40-100 {sh[1]:.0f}%, 100-250 {sh[2]:.0f}%, "
          f"250-1k {sh[3]:.0f}%, 1-2k {sh[4]:.0f}%, >2k {sh[5]:.0f}%")


def write_wav16(path, data, rate=44100):
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(struct.pack("<%dh" % len(data), *data))


def render(loop, sched, rate=44100, os_=1):
    """Paula playing the loop: a sample-and-hold of the 8-bit bytes at the
    period of the moment (sched = [(period, volume)] a frame, 50 a second), times
    volume / 64; the DMA position runs on across frames. os_: the loop's oversampling
    (v4: 2, Paula's period is the logic's (period + 1) // 2, as logicRotorHwPeriod)."""
    out = []
    pos = 0.0
    n = len(loop)
    for per, vol in sched:
        if os_ > 1:
            per = (per + os_ // 2) // os_
        step = (CLOCK / max(per, 124)) / rate
        for _ in range(rate // 50):
            out.append(int(loop[int(pos) % n] * 256 * vol / 64))
            pos += step
    return out


def schedule(kind):
    """per-frame (period, volume) of a scenario, from the game's own rules
    (rotor_sched.c: logic.c driven with scripted input)"""
    r = subprocess.run(["/tmp/rotor_sched", kind], capture_output=True, text=True, check=True)
    return [tuple(int(x) for x in line.split()) for line in r.stdout.split("\n") if line.strip()]


def old_schedule(kind):
    """the old game's rule on the same flight: period 428 - speed/4 (landed
    452), volume 38 + speed/4 (+4 climbing), landed 24. rotor_sched's
    '<kind>-old' columns are speed, climbing (1/0) and landed (1/0)"""
    out = []
    for sp, climb, landed in schedule(kind + "-old"):
        if landed:
            out.append((452, 24))
        else:
            out.append((428 - (sp >> 2), 38 + (sp >> 2) + (4 if climb else 0)))
    return out


def main():
    if "--check" in sys.argv:
        sys.exit(check_against_c())
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    outdir = args[0] if args else os.path.join(os.path.dirname(__file__), "..", "..", "build", "rotor")
    os.makedirs(outdir, exist_ok=True)
    new, _ = make_new(verbose=True)
    old = make_old()
    try:
        import numpy as np
    except ImportError:
        np = None
    if np:
        describe("old", old, 428, np)
        describe("new (hover)", new, HOVER_PERIOD, np)
        describe("new (full speed)", new, 455, np)
    print("loop ends:", new[-3:], "starts:", new[:3])
    with open(os.path.join(outdir, "rotor_loop.bin"), "wb") as f:
        f.write(bytes((v & 0xFF) for v in new))
    if "--render" in sys.argv:
        for kind in ("hover", "cruise", "full", "takeoff"):
            write_wav16(os.path.join(outdir, f"rotor-new-{kind}.wav"), render(new, schedule(kind)))
            write_wav16(os.path.join(outdir, f"rotor-old-{kind}.wav"), render(old, old_schedule(kind)))
    print("wrote", outdir)


if __name__ == "__main__":
    main()
