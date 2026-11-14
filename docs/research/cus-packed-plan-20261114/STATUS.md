# CUS static uniform packing reuse, 2026-11-14

AI-assisted bounded follow-up to public `a33ebfe888bc8abe0053df05e5e2892c0788d6a8`.
No game, benchmark, Mac application build, deployment, or main change performed.

## Checklist

- [x] Check latest public branch and preserve previous checkout/vendor changes.
- [x] Add one renderer-owned packing plan per exact linked vertex/fragment stage.
- [x] Cache validated/converted static uniform bytes by exact immutable owner.
- [x] Keep dynamic image transport and water values per draw.
- [x] Preserve distinct aligned GPU upload slices and existing readiness/retention.
- [x] Host ASan/UBSan tests and independent source review.
- [ ] Mac Objective-C++ compilation and game/visual verification (not run).
- [ ] Actual workload hit rate and CPU/FPS measurement (not run).
- [ ] Terrain common/per-square preparation split (separate next work).

## Scope and bounds

A linked Program owns a vertex and fragment `NativeCUSProgramUniformPlan`.
On an exact uniform owner hit, ordinary name lookups, shape/finite/range checks,
scalar conversion and packing are skipped. The already-packed zero-padded static
block is copied to the original distinct upload slice. Dynamic image masks,
sample codes, clip flags and water values are packed afresh.

The plan owns its reflected names/layout and is not keyed by a mutable source or
raw address alone. Its cache key is weak control-block identity **and** object
address. It retains numeric packed bytes, not source frame/phase/texture owners.
It is a bounded one-entry cache: A -> B -> A rebuilds A. It is used under the
existing serialized renderer gate; this does not introduce concurrent access.
Overlapping reflected fields fall back to original ordered packing. Failed static
packing does not publish a new cache key/template. Dynamic validation remains on
every draw. Per-draw full source validation is deliberately still present.

This does not reuse GPU upload storage, change draw order, remove readiness checks,
change shader math, enable P5, or change GPU submission depth.

## Verification

`run_native_cus_uniform_plan_test.py`: **PASS**, g++ C++20 ASan/UBSan,
-Wall -Wextra -Werror. 80 production generated stages, 1,920 byte comparisons
against generic packing, 160 static builds and 1,760 reuse hits in the synthetic
fixture. Counts prove exercised reuse, **not** workload hit rate or speedup.
Additional tests cover signed zero/padding, dynamic changes, equal values/new
owner, A/B/A, owner release, same address/different control block, aliasing,
missing/NaN-infinity/nonintegral values, failure recovery, destination size,
overlapping reflection and owned ABI/name storage.

Existing `run_native_cus_uniform_content_test.py`: **PASS**.
Independent source review found a `const Program&` integration issue; corrected
before final tests. No other issue found in its bounded review. This is not a
Mac compile or GPU proof. Production and fixture hashes: `host-receipt.json`.
