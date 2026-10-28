"""Compare a comskip-fingerprint code stream with fpcalc's raw fingerprint.

usage: compare_fpcalc.py CODES_JSON FPCALC_OUTPUT
FPCALC_OUTPUT is `fpcalc -raw -length 0 REC`. fpcalc decodes and resamples on
its own, so its codes may sit a few positions off and differ in a few bits; the
offset (within +-40 codes) with the most codes within 2 bits is used.
"""
import json
import sys

stream = json.load(open(sys.argv[1]))
ours = [code for run in stream["runs"] for code in run["codes"]]
theirs = []
for line in open(sys.argv[2]):
    if line.startswith("FINGERPRINT="):
        theirs = [int(x) & 0xFFFFFFFF for x in line.split("=", 1)[1].split(",")]


def pairs(shift):  # ours[i] against theirs[i + shift]
    lo, hi = max(0, -shift), min(len(ours), len(theirs) - shift)
    return [(ours[i], theirs[i + shift]) for i in range(lo, hi)]


def close(shift, bits=2):
    p = pairs(shift)
    return sum(bin(a ^ b).count("1") <= bits for a, b in p) / max(1, len(p))


shift = max(range(-40, 41), key=close)
p = pairs(shift)
counts = [0] * 33
for a, b in p:
    counts[bin(a ^ b).count("1")] += 1
print(f"codes: ours {len(ours)}, fpcalc {len(theirs)}; offset {shift} codes; compared {len(p)}")
print(f"equal {100 * counts[0] / len(p):.2f}%, within 2 bits {100 * sum(counts[:3]) / len(p):.2f}%, "
      f"within 10 bits {100 * sum(counts[:11]) / len(p):.2f}%")
