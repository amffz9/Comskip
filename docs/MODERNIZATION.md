# Modernization completion checklist

The remaining work is complete only when all items have implementation and
verification evidence. Passing the current media smoke tests alone is insufficient.

- [ ] Settings are ordinary validated application-owned values, including regional
  commercial profiles; loading does not mutate process-wide globals.
- [ ] RecordingState owns per-recording detection, logo, caption, timing, output,
  and UI state. Interfaces receive their dependencies explicitly. No shared mutable
  globals, singleton accessors, thread-local replacements, or global aliases remain.
- [ ] FFmpeg resources and files have automatic ownership across normal, error,
  seek, and reopen paths. Lower-level functions return/throw actionable errors
  rather than terminate the process.
- [ ] The review UI uses SDL across supported platforms with explicit event state,
  RAII graphics resources, and a deliberate headless backend.
- [ ] Human-facing messages and review labels use committed catalogs with locale
  selection, fallback, and validated formatting. Machine-readable formats stay stable.
- [ ] Tests cover independent repeated analyses, seeking/reopening, damaged and
  truncated media, stream format changes, known commercial intervals, and exact
  output serializers including escaping and time/frame boundary cases.
- [ ] Windows, Linux, and macOS headless/SDL builds and tests are verified;
  sanitizer checks run where supported. CI configuration alone is not proof.
- [ ] Media and output code have focused interfaces and source modules; obsolete
  shared declarations and unsafe ownership/buffer patterns have been removed.

Use modern C++ facilities where they clarify ownership and contracts: value types,
unique ownership, spans, chrono, typed flags, ranges/algorithms, expected errors,
and standard synchronization. Supporting libraries retain responsibility for
media formats, configuration syntax, XML syntax, localization catalog syntax,
command-line parsing, graphics, and testing.

Commit each verified migration step. Keep remaining limitations explicit rather
than redefine completion around whichever subset currently passes tests.

## Verified migration stages

- Owned FFmpeg input/codec/frame/packet/dictionary/scaler resources are integrated
  in the recording-owned media coordinator. Networking follows decoder ownership.
- C++ lower-level exit requests unwind to the application boundary, and scan task
  failures propagate to the caller. Workers support captured recording dependencies.
- English/Spanish primary CLI, media, and review catalogs, external editable
  catalogs, and real CLI selection tests are integrated. Configuration warnings
  and argument errors have catalog coverage and actual error-path tests. Some
  detector and output diagnostics remain.
- Settings values own every committed configuration field and regional profile;
  editable output templates reject unsafe printf conversions and excess string
  arguments. At 4839fee, input media filenames are owned strings; an actual
  nested Unicode media path longer than 1,024 bytes opens and demuxes on Windows.
  At 34869fa, the connected basename/config/output filename fields are owned
  strings and path derivation uses UTF-8 filesystem paths. Full CLI analysis with
  Unicode input, INI, and output paths each longer than 1,024 bytes passes on
  Windows, including complete exports and optional caption-marker failure.
  Application callsites now receive an explicit recording context. Detection,
  media, output, and review state have moved into that context. Dynamic detection
  buffers, recording files, argument snapshots, and XDS metadata are owned values.
  Pixel buffers and live logo rings allocate for validated recording geometry;
  caption grids use owned arrays. Geometry changes invalidate stale logo state.
- The portable review backend is integrated with explicit event state and
  English/Spanish help and labels. Both backend variants pass their six tests;
  SDL rendering verification currently uses the dummy driver on Windows.
- Pure timed EDL serialization is integrated for standard/live/plus output, with
  boundary/offset/locale/error tests. Seven pugixml XML serializers are integrated
  into normal and review exports, with ten serializer and nine application
  adapter tests. Live DVRMSTB uses the same library, with four conversion and
  two actual live-detector tests. The obsolete custom XML escaper and seven
  unused XML file owners are removed. Plist fragment serialization is integrated
  through pugixml with five pure tests and normal/review/tail compatibility tests;
  actual media tests parse and compare all eight output families.
- The A53 caption bridge bounds each packet to 31 intact triplets, preserves
  oversized payloads through ordered chunks, and rejects malformed lengths.
  Four tests cover framing and data preservation. The obsolete supporting caption
  decoder and its shared globals were removed at 3cc57ed. An independent
  FFmpeg caption decoder passes six tests on Windows and Linux, including Linux
  sanitizers. Owned SRT/SAMI output passes seven Windows tests. Recording-owned
  sessions are integrated, with five actual application tests covering independent
  captions, EOF, failure cleanup, full reopen, CSV replay, and malformed persisted
  records. Standalone subtitle decoding has six pure Windows tests but packet
  routing was integrated at 12be69c, with three owned-session tests and an
  actual standalone-stream Unicode/style/EOF regression. At c4ab1e0, owned cue
  storage and an event sweep preserve overlapping/out-of-order display regions
  in SRT/SAMI, with pure-session and actual-stream regressions. Caption/XDS packet and cache bounds have
  five regressions, and frame-volume storage bounds have two.
