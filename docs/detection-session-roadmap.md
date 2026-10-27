# Roadmap: detection session, live mode and accuracy

Status: proposed order, October 27, 2026. Nothing here is implemented. Each
stage says what it needs before work starts. Stages 1 and 2 are planned in
`docs/frame-sources-plan.md`. Stages 5 and 7 need their own plans first. The
earlier `docs/adaptive-commercial-detection-plan.md` covers the TV-app side
(feedback, storage, a learning worker); this roadmap covers Comskip itself and
reuses that plan's ideas where they overlap (outputs written whole, a review
page, fingerprints).

## What we are aiming at

Comskip runs on the N100 NAS inside the TV app, mostly on NFL recordings. In
order of importance:

1. Don't cut programme content.
2. Catch most of each break. Exact edges matter less.
3. Get marks while the recording is still going (near-live).

Speed is no longer the problem. On the TV app's 480x270 proxy Comskip runs about
20 times faster than real time. Accuracy is the problem: with the old rough
labels, one run on faaea10a finds 6 of 12 breaks, and one on 77483aca finds
all 23 but also cuts 206 s of programme.

## How results are measured

- `bin/perf/score.py`: breaks found, false breaks, programme seconds cut, ad
  seconds missed. Today it reads only `*.truth.json`, which
  `bin/perf/real_truth.py` builds for `faaea10a` and `77483aca` from 30-second
  contact sheets. Those edges are only good to about 15 s.
- New labels, edges within 5 s, as `bin/perf/real/<stem>.labels.csv`.
  `g5.labels.csv` is done (90 min of NFL, 10 breaks). `faaea10a` and `77483aca`
  are being relabelled, and `g4` follows.
- **The labelled set** means every recording with a `.labels.csv`: today `g5`,
  then `faaea10a` and `77483aca` once relabelled, then `g4`. Wherever this
  roadmap says "labelled recordings", it means this set.
- The test container (`docker/test`, `bench.sh` modes file/proxy/gray/hitch/tee)
  for CPU and wall time. The production NAS container is never touched.
- "No worse" means, on every labelled recording: no more programme seconds cut,
  no fewer breaks found, no more false breaks. Ad seconds missed may move by a
  few seconds.

## Already settled (October 27 measurements)

- `thread_count=1` instead of 2 on the proxy: 24% less CPU, same output.
- `scan_step=2` (opt-in, merged): 37% less CPU. Same output on 3 of 4
  recordings, 5-6 s different on one. Stage 3 fixes the likely cause.
- FFmpeg built for grey-only decoding: about 3% less CPU. Dropped.
- Piping already-decoded grey frames into Comskip: 40% less Comskip CPU, and the
  TV app no longer has to encode the proxy (about 18% of the whole chain). Worth
  doing, once stage 1 makes its results match file mode.
- The TV app now waits up to 5 minutes for Comskip to finish, instead of killing
  it 2 s before the end. So the final pass now runs, and catches breaks that the
  live marks missed.

## Stages

### Early, being measured now: turn silence on for the TV app (S)

