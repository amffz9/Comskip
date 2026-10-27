"""Score Comskip .txt cut lists against .labels.csv files.

usage: score_labels.py LABELS_CSV CUTLIST_TXT [FPS]
A break counts as found when at least half of it is cut. "sponsor" rows are
neither counted as missed ads nor as programme cut.
"""
import sys

def load_labels(path):
    rows = []
    for line in open(path):
        if line.strip() and not line.startswith("#"):
            p = line.split(",")
            rows.append((float(p[0]), float(p[1]), p[2].strip()))
    return rows

def load_cuts(path, fps):
    cuts = []
    for n, line in enumerate(open(path)):
        p = line.split()
        if n >= 2 and len(p) >= 2 and p[0].isdigit():
            s, e = int(p[0]) / fps, int(p[1]) / fps
            if e - s > 1:
                cuts.append((s, e))
    return cuts

def overlap(a, b):
    return sum(max(0, min(e1, e2) - max(s1, s2)) for s1, e1 in a for s2, e2 in b)

def score(labels, cuts):
    ads = [(s, e) for s, e, k in labels if k == "ad"]
    sponsors = [(s, e) for s, e, k in labels if k == "sponsor"]
    found = sum(1 for a in ads if overlap([a], cuts) >= 0.5 * (a[1] - a[0]))
    ad_s = sum(e - s for s, e in ads)
    missed = ad_s - overlap(ads, cuts)
    cut_total = sum(e - s for s, e in cuts)
    show_cut = cut_total - overlap(ads, cuts) - overlap(sponsors, cuts)
    return found, len(ads), missed, show_cut

if __name__ == "__main__":
    fps = float(sys.argv[3]) if len(sys.argv) > 3 else 29.97
    f, n, m, c = score(load_labels(sys.argv[1]), load_cuts(sys.argv[2], fps))
    print(f"breaks {f}/{n}  ads missed {m:5.0f} s  programme cut {c:5.0f} s")