- Windows headless and SDL-enabled application builds each pass 119 tests at
  f032ea3. Scoped Windows
  scheduling/power policies have three native restoration
  tests. Coverage includes
  seeking/reopening, damaged/truncated media, stream format changes, independent
  repeated analyses within one process, and failure cleanup followed by success.
  CSV replay, weighted-score boundaries, bounded review sampling, and persisted
  logo bounds. SDL rendering uses the dummy driver. Ubuntu GCC 14 / FFmpeg 6.1
  headless and SDL builds each pass 115 tests at f032ea3. The same suite passes
  with address, undefined-behavior, and leak sanitizers. Windows headless passes
  139 tests at 28556e6, before the seven new subtitle-output tests. Windows
  headless passes all 165 tests at 3cc57ed after removing the bundled C caption
  library. Linux headless and SDL each pass 161 tests on an isolated 3cc57ed
  snapshot plus the explicit caption bridge CMake dependency fix, subsequently
  committed at 12be69c. Sanitizer tests pass 156/161 on that snapshot; all five
  actual caption tests hit the missing-timestamp overflow tracked as B015.
  Windows headless passes all 174 tests at 12be69c, including standalone routing
  and four diagnostic-output regressions. At c4ab1e0, all 181 Windows tests pass,
  including exact CSV subtitle/cutlist roundtrips, stable repeated exports,
  missing-timestamp media, and three staged/bounded cutscene-loading tests.
  Linux headless and SDL builds of an isolated, unmodified c4ab1e0 snapshot each
  pass all 177 tests (14.09 and 12.88 seconds respectively); SDL uses the dummy
  driver. Its address/undefined/leak sanitizer suite passes 176/177 tests:
  missing-timestamp overflow is resolved, but the full-reopen caption test leaks
  210 bytes in three FFmpeg allocations (B025). Complete lifecycle leak safety
  remains unverified.
  With only the committed ed8649b packet-ownership patch applied to that isolated
  c4ab1e0 snapshot, all 177 tests pass under address, undefined-behavior, and leak
  sanitizers (31.83 seconds), with no suppressions. The five actual caption tests
  also pass a separate full-allocation-stack sanitizer run; B025 is resolved for
  this tested flow. This evidence does not cover later filename/parser changes.
  Four detector-warning
  regressions pass at 7e011c3, covering both logo-save failure branches. macOS and
  interactive SDL application verification still require evidence.
- At 4839fee, the complete Windows headless build passes 199 tests, including
  actual output-template compatibility and cutlist error localization. Eight
  checked input-parser tests pass. At bbbf019, all 206 Windows tests pass:
  integrated CSV/reference parsing validates complete owned records before
  settings/observation changes, with six actual input regressions. The FILE
  adapter reads buffered blocks while preserving recording file ownership.
  Frame timestamp lookup is also bounded against actual storage (0f98693).
  The unmodified bbbf019 snapshot passes all 202 Linux tests in headless Release
  (14.89 seconds), SDL Release with dummy video (13.51 seconds), and headless
  address/undefined/leak sanitizer Debug (36.41 seconds). No sanitizer findings
  or suppressions occurred. This covers the committed filename and parser stage,
  while later path and caption changes, B029, and interactive SDL remain separate.
- At 34869fa, all 210 Windows tests pass. The mixed-recording fixture specifies
  200 seconds of program, a single black frame, a 30-second ad, a single black
  frame, and 200 seconds of program. Both thread counts select only the middle
  ad within one frame of separator-derived boundaries and retain all 10,752
  observations with the expected timestamps. This verifies a controlled known
  interval; it does not prove general broadcast detection accuracy.
  The unmodified `d9e1ed1` snapshot verifies the full path stage on Ubuntu
  GCC14 / FFmpeg6.1: headless Release **210/210** (24.25 seconds), SDL Release
  with dummy video **210/210** (25.16 seconds), and headless Debug with address,
  undefined-behavior and leak sanitizers **210/210** (52.70 seconds). No source
  patches, sanitizer findings or suppressions occurred. Each suite runs the
  actual long Unicode input/INI/output CLI test, optional marker failure,
  caption control/extended-byte bounds, exact CSV/subtitle roundtrips, and known
  commercial intervals. Commands and logs are recorded in
  `bin/linux-verification-d9e1ed1.md`. The snapshot adds four pure reference
  comparison tests to the 34869fa stage; four Windows-only tests account for
  the platform count difference. The earlier full Windows result remains
  210/210 at 34869fa, with the four comparison tests verified separately.
  Application reference comparison and growable detection blocks were integrated
  at `1e8f795`; all 226 Windows tests pass, including empty/full interval lists,
  1,201 blocks, genuine merges, initialization, and empty/final scoring.
  Its isolated, unmodified Linux snapshot passes all 222 address, undefined,
  and leak sanitizer tests (56.68 seconds), without findings or suppressions.
  Exact commands and logs are in `bin/linux-verification-1e8f795.md`.
  macOS and interactive SDL remain unverified.
