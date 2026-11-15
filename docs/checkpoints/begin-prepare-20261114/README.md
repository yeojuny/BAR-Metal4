# BEGIN/PREPARE source checkpoint · 2026-11-14

This checkpoint contains the complete engine source used for the tested BEGIN preparation-overlap repair, plus both current measurement campaigns. The signed test engine SHA-256 is **fd2ba879ee5867c4ebde836689e1e53c2e699d0571b8f5efaa54ce3b0135c4b9**. The existing adopted performance baseline is **8a65879eee3d019b6c8823b9caa756c0934bd937fc5217a6c44be108202259a6**, using PREPARE1/BEGIN0. The tested fd2 checkpoint retains a corrected BEGIN option. It has no new product version designation.

The source begins at **c8a8ac929cb69b40a3bdb87543daa855963b4b90**, root tree **c182eb4de0abc24832366501e10ab67d05d80799**, engine tree **c78d0e44a8e9ead1686c61b0aaf5798a2a65c828**. That previously unpublished commit and its particle-tail parent **67a479c8011857d252d91b2145830bd82af0b757** are preserved with original authorship. They descend from the already published **2e8423d3f427943d58cbfc78e5c639ef72501bfd**. The intervening compile and integration fixes belong to their original authors.

## Results and data

Both current campaigns used the same fd2 signed executable with the selected mode changed at launch. They used BC213, the same map/seed/camera, a 1512×950 view, AA0 and VSync0, worker1, and the common capture/frame/prepare overlap flags. Runtime LimitIdleFps was 0 and option_limitoffscreenfps was false. The unchanged pacing module and original 9.8-second controller gate produced about 14 seconds of observed common warmup, followed by simulation300–1500 and about40 seconds counted time. All valid runs were retained. DrawScreen intervals are CPU-observed with millisecond resolution.

| Campaign and mode | Runs | Pooled FPS | Pooled slowest 1% FPS | p99 |
| --- | --- | --- | --- | --- |
| Repair H, old BEGIN | 2 | 106.416 | 74.523 | 12 ms |
| Repair H, fixed BEGIN | 2 | 185.257 | 128.117 | 7 ms |
| Repair H, PREPARE | 2 | 184.905 | 134.913 | 6 ms |
| Additional I, fixed BEGIN | 4 | 185.272 | 130.492 | 7 ms |
| Additional I, PREPARE | 4 | 185.106 | 140.492 | 6 ms |

H fixed BEGIN recovered **74.087%** relative to its old BEGIN mode. Its advantage over the same engine's PREPARE mode was **0.191%**. In the additional I campaign, BEGIN was **0.166 FPS / 0.090%** higher on average, with four paired differences of +0.073%, +0.176%, −0.070% and +0.180%. The mode-wise sample standard deviations were about0.302 and0.277 FPS. These data do not establish a clear mean-FPS winner or statistical equivalence. PREPARE had the higher pooled slowest1% figure in I. The campaigns have separate populations and are not pooled together.

- [H result JSON and every clean run](repair/RESULTS.json), [six unchanged frame CSVs](repair/frames), and [nine normal exits including validation and screens](repair/measurement/OWNER-CLOSED.json).
- [I result JSON and every clean run](comparison/RESULTS.json), [eight unchanged frame CSVs](comparison/frames), [eight normal exits](comparison/measurement/OWNER-CLOSED.json), and [profile equality receipt](comparison/measurement/PROFILE-EQUALITY.json).
- [Independent CSV recalculation](CSV-RECALCULATION.json) verifies every current run's FPS and slowest1% and each campaign's pooled calculations. Mean FPS is totaldraws/totalwalltime. Slowest1% is the reciprocal of the mean of the slowest ceil(1% interval count) frame intervals. The omitted boundary partial interval explains one fewer interval than draws per run.

### Earlier F and G evidence

| Separate historical campaign | Result and scope |
| --- | --- |
| F capture-ready, paced clean comparison | OFF179.234 → PREPARE185.397 FPS, +3.438%. [Completion and run evidence](history/capture-ready-initial.json). |
| F independent paced recheck | OFF179.33 → PREPARE185.36 FPS, about+3.36%. [Recheck evidence](history/capture-ready-recheck.json). |
| G functional BEGIN boundary | JOINED179.150 / BEGIN106.484 / PREPARE185.427 FPS. BEGIN was retained as a failed performance candidate, motivating H. [Completion and run evidence](history/begin-boundary-before-repair.json). |

F initially had two validation attempts below the original warmup gate. Those stopped attempts were excluded from performance pooling and preserved. The later paced comparisons above are separate records. G's first validation game completed normally, but the postprocessor incorrectly required two pause draws even when the existing pacing rule allowed a short pause. The parser was corrected and the **same raw data** were reread without another validation game. These histories must not be treated as engine failures or combined with H/I sample pools.

## Validation and observed limitations

H had nine normal games, consisting of six clean timing runs, one Metal API Validation run with **zero detected errors**, and two screenshot runs. I added eight clean games. All observed foreground samples matched the owned game and the displays were active and unlocked. These checks cover observations at sampled times. Display-off execution was not tested.

