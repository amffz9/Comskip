# Plan: audio-fingerprint ad detection

Status: Stage A outline plus part contracts, revised October 27, 2026 after one
independent review and three user decisions (U1-U3). Nothing is implemented.
The outline still needs the user's approval. This plan covers roadmap stage 7
(`docs/detection-session-roadmap.md`). It spans two repositories:

- **This repo** (Comskip fork, branch `fingerprinting`): the fingerprint tool,
  Comskip's evidence export, the offline harness, labels and scoring.
- **The TV app** (`F:\Development\TV`): feeding audio, running the tool,
  storing and showing marks. The app's agent implements those steps. This plan
  says only what the app must observably do, plus the cross-repo formats.

Every step in "Steps" is small, can be committed on its own, and has a check.
The implementer commits after each one.

## Goal (user's words, condensed)

Fingerprinting becomes the main way breaks are found over time, combined with
today's detection. A match is strong evidence. Where nothing matches, Comskip's
evidence decides. Matches less than about 90 s apart inside one break are
joined. Break edges move to the nearest seams, and a break closes on programme
evidence. Every whole recording's fingerprints are kept, not only known ads,
and the store never expires (seasonal ads return). Ads show up as repeats:
across recordings or channels, or inside one recording when the stretch is
bounded by seams, has no logo and has a standard ad length. A later repeat
corrects the marks of earlier recordings. Recordings made at the same time
share national ads. The library is seeded quickly from recordings already on
the NAS. Live: codes arrive about 8 per second, 3-5 s aligned with a stored ad
is a match, and the stored copy gives the ad's extent. Marks are provisional
or settled. Feedback loops are guarded against: nothing that labels or the
logo say is programme is added, and an ad needs recurrence.

## Glossary

- **Chromaprint**: fixed by the measurements and the user. The audio
  fingerprint library behind `fpcalc`. Named only in the Coder boundary.
- **SQLite**: fixed by the TV app's conventions and roadmap Q10. The store is
  one SQLite file. Named only in the Library and FingerprintTool blocks.
- **Code**: one 32-bit Chromaprint value. Two codes *agree* when at most 10 of
  their bits differ.
- **Code period**: exactly 1365/11025 s (0.123810 s), Chromaprint's hop at
  11025 Hz. The October 27 figure of 0.1239 s was duration divided by code
  count and drifts about 3.9 s over 90 min; it must not be used.
- **Capture**: the TV app's record of one channel being received. A
  *recording* is a window over a capture. Marks belong to the capture.
- **Capture seconds**: the TV app's time axis for a capture
  (`segment.start`, `recording.start` and `commercialMarks[].start/end` are all
  in capture seconds; checked in `dvrMarks.js`). The player's axis is derived
  from it per session by `mapMarksToMedia`. Everything outside Comskip uses
  capture seconds.
- **Proxy seconds**: Comskip's time axis on its input. Today it restarts near 0
  at each encoder generation (SERVER_HARDENING). Only the TV app's Timeline
  part converts proxy seconds to capture seconds.
- **Run**: a stretch of audio with no gap, with the capture second its first
  sample belongs to. Nothing is matched across the boundary between two runs.
- **Code stream**: all codes of one recording, as a list of runs.
- **Seam**: a point where Comskip sees a possible break edge: black frame,
  silence, scene change or aspect change.
- **Logo interval**: a stretch where Comskip sees a logo. A logo is whatever
  that recording's Comskip run learned. It may be programme-specific (NBC uses
  a Sunday Night Football logo, U3).
- **Programme key**: channel plus guide title. Any logo state remembered from
  one recording to another is keyed by programme key, never by channel alone
  (U3). This plan remembers none; Comskip learns per recording.
- **Series key**: what counts as "the same show" for the Library's rules. For
  sports it is league plus network (all CBS NFL games share one key; NBC SNF
  has another). Otherwise it is the guide's series id, or the guide title.
- **Comskip block**: one of Comskip's final blocks, with its score and its
  decision (commercial or programme).
- **Evidence**: seams, logo intervals and Comskip blocks for one capture, in
  capture seconds (format below).
- **Repeat**: two stretches of at least 3 s whose codes agree at one offset
  (the experiment's rule: exact-value seeds, extended only while codes agree,
  at least 3 s long, the copies at least 20 s apart).
- **Standard length**: 10, 15, 20, 30, 45, 60, 90 or 120 s, within 1 s.
- **Library entry**: a stretch of audio that repeats, kept as a unit, with a
  status: `candidate`, `ad`, `in-programme ad`, or `programme` (a known
  programme repeat, such as a sting or bumper, kept so it is never promoted).
- **Airing**: one place in one recording where an entry occurs.
- **Match**: an airing found while processing a code stream. Its *agreed
  part* is where the codes have actually agreed; its *projected part* is the
  rest of the entry's extent, not yet confirmed.
- **Live index**: the entries with status `ad` or `in-programme ad` whose last
  airing is within the last 8 weeks (U2).
- **Mark kind**: `ad` (part of a break; skippable), `in-programme ad` (ad audio
  over programme, such as the 31 s Google split-screen in g5; shown, never
  auto-skipped), `sponsor` (reserved, Q6).
- **Mark state**: `provisional` (may still move or vanish) or `settled`.
- **Labelled set**: every recording in `tests/labels` with a `.labels.csv`.
  Today four football games (g5, g4, faaea10a, 77483aca); step L1 adds two
  dramas.
- **Comskip-107**: Comskip alone with the TV ini and `detect_method=107`, the
  baseline (g5 9/10 breaks, g4 13/13, faaea10a 6/12, 77483aca 24/24).

## Parts

This repo:

- **Coder**: turns a run of audio into codes. Boundary part around Chromaprint.
- **Library**: the store of code streams, entries and airings; finds repeats
  and promotes entries. Sole owner of the store.
- **LiveMatcher**: matches a growing code stream against the live index and
  against the recording's own earlier codes.
