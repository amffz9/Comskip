# Plan: audio-fingerprint ad detection

Status: Stage A outline plus part contracts, October 27, 2026. Nothing is
implemented. The outline still needs the user's approval and one independent
review (planning skill, sections 3 and 4). This plan covers roadmap stage 7
(`docs/detection-session-roadmap.md`). It spans two repositories:

- **This repo** (Comskip fork, branch `fingerprinting`): the fingerprint tool,
  Comskip's evidence export, the offline harness and the label scoring.
- **The TV app** (`F:\Development\TV`): feeding audio, running the tool,
  storing and showing marks. The app's agent implements those steps. This plan
  says only what the app must observably do.

Every step in "Steps" is small, can be committed on its own, and has a check.
The implementer commits after each one.

## Goal (user's words, condensed)

Fingerprinting becomes the main way breaks are found over time, combined with
today's detection. A match is strong evidence. Where nothing matches, Comskip's
evidence decides. Matches less than about 90 s apart inside one break are
joined. Break edges move to the nearest seams, and a break closes on programme
evidence. Every whole recording's fingerprints are kept, not only known ads.
Ads show up as repeats: across recordings or channels, or inside one recording
when the stretch is bounded by seams, has no logo and has a standard ad length.
A later repeat corrects the marks of earlier recordings. Recordings made at the
same time share national ads. The library is seeded from recordings already on
the NAS, and old entries expire after some weeks. Live: codes arrive about 8
per second, 3-5 s aligned with a stored ad is a match, and the stored copy gives
the whole ad's extent, so it is marked at once. Marks are provisional or
settled. Feedback loops are guarded against: nothing that labels or the logo
say is programme is added, and an ad needs recurrence.

## Glossary

- **Chromaprint**: fixed by the measurements and the user. The audio
  fingerprint library behind `fpcalc`. Named only in the Code boundary.
- **Code**: one 32-bit Chromaprint value. Codes come every 0.1239 s
  (the *code period*). Two codes *agree* when at most 10 of their bits differ.
- **Capture**: the TV app's record of one channel being received. It owns
  marks. A *recording* is a window over a capture.
- **Capture seconds**: the TV app's time axis for a capture
  (`segment.start`, `recording.start`, and `commercialMarks[].start/end` are
  all in capture seconds; checked in `dvrMarks.js`). The player's axis is
  derived from it per session by `mapMarksToMedia`. This plan uses capture
  seconds everywhere outside Comskip.
- **Proxy seconds**: Comskip's time axis on its input. Today it restarts near 0
  at each encoder generation (SERVER_HARDENING, `mapProxyTime` bug). Only the
  TV app's Timeline part converts proxy seconds to capture seconds.
- **Run**: a stretch of audio with no gap, with the capture second it starts
  at. Nothing is matched across the boundary between two runs.
- **Code stream**: all codes of one capture, as a list of runs.
- **Seam**: a point where Comskip sees a possible break edge: black frame,
  silence, scene change or aspect change. Comes from Comskip's evidence.
- **Logo interval**: a stretch where Comskip sees the station logo.
- **Comskip block**: one of Comskip's final blocks, with its score and its
  decision (commercial or programme).
- **Evidence**: seams, logo intervals and Comskip blocks for one capture, in
  capture seconds.
- **Repeat**: two stretches of at least 3 s whose codes agree over a common
  offset (the experiment's rule: exact-value seeds, extended while codes
  agree, at least 3 s long, the copies at least 20 s apart).
- **Standard length**: 10, 15, 20, 30, 45, 60, 90 or 120 s, within 1 s.
- **Library entry**: a repeated stretch kept as a unit, with a status:
  `candidate`, `ad`, `in-programme ad`, or `programme` (a known programme
  repeat, such as a sting or bumper, kept so it is never promoted).
- **Airing**: one place in one capture where a library entry occurs.
- **Match**: an airing found while processing a code stream.
- **Mark kind**: `ad` (part of a break; skippable), `in-programme ad` (ad audio
  over programme, such as the 31 s Google split-screen; shown, not skipped by
  default), `sponsor` (sponsor billboard; reserved, see Q6).
- **Mark state**: `provisional` (may still move or vanish) or `settled`.
- **Labelled set**: every recording in `tests/labels` with a `.labels.csv`:
  g5, g4, faaea10a, 77483aca today.
- **Comskip-107**: Comskip alone with the TV ini and `detect_method=107`, the
  baseline (g5 9/10 breaks, g4 13/13, faaea10a 6/12, 77483aca 24/24).

## Parts

This repo:

- **Coder**: turns a run of audio into codes. Boundary part around Chromaprint.
- **Library**: the store of code streams, entries and airings; finds repeats.
  Sole owner of the fingerprint store.
- **LiveMatcher**: matches a growing code stream against the Library's `ad` and
  `in-programme ad` entries and against the capture's own earlier codes.
- **Combiner**: turns matches plus Evidence into marks with kind and state.
- **FingerprintTool**: the executable the TV app runs. It wires Coder, Library,
  LiveMatcher and Combiner for one job and writes the outputs whole.
- **EvidenceExport**: Comskip's new output of seams, logo intervals and blocks.
  Changed part of Comskip.
