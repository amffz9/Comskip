"""Exercise localized application errors and editable-catalog fallback."""
from pathlib import Path
import subprocess
import sys
import tempfile

executable = Path(sys.argv[1]).resolve()

def invoke(*arguments):
    result = subprocess.run([str(executable), *map(str, arguments)],
                            capture_output=True, encoding="utf-8", errors="replace", timeout=20)
    return result.returncode, result.stdout + result.stderr

def require(condition, message):
    if not condition:
        raise AssertionError(message)

with tempfile.TemporaryDirectory(prefix="comskip-locales-") as temporary:
    directory = Path(temporary)
    recording = directory / "missing.ts"
    malformed = directory / "malformed.ini"
    malformed.write_text('windowtitle="unterminated\n', encoding="utf-8")
    for language, prefix in [("es", "Configuración no válida:"), ("en", "Invalid configuration:")]:
        status, report = invoke("--language", language, "--ini", malformed, recording)
        require(status == 1, f"Malformed INI returned {status}:\n{report}")
        require(prefix in report, f"Missing {language} configuration error:\n{report}")
        cause="Valor INI entre comillas sin terminar" if language=="es" else "Unterminated quoted INI value"
        require(cause in report, f"Wrong failure cause:\n{report}")

    invalid = directory / "invalid.ini"
    invalid.write_text("language=es\nthread_count=0\n", encoding="utf-8")
    status, report = invoke("--ini", invalid, recording)
    require(status == 1 and "Configuración no válida:" in report,
            f"INI language did not select the configuration error:\n{report}")
    require("thread_count" in report, f"Error omitted the stable setting key:\n{report}")

    status, report = invoke("--language=es", "--unknown-localized-option", recording)
    require(status == 2 and "opción no válida" in report and "unknown-localized-option" in report,
            f"Unknown option did not produce a localized actionable error:\n{report}")
    status, report = invoke("--language=es", "--threads", "not-an-integer", recording)
    require(status == 2 and "argumento no válido" in report and "--threads" in report,
            f"Invalid integer option did not identify the localized option:\n{report}")

    catalog_directory = directory / "catalogs"
    catalog_directory.mkdir()
    (catalog_directory / "es.ini").write_text('usage="Ayuda externa:\\n  comskip "\n', encoding="utf-8")
    overrides = directory / "overrides.ini"
    overrides.write_text(f'language=es\nlocale_directory="{catalog_directory.as_posix()}"\n', encoding="utf-8")
    status, report = invoke("--ini", overrides, "--help")
    require(status == 2 and "Ayuda externa:" in report, f"External catalog was not used:\n{report}")
    require("Display syntax" in report and "Detection Methods" in report,
            f"Missing catalog entries did not fall back to English:\n{report}")

print("Localized CLI errors and external English fallback passed.")
