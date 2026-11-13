# Bounded owned task and Begin-ready handshake

`NativeSerialRenderWorker::Invoke` now uses real Enqueue + explicit Join internally.
There is still one worker and one task credit. The credit includes construction,
queued/running work and completed-but-unjoined work. A rejected enqueue does not
move the caller's capture. A construction guard restores credit after all partial
owners unwind when any allocation fails; publication itself uses no-throw moves.

The main-owned task retains its packaged callable and future. Join waits beyond
future readiness until the worker releases its callable. It drops the future
before the final callable, releasing captured source owners on the submitting
thread before returning credit. Wrong-thread Join/readiness is rejected, and
wrong-thread destruction fails closed. A ticket survives worker destruction via
shared synchronization state without retaining the worker thread. An abandoned
unjoined ticket drains resources but terminally closes admission: discard cannot
acknowledge success or silently reopen work after an unobserved error.

Dispatch adds a distinct acquire/release Begin-ready signal AFTER the exact
held-target notice and active source setup. The earlier actual Begin marker is
preserved as the failure classifier. A worker exit signal handles no-Begin and
error paths. Retirement-ready is atomic because its runtime notification can
arrive after Begin. Neither future-ready nor worker-finished permits gate release
without Join. Target notice can be read after Begin-ready; retirement settlement
and reservation release still require the actual join barrier.

Production Game/SceneBridge remains joined. No queued return, source capture
credit accounting, or frame overlap is enabled by this checkpoint. The next
integration must distinguish queued from presented and explicitly join CPU work
before GPU retirement/control/reset/readback. No hidden source callback waiting
under RuntimeLease is allowed.

Validation: independent ASan/UBSan runs of 1,000 task credit/ownership cycles,
foreign Join/readiness rejection, discard failure closure, worker-before-ticket
teardown, blocked encode Begin-ready delivery, pre/post-Begin failure handshake.
Five allocation failure positions and 1,000 exception/recovery pairs passed;
the original busy-before-allocation negative control still fails recovery.
Actual dispatch/gate/control and owned world packet regression tests passed.
Native Objective-C++/Metal execution and actual P5 performance are unverified.
