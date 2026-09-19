**Plan: mostly automatic improvement of live commercial detection**

Status: proposed design, September 19, 2026. No detector changes or accuracy measurements have been made. All new flags, tools, thresholds, and resource budgets below are proposals. The initial investigation used the pre-modernization code at `a140b6a`; implementation targets modernized `master` and must re-audit those findings in the current `src/` tree.

The objective is to reduce program content skipped by mistake, reduce incomplete commercial skips, and improve future recordings with little recurring user effort. Existing Comskip remains the baseline and fallback. The project succeeds if a limited extension improves this installation; it does not need to replace Comskip generally.

Mostly automatic operation is realistic. Demonstrating that predictions are correct still requires some independent evidence. An offline Comskip result, another model's prediction, or agreement between detectors is not ground truth. With no reviewed examples, the system can collect data and propose improvements, but it cannot honestly establish that a new model is more accurate.

**Confirmed TV application integration and storage**

The inspected TV application deployment copy is `/mnt/nas/Development/TV`. It is not the authoritative development checkout and will be replaced by normal TV deployment. Its root package uses `apps/server` and `apps/web`; integration work belongs in those active workspaces in the TV development repository. Source inspection only has been performed. The user has deferred reviewing recordings, video frames, and detection accuracy until later.

The checked-in Linux Docker Compose configuration maps NAS host `/volume1/Shared/TV/data` to `TV_DATA_DIR=/data` and `/volume1/Shared/TV/media` to `TV_MEDIA_DIR=/media`. Keep durable learning data on the NAS alongside this existing TV data, using configured roots rather than hardcoding a path under the development checkout. These are configured paths, not a verification of the running container's actual mounts. The client-visible `/mnt/nas` path and the NAS host/container paths need explicit mapping when dispatching remote work.

Proposed storage is `TV_DATA_DIR/comskip-learning/` for feature artifacts, profiles, model versions, review labels, immutable marking snapshots, and evaluation manifests; bounded preview clips and job staging belong under `TV_MEDIA_DIR/comskip-learning/`. The TV service owns catalog/job changes and publication. A separate learning catalog can hold derived data if needed, but remote workers must return artifacts through the TV service rather than writing its live SQLite database across the network. Recordings and existing capture files remain in their current locations. Keep review labels/snapshots beyond transient capture cleanup; retain only explicitly selected evidence clips or recordings through the existing retention/ref-count mechanism.

Reuse these existing integration points:

| TV component | Existing behavior | Planned extension |
| --- | --- | --- |
| `apps/server/services/dvrCommercials.js` | Runs Comskip, polls EDL, maps proxy timestamps, stores capture marks with source metadata and detection time | Preserve versioned raw EDL/INI/build information and publish candidate evidence separately |
| `apps/server/services/dvrRedetect.js` | Queued manual re-detection for finished recording windows, with gap mapping and bounded mark replacement | Reuse processing/mapping routines for an explicitly automatic analysis job type; retain current manual-job semantics |
| `apps/server/services/dvrMarks.js` | Projects shared capture marks into recording windows and applies recording-specific exclusions | Keep capture, recording, and player coordinates explicit in reviews and learning labels |
| `apps/server/services/dvr.js` | Saves removed-mark feedback with interval, reason, and time; delivers commercial updates | Reuse feedback and delivery; extend reviews to boundary edits and missing ads |
| `apps/server/services/dvrCapture.js` | Produces a growing commercial proxy and tracks encoder generations | Persist the exact generation/timeline mapping used by each analysis run |

The shipped detector input is a 480x270, 30 fps H.264/AAC proxy, not the full-quality recording. Later experiments should distinguish features lost or changed by proxy generation from detector errors. Compare the displayed footage with the corresponding proxy footage and mapped EDL boundaries. Do not assume proxy seconds equal recording-relative or playback seconds, especially across capture gaps and encoder restarts.

