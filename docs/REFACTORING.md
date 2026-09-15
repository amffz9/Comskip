# Readability and portability

## Implemented structure

The application core uses C++23. Sources are organized under src/app,
src/config, src/detection, src/media, src/output, src/platform, and src/ui.
FFmpeg and the bundled caption decoder retain their C interfaces. Detection
stages and output code have been split into focused translation units, although
legacy functions still share globals through legacy_detection.h.

CMake is the supported build system. vcpkg supplies FFmpeg, argtable2, SimpleIni,
and Google Test. Unit tests cover commercial-length policy, validated settings,
and standard C++ scan workers. A generated-video integration test compares
single-worker and multi-worker CSV/EDL output. CI is configured for Windows,
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

- FFmpeg owns media demuxing and decoding. Its libswresample is the appropriate
  library for normalizing audio formats before volume analysis; the legacy audio
  analysis path still needs that integration and regression coverage.
- SimpleIni owns configuration syntax, and argtable2 owns command-line options.
- Google Test provides unit tests. Generated fixtures avoid committing recordings.
- Standard C++ threads, condition variables, and jthread replace the old pthread
  compatibility implementation. Prefer standard ownership and filesystem tools
  as further code is modernized.
- The optional SDL interface is a path toward portable review UI. The Windows
  review interface still uses DirectDraw and other native APIs.

## Remaining work

Known review findings still require fixes and targeted tests: Windows argument
buffering, audio sample-format assumptions, AC3 packet consumption, and 10-bit
frame ownership. Expand media coverage for seeking, audio, and 10-bit video.
Replace shared globals with explicit settings and per-recording state, and give
FFmpeg resources automatic ownership. Test output serializers against expected
files. Existing tests do not establish complete detection equivalence.
