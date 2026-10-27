# Plan: interchangeable frame sources behind a detection session

Status: Stage A outline (terms, parts, flow), revised October 27, 2026 to fit
`docs/detection-session-roadmap.md`. This plan covers roadmap stage 1 (make the
pipe give the same result as a file) and stage 2 (Session, Driver, StreamReader,
whole outputs). HardwareDecoder is roadmap stage 8. It stays here in outline
only and is parked. Nothing is implemented.

Not yet ready for per-part contracts. The user accepted every recommended
answer to R1-R5 (October 27), but stage 1 is still an investigation whose cause
is unknown. Its exit check also needs the new labels (see "Labelled set").
Part contracts are written for stage 2 once stage 1 is done and the revised
outline is approved.

Goal (user's words, condensed): stop tying detection to Comskip's own decoding
loop, so the same detector can be fed by three input paths: decoding a file in
software, decoding a file with GPU hardware (VAAPI first), and accepting frames
another process has already decoded so the recording is not decoded twice.
Comskip stays an executable that the TV application runs.

## Glossary

- **FFmpeg**: fixed by the user. The media toolkit Comskip already uses for
  demuxing and decoding. Named only in boundary parts.
- **VAAPI**: fixed by the user. The Linux hardware video interface used on the
  N100 NAS. Other device kinds (Windows D3D11, NVIDIA CUDA) come through the same
  boundary at no extra contract cost.
- **Recording**: one broadcast being analysed, finished or still growing.
- **Session**: one analysis of one recording, from first frame to final result.
  Independent sessions never share mutable state.
- **Picture**: one video frame's luma (brightness) plane: width, height, row
  stride in bytes, 8-bit samples, and its presentation time.
- **Presentation time**: seconds on the recording's timeline, normalised so the
  first picture is near zero. Strictly increasing per source except at a
  declared discontinuity.
- **Sound**: a run of audio samples: channel count, sample rate, sample values,
  and the presentation time of the first sample.
- **Caption data**: closed-caption bytes with a presentation time. Optional.
- **Stream info**: what a source knows before the first picture: nominal frame
  rate, picture size, whether audio and captions exist, total duration if known.
- **Seekable**: a source that can deliver the recording again from its start.
  Files are seekable; a pipe from another process is not.
- **Opening picture copy** (conditional, not built now): one picture per
  second from the opening section, kept by Session until the logo is found or
  a size cap is reached. About 130 KB per second at 480x270. It exists only if
  a later decision adds opening rescoring. R1 was decided as "skip the rewind,
  no rescoring for now".
- **Placeholder frame**: a frame Session invents to fill a gap in presentation
  time, so frame counts stay tied to time. It is never black, never silent,
  never a cut point, and it keeps the logo state of the frame before it.
- **Labelled set**: the recordings with hand-made labels in
  `bin/perf/real/<stem>.labels.csv`, edges within 5 s: `g5` now, `faaea10a` and
  `77483aca` once relabelled, `g4` after. `score.py` must read these files
  before stage 1's exit check. Today it reads only `*.truth.json`, whose
  edges for faaea10a and 77483aca are only good to about 15 s.
- **Read buffer**: StreamReader's own store of pictures and sound it has read
  from the pipe but Driver has not taken yet. It lets the pipe keep draining
  while analysis is briefly busy.
- **End-of-recording signal**: an optional, explicit notice that a growing file
  will not grow any more: a marker file the TV app creates next to the
  recording (R4). Driver then stops waiting within about a second instead of
  after `live_tv_retries` x 4 s. For a pipe, the end of the stream is the
  signal.
- **Rewind request**: the detector's existing request to restart analysis from
  the first picture after it finds the station logo (`two_pass_logo`,
  `startOverAfterLogoInfoAvail`).
- **Commercial list**: the ordered, non-overlapping commercial intervals in
  presentation time, plus the per-block evidence the outputs already report.
- **Snapshot**: a complete commercial list as of some presentation time, for
  live mode. Each snapshot fully replaces the previous one.
- **Snapshot stream**: an optional machine-readable sequence of snapshots on
  Comskip's standard output, one complete snapshot per record, so a supervising
  process can read results without polling output files.
- **Settings**: the validated configuration value Comskip already builds from
  INI and command-line arguments.

## Parts

- **Session**: owns detection for one recording; accepts pictures, sound and
  caption data in time order and produces snapshots and the final commercial
  list.
- **SoftwareDecoder** (boundary): turns a recording file into pictures, sound
  and caption data by decoding on the CPU.
- **HardwareDecoder** (boundary, parked: roadmap stage 8): same promise as
  SoftwareDecoder, but uses a GPU device and delivers only the luma plane.
- **StreamReader** (boundary): reads pictures and sound that another process has
  already decoded, from a non-seekable byte stream, into its read buffer so
  reading never waits on analysis.
- **Driver**: pulls from one source and feeds the Session; owns follow/reopen of
  growing files and decides how a rewind request is honoured.
- **OutputPublisher** (boundary): writes snapshots and the final list to the
  configured output files, replacing each file whole, and to the snapshot stream
  when enabled.
- **CommandLine**: the existing executable; parses arguments, picks the source,
  runs the Driver, maps the outcome to the existing exit codes.

## Tree

- CommandLine: calls Settings loading (existing), Driver, OutputPublisher
- Driver: calls one of SoftwareDecoder | HardwareDecoder | StreamReader, and Session
- Session: calls the existing detection modules; calls nothing external
- Tests may drive Session directly with generated pictures and sound, skipping
  Driver and every source.

## Existing parts

- Detection modules (scene, logo, audio volume, blocks, scoring, captions):
  unchanged algorithms. Changed: they receive pictures and sound from Session
  operations instead of reading decoder state, and request a rewind by returning
  it instead of setting a decoder seek.
- Decoder setup (`src/media/decoder_setup.cpp`): changed. The per-codec table of
  hardware decoder names (`*_qsv`, `*_dxva2`, `*_vdpau`, `*_cuvid`, `*_mmal`) is
  replaced by HardwareDecoder's single device-kind mechanism. Existing flags
  `--qsv`, `--dxva2`, `--vdpau`, `--cuvid`, `--hwassist` map onto it.
- Main decode loop (`comskip_main`, `src/app/analysis.cpp`): split into Driver
  (loop, retries, follow, rewind) and CommandLine (arguments, exit status).
- Output writers (`src/output`): unchanged formats. Changed: live and final
  outputs are published whole, so a reader never sees a half-written file
  (already a goal in the adaptive-detection plan).
- Input timing (the start times of the audio and video streams): the suspected
  cause of the pipe-vs-file differences. Changed only where stage 1 finds it
  necessary.
- Settings loading, localisation, review UI, CSV/TXT replay input: unchanged.

## Out of scope

- A published library interface or any language binding (decision Q1). The
  Session split keeps that possible later.
- Any change to detection algorithms, thresholds or scoring.
- Changes to the TV application. It keeps calling the executable and polling
  output files. Adopting StreamReader or the snapshot stream needs a TV-side
  change, planned separately. That change must make sure the recorder never
  waits on Comskip: a generous queue in front of the pipe that drops pictures
  (and logs it) when full, and a larger pipe buffer. A proxy-file fallback is
  possible only if the TV app keeps encoding the proxy alongside the pipe
  (bench mode `tee`), which gives up the encode saving. That is a TV-app
  decision. Only the Comskip-side read buffer is in this plan.
- Windowed live mode (roadmap stage 5, its own plan). This plan must not block
  it: Session keeps its per-frame measurements, and producing a snapshot must
  not change later snapshots or the final list.
- Measurement cleanups (roadmap stage 3): dim/bright shares, unknown volume,
  logo give-up. They change output on purpose, so they come after the
  byte-identical check here.
- Faster pipe transfer (a bigger pipe buffer, shared memory): a later, measured
  option. Today pipe copying costs about 37 s of system time per 93 minutes.
- GPU computation of detection features (no meaningful gain).
- Reducing picture size on the GPU before transfer: a later, measured option,
  because it changes the detector's sampling.
- macOS verification.

## Must not get worse

1. Software file decoding produces byte-identical outputs to master after
   stage 1 on every fixture in `bin/perf`, and passes every existing test.
   Master is `d9f7ad5` today. [ASSUMED: its default output equals `66d5717`'s,
   since `scan_step` defaults to off.] If stage 1 changes file-mode output, the
   baseline moves to the post-stage-1 result, which must score no worse.
2. Software file decoding uses no more CPU time than master: within run-to-run
   noise, [ASSUMED: at most 2%] on the NAS for the 10-minute SD and 5-minute HD
   fixtures.
3. The software path gains no extra full-size picture copy per frame.
4. Existing command lines, INI files, output files and exit codes keep working
   unchanged. The TV application needs no change to keep its current behaviour.
   The snapshot stream is off unless requested, so standard output is unchanged
   by default.
5. (Stage 8 only.) HardwareDecoder results: `bin/perf/score.py` scores no worse than software on
   the truth fixtures; commercial boundaries differ from software by at most
   [ASSUMED: one frame].
6. Two sessions in one process give the same results as two separate runs.
7. Same frames, same result: with the rewind off (`two_pass_logo=0`), a pipe
   and a file carrying the same decoded pictures and sound give identical
   commercial lists on the labelled set and every `bin/perf` fixture. This is
   stage 1's exit check.
8. With the rewind declined on a pipe, `score.py` scores the pipe no worse than
   file mode on the labelled set: no more programme seconds cut, no
   fewer breaks found, no more false breaks. Any differences are confined to
   the opening section.
9. StreamReader keeps reading while Session is busy. With analysis stalled on
   purpose for [ASSUMED: 5 s], the pipe never stays full for longer than
   [ASSUMED: 1 s], and the cut list afterwards is identical to a run without
   the stall. The read buffer holds at least 10 s of pictures, about 40 MB at
   480x270 (R3).
10. (Conditional: only if opening rescoring is added later.) Memory stays
    bounded. The opening picture copy has its own cap [ASSUMED: 5 minutes,
    about 40 MB at 480x270], independent of `give_up_logo_search`. That limit
    never fires when `added_recording` > 0, which includes the default of 14.
12. Gaps never shift later results. A recording piped with a run of pictures
    removed gives the same cut points after the gap, in presentation time, as
    the full recording, apart from edges inside or touching the gap.
11. Comskip never exits with an error because of the rewind on a pipe. Today
    it exits 2 ("Cannot open recording /dev/stdin").

## Flow

1. CommandLine loads Settings from arguments and INI, as now.
2. CommandLine picks a source: an input of `-` (or an explicit stream option)
   selects StreamReader; a file with a hardware device requested by flag selects
   HardwareDecoder; otherwise SoftwareDecoder.
3. CommandLine asks the source to open. On `device unavailable` from
   HardwareDecoder, it stops with the existing media-open exit code and a
   message naming the device (hardware is opt-in, decision Q2).
4. CommandLine creates a Session with Settings and the source's stream info.
5. Driver repeatedly asks the source for the next item and forwards it. For
   StreamReader the item comes from the read buffer, which a separate reader
   keeps filling from the pipe:
   picture → Session.add_picture, sound → Session.add_sound, caption data →
   Session.add_captions.
6. The source answers `waiting` for a growing file with no new data yet. Driver
   applies the existing live retry/follow rules and asks again. Past the
   timeout, or as soon as an end-of-recording signal arrives, it treats the
   input as ended. StreamReader never answers `waiting`; it blocks until data
   arrives or the stream ends.
   - While waiting on a growing file, an end-of-recording signal ends the wait
     within about a second (see "End-of-recording mechanism").
   - If the read buffer fills, StreamReader stops reading and logs
     `read buffer full` once each time it happens. It resumes when space frees
     up, and it never drops data itself.
   - If presentation time jumps forward by more than 1.5 frame intervals
     between two pictures (the producer dropped some), Session adds placeholder
     frames to fill the gap and logs how many. Detection counts frames, not
     seconds: frame numbers, `added_recording` x fps x 60 and live-mode gap
     lengths are all frame counts. So without placeholders, every result after
     the gap would shift. A backward jump is `stream malformed`.
7. Session.add_picture may answer `rewind requested`.
   - Seekable source: Driver asks the source to restart from the first picture
     and tells Session to restart, preserving the logo it found (today's
     behaviour).
   - Non-seekable source: Driver declines the rewind and never reopens the
     input. Session continues as `two_pass_logo=0` does today (R1, decided).
     Opening rescoring from an opening picture copy is not built. It is added
     only if a later decision asks for it, after labels show errors in the
     opening.
8. In live mode, Session produces a snapshot whenever its list changes. Driver
   passes it to OutputPublisher.publish_snapshot, which replaces each live output
   file whole and, when enabled, writes one record to the snapshot stream.
9. On `end of input`, Driver calls Session.finish, which drains pending audio and
   captions and returns the commercial list. OutputPublisher.publish_final
   writes every configured output and, when enabled, a final record marked as
   final on the snapshot stream.
10. On a source error mid-stream: `damaged data` is skipped as today.
    `hardware decode failed` is [ASSUMED: a reopen with SoftwareDecoder at the
    same presentation time, logged]. `stream malformed` from StreamReader ends
    the run with the media exit code after publishing the last snapshot.
11. CommandLine maps the outcome to the existing exit codes.

## End-to-end check

Run this in the test container (`docker/test`), never the production
container, on the labelled set and the `bin/perf` fixtures:

1. software: `comskip rec.ts` (bench mode `file`/`proxy`)
2. pre-decoded: FFmpeg decodes to grey pictures and PCM in NUT and pipes them
   into `comskip -`, with the TV app's ini (`two_pass_logo` left at its
   default) and the snapshot stream enabled (bench mode `hitch`). This run
   uses stage 2's path, where the rewind is declined on a pipe.
