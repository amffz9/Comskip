# Modernization completion checklist

The remaining work is complete only when all items have implementation and
verification evidence. Passing the current media smoke tests alone is insufficient.

- [x] Settings are ordinary validated application-owned values, including regional
  commercial profiles; loading does not mutate process-wide globals.
- [x] RecordingState owns per-recording detection, logo, caption, timing, output,
  and UI state. Interfaces receive their dependencies explicitly. No shared mutable
  globals, singleton accessors, thread-local replacements, or global aliases remain.
- [ ] FFmpeg resources and files have automatic ownership across normal, error,
  seek, and reopen paths. Lower-level functions return/throw actionable errors
  rather than terminate the process.
- [x] The review UI uses SDL across supported platforms with explicit event state,
  RAII graphics resources, and a deliberate headless backend.
- [x] Human-facing messages and review labels use committed catalogs with locale
  selection, fallback, and validated formatting. Machine-readable formats stay stable.
- [x] Tests cover independent repeated analyses, seeking/reopening, damaged and
  truncated media, stream format changes, known commercial intervals, and exact
  output serializers including escaping and time/frame boundary cases.
- [ ] Windows and Linux headless/SDL builds and tests are verified;
  sanitizer checks run where supported. CI configuration alone is not proof.
  macOS verification is deferred by user instruction (2026-09-15).
- [ ] Media and output code have focused interfaces and source modules; obsolete
  shared declarations and unsafe ownership/buffer patterns have been removed.

Use modern C++ facilities where they clarify ownership and contracts: value types,
unique ownership, spans, chrono, typed flags, ranges/algorithms, expected errors,
and standard synchronization. Supporting libraries retain responsibility for
media formats, configuration syntax, XML syntax, localization catalog syntax,
command-line parsing, graphics, and testing.

Windows sanitizer and non-interactive test execution details are documented in
[`docs/TESTING.md`](TESTING.md).

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
  Its unmodified Linux snapshot passes all 275 headless Release tests
  (30.87 seconds), SDL Release tests (29.74 seconds), and address/undefined/leak
  sanitizer Debug tests (64.41 seconds), without findings or suppressions.
  Exact commands and logs are in `bin/linux-verification-07aa466.md`.
- At `baadc09`, all 284 Windows headless tests pass. Audio analysis and packet
  processing live in `src/media/audio_analysis.cpp`; explicit timing functions
  replace macros that captured local variables. Five timing tests cover exact
  rows, seek suppression, optional file failure/recovery, restart output, and
  exception cleanup. The duplicate restart header remains separate as B045.
- CI now defines headless and SDL jobs on Windows, Linux, and macOS plus a
  Linux address/undefined/leak sanitizer job. The seven-job YAML and build
  script syntax were checked locally. This configuration is not evidence of
  a successful remote run or macOS application behavior.

- At `706801f`, all 285 Windows headless tests pass. Timing restart produces
  one header pair, verified by actual decoder reset/reopen with valid rows and
  owned file cleanup. This closes B045; later Linux/SDL verification is separate.

- The combined FFmpeg-sidecar/diagnostics/logo stage passes all 319 Windows
  headless tests (4.33 seconds). FFmetadata uses the native FFmpeg muxer with
  BITEXACT for stable chapter bytes; FFsplit commands have a focused chrono/span
  serializer. Normal and review exports use a finalized-interval adapter;
  legacy per-block sidecar writers and dead handles are removed. Paired exports
  verify option independence, and actual media runs compare complete sidecars
  across thread counts. Typed owned diagnostics preserve standard exception
  categories and readable English what(), render through committed EN/ES
  catalogs, and retain the translator during unwinding. Actual Spanish CLI
  tests cover brightness, empty CSV, malformed catalog and blocked XML paths.
  Eleven logo tests cover checked shrink offsets, owned closure storage and
  trend/history counters. Logs: `bin/windows-sidecars-diagnostics-{build,test}.txt`.
  Linux/SDL verification, review/caption error migration, font relocation and
  remaining legacy output modules are separate work.

- The unmodified `62664db` snapshot passes Windows SDL **319/319** (4.18s)
  and Linux headless **315/315** (35.88s), SDL **315/315** (37.12s), and
  address/undefined/leak sanitizer **315/315** (80.92s), without findings or
  suppressions. Linux uses a private package exposing only the installed
  rapidcsv header/config, avoiding an initial Windows-GTest header collision;
  source files were never patched. Exact evidence is in
  `bin/windows-verification-62664db.md` and `bin/linux-verification-62664db.md`.
- The font/settings stage passes Windows headless **326/326** (4.85s) and
  SDL **329/329** (6.64s). GUI builds embed NotoSans through a generated resource
  translation unit, with owned SDL font stream/font lifetime. INI settings
  `review_font_file` and `review_font_size` select an external font or the
  bundled default. A copied executable without an asset tree renders help,
  runs independent windows and reopens; invalid external overrides do not
  silently fall back. Dynamic string settings retain null rejection and have
  no arbitrary length cap. Actual INI loading reads a long Unicode cutscene
  path and rejects unrepresentable recording-duration settings before mutation.
  Logs are `bin/windows-font-settings-{build,test}.txt` and
  `bin/windows-font-gui-{configure,build,test}.txt`. Linux verification of this
  stage, interactive SDL, remaining human error reasons and legacy
  output modules remain separate work.

- The unmodified `c5c8496` Linux SDL Release snapshot passes all **325/325**
  tests (21.96s), including the relocated bundled-font executable. No source
  patches or repeated headless/sanitizer runs were needed for this scoped proof.
  Exact evidence is in `bin/linux-verification-c5c8496.md`.
