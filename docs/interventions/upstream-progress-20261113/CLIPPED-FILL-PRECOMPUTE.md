# Bounded main-overlay pure clipping precompute (opt-in)

## Scope and checklist
- [x] Connect an optional retained-source worker across an existing preparation interval.
- [x] Keep original validation, target checks and Find -> TryBuild -> Borrow selection at demand.
- [x] Keep main/water/ground/shadow dispatch and shared renderer/cache/provider ownership unchanged.
- [ ] Native Objective-C++ compilation, platform floating-point environment checks and visual/error parity.
- [ ] Equal-work timing, CPU overlap, latency, allocation and memory measurements.

`BAR_NATIVE_CLIPPED_FILL_PRECOMPUTE=1` enables this source pilot. The default is off.
It is independent of the existing Begin-ready/CPURead options; it does not widen
frame admission, GPU pending depth or remove a drain. It is not a claim of improved
FPS or completion of general P5 rollback/eligibility work.

## Actual independent interval
The optional job is created immediately after NativeWorldDrawPlan167 in
Metal4WorldRenderer. It scans the retained raw source frame, not selection
iterators or their mutable metrics, and admits only main-stage closed plain
clipped fills. The original Map MRT, opaque/alpha models, planar-view models and
screen preparation remain on the caller in their original order. Only the main
overlay receives this invocation's receipt. Water/ground/shadow calls do not.

At the original geometry fallback, the caller still creates its owned task and
performs the full original validation/target seal. Only a successful seal permits
an exact source-owner/control-block/command-address receipt lookup. A missing,
failed, cancelled, consumed or mismatching receipt takes the existing synchronous
Execute/Take path. Speculation cannot surface a geometry error before its original
demand. It does not skip renderer cache or typed provider operations.

## Bounded ownership and cancellation
The job keeps one immutable frame owner. Defaults cap selection at 8 commands,
65,536 input indices, 8 MiB conservative logical output and 4,096 scanned commands.
The byte cap counts maximum mesh payload, not vector capacity, temporary polygon
allocations, thread stack or total process heap. The speculative allocations and
retention are new; allocation failure falls back but identical process-wide OOM
behavior is not promised under changed memory pressure.

The executor serializes Pending -> Running versus Pending -> Cancelled under its
mutex. Running work is not forcibly interrupted. Every exceptional/normal scope
exit cancels and joins before destroying sources/results; no detach is used.
Optional setup, dispatch and kernel failures do not become new renderer failures.
No successful worker result is read before join. The caller alone consumes results
and controls the job, while the worker only calls the pure validated-span kernel.

## Numeric semantics and evidence limits
The speculative preflight is only a memory-safety check, not an early full command
validator. The original full command/target validation remains at demand. The
worker must preserve the caller's numeric control environment and avoid delivering
floating-point traps on its own thread. Result consumption conservatively rejects
an incompatible caller environment and restores arithmetic exception effects at
the original demand boundary. Native platform verification remains required;
host tests cannot establish Metal or macOS FP behavior.

The accompanying host tests link the actual production clipping CPP, not a mock
kernel. Renderer selection tests use the extracted production decision block with
mock cache/provider operations. They do not compile Objective-C++, run Metal, or
measure positive overlap. The source interval is genuinely independent, but its
usefulness depends on captured workloads containing enough eligible clipping work.

Numeric implementation details: early finite checks inspect IEEE float bits, so
signaling NaNs cannot raise an exception before demand. The worker installs the
captured C floating-point environment and uses `feholdexcept` to avoid delivering
worker-thread traps; each successful result records standard `FE_ALL_EXCEPT`
flags. Consumption first requires a conservative byte-equal zero-initialized C
environment snapshot and matching rounding, then raises recorded standard flags
on the caller. Environment representation differences safely reject reuse. This
is not a proof of nonstandard architecture status bits or exact trap-instruction
behavior on macOS; those remain acceptance gates before enabling the pilot.

Host verification additionally covers an observed Running worker blocked in its
first allocation while caller cancellation joins, without production test hooks.
Rejected invalid candidates still consume the validation-index budget, preventing
a bounded candidate count from hiding an unbounded rejected-input preflight.

## Recorded host verification (2026-11-13)
- [x] `run_clipped_fill_precompute_test.py`: actual production clipping kernel,
  exact owner/control-block identity, optional constructor/dispatch/worker failures,
  bounded admission, caller cancellation of an observed Running worker, four
  rounding modes, standard exception flags and 100 owner-release teardown cycles.
- [x] `run_overlay_selection_boundary_test.py`: extracted production fallback
  ordering, demand-seal-before-reuse and exact-target gates; renderer launch/consume
  placement and single main-overlay consumer source checks.
- [x] `run_clipped_world_fill_test.py`: 4,000 byte-identical reference cases and
  allocation-order fault parity against the production pure kernel.
- [x] `run_owned_world_geometry_test.py`: production owned-task header with mock
  builder, exact source/target, state, exclusions, seal failures and joined lifetime.
- [x] Independent read-only review and independent rerun of precompute/selection
  tests: no blocking source correctness finding in this narrow default-off scope.

These host runners use ASan/UBSan with leak detection disabled for the host
runner environment. No full app build or app execution was performed. The
non-allocating environment-flag comparison keeps pilot discovery from introducing
an allocation failure even when an unexpected long environment value is present.
Exact floating-point environment matching can reject otherwise useful results
after intervening preparation changes status flags; no reuse-rate or speed claim
is made. Native Objective-C++/Metal and platform numeric acceptance remain open.