3. the same NUT bytes, saved to a file and read as a file with
   `two_pass_logo=0`. That matches what (2) does when it declines the rewind.
   Must-not-get-worse 7 is the stage 1 form of this comparison, with
   `two_pass_logo=0` on both sides.
4. (stage 8 only) hardware: `comskip --hwaccel=vaapi rec.ts`

It passes if:

- (1) is byte-identical to the baseline (Must-not-get-worse 1);
- (3) equals (2) exactly;
- (2) exits normally and scores no worse than (1) with `score.py`
  (Must-not-get-worse 8);
- (2)'s final snapshot stream record matches its final output files.

Record CPU and wall time for each run. Separate tests check Must-not-get-worse
9 (stall) and 12 (pictures removed from the pipe).

## End-of-recording mechanism (boundary fact, checked October 27)

Growing-file mode keeps one input handle open with FFmpeg's `follow` option and
`rw_timeout` = `live_tv_retries` x 4 s (`src/media/recording_input.cpp:94-97`,
`growing_file_timeout_us` in `src/media/live_input.h`). So the whole wait is
one blocking read inside FFmpeg, and nothing in Comskip runs during it.
Comskip sets no interrupt callback today.

Chosen: an FFmpeg interrupt callback (`AVIOInterruptCB`) on the input. FFmpeg
calls it repeatedly while a read waits for more data. The callback looks for
the marker file, at most once per second, and when the marker is there it
makes the read end as end of input. Its time limit stays as the fallback.

