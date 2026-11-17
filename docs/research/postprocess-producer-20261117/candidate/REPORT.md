# Exact Postprocess producer/read contract, source-only candidate

Base: `c34129af9158bbdef1321d17825f1f60721b74ec` (R03 + completion-observation split).
Coordination observed: revision 74, DONE/JINI after round 18. No coordination state changed.

## Why this change

Safe multi-submission needs a dependency to identify the *actual* producing submission and retained queue, not merely a cached image with `encoded=true`. Postprocess previously registered whichever predecessor happened to be pending and its encode-time check accepted `encoded || completed`. The new source binds immutable outputs to context/queue/source-submission, retains the queue, and registers only an exact outstanding source submission. A receipt pins the version, immutable producer identity snapshot and completion flags. Mutation invalidates it.

An actual previous-image copy now registers a read at the copy construction site, including non-DOF preservation. Sepia, CAS, DOF sampled inputs and existing external readability callbacks use the same receipt. Overwrite-elided copies do not introduce a fake read.

## Safety and scope

- Same-submission reads still require actual producer encoding at the original operation boundary.
- Pending reads require exact predecessor registration; completion remains required by the existing pre-submit retirement check.
- Unknown/mismatched predecessor or queue drains the existing pending work, then rechecks completion. Uncompleted inputs fail closed. Completed immutable inputs may cross queues on the already-checked Metal device.
- No pending=2 activation, new event wait, new resource reuse, changed submit/retirement order, or changed readback/present behavior. Runtime source is byte-identical outside the new typed registration method and queue accessor/declaration.
- Existing CPU upload path is unchanged. Other subsystems still use their existing conservative gates.
- Volumetric-cloud depth still has its existing ResolveImage path rather than a captured encode-time read receipt. This is NOT a complete proof for all Postprocess consumers or global multi-submission safety.
- No performance improvement is claimed. The candidate adds provenance enforcement, not concurrency.

## Tests and independent review

Run `python3 test_contract.py` from this directory. It compiles the actual portable predicate and extracted production `PreparationInput`, `CaptureRead`, `ConsumerIdentity`, and both runtime registration methods in C++20 stubs (Objective-C bridge cast replaced with a portable pointer cast).

PASS: 64 predicate combinations, 6 invalid identity cases, 14 compiled scenario blocks covering same-submission ordering, exact/wrong/missing predecessor, completion gating, producer mutation, cache removal and version/queue pin lifetime, fallback drain, no fabricated completion, CPU source, missing owner and recording rejection. Source wiring audit and runtime unchanged-outside-registration check pass. Stubs model retirement/Metal ownership and are not GPU tests.

Independent static reviewer found no blocking defect, verified output paths bind source submission first, and identified the cloud-depth coverage limit. API comments now distinguish preparation registration from captured encode-time checks.

Not tested: Objective-C++ SDK compilation, ARC queue capture on Apple runtime, actual Metal/GPU ordering, game visuals, failure/detach paths. Linux CPU tests do not replace these checks. No Mac, build/game/benchmark, HQ request, main/release/Homebrew or STATE changes were performed.

## Suggested next validation (not sent to HQ)

A bounded candidate build and existing round18 screen-save smoke can establish integration/parity. Explicit multi-frame cache lifetime/read sequence coverage should precede any concurrency change. If any new provenance assertion fails, record the exact call path/source identity; do not relax to encoded-only permission. Full pending=2 still requires remaining consumer gates, upload/allocator lifetime and present/readback consumption ownership proof.