- **Goal:** use `detect_method=107` (today's 43 plus silence) in the TV app's
  ini. It is a single ini change.
- **Measured so far:** on g5 it found 9 of 10 breaks, against 7 of 10 with 43,
  scored against the new labels.
- **Gate:** adopt it only once it also scores no worse on the other games in the
  labelled set (faaea10a, 77483aca, g4). Watch programme seconds cut first.
- **Risks:** with silence on, the final pass counts unknown volume as silent
  (`volume < 6` also matches -1), so it can add false cut points where audio is
  missing. Stage 3 fixes that, and 107 should be measured again after it.

### 0. Reproduce the captions crash (any time, S)

- **Goal:** find out whether turning on captions (`detect_method` bit 16) still
  crashes FFmpeg, and whether caption data survives the TV app's proxy encode.
- **Why now:** it doesn't depend on anything else and blocks a free source of
  evidence. It can run alongside any stage.
- **Depends on:** the test container.
- **Done:** a crash is reproduced and fixed, or many recordings run clean with
  captions on. We also know whether the proxy (or the pipe) carries captions.
- **Risks:** the crash may only show on certain broadcasts. If the proxy drops
  captions, the TV app would need a change to keep them.
- **Questions:** none.

### 1. Make the pipe give the same result as a file (M)

- **Goal:** feeding Comskip the same decoded frames through a pipe or from a file
  gives the same commercial list.
- **Why now:** the pipe is the biggest saving we have, and stage 2 builds on it.
  Today, with the same frames, the 77483aca edges move by 1 frame. On faaea10a
  some edges move up to 40 s and one break is lost (5 of 12 found instead of 6).
  Sending float audio didn't change that, so audio volume scaling isn't the
  cause. The prime suspect is how Comskip handles the audio and video start
  times. The pipe changes them, and the NUT grey stream starts at 0.033 s.
- **Depends on:** labels that can see the difference. A 1-frame or 40 s shift
  can't be scored against 15 s labels. So before stage 1's exit check, part of
  stage 4 comes first: `score.py` must read `.labels.csv`, and faaea10a and
  77483aca must be relabelled (g5 is already done). Otherwise it is
  investigation first, then a small fix.
- **Done:** with `two_pass_logo=0`, on the labelled set and the `bin/perf`
  fixtures, the pipe and the file give identical cut lists, or differ only by
  an offset we understand and remove. Also: the same NUT bytes, saved as a file
  and read as a file, match the piped run. That shows the cause is in how the
  input is read, not in the frames. Identical cut lists are checked directly.
  Scoring against the labels is needed only where the fix changes file-mode
  output (Q1).
- **Risks:** the fix may also change file-mode results, if file mode has the
  same timing slip. Any such change is measured and recorded, not hidden.
- **Questions:** Q1.

### 2. Detection session, pipe reader and whole outputs (L)

- **Goal:** split detection from Comskip's own decoding loop, so frames can come
  from a file or from a pipe. Add a reader for the pipe that keeps up even when
  analysis briefly stalls. Write output files whole, so the TV app never reads
  half a list. Optionally, let the TV app say "the recording has ended" instead
  of Comskip waiting `live_tv_retries` x 4 s (about 48 s with the TV app's
  settings). Today that wait is a single blocking read with a 48 s timeout, so
  the signal has to be able to interrupt that read. The frame sources plan says
  how.
- **Why now:** live mode, fingerprints and any later input type all need this
  split. It also removes the logo rewind failure on pipes. Today, with
  `two_pass_logo=1`, Comskip on `/dev/stdin` exits with code 2 ("Cannot open
  recording /dev/stdin") when the rewind tries to reopen the input.
- **Depends on:** stage 1.
- **Done:** file mode gives byte-identical output to master after stage 1 on
  every fixture, with no more CPU. The pipe scores no worse than the file on the
  labelled set, with any differences confined to the opening minutes. A test
  where Comskip deliberately stalls for several seconds shows two things: the
  reader keeps draining the pipe, and the cut list is the same as without the
  stall. Output files are never seen half-written.
- **Gaps in the pictures:** detection counts frames, not seconds. It uses frame
  numbers, `added_recording` x fps x 60, and gap lengths in frames in live mode.
  So a missing picture would shift everything after it. Comskip itself never
  drops anything: its reader waits rather than discard. If the producer drops
  pictures anyway, Comskip fills the gap with placeholder frames derived from
  presentation times. Placeholders are never black, never silent and never cut
  points, and they keep the logo state of the frame before. The count of
  placeholders is logged.
- **Risks:** the rewind is tangled into the decode loop, and this is the largest
  refactor here. The TV app only benefits once it changes to feed the pipe, and
  that change belongs in the TV repo. There, the recorder must never wait on
  Comskip: it needs a generous queue that drops frames (and logs it) when full.
  A proxy-file fallback would mean the TV app still encodes the proxy alongside
  the pipe (bench mode `tee`), which gives up that saving. Whether to keep that
  safety net is a TV-app decision, not part of this roadmap.
- **Questions:** Q2, Q3, Q4, Q5, Q12.

### 3. Measurement cleanups (S)

- **Goal:** fix the small measuring quirks that make speed settings change
  results (all in `src/detection/scene_analysis.cpp` unless noted):
  - the dim-frame check counts sampled pixels but compares them with 5% and
    35% of all pixels, so with `scan_step=2` fewer frames count as dim (it
    should compare shares);
  - the picture-box scans (`ScanTop/Bottom/Left/Right`) call a line dark when
    fewer than 5 sampled pixels on it are bright. That fixed 5 means something
    different when `scan_step` samples half as many pixels, and when it steps
    over every other line. This is a likely cause of the 5-6 s difference;
  - the lowest-seen bright and dim values (`min_hasBright`, `min_dimCount`)
    are scaled by picture size (720x480 divided by width x height), not by the
    sampling step, so they shift with `scan_step` too;
  - unknown volume counts as silent in the final pass when silence is on
    (`volume < 6` also matches -1, `src/detection/detection.cpp`);
  - the logo search never gives up when `added_recording` is above 0: each
    check adds `added_recording` x 60 s to `give_up_logo_search`
    (`src/detection/logo.cpp`). The default `added_recording` is 14, and the TV
    app's ini doesn't change it, so for the TV app the limit grows by 840 s per
    check and is never reached.
- **Why now:** after stage 2, so the refactor can first be checked
  byte-identical. After this, `scan_step=2` and `thread_count=1` can safely
  become the TV app's settings (or Comskip's defaults).
- **Depends on:** stage 2.
- **Done:** `scan_step=2` matches `scan_step=0` to within one frame on the
  labelled set and the fixtures. Each fix scores no worse on its own.
- **Risks:** each fix changes output on purpose, so the byte-identical baseline
  moves forward after each one.
- **Questions:** Q6.

### 4. Labelling tool and hand-checked labels (M)

- **Goal:** label a small set of recordings from contact sheets (decided
  October 27: the agent labels, the user spot-checks), with edges refined to
  about a second by denser sheets around each edge. An optional review page
  can step through edges for spot checks.
- **Pulled forward into stage 1:** `score.py` reading `.labels.csv`, and
  relabelling faaea10a and 77483aca. g5 is done. The rest of this stage
  continues alongside stages 1-3: g4, then more games, up to 8 recordings in
  all, with 2 kept aside for final checks.
- **Why now:** accuracy work (stages 5 and 6) can't be judged with 15 s labels.
  Small edge improvements disappear inside that error.
- **Depends on:** nothing technically.
- **Done:** 8 recordings are labelled, 2 of them kept aside, and `score.py`
  reads the labels.
- **Risks:** labels made by eye from contact sheets can be wrong in ways the
  detector is also wrong (for example a dark programme scene read as a break).
  Your spot checks guard against that, so they should focus on edges where
  Comskip and the label disagree.
- **Questions:** Q7, Q8.

### 5. Windowed live mode (L, needs its own plan)

- **Goal:** live marks that are nearly as good as the final pass. Comskip keeps
  every frame's measurements (about 100 bytes per frame, about 30 MB for 3
  hours) and its running learned state: logo, how far to trust it, usual aspect
  ratio, thresholds. Every 30-60 s it reruns break finding and scoring on a
  recent window (for example the last 10 minutes). Anything older than about 3
  minutes becomes settled, and the newest minutes are marked provisional. The
  full analysis still runs at the end. Silence is found as the audio arrives.
