# Bug log

Record newly discovered issues here even when fixing them is deferred. Keep
confirmed defects separate from suspected gaps. Close entries only with a fix
and relevant verification; retain the evidence for future regressions.

## Status

Evidence below describes each issue when it was discovered. This table records
the current resolution; Windows-only results do not establish sanitizer safety.

| Issues | Current evidence |
| --- | --- |
| B001, B004, B005, B007–B011 | Fixed at `3cc57ed`; actual caption lifecycle/replay and warning/logging tests pass on Windows. |
| B002 | Global writer removed at `3cc57ed`; the unmodified `d9e1ed1` snapshot passes all 210 Linux address/undefined/leak sanitizer tests, including actual caption lifecycle, reopen and failure cleanup. |
| B003, B013, B014 | Fixed at `12be69c`; all 174 Windows tests pass. Linux bridge dependency was also verified on the isolated patched snapshot. |
| B006 | Fixed at `3cc57ed`; both failure branches verified by four warning tests at `7e011c3`. |
| B012, B015, B016, B017, B019 | Fixed at `c4ab1e0`; all 181 Windows tests pass. The unmodified `d9e1ed1` snapshot passes all 210 Linux headless, SDL and address/undefined/leak sanitizer tests, including exact CSV/subtitle roundtrips and caption overlap/reopen paths. |
| B020 | Fixed at `589fc7b`; all six settings-value tests pass on Windows. |
| B021 | Overflow fixed at `8a4bda6`; three focused Windows tests pass. Full path support is tracked separately as B023. |
| B022 | Fixed at `54470db`; all six diagnostic-output tests pass, including flush and file removal after disabling demux. |
| B018 | Fixed at `bbbf019`; all 206 Windows tests pass, including six actual input and eight pure parser tests. Its unmodified snapshot passes all 202 Linux tests in headless, SDL, and address/undefined/leak sanitizer builds. B029 remains separate. |
| B023, B027, B028 | Fixed at `34869fa`; all 210 Windows tests pass. The unmodified `d9e1ed1` snapshot passes all 210 Linux headless, SDL and address/undefined/leak sanitizer tests, including actual input/INI/output Unicode paths above 1,024 bytes, optional marker failure, and caption controls/row splitting. |
| B024 | Fixed at `4839fee`; unsafe conversions and argument counts are rejected, with actual escaped-template output compatibility. All 199 Windows tests pass at that stage. |
| B025 | Fixed at `ed8649b`; five actual lifecycle tests pass on Windows. The isolated Linux `c4ab1e0` snapshot plus only that packet patch passes all 177 address/undefined/leak sanitizer tests; the unmodified `d9e1ed1` snapshot passes all 210. Neither run has findings or suppressions. |
| B026 | Fixed at `0f98693`; all 200 Windows tests pass, including zero, negative, and excessive observation counts. |
| B029, B030 | Fixed at `1e8f795`; all 226 Windows tests pass, including six actual reference and six block tests. Its isolated, unmodified Linux snapshot passes all 222 address/undefined/leak sanitizer tests without findings or suppressions (56.68 seconds). |
| B031, B032, B033, B037 | Fixed at `931ee71`; all 256 Windows headless and SDL tests pass. Eight storage/live cases cover 100,001 entries, safe insertion, empty live status, complete classification reset, and enabled/disabled logo and silence filtering. Its unmodified Linux snapshot passes all 252 headless, SDL, and address/undefined/leak sanitizer tests without findings or suppressions. Navigation was separately fixed at `e520374`. |
| B034 | Fixed at `e520374`; six frame-mask tests and settings validation pass within all 241 Windows tests. Its unmodified Linux snapshot passes all 237 headless, SDL, and address/undefined/leak sanitizer tests. |
| B035 | Fixed at `931ee71`; three consecutive-stall tests cover threshold, periodic reporting, progress reset, and independence; the integrated application passes all 256 Windows tests. |
| B036 | Fixed at `931ee71`; all 256 Windows headless tests and all 256 tests in a fresh, unmodified Windows SDL `build-gui` snapshot pass. All six previously timed-out CLI media tests now complete; the actual directory-name regression and filename conventions are covered. |
| B038, B039, B040, B042, B043 | Fixed at `b1f9e86`; all 270 Windows tests pass, including checked brightness/geometry, actual zero-border/empty scenes, fractional-rate CSV replay, and invalid-rate rejection before state mutation. The unmodified `07aa466` snapshot also passes all 275 Linux headless, SDL, and address/undefined/leak sanitizer tests. |
| B041, B044 | Fixed at `07aa466`; all 279 Windows headless and all 279 unmodified Windows SDL snapshot tests pass. Nine geometry/filter tests cover extreme settings, ordinary edges, short/full histories, required buffers, and persisted bounds. Its unmodified snapshot passes all 275 Linux headless, SDL, and address/undefined/leak sanitizer tests without findings or suppressions. |
| B045 | Fixed at `706801f`; all 285 Windows tests pass, including an actual decoder reset/reopen regression and six timing-output tests. |
| B046, B048, B049, B050, B052, B053, B054 | Fixed at `62664db`; all 319 Windows headless and SDL tests pass. Its unmodified snapshot passes all 315 Linux headless, SDL, and address/undefined/leak sanitizer tests. |
| B047, B055, B056 | Fixed in the font/settings stage; all 326 Windows headless and 329 SDL tests pass, including a relocated executable, long Unicode configured cutscene path, and duration overflow rejection before settings publication. |
| B057, B058 | Fixed in the script/diagnostic stage; all 347 Windows headless and 351 SDL tests pass, including actual early-cut joins and later-frame bright-pixel classification. |
| B051 | Open: review/caption/subtitle and argument/format/pixel/runtime reasons are cataloged; other application/helper reasons remain. |
| B059–B063 | Fixed in the logo/player/application stage; all 368 Windows headless and 372 SDL tests pass. Linux proof of this stage remains separate. |
| B064–B066 | Fixed in the diagnostic/progress stage; all 379 Windows headless and 383 SDL tests pass and the public-speed application builds. |
| B067 | Fixed at `66d45a8`; actual Linux SDL invalid-font CLI and relocated-font tests pass. One separate fixture issue B071 prevents its full suite from passing. |
| B068–B070 | Fixed in the editor/geometry/codec stage; all 398 Windows headless and 402 SDL tests pass. |
| B071 | Cross-version fixture corrected; Windows passes. Corrected Linux verification pending. |

## Issue evidence and verification

### B001: Subtitle output decoder shares state between recordings

- **Evidence:** `third_party/ccextractor/ccextractor.c:134` declares global
  `wbout1`/`wbout2`. `CEW_init` is called from application settings loading,
  while media decoding calls global `process_block`/`CEW_reinit`.
- **Impact:** Independent subtitle-enabled analyses can replace each other's
  output and decoder state. Current captionless repeated-analysis tests do not
  cover this path.
- **Fix:** Replace supporting subtitle decoding/output with recording-owned
  FFmpeg resources while retaining detector/XDS observations.
- **Verification needed:** Two interleaved subtitle-enabled recordings with
  distinct cues and destinations, failure cleanup, seeking/reset, and EOF cues.

### B002: Repeated subtitle initialization loses writer allocations

- **Evidence:** `init_write` at `third_party/ccextractor/ccextractor.c:403`
  allocates `buffer` and `data608`; `CEW_init` calls it for both global writers.
  Reinitialization overwrites these pointers without freeing their old values.
- **Impact:** Repeated subtitle-enabled application invocations leak writer
  buffers and decoder state; unchecked allocation failures can also reach
  `init_eia608` with a null pointer.
- **Fix:** Eliminate the global writer lifecycle as part of B001.
- **Verification needed:** Leak sanitizer coverage of repeated subtitle-enabled
  success/failure runs and automatic resource cleanup.

### B003: Selected standalone subtitle streams are not consumed

- **Evidence:** The media coordinator selects a subtitle stream and records its
  PID, but the active caption path consumes video-frame A53 side data. The audit
  found no standalone subtitle decoder initialization. Both active packet
  dispatch loops handle video/audio and discard the selected subtitle packets.
- **Status:** Confirmed missing routing/decoding path. Text/ASS subtitles can
  use the owned FFmpeg subtitle output. Bitmap subtitles need a separate
  supported-output policy; FFmpeg does not convert their images to text.
- **Verification needed:** A recording with a standalone subtitle stream and
  no embedded A53 captions; compare requested subtitle output with its cues.

### B004: Caption dump replay trusts persisted payload lengths

- **Evidence:** `src/app/csv_input.cpp` reads the companion `.data` length with
  `sscanf`, then passes `ccDataLen` to `fread` targeting the fixed 500-byte
  `ccData` buffer without validating its range or the complete record.