Rejected: a short `rw_timeout` with Comskip retrying. The timeout surfaces as
a read error in the middle of demuxing, and the demuxer can't be relied on to
resume cleanly after that. Reopening isn't an option either: the code notes
that reopening or seeking a growing transport stream lands far from the resume
point, which is why it keeps one handle.

To check before relying on it (platform-fact gate): in the vcpkg FFmpeg
version, the file protocol's `follow` wait does call the interrupt callback.
I believe it does, through its read-retry loop, but that comes from memory of
FFmpeg's source, not from this repo.

## Decisions (user, October 24, 2026)

- Q1 No published library. The value is the internal Session split; Comskip
  stays an executable that the TV application runs. A C++ library interface or
  a binding can be added later on top of Session.
- Q2 Hardware decoding: opt-in by flag only. Default output is unchanged.
- Q3 Logo rewind on a non-seekable source: Session keeps compact logo evidence
  and rescores the opening section itself, with no second decode. (Reopened
  October 27 as R1: declining the rewind alone may be enough.) File mode
  keeps today's rewind; switching file mode over is a later, measured change.
- Snapshot stream: included as an opt-in output, so a supervising process can
  stop polling output files.

## Stage 1: pipe-vs-file differences (October 27 findings)

- The same decoded frames give different results through a pipe than from a
  file, even with the rewind off. On 77483aca, edges mostly move by 1 frame.
  On faaea10a, some edges move by up to 40 s and one break is lost (5 of 12
  found instead of 6).