- The script/diagnostic stage passes Windows headless **347/347** (5.14s)
  and SDL **351/351** (7.38s). Focused VCF/ProjectX/AviSynth serializers and
  one finalized-interval adapter replace their legacy per-block streams;
  normal/review exports retain frame conversion and numbering. AviSynth joins
  use emitted records, fixing early-cut concatenation. Seven serializer and six
  actual adapter tests plus complete serial/thread media exports cover these
  formats. Review/caption/subtitle diagnostics now own paths, timestamps and
  copied library details, with four category/rendering tests and actual Spanish
  CLI missing-font/blocked-subtitle regressions. Bright-pixel scaling uses
  checked geometry and wide integers, with two helper tests and an actual
  later-frame classification regression. Dead stream fields, legacy aliases,
  and the disabled settings-code tail are removed. Logs:
  `bin/windows-scripts-errors-build23-{build,test}.txt` and
  `bin/windows-scripts-errors-build23-gui-{build,test}.txt`.
  Saved-logo metadata/mask safety, remaining player exporters and application
  orchestration, other human error reasons, and interactive SDL proof
  remain required work; Linux verification of this stage is separate.

- The logo/player/application stage passes Windows headless **368/368** (16.83s)
  and SDL **372/372** (9.26s). Saved-logo input uses one owned stream, one INI
  metadata parse, checked geometry and locally staged masks before publication.
  Five real-file tests cover the current writer, legacy masks, missing fallback,
  invalid numbers, truncation, rollback and Unicode handle cleanup. Five focused
  player serializers and a finalized normal/review adapter replace their legacy
  per-block streams; thirteen tests and complete serial/thread media exports
  cover empty lists, exact bytes, independent options, SCF milliseconds and long
  hours. Application orchestration moves into `src/app/analysis.cpp`; decoder
  interfaces remain explicit. Argument/format/pixel/runtime diagnostics use
  committed catalogs; wide argument storage validates arrays and sentinel sizing.
  Caption time conversion has a shared checked helper and boundary regression.
  Logs: `bin/windows-logo-player-build23{,-gui}-{build,test}.txt`.
  Packet decoding now reports positioning failure and self-test completion as
  explicit outcomes instead of hiding both behind a zero return and unreachable
  statements. Other human-facing diagnostics, remaining output/decoder decomposition, the
  chapter filename collision B064, Linux verification of this stage and
  interactive SDL proof remain required work.

- The unmodified `9bcdc2e` Linux snapshot passes headless **343/343** (34.24s)
  and address/undefined/leak sanitizer **343/343** (89.79s), without findings
  or suppressions. Its SDL suite passes **346/347** (35.17s); actual CLI
  missing-font failure exposes B067's obsolete application GUI conditional.
  SDL backend resources and relocated-font unit tests really ran; application
  review orchestration was not proven. All jobs are terminal, snapshot sources
  were never patched. Evidence: `bin/linux-verification-9bcdc2e.md`.

- The supporting-diagnostic/progress stage passes Windows headless **379/379**
  (3.52s), SDL **383/383** (4.76s), and a complete public-speed Release build
  (`COMSKIP_DONATOR=OFF`). Caption decoder, A53 bridge, audio conversion, regional
  profile, diagnostic-output and media-dump errors preserve standard exception
  categories and render owned arguments through EN/ES catalogs. Five supporting
  regressions include a real FFmpeg channel-layout rejection; two actual output
  error regressions prove handle cleanup. Decode progress uses an owned
  steady-clock value, wide chrono durations and safe formatting; three
  deterministic tests cover counting, independent reset, long analyses and
  unknown durations. The progress adapter is a focused media source module.
  Dual-format chapters use a distinct `.ipod.chap` file and actual normal/review
  tests preserve both formats; existing individual filenames stay compatible.
  Linux's obsolete GUI flag is corrected and first-frame preview opens before
  subsequent subsampling. Corrected Linux proof is still pending. Logs:
  `bin/windows-diagnostics-progress-build23{,-gui}-{build,test}.txt` and
  `bin/windows-progress-public-{configure,build}.txt`. Other human-facing
  messages, remaining decoder/export decomposition and interactive SDL
  verification remain required work.

- The unmodified `66d45a8` Linux snapshot finishes headless **373/374**
  (38.51s), SDL **377/378** (42.51s), and address/undefined/leak sanitizer
  **373/374** (101.40s), with no sanitizer findings. All three share only
  B071's FFmpeg-version failure-stage test assumption; runtime rejects the
  frame safely. Actual localized GUI invalid-font failure and relocated font
  tests pass, proving B067's correction. Snapshot files were not patched.
  Exact proof: `bin/linux-verification-66d45a8.md`.
- The editor/geometry/codec stage passes Windows headless **398/398** (4.03s)
  and SDL **402/402** (4.98s). VDR and legacy VideoRedo2 exports have typed
  serializers/finalized adapters, with nine focused tests and complete actual
  serial/thread output checks. Their legacy project grammar and separate
  cut/scene timestamp policies stay stable; extracted serializers consistently
  write LF line endings. Geometry/storage helper errors use owned diagnostics
  and catalogs; seven regressions cover categories and actual producer state
  preservation. All nine initialization producers reject negative indices
  before growth. Codec setup moves into `src/media/decoder_setup.cpp`, checks
  parameter-copy errors and applies lowres before opening. Real MPEG2 media
  verifies ordinary, explicit and automatic reduced resolutions with identical
  serial/thread output and complete audio/timing observations. Borrowed stream
  references clear before input release; three tests cover Unicode reopen,
  repeated/null close and all stream kinds. Unused old decoder state fields and
  dead output handles are removed. B071's regression permits the native FFmpeg
  configuration or initialization failure stage with matching localized text.
  Logs: `bin/windows-editor-geometry-build23{,-gui}-{build,test}.txt`.
  Corrected Linux proof, remaining exporters/decoder decomposition, remaining
  human messages and interactive SDL verification remain required work.

  The complete public-speed Release application also builds for this stage;
  evidence: `bin/windows-editor-geometry-public-build.txt`.

