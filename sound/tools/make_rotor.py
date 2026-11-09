#!/usr/bin/env python3
"""ROTOR RESCUE's rotor: a preview of the loop main.c's rotorCreate() makes at
startup, with the same integer arithmetic (keep the two in step), as WAVs to
listen to, and a comparison with the old loop.

    python3 sound/tools/make_rotor.py [OUTDIR]

The sound of a two-blade rotor: on every blade pass a low damped thump (the
"whop", a resonator at ~75 Hz) and the slap (a burst of band-passed noise,
~300-1200 Hz, fast attack); a quiet low rumble between. Four passes a loop,
each a little different (its own noise, its own strength), so the loop doesn't
buzz; ~11 passes a second at period 428 (8287 Hz). The game nudges the pitch
only a little with speed (rotor RPM is nearly constant) and lets the
loudness carry the effort.
"""
import math
import os
import struct
import sys
import wave

RATE = 8287                  # period 428
PASS = 752                   # samples a blade pass: ~11 a second
PASSES = 4
N = PASS * PASSES            # 3008 bytes of chip RAM
STRENGTH = (128, 112, 124, 104)   # each pass's slap, /128
KICK = 750                   # the thump's kick
RUM_SHIFT = 5                # the rumble's level
PEAKS = [0, 0, 0]

# the thump's resonator, Q14: f0 75 Hz, decay ~40 ms
A1 = 32617                   # 2 r cos(w)
A2 = 16285                   # r^2


def lfsr_step(s):
    lsb = s & 1
    s >>= 1
    if lsb:
        s ^= 0xB400
    return s


def to_s16(v):               # C's WORD wrap
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def asr(v, n):               # arithmetic shift right, as the 68000 (and C on it) does
    return v >> n


def make_new():
    """exactly rotorCreate()'s integer loop; two loops run, the 2nd kept (the
    resonator's tail from the loop before is in it: it loops seamlessly)"""
    lfsr = 0x1D2B
    y1 = y2 = 0              # resonator state (LONG)
    lp1 = lp2 = 0            # slap band-pass: two one-poles (WORD << 4 precision in LONG)
    rum = 0
    out = []
    raw = []
    for n in range(2 * N):
        i = n % N
        k = i // PASS
        t = i - k * PASS
        lfsr = lfsr_step(lfsr)
        noise = (lfsr & 0xFF) - 128              # -128..127
        # the thump: kick the resonator at the pass start
        x = KICK if t == 0 else 0
        y = asr(A1 * y1 - A2 * y2, 14) + x
        y2, y1 = y1, y
        # the slap: noise band-passed (a fast low-pass minus a slow one), enveloped
        lp1 += asr((noise << 4) - lp1, 1)        # ~1.2 kHz
        lp2 += asr(lp1 - lp2, 4)                 # ~ 300 Hz
        band = asr(lp1 - lp2, 4)
        env = t * 16 if t < 16 else (256 - asr(t - 16, 1) if t < 528 else 0)
        if env < 0:
            env = 0
        slap = asr(asr(band * env, 7) * STRENGTH[k], 7)   # as the C: two 16 x 16 steps
        # the rumble: noise low-passed hard, quiet
        rum += asr((noise << 4) - rum, 5)
        th = asr(y, 6)
        rm = asr(rum, RUM_SHIFT)
        PEAKS[0] = max(PEAKS[0], abs(th)); PEAKS[1] = max(PEAKS[1], abs(slap)); PEAKS[2] = max(PEAKS[2], abs(rm))
        v = th + slap + rm
        raw.append(v)
    peak = max(abs(v) for v in raw[N:]) or 1
    scale = (118 << 8) // peak                   # as the C: one factor, then a multiply and a shift
    for v in raw[N:]:
        out.append(max(-128, min(127, asr(v * scale, 8))))
    return out, peak


def make_old():
    """the loop the game had (1200 samples, played at period ~276-300)"""
    lfsr = 0x1D2B
    low = low2 = 0
    out = []
    for i in range(1200):
        lfsr = lfsr_step(lfsr)
        noise = (lfsr & 0xFF) - 128
        low = to_s16(low + asr(noise - low, 2))
        low2 = to_s16(low2 + asr(low - low2, 2))
        ph = i if i < 600 else i - 600
        env = ph * 3 if ph < 40 else 120 - asr(ph - 40, 2)
        if env < 8:
            env = 8
        v = asr(low2 * env, 5)
        out.append(max(-128, min(127, v)))
    return out


def write_wav(path, loop, rate, seconds=4.0):
    n = int(rate * seconds)
    data = bytes((loop[i % len(loop)] & 0xFF) ^ 0x80 for i in range(n))   # 8-bit WAV is unsigned
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(1)
        w.setframerate(rate)
        w.writeframes(data)


def band_energy(loop, rate):
    """shares of the energy below 250 Hz, 250 Hz-2 kHz and above 2 kHz (numpy FFT)"""
    import numpy as np
    a = np.asarray(loop, float)
    a -= a.mean()
    spec = np.abs(np.fft.rfft(a)) ** 2
    f = np.fft.rfftfreq(len(a), 1.0 / rate)
    tot = spec.sum()
    return spec[f < 250].sum() / tot, spec[(f >= 250) & (f <= 2000)].sum() / tot, spec[f > 2000].sum() / tot


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "..", "build", "rotor")
    os.makedirs(outdir, exist_ok=True)
    new, peak = make_new()
    old = make_old()
    write_wav(os.path.join(outdir, "rotor-new-hover.wav"), new, RATE)
    write_wav(os.path.join(outdir, "rotor-new-fast.wav"), new, 3546895 // 416)
    write_wav(os.path.join(outdir, "rotor-old-hover.wav"), old, 3546895 // 276)
    write_wav(os.path.join(outdir, "rotor-old-fast.wav"), old, 3546895 // 228)
    print(f"new loop: {len(new)} bytes, raw peak {peak}; component peaks thump/slap/rumble {PEAKS}")
    for name, loop, rate in (("old", old, 3546895 // 276), ("new", new, RATE)):
        lo, mid, hi = band_energy(loop, rate)
        print(f"{name}: energy below 250 Hz {100 * lo:.0f}%, 250 Hz-2 kHz {100 * mid:.0f}%, above 2 kHz {100 * hi:.0f}%")
    print("wrote", outdir)


if __name__ == "__main__":
    main()