- **Harness**: offline replay of labelled recordings with a chosen library, and
  scoring. Never used in production.

TV app:

- **AudioFeeder**: delivers runs of decoded mono audio, stamped in capture
  seconds, from the AC-4-capable audio path. Boundary part around FFmpeg and
  the encoders.
- **Timeline**: converts proxy (or pipe) seconds to capture seconds. Already
  exists as `mapProxyTime`; must be made correct across generations.
- **FingerprintRunner**: starts, feeds and watches FingerprintTool jobs,
  queues library passes and backfill under the NAS budgets.
- **MarkStore**: `capture.commercialMarks`; gains `kind`, `state` and `basis`
  fields, and applies fingerprint results and corrections.
- **MarkClients**: web and Android players and the marks API.

## Tree

```
FingerprintRunner: calls AudioFeeder, Timeline, FingerprintTool, MarkStore
FingerprintTool:   calls Coder, Library, LiveMatcher, Combiner
LiveMatcher:       calls Library (read)
Combiner:          calls nothing (pure)
Harness:           calls FingerprintTool, EvidenceExport (via Comskip), score_labels
MarkClients:       read MarkStore through the marks API
```

## Existing parts

- Comskip detection and EDL: unchanged. EvidenceExport is added beside the EDL
  and is off unless asked for.
- `ComskipDetector` (`dvrCommercials.js`): unchanged, except that after its
  final parse it tells FingerprintRunner the capture's Evidence is final.
- `mapProxyTime`/`generationFor`: changed, must ensure correct capture seconds
  across encoder generations (SERVER_HARDENING fix). Needed before any
  combination on multi-generation captures.
- `marksForWindow` (`dvrMarks.js`): changed. Today it keeps only
  `{id, start, end, status, duration}`. It will also pass `kind` and `state`,
  and it will never hand a non-`ad` mark to a client as skippable (see
  MarkClients).
- Re-detection (`dvrRedetect.js`): unchanged. Its window replacement rules
  (ids kept by overlap of at least half the shorter mark, viewer exclusions
  carried to derived marks, one-assignment publish) are reused for
  fingerprint results and corrections.
- Recording, encoders, HLS, the proxy file: unchanged until step T6 (pipe).

## Out of scope

- Deleting or cutting video. Marks only drive skipping.
- Intro and credits markers for series (a later use of the same Library).
- Visual fingerprints.
- The windowed live mode of roadmap stage 5. LiveMatcher does not need it; it
  uses Comskip's live Evidence as it is.
- Publishing the fingerprint stage in the public fork before the patent
  question (roadmap stage 7) is settled. The user accepts the risk for a free
  repo; this plan records it and does not decide it.

## Must not get worse

1. **Programme cut.** On every labelled recording, the combined `ad` marks cut
   no more programme seconds than Comskip-107, find no fewer breaks and add no
   more false breaks. Label rows of kind `sponsor` or `in-programme ad` count as
   neither ad nor programme. Checked by the Harness at every step that changes
   marks, with an empty library and with each seeded library.
2. **Live never blocks recording.** If FingerprintTool is slow, stalled,
   crashed or missing, the recording, the HLS output and Comskip carry on as
   today. Audio the tool can't take is dropped and counted, never queued
   without limit. Checked by a test that stalls the tool for 60 s and one that
   kills it.
3. **NAS budget.** Per live recording: at most 2% of one N100 core and 32 MB of
   memory for FingerprintTool. A library pass or backfill job: run at the
   lowest priority, one at a time, at most 512 MB peak memory. Fingerprint store
   at most 200 MB on disk. Encoder CPU with the added audio output: no more than
   1% above today. Measured on the NAS at steps T2, T4 and T5.
4. **Off or broken means today.** With fingerprinting disabled, the tool
   missing, the store missing or unreadable, or a job failing, the capture's
   marks are exactly what today's Comskip path gives. Checked by turning each of
   those on in a test capture and comparing marks.
5. **Old clients.** A web or Android build that predates mark kinds never
   skips an `in-programme ad` or `sponsor` mark, and sees `ad` marks exactly as
   today.
6. **Viewer judgements.** A mark a viewer removed for a recording stays removed
   after any fingerprint result or correction (same rule as re-detection).
7. **No laundering.** Comskip's own decisions never make a library entry an
   `ad`. Only recurrence does (Library rules below).

## Flow

Post-recording (ships first; needs neither live tapping nor the pipe):

1. A capture's Comskip run finishes its final parse. ComskipDetector tells
   FingerprintRunner.
2. FingerprintRunner queues a *library pass* for the capture. One pass runs at a
   time, at the lowest priority, under the CPU budget.
3. FingerprintRunner asks Timeline to map Comskip's evidence file into capture
   seconds. If Timeline reports `AxisUntrusted` (a multi-generation capture
   before the generation fix), the pass still stores codes (step 5) but produces
   no marks for this capture (stop after step 7).