- Sending float audio (`pcm_f32le`) did not remove the differences, so audio
  volume scaling is not the main cause.
- Prime suspect: how Comskip handles the audio and video start times. A pipe
  changes the streams' start times, and the NUT grey stream starts at 0.033 s.
  A 1-frame shift fits a frame-numbering offset. The 40 s moves suggest audio
  and video slipping against each other.
- First check: run the same NUT bytes from a file and from the pipe. If those
  two match, the cause is the different kind of stream (NUT instead of the
  H.264 proxy), not the fact that a pipe can't be re-read. If they don't
  match, the cause is in how the pipe is read.
- Stage 1 is done when Must-not-get-worse 7 holds. Stage 2's contracts are
  written after that, because the fix may change what Session must accept
  about start times.

## Finding that limits Q3 (checked in the source)

The rewind does more than recompute logo presence. After the logo is found, the
brightness scans skip logo pixels (`haslogo` in `ScanTop/Bottom/Left/Right`,
`src/detection/scene_analysis.cpp:341`), so brightness, black-frame and
scene-change values for the opening section also change on the second pass.
Logo presence is sampled once per second (`logoFreq = 1.0`) and the search may
run for up to `give_up_logo_search = 2000` seconds.

Keeping enough to replay those masked brightness values exactly means keeping
every picture of the opening section: about 3.9 MB/s at 480x270, far more at HD.
So the rescoring keeps per-second logo evidence only and recomputes logo
presence. Brightness, black-frame and scene-change values for the opening
section stay as measured without the mask.

