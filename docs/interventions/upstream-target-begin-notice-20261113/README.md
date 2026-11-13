# Owned target Begin notice

AI-assisted bounded synchronous worker intervention. No async/lookahead, app
build or Mac runtime verification.

Ordinary DrawPrepared no longer accesses the global UI recorder to mark target
submission. DrawDispatch supplies an explicit sink. The direct path invokes the
original recorder update immediately at the same post-Begin boundary, before
source callbacks. Readback/continuation/callback-bearing paths remain on main.

The serial-worker path owns a per-call copy of exact target-pass entries and a
retained postprocess frame before dispatch. Begin writes a nonzero submission
identity without allocation. After Invoke joins, main delivers the notice via
the existing noexcept recorder update before prior-frame receipts, on BOTH
normal and exception exits. Delivery is idempotent, including a later exception
from prior-frame receipt processing. No Begin means no publication marking.
Begun failures retain the original poisoning/rollback exclusion behavior.

The journal is not a concurrent mailbox: its safety relies on Invoke joining
before main reads and on no concurrent next-frame recorder mutation. Future
admission must target an exact admitted-frame handle, not the active recorder.
This does not close the shield transaction or reservation migration.

Verification command:
`python engine/tools/metal4-contracts/run_native_target_submission_notice_test.py`

Actual journal/serial worker ASan/UBSan tests cover owned payload lifetime,
pre-Begin failure, post-Begin failure, completion failure, prior-receipt failure,
idempotence, and worker-thread marking/main-thread delivery. Structural checks
verify production dispatch success/catch ordering and the post-Begin location.
These tests do not compile Objective-C++ or validate Metal execution.
