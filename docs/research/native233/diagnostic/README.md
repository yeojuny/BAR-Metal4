# Independent review of the native Metal 4 bottleneck

The strongest lead is renderer encoding and preparation that extends past the previous GPU completion. In the joined 238 run, encoding occupied 0.448 ms and preparation 0.261 ms of the mean 0.806 ms from the previous GPU end to the next commit. Those intervals identify where to inspect. Removing a call can shift work or extend another dependency, so the intervals are not an estimate of savings.

Start with the [review prompt](REVIEW-PROMPT.md), [source inventory](../OVERLAY-MANIFEST.json), and [machine-readable evidence](DATA-MANIFEST.json). This packet separates observations, hypotheses, and gates for a follow-up experiment.

For the earlier cross-backend evidence, see the [MVK3 versus Native 220 packet](../../mvk3-native220/README.md). Its raw traces and wait analysis use a separate test population.

## Versions and conditions

| Item | Role |
| --- | --- |
| v1.11.2, source commit `80f13accdc2a02b7e97cb732d230307528ce6990` | Distributed app and the public base tree. |
| 232, signed engine SHA-256 `f8c886b8ecf6e6cc2b421b60a56b3c5b91a75bd7d990e04cf9515870c1f6e0ae` | Parent of the 233 comparison. Ten feature flags. |
| 233, signed engine SHA-256 `3a0b5b4cfcd78240117664ce76a5f5ee5b01b4c0d2c03b23a32e1c8b41585ff6` | Approved local benchmark. Same ten flags plus `BAR_NATIVE_RANGE_DIVISION233=1`. |
| 234, 236, 238–241 | Separate instrumented variants of the benchmark lineage. Their observations use different windows and denominators. |
| 235 and 237 | Isolated candidates tested and held. Neither was included in the later diagnostic baseline. |
| 242 | PiP ownership candidate held after the lifetime audit. Zero candidate or validation game launches. |

The Direct50 comparison used a fixed map, game, seed and camera on one Apple Silicon Mac, on AC and in the foreground. Requested logical render size was 1512×950. The physical drawable and attachment sizes can differ and must be measured for each run. AA, VSync and VSyncGame were off. The nominal 10-second warmup preceded the 40-second measured window, simulation 300–1500. Runs within the accepted 39.9–40.1 second wall window were retained. The clean 233 comparison used AB/BA/AB, without timing instrumentation, and kept the other feature flags fixed. The audio gate was skipped for this campaign. See [233 conditions](233-conditions.json), [six clean runs](233-clean-runs.json), and [pooled results and flags](../RESULTS.json).

The 233 comparison was **165.949 → 167.035 FPS (+0.654%)**, with all three pairs positive and pooled 1% low **111.794 → 112.228 FPS**. The guarded quotient change was checked over 2,001,599 integer inputs, 3,200,000 line checks and 171,662,592 raster pixel pairs with zero mismatches in those tests. Both arms passed Metal API Validation and the tested app-to-game launch path. Those results cover the measured workload and the tested numerical cases. Broad visual equivalence and release readiness remain separate questions.

## Follow the frame

1. The app processes events and updates the simulation and window state. Read [SpringApp.cpp](../../../../engine/rts/System/SpringApp.cpp) and [Game.cpp](../../../../engine/rts/Game/Game.cpp).
2. Draw updates unsynced state, captures and seals world/Lua inputs, then hands them to the renderer. Look for DrawWorld and renderer ingress in [Metal4SceneBridge.cpp](../overlay/engine/rts/Rendering/Metal4/Metal4SceneBridge.cpp).
3. Renderer preparation resolves input versions, validation, owners and GPU objects. Encoding records CUS draws and other passes. Use [Metal4WorldRenderer.mm](../overlay/engine/rts/Rendering/Metal4/Metal4WorldRenderer.mm), [Metal4CUSDeferred.mm](../overlay/engine/rts/Rendering/Metal4/Metal4CUSDeferred.mm), and [Metal4ScreenNative.mm](../overlay/engine/rts/Rendering/Metal4/Metal4ScreenNative.mm).
4. The native runtime joins ready work with prior GPU completion and feedback, commits the current command buffer and requests present. Use [Metal4RuntimeNative.mm](../overlay/engine/rts/Rendering/Metal4/Metal4RuntimeNative.mm), especially `SubmitAndDefer` and `RetirePreviousSubmission`. A drawable presented callback measures the API's reported display time. The app's CPU buffer swap boundary is a separate event.

