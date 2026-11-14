# Reserved worker admission: rejected-observer race

Base: 94ab461abea4f34a06c306233b0ae1c19e746ae0. Scope: one production header, host regression tests and documentation. Existing app binaries and pilot defaults are unchanged.

## Completed
- [x] Replace only reserved-worker mutex try_lock with lock, after an atomic token admission claim.
- [x] Preserve nonblocking ordinary/Try access and exact reservation/admitted checks.
- [x] Concurrent or nested same-token admission still rejects immediately. Sequential token reuse remains permitted, as before.
- [x] Lock/validation exceptions restore the claim; scope destruction clears admitted, unlocks, then releases the claim.
- [x] Controlled source-mutex contention, duplicate admission, sequential reuse and runtime-lock inversion regressions.
- [x] Existing gate and PrepareReady source-contract suites pass on Apple Clang/Mac host.
- [x] Independent read-only call-site/lock-order review: no blocker found in the current dispatch paths.

## Reproduction and verification
The runtime audit appendix A stress was adapted to return failure if *any* handoff conflict occurs, keeping 20,000 iterations and production receipt/settlement/worker/gate headers. The same test was compiled at -O2 on the same Mac against original and fixed headers:

| Headers | Iterations | Rejected probes | Handoff conflicts | Successes | Other errors |
|---|---:|---:|---:|---:|---:|
| Original 13cd5d6 | 20000 | 23399469 | 11755 | 8245 | 0 |
| Fixed candidate | 20000 | 7989718 | 0 | 20000 | 0 |

This stress enlarges a race window and is not a gameplay failure rate. The original-header test exits 1 under the new zero-conflict success criterion; fixed exits 0. A separate ASan/UBSan stress run also passed 20,000/20,000 with zero conflicts and 247,819,575 rejected probes. The gate suite uses ASan/UBSan. The separate probe runner also supports repeatable bounded ASan/UBSan execution with CXX selection.

The controlled test holds the public source Scope across reservation and starts worker admission, then releases source before joining. It verifies waiting rather than false failure, fast rejection of a duplicate admission while owned, and sequential reuse. Existing exception/cancellation/nesting tests remain. The runtime inversion test holds the runtime mutex while a Try probe rejects the worker gate; the observer does not wait, allowing the worker to proceed.

## Lock-order argument
DrawDispatch, ClearWaterFamily, and InvokeNativeRendererControl release source gate ownership before dispatch/join. Worker handoff admission occurs before RuntimeLease/request-lock acquisition. The serial worker releases its queue mutex before invoking the callable. Ordinary and Try scopes remain fail-fast, including runtime callbacks. Thus this patch adds no runtime-lock-to-blocking-gate edge in these callers. Lifetime/token destruction still requires external serialization and Join; this is not a new generalized concurrency API.

## Not established
- [ ] Full updated engine Mac build/link.
- [ ] Native game crash no longer occurs; actual Mac crash stack was not available.
- [ ] Performance recovery or visual equivalence.

No game, benchmark, app replacement or pilot activation was performed. Existing OFF optimizations, MRT/COW policy, prefix GPU drains, source credits and settlement semantics are unchanged. This fixes the demonstrated admission race, not the observed approximately 42% pilot regression. Eligibility/wait/MRT instrumentation and controlled runtime attribution remain separate work.