There is an additional live integration behavior to investigate: `dvrCommercials.js` deliberately retains unmatched older marks during live polling to tolerate partially rewritten EDL files, and removes them only on final reconciliation. Consequently, a false mark later withdrawn by Comskip can remain visible during live viewing. This is a code-based hypothesis to test, not a diagnosis of the user's recording. Pair atomic detector snapshots with an explicit complete-snapshot/revision contract before allowing live withdrawals; continue handling legacy in-place writers safely. A syntactically valid EDL alone does not prove that an in-place rewrite has finished.

At initial inspection the TV Dockerfile named `amffz9/Comskip` branch `ffmpeg8-compat`, while Comskip `master` was still the pre-modernization baseline. The modernization was subsequently fast-forwarded into `master`, its Linux/vcpkg linkage was validated, and the obsolete branch was deleted after TV stopped referring to it. Capture the deployed commit/build before implementing or benchmarking changes. Re-audit the baseline observations below against the modernized `src/` tree before implementation; the original file names and line references describe the earlier checkout.

For later spot checks, assemble a recording ID, capture ID, source/model version, raw and mapped marking bounds, and timestamp-aligned footage into each review item. Sample before, within, and after the proposed break; allow short video with audio when still frames are ambiguous. Also sample unmarked footage to find missed ads. Distinguish a wrong whole-block classification from a correct ad with incorrect boundaries. An existing removed-mark feedback entry is evidence that the proposed skip was wrong, not automatically a label that every second of its interval is program content. No spot checks or accuracy labels are being generated in this planning stage.

**1. Establish the deployment and reproduce the failures**

Confirmed hardware: the NAS has an Intel N100 CPU, and a second computer with an Intel Arc A310 can be woken over LAN, accessed through SSH, and shut down after work. Use the NAS for live detection and orchestration; make the second computer the preferred worker for expensive completed-recording analysis and training.

Record the remaining deployment details: NAS RAM, operating systems, container or native installation, Comskip version/build options, INI, DVR, player, recording formats, and output format consumed by the player. Identify whether the player reloads growing skip files and how it interprets timestamps, padding, and skip actions. Confirm the worker CPU/RAM, network throughput, and GPU driver/runtime support during setup.

Keep a representative failing recording with its original live markings, final markings, log, and configuration. Preserve versioned output snapshots during future recordings. Record the time each interval first became available, not just its final boundaries. Diagnose the reported failures separately: wrong classification, late publication, premature break ending, timestamp mismatch, or stale player data.

Start collecting roughly 20–40 recordings across several days and the channels/program types actually watched. This is a discovery set, not a statistically sufficient accuracy claim. Include sports, news, drama, credits, channel promotions, recordings that start in an ad, and channels whose logos remain during ads. Extend collection if the household uses more diverse sources.

Deliverables: reproducible baseline manifest, representative cases, a report separating detector errors from playback/output errors, and a resource budget based on the actual NAS.

**2. Correct live-mode reliability before introducing ML**

Historical findings from the pre-modernization checkout, to be relocated and verified in the current code:

| Location | Observation | Planned work |
| --- | --- | --- |
| `comskip.c:BuildCommListAsYouGo` | Separate live detector with a 15-second video-time gate and repeated scanning of cut points | Instrument latency, then introduce bounded recent-window updates |
| Frame processing near line 3384 | Unavailable logo information stores zero for logo presence | Represent unknown separately; ensure unknown never becomes positive absence evidence |
| Live export near line 16218 | Frame/FPS export differs from final PTS-based export; padding units differ across representations | Centralize interval conversion and adjustment, preserving documented output semantics |
| Live output opening/writing | Output files are truncated and rewritten | Publish complete snapshots through temporary files and platform-appropriate atomic replacement |
| Live `.incommercial` output | Reads the last commercial without an obvious nonempty-list guard | Add empty-list handling and bounds checks before candidate-array writes |
| `mpeg2dec.c` EOF handling | Close/wait/reopen/seek with finite retries | Distinguish temporary starvation from completion and verify continuity on resume |