Use the overlay for paths listed as replacements in the manifest and the pinned base for the other engine files. The public app source also carries the separately tested foreground fix. This packet does not reconstruct the local linker inputs or the instrumented executables.

## CPU submission and GPU execution

| Evidence | Population | Observation | What the result tests |
| --- | --- | --- | --- |
| 234 submission timeline | 3,485 adjacent pairs and 218 sampled CPU frames, sim 400–1000 | Previous GPU end → ready mean 0.685 ms. Ready → commit 0.102 ms. Commit → GPU start 0.137 ms. GPU envelope 4.822 ms. Ready was late in 3,174/3,485 pairs. | Where work falls on this queue. A queue-envelope gap is not a GPU-wide idle measure. |
| 238 loop and display | 1,736 complete loops, 1,735 adjacent pairs, sim 400–700 | Loop spacing mean/p95 5.776/6.968 ms. Drawable spacing 5.783/7.654 ms from 1,733 positive-timestamp intervals. Same-frame GPU envelope 4.831/5.223 ms. | CPU loop cadence, GPU envelope, and API presentation cadence are distinct. Two zero presented times remain in the CPU/GPU population. |
| 238 exposed work | Same 238 trace | Previous GPU end → next commit 0.806 ms. Mean partition: capture/dispatch 0.015, prepare 0.261, encode 0.448, encode-end → ready 0.003, ready → commit 0.079 ms. | The timing partition narrows investigation. Capture/prepare/encode totals from nested scopes cannot be added to it. |
| 236 completion waits | 3,461 frames, sim 400–1000 | Zero mid-frame drains. Event ready before normal pre-submit wait in 3,078 frames. Event+feedback wall mean 0.043708 ms, p95 0.301291 ms, max 3.567251 ms. | Little direct wait in this scene on average, with a real tail. Other modes and caller paths may behave differently. |
| 236 pass resume | 108 detailed frames, separate sparse sample | 2,052 resumes, 19 per sample frame. | A resume can carry required depth/stencil, read and write ordering. A count alone does not prove a removable store or pass. |

The [234 pair CSV](234-runtime-pairs.csv), [238 joined CSV](238-joined-loops.csv) and [238 aggregates](238-analysis.json) let a reviewer check these joins. Each file has a different origin. The 238 diagnostic changed the SpringApp SDK binding from 27.0 to 27.2 to match the other diagnostic TUs. The instrumentation and SDK effects were not measured independently, so do not use this variant's FPS against a clean build.

The 235 encode samples give a narrower lead: CUS binding and actual draw averaged about 0.142 ms of CPU per sampled frame, and environment dependency handling 0.082 ms. Existing CUS code already reuses matching geometry bindings and merges some consecutive instance runs. Any proposal to cut further calls needs the same draw order, argument bindings and baseInstance behavior. Previous indirect command and model-consolidation experiments did not establish a gain. Cubemap batching (235) and empty alpha resumption removal (237) also failed to establish a repeatable mean FPS gain. See the [earlier dispositions](../README.md).

One separate GPU trace in the 233 candidate lineage reported exposed GPU time for forward units (0.418 ms), MRT units (0.198 ms), CAS+UI (0.502 ms) and shadow (0.411 ms). Treat these as a ranking in a different instrumented window. CAS+UI is a combined pass, and a pass duration cannot be attributed to one shader without further evidence. For a 230 FPS goal, the frame interval is 4.348 ms. Both the observed GPU envelope and main-thread loop work exceed that budget. Their deficits cannot be added, since these stages overlap.

## Lua, CPU backing and PiP