Consequence: piped results can differ slightly from file results in the opening
section, mainly where the logo stays visible over a dark frame. The end-to-end
check measures this with `score.py` rather than requiring identical output.

Correction (October 27): in practice the "kept logo evidence" would have to be
an opening picture copy at one picture per second (about 130 KB/s at 480x270).
The logo's position is unknown until the logo is found, so nothing smaller can
be scored later. The copy would need its own size cap, because the logo
search's give-up never fires when `added_recording` > 0
(`src/detection/logo.cpp:1098-1099`), and the default is 14. Not built for now
(R1).

New evidence (October 27): with `two_pass_logo=0`, file-mode accuracy on
faaea10a and 77483aca (old 15 s labels) was identical to running with the
rewind on:

- faaea10a: 6 of 12 breaks, 1 false, 53 s of programme cut, 690 s of ads missed;
- 77483aca: 23 of 23 breaks, 0 false, 206 s of programme cut, 273 s of ads
  missed.

So the rewind may matter little in practice, and simply declining it on a pipe
may be enough. See R1.

## Platform facts (measured on the NAS, October 24, 2026)

Environment: TV container `tv_app-tv-1` (Alpine) has `/dev/dri` mapped, the
Intel `iHD` VA driver, and system FFmpeg 8.1.2 with VAAPI and QSV enabled. The
deployed `/usr/local/bin/comskip` is statically linked against vcpkg's FFmpeg,
which is built without VAAPI: the vcpkg port's `vaapi` feature (Linux only) is
not requested in `vcpkg.json`. The Windows vcpkg build already has D3D11VA and
DXVA2.

