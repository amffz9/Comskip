"""Regression coverage for seeking, reopening, damaged input, and format changes.

Usage: media_recovery.py COMSKIP FFMPEG WORK_DIRECTORY
Generated fixtures and command logs remain in WORK_DIRECTORY after a failure.
"""
from pathlib import Path
import csv
import math
import shutil
import statistics
import subprocess
import sys
import tempfile


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def execute(arguments, directory, timeout=60):
    try:
        result = subprocess.run([str(argument) for argument in arguments], cwd=directory,
                                capture_output=True, encoding="utf-8", errors="replace",
                                timeout=timeout)
    except subprocess.TimeoutExpired as error:
        raise AssertionError(f"Command exceeded {timeout}s: {arguments}") from error
    return result


def generate(ffmpeg, path, duration, size="160x120", rate=48000, channels=2, offset=0):
    result = execute([ffmpeg, "-hide_banner", "-loglevel", "error", "-y",
                      "-filter_threads", "1", "-f", "lavfi", "-i",
                      f"testsrc2=size={size}:rate=25:duration={duration}",
                      "-f", "lavfi", "-i", f"sine=frequency=997:sample_rate={rate}:duration={duration}",
                      "-map", "0:v:0", "-map", "1:a:0", "-c:v", "mpeg2video",
                      "-pix_fmt", "yuv420p", "-g", "12", "-bf", "2", "-threads", "1",
                      "-c:a", "ac3", "-ar", rate, "-ac", channels,
                      "-output_ts_offset", offset, "-t", duration, "-f", "mpegts", path],
                     path.parent)
    require(result.returncode == 0, f"Could not generate {path.name}: {result.stderr}")


def analyze(executable, fixture, directory, threads=1, selftest=None):
    directory.mkdir()
    settings = directory / "settings.ini"
    settings.write_text("detect_method=1\nnum_logo_buffers=2\noutput_framearray=1\n"
                        "output_edl=1\nlive_tv_retries=0\nadded_recording=0\nverbose=10\n",
                        encoding="utf-8")
    arguments = [executable, f"--ini={settings}", f"--threads={threads}",
                 f"--output={directory}"]
    if selftest is not None:
        arguments.append(f"--selftest={selftest}")
    result = execute([*arguments, fixture], directory)
    report = result.stdout + result.stderr
    (directory / "command-output.log").write_text(report, encoding="utf-8")
    for log in directory.glob("*.log"):
        if log.name != "command-output.log":
            report += log.read_text(encoding="utf-8", errors="replace")
    # request_exit(-1) appears as 255 on Unix and unsigned DWORD -1 on Windows.
    require(result.returncode in (0, 1, 2, 99, 255, 4294967295),
            f"Unexpected crash/status {result.returncode} for {fixture.name}:\n{report[-5000:]}")
    require("panic:" not in report.casefold(), f"Corrupt internal buffering for {fixture.name}:\n{report[-5000:]}")
    return result, report


def read_frames(directory, stem):
    path = directory / f"{stem}.csv"
    require(path.is_file(), f"Missing analyzed frame CSV: {path}")
    lines = path.read_text(encoding="utf-8-sig").splitlines()
    if lines and lines[0].startswith("sep="):
        lines = lines[1:]
    rows = list(csv.reader(lines))
    require(len(rows) > 1, f"No analyzed frames in {path}")
    header = [field.strip().casefold() for field in rows[0]]
    records = [dict(zip(header, row)) | {"channels": int(row[-1])}
               for row in rows[1:] if row]
    timestamps = [float(row["pts"]) for row in records]
    require(all(math.isfinite(value) for value in timestamps), f"Nonfinite timestamps in {path}")
    require(timestamps == sorted(timestamps), f"Timestamp reversal in {path}")
    require(all(b - a < 0.12 for a, b in zip(timestamps, timestamps[1:])),
            f"Unexpected missing-frame gap in {path}")
    return records


def complete_analysis(executable, fixture, directory, expected_frames, threads=1):
    result, _ = analyze(executable, fixture, directory, threads)
    require(result.returncode in (0, 1), f"Valid video rejected with {result.returncode}: {fixture}")
    frames = read_frames(directory, fixture.stem)
    # The existing CSV writer omits final detection boundary frames.
    require(expected_frames - 3 <= len(frames) <= expected_frames,
            f"Lost decoded frames: expected {expected_frames}, found {len(frames)} for {fixture.name}")
    require(abs(float(frames[-1]["pts"]) - (expected_frames - 3) / 25) < 0.16,
            f"Analyzed duration differs from generated duration for {fixture.name}")
    return frames