[Validation activation trace](repair/VALIDATION-TRACE.json) records actual BEGIN request/eligibility/queueing, a real Begin submission, pending predecessor ownership, and CPU credit reclamation. Pending ownership does not prove that GPU hardware was still executing at that exact instant. The worker can wait for prior GPU completion within CPU Join. Host tests model completion separately from the actual Metal validation run.

[Old BEGIN image](repair/screens/old-begin.png) and [fixed BEGIN image](repair/screens/fixed-begin.png) show the same static terrain, units, rings and occlusion at simulation1500. Water animation and paused-text location differ. Common dark horizontal regions and blur remain. The [visual review](repair/VISUAL-REVIEW.json) reports no obvious new static difference, with no full-frame pixel-equivalence claim.

The completed build used normal Ninja dependency handling, including1578 build steps and4 final review-fix steps. The [archive closure](reproduction/ARCHIVE-CLOSURE.json) found zero stale-member mismatches across **35 archives and1538 objects**. [Host checks](reproduction/HOST-CHECKS.json) and [ownership review](reproduction/OWNERSHIP-REVIEW.json) retain their pre-runtime status and scope. The immutable candidate.runtime=PENDING nested in H RESULTS is the earlier package snapshot. The [final status](repair/FINAL-CANDIDATE-STATUS.json), top-level RESULTS, and OWNER-CLOSED records contain the final runtime decision.

Selected initial profile file hashes and five content/map/package hashes matched across I's eight profiles. The entire2.3GB content pool was copied from the same frozen input, but it was not independently rehashed after each copy. This checkpoint does not establish large-battle performance, peak RAM, input-to-present latency, a clean-machine package, or a230FPS outcome.

## Source closure and restoration

[SOURCE-CLOSURE.json](SOURCE-CLOSURE.json) pins three ordered source commits, patch hashes and the16 changed files. [FULL-ENGINE-TREE.json](FULL-ENGINE-TREE.json) pins all **10,229 engine Git entries**. The final engine tree is **ad2cfb3c1b795d1069fb4421e59b1fead785cb7c**. There are13 modified and3 added paths relative to c8, comprising9 production and7 test files. The4 simdjson dotfiles omitted by the owner's source archive remain in the Git base. Dirty third-party samples and untracked app/build material from the source working folder are outside the committed input.

The ordered patches are F [capture-ready](reproduction/parent-capture-ready.patch), G [functional BEGIN](reproduction/begin-functional.patch), then H [BEGIN preparation overlap](reproduction/begin-prepare-overlap.patch). H alone cannot recreate the checkpoint. [Restoration and build guidance](reproduction/README.md) gives both the ready source tree and base-plus-patches routes.

The [evidence manifest](EVIDENCE-MANIFEST.json) records original and published hashes plus path transformations. Published JSON substitutes owner home paths with repository references or local-reference markers. Such markers identify provenance-only artifacts, including packaged apps, and do not claim a file exists in this repository. Frame CSVs and screenshot bytes are unchanged. The private preservation PR retains selected original records. Full personal profiles, game pools and unfiltered environment/log dumps are not part of this public checkpoint.

LuaVBO's two missing error-format arguments were corrected upstream by **tomjn, RecoilEngine PR3695**, and are inherited unchanged from c8. The macOS affinity QoS and benbreen worker-entry ideas in the historical provenance ledger are not implemented in this source baseline. See [checkpoint applicability and original credits](UPSTREAM-CONTRIBUTIONS.json).

## Shared implementation and next strategy

The recommended execution policy is PREPARE1/BEGIN0. Fixed BEGIN remains a session-selected option in the same renderer. BEGIN returns after an actual command Begin notice and retains at most2 source generations. PREPARE returns after enqueue and can retain3. Both keep the same single outstanding GPU submission rule and retire its predecessor before the next GPU commit. Source credits are not GPU inflight depth or a RAM measurement. In this checkpoint, both raw mode flags ON canonically select PREPARE. The correction does not combine their gains.

[Architecture review with source symbols](comparison/ARCHITECTURE-REVIEW.json) explains the shared worker, admission, mailbox, rollback and GPU retirement contracts. Fixed BEGIN's exact scene-only capability checks source credit, Begin receipt, mailbox epoch and context. The worker emits an atomic notice and the main thread applies the completion bookkeeping after Join. If aliases or unsettled ownership prevent reclaim, the existing full GPU drain remains the fallback.

1. Join the main **FinishCPUWork** interval and the worker's predecessor-GPU-retirement interval using the same source_draw. Include BEGIN's early source-credit Join and PREPARE's capture-suffix Join without double-counting. Determine whether exposed time is actual CPU work or a GPU dependency.
2. Based on that measured critical path, choose one substantial change in a proven immutable capture prefix, duplicated common prepare work, or a common GPU pass/submission/retirement boundary. Preserve mutation/version, read/write order, source ownership and rollback.
3. Measure actual cache/allocation/copy costs before selecting them for another experiment.
4. Consider increased GPU inflight depth only after proving slot, allocator, residency, producer-image and read-check lifetimes. Completed-event ownership remains the reuse condition.

Use matched40-second repeated runs, prioritize meanFPS, retain adverse tail results and genuine structural benefits, and test one hypothesis/change at a time. At185FPS, reaching230FPS would require about1.06ms less critical cycle time. These measurements do not yet identify which stage could provide it.
