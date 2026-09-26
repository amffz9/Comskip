"""Live mode follows a recording while it is still being written.

Usage: live_growing_file.py COMSKIP FFMPEG WORK_DIRECTORY
The recording is released in small appends while Comskip runs in live mode.
Live mode must read it through without reopening or seeking, finish normally,
and publish the same cut list as post-processing of the finished file.
"""
from pathlib import Path
import shutil
import subprocess
import sys
import threading
import time


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def main():
    comskip, ffmpeg, work = sys.argv[1], sys.argv[2], Path(sys.argv[3])
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True)
    source = work / "source.ts"
    subprocess.run([ffmpeg, "-hide_banner", "-loglevel", "error", "-y",
                    "-f", "lavfi", "-i", "testsrc2=size=320x240:rate=25:duration=40",
                    "-f", "lavfi", "-i", "sine=frequency=500:duration=40",
                    "-c:v", "mpeg2video", "-g", "12", "-bf", "2", "-c:a", "ac3",
                    "-f", "mpegts", str(source)], check=True, timeout=120)
    settings = "detect_method=43\nverbose=1\nlive_tv_retries=1\noutput_edl=1\n"
    (work / "live.ini").write_text(settings + "live_tv=1\n", encoding="utf-8")
    (work / "post.ini").write_text(settings, encoding="utf-8")

    live_dir, post_dir = work / "live", work / "post"
    live_dir.mkdir()
    post_dir.mkdir()
    growing = live_dir / "recording.ts"
    data = source.read_bytes()
    growing.write_bytes(data[:len(data) // 20])

    def append_rest():
        step = len(data) // 20
        with growing.open("ab") as output:
            for offset in range(step, len(data), step):
                time.sleep(0.25)
                output.write(data[offset:offset + step])
                output.flush()

    writer = threading.Thread(target=append_rest)
    writer.start()
    live = subprocess.run([comskip, f"--ini={work / 'live.ini'}", f"--output={live_dir}", str(growing)],
                          capture_output=True, encoding="utf-8", errors="replace", timeout=120)
    writer.join()
    require(live.returncode in (0, 1), f"Live mode failed with {live.returncode}:\n{live.stdout}\n{live.stderr}")
    log = (live_dir / "recording.log").read_text(errors="replace")
    require("Retry=" not in log, f"Live mode reopened the growing recording:\n{log}")

    shutil.copyfile(source, post_dir / "recording.ts")
    post = subprocess.run([comskip, f"--ini={work / 'post.ini'}", f"--output={post_dir}",
                           str(post_dir / "recording.ts")],
                          capture_output=True, encoding="utf-8", errors="replace", timeout=120)
    require(post.returncode in (0, 1), f"Post-processing failed:\n{post.stdout}\n{post.stderr}")
    live_list = (live_dir / "recording.txt").read_text()
    post_list = (post_dir / "recording.txt").read_text()
    require(live_list == post_list, f"Live and post cut lists differ:\n{live_list}\n---\n{post_list}")
    frames = int(live_list.split()[3])
    require(frames >= 990, f"Live mode stopped early after {frames} frames")
    print(f"Live mode followed the growing recording through {frames} frames")


if __name__ == "__main__":
    main()