- The unmodified `f444b33` Linux snapshot passes headless **393/393**
  (33.24s), SDL with the dummy video driver **397/397** (33.97s), and
  address/undefined/leak sanitizers **393/393** (113.57s), without findings or
  suppressions. This verifies the corrected FFmpeg 6 diagnostic path, decoder
  lifecycle and extracted editor outputs. Exact commands and logs are in
  `bin/linux-verification-f444b33.md`.

- The remaining legacy cut-list families now use focused serializers and one
  finalized normal/review adapter. Womble, MLS, mpgtx, DVR Cut,
  MPEG2Schnitt and plain chapter output have exact serializer and application
  tests; actual media smoke tests compare every output across worker counts.
  MLS review headers use the selected interval list. Self-test error records
  use a checked owned append writer and a committed `selftest_log_file` setting.
  The former decoder translation unit is split into video decoding, recording
  input and seeking modules. Windows passes **413/413** headless and **421/421**
  SDL tests, including four native Windows SDL event tests; the public-speed
  non-donator application also builds. Logs are
  `bin/windows-final-exports-build23{,-gui}-{build,test}.txt` and
  `bin/windows-final-exports-public-build.txt`. Linux verification of this
  combined stage, remaining human-facing messages, safe seek arithmetic and
  physical interactive UI proof remain.

- Scoring diagnostics now use the committed English and Spanish catalogs for
  the active scoring, combination, caption, aspect-ratio, and heuristic paths.
  Numeric values are formatted at their call sites before insertion into plain
  catalog fields, preserving the legacy decimal widths without a printf-format
  translation layer. The reproducible inventory in
  `bin/human-message-inventory.md` now reports **237** remaining literal call
  sites, down from 337; the six entries still attributed to `scoring.cpp` are
  disabled `#if 0` branches retained by the inventory's documented policy.
  Safe C++23 seek arithmetic and the corrected Womble EOF classification add
  focused and actual-media regressions. Windows passes **420/420** headless and
  **428/428** SDL tests; the public-speed non-donator application also builds.
  Other human-facing messages and Linux verification of this stage remain.

- Caption dictionary-processing diagnostics now use the English and Spanish
  catalogs with call-site numeric formatting. The reproducible literal-message
  inventory is down to **227** sites. Five finalized output adapters share one
  standard-library exact-byte writer; open and close/write failures propagate as
  owned `output_open`/`output_write` diagnostics instead of terminating inside
  the adapter. The helper retries plain chapter creation with `std::chrono` and
  `std::this_thread::sleep_for`, preserving the prior retry policy. Exact UTF-8
  replacement and missing-parent tests plus actual Spanish adapter failures pass
  in both Windows configurations. The complete stage passes Windows headless
  **423/423** and SDL **431/431** tests, and the public non-donator application
  builds. Logs are `bin/windows-output-errors-build23{,-gui}-{build,test}.txt`
  and `bin/windows-output-errors-public-build.txt`. Linux verification of this
  combined stage remains separate.

- The unmodified `8737c0b` Linux snapshot passes headless Release **415/415**
  (35.46s), SDL dummy Release **423/423** (34.62s), and the complete
  address/undefined/leak sanitizer suite **415/415** (127.28s), without findings
  or suppressions. This verifies the remaining legacy cut-list extraction,
  decoder split, safe seek arithmetic, Womble EOF correction and scoring
  localization on GCC 14 / FFmpeg 6.1. Exact commands and logs are in
  `bin/linux-verification-8737c0b.md`.

- Saved-logo input and output now use scoped standard-library ownership. The
  writer validates complete masks, checks exact writes and close, and propagates
  owned diagnostics instead of logging or exiting inside the storage layer.
  XDS diagnostics use the committed English and Spanish catalogs; the migration
  also fixes the program-length diagnostic's missing variadic argument. Forty-seven
  exact duplicate declarations were removed from the legacy detection header.
  Windows passes **427/427** headless and **435/435** SDL tests, and the public
  non-donator application builds. Logs are
  `bin/windows-logo-xds-build23{,-gui}-{build,test}.txt` and
  `bin/windows-logo-xds-public-build.txt`. The reproducible literal-message
  inventory remains at **208** active sites. Linux verification of this snapshot
  remains separate.

- All active caption diagnostics now use the English and Spanish catalogs while
  machine-facing caption labels retain their stable format. Numeric widths,
  uppercase hexadecimal fields and boolean `0`/`1` rendering are prepared at
  call sites. Fixing the first-block diagnostic also removes an out-of-bounds
  read from `cc_block[-1]`. The literal-message inventory is down to **189**
  active sites. Windows passes **429/429** headless and **437/437** SDL tests,
  and the public non-donator application builds. Logs are
  `bin/windows-caption-localization-build23{,-gui}-test.txt` and
  `bin/windows-caption-localization-public-build.txt`. Linux verification of
  this snapshot remains separate.