| Evidence | Result | Implication for review |
| --- | --- | --- |
| 239 display lists | 111 sampled frames, 19.2 replays and 683.5 vertices per frame on average. Replay CPU 0.0442 ms/frame, exception-state copies 0.0031 ms/frame. The stable repeated fingerprint subset had about 0.00152 ms/frame of remaining work. | Reuse of a list ID or fingerprint alone does not prove equal texture versions, clip/scissor/stencil state, or exception order. |
| 240 snapshot copy CPU | 1,695 joined frames, 3,787 copies, 18,024,816 bytes, 1.65 µs CPU/frame. Whole copy after NativeCopy invalidation accounted for 3,390 calls. Shared-generation full copies numbered 111. | Copy bytes are not physical traffic, and capture CPU includes more than the byte copy. Exposed copy wall time before ready averaged 0.0068 µs/frame. |
| 241 live CPU backing | Exact simultaneous peak 4,298,158 B during the full scene. Exact counted-window peak 3,899,400 B. The frame-sampled peak was 3,895,304 B. | A short event high-water can be missed by frame sampling. This counts Lua VBO snapshot byte vectors once per backing allocation. |
| 241 process memory | Engine physical footprint 9.348 GiB and RSS 5.349 GiB at sampled peaks in the full scene. The independent kernel lifetime footprint field reached 9.354 GiB. | Process peaks and snapshot peaks occur at different times. The diagnostic covers the engine and owned process group, with no lobby during gameplay. It does not separately account for GPU allocations. |

At the 241 event peak, current snapshots held 2,839,982 B, detached older snapshots 1,454,080 B, and an unpublished allocation 4,096 B. Early ReclaimField updates dominated the older bytes at that peak. A prior 393,216 B PiP vertex generation coexisted with the current generation in 50 sampled frames in the full scene, including 40 in the counted window. The data records allocations, ownership moves, releases, and frame samples separately. The profiler itself added 5,779,584 B of event storage and 56 B per instrumented wrapper, so its process readings are not a clean app-to-app memory comparison.

Trace the contract from [LuaVBOImpl.cpp](../../../../engine/rts/Lua/LuaVBOImpl.cpp) and [NativeDirtyBufferSnapshot.h](../../../../engine/rts/Rendering/Backend/NativeDirtyBufferSnapshot.h) into [Metal4ScreenNative.mm](../overlay/engine/rts/Rendering/Metal4/Metal4ScreenNative.mm). The 240 sensor writer copied 32 B into a 4 KiB destination twice per frame. The same draw read the new generations, so simply delaying capture does not avoid the required bytes. Older CPU readers may require immutable generations. The CPU snapshot, its prepared PiP draw, and the Metal submission's buffer ownership are separate lifetimes.

The 242 candidate replaces the prepared PiP draw's CPU owner with the cull-derived value used for its metrics. Source review found separate Metal ownership through completion, but all 50 observed pairs already coexisted before this candidate could release a reference. Assuming an immediate release after recording yields **zero full-scene peak reduction** and a **4,096 B** reduction in the counted-window peak in that counterfactual. Neither number is a measured candidate saving. Separate root and target owners remain to be traced. The candidate was held before A/B. An independent check also caught a validation-package overlay that compiled the baseline Screen TU. That package did not launch and cannot qualify the candidate. See the [counterfactual and limits](242-pip-counterfactual.json).

[239 samples](239-list-frames.csv) · [240 samples](240-copy-frames.csv) · [241 backing samples](241-backing-frames.csv) · [241 event counts](241-backing-summary.json)

## How to use the data

Numeric CSV rows preserve the source's population and units. For 238, 240 and 241, positive clock values use an arbitrary origin local to each file and zero retains its missing-timestamp meaning. Do not join across recordings by those clocks or by frame ID. The manifest records each original hash and export hash. Review flags, units and denominators before calculating savings. Separate the app's draw throughput from OS drawable cadence, exact backing high water from sampled process footprint, and GPU envelope from the union of busy GPU intervals.

A useful review should point to exact source paths and a falsifiable change, then name the closest existing counterexample. If the source does not establish a safe removable operation, request the missing measurement. Measure the instrumented behavior first. For any clean A/B, freeze the scene, flags, AA/VSync, display geometry and power, preserve all valid runs, and independently check fixed-scene output, resource ownership and Metal API Validation. Report mean FPS, tail behavior and the scope of memory changes separately.