- **Impact:** Oversized or negative lengths can overrun the buffer; truncated
  records can feed incomplete caption data into the detector/exporter.
- **Status:** Fixed. A focused C++23 stream codec validates exact framing and
  the caller's payload capacity before allocation or copying. CSV replay reads
  the complete companion into bounded owned packets before publishing any CSV
  settings or observations.
- **Verification:** Actual replay regressions cover oversized, negative and
  truncated companion records and prove prior settings and observations remain
  unchanged. All **477/477** Windows headless and **485/485** SDL tests pass,
  and the public non-donator application builds.

### B005: Failed cutscene loading passes an integer as a filename

- **Evidence:** `LoadCutScene` in `src/detection/scene_analysis.cpp` supplies
  `c, filename` to an error format containing only one `%s` placeholder.
- **Impact:** An empty/truncated cutscene file can cause invalid pointer access
  in logging instead of an actionable warning.
- **Fix/verification needed:** Typed catalog formatting and an empty-file
  regression that verifies warning and released input ownership.

### B006: Failed optional logo saving writes through a null file handle

- **Evidence:** `SaveLogoMaskData` in `src/detection/logo.cpp` continues to
  `fprintf(logo_file, ...)` after failed opening when restart-after-logo is off.
- **Impact:** An unwritable logo destination can crash normal analysis.
- **Fix/verification needed:** Return after the optional-save warning; preserve
  the required-restart error status. Test both branches with a missing directory.
- **Resolution:** `3cc57ed` returns after optional failure and preserves required
  failure status 7. Both branches now have passing Windows regressions; all four
  detector warning tests pass. Cross-platform verification is still pending.

### B007: Redirected CSV replay searches the output directory for input captions

- **Evidence:** Companion caption lookup used `workbasename`, which follows
  `--output`, rather than the original CSV input basename.
- **Impact:** Redirecting analysis output can silently omit the existing input
  `.data` captions and metadata.
- **Fix:** Prefer the input-relative companion and retain legacy output-relative
  lookup as a fallback.
- **Verification needed:** Actual CSV replay into a different output directory,
  without copying the companion there, matches decoded caption output.

### B008: EOF frames without timestamps can reset the video clock

- **Evidence:** The generated MPEG2/A53 regression accepted a caption at
  4,800,000 microseconds, then completed at 39,999 microseconds. The EOF frame
  lacked `best_effort_timestamp`; normalization used a zero timestamp plus
  residual offset instead of the existing video clock.
- **Impact:** Caption completion fails and detector/CSV timelines can jump
  backwards at EOF.
- **Fix:** Use the existing clock for frames without timestamps, preserving
  timestamp/offset normalization when a timestamp is available.
- **Verification needed:** Actual MPEG2/A53 EOF cue and monotonic CSV timing,
  plus the existing media-format and EOF-drain regressions.

### B009: Diagnostic logging silently truncates long messages

- **Evidence:** `Debug` in `src/app/runtime.cpp` uses `vsnprintf` into the fixed
  recording `debugText` buffer and ignores the required-size return value.
- **Impact:** Long diagnostics, including editable catalog messages, lose their
  ending in both console and log output without reporting truncation.
- **Fix/verification needed:** Owned per-call formatted storage; exact logging of
  a Unicode message longer than the old buffer, formatting and verbosity gating.

### B010: Forward-slash input paths are not split correctly on Windows

- **Evidence:** Actual caption CSV replay using a forward-slash input path kept
  its directory inside the output basename; redirected output failed with
  creation status 6 before caption replay.
- **Impact:** Valid portable Windows paths can produce incorrect destinations
  and prevent analysis/replay.
- **Fix/verification needed:** UTF-8 `std::filesystem` path filename/parent
  extraction; actual replay using forward slashes and a different output folder.

### B011: Caption dump stays buffered after successful analysis

- **Evidence:** Replaying captions while the original recording context remained
  alive produced an empty result: its companion `.data` file was still open and
  buffered after normal decoder EOF.
- **Impact:** A completed recording's persisted captions may be unavailable to
  another analysis until the original context is destroyed.
- **Fix in progress:** Close the data output at normal EOF. The actual decoder
  and CSV replay regression now passes; full verification and commit remain.

### B012: CSV replay loses the final frame's duration

- **Evidence:** The CSV writer emits frames `1..<frame_count`. A generated
  150-frame recording ends its final decoded subtitle at 6.000 seconds, while
  replay ends it at 5.960 seconds.
- **Impact:** Replayed caption durations can be shorter by one frame; detector
  replay also lacks that final persisted observation.
- **Resolution:** Inclusive observation export and canonical replay counts are
  fixed at `c4ab1e0`. Actual application tests require exact 6.000-second EOF
  subtitles, identical EDL/TXT, and stable observation counts through repeated
  CSV exports. These pass in every `d9e1ed1` Linux configuration, including
  address/undefined/leak sanitizers.

### B013: Caption dump writing trusts its path, file handle, and length

- **Evidence:** `dump_data` in `src/output/media_dump.cpp` formats the recording
  basename into a fixed 2,000-byte array using `sprintf`, does not check the
  result of opening the output, and rejects only lengths greater than 1,900.
- **Impact:** An unwritable output can pass a null handle to `fwrite`; a long
  basename can overflow the array. Negative lengths can also become invalid
  write sizes if the function is called with malformed input.
- **Fix/verification needed:** Owned filename and bounded framing, checked file
  opening/writing, and regressions for an unavailable destination, invalid
  lengths, and valid persisted data compatibility.
- **Resolution:** `12be69c` uses owned filename/framing storage, validates the
  buffer and frame field, and handles open/write failure. Four Windows output
  regressions pass, including exact binary framing and invalid inputs. Complete
  cross-platform verification of this fix remains pending.

### B014: Caption session omits its bridge library dependency

- **Evidence:** The Linux build of `3cc57ed` fails to link
  `CaptionSession::consume_stored_packet`: `extract_a53_captions` is unresolved.
  CMake listed the bridge library before its consumer without declaring the
  dependency; Windows linking did not expose this ordering issue.
- **Fix in progress:** Link `caption_session` publicly to `media_conversion`.
  The isolated Linux headless snapshot with this exact patch passes 161 tests;
  GUI and sanitizer verification are still running.
- **Resolution:** The dependency fix is committed in `12be69c`. The isolated
  patched snapshot passes all 161 headless and SDL Linux tests. Its sanitizer
  failures concern the separate timestamp overflow tracked as B015.

### B015: Missing frame timestamps overflow during delay calculation

- **Evidence:** Linux UBSAN stops all five actual caption analysis tests at
  `mpeg2dec.cpp:1229`: subtracting a previous timestamp from `AV_NOPTS_VALUE`
  overflows before the later missing-timestamp fallback runs.
- **Fix/verification needed:** Check both timestamps before arithmetic and avoid
  overflowing subtraction for valid extreme values; rerun actual EOF tests and
  the complete sanitizer suite.

### B016: Standalone subtitle output rejects overlapping cues

- **Evidence:** The standalone decoder accepts overlapping/out-of-order cues,
  but `SubtitleOutput` requires sequential nonoverlapping intervals. The first
  actual routing fixture covers only nonoverlapping cues.
- **Impact:** Valid subtitle streams can fail export or lose simultaneously
  displayed text.
- **Fix/verification needed:** Preserve simultaneous text/styles and cue timing
  in supported SRT/SAMI output, with overlapping and out-of-order integration
  regressions plus reopen and failure cleanup checks.

### B017: Screen-only frame diagnostics print an uninitialized string

- **Evidence:** `OutputFrameArray` declares `char lp[10]` without initialization
  and passes it to `%s` in its `screenOnly` branch. No assignment occurs before
  the print.
- **Impact:** That diagnostic path can read beyond the array, display unrelated
  stack bytes, or crash while printing a frame.
- **Fix/verification needed:** Remove the obsolete string or derive an owned
  valid label; exercise screen-only output with a stored frame.

### B018: Reference and CSV text parsers have unchecked buffers

- **Evidence:** `InputReffer` uses an unchecked first `fgets` before `strlen`,
  copies unbounded tokens into `split[256]`, appends a newline to a possibly full
  `line[2048]`, and does not bound reference entry count. `ProcessCSV` also copies
  unbounded tokens into a 256-byte array.
- **Fix/verification needed:** Checked string/view parsing and numeric conversion;
  actual empty, oversized, invalid-number, missing-newline, and entry-limit cases,
  with valid legacy format compatibility.

### B019: Configured cutscene loading can exceed its eight slots

- **Evidence:** `LoadCutScene` indexes the next slot without checking the eight
  available records and ignores an incomplete brightness header read.
- **Fix/verification needed:** Capacity and complete-record checks before state
  changes, with eight/nine-file and partial-header regressions.

