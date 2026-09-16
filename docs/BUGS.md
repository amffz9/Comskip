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
| B046–B052 | Open; evidence and verification requirements below. |

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
- **Fix:** Validate framing and bounded payload length before reading or changing
  observations; retain compatible valid dump replay through the owned session.
- **Verification needed:** Actual CSV replay with oversized, negative, malformed,
  and truncated companion records, plus valid captions and failure cleanup.

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
  285 Windows tests pass. Later Linux/SDL verification remains separate.

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
- **Status:** Suspected deployment gap. A relocated installation without the
  original checkout has not been tested.
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
  caption/subtitle reasons remain open.
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
- **Status:** Source-confirmed portability restriction; consumers must be audited
  before removing the obsolete boundary. This is not a buffer overflow.
- **Verification needed:** Long Unicode configured file paths through actual
  loading/consumers, preserved embedded-null rejection, and removal of the
  arbitrary cap where no fixed-capacity consumer remains.

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
