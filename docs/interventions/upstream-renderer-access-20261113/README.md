# Explicit renderer CPU cache/control ownership

Added a per-renderer fail-fast access gate to actual cache/control, compilation,
allocation, capture-publication, query and rendering entry points. Eleven retained
provider/readiness closures and the deferred world retirement callback also enter
the same ownership domain. The StartBox provider retains the stable Impl pointer
rather than the movable wrapper's `this` pointer.

The gate uses recursive try-lock ownership: same-thread nested calls are valid
because RuntimeLease retirement can call a previous frame completion from inside
new preparation. Another thread is rejected, never waited on. This is important:
a completion can already hold the runtime request mutex, while another entry may
be about to acquire it. A blocking renderer lock would introduce lock inversion.

DrawDispatch protects its main-side cache/policy snapshot but releases ownership
before direct DrawPrepared or worker.Invoke. DrawPrepared/maintenance/readback/
continuation acquire their own scopes. No source callback is moved onto a worker.
Noexcept cache availability queries fail closed. Noexcept aborts report failure;
SceneBridge preserves source owners when abort cannot enter. HasScreenContinuation
now throws on conflicting access rather than returning a misleading false route.

Deferred completion acquires ownership before its first mutation. If rejected,
Runtime::DrainOldestLocked retains the pending ticket, poisons the runtime and
throws; pop_front and completion delivery do not occur. This is fail-closed
quarantine, not successful retirement or retry permission.

Validation: actual production CPU gate and serial worker under ASan/UBSan,
1,000 nested/error/recovery cycles, 100 cross-thread rejection/recovery cycles,
a deterministic runtime-lock inversion scenario and main-to-worker handoff.
Structural checks cover the actual Objective-C++ entry/provider scopes, dispatch
release and runtime quarantine ordering. Independent reviewer rebuilt tests and
reviewed the wiring: GO. These are not Objective-C++/Metal runtime tests.

Boundaries: renderer lifetime remains externally owned. Concurrent destroy/move
is forbidden, as before. Same-thread recursion is needed for providers/retirement;
it does not authorize destructive controls in arbitrary renderer callbacks.
Source registries and shared engine services outside this renderer are not made
concurrent. In-place GPU caches retain their existing retirement barriers. This
change detects conflicting CPU use, not safe asynchronous lookahead, queued
control scheduling, GPU-resource renaming, or complete source rollback dependency
handling. Gate/provider overhead and real Metal behavior still need integrated
Mac measurement; no performance gain is claimed.

Post-change regressions pass: actual cube and shield cancellation suites, worker
allocation-fault/negative-control suite, and 3,000 Begin-receipt lifecycle cases.
Full SceneBridge host C++ syntax also passes using official SDL 3.2.24 headers
(restored separately after the cloud workspace replacement); no app build or
installation was performed. This does not compile the Objective-C++ renderer.