- Logo detection and caption-summary diagnostics now use the English and
  Spanish catalogs with numeric widths, percentages and durations formatted at
  their call sites. Empty caption summaries, failed logo-block lookups and both
  one-past-end logo transitions are handled before indexing or division. The
  reproducible literal-message inventory is down to **155** active sites.
  Core cut-list and live output now use checked writes, flushes and closes with
  owned open/write diagnostics. Windows passes **441/441** headless and
  **449/449** SDL tests, and the public non-donator application builds. Logs are
  `bin/windows-logo-output-build23{,-gui}-test.txt` and
  `bin/windows-logo-output-public-build.txt`. Linux verification of this stage
  remains pending.

- Recording startup now reports owned path-and-FFmpeg diagnostics and unwinds
  partial demuxer, codec, frame and borrowed-stream state before rethrowing. A
  missing Unicode input can be followed by a valid open on the same recording
  context. Logo histogram calculation is a focused C++23 module using spans,
  `std::expected`, atomic validation and overflow-safe 64-bit percentile math;
  persisted edge values can no longer index outside the fixed legacy histogram.
  Windows passes **434/434** headless and **442/442** SDL tests, and the public
  non-donator application builds. Logs are
  `bin/windows-input-histogram-build23{,-gui}-test.txt` and
  `bin/windows-input-histogram-public-build.txt`. Linux verification remains
  separate.

- Video packet submission and frame retrieval now have an explicit C++ status
  boundary: successful frames, `EAGAIN` and EOF are distinct outcomes, while
  every real FFmpeg failure throws an owned typed diagnostic. This replaces the
  decoder's former silent packet loss. Windows passes **436/436** headless and
  **444/444** SDL tests, and the public non-donator application builds. Logs are
  `bin/windows-video-status-build23{,-gui}-test.txt` and
  `bin/windows-video-status-public-build.txt`. Linux verification remains
  separate.

- Command-line transport-stream PID parsing now uses C++23 `std::expected` and
  `std::from_chars`, requires the complete hexadecimal value and enforces the
  13-bit PID range. The real executable rejects malformed suffixes through the
  localized CLI error path. Windows passes **443/443** headless and **451/451**
  SDL tests, and the public non-donator application builds. Logs are
  `bin/windows-pid-parser-build23{,-gui}-test.txt`. Linux verification remains
  separate.

- Decoder packet processing now returns explicit frame, analysis-complete,
  self-test-complete and positioning-failure outcomes; callers own process-exit
  policy and the formerly unreachable positioning branch is gone. Tuning and
  training output uses the checked owned-file boundary rather than dereferencing
  failed opens or silently ignoring writes and closes. Windows passes **445/445**
  headless and **453/453** SDL tests, and the public non-donator application
  builds. Logs are `bin/windows-decoder-training-build23{,-gui}-test.txt`.
  Linux verification remains pending.

- Logo-search and cutpoint diagnostics in `detection.cpp` now use the English
  and Spanish catalogs with frame widths and three-decimal timestamps formatted
  at their call sites. The reproducible literal-message inventory is down to
  **148** active sites. Optional media dumps now accept spans and propagate
  owned open/write/close diagnostics. Windows passes **445/445** headless and
  **453/453** SDL tests, and the public non-donator application builds. Logs are
  `bin/windows-dumps-detection-build23{,-gui}-test.txt`. Linux verification is
  deferred to the final implementation stage.

- Detection histogram, volume-plateau and silence diagnostics now use the
  English and Spanish catalogs. Aspect ratios, frame counts, cumulative
  percentages and thresholds retain their call-site formatting. The
  reproducible literal-message inventory is down to **139** active sites.
  Frame CSV output is split into a span/ostream serializer with classic-locale
  numeric output, preflight validation and checked stream status. Volume
  histogram indexing uses a checked `std::expected` conversion. Windows passes
  **452/452** headless and **460/460** SDL tests, and the public non-donator
  application builds. Logs are `bin/windows-frame-csv-build23{,-gui}-test.txt`.
  Linux verification is deferred to the final implementation stage.

- All inventoried human-facing diagnostics in `detection.cpp` now use the
  English and Spanish catalogs; numeric histogram/transcript payloads retain
  their machine-oriented format. The reproducible literal-message inventory is
  down to **123** active sites. Diagnostic histogram construction is a focused
  span/`std::expected` component with owned star strings and defined empty-data
  behavior. Final-run footer output is a checked standard-stream module.
  Windows passes **459/459** headless and **467/467** SDL tests, and the public
  non-donator application builds. Logs are
  `bin/windows-histogram-runlog-build23{,-gui}-test.txt`. Linux verification is
  deferred to the final implementation stage.

- Detection block reports, aspect-ratio cleanup reasons, the caption transcript
  heading and final frame count now use the English and Spanish catalogs. The
  detailed transcript payload remains stable. No active human-message literals
  remain in `detection.cpp`; the reproducible inventory is down to **123**
  active sites. This stage is included in the complete Windows verification
  below.

- Runtime allocation diagnostics and CSV load/review lifecycle messages now use
  the English and Spanish catalogs. Their English text and exit behavior remain
  unchanged. The reproducible literal-message inventory is down to **113**
  active sites. Allocation operations use a C++23 `std::expected` boundary so
  `std::bad_alloc` reaches the existing resource-specific message and exit
  status. Cutscene samples now use a focused standard-stream codec with an
  explicit signed 32-bit little-endian header, bounded payloads, transactional
  loading and checked save failures. All **468/468** Windows headless and
  **476/476** SDL tests pass, and the public non-donator application builds.
  Linux verification is deferred to the final implementation stage.

