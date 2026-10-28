"""Offline Harness (docs/fingerprint-plan.md, step F4): replay labelled recordings
through comskip-fingerprint with a chosen library and score the marks.

usage: harness.py TOOL WORK PCM_DIR EVIDENCE_DIR [--labels DIR] [--plan PLAN]...

TOOL is comskip-fingerprint (a path, or a name on PATH). For each recording
NAME, PCM_DIR holds NAME.s16 and EVIDENCE_DIR NAME.evidence.json, made from the
same file (one generation, so proxy seconds are capture seconds):
  ffmpeg -i NAME.ts -map 0:a:0 -vn -ac 1 -ar 11025 -f s16le NAME.s16
  comskip --ini=TV.ini --output=DIR NAME.ts   (TV ini, detect_method=107,
                                              output_evidence=1)
Each --plan is a library: names separated by commas, passed in that order into
one new store; the first name may be "empty" for a store with nothing in it
(then each later name gets a store of its own). After every pass each capture
seen so far is scored from its latest MarksFile, so corrections count, against
Comskip-107's commercial blocks from the same evidence. Scores are in seconds
(score_labels.py's rules, plus false breaks: cuts less than half on ad rows).
Prints a table per plan and writes WORK/results.json.
"""
import argparse
import json
import shutil
import sqlite3
import struct
import subprocess
import sys
from pathlib import Path

RATE = 11025
# Context for each labelled recording: channel, series key, programme window.
# Football windows are the whole broadcast slot (the recordings are its first
# 90 min); air starts are only for ordering (g5 aired before g4).
CONTEXTS = {
    "g5": ("NBC", "nfl:nbc", "NFL Sunday Night Football", 12600, 1760832000),
    "g4": ("CBS", "nfl:cbs", "NFL on CBS", 10800, 1761411600),
    "faaea10a": (None, "ncaaf:replay", "College football replay", 10800, 1761000000),
    "77483aca": ("CBS", "nfl:cbs", "NFL on CBS", 12600, 1761500000),
    "elsbeth303": ("CBS", "series:elsbeth", "Elsbeth", 3600, 1761600000),
    "nash107": ("ABC", "series:9-1-1-nashville", "9-1-1: Nashville", 3600, 1761700000),
}
NON_AD = ("sponsor", "in-programme ad", "intro", "credits")


def labels_of(path):
    rows = []
    for line in open(path):
        if line.strip() and not line.startswith("#"):
            p = line.split(",")
            rows.append((float(p[0]), float(p[1]), p[2].strip()))
    return rows


def overlap(a, b):
    return sum(max(0.0, min(e1, e2) - max(s1, s2)) for s1, e1 in a for s2, e2 in b)


def union(intervals, adjacent=0.1):
    """Merged intervals; ones less than a few frames apart are one (Comskip's
    adjacent blocks, as in its cut list)."""
    merged = []
    for s, e in sorted(intervals):
        if merged and s <= merged[-1][1] + adjacent:
            merged[-1] = (merged[-1][0], max(merged[-1][1], e))
        else:
            merged.append((s, e))
    return merged


def score(labels, cuts):
    """score_labels.py's rules in seconds, plus false breaks: cuts with less than
    half their length on ad rows (rows of the other kinds count against them)."""
    cuts = union(cuts)
    ads = [(s, e) for s, e, k in labels if k == "ad"]
    other = [(s, e) for s, e, k in labels if k != "ad"]
    found = sum(1 for a in ads if overlap([a], cuts) >= 0.5 * (a[1] - a[0]))
    missed = sum(e - s for s, e in ads) - overlap(ads, cuts)
    show_cut = sum(e - s for s, e in cuts) - overlap(ads, cuts) - overlap(other, cuts)
    false = [c for c in cuts if overlap([c], ads) < 0.5 * (c[1] - c[0])]
    kinds = {}
    for s, e, k in labels:
        if k != "ad":
            kinds[k] = kinds.get(k, 0) + overlap([(s, e)], cuts)
    return {"breaks": found, "of": len(ads), "missed": round(missed, 1), "programmeCut": round(show_cut, 1),
            "falseBreaks": len(false), "false": [[round(s, 1), round(e, 1)] for s, e in false],
            "kinds": {k: round(v, 1) for k, v in sorted(kinds.items())}}


def comskip_cuts(evidence):
    return union([(b["from"], b["to"]) for b in evidence["blocks"] if b["decision"] == "commercial"])


