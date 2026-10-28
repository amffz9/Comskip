"""comskip-fingerprint seed, search and pass: exit codes, the store, MarksFiles and
the pass result."""
from pathlib import Path
import csv
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


def tool(*args, stream=b""):
    return subprocess.run([str(executable), *map(str, args)], input=stream, capture_output=True, timeout=120)


with tempfile.TemporaryDirectory(prefix="fingerprint cli ", dir=work) as directory:
    root = Path(directory)
    store = root / "store.sqlite"

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
    run = tool("search", "--store", store, "--recording", "cap 2", "--evidence", evidence2_path, "--evidence", evidence_path,
               "--out", root / "out5")
    assert run.returncode == 0, run.stderr
    result = json.loads(run.stdout)
    assert [m["captureId"] for m in result["marksFiles"]] == ["cap 2"], result  # nothing changed this time
    run = tool("search", "--store", store, "--recording", "cap 1", "--evidence", evidence_path, "--out", root / "out6")
    marks = json.loads((root / "out6" / "cap_1.marks.json").read_text())["marks"]
    assert [(round(m["start"]), round(m["end"])) for m in marks] == [(100, 130), (190, 220), (250, 270)], marks

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
    with open(live_log, newline="") as source:
        rows = list(csv.reader(source))
    header, rows = rows[0], rows[1:]
    assert len(header) == 23 and all(len(row) == len(header) for row in rows), [row for row in rows if len(row) != 23][:3]
    events = [dict(zip(header, row)) for row in rows]
    kinds = {event["event"] for event in events}
    assert {"evidence", "match", "ended", "marks", "mark", "end"} <= kinds, kinds
    # While the cut airing was still agreeing, its mark covered the whole 30 s.
    assert any(event["event"] == "mark" and abs(float(event["from"]) - cut) < 1.01 and abs(float(event["to"]) - cut - 30) < 1.01
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
