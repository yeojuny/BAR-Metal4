# Model provider cost: live-clock repair and bounded attribution

## Status and purpose

This is a **diagnostic source correction, not a demonstrated FPS optimization**.
It changes neither the model-type-facts policy nor any GPU resource/readiness
contract. No engine release, launcher policy, Homebrew formula or saved profile
is changed. Native macOS compilation and runtime acceptance remain pending.

The code was inspected at `57b850d65450ededf46daf55fec615c9de36c901` (PR #4's
integrated P/Q/S source). The patch is based on its documentation-only successor
`9dfc2eb31625f016fc37c8f92858b638bccdf83f`, retaining the subsequently published
experiment evidence. The two modified production files have identical original
blob hashes at both revisions.

## Why this patch comes before another cache

The preserved S result demonstrates 410 omitted RTTI operations per sampled
job, not 410 avoided image preparations. Its clean result is OFF 184.902314 and
ON 184.618023 FPS, with no established gain. It explicitly reports that the
provider wall clock was unavailable and the diagnostic CPU comparison was not
valid across the differing power observations. Do not reinterpret those timings
as a valid before/after comparison.

The selected `OriginalModelImage` region contains 265 of 942 CUS preparation
samples, but that is inclusive statistical sampling, not the whole-frame time
saved by removing type tests. The expensive portion still needs to be located.

Sources retained by the parent checkpoint:

- [S result](../../checkpoints/native-shared-material-20261115/evidence/S/RESULT.json)
- [R selection](../../checkpoints/native-shared-material-20261115/evidence/R/SELECTION.json)
- [R profile breakdown](../../checkpoints/native-shared-material-20261115/evidence/R/outputs/offline-profile-p2/ANALYSIS.json)

## Verified defect

`Metal4ModelTypeFacts.inc::ProviderCall::Now()` used the private renderer `Clock`.
In a normal `RECOIL_METAL4_LIVE_SCENE` build, `NativeCPUTimingInstrumentation` is
false and that clock intentionally returns `{}`. Enabling the bounded runtime
trace does not turn this compile-time clock on. Thus request/cast counters could
work while provider wall duration remained zero.

Use `std::chrono::steady_clock::now()` directly **only inside the existing sampled
provider diagnostics**. Do not turn on renderer-wide CPU/GPU timing, change
simulation time, or use the disabled renderer clock for this sampled measurement.

## New attribution boundary

`OriginalModelImage()` already calls `ProviderCall::Positive()` once after it
has established a model backing and validated retained pixels, and before
`Texture()`, sampler preparation, registration and readability-wrapper creation.
The existing call is now also a diagnostic midpoint. No call site or provider
return value is changed.

For each completed model call:

```text
provider start -> positive model dispatch complete -> provider return
               dispatch envelope                  preparation envelope
```

The dispatch envelope includes positive-fact lookup, original RTTI/validation and
proof insertion. It is **not pure RTTI time**. The preparation envelope includes
actual model image preparation and any nested CPU/GPU waits; it is **not pure GPU
work**. Other successful providers and failed calls are recorded separately.
Failure duration is counted once, never also added to the successful model split.

Wall and thread-CPU sums each partition exactly when their stamps are valid.
Invalid/missing/backward CPU stamps, backward wall stamps and duplicate midpoints
are reported, not converted to zero work. Scope mismatches remain explicit.
Thread CPU excludes blocked/descheduled time but does not identify where active
slices occurred inside the wall envelope.

No readiness callback is invoked for observation. No texture, sampler, backing,
lease or completion owner is retained by the added diagnostic fields. No binding
reuse, producer bypass, wait removal, queue-depth change or MRT209 change is made.

## Trace format

Existing kind numbers 1-35 are unchanged. A local summary parent identifies the
counter group even if one job contains several provider scopes. The runtime table
is already sized from `NativeCPUPhaseKind::Count`, now 43; the per-job record
capacity remains 128. A scope emits 13 records instead of 6, not one record per
provider call. Existing sampled/window controls and post-Join buffer emission
remain in effect.

