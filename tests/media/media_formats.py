"""Exercise sample formats, 10-bit video, AC3 alignment, and Unicode paths.

Usage: media_formats.py COMSKIP FFMPEG WORK_DIRECTORY
Fixtures are generated locally; no copyrighted media or network is needed.
"""
from pathlib import Path
import csv
import statistics
import subprocess
import sys
import tempfile


def execute(arguments, timeout=60):
    return subprocess.run([str(argument) for argument in arguments],
                          capture_output=True, encoding="utf-8", errors="replace",
                          timeout=timeout)


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def read_csv(data):
    lines = data.decode("utf-8-sig").splitlines()
    if lines and lines[0].startswith("sep="):
        lines = lines[1:]
    rows = list(csv.reader(lines))
    require(len(rows) > 1, "Missing CSV header or analyzed frames")
    header = [field.strip().casefold() for field in rows[0]]
    volume_column = header.index("sound") if "sound" in header else header.index("volume")
    timestamp_column = header.index("pts")
    frames = [row for row in rows[1:] if row]
    require(len(frames) >= 200, f"Only {len(frames)} analyzed frames")
    timestamps = [float(row[timestamp_column]) for row in frames]
    volumes = [int(row[volume_column]) for row in frames]
    require(timestamps == sorted(timestamps), "Frame timestamps decreased")
    require(sum(volume > 0 for volume in volumes) >= len(volumes) * 0.8,
            "Decoded audio was missing or silent for most frames")
    return volumes


def generate(ffmpeg, path, video_codec, audio_codec, pixel_format, container, size="160x120"):
    arguments = [ffmpeg, "-hide_banner", "-loglevel", "error", "-y",
                 "-filter_threads", "1", "-f", "lavfi", "-i",
                 f"testsrc2=size={size}:rate=25:duration=10",
                 "-f", "lavfi", "-i", "sine=frequency=997:sample_rate=48000:duration=10",
                 "-map", "0:v:0", "-map", "1:a:0", "-c:v", video_codec,
                 "-pix_fmt", pixel_format, "-threads", "1", "-c:a", audio_codec,
                 "-ac", "2", "-t", "10", "-f", container, path]
    run = execute(arguments)
    require(run.returncode == 0, f"Fixture generation failed for {path.name}:\n{run.stderr}")


def analyze(executable, fixture, directory, threads, alignment=0, lowres=0):
    directory.mkdir()
    settings = directory / "settings.ini"
    settings.write_text("detect_method=1\nnum_logo_buffers=2\noutput_framearray=1\n"
                        "output_edl=1\nlive_tv_retries=0\nadded_recording=0\n"
                        f"verbose=0\nalign_ac3_packets={alignment}\nlowres={lowres}\n", encoding="utf-8")
    run = execute([executable, f"--ini={settings}", f"--threads={threads}",
                   f"--output={directory}", fixture])
    # The legacy commercial-found status is inverted between Windows and Unix.
    require(run.returncode in (0, 1), f"{fixture.name} failed:\n{run.stdout}\n{run.stderr}")
    outputs = {}
    for extension in ("txt", "edl", "csv"):
        path = directory / f"{fixture.stem}.{extension}"
        require(path.is_file(), f"Missing output {path}:\n{run.stdout}\n{run.stderr}")
        outputs[extension] = path.read_bytes()
    return outputs, read_csv(outputs["csv"])


