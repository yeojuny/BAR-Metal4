# Owned renderer dispatch receipt

The real ordinary worker dispatch no longer borrows stack references to the
renderer gate handoff, target Begin notice, retirement flag or deferred callback.
A retained dispatch receipt owns those objects; the worker and its nested Begin
callback capture it by value. The previous-retirement callback is executed only
by the main caller after join. Its readiness flag is consumed before invocation,
so a throwing callback is not attempted twice by exception settlement.

Both success and failure release the renderer reservation before main target
notice and then previous-retirement delivery. Duplicate reservation is rejected
without destroying the original reservation. Failure before worker publication
releases the reservation through main-owned destruction. The existing worker
still joins and releases its task reference before returning, so final source
capture destruction remains on main.

Validation: 1000 actual receipt/serial-worker cycles cover pre-Begin/post-Begin
errors, target-before-retirement order, throwing main callback exactly once,
duplicate reserve rejection and main-thread destruction. Reservation recovery
before worker invocation also tested. Existing target notice/gate/owned-world/
source-context/presentation/worker-publication regressions are run separately.
Several old structural assertions referred to pre-snapshot dispatch signatures;
they are updated to check the actual captured path, not removed.

This is not asynchronous admission. Receipt flags still require joined completion
before reading; the object does not own the renderer lifetime. Game queued-result
handling, bounded source credits, exact historical rollback and ordered control
barriers must still be implemented before P5 can run. No native app build or GPU
execution/performance claim.