4. FingerprintRunner asks AudioFeeder for the capture's audio as runs, from the
   saved audio rendition (the AC-4-capable path's output).
5. FingerprintTool.pass: Coder turns each run into codes. Library stores the
   code stream with its context (channel, programme windows, air time).
6. Library finds repeats of this stream against itself and the whole store,
   updates entries and airings, and promotes entries by the rules.
7. Library lists every *other* capture whose marks change because an entry was
   promoted (corrections).
8. Combiner makes marks for this capture and for each corrected capture whose
   Evidence is available, from airings plus Evidence.
9. FingerprintTool writes one marks file per affected capture (whole) and a
   result listing them, then exits 0.
10. FingerprintRunner applies each marks file through MarkStore (window
    replacement, viewer exclusions kept), and MarkStore notifies clients.
11. On a non-zero exit, a timeout, or `StoreUnavailable`/`StoreCorrupt`,
    FingerprintRunner logs it, leaves marks as they are, and retries the pass
    once later. A corrupt store is moved aside and a new one started.

Live (after the post-recording pass is in use):

12. When a capture starts, FingerprintRunner starts FingerprintTool.live for it
    and connects AudioFeeder's live runs to it. Until step T6 the live audio is
    an extra audio-only output of the encoder that already decodes the
    broadcast audio (including the AC-4 path). After T6 it is the same audio
    the app pipes into Comskip.
13. A new encoder generation starts a new run at that generation's capture
    second. A run of audio the tool can't take is dropped and counted.
14. FingerprintTool.live loads the Library's `ad` and `in-programme ad`
    entries (the *live index*) and refreshes it every few seconds, so entries
    promoted by concurrent captures arrive within seconds.
15. LiveMatcher reports a match when 3-5 s of codes agree with one entry. The
    entry gives the whole ad's extent at once.
16. FingerprintRunner keeps handing the tool Comskip's live Evidence (mapped by
    Timeline) whenever the EDL poll sees it change.
17. Combiner turns live matches plus live Evidence into marks. FingerprintTool
    rewrites the capture's live marks file whole; FingerprintRunner applies it
    through MarkStore. Live results only add or update marks, as today's live
    polls do.
18. LiveMatcher also appends the capture's codes to the Library every few
    seconds, so concurrent captures can find shared ads (step 6 rules apply).
19. When audio ends, the tool writes final live marks and exits. Steps 1-11
    follow; the library pass result is settled and replaces the live result.
20. If the live tool dies, FingerprintRunner restarts it at most 3 times per
    capture, each time as a new run. After that the capture has no live
    fingerprint marks; Comskip's live marks continue and the post-recording
    pass still runs.

Backfill (seeding):

21. FingerprintRunner lists existing recordings (the app's own captures and
    the Plex recordings library, Q1) that aired within the expiry window, oldest
    first, and queues one library pass per recording. Passes on recordings that
    have no Evidence only store codes and promote entries; they make no marks
    for those recordings, except for the app's own captures with Evidence.
22. Backfill runs only while no recording, Comskip run or post-recording pass
    is running, at the lowest priority, at most one job at a time, and pauses
    when a capture starts.

Expiry:

23. Once a day, Library deletes code streams older than the expiry window,
    airings in them, entries with no airing left in the window, and streams of
    captures the app says no longer exist.

## End-to-end check

Seed a library with every available recording that aired before g5, except the
labelled ones. Run the post-recording pass on g5, then on g4. Expected: g4's
marks gain coverage from ads first seen in g5, g5's marks are corrected by
repeats first seen in g4, and both recordings meet Must-not-get-worse 1 against
Comskip-107. Then replay g4 through the live path at real speed (Harness live
mode) with that library and check that each matched ad is marked within 6 s of
its labelled start. The two kept-aside labelled recordings (roadmap stage 4)
are scored only at the end, by someone other than the implementer.

## Irreversible steps

- **Database migration in dvr.sqlite** (only if Q2 is answered "dvr.sqlite"):
  back up dvr.sqlite, dry run on a copy, check row counts before and after, the
  user gives the go-ahead. With the recommended separate store file, there is
  no migration in dvr.sqlite: new mark fields live in `metadata_json`.
- **Deploy to the NAS** (steps T3, T4, T5, T6): each behind a setting that
  defaults off; the user switches it on. Off restores today's behaviour.
- **Corrections rewriting past captures' marks**: before step T3 is switched
  on, MarkStore keeps the previous marks of every capture it corrects for 14
  days, so one action can restore them.

## Decisions

### D1. Where fingerprinting runs

| Option | Combining with seams/logo | Live latency | Shared index for concurrent captures | Crash isolation | AC-4 | Proxy timestamp bug | Offline testing on labels |
| --- | --- | --- | --- | --- | --- | --- | --- |
| (a) Inside Comskip | Direct | Good | No: one process per capture; only through the store | A fingerprint fault ends detection | Only if Comskip's input carries the transcoded audio | Inherits it | Good |
| (b) Separate tool from this repo | Through EvidenceExport | Good | Through the store, refreshed every few seconds | Separate | Fed from the AC-4-capable path | Avoided: audio stamped in capture seconds | Good |
| (c) In the Node app | Through EvidenceExport | Good | In one process | Matching shares the server process, or needs a worker | Fed from the AC-4-capable path | Avoided | Needs the TV repo to evaluate; algorithm in a second language |