### B020: Negative scan borders are not rejected by settings validation

- **Evidence:** Settings validation leaves `border` unrestricted; scene scans
  use that value in direct pixel indexing.
- **Fix/verification needed:** Semantic scan-bound validation, with negative
  configuration rejection and supported boundary cases.
- **Resolution:** `589fc7b` rejects negative borders in both overrides and
  inherited settings. The regression accepts zero and a positive border and
  verifies baseline settings stay unchanged; all six settings-value tests pass
  on Windows. Recording-geometry checks remain a separate audit concern.

### B021: Review extension fallback can overflow a fixed path buffer

- **Evidence:** Review fallback replaces a filename suffix inside its fixed
  buffer; replacing a short suffix with `.dvr-ms` can exceed remaining capacity.
- **Fix/verification needed:** Owned filesystem path extension replacement and
  near-capacity/Unicode fallback regressions.

### B022: Dump cleanup depends on the current enable setting

- **Evidence:** `close_dump` returned early when `output_demux` was false,
  even if the recording already owned open audio/video dump files.
- **Impact:** Disabling dumping after files were opened could leave buffered
  output and handles live until context destruction.
- **Fix in progress:** Close owned handles unconditionally. Regression coverage
  toggles the setting after opening files and checks released file ownership.

### B023: Legacy filename fields prevent full long-path input support

- **Evidence:** `RecordingState` stores `mpegfilename`, `inbasename`, and
  `inifilename` in 260-byte arrays. The review fallback now protects copying,
  but an otherwise valid longer UTF-8 path cannot fit; CLI basename/settings
  derivation also retains those limits.
- **Resolution:** Input filename ownership is fixed at `4839fee`; the connected
  basename/config/output graph is owned at `34869fa`. Actual full CLI tests
  use Unicode input, settings and output paths each longer than 1,024 bytes,
  verify complete exports, and release files for removal. The Windows full
  stage passes 210 tests; unmodified `d9e1ed1` passes all 210 Linux headless,
  SDL and address/undefined/leak sanitizer tests. No application path-size
  ceiling is imposed; invalid filesystem destinations still report errors.

### B024: Editable output templates are unchecked printf formats

- **Evidence:** `OpenOutputFiles` passes `avisynth_options` and `dvrcut_options`
  directly as `fprintf` format strings with one and three string arguments.
  Settings validation checks `windowtitle`, but not these templates.
- **Impact:** Unexpected numeric conversions, `%n`, or excess `%s` conversions
  can read invalid arguments or write through invalid pointers.
- **Fix/verification needed:** Validate supported legacy string placeholders and
  escaping before publishing settings, or migrate to typed template formatting;
  test malformed templates and exact valid output compatibility.

### B025: Full media reopen leaks FFmpeg allocations

- **Evidence:** The unmodified `c4ab1e0` Linux sanitizer snapshot passes 176 of
  177 tests. The actual input-reopen test passes its behavior assertions, but
  LeakSanitizer reports 210 bytes in three FFmpeg allocations (24 direct,
  138 and 48 indirect).
- **Status:** Confirmed leak; precise abandoned ownership path is still under
  investigation. The missing-timestamp overflow no longer occurs.
- **Verification needed:** Reopen/unwind cleanup and the complete
  address/undefined/leak sanitizer suite without suppressions.

### B026: Frame-time lookup trusts inconsistent observation counts

- **Evidence:** `get_frame_pts` clamps to `frame_count - 1` without checking
  stored frame size. Nonempty storage with count zero indexes `-1`; a count
  larger than storage indexes beyond the vector.
- **Fix in progress:** Use the existing frame-rate fallback when observations
  are unavailable and bound valid indices to both count and stored size.
- **Verification needed:** Zero/negative counts, excessive counts, and valid
  timestamp lookup, followed by application timing regressions.

### B027: Caption-presence marker closes a null file handle

- **Evidence:** The `ccCheck` branch in `DetectCommercials` creates a `.ccyes`
  or `.ccno` file and calls `fclose` unconditionally after `myfopen`.
- **Impact:** An unavailable marker destination can crash completed analysis.
- **Resolution:** `34869fa` uses checked FilePtr marker creation and localized
  errors. The real CLI test first creates a long Unicode marker, then replaces
  it with a directory and verifies completed analysis reports the creation
  failure safely. It passes on Windows and all three `d9e1ed1` Linux builds.

### B028: Empty caption text is indexed before its length guard

- **Evidence:** Caption processing evaluates `text[text_len - 1]` before
  checking text length. A fresh control-only pair can reach this with length
  zero. The stored bytes are already unsigned, so character classification is
  not a separate signed-character defect.
- **Resolution:** `34869fa` checks text length before indexing. Empty/control-only
  and extended-byte splitting regressions pass on Windows and all three
  `d9e1ed1` Linux builds, including address/undefined/leak sanitizers.

### B029: Reference comparison trusts commercial sentinel capacity

- **Evidence:** After valid reference input, comparison indexes
  `commercial[commercial_count]` even when count is `-1`, and writes the next
  sentinel beyond the 100,000-entry array when count is 99,999. Reserving a
  reference slot does not reserve a commercial slot.
- **Fix/verification needed:** Explicit empty-list and sentinel capacity handling,
  with real reference comparison for empty and capacity-sized commercial lists.

### B030: Detection block storage has a hidden stale terminal protocol

- **Evidence:** The 1,000th completed block fails while creating its next slot
  (effective capacity 999). Three merge paths leave former real-block metadata
  in the terminal read by scoring. Initialization also leaves classification
  fields unchanged; empty `CleanLogoBlocks` indexes block `-1`.
- **Fix/verification needed:** Owned growable blocks with an explicit fully
  initialized terminal, consistent reset/removal, and empty handling. Verify
  more than 1,000 blocks, merges, recalculation, and empty/final scoring.

### B031: Commercial construction exceeds fixed interval storage

- **Evidence:** `src/output/cutlists.cpp` `BuildCommercial` increments
  `commercial_count` and writes its entry without checking the 100,000-entry
  capacity. Growable blocks now permit more than 100,000 alternating ad runs.
- **Impact:** Large interval lists write beyond recording-owned storage.
- **Fix/verification needed:** Growable commercial storage with safe append,
  covering more than 100,000 runs and downstream output consumers.

### B032: Review interval navigation and insertion trust unavailable entries

- **Evidence:** `src/ui/review.cpp` previous navigation indexes `-1` for empty
  lists or positions before the first interval; next navigation indexes past
  the last interval. Reference insertion shifts/appends beyond capacity when
  all 100,000 entries are populated.
- **Fix/verification needed:** Bounded interval navigation and owned insertion,
  tested for empty lists, both ends, and full-capacity reference editing.

### B033: Live detection indexes empty commercial lists and overfills candidates

- **Evidence:** `src/detection/live.cpp` uses
  `commercial[commercial_count]` for `output_incommercial` after resetting the
  count to `-1`, with no nonempty guard when no break is accepted. Candidate
  `c_start`/`c_end` writes append before the later commercial-capacity guard.
- **Fix/verification needed:** Owned candidate append and explicit empty state;
  test no accepted breaks and more than 100,000 candidates.

### B034: Configured frame masks can address pixels outside the frame

- **Evidence:** `src/detection/detection.cpp` applies `ticker_tape`,
  `top_ticker_tape`, `ignore_side`, `ignore_left_side`, and `ignore_right_side`
  without checking them against decoded geometry. Height+1 ticker rows and
  width+1 side masks cross allocation boundaries; percentages above 100 can
  also exceed frame rows. Settings validation currently does not reject these.
- **Fix/verification needed:** Validate settings and geometry before pixel
  access, avoid overflowing percentage multiplication, and test each excessive
  mask against real frame storage under sanitizers.

### B035: Empty-input warning counter never reaches its threshold

- **Evidence:** `src/media/mpeg2dec.cpp` increments `empty_packet_count` when
  the video clock is unchanged, checks for more than 1,000 packets, then resets
  the counter on every unchanged iteration. Starting at zero, it stays at one
  during the check, so the warning cannot fire.
- **Fix/verification needed:** Explicit consecutive-stall tracking, reset on
  clock progress, and regressions for threshold and recovered progress.

### B036: Windows SDL CLI verification times out after analysis

- **Observed failure:** The isolated, unmodified `e520374` Windows SDL build
  passes its unit/error-path tests, but `media_smoke` and `standalone_subtitles`
  CLI subprocesses exceed their 45-second deadlines. Other media tests also
  remain running. The active recovery fixture's log includes completed frame
  analysis and the end-of-run timestamp, while its process consumes little CPU.