- **Why now:** this is the near-live goal. Today's live mode
  (`src/detection/live.cpp`) recomputes at most once per 15 s of video. Each
  time, it pairs cut points that are at least `min_commercial_size` (4 s) and at
  most `max_commercial_size` apart, joins pieces less than 10 s apart into
  breaks, and lets the logo veto a piece. It doesn't score blocks or use aspect
  ratio. Silence hardly reaches it. With the TV app's `detect_method=43`,
  silence is off entirely. With silence on, a frame is only marked silent when
  it is processed, and its volume is normally still unknown then. So only
  frames whose volume is already known to be exactly 0, or known and quiet with
  `min_silence=1` (the default is 12), are marked. Volume that arrives later is
  copied into each frame and black-frame entry (`set_frame_volume`), but that
  never adds a silence cut point, and the live code never reads volume itself.
- **Depends on:** stages 2 and 3, and stage 4 to measure it.
- **Done:** replay saved per-frame data cut off at successive times and compare
  each live result with the full result. Settled marks agree with the final pass
  (agreement level to be set in its plan), and live mode cuts no more programme
  than the final pass.
- **Risks:** the final analysis changes its own state as it runs, so it must
  work on a copy or be made repeatable. Rerunning every minute must stay cheap.
- **Questions:** Q9.