Input: `bin/perf/hd-h264-5min.ts`, 1920x1080 H.264, 11,036 frames (about
6 minutes). FFmpeg CLI times use `-benchmark`; Comskip times use `time -v`.

| Run | Wall s | CPU s (user+sys) |
|---|---|---|
| Software decode, grey, 4 threads, no output | 20.5 | 69.0 |
| Software decode, grey, 1 thread, no output | 54.3 | 56.3 |
| VAAPI decode, frames stay on GPU | 21.5 | 9.2 |
| VAAPI decode + copy full NV12 frames to CPU | 40.0 | 44.9 |
| VAAPI decode + copy only the luma plane | 49.4 | 60.4 |
| VAAPI decode, FFmpeg's automatic transfer | 52.2 | 55.4 |
| VAAPI decode + GPU scale to 960x540 + copy | 21.9 | 19.5 |
| VAAPI decode + GPU scale to 480x270 + copy | 21.6 | 21.1 |
| Deployed Comskip on the 1080p file | 41.5 | 76.6 |
| FFmpeg grey 1080p raw frames piped into deployed Comskip (`/dev/stdin`) | 60.2 | 199.2 (both processes) |
| Deployed Comskip on pre-scaled 480x270 grey raw frames (file) | 6.9 | 6.7 |
| Deployed Comskip on a 480x270 H.264 proxy | 5.4 | 10.6 |
| Software decode of that proxy alone, 1 thread | 2.4 | 2.8 |

With every frame written to a sink so the CPU reads its pixels:

| Run | Wall s | CPU s |
|---|---|---|
| VAAPI full size, copy | 47.7 | 79.8 |
| VAAPI full size, direct map | 39.3 | 74.4 |
| VAAPI GPU scale to 480x270, copy | 21.6 | 26.8 |
| VAAPI GPU scale to 480x270, direct map | 21.6 | 25.3 |
| Software decode + CPU scale to 480x270 | 38.6 | 131.2 |

Conclusions:

1. Copying full-size frames back from the GPU costs more CPU than it saves.
   Mapping GPU memory directly (the integrated GPU shares RAM) saves only
   about 10% at full size, because the surfaces are tiled and the CPU reads
   them through uncached mappings. HardwareDecoder is worth having only if it
   reduces picture size on the GPU before the CPU reads it. At small size,
   direct mapping and copying cost the same.
2. The deployed Comskip already accepts a non-seekable pre-decoded stream:
   grey raw video plus PCM audio in NUT on `/dev/stdin`, with presentation
   times preserved. Its cut list matched file mode on this fixture. That
   fixture never found a logo, so the rewind on a pipe was untested.
   Corrected October 27: on real recordings the logo is found, and the rewind
   then makes Comskip exit 2 ("Cannot open recording /dev/stdin"). With the
   rewind off, results still differ from file mode (see Stage 1 above).
3. Piping full-size raw frames is slower than decoding the file. The
   pre-decoded path pays off only for small pictures from a process that is
   already decoding.
