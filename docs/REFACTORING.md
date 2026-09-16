# Readability and portability

## Structure

The application core uses C++23. Sources are organized by responsibility:

- `src/app`: application lifecycle and owned recording context.
- `src/config`: INI parsing, validated settings, and regional profiles.
- `src/detection`: commercial scoring, scene analysis, logos, and captions.
- `src/media`: FFmpeg decoding, timing, and audio/video conversion.
- `src/output`: cutlists and focused serializers.
- `src/platform`: filesystem helpers and scoped native policies.
- `src/ui`: portable SDL review and an explicit headless backend.

Stateful application interfaces receive `RecordingContext&` explicitly. Settings,
recording buffers, files, decoder resources, and UI resources have owned lifetimes.
The large `legacy_detection.h` still needs replacement with focused interfaces.

## Supporting libraries

CMake is the supported build system. vcpkg supplies FFmpeg, SimpleIni, argtable2,
pugixml, rapidcsv, Google Test, and optional SDL2/SDL2_ttf. FFmpeg supplies caption decoding
and SRT encoding. Recording-owned caption sessions manage decoding and output;
the obsolete bundled caption library has been removed.

FFmpeg demuxes and decodes both audio and video. libswresample converts decoded
audio into the detector's measurement format; the detector then measures volume.
SimpleIni owns configuration/catalog syntax, argtable2 owns command-line parsing,
and pugixml owns XML syntax. Checked input modules use rapidcsv for CSV syntax
and standard `from_chars` for numeric conversion; application integration is
in progress. Standard C++ provides ownership, paths, synchronization,
and timing. Windows scheduling and sleep prevention require scoped native calls;
unsupported platforms retain their scheduling defaults.

## Configuration and verification

Committed defaults, regional profiles, and English/Spanish catalogs live under
`config/`. See [configuration documentation](../config/README.md) for overrides.
Some human-facing diagnostics still need catalog entries.

Tests include generated media, serial/parallel comparisons, repeated analyses,
failure cleanup, seeking, stream changes, and output serialization. Windows and
Ubuntu headless/SDL builds have been exercised; macOS remains unverified.

See [the completion checklist](MODERNIZATION.md) for current verification
evidence and remaining scope. Passing these tests does not prove complete
commercial-detection equivalence.