- Persisted caption replay now uses a focused C++23 stream codec returning
  `std::expected`, with owned packet payloads, `std::from_chars`, exact framing
  and a caller-provided allocation bound. The CSV adapter processes packets
  sequentially without raw reads, `sscanf` or control-flow jumps. C-backed
  iostream adapters now preserve underlying read failures through `badbit`
  exceptions. Both input files are validated before settings or observations
  are published. Caption type reporting returns owned strings and active
  compiled source no longer uses `sprintf`. Windows passes **477/477** headless
  and **485/485** SDL tests, and the public non-donator application builds.

- Black-frame validation, threshold selection and missing-audio diagnostics in
  `blocks.cpp` now use the English and Spanish catalogs. Detection-reason labels
  are translated as complete catalog values while English spacing and numeric
  widths remain stable at the call sites. The reproducible literal-message
  inventory is down to **104** active sites. A focused active-span helper also
  prevents the final black-frame run from inspecting successor storage outside
  the published observation range. Windows passes **477/477** headless and
  **485/485** SDL tests. Linux verification remains deferred to the final stage.

- Analysis retry, self-test and completion diagnostics now use the English and
  Spanish catalogs. The English catalog preserves the existing line breaks and
  numeric widths, with C++23 `std::format` replacing width-sensitive variadic
  formatting at the call sites. No active human-message literals remain in
  `analysis.cpp`; the reproducible inventory is down to **96** active sites.
  The read-error path also checks FFmpeg's optional I/O context before reading
  its EOF flag. Windows passes **490/490** headless and **498/498** SDL tests,
  and the public non-donator application builds. Linux verification remains
  deferred to the final implementation stage.

- Detector-buffer growth diagnostics now use the English and Spanish catalogs,
  reducing the reproducible literal-message inventory to **88** active sites.
  Storage initialization uses C++23 designated initialization and focused
  defaults instead of legacy detector macros. Reused records are reset to
  deterministic values while neighboring observations remain intact; focused
  tests cover rejection of negative indices, growth/capacity publication,
  preserved observations and value-initialized spare records. Windows passes
  **490/490** headless and **498/498** SDL tests, and the public non-donator
  application builds. Linux verification remains deferred to the final stage.

- Scene-analysis aspect, channel, cutscene, credit, frame-classification and
  resolution diagnostics now use the English and Spanish catalogs. C++23
  `std::format` preserves the established English widths and precision before
  translated templates arrange the values. The four repeated brightness-index
  guards are replaced by one always-active `std::expected` validator with
  focused boundary tests. No active human-message literals remain in
  `scene_analysis.cpp`; the reproducible inventory is down to **70** active
  sites. Decoder header-position capture now preserves the prior position for
  custom or non-seekable FFmpeg inputs without an I/O context. Windows passes
  **490/490** headless and **498/498** SDL tests, and the public non-donator
  application builds. Linux verification remains deferred to the final stage.

- Audio-analysis channel, sample-rate, timestamp, AC3 and decoder-format
  diagnostics now use the English and Spanish catalogs. C++23 `std::format`
  preserves the established decimal widths before translated templates arrange
  the values. AC3 staging now rejects a corrupted negative index before array
  indexing; an actual packet-processing regression verifies the localized
  warning and reset. The reproducible literal-message inventory is down to
  **60** active sites. Windows passes **494/494** headless and **502/502** SDL
  tests, and the public non-donator application builds. Linux verification
  remains deferred to the final implementation stage.

- Live-detection progress diagnostics now use the English and Spanish catalogs.
  C++23 `std::format` preserves the established two-decimal duration layout
  before translated templates arrange the values. The live output boundary now
  writes `.incommercial` when it is the only selected output, while retaining
  checked cut-list and XML writes. A focused integration regression covers the
  standalone status-file path. The reproducible literal-message inventory is
  down to **45** active sites. Windows passes **494/494** headless and
  **502/502** SDL tests, and the public non-donator application builds. Linux
  verification remains deferred to the final implementation stage.

- Frame-rate adjustment and stream-rate fallback diagnostics now use the
  English and Spanish catalogs. C++23 `std::format` preserves the established
  three-decimal, five-character FPS field before translated templates arrange
  the values. The reproducible literal-message inventory is down to **38**
  active sites. Linux verification remains deferred to the final implementation
  stage.

- Remaining black-frame removal, reporting, block construction, merge and logo
  diagnostics in `blocks.cpp` now use the English and Spanish catalogs. The
  English catalog retains the existing tables, labels, numeric widths and
  five-decimal logo quality by formatting the number at the C++23 call site.
  Both contiguous black-frame loops now check that a successor observation is
  active before reading it; a regression uses exact-capacity storage for the
  final active observation. The reproducible literal-message inventory is down
  to **28** active sites.

- Legacy settings input, active-detector selection and settings-heading
  diagnostics now use the English and Spanish catalogs. The English catalog
  preserves tabs, line breaks, labels and method numbering. The reproducible
  literal-message inventory is down to **17** active sites. Focused catalog
  coverage asserts both the exact legacy English layout and Spanish output.

- The retained scoring diagnostic branches now use the English and Spanish
  catalogs, including the shared score-before and score-after entries. C++23
  `std::format` keeps the established two-decimal score representation at the
  call sites. The English catalog retains the original wording exactly. The
  reproducible literal-message inventory is down to **1** call site: the
  application error prefix. Windows passes **498/498** headless and
  **506/506** SDL tests, and the public non-donator application builds. Linux
  verification remains deferred to the final implementation stage.