Use normalized presentation timestamps internally for interval timing, with one explicit mapping to each supported output format. Handle timestamp discontinuities and offsets consistently. Atomic publication must be tested against the actual DVR/player and NAS filesystem, including readers that hold files open.

Add focused regression cases for no commercials, nonzero padding, fractional frame rates, missing/reset timestamps, temporary EOF, resumed recording, partial writes, long recordings, and commercial boundaries around seek/reopen. Keep compatibility checks for existing non-live output.

Deliverable: live reliability patch that can ship independently of learning. Any detection improvement is measured separately from corrected output timing.

**3. Add a replay benchmark and reusable feature recording**

Build two complementary replay paths. A detector replay consumes timestamped features progressively and prohibits access to future samples. A growing-file replay exercises the actual decoder and output path with pauses, bursts, EOF/reopen, and discontinuities. Compare both against the same labeled intervals and output timeline.

Export a versioned feature stream: recording/source identity, PTS, evidence-availability time, logo presence/confidence/unknown status, darkness, silence/audio level, scene changes, aspect ratio, decoder gaps, candidate boundaries, detector scores, and emitted decisions. Use compact rolling-window features for ML and finer event timing for boundary refinement. A starting aggregation period of 0.5–1 second is a tuning proposal, not a requirement.

Existing `--csvout`, `output_framearray`, and training-related output provide starting points, but audit them for final-pass changes and global statistics. Saving finalized features and replaying them as if they were available live would leak future information. Preserve the features actually available at each live decision.

Measure program seconds skipped per program hour, false-skip event count and worst event length, commercial seconds missed per ad minute, early/late boundary error, time to first usable skip, boundary revisions, and CPU/RAM/I/O. Report live detection lag separately from the viewer's delay behind the recording edge. Report by channel and program type, not just a household average.

A detector cannot reliably identify unseen content. When viewing at the recording edge, the choices are confirmation delay or less certain skips. The default should favor preserving program content; measure the delay required for useful detection on this installation.

Deliverables: repeatable comparison command, machine-readable metrics, and a small set of explicitly reviewed regression fixtures.

**4. Automate post-recording analysis and evidence storage**

Add a companion worker, provisionally `comskip-learn`, for queued analysis, profiling, training, and reporting. Use the TV application's existing recording lifecycle and persistent job infrastructure for discovery and orchestration rather than adding a competing filesystem watcher. Extend the existing manual re-detection processing through a distinct automatic analysis job type; its present restart behavior must not silently change. Keep the latency-sensitive live path in Comskip. The N100 runs the queue and small live model; the second computer handles expensive analysis and training. Start NAS background work with one low-priority thread and bounded I/O, deferring it whenever recording/playback suffers or decoding falls behind. These are initial limits to benchmark, not measured capacity claims. Measure concurrent recordings and actual live decoder memory before setting a total memory budget.

Prefer an explicit recording-complete event from the DVR. Where unavailable, combine file stability with recorder/job state and a configurable grace period. A temporary lack of growth alone is not proof of completion. Make jobs idempotent, restartable, and keyed by recording identity and input/configuration versions; use per-recording locks so live and final writers cannot race.

After completion, run the normal offline detector in an isolated output location. Optionally run a second detector. Compare outputs and save provenance without automatically declaring either correct. Once a publication policy has passed evaluation, reconcile markings and publish a versioned final result. Preserve the prior result for rollback, and verify the player's reload behavior.

Store metadata, feature files, labels, fingerprints, and model/configuration versions locally. A SQLite catalog plus compressed feature artifacts is a reasonable initial design. Use explicit size/age limits; retain only bounded review clips where needed rather than duplicate entire recordings. Preserve reviewed fixtures and their label provenance across ordinary cache cleanup. Never alter source recordings.

Deliverables: recording-complete integration, resumable queue, automatic final pass, bounded storage, and a local status report.

**Remote worker lifecycle for the N100/A310 setup**

Batch queued jobs into an overnight window, with an optional earlier batch after recording completion. Avoid waking the other computer for every individual recording. The NAS must continue live detection if the remote worker is off, unreachable, or fails; pending refinement and training simply wait.