def main():
    require(len(sys.argv) == 4, __doc__)
    executable, ffmpeg, work = (Path(argument).resolve() for argument in sys.argv[1:])
    work.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="media recovery ", dir=work))
    base = root / "seekable.ts"
    generate(ffmpeg, base, 13)
    failures = []

    def check(name, operation):
        try:
            operation()
            print(f"Passed {name}", flush=True)
        except (AssertionError, ValueError, KeyError) as error:
            failures.append(f"{name}: {error}")
            print(f"FAILED {name}: {error}", flush=True)

    check("complete reference", lambda: complete_analysis(executable, base, root / "reference", 325))

    def selftest_case(number, expected):
        result, report = analyze(executable, base, root / f"selftest-{number}", selftest=number)
        require(result.returncode in (0, 1), f"Selftest {number} failed with status {result.returncode}")
        require("failed" not in report.casefold(), f"Selftest {number} reported failure:\n{report[-5000:]}")
        for marker in expected:
            require(marker in report, f"Selftest {number} did not exercise {marker}:\n{report[-5000:]}")

    check("seek and reopen", lambda: selftest_case(1, ["Selftest 1 OK: Seektest", "Selftest 3 OK: Reopen"]))
    check("reset and reopen", lambda: selftest_case(2, ["Selftest 2 OK: Reset"]))
    check("EOF reopen", lambda: selftest_case(3, ["Selftest 3 OK: Reopen"]))

    def invalid_case(name, contents):
        fixture = root / name
        fixture.write_bytes(contents)
        result, report = analyze(executable, fixture, root / f"invalid-{fixture.stem}")
        require(result.returncode not in (0, 1), f"Invalid input accepted: {name}")
        require(any(word in report.casefold() for word in ("error", "invalid", "could not", "failed")),
                f"Invalid input lacked an actionable diagnostic: {name}")
        require(not (root / f"invalid-{fixture.stem}" / f"{fixture.stem}.csv").exists(),
                f"Invalid input produced an analyzed CSV: {name}")

    check("empty input", lambda: invalid_case("empty.ts", b""))
    check("malformed input", lambda: invalid_case("malformed.ts", bytes(range(256)) * 8))

    def truncated_case():
        fixture = root / "truncated.ts"
        contents = base.read_bytes()
        fixture.write_bytes(contents[:len(contents) // 2 + 79])
        result, _ = analyze(executable, fixture, root / "truncated")
        require(result.returncode in (0, 1), "Decodable truncated stream was rejected")
        frames = read_frames(root / "truncated", fixture.stem)
        require(75 <= len(frames) <= 250, f"Unexpected truncated duration: {len(frames)} frames")

    check("truncated transport packet", truncated_case)

    def damaged_case():
        fixture = root / "damaged.ts"
        contents = bytearray(base.read_bytes())
        middle = len(contents) // 2 // 188 * 188
        contents[middle:middle + 188 * 12] = bytes(188 * 12)
        fixture.write_bytes(contents)
        result, _ = analyze(executable, fixture, root / "damaged")
        require(result.returncode in (0, 1), "Recoverable transport damage rejected the entire recording")
        frames = read_frames(root / "damaged", fixture.stem)
        require(len(frames) >= 300, f"Recovery lost excessive frames: {len(frames)}")
        require(float(frames[-1]["pts"]) > 12.5, "Decoder stopped at transport damage")

    check("damaged stream recovery", damaged_case)

    def format_change_case():
        first, second = root / "first.ts", root / "second.ts"
        generate(ffmpeg, first, 6)
        generate(ffmpeg, second, 7, size="320x240", rate=44100, channels=1, offset=6)
        fixture = root / "changing.ts"
        fixture.write_bytes(first.read_bytes() + second.read_bytes())
        serial = complete_analysis(executable, fixture, root / "changing-serial", 325)
        parallel = complete_analysis(executable, fixture, root / "changing-parallel", 325, 4)
        require(serial == parallel, "Format changes depend on decoder thread count")
        early = [frame for frame in serial if 1 < float(frame["pts"]) < 5]
        late = [frame for frame in serial if 8 < float(frame["pts"]) < 12]
        require(early and late, "Missing analyzed frames on one side of the format change")
        require(all(frame["channels"] == 2 for frame in early), "Initial stereo format was lost")
        require(all(frame["channels"] == 1 for frame in late), "Mono channel change was not applied")
        require(max(int(frame["maxx"]) for frame in early) <= 160, "Initial frame geometry was wrong")
        require(min(int(frame["maxx"]) for frame in late) >= 290, "Resolution change was not applied")
        for section in (early, late):
            positive = [int(frame["sound"]) > 0 for frame in section]
            require(sum(positive) >= len(positive) * 0.95, "Audio disappeared across a format change")
        before = statistics.median(int(frame["sound"]) for frame in early)
        after = statistics.median(int(frame["sound"]) for frame in late)
        require(1.25 <= after / before <= 1.60,
                "Changing sample rate/channels changed the known sine amplitude incorrectly")

    check("resolution, sample-rate, and channel-layout changes", format_change_case)
    if failures:
        raise AssertionError("\n".join(failures) + f"\nRetained fixtures and logs: {root}")
    require(root.resolve().parent == work and root.name.startswith("media recovery "),
            "Refusing to clean artifacts outside the selected work directory")
    shutil.rmtree(root)
    print("Media seek, reopen, damaged input, and changing-format checks passed")


if __name__ == "__main__":
    main()