- At `e520374`, all 241 Windows headless tests pass. Frame masking uses a
  focused span-based module that validates geometry, storage, pixel counts,
  and percentages before any writes; it preserves padding and keeps configured
  values unchanged. Five review-navigation tests cover empty/full lists, both
  boundaries, and extreme cursor positions. Five playback warnings use catalogs;
  three actual error-path tests cover invalid decoded geometry/stride and an
  FFmpeg-muxed audio-only input. Its unmodified Linux snapshot passes all 237
  tests in headless Release (25.95 seconds), SDL Release (25.75 seconds), and
  address/undefined/leak sanitizer Debug (57.45 seconds), with no findings or
  suppressions. Commands are in `bin/linux-verification-e520374.md`.
  The Windows SDL build compiles, but CLI media tests expose a full-executable-
  path GUI-selection bug (B036); full Windows SDL verification remains open.
  Fixed interval/live-candidate storage remains open as B031–B033.
- At `931ee71`, all 256 Windows headless tests pass. Commercial and reference
  intervals and live candidates have growable owned storage, with checked
  count publication and bounded reference insertion. Eight storage/live tests
  cover more than 100,000 entries and complete rebuild classification, plus
  empty output and logo/silence feature behavior. Consecutive stalled packets
  now reach their warning threshold and reset on progress; three tests verify
  the counter. Audio/seek warnings use catalogs with two actual FFmpeg buffer
  error tests. GUI selection checks the executable filename, with real analysis
  under GUI-named parent directories. A fresh, unmodified Windows SDL snapshot
  passes all 256 tests from its `build-gui` directory (4.23 seconds), including
  all six previously timed-out media CLI tests. Exact commands and results are
  in `bin/windows-verification-931ee71.md`. SDL uses the dummy driver.
  The unmodified Linux snapshot passes all 252 tests in headless Release
  (34.78 seconds), SDL Release (34.31 seconds), and address/undefined/leak
  sanitizer Debug (75.79 seconds), without findings or suppressions. Exact
  commands and dependencies are in `bin/linux-verification-931ee71.md`.
  Brightness and sampling parameters were addressed in the next stage below;
  logo scan/filter arithmetic remains open as B041.
- At `b1f9e86`, all 270 Windows headless tests pass. Checked scene geometry
  and 0–255 brightness thresholds prevent invalid histogram/pixel access and
  zero normalization. Actual tests cover zero-border padded images, small
  retained regions, fully excluded samples, and missing buffers before changes.
  Logo sampling uses a checked positive frame interval; fractional rates sample
  once per frame, and unrepresentable rates fail before CSV state changes.
  Both behaviors have actual legacy CSV replay tests. Logo scan/filter arithmetic
  and short-history handling remain open as B041/B044; sanitizer verification
  of this exact scene stage remains separate.
- At `07aa466`, all 279 Windows headless tests and all 279 tests in an
  unmodified Windows SDL snapshot pass (7.15 seconds for SDL). Nine new tests
  cover validated logo scan axes, overflow bounds, ordinary edge detection,
  short/full filter histories, required buffers, and persisted rectangles.
  Unsafe logo scan macros are removed. Canonical analysis, decoder, and audio
  headers replace ad hoc declarations in implementations and tests. SDL uses
  the dummy driver; exact proof is in `bin/windows-verification-07aa466.md`.
  Linux sanitizer verification of this stage is running separately.
- At `baadc09`, all 284 Windows headless tests pass. Audio analysis and packet
  processing live in `src/media/audio_analysis.cpp`; explicit timing functions
  replace macros that captured local variables. Five timing tests cover exact
  rows, seek suppression, optional file failure/recovery, restart output, and
  exception cleanup. The duplicate restart header remains separate as B045.
- CI now defines headless and SDL jobs on Windows, Linux, and macOS plus a
  Linux address/undefined/leak sanitizer job. The seven-job YAML and build
  script syntax were checked locally. This configuration is not evidence of
  a successful remote run or macOS application behavior.
