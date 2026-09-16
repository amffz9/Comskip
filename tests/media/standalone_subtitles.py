"""Decode a real standalone Unicode SRT stream without embedded A53 captions."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

executable, ffmpeg, work_root = map(Path, sys.argv[1:4])
work_root.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix="standalone subtitles ", dir=work_root) as directory:
    root = Path(directory)
    subtitles = root / "source subtitles.srt"
    subtitles.write_text("1\n00:00:01,250 --> 00:00:03,500\n<i>Café 字幕</i>\nsecond line\n\n"
                         "2\n00:00:02,000 --> 00:00:04,000\n<b>Overlapping cue</b>\n\n"
                         "3\n00:00:07,000 --> 00:00:10,000\nEOF final cue\n\n", encoding="utf-8")
    media = root / "standalone.mkv"
    generated = subprocess.run([str(ffmpeg), "-hide_banner", "-loglevel", "error", "-y",
                                "-f", "lavfi", "-i", "testsrc2=size=160x120:rate=25:duration=10",
                                "-i", str(subtitles), "-map", "0:v", "-map", "1:s",
                                "-c:v", "ffv1", "-c:s", "srt", str(media)],
                               capture_output=True, text=True, timeout=30)
    assert generated.returncode == 0, generated.stdout + generated.stderr
    settings = root / "settings.ini"
    settings.write_text("detect_method=1\nnum_logo_buffers=2\noutput_srt=1\noutput_smi=1\n"
                        "output_framearray=1\nlive_tv_retries=0\nadded_recording=0\nverbose=0\n")
    outputs = []
    for threads in (1, 4):
        destination = root / str(threads)
        destination.mkdir()
        run = subprocess.run([str(executable), f"--ini={settings}", f"--threads={threads}",
                              f"--output={destination}", str(media)],
                             capture_output=True, encoding="utf-8", errors="replace", timeout=45)
        assert run.returncode in (0, 1), run.stdout + run.stderr
        srt = (destination / "standalone.srt").read_text(encoding="utf-8")
        assert "00:00:01,250 --> 00:00:02,000" in srt, srt
        assert "00:00:02,000 --> 00:00:03,500" in srt, srt
        assert "00:00:03,500 --> 00:00:04,000" in srt, srt
        assert "00:00:07,000 --> 00:00:10,000" in srt, srt
        assert "<i>Café 字幕</i>" in srt and "second line" in srt and "EOF final cue" in srt, srt
        assert len(re.findall(r" --> ", srt)) == 4, srt
        screens = re.split(r"\n\s*\n", srt.strip())
        assert "Café 字幕" in screens[0] and "Overlapping cue" not in screens[0], srt
        assert "Café 字幕" in screens[1] and "Overlapping cue" in screens[1], srt
        assert "Café 字幕" not in screens[2] and "Overlapping cue" in screens[2], srt
        sami = (destination / "standalone.smi").read_text(encoding="utf-8")
        document = ET.fromstring(sami)
        sync = document.findall("BODY/SYNC")
        assert [int(node.attrib["Start"]) for node in sync] == [1250, 2000, 2000, 3500, 3500, 4000, 7000, 10000], sami
        assert "Café 字幕" in sami and "EOF final cue" in sami, sami
        outputs.append((srt, sami))
    assert outputs[0] == outputs[1], "Standalone subtitle output differs with scanner thread count"
print("Standalone MKV/SRT decoding, Unicode/styles, EOF timing and serial/parallel outputs passed")