- **Confirmed cause:** `legacy_settings.cpp` checks `GUI` and `-gui` anywhere
  in `argv[0]`, including parent directories. The snapshot's `build-gui`
  directory overrides default `output_debugwindow=0` and selects the endless
  interactive review loop. Media startup and analysis policy also inspect the
  full executable path. Linux test-directory names did not expose this trigger.
  Evidence is in `bin/windows-e520374-gui-test.txt` and the selection callsites.
- **Verification needed:** Identify the actual wait, fix its cause where needed,
  and rerun the complete Windows SDL suite. Compilation and unit success alone
  do not establish working CLI media analysis for this build.

### B037: Rebuilding commercials skips the first following program block

- **Evidence:** `BuildCommercial`'s nested ad-run loop stops on the first
  following program block, then the outer loop increments again without clearing
  that block's `iscommercial` flag. A rebuild can retain an earlier true flag.
- **Fix/verification needed:** Visit every block while grouping commercial
  runs. Seed prior classifications and verify every program flag is cleared,
  with exact interval grouping and repeated builds.

### B038: Brightness settings become unchecked histogram indices

- **Evidence:** `scene_analysis.cpp` uses `max_brightness` and
  `test_brightness` as bounds/indices of 256-entry histograms. Values 256,
  `max_brightness=-1`, or `test_brightness=-2` reach out-of-range indices.
- **Fix/verification needed:** Validate the supported 0–255 range before
  analysis, with inherited/override settings and actual scene regressions.

### B039: Border settings permit zero brightness normalization

- **Evidence:** For a 320×240 frame, `border=120` makes the first-frame
  normalization divisor zero in `scene_analysis.cpp`. Only nonnegative border
  validation exists. Smaller retained areas can also truncate to zero after /16.
- **Fix/verification needed:** Validate the retained sampling geometry and
  nonzero divisor before pixel loops/normalization, including exact boundaries.

### B040: Frame rate becomes an invalid integer logo-sampling interval

- **Evidence:** CSV replay computes modulus by `int(fps * logoFreq)`.
  Accepted `fps=0.5` yields zero when legacy CSV has no rate metadata;
  accepted finite `fps=1e20` exceeds integer-conversion range.
- **Fix/verification needed:** A checked, positive representable sampling
  interval shared by replay and detection; test metadata/inherited rates and
  fractional rates without rejecting valid media frame rates unnecessarily.

### B041: Logo scan and filter parameters overflow signed arithmetic

- **Evidence:** `scan_geometry.h` multiplies `edge_step` by four and sums
  radius/border in int arithmetic. Accepted `edge_step=536870912` or
  `edge_radius=2147483647` overflow. `logo.cpp` filter products overflow for
  accepted `logo_filter=1073741824` at ordinary fps25.
- **Fix/verification needed:** Checked derived scan/filter geometry and wide
  indices, avoiding overflowing products, with extreme settings and real-frame
  tests under sanitizers.

### B042: Zero-border directional sampling starts outside the image

- **Evidence:** `ScanTop` starts at `y=height-border-delta` and immediately
  indexes that row; border0/delta0 starts after the final row. `ScanRight`
  similarly starts at `x=videowidth` for zero border, reading padding or the
  next row and potentially crossing tight-stride storage on the final row.
- **Fix/verification needed:** Valid last-pixel coordinates at zero border,
  preserving positive-border sampling, with actual tight-stride scene tests.

### B043: Fully excluded scene samples cause zero-denominator arithmetic

- **Evidence:** When every `haslogo` pixel is excluded from scene sampling,
  `pixels` remains zero. `scene_analysis.cpp` divides integer brightness and
  uniformity by it and converts the nonfinite scene-change ratio to int.
- **Fix/verification needed:** Explicit empty-sample results and bounded
  normalization, with actual first/subsequent fully excluded frame tests.

### B044: Logo filtering assumes a complete recent sampling window

- **Evidence:** `logo.cpp` `ProcessLogoTest` clears
  `frame[framenum_real-i]` for every `i<LOGO_SAMPLE` without clipping history.
  With `logo_filter=1`, fps25, and a valid first frame numbered one, a direct
  call indexes negative frame positions. Ordinary full sampling-window calls
  do not establish safety for shorter explicit histories.
- **Fix/verification needed:** Validate the current observation and clip recent
  history before writes; test short and ordinary histories with exact filtering
  behavior and invalid counts under sanitizers.

### B045: Timing restart writes duplicate CSV headers

- **Evidence:** Decoder restart calls `open_timing_diagnostics`, which writes
  the separator/column header, then immediately calls `write_timing_header`
  again. This was also the old `DUMP_OPEN`/`DUMP_HEADER` macro behavior.
- **Impact:** Restarted timing output has two separator/column header pairs,
  disrupting ordinary CSV consumers.
- **Resolution:** Fixed at `706801f`: one header per newly truncated file.
  Six timing tests, including actual reset/reopen and row output, and all
  285 Windows tests pass. The later unmodified `62664db` snapshot passes all
  315 Linux headless/SDL/sanitizer and 319 Windows SDL tests.

### B046: Logo shrink settings can overflow frame conversions

- **Evidence:** `src/detection/logo.cpp:866–870` converts unchecked
  `shrink_logo * fps` and `shrink_logo_tail * fps` to `int`, then performs
  integer additions and doubling. Extreme finite settings can exceed `int`.
- **Resolution:** Checked offsets and wide frame arithmetic reject invalid
  settings before mutation. Eleven logo shrink/counter tests, including actual
  appearance, closure and live paths, pass within all 319 Windows tests.
- **Verification needed:** Reject unrepresentable frame offsets before mutation;
  cover extreme finite settings and ordinary logo shrink behavior.

### B047: Default review font depends on the source checkout

- **Evidence:** `CMakeLists.txt:84` embeds the source-tree NotoSans path;
  `src/ui/review_window.cpp:112` uses it as the default font.
- **Resolution:** GUI builds embed the licensed font with an owned SDL font
  stream and allow INI-configured external font overrides. A relocated test
  executable with no asset tree opens, renders, reopens and runs two independent
  windows. All 329 Windows SDL tests pass; all 326 headless tests also pass.
  The unmodified `c5c8496` snapshot passes all 325 Linux SDL tests, including
  the relocated executable (21.96 seconds).
- **Verification needed:** Run a copied installation without source assets;
  provide and verify a portable bundled-resource/default-font resolution.

### B048: Diagnostic CSV filenames are not escaped

- **Evidence:** `src/output/diagnostics.cpp:230,266,446–459` and legacy
  training writers in `src/output/cutlists.cpp` wrap filenames in quotes
  without doubling embedded quotes.
- **Impact:** Legal filenames containing quotes can corrupt CSV fields.
- **Progress:** Diagnostic histogram/quality writers now use standard-library
  doubled-quote escaping. An actual quality-output regression roundtrips a
  comma, quotes, and multiline filename through rapidcsv; all seven reference
  application tests pass on Windows. Legacy strict/training writers also use
  the helper, with actual strict output coverage; all 319 Windows tests pass.
- **Verification needed:** Parse generated output for filenames containing
  quotes, commas, and newlines; retain ordinary output compatibility.

### B049: Enabling FFmetadata changes other export formats

- **Evidence:** `src/output/cutlists.cpp:543–544` changes `start` to zero
  for early cuts while writing FFmetadata. Later writers reuse that argument.
- **Impact:** Output options can change another format's cut boundaries.
- **Resolution:** Native FFmpeg metadata export is independent of legacy
  writers; VDR/EDL start normalization also uses local values. Actual paired
  FFmetadata/ProjectX and VDR/EDL/ProjectX/BSPlayer regressions pass within
  all 319 Windows tests.
- **Verification needed:** Compare other exports with FFmetadata enabled and
  disabled for cuts starting within the first five frames.

### B050: MPEG2Schnitt opening sets the MPEG toolbox flag

- **Evidence:** `src/output/cutlists.cpp:444` sets `output_mpgtx` after
  successfully opening the MPEG2Schnitt file.
- **Resolution:** The correct MPEG2Schnitt flag is retained. An actual open
  regression verifies it does not enable MPEG toolbox; all 319 Windows tests pass.
- **Verification needed:** Independently enable each format and verify its
  settings and complete output without changing the other format's state.

### B051: Localized errors retain English application reasons

- **Evidence:** Actual Spanish CLI runs report `Configuración no válida:`
  followed by English brightness validation, and `Comskip: CSV input has no
  header` for empty CSV. Reproduction files are under the ignored
  `bin/localization-error-audit` directory. Settings interpolate raw `what()`;
  `src/app/main.cpp` destroys the context before its exception handler.
- **Progress:** Typed owned diagnostics now cover settings, parsers, geometry,
  XML, EDL and plist, preserving standard categories and English what(). Actual
  Spanish brightness/CSV/catalog/XML-destination regressions and complete
  typed-code catalog checks pass within all 319 Windows tests. Review and
  caption/subtitle reasons were migrated in the next stage; other application
  and helper reasons remain open.
