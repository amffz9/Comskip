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

Live mode (step F5): harness.py TOOL WORK PCM_DIR EVIDENCE_DIR --live NAME...
  [--speed S] [--lag SECONDS] [--stall AT:SECONDS] [--limit SECONDS]
  [--library loo|all] [--cut-after SECONDS] [--excerpt START:LENGTH] [--tag SUFFIX]
Each NAME is replayed through `comskip-fingerprint live` with a library grown
by passes of the other labelled recordings (leave one out). Its audio is fed as
0.1 s pieces at S times real speed (default 1), and the longest single write is
timed. The live Evidence file is its final evidence cut at the audio second
reached minus --lag (default 10 s), rewritten whole every 10 s of audio with a
growing revision, and the final copy just before the end. The tool runs under
/usr/bin/time for its peak memory and CPU, the MarksFile is read every 0.2 s to
check it is always whole, and the tool's --log (logfmt) gives, on its own clock (the
capture second reached when it wrote), when each match was first marked and
when each projected part was withdrawn. --cut-after S makes divergences: in
every airing of an ad entry in the replayed recording (from the library) at
least S + 6 s long and inside a labelled ad row, the audio from S s in to its end is replaced by programme
audio (the middle of the longest unlabelled stretch), as when a shorter cut of
an ad airs. --excerpt feeds only LENGTH s from START, as one run starting at
START (real speed is for short excerpts; full recordings run accelerated).
A progress line comes about once a minute per run; a run whose fed position
does not advance for 60 s of wall time is stopped and fails. The tool's usage
lines give resident memory and CPU over stream time: a run fails over 48 MB, or
when memory rises more than 10 MB an hour over the last 10 usage lines after the
first 5 minutes. Writes WORK/live/NAME/live.json and WORK/live-results.json.
"""
import argparse
import csv  # the label files
import json
import os
import re
import signal
import shutil
import sqlite3
import struct
import subprocess
import sys
import threading
import time
from pathlib import Path

STUCK_SECONDS = 60  # a live run whose fed position does not advance this long fails
MEMORY_BUDGET_MB = 48  # F5: the tool's resident memory stays under this
# The leak rule over the tool's usage rows (one a stream minute): after the
# first WARM_UP stream seconds, the least-squares slope of resident memory over
# the last TREND_ROWS rows may not exceed LEAK_MB_PER_HOUR. Growth the tool
# means to have, its own codes and the map of them for self-repeats, measured
# about 5 MB an hour on g4 and g5; the rule allows twice that.
WARM_UP = 300
TREND_ROWS = 10
LEAK_MB_PER_HOUR = 10.0
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
            replace_json(self.inputs / f"{name}.context.json", context)
            source = self.evidence_dir / f"{name}.evidence.json"
            if source.exists():
                copy = json.loads(source.read_text())
                copy["captureId"] = name
                replace_json(self.inputs / f"{name}.evidence.json", copy)

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


PIECE = RATE // 10  # samples per A message in live mode: 0.1 s


def evidence_until(evidence, cutoff, revision, final):
    """What Comskip's live run would have shown by `cutoff`: seams and blocks
    before it, logo intervals cut at it."""
    return {"captureId": evidence["captureId"], "revision": revision, "final": final, "logoKnown": evidence["logoKnown"],
            "seams": [x for x in evidence["seams"] if x["at"] <= cutoff],
            "logo": [{"from": x["from"], "to": min(x["to"], cutoff)} for x in evidence["logo"] if x["from"] < cutoff],
            "blocks": [x for x in evidence["blocks"] if x["to"] <= cutoff]}


def replace_json(path, value):
    """Writes the file whole, so a reader (another Harness, the tool) never sees it half-written."""
    temporary = path.with_name(f"{path.name}.{os.getpid()}.tmp")
    temporary.write_text(json.dumps(value))
    temporary.replace(path)


LOGFMT_FIELD = re.compile(r'(\w+)=("(?:[^"\\]|\\.)*"|\S*)')


def logfmt(line):
    """One logfmt line as a dict of strings; quoted values use JSON escapes."""
    return {key: json.loads(value) if value.startswith('"') else value for key, value in LOGFMT_FIELD.findall(line)}


def read_log(path):
    """The tool's --log (logfmt) as events, numbers as numbers; each marks event
    carries its mark lines as [start, end, kind, state, basis]."""
    texts = ("event", "status", "kind", "state", "basis")
    events = []
    with open(path, encoding="utf-8") as source:
        for line in source:
            event = {key: value if key in texts else (float(value) if "." in value else int(value))
                     for key, value in logfmt(line).items()}
            if event["event"] == "mark":
                events[-1]["marks"].append([event["start"], event["end"], event["kind"], event["state"], event["basis"]])
                continue
            event["withdrawn"] = event.get("withdrawn") == 1
            event["marks"] = []
            events.append(event)
    return events


class Live:
    def __init__(self, harness, args):
        self.harness = harness
        self.speed = args.speed
        self.lag = args.lag
        self.stall = args.stall
        self.limit = args.limit
        self.tag = args.tag
        self.everything = args.library == "all"
        self.cut_after = args.cut_after
        self.excerpt = tuple(float(x) for x in args.excerpt.split(":")) if args.excerpt else None

    def library(self, name):
        """A store grown by passes of every other labelled recording (leave one
        out), or with --library all of all six, this one included; kept and
        reused once complete."""
        folder = self.harness.work / "live" / "libraries" / ("all" if self.everything else name)
        store = folder / "store.sqlite"
        if (folder / "done").exists():
            return store
        if folder.exists():
            shutil.rmtree(folder)
        folder.mkdir(parents=True)
        seen = []
        for other in CONTEXTS:
            if other == name and not self.everything:
                continue
            self.harness.run_pass(store, other, seen, folder / f"after-{other}")
            seen.append(other)
        (folder / "done").write_text("\n".join(seen))
        return store

    def replay(self, name):
        h = self.harness
        folder = h.work / "live" / (name + self.tag)
        if folder.exists():
            shutil.rmtree(folder)
        folder.mkdir(parents=True)
        store = folder / "store.sqlite"
        shutil.copy(self.library(name), store)
        evidence_path, marks_path, log_path = folder / "live.evidence.json", folder / "live.marks.json", folder / "live.log"
        evidence = h.evidence(name)
        command = ["/usr/bin/time", "-f", "maxrss_kb=%M user=%U system=%S elapsed=%e", h.tool, "live", "--store", str(store),
                   "--recording", name, "--context", str(h.inputs / f"{name}.context.json"), "--evidence", str(evidence_path),
                   "--marks", str(marks_path), "--log", str(log_path)]
        if self.stall:
            command += ["--stall", self.stall]
        # Its own process group, so stopping a stuck run stops /usr/bin/time and the tool.
        process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=0,
                                   start_new_session=True)
        pcm = (h.pcm / f"{name}.s16").read_bytes()
        cuts = self.cut(name, store, pcm) if self.cut_after else []
        if cuts:
            pcm = cuts.pop()
        # An excerpt is fed as one run starting at its capture second, so every
        # time stays a capture second.
        origin = 0.0
        if self.excerpt:
            origin, length = self.excerpt
            pcm = pcm[int(origin * RATE) * 2: int((origin + length) * RATE) * 2]
            cuts = [c for c in cuts if origin <= c < origin + length]
        if self.limit:
            pcm = pcm[: int(self.limit * RATE) * 2]
        samples = len(pcm) // 2
        end = origin + samples / RATE
        writes = {"longest": 0.0, "over1s": 0}
        reads = {"whole": 0, "broken": 0}
        done = threading.Event()
        # The stream position fed so far and when it last advanced, for the watchdog.
        progress = {"position": 0.0, "advanced": time.monotonic(), "stuck": None}

        def feed():
            try:
                send()
            except BrokenPipeError:
                if progress["stuck"] is None:
                    raise  # the tool exited early; its exit status says why

        def send():
            process.stdin.write(b"R" + struct.pack("<Id", 8, origin))
            revision, next_evidence = 0, origin + 10.0
            began = time.monotonic()
            for first in range(0, samples, PIECE):
                due = began + first / RATE / self.speed
                if due > time.monotonic():
                    time.sleep(due - time.monotonic())
                chunk = pcm[2 * first: 2 * min(samples, first + PIECE)]
                start = time.monotonic()
                process.stdin.write(b"A" + struct.pack("<I", len(chunk)) + chunk)
                took = time.monotonic() - start
                writes["longest"] = max(writes["longest"], took)
                writes["over1s"] += took > 1.0
                reached = origin + (first + PIECE) / RATE
                progress["position"], progress["advanced"] = min(reached, end) - origin, time.monotonic()
                if reached >= next_evidence:
                    revision += 1
                    replace_json(evidence_path, evidence_until(evidence, reached - self.lag, revision, False))
                    next_evidence += 10.0
            replace_json(evidence_path, evidence_until(evidence, end, revision + 1, True))
            time.sleep(1.0)  # the tool looks at the evidence every 0.5 s
            process.stdin.write(b"E" + struct.pack("<I", 0))
            process.stdin.close()

        def poll():
            while not done.is_set():
                if marks_path.exists():
                    try:
                        json.loads(marks_path.read_text())
                        reads["whole"] += 1
                    except (ValueError, OSError):
                        reads["broken"] += 1
                time.sleep(0.2)

        def watch():
            """A progress line about once a minute; a run whose position stops for 60 s fails."""
            began = next_line = time.monotonic()
            while not done.wait(1.0):
                now = time.monotonic()
                if now >= next_line:
                    print(f"  live {name}{self.tag}: {progress['position']:.0f} of {samples / RATE:.0f} s, "
                          f"{now - began:.0f} s elapsed", flush=True)
                    next_line += 60
                if progress["position"] < samples / RATE and now - progress["advanced"] > STUCK_SECONDS:
                    progress["stuck"] = progress["position"]
                    os.killpg(process.pid, signal.SIGKILL)
                    return

        errors = []
        threads = [threading.Thread(target=feed, daemon=True), threading.Thread(target=poll),
                   threading.Thread(target=lambda: errors.append(process.stderr.read())), threading.Thread(target=watch)]
        for thread in threads:
            thread.start()
        output = process.stdout.read()
        threads[2].join()
        process.wait()
        if progress["stuck"] is None:
            threads[0].join()
        done.set()
        threads[1].join()
        threads[3].join()
        if progress["stuck"] is not None:
            sys.exit(f"live {name}{self.tag}: the stream position stayed at {progress['stuck']:.1f} s of "
                     f"{samples / RATE:.1f} s for {STUCK_SECONDS} s of wall time; the tool was stopped")
        if process.returncode != 0:
            sys.exit(f"live {name} failed ({process.returncode}): {errors[0].decode(errors='replace')}")
        usage = dict(item.split("=") for item in errors[0].decode().split() if "=" in item)
        result = json.loads(output)
        events = read_log(log_path)
        measured = self.measure(name, events, end, cuts, origin)
        measured["usage"] = usage_trend(events)
        measured.update({"library": "all" if self.everything else "leave one out", "result": result, "writes": writes, "marksReads": reads,
                         "maxRssMB": round(int(usage["maxrss_kb"]) / 1024, 1),
                         "cpuSeconds": round(float(usage["user"]) + float(usage["system"]), 2),
                         "elapsed": float(usage["elapsed"]), "speed": self.speed, "stall": self.stall,
                         "excerpt": list(self.excerpt) if self.excerpt else None})
        final = json.loads(marks_path.read_text())["marks"]
        within = lambda items: [(max(s, origin), min(e, end), *rest) for s, e, *rest in items if s < end and e > origin]
        labels = within(labels_of(h.labels / f"{name}.labels.csv"))
        measured["score"] = score(labels, within([(m["start"], m["end"]) for m in final if m["kind"] == "ad"]))
        measured["comskip"] = score(labels, within(comskip_cuts(evidence_until(evidence, end, 0, True))))
        (folder / "live.json").write_text(json.dumps(measured, indent=1))
        failures = measured["usage"]["failures"]
        if measured["maxRssMB"] >= MEMORY_BUDGET_MB:
            failures.append(f"peak memory {measured['maxRssMB']} MB by /usr/bin/time, over the {MEMORY_BUDGET_MB} MB budget")
        if failures:
            live_report([measured])
            sys.exit(f"live {name}{self.tag} failed: " + "; ".join(failures))
        return measured

    def cut(self, name, store, pcm):
        """Cuts every long enough ad airing of the recording short; returns the
        cut points (capture seconds), then the new audio last."""
        with sqlite3.connect(store) as db:
            airings = db.execute("SELECT a.start, a.\"end\" FROM airing a JOIN entry e ON e.id = a.entry "
                                 "WHERE a.recording = ? AND e.status = 'ad' ORDER BY a.start", (name,)).fetchall()
        rows = labels_of(self.harness.labels / f"{name}.labels.csv")
        ads = [(s, e) for s, e, kind in rows if kind == "ad"]
        labels = sorted((s, e) for s, e, _ in rows)
        gaps = [(labels[i][1], labels[i + 1][0]) for i in range(len(labels) - 1) if labels[i + 1][0] > labels[i][1]]
        source_from, source_to = max(gaps, key=lambda gap: gap[1] - gap[0])
        middle = (source_from + source_to) / 2
        audio = bytearray(pcm)
        points = []
        for start, end in airings:
            inside = any(s - 5 <= start and end <= e + 5 for s, e in ads)  # not an in-programme ad
            if not inside or end - start < self.cut_after + 6 or any(abs(start - p) < 1 for p in points):
                continue
            at = start + self.cut_after
            first, last = int(at * RATE), int(end * RATE)
            source = int((middle - (last - first) / RATE / 2) * RATE)
            audio[2 * first: 2 * last] = pcm[2 * source: 2 * (source + last - first)]
            points.append(round(at, 3))
        return points + [bytes(audio)]

    def measure(self, name, events, duration, cuts=(), origin=0.0):
        """From the tool's log: for each match of a live index entry, when the
        first MarksFile with an ad mark over its start was written; for each
        projected part withdrawn by a divergence, how long after the codes
        stopped agreeing and after the agreed part's end."""
        marks_events = [e for e in events if e["event"] == "marks"]
        last = {}
        for e in events:
            if e["event"] in ("match", "ended"):
                last[e["match"]] = e
        latencies = []
        for index, match in sorted(last.items()):
            if match["entry"] == 0:
                continue
            start = match["agreed_from"]
            marked = next((e for e in marks_events
                           if any(m[2] == "ad" and m[0] <= start + 1.0 and m[1] >= start + 1.0 for m in e["marks"])), None)
            latencies.append({"match": index, "entry": match["entry"], "agreedFrom": start, "agreedTo": match["agreed_to"],
                              "markedAt": marked["now"] if marked else None,
                              "latency": round(marked["now"] - start, 2) if marked else None})
        withdrawals = []
        for e in events:
            # Projected parts withdrawn at the end of the audio are not divergences.
            if e["event"] != "ended" or not e["withdrawn"] or e["entry"] == 0 or e["now"] >= duration - 3:
                continue
            after = next((m for m in marks_events if m["wall"] >= e["wall"]), None)
            cut = min(cuts, key=lambda point: abs(point - e["agreed_to"])) if cuts else None
            withdrawals.append({"match": e["match"], "entry": e["entry"], "agreedTo": e["agreed_to"],
                                "cutAt": cut, "afterCut": round(e["ended_at"] - cut, 2) if cut is not None else None,
                                "stoppedAt": e["stopped_at"], "endedAt": e["ended_at"],
                                "afterStop": round(e["ended_at"] - e["stopped_at"], 2),
                                "afterAgreedEnd": round(e["ended_at"] - e["agreed_to"], 2),
                                "writeDelay": round(after["wall"] - e["wall"], 3) if after else None})
        # Each labelled break whose start lies within 6 s of a match's start: when
        # its first ad mark appeared, against the labelled start (labels are +-5 s).
        breaks = []
        for s, e, kind in labels_of(self.harness.labels / f"{name}.labels.csv"):
            if kind != "ad" or s >= duration or s < origin:
                continue
            first = [m for m in latencies if abs(m["agreedFrom"] - s) <= 6.0]
            if not first:
                continue
            marked = next((m for m in marks_events if any(x[2] == "ad" and x[0] <= s + 6 and x[1] > s for x in m["marks"])), None)
            breaks.append({"label": [s, e], "matchStart": round(first[0]["agreedFrom"], 2),
                           "latency": round(marked["now"] - s, 2) if marked else None})
        # On real time, the wall clock less the capture second reached stays
        # constant when writing never lags the audio.
        offsets = [e["wall"] * self.speed - (e["now"] - origin) for e in marks_events]
        return {"name": name, "duration": duration, "cuts": list(cuts), "matches": latencies, "withdrawals": withdrawals, "breaks": breaks,
                "selfRepeats": sum(1 for m in last.values() if m["entry"] == 0),
                "clockOffset": [round(min(offsets), 3), round(max(offsets), 3)] if offsets else None,
                "marksWritten": len(marks_events)}


