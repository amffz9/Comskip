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
| B002 | Global writer removed at `3cc57ed`; complete replacement lifecycle leak verification remains pending. |
| B003, B013, B014 | Fixed at `12be69c`; all 174 Windows tests pass. Linux bridge dependency was also verified on the isolated patched snapshot. |
| B006 | Fixed at `3cc57ed`; both failure branches verified by four warning tests at `7e011c3`. |
| B012, B015, B016, B017, B019 | Fixed at `c4ab1e0`; all 181 Windows tests pass. Full Linux/sanitizer verification is pending. |
| B020 | Fixed at `589fc7b`; all six settings-value tests pass on Windows. |
| B021 | Overflow fixed at `8a4bda6`; three focused Windows tests pass. Full path support is tracked separately as B023. |
| B022 | Fixed at `54470db`; all six diagnostic-output tests pass, including flush and file removal after disabling demux. |
| B018 | Fixed at `bbbf019`; all 206 Windows tests pass, including six actual malformed/valid input regressions and eight pure parser tests. Linux verification is pending. |
| B023 | Open; the connected filename graph is being migrated for full CLI long-path support. |
| B024 | Fixed at `4839fee`; unsafe conversions and argument counts are rejected, with actual escaped-template output compatibility. All 199 Windows tests pass at that stage. |
| B025 | Fixed at `ed8649b`; five actual lifecycle tests pass on Windows. The isolated Linux `c4ab1e0` snapshot plus only that packet patch passes all 177 address/undefined/leak sanitizer tests without findings or suppressions. |
| B026 | Fixed at `0f98693`; all 200 Windows tests pass, including zero, negative, and excessive observation counts. |

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
- **Status:** Deferred for a compatible CSV format/timeline fix. The replay
  regression explicitly checks the current difference instead of treating it
  as equivalent output.
- **Verification needed:** Final observation roundtrip and matching EOF cue
  timing, including compatibility with existing CSV files.

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
- **Status:** Confirmed portability gap. Input media filename ownership is
  being migrated first; basename and configuration-path migration must follow.
- **Verification needed:** Actual long nested Unicode media opened through both
  decoder and CLI, with normal output/settings lookup and failure cleanup on
  Windows and Linux.

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
- **Fix/verification needed:** Owned, checked marker creation with localized
  failure reporting and an actual unavailable-destination regression.

### B028: Empty caption text is indexed before its length guard

- **Evidence:** Caption processing evaluates `text[text_len - 1]` before
  checking text length. A fresh control-only pair can reach this with length
  zero. The stored bytes are already unsigned, so character classification is
  not a separate signed-character defect.
- **Fix/verification needed:** Guard indices first, with empty/control-only and
  extended-byte regressions.

### B029: Reference comparison trusts commercial sentinel capacity

- **Evidence:** After valid reference input, comparison indexes
  `commercial[commercial_count]` even when count is `-1`, and writes the next
  sentinel beyond the 100,000-entry array when count is 99,999. Reserving a
  reference slot does not reserve a commercial slot.
- **Fix/verification needed:** Explicit empty-list and sentinel capacity handling,
  with real reference comparison for empty and capacity-sized commercial lists.

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
