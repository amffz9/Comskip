"""Actual GUI CLI failure must localize the action and retain its UTF-8 path."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

executable, work_root = map(Path, sys.argv[1:3])
work_root.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix="font failure café ", dir=work_root) as temporary:
    root = Path(temporary)
    media = root / "review.y4m"
    with media.open("wb") as stream:
        stream.write(b"YUV4MPEG2 W160 H120 F25:1 Ip A1:1 C420jpeg\n")
        frame = bytes([80]) * (160 * 120) + bytes([128]) * (160 * 120 // 2)
        for _ in range(50):
            stream.write(b"FRAME\n" + frame)
    missing = root / "missing café.ttf"
    settings = root / "settings.ini"
    settings.write_text(
        "language=es\noutput_debugwindow=1\ndetect_method=1\nverbose=0\n"
        "live_tv_retries=0\nadded_recording=0\n"
        f'review_font_file="{missing.as_posix()}"\n', encoding="utf-8")
    env = os.environ.copy()
    env["SDL_VIDEODRIVER"] = "dummy"
    env["SDL_AUDIODRIVER"] = "dummy"
    run = subprocess.run([str(executable.resolve()), f"--ini={settings.resolve()}", str(media.resolve())],
                         env=env, capture_output=True, encoding="utf-8", errors="replace", timeout=30)
    diagnostic = run.stderr
    assert run.returncode == 2, run.stdout + diagnostic
    assert "No se puede abrir la fuente de revisión" in diagnostic, diagnostic
    assert "missing café.ttf" in diagnostic, diagnostic
    assert "Cannot open review font" not in diagnostic, diagnostic
    media.unlink()
    settings.unlink()
