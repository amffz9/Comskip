"""Known middle ad between long program segments, with frame-sized separators."""
from pathlib import Path
import csv
import subprocess
import sys
import tempfile

executable = Path(sys.argv[1]).resolve()
work = Path(sys.argv[2]).resolve()
work.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix="commercial intervals ", dir=work) as directory:
    root = Path(directory)
    video = root / "program.y4m"
    width, height, fps = 160, 120, 25
    chroma = bytes([128]) * (width * height // 2)
    textures = [b"FRAME\n" + bytes(40 + (x + phase) % 160
                for x in range(width * height)) + chroma for phase in range(16)]
    black = b"FRAME\n" + bytes([16]) * (width * height) + chroma
    with video.open("wb") as output:
        output.write(f"YUV4MPEG2 W{width} H{height} F25:1 Ip A1:1 C420jpeg\n".encode())
        for segment, duration in enumerate((200, 30, 200)):
            for frame in range(duration * fps):
                output.write(textures[frame % len(textures)])
            if segment < 2:
                output.write(black)
    settings = root / "settings.ini"
    settings.write_text("detect_method=1\ncommercial_lengths=30\n"
        "optional_commercial_lengths=30\nmin_show_segment_length=120\n"
        "min_commercialbreak=20\nmax_commercial_size=120\n"
        "added_recording=0\npadding=0\nremove_before=0\nremove_after=0\n"
        "output_edl=1\noutput_framearray=1\nnum_logo_buffers=2\npunish_no_logo=0\nverbose=0\n")
    results = []
    for threads in (1, 4):
        destination = root / str(threads)
        destination.mkdir()
        run = subprocess.run([str(executable), f"--ini={settings}",
            f"--threads={threads}", f"--output={destination}", str(video)],
            capture_output=True, text=True, timeout=60)
        assert run.returncode in (0, 1), run.stdout + run.stderr
        edl = (destination / "program.edl").read_text()
        intervals = [line.split() for line in edl.splitlines()]
        assert len(intervals) == 1, f"Expected only the middle ad: {edl!r}"
        start, end = map(float, intervals[0][:2])
        # Separators begin at frames 5000 and 5751; one frame is 0.04 seconds.
        assert abs(start - 200.0) <= 0.041, edl
        assert abs(end - 230.04) <= 0.041, edl
        assert intervals[0][2] == "0", edl
        observations = (destination / "program.csv").read_text()
        rows = list(csv.reader(observations.splitlines()[2:]))
        assert len(rows) == 10752, "Decoded observations do not match the fixture"
        for index, timestamp in ((5000, 200.0), (5751, 230.04)):
            assert float(rows[index][16]) == timestamp, rows[index]
            assert int(rows[index][1]) <= 19, "Separator did not remain black"
        assert float(rows[-1][16]) == 430.04, rows[-1]
        results.append((edl, observations))
    assert results[0] == results[1], "Serial and parallel intervals differ"