### 6. Accuracy changes (M each)

- **Goal:** try the ideas on the improvements page of `docs/algorithm`, one at a
  time (silence on has moved to the early item above): cut where the logo
  appears or
  disappears; let the logo be one vote among several instead of a veto; a
  sports profile; expect the next seam 15, 30 or 60 s after one is found. Small
  picture buffers can help here, for example a rolling 30 s of full frames
  (about 120 MB) to look again near a suspected seam with looser thresholds.
  Long full-frame buffers are not worth it on a 7 GB NAS.
- **Why now:** only once the labels exist and live mode shares the same scoring,
  so each gain shows up both live and at the end.
- **Depends on:** stage 4. Stage 5 is preferred first.
- **Done:** each change is kept only if it scores no worse on every labelled
  recording and better on at least one.
- **Risks:** tuning to a handful of recordings. Keep some labelled recordings
  aside, untouched, for a final check.
- **Questions:** none until labels exist.

### 7. Audio fingerprints of repeated ads (L, needs its own plan)

- **Goal:** recognise audio that repeats across recordings. Repeats across
  different shows or channels are ads, a strong vote in the score (black-frame
  seams still give the edges). Repeats within one series near the start or end
  are intros or credits, which could become future skip markers. Estimated
  cost: about 1% of one N100 core, about 100 KB of storage per hour, and lookups
  in milliseconds.
- **Why now:** it is the biggest likely accuracy gain, but it needs stages 2 and
  4 to be measured, and stage 5 to be used live.
- **Early cheap step (any time, S):** fingerprint the audio of one recording in
  the test container. Measure the CPU, memory and storage it takes, and count
  how many stretches repeat within that one recording. This shows the real cost
  on the N100 and how much an ad break repeats inside a single game, before the
  full plan is written.
- **Depends on:** stages 2 and 4. Stage 5 only for the live use.
- **Done:** set in its own plan. Fingerprints are there to make marks more
  accurate, not to reproduce today's marks, so there is no "same output as
  before" check. They are judged against hand-checked labels: they must never
  cut more programme content (this rule comes first), and they must catch
  measurably more of the breaks. If they turn out too costly to run live, they
  still count as done when they refine the marks after the recording ends or in
  the background, and the live marks are improved when that result arrives.
- **Risks:** programme content repeats too (theme songs, promos, NFL graphics).
  This is the main way fingerprints could cut programme, which is why that rule
  comes first. The first airing of an ad gets nothing. Campaigns turn over, so
  old entries must expire, and duplicates must be merged. The cost may rule out
  live use. If it does, the plan falls back to refining marks after the
  recording, and does not drop fingerprints.
- **Combined with today's detection (user's direction, October 27):** a
  fingerprint match is a strong vote on a block, and where nothing matches,
  today's evidence decides as before. During and after each recording, breaks
  the existing detection is confident about are fingerprinted into the library,
  so it grows by itself. Guard against the loop reinforcing a mistake: only
  add pieces bounded by seams, and treat an entry as an ad only once it has
  recurred in a different show or channel, or within the same recording as a
  seam-bounded, logo-free piece of a standard ad length. Ads repeat within one
  game, so a pass over the whole recording also lets later airings mark the
  first one. Programme also repeats within a game (bumpers, theme stings,
  sponsor billboards), which is why the length and logo conditions apply. Never add pieces that labels or
  the logo say are programme, and expire entries that stop recurring.
- **Keep every recording's fingerprints, not only known ads (October 27):**
  ads are found as repeats, so the library needs no confirmation from today's
  detection. Each new recording is matched against all stored recordings
  (about 100 KB per hour, expired after some weeks). A repeat found later
  also corrects the earlier recording's marks. Recordings made at the same
  time on other channels share national ads, which helps live matching.
  Seed the library by fingerprinting existing recordings on the NAS.