def usage_trend(events):
    """The tool's usage rows: resident memory and CPU over stream time, failing
    over the memory budget or on a rise past warm-up (the leak rule above)."""
    rows = [e for e in events if e["event"] == "usage"]
    mb = lambda row: row["rss_bytes"] / 2 ** 20
    failures = [f"resident memory {mb(r):.1f} MB at stream second {r['now']:.0f}, over the {MEMORY_BUDGET_MB} MB budget"
                for r in rows if mb(r) >= MEMORY_BUDGET_MB][:1]
    trend = {"rows": len(rows), "failures": failures}
    if not rows:
        return trend
    trend.update({"rssFirstMB": round(mb(rows[0]), 1), "rssLastMB": round(mb(rows[-1]), 1),
                  "rssMaxMB": round(max(mb(r) for r in rows), 1), "peakRssMB": round(rows[-1]["peak_rss_bytes"] / 2 ** 20, 1),
                  "indexMB": round(max(r["index_bytes"] for r in rows) / 2 ** 20, 2), "cpuSeconds": rows[-1]["cpu_seconds"]})
    # CPU per stream minute, first and last ten rows: a climb shows as a rise.
    steps = [(b["cpu_seconds"] - a["cpu_seconds"]) / max(b["now"] - a["now"], 1e-9) * 60 for a, b in zip(rows, rows[1:])]
    if steps:
        trend["cpuPerStreamMinute"] = [round(sum(steps[:10]) / len(steps[:10]), 3), round(sum(steps[-10:]) / len(steps[-10:]), 3)]
    late = [r for r in rows if r["now"] >= WARM_UP][-TREND_ROWS:]
    if len(late) < TREND_ROWS:
        trend["slopeMBPerHour"] = None  # too short to judge a trend
        return trend
    xs, ys = [r["now"] / 3600 for r in late], [mb(r) for r in late]
    mx, my = sum(xs) / len(xs), sum(ys) / len(ys)
    slope = sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / sum((x - mx) ** 2 for x in xs)
    trend["slopeMBPerHour"] = round(slope, 2)
    if slope > LEAK_MB_PER_HOUR:
        failures.append(f"resident memory rising {slope:.1f} MB an hour over the last {TREND_ROWS} usage rows "
                        f"(the rule allows {LEAK_MB_PER_HOUR})")
    return trend


