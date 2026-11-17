# Completion observation / settlement split: pending=1 unchanged

Base source: R03 `0780bd7c530b0696067b717428b49a416eda556a`.
Runtime .mm blob `866d517d9219a2908f570d68db3d313a2c150bce` and .h blob `77076f0cd65f9d92360fd49e6caae3bb08b25cab` were fetched at the fixed commit and matched the local originals. Latest coordination read was revision 70 DONE/JINI (round17). Parent confirmed HQ termination and cleanup. This experiment is independent of R04 and the texture tracing branch.

## Exact scope

`DrainOldestLocked` calls `AwaitCompletionLocked`, then immediately `SettleCompletedLocked`, under its existing caller serialization. Normal paths retain requestMutex across both phases; the destructor keeps its pre-existing exclusive-use shutdown assumption. Await retains both original blocking waits, in event-then-feedback order with unchanged 30-second timeouts. It records the same metrics and returns a private caller-local, noncopyable observation tied to runtime/ticket/queue/event/value. The receipt is consumed at settlement entry and rejects mismatched or duplicate use. It is not a polling API: the feedback semaphore is consumed once.

Settlement contains the former post-wait statements in precisely the same order, including pending=false before checking feedback error, allocator reset before renderer retirement, receipt publication after successful actions, completed-owner transfer and deque removal. A successful observation means both completion mechanisms finished, NOT GPU success, renderer completion, readback readiness, or display presentation. The outer exception handler and poisoned/quarantine semantics are unchanged. A callback failure must never trigger a second settlement attempt.

No extra shared_ptr owner copies were introduced by the helpers: ticket is passed by const reference and settlement aliases its frame owner. The original DrainOldestLocked frame shared_ptr remains live. Receipt identities are non-owning raw identity values and exist only within the same serialized call; the runtime/ticket retain their objects. No callback-thread execution, asynchronous cleanup queue, polling, thread creation, upload reuse, resource release, changed timeout or second GPU ticket is introduced.

The extra exact-receipt assertion can only reject a broken/mismatched internal call. Normal call timing may include function overhead and assertion cost; there is no performance improvement claim.

## Preserved boundaries

- pending=1 guards, predecessor retirement-before-submit, recording slot selection and unregistered-borrow fail-closed behavior unchanged.
- SubmitAndDefer waitForDrawable/commit/signalDrawable/present/event order byte-identical. Event completion is not physical display completion; present backpressure remains independent.
- Existing completed-owner graph destruction after successor submission stays where it was. This is not a new release optimization.
- Renderer callback capture/lifetime, producer completion and engine recorder acknowledgment order untouched.
- SubmitAndWait, readback DidComplete, continuation publication and PixelsRGBA8 CPU consumption untouched.
- Queue identity, upload/residency/uniform owner lifetime remains the existing single-runtime contract. This extraction does not prove pending=2 safety.
- Early pool drains, shutdown/detach and every caller still use the same DrainOldestLocked wrapper.

## CPU verification actually run

`python3 test_source_and_contract.py`

1. Reconstructs original .mm exactly by inlining extracted wait/settlement statements and deleting only the new helper code. Header reconstruction likewise proves only the private declarations were added. Thus all unmodified present/readback/control code remains byte-identical in these files.
2. Compiles the actual observation definition and exact assertion predicate with C++17 (`-Wall -Wextra -Werror`) against small portable stand-ins. Nine checks cover success, duplicate consumption, wrong runtime/ticket/queue/event/value, preconsumed observation and missing front ticket.
3. Runs 36 original-versus-split state/ordering model cases, combining normal/error at event, feedback, GPU error, allocator, callback, residency, residency receipt and uniform receipt stages with keepResidency and completedOwner variants. Verifies quarantine, unsafe flag and no success handoff on failure.

This is NOT a production Objective-C++ compilation or live concurrency/GPU test. The model is a contract check, not a test of Metal implementations. Trace timing values can differ; original scopes and action order are preserved.

## Required next gate (not executed here)

After coordinator fixes this source/manifest and gives HQ a separate bounded round: compile the actual engine, then validate representative deferred presentation plus early drain, synchronous readback/continuation and shutdown/detach flows. Keep pending limit1 and ensure exact callback/receipt counts, no extra reset/release/CPU read before settlement, normal Metal validation and cleanup. Fault injection, if selected, must be test-only and fixed before the handoff, not ad hoc changes in HQ. A later matched performance check should treat this as a structural equivalence experiment, not expected speedup.

No Mac/HQ access, production compile, GPU helper, game, benchmark, main/release/Homebrew change or coordination ownership handoff was performed. AI-assisted patch; CPU checks and independent source review are the only completed verification.

## Independent review

A separate read-only reviewer found no concrete order/lifetime/type defect. Its mutex-comment correction was incorporated: shutdown already calls the drain without acquiring requestMutex, so the patch preserves existing serialization rather than claiming every call holds that mutex. No independent production compilation was performed.
