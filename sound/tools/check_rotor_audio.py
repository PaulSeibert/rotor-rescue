#!/usr/bin/env python3
"""Does the rotor in a recording sound deep, or tinny? (SPEC section 9, rotor v4)

    python3 sound/tools/check_rotor_audio.py build/agk-run/a500/rotor/audio.wav [FROM TO]

Reads an `agk run` audio.wav (and its .marks) and measures the hover (marks `hover` .. `hover_end` of
tests/rotor.agk; or give two mark names) the way an ear counts it: the power spectrum A-weighted
(IEC 61672), as shares of the total. Fails (exit 1) when
  - 250 Hz-1 kHz is more than 20 % (v3's hover in vAmiga: 26.9 %, the slap; v4: 10.2 %), or
  - 1 kHz and up is more than 4 % (v3: 10.9 %, slap tails and the sample-rate lines; v4: 1.6 %), or
  - a steady line between 3 and 9 kHz stands more than 15 dB over its surroundings (Paula's sample-rate
    line, a whistle where ears are keenest: v3's sat at 6 kHz hovering, 19 dB; v4's main one is at 12 kHz,
    outside, and what is left in 3-9 kHz is 11 dB).
Needs numpy. This is the guard behind unit test testRotorLoopIsDeepAndSoft: that one checks the loop's
samples, this one checks what Paula, the emulator and the pitch made of it. It was shown failing on the
v3 build's hover recording before it was trusted.
"""
import sys
import wave

import numpy as np


def load(path):
    w = wave.open(path)
    x = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").astype(float) / 32768
    ch = w.getnchannels()
    if ch > 1:
        x = x.reshape(-1, ch).mean(axis=1)
    return x, w.getframerate()


def marks(path):
    m = {}
    try:
        for line in open(path + ".marks"):
            a = line.split()
            if len(a) == 2:
                m[a[0]] = int(a[1])
    except OSError:
        pass
    return m


def a_db(f):
    f2 = np.maximum(f, 1.0) ** 2
    ra = (12194 ** 2 * f2 ** 2) / ((f2 + 20.6 ** 2) * np.sqrt((f2 + 107.7 ** 2) * (f2 + 737.9 ** 2)) * (f2 + 12194 ** 2))
    return 20 * np.log10(ra)


def spectrum(x, sr, seg=8192):
    x = x - x.mean()
    acc, n = 0, 0
    win = np.hanning(seg)
    for i in range(0, len(x) - seg, seg // 2):
        acc = acc + np.abs(np.fft.rfft(x[i:i + seg] * win)) ** 2
        n += 1
    return np.fft.rfftfreq(seg, 1 / sr), acc / max(n, 1)


def main():
    path = sys.argv[1]
    x, sr = load(path)
    m = marks(path)
    a, b = ("hover", "hover_end") if len(sys.argv) < 4 else (sys.argv[2], sys.argv[3])
    if a in m and b in m:
        x = x[m[a]:m[b]]
    f, p = spectrum(x, sr)
    pa = p * 10 ** (a_db(f) / 10)
    tot = pa[f >= 20].sum()
    mid = 100 * pa[(f >= 250) & (f < 1000)].sum() / tot
    high = 100 * pa[f >= 1000].sum() / tot
    # the strongest narrow line between 3 and 9 kHz against the median of its neighbourhood (+-300 Hz)
    sel = np.where((f > 3000) & (f < 9000))[0]
    lines = 0.0
    lf = 0.0
    step = f[1] - f[0]
    half = int(300 / step)
    pdb = 10 * np.log10(p + 1e-30)
    for i in sel[::3]:
        d = pdb[i] - np.median(pdb[i - half:i + half])
        if d > lines:
            lines, lf = d, f[i]
    ok = mid <= 20 and high <= 4 and lines <= 15
    print(f"{path}: 250 Hz-1 kHz {mid:.1f} % (max 20), 1 kHz up {high:.1f} % (max 4), "
          f"strongest line 3-9 kHz {lines:.0f} dB over its surroundings at {lf:.0f} Hz (max 15) -> {'PASS' if ok else 'FAIL'}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
