"""Actual subtitle destination failure must preserve localized context."""
from pathlib import Path
import subprocess
import sys
import tempfile

executable, ffmpeg, work_root = map(Path, sys.argv[1:4])
work_root.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix="subtitle failure café ", dir=work_root) as temporary:
    root = Path(temporary)
    subtitles = root / "source.srt"
    subtitles.write_text("1\n00:00:00,250 --> 00:00:01,500\nCafé\n\n", encoding="utf-8")
    media = root / "captions.mkv"
    generated = subprocess.run([str(ffmpeg), "-hide_banner", "-loglevel", "error", "-y",
        "-f", "lavfi", "-i", "testsrc2=size=160x120:rate=25:duration=2", "-i", str(subtitles),
        "-map", "0:v", "-map", "1:s", "-c:v", "ffv1", "-c:s", "srt", str(media)],
        capture_output=True, text=True, timeout=30)
    assert generated.returncode == 0, generated.stdout + generated.stderr
    output = root / "output café"
    output.mkdir()
    blocked = output / "captions.srt"
    blocked.mkdir()
    settings = root / "settings.ini"
    settings.write_text("language=es\ndetect_method=1\noutput_srt=1\nverbose=0\n"
                        "live_tv_retries=0\nadded_recording=0\n", encoding="utf-8")
    run = subprocess.run([str(executable.resolve()), f"--ini={settings.resolve()}",
                          f"--output={output.resolve()}", str(media.resolve())],
                         capture_output=True, encoding="utf-8", errors="replace", timeout=30)
    diagnostic = run.stderr
    assert run.returncode == 2, run.stdout + diagnostic
    assert "No se puede abrir el destino de subtítulos" in diagnostic, diagnostic
    assert "captions.srt" in diagnostic and "output café" in diagnostic, diagnostic
    assert "Opening subtitle destination" not in diagnostic, diagnostic
    assert blocked.is_dir()
    media.unlink()
    subtitles.unlink()