- **Combiner**: turns matches plus Evidence into marks with kind and state.
- **FingerprintTool**: the executable the TV app runs; wires Coder, Library,
  LiveMatcher and Combiner for one job and writes outputs whole.
- **EvidenceExport**: Comskip's new output of seams, logo intervals and blocks.
- **Harness**: offline replay of labelled recordings with a chosen library,
  and scoring. Never used in production.

TV app:

- **AudioFeeder**: delivers runs of mono audio, stamped in capture seconds,
  from the AC-4-capable audio path. Boundary part around FFmpeg, the encoders
  and the ffmpeg-ac4 companion.
- **Timeline**: converts proxy (or pipe) seconds to capture seconds. Exists as
  `mapProxyTime`; must be made correct across generations.
- **FingerprintRunner**: starts, feeds and watches FingerprintTool jobs, keeps
  each capture's Evidence copy, and queues passes and backfill under the NAS
  budgets.
- **MarkStore**: the single owner of what a client sees as a capture's marks.
  Keeps Comskip's list and the fingerprint list apart and merges them on read.
- **MarkClients**: the marks API and the web and Android players.

## Tree

```
FingerprintRunner: calls AudioFeeder, Timeline, FingerprintTool, MarkStore
FingerprintTool:   calls Coder, Library, LiveMatcher, Combiner
LiveMatcher:       calls Library (read)
Combiner:          calls nothing (pure)
Harness:           calls FingerprintTool, EvidenceExport (via Comskip), score_labels
MarkClients:       read MarkStore through the marks API
```

## Existing parts (checked in source)

- Comskip detection and EDL: unchanged. EvidenceExport is added beside the EDL
  and is off unless asked for. Comskip already keeps a per-frame array
  (brightness, scene change, logo, sound, black, cut scene, aspect, time) that
  `output_framearray` prints, and the `.log` lists blocks. EvidenceExport is
  built from that same data, not from a new measurement.
- `ComskipDetector.#reconcile` (`dvrCommercials.js`, about lines 630-713): it
  rebuilds `capture.commercialMarks` from the EDL. Live polls match existing
  marks by `proxyStart` and keep unmatched ones; the final parse drops every
  mark not in the EDL. So a fingerprint mark in the same array would be kept
  beside an overlapping EDL block while live and dropped at the final parse.
  **Changed by this plan only in what it owns**: `commercialMarks` becomes
  Comskip's list alone (it already is today). Its logic stays as it is.
  After its final parse it tells FingerprintRunner, which saves the Evidence
  copy.
- Re-detection (`dvrRedetect.js`, about lines 450-530): replaces every mark in
  its window, with ids kept by overlap of at least half the shorter mark and
  viewer exclusions carried to derived marks. **Unchanged in logic**; it also
  writes only Comskip's list. After it publishes, FingerprintRunner reruns the
  Combiner for that capture with the new Evidence (its run must export
  Evidence too).
- `DvrService.publishMarks` (`dvr.js` about line 643): copies
  `commercialMarks` between the held copies of a capture. Changed: it copies
  the fingerprint list as well.
- `marksForWindow` (`dvrMarks.js`): changed. It reads MarkStore's merged view
  instead of `commercialMarks`, passes `kind` and `state`, and returns non-`ad`
  marks separately (Q4: web and Android skip every complete mark).
- `mapProxyTime`/`generationFor`: changed, must be correct across encoder
  generations (SERVER_HARDENING fix). Needed before combination on
  multi-generation captures.
- Retention (`dvrRetention.reclaimCommercialProxy`): deletes the proxy and
  every file named `<proxy base>.*` once detection is terminal, which can be
  before a queued pass runs. Unchanged; the Evidence copy (below) lives outside
  that name pattern.
- Recording, encoders, HLS, the proxy file: unchanged until step T6 (pipe).

## Out of scope

- Deleting or cutting video. Marks only drive skipping.
- Intro and credits markers for series (a later use of the same Library).
- Visual fingerprints.
- The windowed live mode of roadmap stage 5. LiveMatcher uses Comskip's live
  Evidence as it is.
- Publishing this stage in the public fork before the patent question is
  settled (roadmap stage 7). The user accepts the risk for a free repo.

## Must not get worse

1. **Programme cut.** On every labelled recording, the `ad` marks cut no more
   programme seconds than Comskip-107, find no fewer breaks and add no more
   false breaks. Label rows of kind `sponsor` or `in-programme ad` count as
   neither ad nor programme. Checked by the Harness at every step that changes
   marks, with an empty library and with each seeded library.
2. **Live never blocks recording.** If FingerprintTool is slow, stalled,
   crashed or missing, the recording, the HLS output and Comskip carry on as
   today. Audio the tool can't take is dropped and counted, never queued
   without limit. Checked by a test that stalls the tool for 60 s and one that
   kills it.
3. **NAS budget.**
   - Per live recording: at most 2% of one N100 core and 48 MB of memory for
     FingerprintTool (live index up to 20 MB, the recording's own codes about
     350 KB for 3 h, the rest headroom).
   - Post-recording pass: lowest CPU and I/O priority; one repeat search at a
     time; at most 512 MB peak memory.
   - Backfill (U1): up to 4 coding jobs at once when no capture is in progress,
     1 while any capture is in progress; repeat searches still one at a time.
   - Encoder CPU with the added audio output: no more than 1% above today.
   - Store growth: about 100 KB per recorded hour, never expired (U2). About
     26 MB after the seed, then about 4 MB per week at an assumed 40 recorded
     hours a week. FingerprintRunner logs the size and warns above 1 GB.
   - Measured on the NAS at steps T3, T4 and T5.
4. **Off or broken means today.** With fingerprinting disabled, the tool
   missing, the store missing or unreadable, or a job failing, every client
   sees exactly the marks today's Comskip path gives. Checked by turning each
   of those on in a test capture and comparing the marks API output.
