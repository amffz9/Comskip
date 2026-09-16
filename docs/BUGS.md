# Bug log

Record newly discovered issues here even when fixing them is deferred. Keep
confirmed defects separate from suspected gaps. Close entries only with a fix
and relevant verification; retain the evidence for future regressions.

## Open

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
