#!/usr/bin/env python3
"""The sea's drift table (SPEC 11, "Sea animation"): prints `s_pSeaDrift` for src/logic.c.

Line j of the sea (0-27) drifts A_j sin(2 pi (phase - j) / 32) quarter pixels at phase 0-31,
A_j = (28 + 3 j) / 7 (the camera's own 1/z ratio times 4 px, in quarters) times a ramp that is 0 above
line SWELL_FROM (the far water stays still) and grows over the next RAMP lines to 1; the bare rows 0
and 27 do not move. tests/unit/test_logic.c (testSeaDrift) checks the table against this formula.
Usage: python make_sea_drift.py   (paste the output over the table in logic.c)
"""
import math

PHASES, LINES = 32, 28
SWELL_FROM, RAMP = 12, 6   # = SEA_SWELL_FROM in src/logic.h

def drift(p, j):
    if j in (0, LINES - 1):
        return 0
    if j < SWELL_FROM:
        return 0
    a = (28 + 3 * j) / 7.0 * min(1.0, (j - SWELL_FROM + 1) / RAMP)
    v = a * math.sin(2 * math.pi * (p - j) / PHASES)
    return int(math.floor(v + 0.5))

if __name__ == "__main__":
    print("static const int8_t s_pSeaDrift[SEA_PHASES][SEA_LINES] = {")
    for p in range(PHASES):
        print("\t{" + ", ".join("%d" % drift(p, j) for j in range(LINES)) + "},")
    print("};")
