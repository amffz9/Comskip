"""Elementary MPEG-2 has missing packet PTS; decode it without signed overflow."""
from pathlib import Path
import csv
import subprocess
import sys
import tempfile

executable, ffmpeg, work_root = map(Path, sys.argv[1:4])
work_root.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix="missing timestamps ", dir=work_root) as directory:
    root = Path(directory)
    media = root / "no packet pts.m2v"
    generated = subprocess.run([str(ffmpeg), "-hide_banner", "-loglevel", "error", "-y",
                                "-f", "lavfi", "-i", "testsrc2=size=160x120:rate=25:duration=10",
                                "-c:v", "mpeg2video", "-bf", "2", "-f", "mpeg2video", str(media)],
                               capture_output=True, text=True, timeout=30)
    assert generated.returncode == 0, generated.stdout + generated.stderr
    settings = root / "settings.ini"
    settings.write_text("detect_method=1\nnum_logo_buffers=2\noutput_framearray=1\n"
                        "live_tv_retries=0\nadded_recording=0\nverbose=0\n")
    run = subprocess.run([str(executable), f"--ini={settings}", f"--output={root}", str(media)],
                         capture_output=True, text=True, timeout=45)
    assert run.returncode in (0, 1), run.stdout + run.stderr
    rows = list(csv.reader((root / "no packet pts.csv").read_text().splitlines()[2:]))
    assert len(rows) == 250, f"Expected one stored observation per decoded frame, got {len(rows)}"
    timestamps = [float(row[16]) for row in rows]
    assert timestamps == sorted(timestamps) and timestamps[-1] > 9.8, timestamps
print("Missing MPEG-2 timestamps decode all frames on a monotonic timeline")
