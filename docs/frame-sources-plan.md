# Plan: interchangeable frame sources behind a detection session

Status: Stage A outline (terms, parts, flow), October 24, 2026. User decisions
recorded below. Outline revised to drop the published library interface;
per-part contracts not yet written. Nothing is implemented.

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
- **HardwareDecoder** (boundary): same promise as SoftwareDecoder, but uses a
  GPU device and delivers only the luma plane.
- **StreamReader** (boundary): reads pictures and sound that another process has
  already decoded, from a non-seekable byte stream.
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
- Output writers (`src/output`): unchanged formats. Changed: live outputs are
  published whole (already a goal in the adaptive-detection plan).
- Settings loading, localisation, review UI, CSV/TXT replay input: unchanged.

## Out of scope

- A published library interface or any language binding (decision Q1). The
  Session split keeps that possible later.
- Any change to detection algorithms, thresholds or scoring.
- Changes to the TV application. It keeps calling the executable and polling
  output files; adopting StreamReader or the snapshot stream needs a TV-side
  change planned separately.
- GPU computation of detection features (no meaningful gain).
- Reducing picture size on the GPU before transfer: a later, measured option,
  because it changes the detector's sampling.
- macOS verification.

## Must not get worse

1. Software file decoding produces byte-identical outputs to master `66d5717`
   on every fixture in `bin/perf` and passes every existing test.
2. Software file decoding uses no more CPU time than master: within run-to-run
   noise, [ASSUMED: at most 2%] on the NAS for the 10-minute SD and 5-minute HD
   fixtures.
3. The software path gains no extra full-size picture copy per frame.
4. Existing command lines, INI files, output files and exit codes keep working
   unchanged. The TV application needs no change to keep its current behaviour.
   The snapshot stream is off unless requested, so standard output is unchanged
   by default.
5. HardwareDecoder results: `bin/perf/score.py` scores no worse than software on
   the truth fixtures; commercial boundaries differ from software by at most
   [ASSUMED: one frame].
6. Two sessions in one process give the same results as two separate runs.

## Flow

1. CommandLine loads Settings from arguments and INI, as now.
2. CommandLine picks a source: an input of `-` (or an explicit stream option)
   selects StreamReader; a file with a hardware device requested by flag selects
   HardwareDecoder; otherwise SoftwareDecoder.
3. CommandLine asks the source to open. On `device unavailable` from
   HardwareDecoder, it stops with the existing media-open exit code and a
   message naming the device (hardware is opt-in, decision Q2).
4. CommandLine creates a Session with Settings and the source's stream info.
5. Driver repeatedly asks the source for the next item and forwards it:
   picture → Session.add_picture, sound → Session.add_sound, caption data →
   Session.add_captions.
6. The source answers `waiting` for a growing file with no new data yet. Driver
   applies the existing live retry/follow rules and asks again. Past the
   timeout it treats the input as ended. StreamReader never answers `waiting`;
   it blocks until data or end of stream.
7. Session.add_picture may answer `rewind requested`.
   - Seekable source: Driver asks the source to restart from the first picture
     and tells Session to restart, preserving the logo it found (today's
     behaviour).
   - Non-seekable source: Driver declines the rewind. Session rescores logo
     presence for the opening section from its kept logo evidence and continues
     (see the Q3 finding).
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

On the NAS, inside the TV container, analyse one real 480x270 proxy recording
three ways:

1. software: `comskip rec.ts`
2. hardware: `comskip --hwaccel=vaapi rec.ts`
3. pre-decoded: another process decodes `rec.ts` to grey pictures and audio
   and pipes them into `comskip -`, with the snapshot stream enabled.

Pass if (1) is byte-identical to master's output, (2) meets Must-not-get-worse 5,
and (3) scores no worse than (1) with `score.py` on the truth fixtures, with any
boundary differences confined to the opening section, and its final snapshot
stream record matches its final output files. Record CPU and wall time for each.

## Decisions (user, October 24, 2026)

- Q1 No published library. The value is the internal Session split; Comskip
  stays an executable that the TV application runs. A C++ library interface or
  a binding can be added later on top of Session.
- Q2 Hardware decoding: opt-in by flag only. Default output is unchanged.
- Q3 Logo rewind on a non-seekable source: Session keeps compact logo evidence
  and rescores the opening section itself, with no second decode. File mode
  keeps today's rewind; switching file mode over is a later, measured change.
- Snapshot stream: included as an opt-in output, so a supervising process can
  stop polling output files.

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

## Assumptions

- [ASSUMED: the pre-decoded stream carries grey 8-bit pictures and 16-bit PCM
  audio with presentation times, in a container format the stock FFmpeg command
  line can already write, so the producing side needs no custom code.]
- [ASSUMED: caption data is unavailable in the pre-decoded path; caption-based
  scoring is off there.]
- [ASSUMED: HardwareDecoder delivers pictures at the source resolution.]
- [ASSUMED: one Session is driven from one thread at a time; different Sessions
  may run concurrently.]
- [ASSUMED: the snapshot stream format is line-delimited records with times in
  seconds; its exact fields are fixed in the OutputPublisher contract.]