**Recommended: (b).** A separate FingerprintTool, built in this repo, linking
Chromaprint, run by the TV app once per live capture and once per library pass.
Comskip only adds EvidenceExport. Reasons: the library pass, corrections to
past captures and backfill are not per-capture detection work and don't belong
inside Comskip; a fingerprint fault never costs today's marks; audio arrives
stamped in capture seconds, so fingerprints never see the proxy axis; and the
Harness tests the same executable against the labels in this repo. The shared
live index costs at most 4 x 20 MB with one process per capture, which fits the
NAS. A single long-running fingerprint service holding one index is the later
option (roadmap, "long-running detection service") if the per-capture refresh
proves too slow.

### D2. Storage

- **(a) Tables in dvr.sqlite** (roadmap Q10): one database, one backup. But a
  process outside the app writes the app's database, lock waits from a long
  library pass can stall app writes, a migration is needed, and code streams
  (up to 200 MB) grow the app's backups.
- **(b) A separate store file next to dvr.sqlite, owned by FingerprintTool.**
  Losing it loses only evidence and is rebuilt by backfill. No migration in the
  app. The app passes its path.

**Recommended: (b).** This changes roadmap Q10. Writers: only FingerprintTool
(live jobs append codes; library passes write entries and airings; one library
pass at a time). Concurrent live jobs and one pass may write at once; the store
must allow that without a job failing (`StoreBusy` is retried, never fatal).

Concepts held (not a schema):

- **Code stream**: capture id, channel, air start (wall clock), programme
  windows (capture-second ranges with series id and title), runs (start capture
  second, codes), source (`capture` or `plex`), expires at.
- **Entry**: id, status (`candidate`, `ad`, `in-programme ad`, `programme`),
  length in seconds, a reference copy of its codes, first seen, last seen,
  number of airings, number of distinct series and channels, why it has its
  status.
- **Airing**: entry id, capture id, start and end in capture seconds, how it
  was found (`pass` or `live`), whether seams bound it within 1 s at each end,
  whether a logo was seen in it (yes, no, unknown).
- **Corrections outbox**: capture id, reason, time; emptied by the result of
  each pass.

Size: codes are about 100 KB per recorded hour. At an assumed 40 recorded hours
a week [ASSUMED: unknown; the jobs table can tell] and 8 weeks, that is about
32 MB of codes plus entries, airings and whatever lookup structure the
implementer chooses, within the 200 MB limit.

### D3. Marks with kind and state reach the app; corrections

- FingerprintTool writes, per capture, one marks file, written whole: the
  capture id, the Evidence revision it used, and marks
  `{start, end, kind, state, basis}` in capture seconds. `basis` is
  `fingerprint`, `comskip` or `both`.
- MarkStore keeps fingerprint marks in `capture.commercialMarks` beside today's
  fields (`id, start, end, status, proxyStart, proxyEnd, generation, source,
  detectedAt`), adding `kind`, `state`, `basis`. A mark without `kind` is an
  `ad`; without `state` it is `settled` if `complete`, else `provisional`.
  Existing stored marks need no change.
- The marks file *replaces* Comskip's marks only over the windows it covers,
  with re-detection's rules (ids kept by overlap, viewer exclusions carried to
  derived marks, published in one assignment). Outside those windows Comskip's
  marks stay.
- Corrections to past captures are the same marks files for other captures,
  listed in the pass result. A correction may add `ad` marks, widen marks, and
  remove or narrow only marks whose `basis` is `fingerprint`. It never removes
  a Comskip mark (Q5).
- MarkClients: marks the client already understands keep their shape. `ad`
  marks behave as today. `in-programme ad` and `sponsor` marks go to clients
  only in a form old clients don't skip (Q4). `provisional` marks: skippable as
  today's live marks are, but auto-skip uses only `settled` ones (Q3).

### D4. Timeline

- Fingerprint time is capture seconds from the start: AudioFeeder stamps each
  run with the capture second its first sample belongs to (a generation's
  `base` for live audio; the segment start for saved audio). Within a run, time
  is sample count divided by sample rate. A new generation, a reception gap or
  a dropped stretch starts a new run. The tool never sees proxy seconds.
- Comskip's Evidence is in proxy seconds and is mapped by Timeline. Until the
  SERVER_HARDENING generation fix lands, Timeline reports `AxisUntrusted` for
  captures with more than one generation that wrote to the proxy, and those
  captures get fingerprint codes but no fingerprint marks.
- After the pipe conversion (T6) the app decides the pipe's clock, so it must
  feed Comskip one continuous clock across generations; Timeline then maps that
  clock the same way.

### D5. Staging