1. Check authenticated worker status. Record whether the computer was already awake. Send Wake-on-LAN only when appropriate, then wait for SSH with a bounded boot timeout and retry/backoff policy. A missing ping is not proof that the computer was off; shutdown ownership requires stronger evidence, such as a worker boot identity and a valid wake-session lease.
2. Submit jobs over SSH using a dedicated account, verified host identity, explicit job manifests, and a worker lease. Use read-only access to source recordings, with a separate location for job results. Transfer compact feature files for retraining; grant video access only to jobs that need decoding or new features. Benchmark shared read-only storage against selective staging to avoid saturating recording I/O.
3. Run isolated jobs with pinned tool/model versions, time limits, checkpoints where supported, and heartbeat/status reporting. The worker emits candidate results; only the NAS publishes live/final skip files or activates models. Use checksums, schema validation, and completion manifests so interrupted transfers cannot become active results.
4. Return the candidate model/profile bundle and evaluation artifacts. The NAS reruns compatibility and inference-parity checks, archives the prior version, and applies the normal promotion gates. A successful training exit alone never activates a model.
5. After all results are durably received, allow shutdown only if this service owns the wake session, no other worker jobs remain, and a worker-side idle/inhibitor check confirms it is not in use. Leave an already-running or ownership-ambiguous computer on. A user session or other scheduled workload overrides shutdown. Scope remote shutdown capability to this policy; do not shut down merely because SSH disconnected.

Test boot failure, dropped SSH, duplicate job delivery, interrupted result transfer, worker restart, a user beginning to use the computer, and NAS restart during a wake session. Persist queue and ownership state and expire leases conservatively. Installation will need the worker address/MAC, OS, SSH account, access paths, and power policy once; ordinary operation should not require manual commands.

