"""Find audio stretches that repeat within one recording and compare them with labels.

usage: repeats.py FPCALC_OUTPUT DURATION_S LABELS_CSV [BITS [MIN_LEN]]
FPCALC_OUTPUT may instead be a comskip-fingerprint code stream (.json) with one
run; its code period is exact, and DURATION_S is then ignored.
"""
import json
import sys
from collections import defaultdict

fp_path, duration, labels_path = sys.argv[1], float(sys.argv[2]), sys.argv[3]
fp = []
if fp_path.endswith(".json"):
    stream = json.load(open(fp_path))
    (run,) = stream["runs"]
    fp, hop, origin = run["codes"], stream["codePeriod"], run["start"]
else:
    for line in open(fp_path):
        if line.startswith("FINGERPRINT="):
            fp = [int(x) & 0xFFFFFFFF for x in line.split("=", 1)[1].split(",")]
    # duration / codes is not the code period (1365/11025 s); it drifts.
    hop, origin = duration / len(fp), 0.0
MIN_GAP = int(20 / hop)        # partner must be at least 20 s away
MIN_LEN = float(sys.argv[5]) if len(sys.argv) > 5 else 3.0
BITS = int(sys.argv[4]) if len(sys.argv) > 4 else 10

index = defaultdict(list)
for i, v in enumerate(fp):
    index[v].append(i)

# Exact-value seeds grouped by offset d.
seeds = defaultdict(list)
for v, pos in index.items():
    if 2 <= len(pos) <= 30:
        for x in range(len(pos)):
            for y in range(x + 1, len(pos)):
                d = pos[y] - pos[x]
                if d >= MIN_GAP:
                    seeds[d].append(pos[x])

def pop(x):
    return bin(x).count("1")

found = []  # (a_start, a_end, d) in items
for d, starts in seeds.items():
    starts.sort()
    # cluster seeds for this offset
    cl = [starts[0]]
    groups = []
    for s in starts[1:]:
        if s - cl[-1] <= int(2 / hop):
            cl.append(s)
        else:
            groups.append(cl); cl = [s]
    groups.append(cl)
    for g in groups:
        if len(g) < 2:
            continue
        lo, hi = max(0, g[0] - int(60 / hop)), min(len(fp) - d, g[-1] + int(60 / hop))
        good = [pop(fp[i] ^ fp[i + d]) <= BITS for i in range(lo, hi)]
        # smooth: an item counts if >= 5 of the 8 around it match
        sm = [sum(good[max(0, k - 4):k + 4]) >= 5 for k in range(len(good))]
        k0 = g[0] - lo
        if k0 >= len(sm) or not sm[k0]:
            continue
        a = k0
        while a > 0 and sm[a - 1]:
            a -= 1
        b = k0
        while b + 1 < len(sm) and sm[b + 1]:
            b += 1
        if (b - a + 1) * hop >= MIN_LEN:
            found.append((lo + a, lo + b + 1, d))

# Every repeated interval (both copies), merged.
iv = []
for a, b, d in set(found):
    iv.append((origin + a * hop, origin + b * hop, origin + (a + d) * hop))
    iv.append((origin + (a + d) * hop, origin + (b + d) * hop, origin + a * hop))
iv.sort()
merged = []
for s, e, partner in iv:
    if merged and s <= merged[-1][1] + 1:
        m = merged[-1]
        merged[-1] = (m[0], max(m[1], e), m[2] | {round(partner)})
    else:
        merged.append((s, e, {round(partner)}))

labels = []
for line in open(labels_path):
    if line.strip() and not line.startswith("#"):
        p = line.split(",")
        labels.append((float(p[0]), float(p[1]), p[2]))

def overlap(s, e, kind):
    return sum(max(0, min(e, le) - max(s, ls)) for ls, le, k in labels if k == kind)

print(f"items {len(fp)}, hop {hop:.4f} s, repeated intervals {len(merged)}")
print("\nBreak coverage (seconds of each ad break that repeat somewhere):")
for ls, le, k in labels:
    if k != "ad":
        continue
    cov = sum(max(0, min(e, le) - max(s, ls)) for s, e, _ in merged)
    print(f"  {ls:6.0f}-{le:6.0f}  {cov:5.0f} / {le - ls:4.0f} s  ({100 * cov / (le - ls):3.0f}%)")
print("\nRepeats outside ad breaks (candidate false positives):")
for s, e, partners in merged:
    ad = overlap(s, e, "ad")
    if ad < 0.5 * (e - s):
        sp = overlap(s, e, "sponsor")
        tag = "sponsor" if sp >= 0.5 * (e - s) else "PROGRAMME"
        print(f"  {s:7.1f}-{e:7.1f} ({e - s:5.1f} s) {tag:9} partners {sorted(partners)[:5]}")
print("\nAll repeated intervals:")
for s, e, partners in merged:
    print(f"  {s:7.1f}-{e:7.1f} ({e - s:5.1f} s) partners {sorted(partners)[:6]}")