- **Measured October 27 (two labelled 90-minute games, NBC and CBS):**
  fingerprinting costs about 9 CPU s per 90 min including audio decode
  (Comskip about 65 s on the proxy). Repeats covered about 15% of ad seconds
  within one game and 25% across both, touching 15 of 23 breaks. There were no
  programme false positives except a 31 s in-game Google split-screen, whose
  audio is a real ad (an "in-programme ad"), and a 2.5 s CBS sting that the
  length rule removes. Script: scratchpad `fp/repeats.py`, to be moved into
  `bin/perf` when this stage starts.
- **Questions:** Q10.

### 8. GPU hardware decoding (VAAPI), opt-in (M, may be dropped)

- **Goal:** decode files on the GPU when asked by flag, as already outlined in
  the frame sources plan.
- **Why now:** last, because with the pipe the TV app never needs Comskip to
  decode. It helps only for running Comskip on full-size files, and only if the
  GPU shrinks the picture first.
- **Depends on:** stage 2.
- **Done:** see the frame sources plan (scores no worse than software, edges
  within one frame).
- **Risks:** vcpkg's FFmpeg is built without VAAPI, so this changes the build.
- **Questions:** Q11.

## Decisions (user, October 27, 2026)

All recommendations below were accepted, with one change to stage 4: labels
are made by the agent from contact sheets (one frame every 5 s, then denser
sheets around each edge), and the user spot-checks them. The first one,
`bin/perf/real/g5.labels.csv` (90 min of NFL, 10 breaks, edges within 5 s),
took nine contact sheets. The review page (Q8) is reduced to an optional
spot-check aid.

## Questions as asked (all decided as recommended)

- **Q1.** If the start-time fix in stage 1 also changes file-mode output, is a
  changed but no-worse result acceptable? Recommended: yes, measured and
  recorded as the new baseline.
- **Q2.** On a pipe, should Comskip simply skip the logo rewind (as
  `two_pass_logo=0` does, which scored the same on faaea10a and 77483aca with
  the old labels), or
  also rescore the opening minutes from a 1-picture-per-second copy it keeps?
  Recommended: skip it first, and add rescoring only if hand-checked labels show
  opening errors.
- **Q3.** How much should Comskip's pipe reader be able to hold? Recommended:
  at least 10 s of pictures (about 40 MB at 480x270). If it fills, Comskip stops
  reading and logs it, and the TV app's queue drops frames.
- **Q4.** How should the TV app say the recording has ended in file mode?
  Recommended: for the pipe, the end of the stream is the signal. For a file,
  the TV app creates a small marker file next to the recording, and Comskip
  checks for it while waiting. (How Comskip notices it during its blocking read
  is in the frame sources plan.)
- **Q5.** Drop the rewind in file mode too? Recommended: not yet. Decide after
  stage 4 labels, since it is one setting away.
- **Q6.** After stage 3, make `scan_step=2` and `thread_count=1` Comskip's
  defaults, or only set them in the TV app's ini? Recommended: the TV app's ini
  first, defaults later.
- **Q7.** Will you hand-check labels? Recommended: yes. Label 8 recordings:
  6 NFL and 2 other, with 2 of them kept aside for final checks.
- **Q8.** Where does the review page live? Recommended: in this repo, as a
  static page built from Comskip's own output plus clips cut from the recording,
  with no TV-app change.
- **Q9.** How late may live marks be settled? Recommended: settled after 3
  minutes, provisional before that, and the TV app skips only settled marks.
- **Q10.** Where are fingerprints stored? Recommended: a table in the TV app's
  existing SQLite (`dvr.sqlite`), with Comskip given a path to it, rather than
  a new external service.
- **Q11.** Keep stage 8 at all? Recommended: park it. Revisit only if you start
  running Comskip on full-size files.
- **Q12.** Is the opt-in snapshot stream (results on standard output) still
  wanted, now that the TV app polls files and waits for the final pass?
  Recommended: keep it opt-in, but build it last in stage 2 and drop it if time
  is short.

## Later option: a long-running detection service (not planned)

Keep one Comskip process per recording for now: start-up takes milliseconds,
shared state is small (fingerprints about 100 KB per recorded hour, a learned
logo a few KB) and can be read from SQLite or a file at start, and a crash
stays contained to one recording. The Session split and the "two sessions in
one process" check keep a service possible. Revisit if a slow-to-load model is
added, the fingerprint library reaches gigabytes, or several recordings should
share one GPU decoder.