Use the remote CPU for the initial feature-based classifier; do not make A310 acceleration a dependency. For later neural inference, benchmark an Intel-compatible backend such as OpenVINO against CPU, validate model conversion and output parity, and enable acceleration only if it materially helps. OpenVINO documents Intel GPU inference support, but the exact model, drivers, and operating system still need validation: [OpenVINO GPU plugin](https://github.com/openvinotoolkit/openvino/blob/master/src/plugins/intel_gpu/README.md). This does not imply that pycommflag's training stack runs on the A310, or that an inference runtime accelerates training. Hardware video decoding is a separate optional experiment and must preserve feature/timestamp behavior.

**5. Learn repeat content and channel behavior**

Build channel profiles from stable source IDs, with a global fallback for unknown channels. Track logo location/reliability, brightness and silence distributions, observed boundary patterns, and typical segment durations. Use robust statistics, minimum sample requirements, bounded parameter changes, and aging of old observations. Profile updates are versioned candidates evaluated through the same process as models; changing a logo template must not bypass testing.

Add audio fingerprints, optionally verified with sparse visual fingerprints, to cluster repeated clips across recordings. Prototype with an established matcher before designing a new one. The audfprint project supports matching noisy audio excerpts and reporting match time support, making it a candidate for an offline feasibility experiment: https://github.com/dpwe/audfprint.

Separate content identity from classification. A recurring intro, recap, sports replay, or station ident is not an advertisement merely because it repeats. Maintain ad, program, and unknown exemplars; require additional context before assigning an automatic weak label. A human correction to a repeated clip can inform many occurrences, while differing edits remain separate variants.

For skipping, require sustained aligned evidence and distinguish matched coverage from inferred start/end. Do not extend a short match over an entire predicted ad or fill gaps between matches without evidence. Even a known ad can be cut short in a later broadcast; live output must stop at verified available content. Let boundary refinement choose a supported local cut point rather than a duration guess.

Deliverables: versioned channel profiles, repeated-content index, collision/variant tests, and an ablation report showing whether each actually helps.

**6. Create training labels with minimal manual effort**

Use automatic labels with explicit uncertainty. Sources can include finalized Comskip decisions, an optional second model, known-content matches, stable show context, and transitions. Each source may abstain. Track correlations: several heuristics derived from the same logo signal do not constitute independent votes. Conflicts and uncertain boundaries stay unlabeled; train on confident segment interiors rather than pretending exact boundaries are known.

This approach is commonly called weak supervision: programmatic labeling rules provide noisy labels or abstain. Snorkel documents the method, but adopting its framework is optional: https://snorkelproject.org/get-started/.

Evaluate pycommflag as an optional offline comparison source before investing in a new model. It provides an ML detector, feature logs, editing support, and EDL/text output. Its documentation describes a US broadcast/cable pretrained model and calls the project experimental. That makes it useful to benchmark, not a presumed authority: https://github.com/b-pass/pycommflag. Run it isolated from production DVR databases and output locations.

Keep three label classes in storage: human-reviewed, automatically inferred with provenance, and unknown. Never promote a prediction to reviewed truth. Keep model-generated labels tied to their generating version; prevent the current model from laundering its own guesses through fingerprints or later retraining. Train with bounded weak-label weights and sufficient program examples, not just abundant ad candidates.

Provide a lightweight review page showing a short clip around a proposed boundary, with Program / Ad / Unsure and boundary adjustment. Deduplicate recurring clips. Prioritize likely program-content cuts and detector disagreements, but reserve a random sample of agreement regions and unmarked content to catch shared mistakes and missed ads. Include occasional full-break or recording audits; boundary clips alone cannot establish whole-recording accuracy.

Aim for one small initial review batch and an optional weekly queue of about 5–10 minutes. This is a user-effort budget, not a promise that it supplies enough evidence. When the budget is exhausted or no review happens, keep uncertain areas unresolved and candidates in shadow mode. The user should never need to tune INI values or run training manually.

Deliverables: provenance-aware labels, compact review queue, correction propagation for verified repeats, and an independent evaluation set separated from training.

**7. Add the optional model and conservative live decision policy**

Start with a logistic-regression baseline and compare a small gradient-boosted tree model on existing features. Choose the simplest model that improves the held-out metrics. No GPU requirement for the first live implementation. LightGBM has a C prediction API if tree-model results justify that dependency: https://lightgbm.readthedocs.io/en/latest/C-API.html. Confirm target architecture/build compatibility before selecting the runtime.

Live inputs must be causal: trailing features, current evidence, channel profile, and duration so far. Completed-segment duration, future logo recovery, whole-recording normalization, and offline boundary refinement belong only to offline analysis. Enforce feature schema compatibility in both training and inference. Calibrate scores using reviewed validation examples before treating them as probabilities; otherwise describe them as scores.

Use a state machine for program, suspected ad, confirmed ad, and suspected return. Require sustained evidence to enter and leave an ad break. Refine boundaries locally, retain an uncertain tail, and export only supported confirmed intervals. A separate tentative-status output can inform compatible clients without turning uncertainty into a skip instruction.

Introduce proposed modes `--ml-mode=off|shadow|assist`, plus a model/profile location. `off` preserves the corrected conventional path. `shadow` records alternate decisions without publishing them. Initial `assist` should focus on vetoing dubious skips and improving supported boundaries; evaluate adding new skip intervals as a separate, higher-risk capability. Abstention can fall back to the baseline, but a positive program-content veto must not be overridden by that fallback.

A missing, invalid, stale, incompatible, or unavailable model must not stop ordinary detection. Model activation happens between recordings and is pinned for each job. If optional fingerprint inference runs out of process, a timeout must remove only that evidence source, not block the live decoder.

Deliverables: optional build/runtime integration, model loader/schema validation, shadow logs, conservative assisted policy, and failure/fallback tests.

**8. Validate, promote, and monitor automatically**

Split data by recording and time, never by random neighboring frames. Keep episodes and repeated-content families grouped for classifier generalization evaluation. Maintain a separate future-recording test of known-ad recognition, where the fingerprint index contains only material available before each test recording. Evaluate unfamiliar ads separately. Fit profiles, normalization, label selection, and calibration only on their permitted data splits.

Compare unmodified live Comskip, corrected live Comskip, final offline Comskip, profiles only, fingerprints only, and ML assistance. This reveals whether added complexity buys anything. Use reviewed labels for accuracy claims; detector agreement and fewer revisions are operational signals, not proof of correctness.

Before promotion, define channel-specific error budgets from baseline measurements. The primary gate is an acceptable upper uncertainty bound on program-content loss and false-skip events, with no material regression in audited cases. The candidate must also improve missed ads, boundary accuracy, or latency enough to justify its complexity and meet resource limits. Estimate uncertainty across recordings, accounting for repeated content, rather than treating individual frames as independent samples. Zero observed errors in a small sample does not imply zero risk.

Use a fixed reviewed regression set plus rotating prospective audits. Repeated tuning against one holdout eventually overfits it; retain a fresh future test before broader rollout. Sparse channels stay on the fallback. Do not automatically promote a model merely because its weak-label accuracy, confidence, or agreement score increased.

Run a candidate in shadow mode first, then limit assistance to validated channels. Keep the last accepted model/profile/output versions. Retrain on a schedule only when enough new evidence exists; training never implies deployment. Automate promotion within the configured policy once sufficient reviewed evidence exists. Otherwise continue collecting data without repeatedly prompting the user.

Monitor channel-logo changes, feature-distribution shifts, disagreement, skip-rate anomalies, queue backlog, and resource use. These can trigger fallback, additional sampling, and rollback, but cannot detect every silent semantic error. Explicit corrections and periodic audits remain necessary to assess ongoing accuracy.

Deliverables: automated comparison report, staged activation, rollback command, and low-noise weekly status showing accepted changes, unresolved cases, and evidence limits.

**Proposed user experience**

One-time setup supplies recording locations, channel metadata, completion notification, and output integration. The service discovers recordings, collects features, runs the final pass, builds candidate profiles/models, compares them, and activates only qualifying updates. The user gets an optional compact review queue and can leave it alone; insufficient evidence simply means no promotion. Source videos remain intact.

There are two explicit levels of autonomy. With occasional spot checks, the service can accumulate independent evidence and qualify adaptive updates. With zero spot checks, it can still provide deterministic reliability fixes, offline reanalysis, and automatic shadow experiments, but unvalidated self-trained changes remain unpublished. This avoids promising error correction that the system has no way to verify.

**Implementation order and stopping points**

| Milestone | Scope | Exit criterion |
| --- | --- | --- |
| A | Deployment audit, preserved baseline, live reliability fixes, initial replay | Reported failure categories reproduced; timing and output regression cases pass |
| B | Feature export, completed-recording worker, Wake-on-LAN/SSH lifecycle, reviewed baseline and optional second-detector comparison | Jobs run unattended; power ownership and failure recovery work; comparisons are reproducible; actual NAS cost is known |
| C | Review queue, channel profiles, repeated-content experiment | Label provenance and cold-start behavior work; measured incremental benefit or explicit rejection |
| D | Small classifier and live shadow policy | Future-safe features, validated fallback, independent comparison against simpler alternatives |
| E | Restricted assistance, automatic qualification, monitoring and rollback | Prospective error/resource gates pass on eligible channels |

Each milestone should be a small set of reviewable changes with focused tests. Do not begin by refactoring the entire detector. If reliability fixes and final-pass processing solve most of the observed problem, stop there. If fingerprinting helps but ML does not, ship fingerprinting alone. If no candidate beats the corrected baseline, retain Comskip and report that result.

The remaining unknowns that determine scheduling are NAS RAM and concurrent workload, worker OS/CPU, DVR/player behavior, recording diversity, and baseline failures. Engineering work and the elapsed time needed to accumulate representative broadcasts are separate; a fast implementation does not eliminate the latter. The immediate implementation target is milestones A and B, which provide useful improvements and the evidence needed to decide whether C through E are worthwhile.