- **Verification needed:** Catalog-backed typed diagnostics rendered at a
  boundary with a live translator; Spanish settings/parser/output regressions,
  English fallback, preserved exception categories and exit statuses.

### B052: XML output opening can fail without an error message

- **Evidence:** `src/output/xml_output_adapter.cpp:177` requests exit status
  6 when opening fails, without reporting the destination or reason.
- **Resolution:** Failed XML destinations report owned paths and localized
  reasons. Unit and actual Spanish CLI blocked-destination regressions pass
  with exit status 6 within all 319 Windows tests.
- **Verification needed:** An unwritable destination reports its path and a
  localized actionable reason while retaining the intended exit status.

### B053: Logo closure can clear beyond owned frame observations

- **Evidence:** Logo closure clears observations through `framenum_real` without
  checking it against owned frame storage when `framearray` is enabled.
- **Resolution:** Owned storage is checked before closure mutations. The actual
  unchanged-state regression passes within all 319 Windows tests.
- **Verification needed:** Reject missing observations before closure mutations;
  preserve ordinary closure and flag clearing.

### B054: Logo observation counters can overflow while accumulating

- **Evidence:** `src/detection/logo.cpp` adds sampling intervals to
  `frames_with_logo`, including a sampling-interval/trend-count product, using
  unchecked signed integer arithmetic.
- **Resolution:** Wide checked history, trend and single-frame additions reject
  overflow before publication. Actual retrospective appearance and unchanged
  overflow-state tests pass within all 319 Windows tests.
- **Verification needed:** Checked wide intermediate arithmetic before changing
  the active block or publishing its counter; ordinary trend startup and steady
  accumulation, extreme sampling/trend settings and existing counters.

### B055: Dynamic filename settings retain an arbitrary legacy length cap

- **Evidence:** `src/config/settings_value.cpp` rejects string settings at
  1,024 bytes except language/catalog-directory fields, despite owned dynamic
  strings. This includes cutscene paths.
- **Resolution:** Audited dynamic consumers no longer impose the obsolete cap;
  embedded-null rejection remains. Actual INI loading reads a Unicode cutscene
  path above 1,024 bytes and its complete payload. All 326 Windows headless and
  329 SDL tests pass. This was a restriction rather than a buffer overflow.
- **Verification needed:** Long Unicode configured file paths through actual
  loading/consumers, preserved embedded-null rejection, and removal of the
  arbitrary cap where no fixed-capacity consumer remains.

### B056: Additional recording minutes can overflow logo-search seconds

- **Evidence:** `LoadIniFile` multiplies int `added_recording` by 60 and adds
  it to int `giveUpOnLogoSearch` without range checking; extreme accepted
  settings cause signed overflow while loading configuration.
- **Resolution:** Chrono-based wide conversion and checked addition preserve
  ordinary extension and reject overflow during candidate validation, before
  settings publication. Three duration tests and actual invalid INI loading
  pass within all 326 Windows headless and 329 SDL tests.
- **Verification needed:** Ordinary extension/no-extension, multiplication and
  addition extremes, and rejection before publishing invalid configuration.

### B057: Early commercial cuts can concatenate AviSynth trims without a join

- **Evidence:** The legacy AviSynth writer chooses ` ++ ` from `prev < 10`
  instead of whether it already wrote a trim. Cuts 6..9 followed by 20..30
  can emit `trim(1,7)trim(11,21)` with no joining operator.
- **Resolution:** The focused AviSynth serializer joins emitted trim records,
  independent of their frame positions. Actual early-cut, normal/review and
  serial/thread export regressions pass within all 347 Windows headless and
  351 SDL tests.
- **Verification needed:** Track emitted trims independently of frame positions;
  cover early cuts, ordinary multiple trims, empty exports, and terminal ranges.

### B058: Bright-pixel threshold scaling can overflow before division

- **Evidence:** `src/detection/scene_analysis.cpp:782` evaluates
  `maxbright * width * height / 720 / 480` as signed int. Extreme accepted
  `maxbright` values overflow at ordinary image dimensions before division.
  This setting differs from `max_brightness` and `test_brightness`.
- **Resolution:** Checked int-addressable geometry and 64-bit scaling retain
  negative thresholds and division semantics. Two helper tests and actual
  later-frame positive-bright-pixel classification pass within all 347 Windows
  headless and 351 SDL tests.
- **Verification needed:** Wide scaling with checked addressable geometry,
  preserved ordinary/negative threshold math, and actual later-frame black
  classification with a positive bright-pixel count and extreme threshold.

### B059: Saved logo metadata narrows unchecked floating values to frame indices

- **Evidence:** `src/detection/logo.cpp:1744–1750` converts `FindNumber` double
  results to int dimensions and bounds before validation. A finite value such
  as `picWidth=1e20` passes the nonnegative check and exceeds the int range.
- **Status:** Fixed by staged checked metadata parsing. All 368 Windows headless
  and 372 SDL tests pass, including real saved-logo files and invalid values.
- **Verification needed:** Validate all metadata before state mutation or buffer
  allocation; cover extreme finite values, malformed input, ordinary saved
  logos, missing-property fallback, and owned file cleanup.

### B060: Saved logo loading reopens files without checking every result

- **Evidence:** After its initial metadata read, `LoadLogoMaskData` opens the
  same file again for individual masks and consumes characters without checking
  every returned FILE pointer.
- **Impact:** A file disappearing or becoming inaccessible between opens can
  reach `getc` with a null pointer.
- **Status:** Fixed using one owned stream and locally staged masks. Actual
  writer roundtrip, truncation, unchanged state, and file cleanup tests pass
  within all 368 Windows headless and 372 SDL tests.
- **Verification needed:** One owned stream across metadata and mask reads,
  missing/truncated file regressions, unchanged state and released handles.

### B061: ZoomPlayer chapter output indexes an empty commercial list

- **Evidence:** Legacy chapter initialization reads `commercial[0]` when the
  chapter stream exists, without checking whether the finalized vector is empty.
- **Impact:** A valid recording with no commercials can access absent storage.
- **Status:** Fixed by finalized-list player adapter. Empty normal/review
  exports and exact real media output pass within all 368 Windows headless
  and 372 SDL tests.
- **Verification needed:** Empty normal and review lists, ordinary first-cut
  behavior, complete chapter output and owned stream cleanup.

### B062: SCF timestamps use frame remainders as milliseconds and wrap hours

- **Evidence:** The SCF writer prints `frame % rounded_fps` as a three-digit
  decimal fraction and applies `%60` to hours. At 25 fps, nominal frame 37
  is 1.480 seconds, but the writer emits `00:00:01.012`.