- Byte-seek sizing now treats FFmpeg I/O state as optional across initial and
  refinement seeks, with focused missing-I/O regressions. CMake exposes the
  opt-in `COMSKIP_ENABLE_SANITIZERS` configuration for supported compilers;
  the focused Windows AddressSanitizer seek tests pass. A separate configured
  missing-input path fails only under the current Windows sanitizer runtime and
  is tracked as B110. Windows passes **501/501** headless and **509/509** SDL
  tests, and the public non-donator application builds. Linux verification
  remains deferred to the final implementation stage.

- Automatic brightness and uniformity thresholds validate every histogram bin
  and accumulated count before opening a training CSV or scanning for a
  percentile. Empty and negative input now produces the established typed
  diagnostic; wide accumulation preserves the legacy first-uniform-bin rule.
  All **11** focused Windows output-diagnostics tests pass. Linux verification
  remains deferred to the final implementation stage.

- The current Windows branch passes the complete headless CTest target
  **513/513** and the SDL/GUI target **517/517** with dummy video and audio
  drivers. These runs include generated-media, localization, subtitle,
  output, repeated-analysis, and resource-cleanup tests. Linux, sanitizer,
  and macOS execution remain separate verification work.

- Platform compatibility no longer exposes the obsolete C-only boolean and
  unqualified `min`/`max` helpers. Timing and startup logs use checked writes,
  and startup time conversion uses thread-safe platform primitives. Focused
  platform tests pass **3/3**; the complete Windows headless and SDL suites
  pass **513/513** and **517/517**. Linux, sanitizer, and macOS execution
  remain separate verification work.

- Scene, logo, and AC3 buffers now use typed C++ algorithms for non-overlapping
  copies and clearing, and optional logo deletion uses RAII. Caption dictionary
  parsing normalizes CRLF entries with a regression test. The complete Windows
  headless and SDL suites remain green at **513/513** and **517/517**.

- The obsolete `WRITEPATTERN` training branch was removed, leaving one checked
  training serializer. Startup and footer timestamps now return owned strings
  through `ctime_s`/`ctime_r`, with platform tests covering both conversion and
  formatting. The complete Windows suites pass **513/513** headless and
  **517/517** with SDL dummy drivers.

- Active FFmpeg setup, recording input, codec iteration, seeking, audio staging,
  caption parsing, and packet checks now use `nullptr` and typed opaque state
  instead of C `NULL` casts. The complete Windows suites remain green at
  **513/513** headless and **517/517** SDL tests.

- CSV replay now uses explicit C++ casts at the legacy C boundary for frame
  dimensions, caption counts, and black-frame insertion. The focused replay
  tests pass **24/24**, and the complete Windows suites remain green at
  **513/513** headless and **517/517** SDL tests.

- Frame-rate updates now reject zero, negative, and non-finite frame periods
  before calculating a new rate. Audio and video timing arithmetic uses named
  C++ casts and `std::fabs` at the FFmpeg boundary. The focused timing tests
  pass **4/4**, while the complete Windows suites pass **514/514** headless
  and **517/517** SDL tests.

- FFmpeg container durations remain in double precision instead of narrowing
  through `float`, and cut-list output uses explicit casts for legacy numeric
  fields. The complete Windows headless suite passes **514/514** after these
  output and media-boundary cleanups.

- Media input retries now use a structured loop instead of a `goto`, retaining
  the existing retry limit, delay, and owned FFmpeg diagnostic path. The
  complete Windows headless suite passes **514/514** after this change.

- Optional live `.incommercial` output now handles an open failure with an
  explicit conditional instead of a skip label, keeping the failure nonfatal
  while making file ownership and the success path clear. The complete
  Windows headless suite remains green at **514/514**.

- Live interval arithmetic now uses explicit C++ conversions for rate, frame,
  and timestamp values at the legacy output boundary. The complete Windows
  headless suite remains green at **514/514**.

- Decode-loop seek correction, packet skipping, and reopen retries now use
  structured `continue` flow instead of labels and gotos. The complete
  Windows headless suite remains green at **514/514**.

- Fatal application diagnostics still go to stderr, and now also mirror to an
  already-open run log through the checked file boundary. A log-write failure
  cannot mask the original diagnostic or exit status. The complete Windows
  headless suite remains green at **514/514**.

- Seek fallback retries and review packet skips now use structured loops rather
  than labels. Byte-seek fallback, decoder flushing, and packet ownership stay
  unchanged; the complete Windows headless suite remains green at **514/514**.

- Audio packet resend/drain handling now uses a structured retry loop instead
  of a label, preserving EAGAIN recovery and borrowed packet ownership. The
  complete Windows headless suite remains green at **514/514**.

- The settings parser now returns directly when argtable allocation fails;
  the existing RAII argtable owner still performs cleanup. The complete
  Windows headless suite remains green at **514/514**.

- Aspect-ratio block normalization now repeats through an explicit changed-pass
  loop when merges or undefined-ratio fixes mutate storage, replacing restart
  labels while preserving the existing ordering. The complete Windows
  headless suite remains green at **514/514**.

- Volume-threshold estimation and fallback scanning now use bounded structured
  retry passes instead of labels, preserving the existing threshold escalation
  rules. The complete Windows headless suite remains green at **514/514**.

- CSV review reload now reopens the owned CSV and companion caption files from
  the recording basename before restarting parsing. The focused input suite
  passes **10/10**, and the complete Windows headless suite passes **515/515**.

- CSV review reload now starts a fresh `ProcessCSV` pass instead of jumping to
  a consumed parser label, resetting per-pass locals while retaining the
  reopened owned inputs. The complete Windows headless suite remains green at
  **515/515**.

