"""Verify catalog selection through the real command-line entry point."""
import subprocess
import sys

for language, expected in (("en", "Usage:"), ("es", "Uso:")):
    run = subprocess.run([sys.argv[1], f"--language={language}", "--help"],
                         capture_output=True, encoding="utf-8", errors="replace", timeout=15)
    assert run.returncode == 2, run.stdout + run.stderr
    assert expected in run.stdout, run.stdout + run.stderr
    assert "--language" in run.stdout
run = subprocess.run([sys.argv[1], "--language=../es", "--help"],
                     capture_output=True, encoding="utf-8", errors="replace", timeout=15)
assert run.returncode == 2
assert "Unsupported language" in run.stderr
print("Catalog selection and application-boundary status checks passed")
