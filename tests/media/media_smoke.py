"""Generate a tiny public-domain fixture and compare serial/parallel detection."""
from pathlib import Path
import csv
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

executable = Path(sys.argv[1]).resolve()
work_root = Path(sys.argv[2]).resolve()
work_root.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix="media test ", dir=work_root) as directory:
    root = Path(directory)
    video = root / "sample.y4m"
    width, height = 160, 120
    with video.open("wb") as output:
        output.write(f"YUV4MPEG2 W{width} H{height} F25:1 Ip A1:1 C420jpeg\n".encode())
        for frame in range(250):
            luma = bytes(16 if frame % 100 < 4 else 40 + ((x + frame) % 160)
                         for x in range(width * height))
            output.write(b"FRAME\n" + luma + bytes([128]) * (width * height // 2))
    settings = root / "test.ini"
    settings.write_text("detect_method=1\nnum_logo_buffers=2\noutput_framearray=1\n"
                        "output_edl=1\nlive_tv_retries=0\nadded_recording=0\nverbose=0\n"
                        "output_videoredo=1\noutput_videoredo3=1\nvideoredo_offset=0\n"
                        "output_edlx=1\noutput_btv=1\noutput_cuttermaran=1\n"
                        "output_dvrmstb=1\noutput_mkvtoolnix=2\noutput_plist_cutlist=1\n")
    results = []
    for threads in (1, 4):
        destination = root / str(threads)
        destination.mkdir()
        run = subprocess.run([str(executable), f"--ini={settings}", f"--threads={threads}",
                              f"--output={destination}", str(video)],
                             capture_output=True, text=True, timeout=45)
        # Existing Unix/Windows versions invert the 0/1 commercial-found status.
        assert run.returncode in (0, 1), run.stdout + run.stderr
        assert "Commercials were found." in run.stdout, run.stdout + run.stderr
        files = {extension: (destination / f"sample.{extension}").read_bytes()
                 for extension in ("txt", "edl", "csv")}
        rows = list(csv.reader(files["csv"].decode().splitlines()[2:]))
        assert len(rows) == 250, f"Expected 250 analyzed frames, got {len(rows)}"
        timestamps = [float(row[16]) for row in rows if len(row) > 16]
        assert timestamps and timestamps == sorted(timestamps)
        # With the correct 25fps timeline and EOF drain, this short synthetic
        # recording matches a commercial block under the default length policy.
        assert files["edl"] == b"0.00\t9.92\t0\n", "Unexpected commercial intervals"
        for extension in ("VPrj", "edlx", "chapters.xml", "cpf", "xml",
                          "mkvtoolnix.chapters", "mkvtoolnix.tags", "plist"):
            files[extension] = (destination / f"sample.{extension}").read_bytes()
            ET.fromstring(files[extension])
        project = ET.fromstring(files["VPrj"])
        assert project.tag == "VideoReDoProject"
        assert Path(project.findtext("Filename")) == video
        assert len(project.findall("CutList/Cut")) == 1
        dvr = ET.fromstring(files["xml"])
        commercials = dvr.findall("commercial")
        assert len(commercials) == 1
        assert commercials[0].attrib == {"start": "0.000000", "end": "9.920000"}
        results.append(files)
    assert results[0] == results[1], "Serial and parallel outputs differ"
print("Media decode, INI loading, serial/parallel analysis, and output checks passed")