def stream_bytes(pcm):
    yield b"R" + struct.pack("<Id", 8, 0.0)
    with open(pcm, "rb") as source:
        while True:
            piece = source.read(2 * RATE)
            if not piece:
                break
            piece = piece[: len(piece) // 2 * 2]
            yield b"A" + struct.pack("<I", len(piece)) + piece
    yield b"E" + struct.pack("<I", 0)


class Harness:
    def __init__(self, args):
        self.tool = str(Path(args.tool).resolve()) if Path(args.tool).exists() else shutil.which(args.tool)
        self.work = Path(args.work).resolve()
        self.pcm = Path(args.pcm)
        self.evidence_dir = Path(args.evidence)
        self.labels = Path(args.labels)
        self.inputs = self.work / "inputs"
        self.inputs.mkdir(parents=True, exist_ok=True)
        for name, (channel, series, title, window, air) in CONTEXTS.items():
            context = {"channel": channel, "airStart": air, "source": "capture",
                       "programmes": [{"from": 0, "to": window, "title": title, "seriesKey": series}]}
            (self.inputs / f"{name}.context.json").write_text(json.dumps(context))
            source = self.evidence_dir / f"{name}.evidence.json"
            if source.exists():
                copy = json.loads(source.read_text())
                copy["captureId"] = name
                (self.inputs / f"{name}.evidence.json").write_text(json.dumps(copy))

    def evidence(self, name):
        return json.loads((self.inputs / f"{name}.evidence.json").read_text())

    def run_pass(self, store, name, seen, out):
        command = [self.tool, "pass", "--store", str(store), "--recording", name,
                   "--context", str(self.inputs / f"{name}.context.json"), "--out", str(out)]
        for other in seen + [name]:
            command += ["--evidence", str(self.inputs / f"{other}.evidence.json")]
        process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        for chunk in stream_bytes(self.pcm / f"{name}.s16"):
            process.stdin.write(chunk)
        process.stdin.close()
        output = process.stdout.read()
        errors = process.stderr.read()
        if process.wait() != 0:
            sys.exit(f"pass {name} failed ({process.returncode}): {errors.decode(errors='replace')}")
        return json.loads(output)

    def airing_coverage(self, store, name, labels):
        ads = [(s, e) for s, e, k in labels if k == "ad"]
        total = sum(e - s for s, e in ads)
        with sqlite3.connect(store) as db:
            rows = db.execute("SELECT a.start, a.\"end\", e.status FROM airing a JOIN entry e ON e.id = a.entry "
                              "WHERE a.recording = ?", (name,)).fetchall()
        every = union([(s, e) for s, e, _ in rows])
        marked = union([(s, e) for s, e, status in rows if status == "ad"])
        return {"repeatShare": round(overlap(ads, every) / total, 3), "adEntryShare": round(overlap(ads, marked) / total, 3),
                "repeatSeconds": round(sum(e - s for s, e in every), 1),
                "repeatOutsideAds": round(sum(e - s for s, e in every) - overlap(ads, every), 1)}

    def entry_counts(self, store):
        with sqlite3.connect(store) as db:
            return dict(db.execute("SELECT status, count(*) FROM entry GROUP BY status ORDER BY status").fetchall())

    def plan(self, spec):
        names = spec.split(",")
        steps = []
        if names[0] == "empty":
            for name in names[1:]:
                steps.append([name])
        else:
            steps.append(names)
        results = {"plan": spec, "passes": []}
        for group in steps:
            folder = self.work / spec.replace(",", "+") / "+".join(group)
            if folder.exists():
                shutil.rmtree(folder)
            folder.mkdir(parents=True)
            store = folder / "store.sqlite"
            latest = {}
            seen = []
            for name in group:
                out = folder / f"after-{name}"
                result = self.run_pass(store, name, seen, out)
                for item in result["marksFiles"]:
                    latest[item["captureId"]] = json.loads(Path(item["path"]).read_text())
                seen.append(name)
                corrected = [m["captureId"] for m in result["marksFiles"] if m["captureId"] != name]
                scores = {}
                for capture in seen:
                    labels = labels_of(self.labels / f"{capture}.labels.csv")
                    marks = latest[capture]["marks"]
                    ad = [(m["start"], m["end"]) for m in marks if m["kind"] == "ad"]
                    fingerprint = [(m["start"], m["end"]) for m in marks if m["kind"] == "ad" and m["basis"] != "comskip"]
                    scores[capture] = {
                        "marks": score(labels, ad),
                        "comskip": score(labels, comskip_cuts(self.evidence(capture))),
                        "fingerprintMarks": len(fingerprint),
                        "nonAdMarks": [[m["start"], m["end"], m["kind"]] for m in marks if m["kind"] != "ad"],
                        "coverage": self.airing_coverage(store, capture, labels),
                    }
                results["passes"].append({"store": str(store), "pass": name, "corrected": corrected,
                                          "promoted": result["promoted"], "airingsFound": result["airingsFound"],
                                          "entries": self.entry_counts(store), "scores": scores})
        return results


def report(results):
    for plan in results:
        print(f"== library plan {plan['plan']}")
        for step in plan["passes"]:
            print(f"  pass {step['pass']}: corrected {step['corrected'] or '-'}, promoted {step['promoted']}, "
                  f"airings {step['airingsFound']}, entries {step['entries']}")
            for capture, s in step["scores"].items():
                m, c, cov = s["marks"], s["comskip"], s["coverage"]
                worse = []
                if m["programmeCut"] > c["programmeCut"] + 0.05:
                    worse.append("programme cut")
                if m["breaks"] < c["breaks"]:
                    worse.append("breaks")
                if m["falseBreaks"] > c["falseBreaks"]:
                    worse.append("false breaks")
                print(f"    {capture:11} marks {m['breaks']:2}/{m['of']:2} missed {m['missed']:6.0f} cut {m['programmeCut']:5.0f} "
                      f"false {m['falseBreaks']} | comskip {c['breaks']:2}/{c['of']:2} missed {c['missed']:6.0f} "
                      f"cut {c['programmeCut']:5.0f} false {c['falseBreaks']} | repeats {100 * cov['repeatShare']:4.1f}% "
                      f"ad entries {100 * cov['adEntryShare']:4.1f}% fp marks {s['fingerprintMarks']} "
                      f"other {len(s['nonAdMarks'])} | {'WORSE: ' + ', '.join(worse) if worse else 'ok'}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("tool")
    parser.add_argument("work")
    parser.add_argument("pcm")
    parser.add_argument("evidence")
    parser.add_argument("--labels", default=str(Path(__file__).resolve().parents[2] / "tests" / "labels"))
    parser.add_argument("--plan", action="append", required=True)
    args = parser.parse_args()
    harness = Harness(args)
    results = [harness.plan(spec) for spec in args.plan]
    (harness.work / "results.json").write_text(json.dumps(results, indent=1))
    report(results)


if __name__ == "__main__":
    main()
