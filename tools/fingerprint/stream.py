"""Wrap raw audio in the audio stream format that comskip-fingerprint reads.

usage: stream.py [--start S] [--piece SECONDS] [--no-end] < PCM > STREAM
PCM is 16-bit little-endian mono at 11025 Hz, for example from
  ffmpeg -i REC -vn -ac 1 -ar 11025 -f s16le -
The whole input is one run starting at S (default 0), sent as A messages of
--piece seconds (default 1), then E unless --no-end.
"""
import argparse
import struct
import sys

parser = argparse.ArgumentParser()
parser.add_argument("--start", type=float, default=0.0)
parser.add_argument("--piece", type=float, default=1.0)
parser.add_argument("--no-end", action="store_true")
args = parser.parse_args()
size = 2 * max(1, round(args.piece * 11025))
source, sink = sys.stdin.buffer, sys.stdout.buffer
sink.write(b"R" + struct.pack("<Id", 8, args.start))
pending = b""
while True:
    data = source.read(size - len(pending))
    if not data:
        break
    pending += data
    if len(pending) == size:
        sink.write(b"A" + struct.pack("<I", size) + pending)
        pending = b""
pending = pending[: len(pending) // 2 * 2]
if pending:
    sink.write(b"A" + struct.pack("<I", len(pending)) + pending)
if not args.no_end:
    sink.write(b"E" + struct.pack("<I", 0))