- **Format reference:** The [MKVToolNix simple chapter format](https://mkvtoolnix.download/doc/mkvmerge.html#chapters.simple)
  defines these CHAPTER/CHAPTERNAME timestamp pairs with decimal-second fractions.
- **Status:** Fixed by nominal-frame to millisecond conversion without hour
  wrapping. Fractional frames, long hours and actual media bytes pass within
  all 368 Windows headless and 372 SDL tests.
- **Verification needed:** Proper fractional-frame conversion, ordinary and
  fractional rates, long hours, unchanged chapter names and numbering.

### B063: Wide command-line argument storage accepts invalid arrays

- **Evidence:** The Windows `Arguments` constructor reserved a signed count
  without checking it, dereferenced the array and each entry without validation,
  and evaluated `count + 1` in signed arithmetic.
- **Impact:** Invalid caller input can cause an excessive allocation or null
  dereference; the largest representable count overflows sentinel arithmetic.
- **Status:** Fixed by validation and size-based sentinel arithmetic. Invalid
  arrays, empty sentinel and long Unicode ownership pass within all 368 Windows
  headless and 372 SDL tests. Normal OS-provided argv is valid.
- **Verification needed:** Negative count, null array, null entry, empty argv
  sentinel, and unchanged long Unicode argument ownership.

### B064: Two chapter options share one output filename

- **Evidence:** `output_chapters` and `output_ipodchap` both write
  `outbasename + ".chap"`, despite producing different machine-readable formats.
  The legacy chapter stream remains buffered while the finalized iPod adapter
  writes that same file independently.
- **Impact:** Enabling both options can overwrite or interleave chapter output.
- **Status:** Fixed: dual-format exports use `.ipod.chap` for iPod while
  single-format exports retain `.chap`. Actual normal/review dual-format
  regressions pass within all 379 Windows headless and 383 SDL tests.
- **Verification needed:** Separate output destinations or validated mutually
  exclusive options, with actual analysis and review export regressions.

### B065: Public decode speed configuration does not compile

- **Evidence:** The `!DONATOR && !DEBUG` branch in decode progress accessed
  `is_h264` without `context.state`, two Debug calls without context, and
  codec options without their owner. The OFF build confirmed four additional
  compilation errors after the progress dependency was corrected. CMake exposes `COMSKIP_DONATOR=OFF`, but
  current canonical builds use the default ON and excluded that branch.
- **Status:** Fixed: all missing recording dependencies are explicit. The
  complete `COMSKIP_DONATOR=OFF` Release application builds successfully; logs
  are `bin/windows-progress-public-{configure,build}.txt`.
- **Verification needed:** Build the public-speed configuration explicitly.

### B066: Decode progress uses unsafe elapsed counters and conversions

- **Evidence:** Wall-clock timing narrows elapsed centiseconds to int; clock
  adjustments can produce negative intervals. Percentage calculation converts
  division by an unknown/zero media duration to int without a finite check.
  The public-speed retry label also increments the decoded-frame counter on
  each wait for the same frame.
- **Impact:** Incorrect timing/frame statistics, unsafe conversions, and
  elapsed-counter overflow on sufficiently long analyses.
- **Status:** Fixed with owned steady-clock timing, wide durations, one frame
  observation per decode and checked display calculations. Three deterministic
  tests pass within all 379 Windows headless and 383 SDL tests; the public-speed
  application builds successfully.
- **Verification needed:** Deterministic one-second reports, reset and independent
  analyses, long elapsed times, unknown durations and public-speed compilation.

### B067: Linux SDL analysis skips the review loop

- **Evidence:** The unmodified `9bcdc2e` Linux SDL suite passes 346/347 tests;
  actual CLI invalid-font regression returns normal status rather than error 2.
  SDL resources are compiled with `COMSKIP_BUILD_GUI=1`, but application review
  orchestration checks the obsolete `_WIN32 || HAVE_SDL` condition. `HAVE_SDL`
  is not defined. Short recordings also skip all subsampled preview frames.
- **Impact:** Linux SDL analysis finishes without entering interactive review;
  configured font failures can remain unobserved on short recordings.
- **Status:** Application loop uses the actual GUI build flag; the first frame
  opens the requested review window independent of later preview subsampling.
  Actual Linux verification pending.
- **Verification needed:** Unmodified corrected Linux SDL analysis must fail
  on missing configured font with the selected locale and retain working review
  rendering, event handling and headless behavior.

### B068: Detector initialization masks negative write indices during growth

- **Evidence:** `InitializeFrameArray`, `InitializeBlackArray`, and
  `InitializeSchangeArray` passed `max(index, count)` to checked growth,
  hiding a negative index, then wrote directly to `vector[index]`.
- **Impact:** A negative producer index can write outside owned storage even
  though the buffer growth helper validates its input.
- **Status:** Fixed: all nine producers reject negative indices before
  mutation. Actual preservation/category tests pass within all 398 Windows
  headless and 402 SDL tests.
- **Verification needed:** Reject negative indices before buffer/counter
  mutation in all initialization producers, including caption text storage.

### B069: Reduced-resolution decode option is configured after codec opening

- **Evidence:** Recording decoder setup assigned `codecCtx->lowres` only after
  `avcodec_open2`, so codec initialization used the default resolution instead
  of the configured `lowres` setting.
- **Impact:** Reduced-resolution settings do not initialize decoding as intended.
- **Status:** Fixed before codec opening. Actual MPEG2 ordinary, explicit,
  and automatic reduced-resolution decoding passes across worker counts within
  all 398 Windows headless and 402 SDL tests.
  Setup extraction also checks the previously ignored parameter-copy status
  and avoids dereferencing an absent software decoder in hardware logging.
- **Verification needed:** Actual MPEG2 decoding at configured and automatically
  chosen reduced resolution, ordinary resolution, and unchanged frame/timing
  observations across worker counts.

### B070: Closing decoder input leaves borrowed stream pointers dangling

- **Evidence:** `file_close` resets owned codec/input objects and stream indices,
  but retains `video_st`, `audio_st`, and `subtitle_st`, which belong to the
  released input. A subsequent failed audio decoder open can test and access
  the stale `audio_st` pointer in `file_open`.
- **Impact:** Reopen/error paths can retain invalid borrowed stream references.
- **Status:** Fixed by clearing borrowed pointers before input release and
  supporting repeated/null-owner close. Three actual/synthetic lifecycle tests
  pass within all 398 Windows headless and 402 SDL tests.
- **Verification needed:** Clear all borrowed references when input ownership
  ends; actual close/reopen and failed stream selection must preserve safe state.

### B071: Audio diagnostic regression assumes a version-specific failure stage

- **Evidence:** The unmodified `66d45a8` Linux Release tests reject the valid
  65-channel frame at `swr_init`, while Windows FFmpeg 8 rejects it during
  `swr_alloc_set_opts2`. The regression asserted only the configuration code.
- **Impact:** A safe supported-version diagnostic path fails the test even
  though the runtime preserves the correct category and owned FFmpeg detail.
- **Status:** Regression accepts either native stage and verifies corresponding
  localized text. Windows FFmpeg 8 passes within all 398 headless/402 SDL tests;
  corrected Linux FFmpeg 6 snapshot verification remains pending.
- **Verification needed:** Both FFmpeg 6 and 8 must reject the real frame with
  the correct typed stage, owned detail and English/Spanish rendering.

### B072: Self-test error logging writes through unchecked file handles

- **Evidence:** Seven decoder/application error paths opened `seektest.log`
  and immediately passed the returned handle to fprintf without checking open.
- **Impact:** An inaccessible log destination can cause a null FILE write while
  reporting a seek/reopen error. Failed writes and closes were also ignored.
- **Status:** Fixed with a checked owned append writer and configurable committed
  destination. Three focused regressions pass within all 413 Windows headless
  and 421 SDL tests.
- **Verification needed:** Failed creation must produce an owned localized path
  diagnostic, complete append records must survive, and every handle must close.

### B073: MLS review header counts the normal commercial list

- **Evidence:** Legacy MLS review output used `state.commercial_count` for its
  Count header while serializing the selected reference intervals.
- **Impact:** Review exports with different list sizes advertise the wrong
  number of entries.
- **Status:** Fixed in the finalized adapter. Exact normal/review regressions
  pass within all 413 Windows headless and 421 SDL tests.
- **Verification needed:** Different normal/reference sizes must produce a
  correct header, matching entries and unchanged independent output formats.

### B074: Womble final commercial interval is serialized as retained show

- **Evidence:** Legacy Womble handling marked an interval ending at the recording
  tail as `last`, then wrote the span from the previous boundary to its end as
  a show clip. The finalized adapter now distinguishes a detected commercial
  from the synthetic retained tail.
- **Impact:** An all-commercial recording, or a commercial interval reaching
  EOF, can be labeled as retained show instead of commercial content.
- **Status:** Fixed in the finalized adapter with pure serializer and actual
  adapter regressions for all-commercial and trailing-commercial recordings.
- **Verification needed:** All-commercial and trailing-commercial recordings
  must label the interval through EOF as commercial, while any retained prefix
  remains a show with the existing clip numbering and frame boundaries.

## Fixed during modernization

- **Caption packet/XDS bounds:** Advertised packet counts could consume stale
  bytes; the validity-bit expression accepted invalid triplets. XDS comparisons
  could exceed 100-byte rows and append beyond 40 rows, and the first valid
  packet was discarded. Packet preflight, bounded cache/assembly handling, and
  five malformed/checksum/recovery regressions address these defects.
- **Frame-volume terminal index:** `set_frame_volume` accepted an index outside
  the stored frame span. Storage and integer-representation checks now ignore
  invalid indices; two tests verify both rejection and valid observation updates.
- **A53 caption overflow and skipped side data:** `f032ea3` bounds/chunks intact
  triplets, fixes the reused loop index, and adds four framing/preservation tests.
- **Terminal XML endpoint rejection:** `273490f` accepts the detector's exact
  terminal boundary and reads valid GOP metadata; adapter and actual media tests
  cover the case.
- **CSV/live histogram indexing:** `2131eee` bounds histogram buckets while
  retaining measured brightness; CSV replay and Linux sanitizer tests cover it.
### B075: Byte seeking converted I/O errors into enormous unsigned offsets

`avio_size` returns negative errors, but the seek path stored that result in an
unsigned integer before calculating an offset. Checked byte-position arithmetic
now rejects negative sizes and clamps valid targets to the owned input extent.

### B076: Unknown-duration byte seeking used frames-times-rate as duration

The fallback divided by `frame_count * fps`; recorded duration is
`frame_count / fps`. Zero and nonfinite durations were also allowed into the
division. The fallback now validates positive finite inputs and uses the correct
units before calculating the byte position.

### B077: Timestamp seeking used unchecked floating-point to integer arithmetic

Nonfinite/extreme targets, invalid stream time bases, tick conversion, and
`start_time` addition could produce undefined or overflowing seek positions.
Checked standard-library helpers now reject invalid or unrepresentable targets
before calling FFmpeg seek APIs.

### B078: Finalized output adapters terminate analysis from the lower layer

- **Evidence:** FFmpeg sidecar, frame-script, player, legacy-editor and legacy
  cut-list adapters called `request_exit` directly after file open or write
  failure, and several printed to process `stderr` themselves.
- **Impact:** Embedded and repeated analyses cannot recover at their application
  boundary or inspect an owned failure path; output code also duplicates file
  lifecycle and retry handling.
- **Status:** Fixed. The adapters share a standard-library exact-byte writer
  which closes deterministically and throws owned `output_open`/`output_write`
  diagnostics. Chapter creation retains its one retry through `std::chrono`.
- **Verification:** Exact UTF-8 replacement, retry failure ownership and actual
  Spanish adapter failures pass within all 423 Windows headless and 431 SDL
  tests; the public non-donator build succeeds.

### B079: Saved-logo output could silently truncate or corrupt its mask

- **Evidence:** `SaveLogoMaskData` ignored every `fprintf` result and the final
  `fclose` result. A failed destination could leave a partial metadata or mask
  file while analysis continued as though it had succeeded.
- **Impact:** A later run can consume an incomplete saved-logo cache and either
  lose logo detection or fail far from the original write error.
- **Status:** Fixed. The writer validates its source buffers, checks the exact
  byte count, checks close, and reports owned output diagnostics.
- **Verification:** Round-trip, read-only destination, invalid-buffer,
  open-failure and removable-file regressions pass in all 427 Windows headless
  and 435 SDL tests. Linux verification remains part of the next snapshot run.

### B080: Saved-logo loading used manually owned streams

- **Evidence:** `LoadLogoMaskData` kept the saved-logo input and optional
  detector-output input in raw `FILE*` variables. Exceptional parsing paths
  depended on individual manual close calls, and detector-output read and close
  failures had no structured error.
- **Impact:** Repeated or embedded analysis could retain a file handle after a
  malformed file, and callers could not distinguish detector-output read
  failures from unrelated output errors.
- **Status:** Fixed. Both inputs now have scoped ownership; every explicit close
  is checked and detector-output failures carry an owned path diagnostic.
- **Verification:** Malformed and successful load tests release the source path
  immediately and pass in both complete Windows configurations. Linux
  verification remains part of the next snapshot run.

### B081: XDS program-length logging reads a missing variadic argument

- **Evidence:** The program-length diagnostic contained six integer conversions
  after its frame prefix but supplied only five corresponding values.
- **Impact:** Enabling this diagnostic invokes undefined variadic behavior and
  can print arbitrary data or fail while processing otherwise valid XDS input.
- **Status:** Fixed by the catalog migration. The deterministic message accepts
  the three elapsed fields actually decoded by the parser and formats all values
  before insertion into a validated plain-field catalog entry.
- **Verification:** English and Spanish catalog tests preserve the decoded
  program length and elapsed hour/minute/second values. Both complete Windows
  configurations pass without the former missing variadic argument.

### B082: The first caption-block diagnostic read before its owned array

- **Evidence:** `OutputCCBlock(context, 0)` displayed block zero's geometry but
  fetched its type from `cc_block[-1]`.
- **Impact:** Starting the first caption block could read unrelated memory when
  verbose caption diagnostics were enabled.
- **Status:** Fixed. The diagnostic uses the same validated block index for all
  fields and ignores an invalid negative index.
- **Verification:** The focused first-block regression passes within all 429
  Windows headless and 437 SDL tests. The next Linux sanitizer snapshot will
  cover the former out-of-bounds read.

### B083: Recording-open failures discard their FFmpeg cause

- **Evidence:** `file_open` prints an FFmpeg failure and throws only
  `ExitRequested(-1)` for open, probing and missing-video failures.
- **Impact:** Embedded callers lose the filename and cause; command-line status
  conversion can also expose `-1` as 255.
- **Status:** Fixed. Recording open and probing failures now retain the UTF-8
  path and copied FFmpeg detail, while a missing video stream has its own owned
  diagnostic. The application boundary alone chooses the process status.

### B084: Failed recording opens retain a partially initialized decoder

- **Evidence:** `file_open` publishes `VideoState` and its format context before
  probing succeeds. A caught failure leaves them in the recording context, so a
  retry can skip input opening and stream discovery.
- **Impact:** One bad input can poison a reusable in-process recording context.
- **Status:** Fixed. Every exception from recording startup closes codecs,
  frames, the demuxer and borrowed stream references before it escapes. An
  actual missing-Unicode-path then valid-media retry uses the same context.

### B085: Video packet decoding silently discards FFmpeg errors

- **Evidence:** `avcodec_send_packet` failures are ignored, and receive errors
  other than `EAGAIN`/EOF end the loop without a diagnostic.
- **Impact:** Corrupt or unsupported packets can look like ordinary no-frame
  results, hiding data loss and preventing callers from choosing recovery.
- **Status:** Fixed. Send failures and receive failures other than `EAGAIN` and
  EOF now throw typed diagnostics containing a copied FFmpeg error string.
- **Verification:** Deterministic status tests distinguish frame, retry, EOF,
  send failure and receive failure without relying on codec-specific corruption.
  All 436 Windows headless and 444 SDL tests pass; the public build succeeds.

### B086: Logo histogram indexing trusts unbounded persisted values

- **Evidence:** CSV accepts any finite `good_edge`; `FindLogoThreshold` converts
  it directly to an index in a fixed 256-entry histogram. Negative and values
  above one index outside the array, after earlier entries may already mutate it.
- **Impact:** A crafted or damaged CSV can cause out-of-bounds memory access and
  leave partial histogram state.
- **Status:** Fixed. A focused C++23 module validates the frame span, bucket
  count and every finite `[0,1]` observation before allocating a result. It uses
  64-bit counts and overflow-safe percentile math, then publishes the legacy
  histogram only after the complete calculation succeeds.
- **Verification:** Boundary buckets, invalid finite/nonfinite values, invalid
  counts and near-`uint64_t` arithmetic pass in all 434 Windows headless and 442
  SDL tests. Linux sanitizer verification remains pending for this snapshot.

### B087: Positioning-error recovery contains unreachable failure handling

- **Evidence:** `video_packet_process` jumps to `quit` before resetting retries
  and requesting failure, and its ordinary return is indistinguishable from a
  packet that produced no frame.
- **Impact:** A serious seek-position mismatch may be silently treated as normal
  decode progress.
- **Status:** Fixed. Packet decoding returns distinct frame, analysis-complete,
  self-test-complete and positioning-failure outcomes. Callers convert terminal
  outcomes at their control boundary, and the unreachable branch is removed.
- **Verification:** Deterministic outcome tests cover every state and terminal
  priority; existing seek/reopen self-tests exercise the application handling.
  All 445 Windows headless and 453 SDL tests pass. Linux verification remains
  pending.

### B088: Core cut-list and live output ignored write and close failures

- **Evidence:** Default cut lists, EDL/live files and live commercial-state
  files used unchecked `fprintf`, `fflush` and deleter-driven close calls. Their
  repeated open failures also terminated analysis inside the output layer.
- **Impact:** A full or disconnected destination could silently receive a
  truncated file, while embedded callers could not recover from open failure.
- **Status:** Fixed for the core cut-list and live-output paths. They now use
  owned `output_open`/`output_write` diagnostics, checked writes and explicit
  checked flush/close while retaining the existing bounded open retry and the
  optional live `.incommercial` open policy.
- **Verification:** Focused read-only, successful-close and actual
  missing-destination regressions pass in all 441 Windows headless and 449 SDL
  tests. Linux verification remains pending.

### B089: Logo report checked lookup failures after discarding them

- **Evidence:** `PrintLogoFrameGroups` changed negative `FindBlock` results to
  zero before testing whether either lookup failed, making both failure branches
  unreachable and allowing an unrelated first block to supply timing data.
- **Status:** Fixed. Lookup failure is now handled before any index is used.
- **Verification:** The empty-block report path returns without indexing block
  storage and passes in both complete Windows suites. Linux sanitizer
  verification remains pending.

### B090: Empty caption summaries indexed missing storage and divided by zero

- **Evidence:** `PrintCCBlocks` always read `cc_block[0]` and divided caption
  totals by `framesprocessed` and `fps`, including when those values were zero.
- **Status:** Fixed. Empty storage produces a complete zero-block heading and
  returns safely; percentage and duration calculations use zero when their
  denominator is not positive.
- **Verification:** The empty/zero-denominator regression passes in all 441
  Windows headless and 449 SDL tests. Linux sanitizer verification remains
  pending.

### B091: Logo transition bounds accepted the one-past-end frame index

- **Evidence:** Both logo disappearance and appearance checks rejected frame
  indices greater than owned storage but accepted `frame.size()`, immediately
  before loops that index that frame range.
- **Status:** Fixed. Both transitions now reject indices greater than or equal
  to the owned frame count.
- **Verification:** A focused regression exercises both one-past-end transition
  directions and passes in both complete Windows suites. Linux sanitizer
  verification remains pending.

### B092: Command-line PID parsing accepted malformed and oversized values

- **Evidence:** `--pid` used unchecked `sscanf("%x")`; it accepted a valid
  prefix followed by arbitrary text, silently left the previous value when no
  conversion occurred, and did not enforce the 13-bit transport-stream range.
- **Status:** Fixed. A C++23 `std::expected`/`std::from_chars` parser requires
  the complete hexadecimal value, accepts an optional `0x` prefix and limits
  values to `0x0000`–`0x1fff`. Invalid input follows the localized command-line
  error path.
- **Verification:** Both focused parser tests pass in all 443 Windows headless
  and 451 SDL tests. The real executable rejects `--pid=12junk` with the
  localized option/value diagnostic and status 1. Linux verification remains
  pending.

### B093: Tuning and training output dereference failed opens

- **Evidence:** The `.tun`, `strict.csv` and `comskip.csv` paths call `myfopen`
  and then write without consistently checking the returned owner. Their writes
  and eventual deleter-driven closes are also unchecked.
- **Impact:** An unwritable or disconnected working destination can cause a
  null-stream crash or silently truncate diagnostic training output.
- **Status:** Fixed. These secondary outputs now use owned open/write/close
  diagnostics and explicitly close completed tuning/training files.
- **Verification:** A blocked `strict.csv` destination produces `output_open`
  with the owned path instead of dereferencing null. Existing exact CSV content
  and checked write/close regressions pass in all 445 Windows headless and 453
  SDL tests. Linux verification remains pending.

### B094: Optional media dumps silently ignored storage failures

- **Evidence:** Raw audio/video dump creation did not check `myfopen`; all raw
  writes ignored `fwrite`, and close relied on a non-reporting deleter. Data
  dumps logged and returned on open/write failure, preventing callers from
  handling incomplete output.
- **Status:** Fixed. Dump payloads use `std::span<const std::uint8_t>`, all three
  destinations propagate owned `output_open`/`output_write` diagnostics, and
  explicit close reports flush failures.
- **Verification:** Exact binary payload, failed destination, disabled output,
  size/frame bounds and explicit close regressions pass in all 445 Windows
  headless and 453 SDL tests. Linux verification remains deferred to the final
  implementation stage.

### B095: Immediate logo-disappearance logging mismatched variadic arguments

- **Evidence:** The cutpoint diagnostic expected an integer frame followed by a
  floating-point timestamp, but passed the timestamp first and the integer
  second. Reading both through the incompatible printf conversions was undefined.
- **Status:** Fixed. The frame and timestamp are now formatted with C++23
  `std::format` before insertion into the localized message.
- **Verification:** Exact English padding and three-decimal timestamp coverage
  passes within all 445 Windows headless and 453 SDL tests. Linux verification
  remains deferred to the final implementation stage.

### B096: Volume-plateau histogram trusts the observed volume as an index

- **Evidence:** Detection increments `platauHistogram[frame.volume / 10]`
  without validating that the recorded volume maps into the 255-element array.
- **Impact:** A negative, corrupt or unusually large observation can index
  outside owned histogram storage during silence calibration.
- **Status:** Fixed. A C++23 `std::expected` bucket conversion validates width,
  negative observations and owned histogram capacity before mutation.
- **Verification:** Boundary/error tests pass in all 452 Windows headless and
  460 SDL tests. Linux verification remains deferred to the final stage.

### B097: Frame CSV output mutated destinations before validation

- **Evidence:** `OutputFrameArray` opened and truncated its CSV before checking
  observation bounds, opened the file even for screen-only output, used
  locale-sensitive `fprintf`, and ignored every write/close result.
- **Impact:** Invalid state or a screen preview could destroy an existing replay
  file; storage failures could leave a plausible partial file, and comma-decimal
  locales could produce output the parser cannot replay.
- **Status:** Fixed. A focused span/ostream serializer validates all rows before
  writing, uses the classic locale, reports stream failure and preserves the
  final observation. The application adapter validates before opening, avoids
  all file access in screen mode, and maps open/write/close failures to owned
  diagnostics.
- **Verification:** Golden roundtrip, empty/final rows, invalid numeric state,
  failing stream, no-artifact bounds and screen-only regressions pass in all
  452 Windows headless and 460 SDL tests. Linux verification remains deferred.

### B098: Diagnostic histograms divided by zero and used manual star buffers

- **Evidence:** Empty brightness/uniformity/general histograms divided by a zero
  maximum and zero `framesprocessed`; manual index loops populated fixed star
  arrays from those nonfinite divisors without validating negative counts.
- **Status:** Fixed. A focused span-based report builder uses owned strings,
  64-bit counters, bounded star counts, explicit zero-denominator results and
  `std::expected` validation before output.
- **Verification:** Empty, ordinary, maximum-star, negative-count and
  invalid-geometry regressions pass in all 459 Windows headless and 467 SDL
  tests. Linux verification remains deferred.

### B099: Verbose final-run logging writes through an unchecked file handle

- **Evidence:** After the final frame-count diagnostic, detection opens the log
  in append mode and immediately calls `fprintf` without checking `myfopen`.
- **Impact:** A missing or unwritable log destination can cause a null-stream
  crash after analysis has otherwise completed.
- **Status:** Fixed. A focused standard-stream writer appends the footer, checks
  open/write/close state and propagates owned destination diagnostics.
- **Verification:** Exact append, Unicode path and blocked-destination tests pass
  in all 459 Windows headless and 467 SDL tests. Linux verification remains
  deferred.

### B100: Vector allocation diagnostics are unreachable after allocation failure

- **Evidence:** Runtime initialization calls `std::vector::resize` and only then
  checks `empty()`. Allocation failure throws `std::bad_alloc`, so the localized
  message and legacy exit status after each resize are never reached.
- **Impact:** Low-memory failures bypass the application's diagnostic and exit
  policy, and the eight resource-specific messages cannot describe the failure.
- **Status:** Fixed. Allocation operations now cross a focused C++23
  `std::expected` boundary that catches `std::bad_alloc`; runtime initialization
  reports the localized resource identity and preserves each intended status.
- **Verification:** Success, allocation-failure and unrelated-exception tests
  cover the boundary. All **468/468** Windows headless and **476/476** SDL tests
  pass, and the public non-donator application builds.

### B101: Black-frame validation can inspect one element past the active range

- **Evidence:** The contiguous-run loop in `ValidateBlackFrames` allows
  `k == black_count - 1` and then evaluates `black[k + 1]`. The allocation may
  currently contain spare capacity, but that slot is outside the active
  black-frame range and its contents do not describe a valid observation.
- **Impact:** Validation can consume stale/default state when the last active
  black frame starts or extends a run, producing an incorrect run boundary and
  potentially reading outside allocated storage when capacity is exact.
- **Status:** Fixed. Contiguous-run discovery now receives a span containing
  exactly the active black-frame observations and checks the successor index
  before reading it.
- **Verification:** Focused tests cover a contiguous poison record immediately
  beyond the active span, ordinary run extension and an invalid starting index.
  All **477/477** Windows headless and **485/485** SDL tests pass, and the
  public non-donator application builds.

### B102: C stream read failures can appear as ordinary text EOF

- **Evidence:** `FileStreamBuffer::underflow` throws when `fread` reports an
  error, but its two `std::istream` callers left the default exception mask in
  place. The iostream layer catches a stream-buffer exception, sets `badbit`,
  and otherwise lets line parsing observe EOF.
- **Impact:** A failed CSV or reference-file read can be accepted as a cleanly
  terminated document, leaving analysis based on a prefix of the input.
- **Status:** Fixed. Both adapters enable `badbit` exceptions, preserving the
  owned read diagnostic. Persisted caption records now cross a separate typed
  `std::expected` boundary with exact field reads and a caller-supplied payload
  limit.
- **Verification:** Focused packet tests cover valid consecutive records, clean
  EOF, malformed and negative fields, each truncated field and rejection before
  allocation when the declared payload exceeds the destination capacity. All
  **477/477** Windows headless and **485/485** SDL tests pass, and the public
  non-donator application builds.
