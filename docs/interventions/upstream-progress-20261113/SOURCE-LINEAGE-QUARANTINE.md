# P5: failed ancestor and younger capture boundary

## Implemented source boundary
- [x] Source credits carry monotonic acquisition ordinals, independent of simulation/draw numbering. The existing two-source budget and exact pool identity remain.
- [x] Exact transferred/Begun scene failure marks its credit lineage quarantined before family journals are quarantined.
- [x] The failed credit and already-acquired younger credits cannot Validate, Hold or pass success-retirement validation. Future Acquire/HasCapacity calls reject rather than retrying a poisoned source generation.
- [x] Held owners remain retained. A genuinely older successful receipt remains valid; a younger failure does not rewrite earlier history.
- [x] Current younger source fields are never overwritten by an old restore. Pre-Begin restore still follows its existing exact-destination checks and does not quarantine the lineage.

This is a fail-closed dependency boundary, **not automatic whole-world rollback**. Source state is conservatively assumed to depend on earlier captures. After a Begun failure, restart is required; no dependency-safe recovery, GPU success, allocator reuse, or wider provider eligibility is inferred. A fresh scene owns a fresh credit identity, rejecting old-scene tokens.

## Validation
Actual source-credit class: 3,000 host cycles covering the existing budget/ownership behavior plus decreasing draw IDs, old failure/younger invalidation, held-owner retention, repeated quarantine, future admission rejection, scene identity isolation and older-success preservation. Extracted actual ScenePendingAdmission tests include a queued older failure while younger pending source state exists; it remains untouched and cannot be admitted. Source-order assertions ensure lineage invalidation precedes journal quarantine.

The existing Begin-ready pilot regression also passes (15 eligibility gates and 100 real worker handshake cycles). Independent source review passed. Coverage is admission/CPU-settlement failure; a later asynchronous GPU error after CPU acceptance remains the existing runtime terminal path, not newly generalized recovery.

Host ASan/UBSan tests use modeled engine adapters, not a live Metal renderer. Full app build/run remains deferred. Native pause-worker/capture/abort and error/timeout integration still need validation.

## Still incomplete
- [ ] General source/provider eligibility seal for unrestricted asynchronous preparation.
- [ ] Dependency-safe cancellation/rollback/rebuild of complete younger world state, rather than terminal quarantine.
- [ ] Native compilation, actual active CPU overlap and equal-work latency/performance acceptance.
