"""Prove the GUI default font works without a source/install asset tree."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    parser.add_argument("--runtime-dir", action="append", type=Path, default=[])
    args = parser.parse_args()
    executable = args.executable.resolve()
    with tempfile.TemporaryDirectory(prefix="comskip-font-café-") as temporary:
        root = Path(temporary)
        deployment = root / "relocated"
        working = root / "empty-working-directory"
        deployment.mkdir()
        working.mkdir()
        destination = deployment / executable.name
        shutil.copy2(executable, destination)
        if os.name == "nt":
            for directory in [executable.parent, *args.runtime_dir]:
                for dll in directory.glob("*.dll"):
                    shutil.copy2(dll, deployment / dll.name)
        env = os.environ.copy()
        env["SDL_VIDEODRIVER"] = "dummy"
        env["SDL_AUDIODRIVER"] = "dummy"
        result = subprocess.run(
            [str(destination), "--gtest_filter=ReviewFont.DefaultFontRendersAfterRelocation"],
            cwd=working, env=env, capture_output=True, text=True, timeout=30)
        print(result.stdout, end="")
        print(result.stderr, end="")
        if result.returncode != 0:
            raise AssertionError(f"Relocated GUI font test exited {result.returncode}")
        if "[  PASSED  ] 1 test." not in result.stdout:
            raise AssertionError("Relocated GUI font test did not run exactly one test")


if __name__ == "__main__":
    main()
