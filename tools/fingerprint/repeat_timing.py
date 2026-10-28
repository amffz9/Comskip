"""Check where a repeat lands in time against the audio itself.

usage: repeat_timing.py CODES_JSON PCM FROM TO
CODES_JSON is a comskip-fingerprint code stream with one run; PCM the 11025 Hz
mono s16le audio it was made from. Finds the repeat whose later copy lies in
FROM-TO seconds (the offset with the most exact-value seeds, as in repeats.py),
extends it while codes differ in at most 10 bits, then measures the two copies'
true distance by cross-correlating the audio, and prints both.
"""
import json
import sys
from array import array
from collections import Counter, defaultdict

RATE = 11025
stream = json.load(open(sys.argv[1]))
(run,) = stream["runs"]
codes, period, origin = run["codes"], stream["codePeriod"], run["start"]
pcm_path, window = sys.argv[2], (float(sys.argv[3]), float(sys.argv[4]))
min_gap = int(20 / period)

index = defaultdict(list)
for i, value in enumerate(codes):
    index[value].append(i)
seeds = Counter()
for positions in index.values():
    if 2 <= len(positions) <= 30:
        for x, first in enumerate(positions):
            for later in positions[x + 1:]:
                if later - first >= min_gap and window[0] <= origin + later * period < window[1]:
                    seeds[later - first] += 1
if not seeds or seeds.most_common(1)[0][1] < 3:
    sys.exit("no repeat with at least 3 seeds ends in that window")
(offset, count), = seeds.most_common(1)
anchors = sorted(later - offset for positions in index.values() if 2 <= len(positions) <= 30
                 for later in positions
                 if later - offset in positions and window[0] <= origin + later * period < window[1])
a = b = anchors[len(anchors) // 2]
agree = lambda i: 0 <= i and i + offset < len(codes) and bin(codes[i] ^ codes[i + offset]).count("1") <= 10
while agree(a - 1) or agree(a - 2):
    a -= 1
while agree(b + 1) or agree(b + 2):
    b += 1
b += 1


def samples(start, count):
    with open(pcm_path, "rb") as source:
        source.seek(2 * start)
        values = array("h")
        values.frombytes(source.read(2 * count))
    return values


# Cross-correlate 4000 samples from the middle of the first copy with the second
# copy at every lag within 0.25 s of the codes' offset.
size, search = 4000, RATE // 4
first = int((a + b) / 2 * period * RATE)
reference = samples(first, size)
coarse = round(offset * period * RATE)
target = samples(first + coarse - search, size + 2 * search)
energy = sum(v * v for v in reference) ** 0.5
best_lag, best_score = 0, -2.0
window_energy = sum(v * v for v in target[:size])
for shift in range(2 * search + 1):
    if shift:
        window_energy += target[shift + size - 1] ** 2 - target[shift - 1] ** 2
    dot = sum(r * t for r, t in zip(reference, target[shift:shift + size]))
    score = dot / (energy * max(window_energy, 1) ** 0.5 + 1e-9)
    if score > best_score:
        best_lag, best_score = coarse - search + shift, score
codes_lag, audio_lag = offset * period, best_lag / RATE
print(f"seeds {count} at offset {offset} codes; copies {origin + a * period:.3f}-{origin + b * period:.3f} s "
      f"and {origin + (a + offset) * period:.3f}-{origin + (b + offset) * period:.3f} s ({(b - a) * period:.1f} s)")
print(f"distance from codes {codes_lag:.3f} s, from audio {audio_lag:.4f} s (correlation {best_score:.3f}); "
      f"difference {codes_lag - audio_lag:+.3f} s; with a 0.1239 s period {offset * 0.1239 - audio_lag:+.3f} s")
