"""Verify localized reasons, safe catalog failure, and XML destination errors."""
from pathlib import Path
import subprocess
import sys
import tempfile

executable = Path(sys.argv[1]).resolve()
ffmpeg = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else None

with tempfile.TemporaryDirectory(prefix="comskip-diagnostics-") as temporary:
    directory = Path(temporary)
    def invoke(*arguments):
        result = subprocess.run([str(executable), *map(str, arguments)], cwd=directory,
                                capture_output=True, encoding="utf-8", errors="replace", timeout=30)
        return result.returncode, result.stdout + result.stderr
    def require(condition, report):
        if not condition:
            raise AssertionError(report)

    csv = directory / "empty.csv"
    csv.write_text("", encoding="utf-8")
    settings = directory / "settings.ini"
    settings.write_text("language=es\nmax_brightness=256\n", encoding="utf-8")
    status, report = invoke("--ini", settings, csv)
    require(status == 1 and "max_brightness debe estar entre 0 y 255" in report
            and "must be between" not in report, report)

    settings.write_text("language=es\n", encoding="utf-8")
    status, report = invoke("--ini", settings, csv)
    require(status == 2 and "La entrada CSV no tiene cabecera" in report
            and "CSV input has no header" not in report, report)

    catalogs = directory / "catálogos"
    catalogs.mkdir()
    (catalogs / "es.ini").write_text('diag_setting_byte="Sin argumento"\n', encoding="utf-8")
    settings.write_text(f'language=es\nlocale_directory="{catalogs.as_posix()}"\n', encoding="utf-8")
    status, report = invoke("--ini", settings, csv)
    require(status == 2 and "Los argumentos del catálogo no coinciden: diag_setting_byte" in report,
            report)

    if ffmpeg:
        recording = directory / "clip.ts"
        subprocess.run([str(ffmpeg), "-hide_banner", "-loglevel", "error", "-y",
                        "-f", "lavfi", "-i", "color=black:size=320x240:rate=25:duration=3",
                        "-c:v", "mpeg2video", "-f", "mpegts", str(recording)], check=True, timeout=30)
        settings.write_text("language=es\noutput_videoredo3=1\noutput_default=0\n", encoding="utf-8")
        blocked = directory / "clip.VPrj"
        blocked.mkdir()
        status, report = invoke("--ini", settings, "--output", directory, recording)
        require(status == 6 and "No se pudo abrir el archivo de salida:" in report
                and str(blocked) in report, report)

print("Typed Spanish diagnostics and safe application reporting passed.")