4. On the TV app's 480x270 proxy, decoding is about 2.8 of 10.6 CPU seconds
   for 6 minutes, and the whole run takes about 1.5% of real time. VAAPI
   doesn't change the NAS's live cost much there. It matters only for
   full-resolution inputs. Corrected October 27: piping does matter. It cuts
   Comskip's CPU by 40% and removes the TV app's proxy encode (see below).
5. Unverified option (external research): have the GPU's scaler write its
   output into memory Comskip allocated itself (a "user pointer" VAAPI
   surface). The CPU then reads ordinary cached memory and skips the
   per-frame map, unmap and copy. FFmpeg does not offer this, so it needs
   direct VAAPI calls inside HardwareDecoder. It must be shown to work on the
   N100's `iHD` driver, and to save CPU over measurement row 4 of the second
   table, before the contract relies on it.

## Measurements (test container, October 27, 2026)

These use the TV app's proxy (480x270, 30 fps H.264) and an ini that differs
from the defaults only in `detect_method=43`, `live_tv=1`,
`live_tv_retries=12` and `output_edl=1`.

| Change | Effect |
|---|---|
| `thread_count=1` instead of 2 | 24% less CPU, identical output |
| `scan_step=2` (opt-in, default off) | 37% less CPU. Identical on 3 of 4 recordings, 5-6 s different on one. Likely causes: the fixed "fewer than 5 bright pixels" line test in the picture-box scans, and the dim check comparing sampled counts with full-picture shares (roadmap stage 3) |
| FFmpeg built with `--enable-gray` | About 3% less Comskip CPU. Dropped |
| Grey frames + PCM piped in NUT instead of the proxy | Comskip CPU (user+sys) went from 99.8 to 59.9 s on 93 min, and from 199 to 121 s on 183 min |
| Same change, producer side | Saves the x264 proxy encode, about 18% of the whole chain (90 min of 1080p HEVC) |
| Pipe transfer | About 37 s of system time and 21 GB of raw data for 93 min |

A 64 KB pipe holds about half a 480x270 grey picture. So any stall in Comskip
blocks the producer within one frame. That is why the plan has a read buffer
and Must-not-get-worse 9.

## Questions raised October 27 (all decided as recommended)

The same questions appear in the roadmap: R1 = roadmap Q2, R2 = Q1, R3 = Q3,
R4 = Q4, R5 = Q12.

- R1. On a pipe, should Comskip only decline the rewind, or also rescore the
  opening from an opening picture copy? Recommended: decline only. Add
  rescoring if hand-checked labels show errors in the opening. (This replaces
  part of Q3.)
- R2. If the stage 1 fix also changes file-mode output, is a changed but
  no-worse baseline acceptable? Recommended: yes, and it is recorded.
- R3. Read buffer size? Recommended: at least 10 s of pictures (about 40 MB).
- R4. What form should the end-of-recording signal take for growing files?
  Recommended: a marker file that the TV app creates next to the recording.
  For pipes, the end of the stream.
- R5. Is the snapshot stream still wanted, now that the TV app polls files and
  waits for the final pass? Recommended: keep it opt-in, but build it last in
  stage 2 and drop it if time is short.

## Assumptions

- [ASSUMED: the pre-decoded stream carries grey 8-bit pictures and 16-bit PCM
  audio with presentation times, in a container format the stock FFmpeg command
  line can already write (NUT, as measured), so the producing side needs no
  custom code. Float audio was tried and changed nothing.]
- [ASSUMED: caption data is unavailable in the pre-decoded path; caption-based
  scoring is off there.]
- [ASSUMED: HardwareDecoder delivers pictures at the source resolution.
  It is parked with stage 8: the measurements say it pays off only if the GPU
  shrinks pictures first.]
- [ASSUMED: one Session is driven from one thread at a time; different Sessions
  may run concurrently.]
- [ASSUMED: the snapshot stream format is line-delimited records with times in
  seconds; its exact fields are fixed in the OutputPublisher contract.]
