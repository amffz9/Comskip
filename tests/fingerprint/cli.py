"""comskip-fingerprint seed, search and pass: exit codes, the store, MarksFiles and
the pass result."""
from pathlib import Path
import re
import json
import math
import random
import sqlite3
import struct
import subprocess
import sys
import tempfile
import threading
import time
from contextlib import closing

RATE, CODE_SAMPLES, SPAN = 11025, 1365, 19 * 1365 + 4096
executable = Path(sys.argv[1]).resolve()
work = Path(sys.argv[2]).resolve()
work.mkdir(parents=True, exist_ok=True)


def run_start(start):
    return b"R" + struct.pack("<Id", 8, start)


def pcm(samples):
    return b"".join(b"A" + struct.pack("<I", 2 * len(piece)) + struct.pack(f"<{len(piece)}h", *piece)
                    for piece in (samples[i:i + RATE] for i in range(0, len(samples), RATE)))


def tones(seconds, frequency=440.0):
    count = int(seconds * RATE)
    return [int(8000 * math.sin(2 * math.pi * frequency * (1 + (i // 3000) % 4) * i / RATE)) for i in range(count)]


def melody(seconds, seed):
    """Random pairs of notes every 0.25 s: audio that repeats only where copied."""
    rng = random.Random(seed)
    samples = []
    for _ in range(int(seconds * 4)):
        a, b = rng.uniform(200, 2000), rng.uniform(200, 2000)
        start = len(samples)
        samples += [int(5000 * math.sin(2 * math.pi * a * (start + i) / RATE) + 4000 * math.sin(2 * math.pi * b * (start + i) / RATE))
                    for i in range(RATE // 4)]
    return samples


END = b"E" + struct.pack("<I", 0)


LOGFMT_FIELD = re.compile(r'(\w+)=("(?:[^"\\]|\\.)*"|\S*)')


def logfmt(line):
    """One logfmt line as a dict of strings; quoted values use JSON escapes."""
    return {key: json.loads(value) if value.startswith('"') else value for key, value in LOGFMT_FIELD.findall(line)}


def tool(*args, stream=b""):
    return subprocess.run([str(executable), *map(str, args)], input=stream, capture_output=True, timeout=120)


with tempfile.TemporaryDirectory(prefix="fingerprint cli ", dir=work) as directory:
    root = Path(directory)
    store = root / "store.sqlite"

    # version: the rules version the app keys its whole-store search on.
    run = tool("version")
    assert run.returncode == 0, run.stderr
    assert isinstance(json.loads(run.stdout)["rules"], int), run.stdout

    # seed: the stream goes into the store; --codes also writes it as JSON.
    codes = root / "short.codes.json"
    run = tool("seed", "--store", store, "--recording", "short", "--codes", codes, stream=run_start(1234.5) + pcm(tones(2)) + END)
    assert run.returncode == 0, run.stderr
    assert json.loads(run.stdout) == {"recording": "short", "ended": True, "codes": 0, "marksFiles": [],
                                      "promoted": 0, "droppedAudioSeconds": 0.0}, run.stdout
    assert json.loads(codes.read_text())["runs"] == [{"start": 1234.5, "codes": []}]
    assert abs(json.loads(codes.read_text())["codePeriod"] - CODE_SAMPLES / RATE) < 1e-9

    run = tool("seed", "--store", store, "--recording", "two runs",
               stream=run_start(0) + pcm(tones(20)) + pcm(tones(10, 330)) + run_start(600) + pcm(tones(5)))
    assert run.returncode == 0, run.stderr
    result = json.loads(run.stdout)
    expected = [(30 * RATE - SPAN) // CODE_SAMPLES + 1, (5 * RATE - SPAN) // CODE_SAMPLES + 1]
    assert result["ended"] is False and result["codes"] == sum(expected), result
    with closing(sqlite3.connect(store)) as db:
        runs = db.execute("SELECT start, length(codes) / 4 FROM run WHERE recording = 'two runs' ORDER BY start").fetchall()
        assert db.execute("PRAGMA journal_mode").fetchone()[0] == "wal"
    assert runs == [(0.0, expected[0]), (600.0, expected[1])], runs

    run = tool("seed", "--store", store, "--recording", "no run", stream=pcm(tones(1)) + END)
    assert run.returncode == 2 and run.stderr, (run.returncode, run.stderr)

    # pass: a 30 s spot aired twice in one recording, bounded by seams, with no
    # logo over it, is an ad in the MarksFile even with an empty library.
    spot = melody(30, 1)
    jingle = melody(20, 8)  # aired once here, and later in another capture
    audio = melody(100, 2) + spot + melody(60, 3) + spot + melody(30, 4) + jingle + melody(30, 27)
    first, second = 100.0, 190.0
    evidence = {"captureId": "cap 1", "revision": 7, "final": True, "logoKnown": True,
                "seams": [{"at": t, "kinds": ["black"]} for t in (first, first + 30, second, second + 30)],
                "logo": [{"from": 0, "to": first - 1}, {"from": first + 31, "to": second - 1}, {"from": second + 31, "to": 249},
                         {"from": 271, "to": 300}],
                "blocks": [{"from": 0, "to": 300, "score": 0.5, "decision": "programme"}]}
    evidence_path = root / "cap 1.evidence.json"
    evidence_path.write_text(json.dumps(evidence))
    context = root / "context.json"
    context.write_text(json.dumps({"channel": "CBS", "airStart": 1760000000, "source": "capture",
                                   "programmes": [{"from": 0, "to": 3600, "title": "Show", "seriesKey": "show"}]}))
    stream = run_start(0) + pcm(audio) + END
    out = root / "out"
    run = tool("pass", "--store", store, "--recording", "cap 1", "--context", context, "--evidence", evidence_path,
               "--out", out, stream=stream)
    assert run.returncode == 0, run.stderr
    result = json.loads(run.stdout)
    assert result["promoted"] == 1 and result["needsEvidence"] == [], result
    (item,) = result["marksFiles"]
    assert item["captureId"] == "cap 1" and Path(item["path"]).name == "cap_1.marks.json", item
    marks_text = Path(item["path"]).read_text()
    marks = json.loads(marks_text)
    assert marks["evidenceRevision"] == 7 and marks["final"] is True, marks
    found = [(m["start"], m["end"], m["kind"], m["basis"], m["state"]) for m in marks["marks"]]
    assert len(found) == 2, found
    for (start, end, kind, basis, state), at in zip(found, (first, second)):
        assert abs(start - at) < 1.01 and abs(end - at - 30) < 1.01, found
        assert (kind, basis, state) == ("ad", "fingerprint", "settled"), found

    # The same pass again gives the same file; search alone does too.
    run = tool("pass", "--store", store, "--recording", "cap 1", "--context", context, "--evidence", evidence_path,
               "--out", out, stream=stream)
    assert run.returncode == 0, run.stderr
    assert json.loads(run.stdout)["promoted"] == 0
    assert Path(item["path"]).read_text() == marks_text
    out2 = root / "out2"
    run = tool("search", "--store", store, "--recording", "cap 1", "--evidence", evidence_path, "--out", out2)
    assert run.returncode == 0, run.stderr
    assert (out2 / "cap_1.marks.json").read_text() == marks_text

    # Without its evidence copy a capture gets no MarksFile.
    run = tool("search", "--store", store, "--recording", "cap 1", "--out", root / "out3")
    assert run.returncode == 0 and json.loads(run.stdout)["marksFiles"] == [], run.stdout

    # A capture in another series airs the spot, an airing of the known ad that
    # changes nothing in the first capture, and the jingle, which becomes an ad
    # across shows: the first capture is corrected, and without its evidence
    # copy it is listed as needing one.
    other = run_start(0) + pcm(melody(40, 5) + spot + melody(40, 6) + jingle + melody(30, 29)) + END
    context2 = root / "context2.json"
    context2.write_text(json.dumps({"channel": "NBC", "airStart": 1760100000, "source": "capture",
                                    "programmes": [{"from": 0, "to": 3600, "title": "Other", "seriesKey": "other"}]}))
    evidence2 = dict(evidence, captureId="cap 2", seams=[], logo=[], blocks=[])
    evidence2_path = root / "cap 2.evidence.json"
    evidence2_path.write_text(json.dumps(evidence2))
    run = tool("pass", "--store", store, "--recording", "cap 2", "--context", context2, "--evidence", evidence2_path,
               "--out", root / "out4", stream=other)
    assert run.returncode == 0, run.stderr
    result = json.loads(run.stdout)
    assert [m["captureId"] for m in result["marksFiles"]] == ["cap 2"], result
    assert result["needsEvidence"] == ["cap 1"] and result["promoted"] == 1, result
    marks = json.loads((root / "out4" / "cap_2.marks.json").read_text())["marks"]
    # Two ads 40 s apart with no logo between are one break.
    assert [(round(m["start"]), round(m["end"])) for m in marks] == [(40, 130)], marks
    # The app's rerun for needsEvidence: the pass already changed the entries,
    # so nothing changes now, yet the corrected capture still gets its MarksFile
    # (every given Evidence copy of a stored capture is combined). Twice, the same.
    corrected = [(100, 130), (190, 220), (250, 270)]
    for out_dir in ("out5", "out5b"):
        run = tool("search", "--store", store, "--recording", "cap 2", "--evidence", evidence2_path,
                   "--evidence", evidence_path, "--out", root / out_dir)
        assert run.returncode == 0, run.stderr
        result = json.loads(run.stdout)
        assert [m["captureId"] for m in result["marksFiles"]] == ["cap 2", "cap 1"], result
        assert result["needsEvidence"] == [] and result["promoted"] == 0, result
        marks = json.loads((root / out_dir / "cap_1.marks.json").read_text())["marks"]
        assert [(round(m["start"]), round(m["end"])) for m in marks] == corrected, marks
    assert (root / "out5" / "cap_1.marks.json").read_text() == (root / "out5b" / "cap_1.marks.json").read_text()
    run = tool("search", "--store", store, "--recording", "cap 1", "--evidence", evidence_path, "--out", root / "out6")
    marks = json.loads((root / "out6" / "cap_1.marks.json").read_text())["marks"]
    assert [(round(m["start"]), round(m["end"])) for m in marks] == corrected, marks
    # An Evidence copy of a capture the store has no stream for gets no MarksFile.
    stranger_path = root / "stranger.evidence.json"
    stranger_path.write_text(json.dumps(dict(evidence2, captureId="stranger")))
    run = tool("search", "--store", store, "--recording", "cap 2", "--evidence", stranger_path, "--out", root / "out7")
    assert run.returncode == 0 and json.loads(run.stdout)["marksFiles"] == [], run.stdout

    # context: a stored recording's context is replaced, its codes kept, and the
    # entries promoted again; the summary is promote's.
    with closing(sqlite3.connect(store)) as db:
        runs_before = db.execute("SELECT start, codes FROM run WHERE recording = 'cap 2' ORDER BY start").fetchall()
    context2b = root / "context2b.json"
    context2b.write_text(json.dumps({"channel": "4.1", "network": "NBC", "airStart": 1760100000, "source": "plex",
                                     "programmes": [{"from": 0, "to": 3600, "title": "Other", "seriesKey": "other"}]}))
    run = tool("context", "--store", store, "--recording", "cap 2", "--context", context2b)
    assert run.returncode == 0, run.stderr
    result = json.loads(run.stdout)
    assert set(result) == {"promoted", "changedEntries", "corrected", "entries"}, result
    assert result["entries"].get("ad", 0) >= 2, result
    with closing(sqlite3.connect(store)) as db:
        source, channel, stored = db.execute("SELECT source, channel, context FROM recording WHERE id = 'cap 2'").fetchone()
        assert db.execute("SELECT start, codes FROM run WHERE recording = 'cap 2' ORDER BY start").fetchall() == runs_before
    assert (source, channel) == ("plex", "4.1") and json.loads(stored)["network"] == "NBC", stored
    # A recording the store does not hold is invalid input; a missing --context is a usage error.
    run = tool("context", "--store", store, "--recording", "nobody", "--context", context2b)
    assert run.returncode == 2 and run.stderr, (run.returncode, run.stderr)
    run = tool("context", "--store", store, "--recording", "cap 2")
    assert run.returncode == 6 and b"usage" in run.stderr, (run.returncode, run.stderr)
    # --contexts: many recordings, one promote; ids the store lacks are listed, not fatal.
    batch = root / "contexts.json"
    batch.write_text(json.dumps({"cap 2": {**json.loads(context2b.read_text()), "network": "ABC"}, "gone": {}}))
    run = tool("context", "--store", store, "--contexts", batch)
    assert run.returncode == 0, run.stderr
    result = json.loads(run.stdout)
    assert result["replaced"] == 1 and result["unknown"] == ["gone"], result
    with closing(sqlite3.connect(store)) as db:
        stored = db.execute("SELECT context FROM recording WHERE id = 'cap 2'").fetchone()[0]
    assert json.loads(stored)["network"] == "ABC", stored
    run = tool("context", "--store", store, "--recording", "cap 2", "--contexts", batch)
    assert run.returncode == 6 and b"usage" in run.stderr, (run.returncode, run.stderr)

    # live: the spot airs whole, then cut to 15 s; the Evidence file appears only
    # once the tool runs. Audio is paced at 10x real time; writing it never waits.
    live_audio = melody(50, 11) + spot + melody(100, 12) + spot[: 15 * RATE] + melody(60, 13)
    live_context = root / "live context.json"
    live_context.write_text(json.dumps({"channel": "CBS", "airStart": 1760200000, "source": "capture",
                                        "programmes": [{"from": 0, "to": 3600, "title": "Show", "seriesKey": "show"}]}))
    live_evidence = root / "live.evidence.json"
    live_marks = root / "live.marks.json"
    live_log = root / "live.log"

    def live(name, audio, speed, *extra, evidence_after=0.0):
        """Runs live with the audio paced at `speed` times real time; returns the
        process result, the longest single write in seconds, and the stdout JSON."""
        process = subprocess.Popen([str(executable), "live", "--store", str(store), "--recording", name, "--context",
                                    str(live_context), "--evidence", str(live_evidence), "--marks", str(live_marks),
                                    "--log", str(live_log), *map(str, extra)],
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=0)
        longest = [0.0]

        def feed():
            process.stdin.write(run_start(0))
            piece = RATE // 10
            began = time.monotonic()
            for index in range(0, len(audio), piece):
                due = began + index / RATE / speed
                if due > time.monotonic():
                    time.sleep(due - time.monotonic())
                chunk = audio[index:index + piece]
                start = time.monotonic()
                process.stdin.write(b"A" + struct.pack("<I", 2 * len(chunk)) + struct.pack(f"<{len(chunk)}h", *chunk))
                longest[0] = max(longest[0], time.monotonic() - start)
                if evidence_after and index / RATE >= evidence_after and not live_evidence.exists():
                    live_evidence.write_text(json.dumps({"revision": 3, "final": False, "logoKnown": True,
                                                         "seams": [], "logo": [], "blocks": []}))
            process.stdin.write(END)
            process.stdin.close()

        errors = []
        writer = threading.Thread(target=feed)
        reader = threading.Thread(target=lambda: errors.append(process.stderr.read()))
        writer.start()
        reader.start()
        out = process.stdout.read()
        writer.join()
        reader.join()
        err = errors[0]
        process.wait(timeout=60)
        return process.returncode, err, longest[0], json.loads(out) if process.returncode == 0 else None

    code, err, longest, result = live("cap 3", live_audio, 10, evidence_after=20)
    assert code == 0, err
    assert result["ended"] is True and result["droppedAudioSeconds"] == 0.0, result
    assert result["marksFiles"] == [{"captureId": "cap 3", "path": str(live_marks)}], result
    assert result["withdrawn"] >= 1, result
    assert longest < 1.0, longest
    final = json.loads(live_marks.read_text())
    assert final["captureId"] == "cap 3" and final["evidenceRevision"] == 3 and final["final"] is False, final
    found = [(round(m["start"]), round(m["end"]), m["kind"]) for m in final["marks"]]
    whole, cut = 50.0, 50.0 + 30 + 100
    assert len(found) == 2, found
    assert abs(final["marks"][0]["start"] - whole) < 1.01 and abs(final["marks"][0]["end"] - whole - 30) < 1.01, found
    # The cut airing: its projected part was withdrawn, so the mark ends where the codes stopped agreeing.
    assert abs(final["marks"][1]["start"] - cut) < 1.01 and abs(final["marks"][1]["end"] - cut - 15) < 1.01, found
    lines = live_log.read_text(encoding="utf-8").splitlines()
    events = [logfmt(line) for line in lines]
    # Every line is key=value fields starting event, wall, now.
    assert all(list(event)[:3] == ["event", "wall", "now"] for event in events), lines[:3]
    assert any('kind="in-programme ad"' in line or "kind=ad" in line for line in lines), lines[:5]
    kinds = {event["event"] for event in events}
    assert {"evidence", "match", "ended", "marks", "mark", "usage", "end"} <= kinds, kinds
    # A usage row about once a minute of stream time, and one at the end.
    usage = [event for event in events if event["event"] == "usage"]
    assert len(usage) == 255 // 60 + 1, usage
    # CPU time is counted in clock ticks (15.6 ms on Windows), so the first
    # minute of a fast matcher may show none; it only grows and ends above 0.
    assert all(int(u["rss_bytes"]) > 0 and int(u["peak_rss_bytes"]) >= int(u["rss_bytes"]) for u in usage), usage
    cpu = [float(u["cpu_seconds"]) for u in usage]
    assert cpu == sorted(cpu) and cpu[-1] > 0, usage
    assert [round(float(u["now"])) for u in usage[:-1]] == [60, 120, 180, 240], usage
    # While the cut airing was still agreeing, its mark covered the whole 30 s.
    assert any(event["event"] == "mark" and abs(float(event["start"]) - cut) < 1.01 and abs(float(event["end"]) - cut - 30) < 1.01
               for event in events), events
    withdrawn = [event for event in events if event["event"] == "ended" and event["withdrawn"] == "1"]
    assert withdrawn and all(float(event["ended_at"]) - float(event["stopped_at"]) <= 2.0 for event in withdrawn), withdrawn

    # A stalled matcher: 4 s asleep while 80 s of audio arrives. Reading goes on,
    # the queue keeps 30 s and the rest is dropped and counted.
    live_evidence.unlink()
    code, err, longest, result = live("cap 4", melody(150, 21), 20, "--stall", "20:4", evidence_after=1)
    assert code == 0, err
    assert result["ended"] is True and 30 <= result["droppedAudioSeconds"] <= 70, result
    assert longest < 1.0, longest
    # The audio after the gap is a new run in the store, so nothing is matched across it.
    with closing(sqlite3.connect(store)) as db:
        starts = [row[0] for row in db.execute("SELECT start FROM run WHERE recording = 'cap 4' ORDER BY start")]
    assert len(starts) == 2 and starts[0] == 0 and starts[1] > 20, starts

    # live needs one evidence path and a MarksFile path; audio before a run start is invalid.
    run = tool("live", "--store", store, "--recording", "x", "--evidence", live_evidence, stream=END)
    assert run.returncode == 6 and b"usage" in run.stderr, (run.returncode, run.stderr)
    run = tool("live", "--store", store, "--recording", "x", "--evidence", live_evidence, "--marks", live_marks,
               stream=pcm(tones(1)) + END)
    assert run.returncode == 2 and run.stderr, (run.returncode, run.stderr)

    # Errors: unknown recording 2, bad evidence 2, a file that is not a store 4,
    # a newer store 5, usage 6.
    run = tool("search", "--store", store, "--recording", "nothing", "--out", out)
    assert run.returncode == 2 and run.stderr, (run.returncode, run.stderr)
    bad = root / "bad.json"
    bad.write_text('{"revision": 1}')
    run = tool("search", "--store", store, "--recording", "cap 1", "--evidence", bad, "--out", out)
    assert run.returncode == 2 and run.stderr, (run.returncode, run.stderr)
    garbage = root / "garbage.sqlite"
    garbage.write_bytes(b"x" * 4096)
    run = tool("seed", "--store", garbage, "--recording", "x", stream=END)
    assert run.returncode == 4 and run.stderr, (run.returncode, run.stderr)
    newer = root / "newer.sqlite"
    assert tool("seed", "--store", newer, "--recording", "x", stream=END).returncode == 0
    with closing(sqlite3.connect(newer)) as db:
        db.execute("UPDATE meta SET value = '2' WHERE key = 'schema'")
        db.commit()
    run = tool("seed", "--store", newer, "--recording", "x", stream=END)
    assert run.returncode == 5 and run.stderr, (run.returncode, run.stderr)
    run = tool("seed", "--recording", "x")
    assert run.returncode == 6 and b"usage" in run.stderr, (run.returncode, run.stderr)
print("fingerprint cli ok")
