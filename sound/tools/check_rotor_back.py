#!/usr/bin/env python3
"""Is the rotor back after the effects ended? (SPEC section 9, "rotor channel claim")

    python3 sound/tools/check_rotor_back.py build/agk/a500/hqsound/audio.wav [FROM TO]

Reads an `agk test`/`agk run` audio.wav and its .marks (default marks `after` .. `after_end` of
tests/hqsound.agk, taken a couple of seconds after three effects started in one picture) and checks every
0.2 s window between them is the rotor: low (the 30-150 Hz body carries the window) and not bright. Fails
(exit 1) when a window has more than 35 % of its power above 1 kHz or less than 30 % between 30 and 150 Hz:
that is a Paula channel looping an effect's samples (the "saved" arpeggio, 530-1320 Hz) where the rotor's
wave should be. Measured: the build before the fix 36-39 % above 1 kHz and 0-2 % in the body; the fixed
build 1-7 % and 88-99 %.
The harness's expect-sound can't tell the two apart (both are audible), which is why the scenario alone
was not red on the bug: shown failing on the build before the fix (peak at 530/795 Hz) before it was trusted. Needs numpy.
"""
import sys
import wave

import numpy as np

BRIGHT_MAX = 0.35      # share of the power above 1 kHz (the rotor: 1-7 %; a looped "saved": 36-39 %)
BODY_MIN = 0.30        # share between 30 and 150 Hz (the rotor's body)


def load(path):
    w = wave.open(path)
    x = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2").astype(float)
    if w.getnchannels() > 1:
        x = x.reshape(-1, w.getnchannels()).mean(axis=1)
    return x, w.getframerate()


def main(argv):
    path = argv[1]
    x, sr = load(path)
    marks = {}
    for line in open(path + ".marks"):
        k, v = line.split()
        marks[k] = int(v)
    a, b = (argv[2], argv[3]) if len(argv) > 3 else ("after", "after_end")
    step = int(sr * 0.2)
    bad = 0
    n = 0
    for i in range(marks[a], marks[b] - step + 1, step):
        c = x[i:i + step]
        sp = np.abs(np.fft.rfft(c * np.hanning(len(c)))) ** 2
        f = np.fft.rfftfreq(len(c), 1 / sr)
        tot = sp[1:].sum() or 1.0
        bright = sp[f > 1000].sum() / tot
        body = sp[(f >= 30) & (f <= 150)].sum() / tot
        ok = bright <= BRIGHT_MAX and body >= BODY_MIN
        n += 1
        bad += not ok
        print("%s %.1f s: >1 kHz %4.0f %%, 30-150 Hz %4.0f %%" % ("ok  " if ok else "FAIL", i / sr, bright * 100, body * 100))
    print("%d of %d windows are not the rotor" % (bad, n))
    return 1 if bad or not n else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