| Kind | `count_a` | `count_b` |
|---|---|---|
| 30-33 | Existing request/type/cache counters | Existing meaning |
| 34 | Total valid provider wall ns | Total valid provider thread-CPU ns |
| 35 | Calls with invalid CPU timing | Execution-scope mismatches |
| 36 | Provider requests in this scope | Completed model + other calls |
| 37 | Completed model calls | Completed other calls |
| 38 | Failed provider calls | Calls with invalid wall timing |
| 39 | Successful model dispatch wall ns | Successful model dispatch CPU ns |
| 40 | Successful model preparation wall ns | Successful model preparation CPU ns |
| 41 | Other successful provider wall ns | Other successful provider CPU ns |
| 42 | Failed call wall ns | Failed call CPU ns |

For these counters, `total_us`, `start_ns/end_ns` and `thread_cpu_ns` describe
**summary emission**, not the earlier provider calls. Analyze `count_a/count_b`.
The new reader validates the summary parent, exact job/lease identity, counts,
partition totals, clock validity and recorded drops. It refuses partial or
failed groups rather than silently selecting convenient rows. A completely lost
group cannot be inferred from its absence: retain/check the existing buffer
manifests, window coverage and run receipts as well.

## Host validation

Run from the repository root:

```sh
python3 engine/tools/metal4-contracts/run_model_provider_cost_test.py
```

This compiles the actual `.inc`, actual positive-facts implementation and actual
trace formatter using a small modeled Metal lease/trace sink/thread-CPU clock.
It checks OFF and ON separately, 1,000 model-call partition cycles per mode,
exceptions before/after model dispatch, invalid stamps, scope mismatch, duplicate
midpoints, weak backing lifetime, and no thread-CPU reads with diagnostics off.
It also tests the parser/CLI and 24 corrupted-log variants. A negative control
replaces only the fixed clock with the historical zero clock; its regression
assertion must fail.

Executed here with Clang 17 on Linux, C++20, ASan + UBSan: PASS. The normal
Clang compile also passed `-Wall -Wextra -Werror` (the existing compact source
style uses `-Wno-misleading-indentation`). This is not an Objective-C++ engine
build, Apple Metal test, real-game benchmark, or pixel-equivalence result.

## Native experiment instructions

1. Build a separate private candidate from this branch using the recorded native
   toolchain. Preserve the shipped app and the unmodified P/Q/S baseline. Record
   source and signed engine hashes, child environment, scene and power policy.
2. Keep PREPARE=1, BEGIN=0, worker=1, CUS prototype=0, Sepia off and the same existing
   scene/preset. For diagnostic comparison, use the same small sim300..306 trace
   window, `BAR_NATIVE_FRAME_TRACE_EVERY_LOOP=1`, and identical Validation/power/
   display conditions. Set `BAR_NATIVE_MODEL_TYPE_FACTS` to the desired comparison
   value in the **actual native child**. The public outer launcher clears pilot
   variables; exports before that wrapper are not evidence of effective mode.
3. Preserve all run/foreground/power/exit receipts. Parse each process separately:

   ```sh
   python3 engine/tools/metal4-contracts/analyze_model_provider_cost.py \
     /path/to/off/infolog.txt /path/to/on/infolog.txt --output provider-cost.json
   ```

4. Read `model_dispatch`, `model_preparation` and `other_success` separately. The
   reader does not pool different runs or infer speedups. Diagnostics add observer
   work, including a midpoint CPU-clock read on each model call. They are not a
   substitute for clean, trace-OFF repeated FPS runs.
5. Correlate the existing executing job identity with main Join and retirement
   records before calling any provider time removable critical-path time. Never
   add provider time to its containing Prepare/worker/Join interval.

The immediate question is whether the tested type-facts optimization materially
reduces the dispatch envelope, whether the remaining cost is actual model
preparation or other families, and whether any saving reaches the exposed job
completion. This patch does not establish repeated full binding inputs or their
completion state. Those remain prerequisites before broader provider-result reuse.

## Decision after the trace

- Small dispatch / large model preparation: stop optimizing cast counts; inspect
  actual texture lookup, sampler preparation, registration and wrapper costs.
- Large other-provider envelope: identify that family before redesigning S3O.
- CPU time decreases but exposed completion does not: inspect the containing
  dependency/retirement/other CPU path, not additional type-cache machinery.
- A large repeated completed input is eventually demonstrated: implement one
  narrow preparation-result reuse with exact lifetime/order proof, then clean A/B.

No FPS projection or performance adoption is claimed by this commit.
