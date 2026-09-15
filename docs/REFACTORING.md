# Readability and portability

## Current structure

`comskip.cpp` combines configuration, detection heuristics, output formats, captions,
and the review UI in roughly 16,500 lines. `mpeg2dec.cpp` combines the entry point,
FFmpeg decoding, audio analysis, and seeking. Many functions share global state.
There are media-dependent seek/reopen self-tests and a video-output test program;
the new commercial-length tests start independent detection coverage.

## Proposed boundaries

- **Configuration:** one settings structure; INI and CLI parsing with validated
  values and explicit precedence. Keep existing defaults and option names.
- **Media:** FFmpeg ownership, decoded frames, timestamps, seeking, and audio
  normalization. Hide decoder resources behind a small interface.
- **Detection:** separate modules for brightness/scene changes, silence, logos,
  block scoring, and commercial-length policy. Pass settings and per-recording
  state explicitly rather than reading globals.
- **Output:** serializers for cut lists, chapters, and metadata, tested against
  expected files.
- **Application:** command-line orchestration, diagnostics, and an optional UI.
- **Platform:** isolate unavoidable native filesystem, process, and hardware
  integration. Prefer standard libraries where their semantics are sufficient;
  preserve Unicode paths and optional hardware acceleration.

`commercial_length.cpp` is the first extracted module. Its policy contains the
previous global inputs. Its match result lets the application retain its existing
diagnostic output without coupling the module to logging.

## Language decision

C++ is the smallest incremental migration: retain FFmpeg's C interface while
introducing automatic resource ownership, dynamic containers, and standard
filesystem/thread/time facilities. Merely compiling the current C as C++ would
not address its shared state or module structure.

Rust is a reasonable longer-term target if memory safety is a priority. Move pure
detection and configuration first, with an explicit state model and a small,
audited FFmpeg boundary. Foreign C code still requires separate safety reasoning.

Go suits a surrounding service or orchestration tool better than this decoder
core. Direct FFmpeg integration needs cgo and a C toolchain, which reduces the
benefit of changing languages for portability alone.

## Localization and regional settings

User-facing messages are currently embedded English strings. Configuration
defaults are embedded in source, although many values can be overridden through
INI or CLI options. Commercial-length tables include a compile-time regional
variant; timing corrections and some tolerance limits are also source constants.

Keep UI translations separate from regional detection profiles. Introduce message
catalogs for user-facing text, and data-driven, validated profiles for commercial
lengths if runtime regional selection is needed. Preserve machine-readable output
syntax and INI keys across UI languages. Hardcoded defaults can remain useful as
documented fallbacks.

## Next steps

1. Add regression coverage and fixes for the argument/configuration overflows,
   audio-format handling, AC3 buffering, and frame ownership found in review.
2. Add small licensed or generated media fixtures and expected detection/output
   results, covering timestamps, seeking, missing audio, and 10-bit video.
3. Extract configuration and output code; separate settings from per-run state.
4. Extract detection stages one at a time, comparing outputs with the baseline.
5. Select a migration language after those interfaces and tests are established.

Use compiler warnings and memory/undefined-behavior sanitizers where available.
The unit tests alone do not establish full detection equivalence or full-platform
application support.