- The final fallback application diagnostic now uses standard C++ stream output
  instead of a C formatter; translated diagnostics and run-log mirroring are
  unchanged. The complete Windows headless suite remains green at **515/515**.

- Review plotting and pixel sampling now use explicit C++ conversions at the
  SDL rendering boundary. The complete Windows SDL suite remains green at
  **517/517**.

- Block confidence, non-uniform causes, logo quality, and scene-rate arithmetic
  now use explicit C++ conversions. The complete Windows headless suite
  remains green at **515/515**.

- Scene-analysis geometry, histogram, and cut-scene calculations now use
  explicit C++ conversions. The complete Windows headless suite remains green
  at **515/515**.

- Logo geometry, logo ratios, detection percentages, and scoring arithmetic now
  use explicit C++ conversions. The complete Windows headless suite remains
  green at **515/515**.

- Review rendering coordinates, frame navigation, marker midpoint calculations,
  and playback timing now use explicit C++ conversions. The complete Windows
  SDL suite remains green at **517/517**.

- Remaining active conversions in analysis, captions, live progress, scene
  sampling, block reporting, and detection paths now use explicit C++ casts.
  The complete Windows headless suite remains green at **515/515**.

- Logo edge-mask cleanup now uses a structured found flag and loop break rather
  than a label jump; unused scan callback arguments are explicitly discarded.
  The complete Windows headless suite remains green at **515/515**.

- Remaining active block and caption arithmetic now uses explicit C++
  conversions. The complete Windows headless suite remains green at **515/515**.

- Media timing, seeking, audio alignment, scoring, and detection comparisons
  now call the standard C++ math overloads explicitly. The complete Windows
  headless suite remains green at **515/515**.

- Caption dictionary, platform file-mode, and codec listing paths now use
  standard C++ string-length operations. The complete Windows headless suite
  remains green at **515/515**.

- Codec enumeration output now uses standard C++ streams while preserving the
  existing localized text and line wrapping. The complete Windows headless
  suite remains green at **515/515**.

- CSV argument and close-window console output now uses standard C++ streams.
  The complete Windows headless suite remains green at **515/515**.

- Legacy command-line argument display and screen-only frame diagnostics now
  use standard C++ streams while preserving their text layout. The complete
  Windows headless suite remains green at **515/515**.

- `comskip-check` now builds all registered test executables before CTest,
  copies the Clang ASan runtime on Windows, and applies non-interactive
  sanitizer logging. A fresh sanitizer build reaches all 515 tests; the known
  Windows ASan failure-unwind limitation remains tracked as B110.

- The current Windows SDL configuration passes **523/523** tests after the
  fresh-build dependency change; the headless configuration remains **515/515**.

- Frame conversion now reuses the shared `FramePtr` FFmpeg RAII owner instead
  of defining a second local deleter. Focused conversion tests pass **2/2** and
  the complete Windows headless suite remains **515/515**.

- Caption and standalone subtitle decoding now share one non-copyable
  `SubtitleOwner` for `AVSubtitle` cleanup. The focused decoder suite passes
  **12/12**.

- Standalone subtitle decoding now uses the shared `CodecParametersPtr` owner
  for copied FFmpeg parameters. Its focused suite passes **6/6**.

- Frame conversion now accepts `ScalerPtr&` directly; the video decoder no
  longer releases the scaler to a raw pointer at the call boundary. Focused
  conversion tests pass **2/2**.

- Audio normalization now uses the shared `ResamplerPtr` FFmpeg owner instead
  of a module-local deleter. Focused audio tests pass **2/2**.

- FFmpeg search-path, score-threshold, and logo-histogram diagnostics now use
  committed English/Spanish catalog entries. The new localization regression
  passes **1/1**.

- Histogram headings and frame-output status text now use the same catalog
  boundary; the localization regression remains green.

- Subtitle output now uses the shared `OutputFormatPtr` FFmpeg owner for
  `AVFormatContext` and `AVIOContext` cleanup. Focused subtitle output tests
  pass **7/7**.

- FFmpeg metadata sidecars now use the shared `DynamicOutputFormatPtr` owner
  for dynamic output buffers. Focused sidecar tests pass **7/7**.

- `OutputHistogram` now accepts a bounded `std::span<const int>` and
  `std::string_view`, rejecting incomplete input before processing. Its focused
  regression passes **1/1**.

- Timing diagnostics now accept `std::string_view` for row labels while
  retaining the exact CSV layout.

- Reference input loading now accepts `std::string_view` for the sidecar
  extension, removing another null-terminated string contract.

- Cut-scene loading now accepts `std::string_view` for UTF-8 filenames,
  preserving the existing path conversion and localized errors.

- Logo edge detection and comparison now accept bounded read-only pixel spans,
  validating storage before indexing instead of relying on raw pointers.

- Logo mask cleanup, bounds, and diagnostic dumping now use bounded spans for
  their pixel buffers as well.

- Localized debug helper message IDs now use `std::string_view` consistently,
  keeping catalog identifiers independent of null-terminated storage.

- Commercial profile parsing now uses `std::string_view` for INI keys and
  explicitly owns diagnostic arguments. Focused settings/profile tests pass
  **4/4**, and the complete Windows headless suite passes **517/517**.

- Saved-logo metadata parsing now uses `std::string_view` for INI keys and
  owns the reported key in range diagnostics. The focused logo suite passes
  **53/53**, and the complete Windows headless suite remains **517/517**.

- Platform file opening now exposes a bounded C++ `std::string_view` API that
  validates embedded NULs before crossing the legacy UTF-8 C boundary. Output
  open helpers use it directly; focused platform/output coverage passes
  **12/12**.