def live_report(results):
    for r in results:
        lat = sorted(m["latency"] for m in r["matches"] if m["latency"] is not None)
        unmarked = sum(1 for m in r["matches"] if m["latency"] is None)
        late = sum(1 for m in lat if m > 6.0)
        wd = r["withdrawals"]
        s, c = r["score"], r["comskip"]
        print(f"== live {r['name']} at {r['speed']}x, {r['duration']:.0f} s, library {r['library']}" + (f", stall {r['stall']}" if r["stall"] else ""))
        print(f"  index matches {len(r['matches'])}, self-repeats {r['selfRepeats']}; marked after agreed start: "
              f"max {lat[-1] if lat else '-'} s, median {lat[len(lat) // 2] if lat else '-'} s, over 6 s {late}, "
              f"never marked {unmarked}")
        bl = sorted(b["latency"] for b in r["breaks"] if b["latency"] is not None)
        print(f"  labelled breaks led by a match {len(r['breaks'])}: marked after labelled start {bl or '-'}")
        if wd:
            print(f"  divergences {len(wd)}: withdrawn after codes stopped agreeing max {max(w['afterStop'] for w in wd)} s; "
                  f"after the agreed end {min(w['afterAgreedEnd'] for w in wd)}-{max(w['afterAgreedEnd'] for w in wd)} s; "
                  f"MarksFile written after max {max((w['writeDelay'] or 0) for w in wd)} s")
            after_cut = [w["afterCut"] for w in wd if w["afterCut"] is not None]
            if after_cut:
                print(f"  cuts {len(r['cuts'])}: withdrawn after the cut point {min(after_cut)}-{max(after_cut)} s")
        else:
            print("  divergences 0")
        print(f"  dropped {r['result']['droppedAudioSeconds']} s, longest write {r['writes']['longest']:.3f} s "
              f"(over 1 s: {r['writes']['over1s']}), MarksFile reads whole {r['marksReads']['whole']} broken "
              f"{r['marksReads']['broken']}, peak memory {r['maxRssMB']} MB, CPU {r['cpuSeconds']} s "
              f"({100 * r['cpuSeconds'] / r['elapsed']:.2f}% of one core), wall less audio clock {r['clockOffset']} s")
        u = r["usage"]
        if u["rows"]:
            print(f"  usage rows {u['rows']}: RSS {u['rssFirstMB']} -> {u['rssLastMB']} MB (max {u['rssMaxMB']}, peak {u['peakRssMB']}), "
                  f"slope after warm-up {u['slopeMBPerHour'] if u['slopeMBPerHour'] is not None else 'n/a (too short)'} MB/h, "
                  f"live index {u['indexMB']} MB, CPU per stream minute {u.get('cpuPerStreamMinute')} s"
                  + (f"; FAILED: {'; '.join(u['failures'])}" if u["failures"] else ""))
        print(f"  final live marks {s['breaks']}/{s['of']} missed {s['missed']} cut {s['programmeCut']} false {s['falseBreaks']}"
              f" | comskip {c['breaks']}/{c['of']} missed {c['missed']} cut {c['programmeCut']} false {c['falseBreaks']}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("tool")
    parser.add_argument("work")
    parser.add_argument("pcm")
    parser.add_argument("evidence")
    parser.add_argument("--labels", default=str(Path(__file__).resolve().parents[2] / "tests" / "labels"))
    parser.add_argument("--plan", action="append")
    parser.add_argument("--live", nargs="+")
    parser.add_argument("--speed", type=float, default=1.0)
    parser.add_argument("--lag", type=float, default=10.0)
    parser.add_argument("--stall", help="AT:SECONDS, passed to the tool")
    parser.add_argument("--limit", type=float, help="feed only the first SECONDS")
    parser.add_argument("--excerpt", help="live: feed only START:LENGTH seconds of the audio, as a run starting at START")
    parser.add_argument("--tag", default="", help="suffix for the replay folders")
    parser.add_argument("--cut-after", type=float, help="live: cut each ad airing short this far in")
    parser.add_argument("--library", choices=("loo", "all"), default="loo",
                        help="live: leave the replayed recording out of the library (default), or grow it from all six")
    args = parser.parse_args()
    if not args.plan and not args.live:
        parser.error("give --plan or --live")
    harness = Harness(args)
    if args.plan:
        results = [harness.plan(spec) for spec in args.plan]
        (harness.work / "results.json").write_text(json.dumps(results, indent=1))
        report(results)
    if args.live:
        live = Live(harness, args)
        results = [live.replay(name) for name in args.live]
        (harness.work / f"live-results{args.tag}.json").write_text(json.dumps(results, indent=1))
        live_report(results)


if __name__ == "__main__":
    main()