def compare_alignment(first, second):
    require(abs(len(first) - len(second)) <= 4, "AC3 alignment changed analyzed duration")
    # Codec priming and flushing can affect the beginning/end of a short file.
    interior = min(len(first), len(second)) - 10
    pairs = list(zip(first[10:interior], second[10:interior]))
    require(bool(pairs), "No interior AC3 frames to compare")
    relative = [abs(a - b) / max(a, b, 1) for a, b in pairs]
    require(statistics.median(relative) <= 0.05,
            "AC3 alignment changed typical volume by more than 5%")
    require(sorted(relative)[int(len(relative) * 0.95)] <= 0.15,
            "AC3 alignment changed more than 5% of interior frame volumes substantially")
    mean_first = statistics.mean(a for a, _ in pairs)
    mean_second = statistics.mean(b for _, b in pairs)
    require(abs(mean_first - mean_second) / max(mean_first, mean_second, 1) <= 0.02,
            "AC3 alignment changed mean interior volume by more than 2%")


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    require(len(sys.argv) == 4, __doc__)
    executable, ffmpeg, work_root = (Path(argument).resolve() for argument in sys.argv[1:])
    work_root.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="media formats ", dir=work_root) as temporary:
        root = Path(temporary)
        fixtures = [
            ("pcm-s16.nut", "rawvideo", "pcm_s16le", "yuv420p", "nut"),
            ("pcm-s32.nut", "rawvideo", "pcm_s32le", "yuv420p", "nut"),
            ("pcm-float.nut", "rawvideo", "pcm_f32le", "yuv420p", "nut"),
            ("ten-bit.mkv", "ffv1", "pcm_s16le", "yuv420p10le", "matroska"),
            ("mpeg-ac3.ts", "mpeg2video", "ac3", "yuv420p", "mpegts"),
            ("録画 café.nut", "rawvideo", "pcm_s16le", "yuv420p", "nut"),
        ]
        ac3_fixture = None
        ac3_volumes = None
        for index, (name, video, audio, pixels, container) in enumerate(fixtures):
            fixture = root / name
            generate(ffmpeg, fixture, video, audio, pixels, container)
            serial, volumes = analyze(executable, fixture, root / f"{index}-serial", 1)
            parallel, _ = analyze(executable, fixture, root / f"{index}-parallel", 4)
            require(serial == parallel, f"Serial and parallel outputs differ for {name}")
            if audio == "ac3":
                ac3_fixture, ac3_volumes = fixture, volumes
            print(f"Passed {name}", flush=True)

        aligned_serial, aligned_volumes = analyze(executable, ac3_fixture, root / "ac3-aligned-serial", 1, 1)
        aligned_parallel, _ = analyze(executable, ac3_fixture, root / "ac3-aligned-parallel", 4, 1)
        require(aligned_serial == aligned_parallel, "Parallel AC3 alignment outputs differ")
        compare_alignment(ac3_volumes, aligned_volumes)

        lowres_fixture = root / "reduced-resolution.ts"
        generate(ffmpeg, lowres_fixture, "mpeg2video", "ac3", "yuv420p", "mpegts", "640x480")
        for lowres, width, height in ((0, 640, 480), (1, 320, 240), (10, 320, 240)):
            serial, _ = analyze(executable, lowres_fixture, root / f"lowres-{lowres}-serial", 1, lowres=lowres)
            parallel, _ = analyze(executable, lowres_fixture, root / f"lowres-{lowres}-parallel", 4, lowres=lowres)
            require(serial == parallel, f"Reduced resolution {lowres} changed across worker counts")
            observations = list(csv.reader(serial["csv"].decode().splitlines()[2:]))
            measured_width = max(int(row[13]) for row in observations)
            measured_height = max(int(row[7]) for row in observations)
            require(width * 0.8 <= measured_width <= width and height * 0.8 <= measured_height <= height,
                    f"lowres={lowres} did not apply decoded geometry: {measured_width}x{measured_height}")
        print("Passed ordinary, explicit and automatic reduced-resolution decoding", flush=True)

        # A single nonexistent/invalid component differs from a valid long
        # nested path (covered by CliPaths). Let the filesystem report failure
        # without reintroducing an arbitrary application path-size ceiling.
        invalid_directory = "x" * 4096
        invalid = execute([executable, "--output=" + invalid_directory, root / fixtures[0][0]])
        diagnostic = (invalid.stdout + invalid.stderr).casefold()
        require(invalid.returncode not in (0, 1), "An invalid output directory was accepted")
        require("could not create file" in diagnostic and invalid_directory in diagnostic,
                f"Invalid output directory lacked a complete creation diagnostic:\n{diagnostic}")
    print("Media formats, Unicode paths, AC3 alignment, and argument checks passed")


if __name__ == "__main__":
    main()
