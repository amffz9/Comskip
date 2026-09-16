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
                        "output_dvrmstb=1\noutput_mkvtoolnix=2\noutput_plist_cutlist=1\n"
                        "output_ffmeta=1\noutput_ffsplit=1\n"
                        "output_vcf=1\noutput_projectx=1\noutput_avisynth=1\n"
                        "output_zoomplayer_cutlist=1\noutput_zoomplayer_chapter=1\n"
                        "output_scf=1\noutput_ipodchap=1\noutput_bsplayer=1\noutput_vdr=1\n"
                        "output_womble=1\noutput_mls=1\noutput_mpgtx=1\noutput_dvrcut=1\n"
                        "output_mpeg2schnitt=1\noutput_chapters=1\n")
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
                 for extension in ("txt", "edl", "csv", "ffmeta", "ffsplit", "vcf", "chp", "cut", "scf", "chap", "ipod.chap", "bcf", "vdr", "wme", "mls")}
        for name in ("mpgtx", "dvrcut"):
            files[name] = (destination / f"sample_{name}.bat").read_bytes()
        files["mpeg2schnitt"] = (root / "sample_mpeg2schnitt.bat").read_bytes()
        for extension in ("Xcl", "avs"):
            files[extension] = Path(str(video) + "." + extension).read_bytes()
        rows = list(csv.reader(files["csv"].decode().splitlines()[2:]))
        assert len(rows) == 250, f"Expected 250 analyzed frames, got {len(rows)}"
        timestamps = [float(row[16]) for row in rows if len(row) > 16]
        assert timestamps and timestamps == sorted(timestamps)
        # With the correct 25fps timeline and EOF drain, this short synthetic
        # recording matches a commercial block under the default length policy.
        assert files["edl"] == b"0.00\t9.92\t0\n", "Unexpected commercial intervals"
        assert files["ffmeta"] == (b";FFMETADATA1\n[CHAPTER]\nTIMEBASE=1/100\n"
                                   b"START=0\nEND=992\ntitle=Commercial Segment\n")
        assert files["ffsplit"] == b"", "An all-commercial recording has no retained show command"
        assert files["vcf"] == b"VirtualDub.video.SetMode(0);\nVirtualDub.subset.Clear();\n"
        assert files["Xcl"] == b"CollectionPanel.CutMode=2\n1\n1\n"
        assert files["avs"].endswith(b"trim(1,1)\n")
        assert files["cut"] == b'JumpSegment("From=0.0000","To=9.9200")\n'
        assert files["chp"] == b"AddChapterBySecond(0,Commercial Segment)\nAddChapterBySecond(9,Show Segment)\n"
        # SCF deliberately uses nominal frame indices (1..250), not media PTS.
        assert files["scf"] == (b"CHAPTER01=00:00:00.040\nCHAPTER01NAME=Commercial starts\n"
                                b"CHAPTER02=00:00:10.000\nCHAPTER02NAME=Commercial ends\n"), repr(files["scf"])
        assert files["bcf"] == b"1,0,9920\n"
        assert files["ipod.chap"].startswith(b"CHAPTER01=00:00:00.000\nCHAPTER01NAME=1\n")
        assert files["ipod.chap"].endswith(b"CHAPTER02NAME=2\n")
        assert files["chap"].startswith(b"FILE PROCESSING COMPLETE    249 FRAMES AT  2500\n-------------------\n")
        assert files["wme"].startswith(b"CLIPLIST: #1 show\n")
        assert b"[BookmarkList]\n" in files["mls"]
        assert files["mpgtx"].endswith(b"[0:00:00-]\n")
        assert files["dvrcut"] == b'dvrcut "%1" "%2" \n'
        assert files["mpeg2schnitt"].endswith(b"/o1 /i249 \n")
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
    legacy_settings = root / "legacy.ini"
    legacy_settings.write_text(settings.read_text().replace("output_videoredo3=1", "output_videoredo3=0"))
    legacy_results = []
    for threads in (1, 4):
        destination = root / f"legacy-{threads}"
        destination.mkdir()
        run = subprocess.run([str(executable), f"--ini={legacy_settings}", f"--threads={threads}",
                              f"--output={destination}", str(video)], capture_output=True, text=True, timeout=45)
        assert run.returncode in (0, 1), run.stdout + run.stderr
        project = (destination / "sample.VPrj").read_bytes()
        marks = (destination / "sample.vdr").read_bytes()
        assert project.startswith(f"<Version>2\n<Filename>{video}\n".encode())
        assert b"<Cut>0:99200000\n" in project, repr(project)
        assert b"<SceneMarker 0>" in project
        assert marks.startswith(b"0:00:00.00 start\n") and marks.endswith(b" end\n")
        legacy_results.append((project, marks))
    assert legacy_results[0] == legacy_results[1], "Legacy editor serial and parallel outputs differ"
print("Media decode, INI loading, serial/parallel analysis, and output checks passed")
