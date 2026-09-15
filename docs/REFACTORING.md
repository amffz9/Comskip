# Readability and portability

## Implemented structure

The application core uses C++23. Sources are organized under src/app,
src/config, src/detection, src/media, src/output, src/platform, and src/ui.
FFmpeg and the bundled caption decoder retain their C interfaces. Detection
stages and output code have been split into focused translation units, although
legacy functions still share globals through legacy_detection.h.

CMake is the supported build system. vcpkg supplies FFmpeg, argtable2, SimpleIni,
and Google Test. Unit tests cover commercial-length policy, validated settings,
standard C++ scan workers, Unicode filesystem paths, and FFmpeg audio/video conversion. Generated-video integration tests compare
single-worker and multi-worker CSV/EDL output for PCM audio, AC3, and 10-bit video. CI is configured for Windows,
Linux, and macOS; only Windows has been verified locally.

## Configuration and localization

config/defaults.ini is committed and embedded at build time. Runtime INI files
override those defaults. SimpleIni handles INI parsing and serialization; a small
adapter preserves legacy quoted-value escapes. Numeric settings are validated
before application state changes. Regional commercial lengths and timing policy
are configurable through INI profiles in config/profiles.

User-facing messages remain embedded English strings. Localization still needs
message catalogs; translations should be separate from detection profiles and
should preserve machine-readable output syntax and configuration keys.

## Supporting libraries

- FFmpeg owns media demuxing and decoding. libswresample normalizes decoded
  audio formats before the detector averages channels and measures volume.
  Existing S16 and float scales are retained; out-of-range samples are saturated.
- SimpleIni owns configuration syntax, and argtable2 owns command-line options.
- Google Test provides unit tests. Generated fixtures avoid committing recordings.
- Standard C++ threads, condition variables, and jthread replace the old pthread
  compatibility implementation. Standard filesystem paths, removal, sleep, and
  clocks replace several native helpers. Prefer standard ownership tools
  as further code is modernized.
- The optional SDL interface is a path toward portable review UI. The Windows
  review interface still uses DirectDraw and other native APIs.

## Remaining work

The reviewed argument/configuration buffer overflows, audio representation
assumptions, AC3 packet accounting, and 10-bit frame ownership have been addressed.
Further work can replace shared globals with explicit settings and per-recording
state, give remaining FFmpeg resources automatic ownership, and expand tests for
seeking and output serializers. Existing tests do not establish complete detection
equivalence.
