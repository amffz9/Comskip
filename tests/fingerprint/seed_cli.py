"""comskip-fingerprint seed: exit codes, the code stream file and the pass result."""
from pathlib import Path
import json
import math
import struct
import subprocess
import sys
import tempfile

RATE, CODE_SAMPLES, SPAN = 11025, 1365, 19 * 1365 + 4096
executable = Path(sys.argv[1]).resolve()
work = Path(sys.argv[2]).resolve()
work.mkdir(parents=True, exist_ok=True)


def run_start(start):
    return b"R" + struct.pack("<Id", 8, start)


def audio(seconds, frequency=440.0):
    count = int(seconds * RATE)
    samples = [int(8000 * math.sin(2 * math.pi * frequency * (1 + (i // 3000) % 4) * i / RATE)) for i in range(count)]
    payload = struct.pack(f"<{count}h", *samples)
    return b"A" + struct.pack("<I", len(payload)) + payload


END = b"E" + struct.pack("<I", 0)


def seed(root, name, stream):
    codes = root / f"{name}.codes.json"
    run = subprocess.run([str(executable), "seed", "--recording", name, "--codes", str(codes)],
                         input=stream, capture_output=True, timeout=30)
    return run, codes


with tempfile.TemporaryDirectory(prefix="seed cli ", dir=work) as directory:
    root = Path(directory)

    run, codes = seed(root, "short", run_start(1234.5) + audio(2) + END)
    assert run.returncode == 0, run.stderr
    assert json.loads(run.stdout) == {"recording": "short", "ended": True, "codes": 0, "marksFiles": [],
                                      "promoted": 0, "droppedAudioSeconds": 0.0}, run.stdout
    stream = json.loads(codes.read_text())
    assert stream["runs"] == [{"start": 1234.5, "codes": []}], stream
    assert abs(stream["codePeriod"] - CODE_SAMPLES / RATE) < 1e-9

    run, codes = seed(root, "two runs", run_start(0) + audio(20) + audio(10, 330) + run_start(600) + audio(5))
    assert run.returncode == 0, run.stderr
    result = json.loads(run.stdout)
    runs = json.loads(codes.read_text())["runs"]
    expected = [(30 * RATE - SPAN) // CODE_SAMPLES + 1, (5 * RATE - SPAN) // CODE_SAMPLES + 1]
    assert [len(r["codes"]) for r in runs] == expected, [len(r["codes"]) for r in runs]
    assert [r["start"] for r in runs] == [0, 600]
    assert result["ended"] is False and result["codes"] == sum(expected), result
    assert len(set(runs[0]["codes"])) > 10

    run, codes = seed(root, "no run", audio(1) + END)
    assert run.returncode == 2 and run.stderr, (run.returncode, run.stderr)
    assert not codes.exists() and not Path(f"{codes}.tmp").exists()

    run = subprocess.run([str(executable), "seed", "--recording", "x"], capture_output=True, timeout=30)
    assert run.returncode == 6 and b"usage" in run.stderr, (run.returncode, run.stderr)
print("seed cli ok")