See "Steps". Order: offline in this repo first, then the post-recording pass in
the app, then backfill, then live, then the pipe, then making fingerprints
primary. Backfill is placed before live (the user's order had it after):
backfill is the same machinery as the post-recording pass, and live matching
is only useful once the library holds confirmed ads.

### D6. Must-not-get-worse

See that section. The programme-cut rule is checked by the Harness at every
step that changes marks.

## Part contracts

Types used below:

- `CaptureId`: the TV app's capture id (text). Two are equal when the text is.
- `Seconds`: capture seconds, a non-negative real number.
- `Run`: `{start: Seconds, audio}`; audio is mono samples at a fixed rate the
  Coder states; a run is at least 0 s long.
- `CodeRun`: `{start: Seconds, codes}`; code *i* covers
  `start + i x code period`.
- `Context`: `{channel, airStart (wall clock), programmes: [{from, to: Seconds,
  seriesId, title}], source: capture | plex}`.
- `Evidence`: `{revision, final: yes/no, seams: [{at: Seconds, kinds}],
  logo: [{from, to}], logoKnown: yes/no, blocks: [{from, to, score,
  decision: commercial | programme}]}`.
- `Airing`, `Entry`: see D2.
- `Mark`: `{start, end: Seconds, kind, state, basis}`; `start < end`.
- `MarksFile`: `{captureId, evidenceRevision, windows: [{from, to}], marks}`.

### PART: Coder

- Kind: boundary (Chromaprint).
- Owns: nothing.
- Never does: matching, storing, timing beyond its run.
- Offers:
  - `op code(Run) -> CodeRun`
    - Requires: audio at the rate and channel count the Coder states.
    - Ensures: the same codes `fpcalc -raw` gives for the same audio (within
      the first and last 3 s of a run, codes may differ); the CodeRun's start
      is the run's start.
    - Order: repeatable; same input gives the same output.
  - `op codeStreaming(Run growing) -> codes as they become available`
    - Ensures: each code appears within 1 s of the audio it covers arriving;
      the codes equal what `code` gives on the whole run.
- Platform facts to check first (gate 4): whether the Chromaprint build streams
  codes without restarting its context; code period at the chosen rate; codes
  identical to fpcalc 1.5.1 on g5.

| Given | When | Then |
| --- | --- | --- |
| g5 audio as one run at 0 | code | codes equal fpcalc's raw codes on g5 |
| a 2 s run | code | a short or empty code list, no error |
| g5 audio fed in 1 s pieces | codeStreaming | the same codes as `code`, each within 1 s |
| a run starting at 1234.5 | code | CodeRun start 1234.5 |

### PART: Library

- Kind: core.
- Owns: the store; entry status; promotion and expiry decisions.
- Never does: decide marks; read Comskip's raw output; know proxy seconds.
- Offers:
  - `op addStream(CaptureId, Context, CodeRun...) -> ok | StoreUnavailable | StoreCorrupt | StoreBusy`
    - Ensures: the stream is stored; appending more runs for the same capture
      extends it; storing the same run twice changes nothing.
  - `op findRepeats(CaptureId, Evidence or none) -> {airings, promoted entries, corrected captures}`
    - Requires: the capture's stream is stored.
    - Ensures: every repeat of at least 3 s between this stream and itself
      (copies at least 20 s apart) or any stored stream becomes an airing of an
      entry, merging with an existing entry when their codes agree.
      Promotion rules, applied to all of an entry's airings:
      - `ad` when it aired in at least two different series or channels, no
        airing is in a stretch the labels (Harness only) or a seen logo call
        programme, and its length is at least 3 s; or when it repeats within one
        capture, is bounded by seams within 1 s at both ends in at least one
        airing, has no logo seen in any airing, and has a standard length.
      - `in-programme ad` when it aired in at least two different series or
        channels and a logo was seen during at least half of it in every airing.
      - `programme` when every airing is in one series and a logo was seen in
        it, or labels call it programme. A `programme` entry is never promoted.
      - otherwise `candidate`.
      Comskip's block decisions are never used for promotion.
    - Ensures: corrected captures are those, other than this one, that hold an
      airing of an entry whose status changed in this call.
    - Effects: entries and airings change; the corrections outbox fills.
  - `op liveIndex() -> entries with status ad or in-programme ad, with codes`
    - Ensures: at most 20 MB; at most the most recently seen entries if more.
  - `op expire(window, existing CaptureIds) -> counts removed`
    - Ensures: streams older than the window or not in the existing list are
      removed with their airings; entries with no airing left are removed.
- Invariants: an entry with status `programme` is never `ad`; every airing
  belongs to a stored stream; an `ad` entry has airings in two series or
  channels, or meets the within-capture rule.

| Given | When | Then |
| --- | --- | --- |
| empty store; g5 stream with Evidence | findRepeats | airings for g5's repeats; the 30 s ads with seams at both ends and no logo become `ad`; the 2.5 s CBS-style sting stays `candidate` (not a standard length) |
| g5 stored; g4 added | findRepeats on g4 | entries seen in both games become `ad` (two series); g5 is in corrected captures |
| a 31 s repeat across two series with logo seen throughout | findRepeats | `in-programme ad` |
| a repeat within one series with logo seen | findRepeats | `programme`; later seen on another channel without logo: stays `programme`, logged |
| repeat with no Evidence (Plex recording) | findRepeats | stored as `candidate` unless the cross-series rule holds |
| same stream added twice | addStream | no change |
| store file is locked by another writer for 2 s | addStream | waits and succeeds; StoreBusy only after the job's limit, and the caller retries |
| store file unreadable | any op | StoreCorrupt |
| a stream 9 weeks old, window 8 weeks | expire | removed with its airings |

### PART: LiveMatcher

- Kind: core.
- Owns: the capture's in-memory codes and its live matches.
- Offers:
  - `op feed(codes) -> new matches`
    - Ensures: a match is reported once at least 3 s of codes agree with one
      live index entry, or with the capture's own codes at least 20 s earlier,
      at one offset; it carries the entry's whole extent placed by that offset
      (start and end may lie in the future). A self-repeat match is a
      candidate only: it never becomes a mark unless the Combiner's
      within-capture conditions hold.
    - Ensures: an entry already matched at an overlapping position is not
      reported again.
  - `op refresh(live index)`: later matches use the new index.

| Given | When | Then |
| --- | --- | --- |
| index holds a 30 s ad; capture codes reach 4 s into it at 600 s | feed | match 600-630 at once |
| 2 s agree, then 2 s noise | feed | no match |
| the same ad later at 1800 s | feed | a second match 1800-1830 |
| a concurrent capture promotes an ad | refresh, then feed | matches on that ad from now on |
| run boundary in the middle of an ad | feed | codes before and after the boundary are never joined into one match |

### PART: Combiner

- Kind: core. Pure: same inputs, same marks.
- Offers:
  - `op combine(matches, Evidence, now or final) -> Mark list`
    - Ensures, in this order:
      1. Each `ad` match becomes an `ad` stretch; each `in-programme ad` match an
         `in-programme ad` mark. A match edge moves to a seam within 1 s.
      2. Two `ad` stretches less than 90 s apart are joined into one break when
         no logo interval of 5 s or more lies between them. When the logo is
         not known for the capture, only gaps up to 30 s are joined
         [ASSUMED; measured in step F4].
      3. A break's start extends back over Comskip commercial blocks that touch
         it, and its end extends forward the same way, to those blocks' seams.
      4. A break closes at the first logo interval of 5 s or more, or Comskip
         programme block, after its last match or extension.
      5. Where no match lies, Comskip's commercial blocks become `ad` marks with
         basis `comskip`, as today.
      6. Marks of kind `ad` never overlap; an `in-programme ad` inside an `ad`
         mark is dropped.
    - State: in final mode every mark is `settled`. In live mode an `ad` mark
      from a library match is `settled` once its end is before `now`; anything
      made by joining or extension, and every basis-`comskip` mark, is
      `provisional` until 3 minutes after its end with no change (roadmap Q9).
    - Errors: `EvidenceMissing` when no Evidence was given; marks are then the
      matches alone, all `provisional`, and FingerprintRunner does not apply
      them (only logs) in stages before F-primary.

| Given | When | Then |
| --- | --- | --- |
| matches 860-890 and 920-950, no logo between, seam at 859.6 | combine final | one ad mark 859.6-950 |
| matches 860-890 and 1000-1030 | combine | two breaks (gap 110 s) unless Comskip blocks join them |
| match 860-890, Comskip commercial block 830-860 | combine | ad 830-890 |
| match 860-890, logo from 892 for 60 s | combine | break closes at 890 (or seam within 1 s) |
| no matches | combine | exactly Comskip's commercial blocks, basis comskip |
| in-programme ad match 1478-1509 | combine | one `in-programme ad` mark, not joined into any break |
| live, now 900, match 860-890 | combine live | ad 860-890 settled |
| live, now 900, joined 860-950 | combine live | the joined part provisional |
| no Evidence | combine | EvidenceMissing; marks provisional, not applied |

### PART: FingerprintTool

- Kind: core (wiring).
- Offers (as invocations by the TV app):
  - `op pass(store, CaptureId, Context, audio runs, Evidence path or none, output dir) -> result | named error`
    - Ensures: Library.addStream, findRepeats, then Combiner for this capture
      and each corrected capture whose latest Evidence the app has placed in
      the output dir; writes one MarksFile per such capture and a result
      listing them, each written whole; exit 0. A capture whose Evidence is
      missing is listed as `needsEvidence` with no MarksFile.
    - Errors: `StoreUnavailable`, `StoreCorrupt`, `InputInvalid` (exit codes
      distinct per error, message on stderr).
    - Repeatable: running the same pass twice gives the same files.
  - `op live(store, CaptureId, Context, audio runs growing, Evidence path watched, marks path) -> exits at end of audio`
    - Ensures: codes appended to the store at least every 10 s; the marks file
      rewritten whole within 1 s of any change; never blocks its input reader
      for more than 1 s (it drops and counts instead).
  - `op seed(store, recording path or audio runs, Context) -> result`: a pass
    without Evidence and without marks output.
  - `op expire(store, window, existing captures list)`.
- Never does: write dvr.sqlite, capture directories other than the given
  output dir, or anything the app reads half-written.

### PART: EvidenceExport (Comskip, changed)

- Offers: an ini key turns on an evidence file next to the EDL.
  - Ensures: lists seams (time and kinds), logo intervals, whether a logo was
    learned, and final blocks with score and decision, in proxy (input)
    seconds; written whole; in live mode rewritten whenever the EDL is; a
    revision number that grows with each rewrite; the EDL and every other output
    byte-identical to a run without the key.
- [ASSUMED: no existing Comskip output already has all of this; the
  implementer checks the existing outputs first and reuses one if it does.]

| Given | When | Then |
| --- | --- | --- |
| g5 file, key on | run | evidence file with seams, logo intervals, blocks; EDL identical to key off |
| live run | each EDL rewrite | evidence revision increases; never seen half-written |

### PART: AudioFeeder (TV app, boundary)

- Ensures: mono audio of the main audio track (the one HLS uses, `0:a:0`),
  taken from the AC-4-capable path (the ffmpeg-ac4 companion's transcoded
  audio on AC-4 captures, the normal decode otherwise), in runs stamped in
  capture seconds (D4).
- Post-recording: from the saved audio rendition of the capture's segments,
  one run per stretch of consecutive segments.
- Live, before T6: an extra audio-only output of the encoder generation that
  already decodes the audio, ending when that generation ends. After T6: the
  same audio the app pipes into Comskip.
- Ensures: the encoder never waits on the tool; a full queue drops audio and
  counts it, and the next audio starts a new run.
- Errors: `NoAudio` (capture has no decodable audio; fingerprinting skips it
  and logs it).

### PART: Timeline (TV app)

- `op toCapture(proxy seconds, capture) -> Seconds | AxisUntrusted`
  - Ensures: correct across encoder generations once the SERVER_HARDENING fix
    is in; before that, `AxisUntrusted` for a capture with more than one
    generation that wrote to the proxy.

### PART: FingerprintRunner (TV app)

- Ensures: at most one pass or seed job at a time; live jobs one per capture;
  every job at the lowest priority and inside the CPU budget; backfill only
  when no capture, Comskip run or pass is running, and paused when one starts;
  the setting `off` stops all jobs and leaves marks to Comskip.
- Ensures: after a pass, applies each MarksFile through MarkStore, writes
  mapped Evidence for captures listed `needsEvidence` if it has it, and reruns
  the pass for them once.
- Errors handled: tool missing, non-zero exit, timeout (pass: 10 minutes per
  recorded hour; live: no output for 60 s while audio flows) -> log, keep marks,
  retry once (pass) or restart up to 3 times (live).

### PART: MarkStore (TV app, changed)

- `op apply(CaptureId, MarksFile) -> changed | unchanged | stale`
  - Ensures: inside the file's windows, marks are replaced with re-detection's
    rules; outside, untouched; a file whose Evidence revision is older than one
    already applied is `stale` and ignored; viewer exclusions carried to
    derived marks; a correction never removes a basis-`comskip` mark.
  - Ensures: previous marks of a corrected past capture kept 14 days.
  - Emits: the existing marks change notification (SSE).

### PART: MarkClients (TV app)

- Ensures: `ad` marks look and skip as today; auto-skip skips only `settled`
  `ad` marks; `in-programme ad` marks are shown as a distinct shading with a
  Skip button but never auto-skipped; a client that predates kinds never
  receives a non-`ad` mark as a skippable mark (Q4).

## Steps

Each step is one or a few commits with its own check. "F" steps are in this
repo, "T" steps in the TV app.

- **F1. Labels and scoring for kinds.** Move `tools/fingerprint/repeats.py`
  into `bin/perf` (as the roadmap said). Add label kind `in-programme ad`
  (scored as neither, like `sponsor`), and label the g5 Google split-screen
  and similar rows. `score_labels.py` scores only `ad` marks as cuts and reports
  `in-programme ad` marks separately. Check: the README's Comskip-107 table is
  reproduced unchanged.
- **F2. EvidenceExport in Comskip.** Check: file and live runs on the labelled
  set and `bin/perf` fixtures give identical EDLs with the key on and off; the
  evidence file is whole at every rewrite.
- **F3. Coder and FingerprintTool `pass` with codes only.** Check: codes equal
  fpcalc 1.5.1 on g5 and g4; CPU about 9 s per 90 min in the test container.
- **F4. Library and Combiner, offline Harness.** Harness: replay a labelled
  recording against a library seeded with a chosen set of recordings, Comskip
  Evidence from the same file (one generation, so proxy seconds equal capture
  seconds). Check: with an empty library, g5 and g4 within-game repeats give
  the coverage measured on October 27 (about 15% of ad seconds) and
  Must-not-get-worse 1 holds on all four; with g4 then g5, the corrections
  appear.
- **F5. Live mode in FingerprintTool.** Harness live mode feeds audio at real
  speed. Check: each matched ad marked within 6 s of its labelled start;
  stalled reader test drops and counts instead of blocking; memory under 32 MB.
- **T1. Mark kind and state fields, old clients safe.** MarkStore and the marks
  API carry `kind`, `state`, `basis`; clients ignore or show them per
  MarkClients. No fingerprint yet. Check: existing marks unchanged; a test
  `in-programme ad` mark is not skipped by the current web and Android builds.
- **T2. Generation fix for Timeline** (from SERVER_HARDENING; may already be
  planned there). Check: a real-Comskip test across a generation seam gives
  correct capture seconds.
- **T3. Post-recording pass, shadow then on.** FingerprintRunner, AudioFeeder
  (saved rendition), Evidence handoff. First shadow: results logged and
  compared with Comskip's marks, not applied. Then on, behind a setting. Check:
  NAS CPU and memory within budget; for a week of recordings, programme seconds
  added by fingerprint marks reviewed on a sample; off-or-broken tests
  (Must-not-get-worse 4).
- **T4. Backfill from existing recordings.** Seed jobs for the app's own
  captures and the Plex recordings library (path: Q1), oldest first within the
  expiry window (flow step 21). Cost: about 9 CPU s per 90 min of audio including decode, so
  100 recorded hours is about 10 CPU minutes, plus library search time to be
  measured. Run at the lowest CPU and disk priority, one job at a time, only
  when nothing is recording or detecting, with a pause of at least as long as
  each job's run time between jobs (rate limit). Check: library size and entry
  counts logged; Harness rerun on the labelled set with the seeded library
  (measurement plan).
- **T5. Live fingerprinting.** AudioFeeder's encoder audio output per
  generation, FingerprintTool `live`, live Evidence handoff, provisional and
  settled marks. Check: Must-not-get-worse 2 and 3 on the NAS with the maximum
  number of concurrent captures seen in the jobs table; live marks appear while
  recording.
- **T6. Pipe frames and audio into Comskip** (target input path). Replaces the
  commercials.ts proxy. Depends on roadmap stages 1-2: B126 (the logo rewind
  fails on a raw-frame pipe) and B127 (pipe and file results differ) fixed in
  Comskip. The pipe carries one continuous clock across encoder generations.
  AudioFeeder then takes live audio from the same piped audio. Check: the
  roadmap stage 2 checks, plus fingerprint live marks unchanged against T5 on
  replayed captures. The post-recording pass (T3) does not depend on this step.
- **F6/T7. Fingerprints primary.** Combiner rules as above become the default
  in the TV app's settings; Comskip's own blocks only decide unmatched areas.
  Check: Must-not-get-worse 1 on all labelled recordings (including the two
  kept aside) with the seeded library, and better coverage than Comskip-107 on
  at least two.

## Measurement plan

For each labelled recording, with libraries of growing size:

- Library sets: empty (within-recording only); the other labelled recordings;
  N backfilled recordings that aired *before* the labelled one, for N = 5, 10,
  20, 50 (never later recordings, so results don't use the future).
- Report: breaks found, false breaks, ad seconds missed, programme seconds cut
  (sponsor and in-programme ad rows excluded), share of ad seconds covered by
  matches alone, count of `in-programme ad` marks and how many fall on labelled
  rows. Compare with Comskip-107.
- Also count entries per status in each library, and how many `programme`
  entries were found (guards working).
- Live: time from labelled ad start to mark, and how often a provisional mark
  later moved by more than 5 s or vanished.

## Gates (run once, October 27)

1. Cross-reference: every part in the flow is in Parts; every named error
   (`StoreUnavailable`, `StoreCorrupt`, `StoreBusy`, `InputInvalid`,
   `EvidenceMissing`, `AxisUntrusted`, `NoAudio`) is produced by one part and
   handled in the flow or FingerprintRunner.
2. Walk-through: g5 post-recording with an empty library; a two-generation
   capture before T2 (codes stored, no marks); a live capture whose tool dies
   twice. Each runs on the document alone.
3. Goal trace: every sentence of the goal maps to a Library rule, a Combiner
   rule, a flow step or a must-not-get-worse item.
4. Platform facts not yet checked first-hand: Chromaprint streaming without a
   context restart (Coder); the encoder's extra audio output cost and the
   AC-4 companion's audio being available to it (AudioFeeder); store
   concurrency with several writers (Library); library search memory on the
   N100. Each is the first check of its step.

No independent review has been done yet.

## Open questions (recommended answers)

1. **Plex recordings library path on the NAS?** Unknown. Recommended: the user
   gives the path; T4 reads it read-only and stores no copy of the media.
2. **Store location?** Recommended: a separate store file next to dvr.sqlite,
   written only by FingerprintTool (D2). Changes roadmap Q10.
3. **Settling for live fingerprint matches?** Recommended: a library-match `ad`
   mark is settled as soon as its whole extent has aired; joins, extensions and
   Comskip-only marks settle 3 minutes after their end (Q9's rule). Auto-skip
   uses settled marks only.
4. **How do old clients avoid skipping in-programme ads?** Recommended: the
   marks API keeps today's mark list for `ad` marks only and sends other kinds
   in a separate list that old clients ignore.
5. **May a correction remove a Comskip mark?** Recommended: no; corrections
   add or widen, and remove only fingerprint-based marks. Revisit once
   fingerprints are primary.
6. **Sponsor kind?** Recommended: reserve it; v1 emits only `ad` and
   `in-programme ad`. Decide after F4 shows how sponsor billboards repeat.
7. **Expiry window?** Recommended: 8 weeks for code streams, entries removed
   when their last airing leaves the window.
8. **Who maps Comskip's Evidence to capture seconds?** Recommended: the TV app
   (Timeline), so proxy seconds stay inside the app and FingerprintTool sees
   only capture seconds.
9. **Gap joining when no logo was learned?** Recommended: only gaps up to 30 s,
   until F4 measures more.
10. **Patents:** user accepts the risk for a free repo; keep the stage off by
    default in the public build and read the claims before announcing it.

## Assumptions

- [ASSUMED: about 40 recorded hours a week; the jobs table will tell.]
- [ASSUMED: at most 4 concurrent live captures (4 tuners).]
- [ASSUMED: the saved audio rendition of a capture is kept at least until its
  post-recording pass has run.]
- [ASSUMED: the Plex recordings have readable audio that the AC-4-capable path
  can decode.]
- [ASSUMED: no existing Comskip output already gives EvidenceExport's data.]