5. **Old clients.** A web or Android build that predates mark kinds never
   skips an `in-programme ad` or `sponsor` mark, and sees `ad` marks exactly as
   today.
6. **Viewer judgements.** A mark a viewer removed for a recording stays removed
   after any fingerprint result or correction.
7. **No laundering.** Comskip's decisions never make an entry an `ad`. Only
   recurrence does (Library rules).
8. **Not only football.** Before step T3 is switched on, the labelled set
   includes the two dramas from L1, and rule 1 holds on them.

## Flow

Post-recording (ships first; needs neither live tapping nor the pipe):

1. A capture's Comskip run finishes its final parse. ComskipDetector tells
   FingerprintRunner.
2. FingerprintRunner maps Comskip's evidence file through Timeline and saves
   the mapped copy as the capture's *Evidence copy*, a file in the capture
   directory outside the proxy's name pattern, kept for the capture's life and
   deleted with it. This happens before retention can delete the proxy's
   files. If Timeline reports `AxisUntrusted`, no Evidence copy is saved.
3. FingerprintRunner queues a pass for the capture.
4. AudioFeeder supplies the capture's audio as runs, from the saved audio
   rendition (the AC-4-capable path's output).
5. FingerprintTool.pass: Coder turns each run into codes. Library stores the
   code stream with its context (channel, programme windows with title and
   series key, air time).
6. Library finds repeats of this stream against itself and the whole store,
   updates entries and airings, and promotes entries by its rules.
7. Library lists every other capture whose marks change because an entry
   changed status (corrections). Plex recordings have no marks and are never
   listed.
8. Combiner makes marks for this capture and for each corrected capture that
   has an Evidence copy. A capture without one (`AxisUntrusted`, or Evidence
   missing) gets no fingerprint marks; its Comskip marks stand.
9. FingerprintTool writes one MarksFile per such capture and a result listing
   them, each written whole, and exits 0.
10. FingerprintRunner hands each MarksFile to MarkStore, which replaces that
    capture's fingerprint list and notifies clients.
11. On a non-zero exit or a timeout FingerprintRunner logs it, leaves marks as
    they are, and retries the pass once later. On `StoreCorrupt` it moves the
    store aside, starts a new one, and queues backfill again.

Live (after the post-recording pass is in use):

12. When a capture starts, FingerprintRunner starts FingerprintTool.live for it
    and connects AudioFeeder's live audio. Until T6 that is an extra audio-only
    output of the encoder process that already decodes the broadcast audio
    (on AC-4 captures, the companion's transcoded audio). After T6 it is the
    audio the app pipes into Comskip.
13. A new encoder generation starts a new run at that generation's capture
    second. Audio the tool can't take is dropped and counted, and the next
    audio starts a new run.
14. FingerprintTool.live loads the live index and refreshes it every 10 s, so
    entries promoted by concurrent captures arrive within seconds.
15. LiveMatcher reports a match when 3-5 s of codes agree with one entry. The
    agreed part grows while the codes keep agreeing; the projected part covers
    the rest of the entry's extent.
16. Whenever the EDL poll sees Comskip's evidence change, FingerprintRunner
    maps it and rewrites the live Evidence file whole (by replacing it); the
    tool rereads it when its revision grows.
17. Combiner turns live matches plus live Evidence into marks; FingerprintTool
    rewrites the live MarksFile whole; FingerprintRunner hands it to MarkStore.
18. LiveMatcher appends the recording's codes to the store at least every
    10 s, so concurrent captures can find shared ads.
19. When audio ends, the tool writes its last live MarksFile and exits. Steps
    1-11 follow; the pass result is settled and replaces the live result.
20. If the live tool dies, FingerprintRunner restarts it at most 3 times per
    capture, each time as a new run. After that the capture has no live
    fingerprint marks; Comskip's live marks continue and the pass still runs.

Backfill (seeding, U1):

21. FingerprintRunner lists candidate recordings: the app's own captures, and
    the files under the Plex root (`/volume1/Shared/plex/Shows`, read-only).
    It skips folders on the exclusion list (default: `Stargate SG-1`,
    `Stargate Atlantis`, `Stargate Universe`, `Battlestar Galactica`; a
    setting) and files whose duration doesn't look like a broadcast slot (not
    within 3 min of a multiple of 30 min, and not at least 2.5 h). It skips
    files already in the store with the same size and modification time.
22. Each recording gets a seed job: code it and store the stream (Coder runs
    in parallel, up to 4 jobs with no capture in progress, 1 while any capture
    is in progress, at the lowest CPU and I/O priority). Repeat searches run
    one at a time, oldest recording first. Seed jobs make no marks, except
    corrections to the app's own captures that have an Evidence copy.

Live index aging (U2):

23. The store keeps everything. An entry leaves the live index when its last
    airing is more than 8 weeks old. When a pass finds an airing of an older
    entry, its last airing moves forward and it is back in the live index at
    the next refresh.

## End-to-end check

Seed a library with every available recording that aired before g5, except
the labelled ones. Run the pass on g5, then on g4. Expected: g4 gains coverage
from ads first seen in g5, g5's marks are corrected by repeats first seen in
g4, and both meet Must-not-get-worse 1 against Comskip-107. Then replay g4
through the live path at real speed (Harness live mode) with that library and
check that each matched ad is marked within 6 s of its labelled start, and
that no projected part outlives a divergence by more than 2 s. The two
kept-aside labelled recordings (roadmap stage 4) are scored only at the end,
by someone other than the implementer.

## Irreversible steps

- **Deploy to the NAS** (T3, T4, T5, T6): each behind a setting that defaults
  off; the user switches it on. Off restores today's behaviour because
  Comskip's list was never changed.
- **Corrections rewriting past captures' fingerprint lists**: MarkStore keeps
  the previous fingerprint list of every corrected capture for 14 days, so one
  action can restore it.
- No migration in dvr.sqlite: the fingerprint list and the new fields live in
  the capture's `metadata_json`.

## Decisions

### D1. Where fingerprinting runs

| Option | Combining with seams/logo | Live latency | Shared index for concurrent captures | Crash isolation | AC-4 | Proxy timestamp bug | Offline testing on labels |
| --- | --- | --- | --- | --- | --- | --- | --- |
| (a) Inside Comskip | Direct | Good | Only through the store | A fingerprint fault ends detection | Only if Comskip's input carries the transcoded audio | Inherits it | Good |
| (b) Separate tool from this repo | Through EvidenceExport | Good | Through the store, refreshed every 10 s | Separate | Fed from the AC-4-capable path | Avoided | Good |
| (c) In the Node app | Through EvidenceExport | Good | In one process | Shares the server process or needs a worker | Fed from the AC-4-capable path | Avoided | Needs the TV repo; algorithm in a second language |

**Recommended: (b)**, confirmed by the review. A separate FingerprintTool,
built in this repo and linking Chromaprint, run by the TV app once per live
capture and once per pass or seed job. Comskip only adds EvidenceExport. The
pass, corrections and backfill are not per-recording detection work; a
fingerprint fault never costs today's marks; audio arrives stamped in capture
seconds; and the Harness tests the same executable on the labels. A single
long-running service holding one index is the later option if the 10 s
refresh proves too slow.

### D2. Storage

- (a) Tables in dvr.sqlite (roadmap Q10): one database, but a process outside
  the app writes it, long writes can stall the app, and the store grows its
  backups.
- (b) A separate store file next to dvr.sqlite, owned by FingerprintTool.

**Recommended: (b)**, confirmed by the review. This changes roadmap Q10.

Store contract:

- One SQLite file in write-ahead-log mode. Every connection waits for locks
  (busy timeout of at least 10 s) instead of failing. That is enough for up to
  4 live appenders, which write a few rows every 10 s, plus one repeat search,
  which writes in short transactions (one per entry update, never one for the
  whole pass). Backfill coding jobs append like live jobs.
- The store carries a schema version. A tool that finds a newer version than it
  knows exits with `StoreTooNew`; an older one it upgrades in place.
- Only FingerprintTool opens it. The TV app knows its path and nothing else.
- Concepts held (columns are the implementer's choice):
  - **Code stream**: recording id (capture id, or `plex:` plus the file path),
    channel (may be unknown for Plex), air start, programme windows
    (capture-second ranges with title and series key), runs (start, codes),
    source (`capture` or `plex`), file size and modification time (Plex).
  - **Entry**: id, status, length, a reference copy of its codes, first and
    last airing, airing count, distinct series keys and channels, why it has
    its status.
  - **Airing**: entry id, recording id, start and end, found by (`pass`,
    `live`, `seed`), seam within 1 s at start and at end, logo seen (yes, no,
    unknown), which part of the entry it covers.
- Nothing expires (U2).

### D3. Marks with kind and state reach the app; corrections

- **One owner.** MarkStore owns what clients see. It keeps two lists per
  capture: Comskip's (`commercialMarks`, written only by ComskipDetector and
  re-detection, as today) and the fingerprint list (written only from
  MarksFiles). The merged view is the fingerprint list where the capture has
  one, and Comskip's list where it doesn't. A MarksFile always covers the whole
  capture, so no partial windows are merged.
- The fingerprint list holds the Combiner's marks, including the Comskip
  blocks it kept (basis `comskip`), so no Comskip block is lost by using the
  fingerprint list (Combiner rule 5).
- A fingerprint mark keeps the id of the Comskip mark it overlaps by at least
  half of the shorter one, so viewer exclusions and Undo still match. A
  fingerprint mark matching none gets a new id; an excluded Comskip mark's
  exclusion is carried to each fingerprint mark it overlaps (re-detection's
  rule).
- Corrections are MarksFiles for other captures, applied the same way. A
  correction can never drop a basis-`comskip` mark, because the Combiner
  always keeps them.
- MarkClients: see the part.

### D4. Timeline

- Fingerprint time is capture seconds: AudioFeeder stamps each run with the
  capture second its first sample belongs to (a generation's `base` for live
  audio; the segment start for saved audio). Within a run, time is sample count
  divided by 11025, and code *i* starts at run start + *i* x code period.
- Comskip's Evidence is in proxy seconds; Timeline maps it. Until the
  SERVER_HARDENING fix lands, Timeline reports `AxisUntrusted` for a capture
  with more than one generation that wrote to the proxy.
- After T6 the app decides the pipe's clock and must feed Comskip one
  continuous clock across generations; Timeline maps that clock the same way.

### D5. Staging

See "Steps". Order: offline in this repo first, labels for dramas, then the
post-recording pass in the app, then backfill, then live, then the pipe, then
fingerprints primary. Backfill comes before live because live matching is
only useful once the library holds confirmed ads; the review agreed.

## Cross-repo formats

These are the contracts between the two repos. Times are capture seconds as
decimal numbers with at least millisecond precision.

**Audio stream** (TV app to FingerprintTool, on the tool's standard input):

- A sequence of messages. Each message is a 1-byte type, a 4-byte
  little-endian payload length, and the payload.
- `R` (run start): payload is the run's start in capture seconds, an 8-byte
  little-endian IEEE double. Every run begins with one. Audio before the first
  `R` is `InputInvalid`.
- `A` (audio): payload is samples, 16-bit little-endian signed, mono, 11025 Hz,
  an even number of bytes. They follow the samples before them in the same
  run.
- `E` (end): no payload; the recording's audio is complete. End of input
  without `E` means the audio stopped early (the tool still writes its outputs
  and exits 0, reporting `ended: false`).
- The app resamples and downmixes; the tool never decodes broadcast audio.

**Evidence file** (TV app to FingerprintTool; one JSON object, replaced whole):

```
{ "captureId": "...", "revision": 12, "final": false,
  "logoKnown": true,
  "seams": [ { "at": 859.6, "kinds": ["black", "silence"] } ],
  "logo": [ { "from": 0.0, "to": 118.2 } ],
  "blocks": [ { "from": 118.2, "to": 400.1, "score": 1.8,
                "decision": "commercial" } ] }
```

`kinds` are from `black`, `silence`, `scene`, `aspect`. `decision` is
`commercial` or `programme`. Comskip's own evidence output (EvidenceExport)
has the same fields in proxy seconds, without `captureId`; the app maps every
time and adds `captureId`.

**MarksFile** (FingerprintTool to TV app; one JSON object, replaced whole):

```
{ "captureId": "...", "evidenceRevision": 12, "final": true,
  "marks": [ { "start": 859.6, "end": 950.0, "kind": "ad",
               "state": "settled", "basis": "both" } ] }
```

`kind` is `ad` or `in-programme ad`; `state` is `provisional` or `settled`;
`basis` is `fingerprint`, `comskip` or `both`. Marks are sorted by start;
`ad` marks never overlap. A MarksFile whose `evidenceRevision` is lower than
one MarkStore already applied is ignored.

**Pass result** (FingerprintTool to TV app; JSON on standard output at exit):

```
{ "recording": "...", "ended": true, "codes": 43610,
  "marksFiles": [ { "captureId": "...", "path": "..." } ],
  "promoted": 4, "droppedAudioSeconds": 0.0 }
```

**Exit codes**: 0 done; 2 `InputInvalid`; 3 `StoreUnavailable` (can't open or
lock within the limit); 4 `StoreCorrupt`; 5 `StoreTooNew`; 6 any other
failure. A message goes to standard error in every non-zero case.

## Part contracts

Types used below:

- `RecordingId`: a capture id, or `plex:` plus a file path.
- `Seconds`: capture seconds, a non-negative real number.
- `Run`, `Evidence`, `MarksFile`: as in "Cross-repo formats".
- `CodeRun`: `{start: Seconds, codes}`; code *i* starts at
  `start + i x code period`.
- `Context`: `{channel or unknown, airStart, programmes: [{from, to: Seconds,
  title, seriesKey}], source: capture | plex}`.
- `Mark`: `{start, end: Seconds, kind, state, basis}`; `start < end`.

### PART: Coder

- Kind: boundary (Chromaprint).
- Input: 16-bit mono audio at 11025 Hz, already resampled by the caller.
  Chromaprint does no resampling of its own at that rate.
- Offers:
  - `op code(Run) -> CodeRun`
    - Ensures: at least 99% of codes are within 2 bits of
      `fpcalc -raw -length 0` on the same recording, once aligned (fpcalc
      resamples with its own resampler and defaults to the first 120 s, so
      exact equality is not expected). The CodeRun starts at the run's start.
    - Repeatable: the same input gives the same output.
  - `op codeStreaming(growing Run) -> codes as they become available`
    - Ensures: each code appears within 1 s of the audio it covers arriving,
      and the codes equal what `code` gives on the whole run, with no restart
      of Chromaprint's context inside a run.
- Platform facts, first check of F3: the reviewer reports that Chromaprint has
  streamed without a restart since 1.4 (taking the codes so far and clearing
  them) and that vcpkg ships 1.6.0. Not yet checked here.

| Given | When | Then |
| --- | --- | --- |
| g5 audio, 11025 Hz mono, one run at 0 | code | at least 99% of codes within 2 bits of fpcalc's (`-length 0`) |
| g5 audio fed in 1 s pieces | codeStreaming | same codes as `code`, each within 1 s |
| a 2 s run | code | a short or empty list, no error |
| a run starting at 1234.5 | code | CodeRun start 1234.5 |
| a repeat at 80 min in a recording | code, then Library | the repeat's times land within 0.2 s of the labelled time |

### PART: Library

- Kind: core.
- Owns: the store; entry status; promotion and live-index aging.
- Never does: decide marks; read Comskip's raw output; know proxy seconds;
  remember any logo state across recordings except keyed by programme key.
- Offers:
  - `op addStream(RecordingId, Context, CodeRun...) -> ok | StoreUnavailable | StoreCorrupt`
    - Ensures: the stream is stored; more runs for the same recording extend
      it; storing the same run twice changes nothing.
  - `op findRepeats(RecordingId, Evidence or none) -> {airings, changed entries, corrected captures}`
    - Requires: the recording's stream is stored.
    - Ensures: every repeat of at least 3 s between this stream and itself
      (copies at least 20 s apart) or any stored stream becomes an airing. A
      repeat extends only while its codes agree. When two airings of one entry
      agree on only part of it (a 15 s cut of a 30 s ad), the entry is split
      into the shared part and the rest, each its own entry.
    - Promotion rules, over all of an entry's airings, where "logo seen" comes
      from each airing's own recording:
      - `ad`, across shows: airings in at least two series keys or two
        channels; no airing has a logo seen in it; and either a standard
        length, or seams within 1 s at both ends in at least one airing.
      - `ad`, within one show: seams within 1 s at both ends in at least one
        airing, no logo seen in any airing, and a standard length. This
        applies within one recording and across recordings with the same
        series key (two CBS NFL games).
      - `in-programme ad`: airings in at least two series keys or channels,
        and a logo seen during at least half of it in every airing that has
        Evidence.
      - `programme`: labels call it programme (Harness only), or every airing
        has a logo seen and all share one series key. Never promoted after.
      - otherwise `candidate`. An airing with logo unknown (no Evidence, such
        as a Plex file) counts toward recurrence but can't satisfy "no logo
        seen" alone: at least one airing with Evidence must.
      - Comskip's block decisions are never used.
    - Ensures: corrected captures are the app's captures, other than this
      recording, with an airing of an entry that changed status or was split.
  - `op liveIndex(now) -> entries`
    - Ensures: entries with status `ad` or `in-programme ad` whose last airing
      is within 8 weeks of `now`, at most 20 MB; the most recently aired first
      if over.
- Invariants: a `programme` entry is never `ad`; every airing belongs to a
  stored stream; an `ad` entry meets one of the two `ad` rules.

| Given | When | Then |
| --- | --- | --- |
| empty store; g5 with Evidence | findRepeats | 30 s repeats bounded by seams, no logo, become `ad`; the 2.5 s CBS sting stays `candidate` |
| g5 stored; g4 (CBS, other series key) added | findRepeats on g4 | entries in both become `ad`; g5 is a corrected capture |
| two CBS NFL games share an NFL graphic with logo seen | findRepeats | `programme` |
| two CBS NFL games share a 9 s stretch without seams | findRepeats | stays `candidate` |
| a 31 s repeat across two series, logo seen throughout | findRepeats | `in-programme ad` |
| a 30 s entry; a later airing agrees only for its first 15 s | findRepeats | entry split into a 15 s and a 15 s entry |
| repeat only between two Plex files (no Evidence) in different series | findRepeats | `candidate` until an airing with Evidence shows no logo |
| an `ad` last aired 10 weeks ago | liveIndex | not included |
| that ad found again by a pass today | liveIndex at next refresh | included |
| same stream added twice | addStream | no change |
| another job holds the write lock for 2 s | addStream | waits and succeeds |
| store unreadable | any op | StoreCorrupt |

### PART: LiveMatcher

- Kind: core.
- Owns: the recording's in-memory codes and its live matches.
- Offers:
  - `op feed(codes) -> new or changed matches`
    - Ensures: a match is reported once at least 3 s of codes agree with one
      live index entry, or with the recording's own codes at least 20 s
      earlier, at one offset. Its agreed part grows while codes agree; its
      projected part is the rest of the entry's extent. When codes stop
      agreeing for more than 1 s inside the extent, the projected part is
      withdrawn and the match ends at the agreed part.
    - Ensures: a self-repeat match is never turned into an `ad` mark unless
      the Combiner's within-show conditions hold.
    - Ensures: no match joins codes across a run boundary.
  - `op refresh(live index)`: later matches use the new index.

| Given | When | Then |
| --- | --- | --- |
| index holds a 30 s ad; codes reach 4 s into it at 600 s | feed | match: agreed 600-604, projected 604-630 |
| codes keep agreeing to 630 | feed | agreed 600-630, nothing projected |
| a 15 s cut airs: codes stop agreeing at 615 | feed at 616.1 | projected part withdrawn; match 600-615 |
| 2 s agree, then 2 s noise | feed | no match |
| a concurrent capture promotes an ad | refresh, then feed | matches on that ad from then on |
| run boundary inside an ad | feed | codes on either side never joined |

### PART: Combiner

- Kind: core. Pure: same inputs, same marks.
- Offers:
  - `op combine(matches, Evidence, now or final) -> Mark list`
    - Ensures, in this order:
      1. Each `ad` match becomes an `ad` stretch over its agreed part plus its
         projected part; each `in-programme ad` match an `in-programme ad`
         mark. A match edge moves to a seam within 1 s.
      2. Two `ad` stretches less than 90 s apart are joined when no logo
         interval of 5 s or more lies between them. When no logo was learned
         for the recording, only gaps up to 30 s are joined (Q9).
      3. A break's start extends back over Comskip commercial blocks touching
         it, and its end forward the same way, to those blocks' edges.
      4. A break closes at the first logo interval of 5 s or more, or Comskip
         programme block, after its last match or extension.
      5. Every Comskip commercial block not already covered becomes an `ad`
         mark with basis `comskip`, as today.
      6. `ad` marks never overlap; an `in-programme ad` inside an `ad` mark is
         dropped.
    - State: in final mode every mark is `settled`. In live mode an `ad` mark
      whose every part is agreed (not projected) and whose end is before
      `now` is `settled`. Projected parts, joins, extensions and basis-
      `comskip` marks are `provisional` until 3 minutes after their end with no
      change (roadmap Q9).
    - Errors: `EvidenceMissing` when no Evidence was given; FingerprintTool
      then writes no MarksFile for that capture.

| Given | When | Then |
| --- | --- | --- |
| matches 860-890 and 920-950, no logo between, seam at 859.6 | combine final | one ad 859.6-950 |
| matches 860-890 and 1000-1030, no Comskip block between | combine | two breaks |
| match 860-890, Comskip commercial block 830-860 | combine | ad 830-890 |
| match 860-890, logo from 892 for 60 s | combine | break closes at 890 (or seam within 1 s) |
| no matches | combine | exactly Comskip's commercial blocks, basis comskip |
| in-programme ad match 1478-1509 | combine | one `in-programme ad`, not joined to a break |
| live, now 900, match fully agreed 860-890 | combine live | ad 860-890 settled |
| live, now 605, agreed 600-604, projected to 630 | combine live | ad 600-630 provisional |
| live, now 900, joined 860-950 | combine live | the joined mark provisional |
| no Evidence | combine | EvidenceMissing |

### PART: FingerprintTool

- Kind: core (wiring). Invocations by the TV app:
  - `op pass(store, RecordingId, Context, audio stream, Evidence copy paths of this and other captures, output dir) -> pass result | exit code`
    - Ensures: addStream, findRepeats, then Combiner for this capture and each
      corrected capture whose Evidence copy was given; one MarksFile per
      capture combined; a corrected capture without an Evidence copy is listed
      as `needsEvidence`. Repeatable: the same pass twice gives the same files.
  - `op live(store, RecordingId, Context, audio stream, live Evidence path, MarksFile path)`
    - Ensures: codes appended at least every 10 s; the live index refreshed
      every 10 s; the Evidence file reread within 2 s of being replaced with a
      higher revision; the MarksFile replaced within 1 s of any change; reading
      standard input never pauses for more than 1 s (it drops and counts).
  - `op seed(store, RecordingId, Context, audio stream)`: addStream only, for
    parallel coding.
  - `op search(store, RecordingId, Evidence copy paths, output dir)`: the
    findRepeats and Combiner half of a pass, for seeded recordings.
- Never does: write dvr.sqlite, or any file other than its store and the given
  output paths; leave a file half-written.

### PART: EvidenceExport (Comskip, changed)

- Offers: an ini key turns on an evidence file next to the EDL, with the
  Evidence fields in proxy seconds.
  - Ensures: built from the same per-frame data `output_framearray` prints
    and the blocks the `.log` lists; written whole; in live mode rewritten
    whenever the EDL is, with a revision that grows; the EDL and every other
    output byte-identical to a run without the key; the file stays small
    (seams and intervals, never per-frame rows).

| Given | When | Then |
| --- | --- | --- |
| g5 file, key on | run | evidence file; EDL identical to key off |
| live run | each EDL rewrite | revision increases; never seen half-written |

### PART: AudioFeeder (TV app, boundary)

- Ensures: mono 11025 Hz audio of the main audio track (the one HLS uses,
  `0:a:0`), taken from the AC-4-capable path: on AC-4 captures, the ffmpeg-ac4
  companion's transcoded audio; otherwise the normal decode. This is a
  requirement, not a fallback: a capture whose HLS audio plays has
  fingerprint audio.
- Post-recording: from the saved audio rendition, one run per stretch of
  consecutive segments.
- Live, before T6: an extra audio-only output of the encoder process that
  already decodes the audio, one run per generation. After T6: the audio piped
  into Comskip.
- Plex files: decoded from the file, one run starting at 0.
- Ensures: the encoder never waits on the tool; a full queue drops audio and
  counts it; the next audio starts a new run.
- Errors: `NoAudio`: logged, recording skipped.

### PART: Timeline (TV app)

- `op toCapture(proxy seconds, capture) -> Seconds | AxisUntrusted` (D4).

### PART: FingerprintRunner (TV app)

- Ensures: saves the Evidence copy at final parse (flow 2) and after each
  re-detection; one repeat search at a time; live jobs one per capture; seed
  coding jobs up to 4 with no capture in progress and 1 while any capture is in
  progress (U1); all jobs at the lowest CPU and I/O priority; the backfill
  exclusion list and the duration filter are settings; setting `off` stops all
  jobs and the merged view falls back to Comskip's list.
- Handles: tool missing, non-zero exit, timeout (pass: 10 minutes per recorded
  hour; live: no MarksFile change and no exit for 60 s while audio flows) by
  logging, keeping marks, and retrying once (pass) or restarting up to 3 times
  (live). For `needsEvidence` captures with an Evidence copy, reruns the
  search once.

### PART: MarkStore (TV app, changed)

- `op applyFingerprint(captureId, MarksFile) -> changed | unchanged | stale`
  - Ensures: replaces the capture's fingerprint list whole (D3 id and
    exclusion rules); `stale` when its Evidence revision is lower than the one
    applied; keeps the previous list of a past capture 14 days.
- `op view(capture) -> merged marks`: D3.
- Emits: the existing marks change notification (SSE).
- Invariant: ComskipDetector and re-detection never write the fingerprint
  list; MarkStore never writes Comskip's list.

### PART: MarkClients (TV app)

- Ensures: `ad` marks look and skip as today; auto-skip skips only `settled`
  `ad` marks; `in-programme ad` marks come in a separate list and are shown
  with their own shading, never auto-skipped; a client that predates kinds
  never receives a non-`ad` mark in the list it skips from.

## Steps

"F" steps are in this repo, "T" steps in the TV app, "L" steps are labelling.

- **F1. Labels and scoring for kinds.** Move `tools/fingerprint/repeats.py`
  into `bin/perf`. Add label kind `in-programme ad` (scored as neither) and
  label the g5 Google split-screen. `score_labels.py` scores only `ad` marks as
  cuts and reports `in-programme ad` marks separately. Check: the README's
  Comskip-107 table is reproduced.
- **L1. Label two dramas** from the Plex library, one CBS and one NBC, made by
  the agent from contact sheets and spot-checked by the user. Score
  Comskip-107 on them and add the rows to the README. Check: both scored.
- **F2. EvidenceExport in Comskip.** Check: identical EDLs with the key on and
  off on the labelled set and `bin/perf` fixtures; the file is whole at every
  rewrite.
- **F3. Coder and `seed`.** Check: the platform facts; at least 99% of codes
  within 2 bits of fpcalc `-length 0` on g5 and g4; the 80-minute repeat
  within 0.2 s; CPU about 9 s per 90 min in the test container.
- **F4. Library, Combiner, `search` and `pass`; offline Harness.** Harness:
  replay a labelled recording with a chosen library; Evidence from the same
  file (one generation, so proxy seconds equal capture seconds). Check: with
  an empty library, the within-game coverage of October 27 (about 15% of ad
  seconds on g5 and g4); Must-not-get-worse 1 on all six; corrections appear
  for g4 then g5; the 15 s / 30 s split example.
- **F5. `live` mode.** Harness live mode feeds audio at real speed. Check:
  each matched ad marked within 6 s of its labelled start; projected parts
  withdrawn within 2 s of a divergence; a stalled reader drops and counts;
  memory under 48 MB.
- **T1. Two mark lists and the merged view.** MarkStore, `publishMarks`
  copying both lists, `marksForWindow` reading the merged view and splitting
  non-`ad` marks into their own list; `kind`, `state`, `basis` fields.
  ComskipDetector and re-detection keep writing only `commercialMarks`. No
  fingerprint yet. Check: with no fingerprint list, marks API output identical
  to today; a test `in-programme ad` mark is not skipped by the current web and
  Android builds; a final parse and a re-detection leave a test fingerprint
  list untouched.
- **T2. Generation fix for Timeline** (SERVER_HARDENING). Check: a real-
  Comskip test across a generation seam gives correct capture seconds.
- **T3. Post-recording pass, shadow then on.** FingerprintRunner, AudioFeeder
  (saved rendition), the Evidence copy saved at final parse and after
  re-detection. Shadow first: results logged and compared, not applied. Then on
  behind a setting, only after L1 and F4 pass Must-not-get-worse 1 and 8.
  Check: NAS CPU and memory within budget; the off-or-broken tests; an Evidence
  copy survives proxy reclamation.
- **T4. Backfill.** Seed jobs for the app's own captures and the Plex root
  (`/volume1/Shared/plex/Shows`, read-only, about 230 broadcast episodes of
  about 60 min plus 10 NFL games, about 260 h), with the exclusion list and
  duration filter. Cost: about 9 CPU s per 90 min, so about 26 CPU minutes for
  260 h, about 7 minutes of wall time on 4 cores; reading the files (the NFL
  games alone are 146 GB) is likely the limit, hence the lowest I/O priority
  and 1 job while a capture is in progress. Repeat searches follow one at a
  time. Check: store size and entry counts logged; recording during backfill
  shows no input backpressure; Harness rerun on the labelled set with the
  seeded library (measurement plan).
- **T5. Live fingerprinting.** AudioFeeder's encoder audio output per
  generation, `live`, live Evidence handoff, provisional and settled marks.
  Check: Must-not-get-worse 2 and 3 on the NAS with the most concurrent
  captures seen in the jobs table; live marks appear while recording.
- **T6. Pipe frames and audio into Comskip** (target input path, replacing
  the commercials.ts proxy). Depends on roadmap stages 1-2 in Comskip: B126
  (the logo rewind fails on a raw-frame pipe) and B127 (pipe and file results
  differ). The pipe carries one continuous clock across generations, and
  AudioFeeder takes live audio from the piped audio. Check: roadmap stage 2
  checks; live fingerprint marks unchanged against T5 on replayed captures.
  The post-recording pass (T3) does not depend on this step.
- **F6/T7. Fingerprints primary.** The Combiner's result becomes the default
  merged view. Check: Must-not-get-worse 1 on all labelled recordings
  including the two kept aside, with the seeded library, and better coverage
  than Comskip-107 on at least two.

## Measurement plan

For each labelled recording, with libraries of growing size:

- Library sets: empty (within-recording only); the other labelled recordings;
  N backfilled recordings that aired before the labelled one, for N = 5, 10,
  20, 50, all (never later recordings, so results don't use the future).
- Report: breaks found, false breaks, ad seconds missed, programme seconds cut
  (sponsor and in-programme ad rows excluded), share of ad seconds covered by
  matches alone, `in-programme ad` marks and how many fall on labelled rows.
  Compare with Comskip-107.
- Count entries per status, and `programme` entries found (guards working).
- Live: time from labelled ad start to mark; how often a provisional mark later
  moved by more than 5 s or vanished; how often a projected part was withdrawn.

## Gates (run October 27, re-run on the edited parts after review)

1. Cross-reference: every part in the flow is in Parts; every named error
   (`InputInvalid`, `StoreUnavailable`, `StoreCorrupt`, `StoreTooNew`,
   `EvidenceMissing`, `AxisUntrusted`, `NoAudio`) is produced by one part and
   handled in the flow or FingerprintRunner.
2. Walk-through: g5 pass with an empty library; a two-generation capture
   before T2 (codes stored, no marks); a live capture whose tool dies twice; a
   15 s cut of a known 30 s ad live; a final parse after a fingerprint list
   exists (list untouched).
3. Goal trace: every sentence of the goal maps to a Library rule, a Combiner
   rule, a flow step or a must-not-get-worse item.
4. Platform facts still to check first-hand, each the first check of its
   step: Chromaprint streaming and the vcpkg version (F3); the encoder's extra
   audio output cost and the companion's audio reaching it (T5); store
   concurrency (F4); repeat-search memory on the N100 (T3).

One independent review done; all its findings are addressed above.

## Open questions (recommended answers)

1. **Store location?** Recommended and confirmed by review: a separate store
   file next to dvr.sqlite (D2). Changes roadmap Q10.
2. **Settling live fingerprint marks?** Recommended: an `ad` mark is settled
   once every part of it has agreed and aired; projected parts, joins,
   extensions and Comskip-only marks settle 3 minutes after their end (Q9).
   Auto-skip uses settled marks only.
3. **How do old clients avoid skipping in-programme ads?** Recommended and
   confirmed by review: the skip list carries `ad` marks only; other kinds go
   in a separate list.
4. **May a correction remove a Comskip mark?** Recommended: no (the Combiner
   keeps them all). Revisit once fingerprints are primary.
5. **Evidence copy in the capture record or a file?** The review suggested the
   capture record. Recommended: a file in the capture directory outside the
   proxy's name pattern, because the copy is up to a few hundred KB and the
   capture's `metadata_json` is rewritten on every capture change.
6. **Sponsor kind?** Recommended: reserve it; emit only `ad` and
   `in-programme ad`. Decide after F4 shows how sponsor billboards repeat.
7. **Expiry?** Decided (U2): the store never expires; the live index holds
   entries aired in the last 8 weeks, and an older ad found again re-enters it.
8. **Who maps Comskip's Evidence to capture seconds?** Recommended: the TV app
   (Timeline).
9. **Gap joining when no logo was learned?** Recommended: only gaps up to
   30 s, until F4 measures more.
10. **Channel and air date for Plex files?** Recommended: series key from the
    show folder, channel unknown unless the file name gives it, air date from
    the file name or else its modification time.
11. **Patents:** the user accepts the risk for a free repo; keep the stage off
    by default in the public build and read the claims before announcing it.

## Assumptions

- [ASSUMED: about 40 recorded hours a week; the jobs table will tell.]
- [ASSUMED: at most 4 concurrent live captures (4 tuners).]
- [ASSUMED: a capture's saved audio rendition is kept until its pass has run.]
- [ASSUMED: the Plex files' AC-3 audio decodes through the normal path.]
- [ASSUMED: broadcast slots are multiples of 30 min; the duration filter is a
  setting if not.]