- Diagnostics, timing, caption, runtime, and detection output paths now use
  the bounded platform opener instead of manufacturing temporary C strings.
  Focused coverage passes **30/30**.

- CSV replay, live output, logo persistence, and review-file paths now use the
  same bounded opener. Focused coverage passes **22/22**.

- Legacy settings and cutlist output now use the bounded opener throughout;
  only the narrow C compatibility bridge retains C strings. Focused settings,
  diagnostics, and cutlist coverage passes **23/23**.

- Media packet processing, seeking, audio analysis, and file opening now use
  references wherever `VideoState` ownership is guaranteed. The picture decode
  interface no longer carries an unused file handle, and the EDL/file-stream
  adapters plus CLI diagnostics require valid non-null `FILE` references at
  their boundaries. Audio packets and decoded frames use the same explicit
  non-null contract, and `SubmitFrame` no longer carries an unused stream
  parameter.
  The complete Windows CTest target passes **523/523**.

- Caption transcript rows now use English/Spanish catalog entries while
  preserving their fixed-width machine-readable layout. Localization and
  detection coverage passes **40/40**.

- Cutlist threshold and heuristic deletion diagnostics now use catalog-backed
  English/Spanish messages while preserving numeric formatting. Focused
  localization and cutlist coverage passes **37/37**.

- Cutlist keep-first/keep-last decisions and final-list status messages now
  use the same catalogs, including Spanish wording, with the existing output
  layout preserved.

- FFmpeg dynamic sidecar buffers now use the shared `BufferPtr` RAII owner
  instead of a local deleter. Focused sidecar coverage passes **13/13**.

- The debug sink now accepts `std::string_view` directly, so translated
  messages avoid the variadic `"%s"` bridge. A regression confirms literal
  percent characters are preserved; focused diagnostics coverage passes
  **25/25**.

- Runtime, decoder setup, recording input, timing, audio, playback, CSV, and
  analysis callsites now pass translated strings directly to the type-safe
  sink. The focused integration coverage passes **45/45**.

- Legacy configuration and diagnostic output callsites now use the same direct
  string-view sink. Focused configuration/output coverage passes **55/55**.

- Detection, caption-dictionary, cutlist, and histogram diagnostics now pass
  translated strings directly as well. Focused coverage passes **60/60**;
  fixed-format diagnostic rows remain on the legacy formatter for layout
  compatibility.

- Cutlist headings, total-length summaries, and cut-code legends now use the
  English/Spanish catalogs while preserving the existing output layout.

- Active cutlist H6 diagnostics and verbose statistics now use the same
  catalogs, with preformatted numeric fields preserving the legacy widths.
  Focused localization and cutlist coverage passes **37/37**.

- `LoadSettings` now returns `void`; its obsolete borrowed `FILE*` result was
  removed because recording-owned input state already carries the resource.
  Settings-focused coverage passes **10/10**, and the full Windows suite passes
  **521/521**.

- Legacy time formatting now returns owned `std::string` values, with the
  frame variant depending only on an explicit FPS value. The shared scratch
  buffer was removed; the focused regression and cutlist suite pass **6/6**.

- C++ file-opening callsites now use `open_file_owned` and receive `FilePtr`
  directly, keeping the raw `FILE*` bridge at the platform boundary. Platform,
  diagnostics, and cutlist ownership coverage passes **26/26**; the full
  Windows suite passes **523/523**.

- Media and output boundaries now use scoped result types and optional state:
  stream opening returns `comskip::media::StreamOpenResult`, audio-volume and
  codec history no longer use sentinels, and output adapters represent missing
  prior intervals with `std::optional`. The complete Windows suite remains
  green at **523/523**.

- `VideoState` now represents optional audio, video, and subtitle stream
  selections with `std::optional<int>`. FFmpeg calls unwrap an explicitly
  selected stream only at the API boundary, removing the shared `-1` stream
  sentinel. The complete Windows suite passes **523/523**.

- The transient decoded-pixel view and logo-ring cursor now use `std::span` and
  `std::optional<int>` respectively. Empty views and uninitialized ring state
  are explicit, while the existing borrowed FFmpeg frame lifetime is retained.
  The complete Windows suite passes **523/523**.

- Audio accumulation now tracks a checked sample count rather than a raw write
  pointer into the fixed sample buffer. Compaction uses an indexed range copy,
  and overflow recovery resets the count explicitly. The complete Windows suite
  passes **523/523**.

- The owned audio sample buffer is now a `std::array`, with its capacity derived
  from the container type. FFmpeg-facing packet storage remains a C-compatible
  buffer only where the library API requires it. The complete Windows suite
  passes **523/523**.

- AC-3 packet staging is also owned by `std::array`; `.data()` is used only at
  FFmpeg and C-library boundaries, and capacity derives from the container.
  The complete Windows suite passes **523/523**.

- Cut-scene matching now accepts a `std::span<const unsigned char>` view, so
  the detector boundary carries the borrowed sample extent instead of a raw
  array pointer. The complete Windows suite passes **523/523**.

- Removed unused duplicate stream-index fields, placeholder state members, and
  an unreferenced rating-system table from `RecordingState`. `VideoState` is
  now the single owner of selected media streams. The complete Windows suite
  passes **523/523**.

- Internal scan-line bounds now use `std::array<int, 4800>`, retaining the
  existing fixed capacity while making range and size operations container
  based. The complete Windows suite passes **523/523**.

- Scene-analysis histogram storage now uses nested `std::array` containers;
  optional frame histogram publication uses `std::ranges::copy` instead of raw
  byte copying. The complete Windows suite passes **523/523**.
