# One CPU post-completion diagnostic: small observed settlement tail

The one authorized c341 game exited normally. **33 exact interior predecessor → successor chains** in simulation 900–915 show a mean post-observation tail of **32.586 microseconds (0.032586 ms)**, median 32.458 us, p95 46.417 us and maximum 47.958 us. This observation does not show a repeatedly large CPU settlement delay before the next submission.

The result is **EXACT_INTERIOR_CHAINS_WITH_BOUNDARY_LIMITS**, not full-window predecessor coverage. Of 34 sampled worker jobs, the first retires an unsampled pre-window submission (retire status 4); that chain is explicitly retained as boundary-censored and excluded from exact-chain statistics. The final sampled submission has no sampled future successor. No missing interior predecessor, duplicate ID, missing parent, dropped record or manifest truncation was found.

| Same-clock interval | n | Mean us | Median us | p95 us | Max us |
|---|---:|---:|---:|---:|---:|
| FeedbackWait(N).end → Submit(N+1).start | 33 | 32.586 | 32.458 | 46.417 | 47.958 |
| Submit call start → end, separately | 33 | 29.876 | 29.584 | 35.458 | 36.875 |
| AllocatorReset21 intersection | 33 | 2.148 | 2.083 | 3.792 | 4.541 |
| GPUCallback22 intersection | 33 | 17.798 | 17.417 | 26.416 | 31.833 |
| ResidencyCleanup23 intersection | 33 | 0.429 | 0.417 | 0.583 | 0.583 |
| ReadChecks29 intersection | 33 | 1.486 | 1.334 | 2.291 | 2.375 |
| Union of those four child intervals | 33 | 21.861 | 21.624 | 33.582 | 37.040 |
| Remaining outer-tail time | 33 | 10.725 | 10.875 | 12.957 | 13.334 |

All 33 exact chains are included. Percentiles use nearest rank. Component intervals are intersected with the outer tail and unioned; RuntimeRetire is not added to its children. ResourceCommit24 and Encode6 ended before retirement in every exact chain, so no early-drain case was mixed into the ordinary deferred population. The callback is the largest measured component of this small tail. Unclassified remainder includes instrumentation and unbracketed error/submit preparation; it is not assigned to an invented owner.

The mean submit-start cadence was 15.532149 ms; the sum of measured tails is about 0.210% of the corresponding cadence sum. This is contextual timing, **not predicted FPS gain or removable GPU idle**. EventWait averaged 7.782742 ms and FeedbackWait 0.057256 ms; those waits are not counted as potential savings. The tail begins at host feedback-wait completion, not actual GPU end. Submit-call duration includes drawable waiting and other submit/present work.

Trace completeness and limits:

- 1,496 trace rows, 68 complete buffer manifests (34 capture + 34 worker), 0 dropped; maximum 27 records per buffer versus capacity 128.
- 1,566 total emitted budget records and 1,246,039 budget-accounted bytes, below 8,192 records / 4 MiB. Two emission receipts cover 67 + 1 buffers, both outside_window=1 at log-prefix simulation 916. Request/job simulations cover every integer 900–915.
- Two status-4 records describe the initial unsampled main join and initial predecessor retirement. 238 status-1 MainFinish rows report no-job branches; they are not failed or missing worker chains. No status-2/3 rows occur.
- Job 34 was admitted at simulation 915 and its tail overlaps the first outside-window emission. It remains included (28.250 us tail, 28.542 us submit). Its buffer was emitted later. The overlap is a possible instrumentation interaction; no causal correction, subtraction or rerun is made.
- This is one short ROI in one diagnostic game. The first boundary is incomplete and the last job overlaps emission; no full-boundary, general-game, performance-gain or GPU-idle claim is made.

Execution and preservation:

- Round cpu-postcompletion-tail-20261117-21 / attempt cpu-postcompletion-tail-01. STATE revision82 READY/HQ was read with REQUEST/READY; all six READY artifact hashes and source/engine were reverified, absence of earlier execution confirmed, and revision83 RUNNING reserved by contents-SHA CAS and reread before launch.
- One engine/game invocation, normal exit, approximately 80.18 seconds from owned launch to cleanup; fixed bounds 210 seconds and 32 MiB. Engine log 1,336,187 bytes. Owned engine/process group and finite caffeinate exited; postrun PID checks passed. No retry.
- Actual OS-observed child diagnostic environment matches planned pins: FRAME_TRACE=1, EVERY_LOOP=1, inclusive 900–915, validation/shader validation/capture and all other optional probes off. PREPARE1/BEGIN0/worker1 and original overlap/capture-ready/S01/G01 remain. HOST_VERTEX, COMBINED_OVERLAP220 and Sepia remain unset. Public child evidence is an allowlist; full ps output stays local.
- Same Seths Ravine Remake 1.3.1, Ultra preset4/AA0, fullview, fixed camera, 1512x950 view / 1512x982 drawable, RNG154658555 and benchmark7654321, original pacing and 540–1740 anchors. Warmup resumed at 10.007 wall seconds; combat warmup was 10.001 seconds. The existing post-count native image succeeded. The counted diagnostic wall interval was 40.476001740 seconds; slow time/bins are preserved, no clean-FPS gate was imposed, and diagnostic FPS is neither computed nor accepted.
- Source 10,273 files, original metadata/profile files, signed engine and 19 dylibs, original runner, and all six READY artifacts remain unchanged after the game. Only the isolated experiment profile was writable. No restaging, source edit, configure/build/standalone shader compile, GPU helper/capture/replay/profile, FPS A/B or additional game.
- Existing bar remains PAUSED on the previous HQ. No automation creation, retarget or restart. Other local readers were quiescent per handoff; old HQ and independent audit completion were directly checked.

Decision for JINI: the observed exact interior tail does not justify treating CPU settlement movement as the large current bottleneck. The prior decision rule points next toward SensorLOS/R04 GPU-work reduction, subject to JINI's separate choice and authorization. No optimization is implemented or promoted by this result, and no additional run is requested automatically.

Reproduction: analyze.py reads raw/engine.log (authoritative trace stream), with local layout documented in the script; SUMMARY.json, TRACE-COMPLETENESS.json and CHAINS.json preserve exact integer endpoints, request/job/lease/submission/event/runtime IDs, retired IDs, ancestry, raw line numbers, boundary-censored records and every component interval. raw/infolog.txt is preserved too and is not double-counted. ARTIFACTS.json pins public files and local-only ps/image evidence. The frozen PARSER-SPEC.md is unchanged.

Engine/source author and license provenance remains unchanged. UPSTREAM-CONTRIBUTIONS.json preserves the applicable Tom J Nowell (tomjn) PR3695 diagnostic and PR3035 affinity/QoS credits and the recorded benbreen/yeojuny scope. This round adds diagnostic evidence only and claims no new authorship for inherited engine code.
