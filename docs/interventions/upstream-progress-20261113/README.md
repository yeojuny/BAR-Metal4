# Upstream intervention status (2026-11-13)

This is a source implementation ledger, **not a runtime acceptance report or a completion percentage**. Main and the installed application remain unchanged. Work targets `refactor/upstream-terrain-t1-20261112`.

| Handoff area | Source status | Still required |
|---|---|---|
| P0 reproducible control | Prior baseline records retained | Verify actual Mac app/flags/toolchain and capture control measurements |
| P1-A T1–T4 terrain | Exact-change, immutable linkage, lazy publication, fixed slots and typed source base implemented | Integrated Mac compilation, visual/error parity and allocation/time measurement; late dynamic packing remains intentional |
| P1-B postprocess | Snapshot graph index implemented | Integrated workload validation |
| P1-C CUS roster | Immutable ordered roster implemented | Integrated workload validation |
| P2 payload/receipts | Uniform content, manifest/partition and exact receipt sharing implemented | Audit remaining per-phase copies; do not claim all payload transport eliminated |
| P3 owned frames | Owned screen storage and normal world/options/screen dispatch packet implemented before journal transfer | Transitive source-family seal validation and ownership audit; no asynchronous queue lifetime claim |
| P3 journals | UI/postprocess/cloud exact 2-slot held-frame ledger, epoch completion mailbox and one-shot callback replay tickets and bounded pre-submit completion reservations and main acceptance gating implemented | Ordinary synchronous UI/post/cloud pre-submit transfer, exact held Begin routing, no-Begin restore and begun-failure quarantine implemented; exact shadow/MRT/shield/cube/refraction abort guards implemented; younger whole-world dependency ownership remains incomplete |
| P4 input separation | Presentation policy, preunit environment receipt and immutable PBR declaration batches isolated; ordinary worker Begin notices applied on main before success/error receipts (`fe28d8a`) | Exact cube/shield abort receipts and invocation read identity implemented; live registries/services and remaining mutable caches; height/normal pair storage snapshot-owned, complete water selection and per-role pairs pinned, main/cropped-depth storage bounded idle with exact frame pins; original retirement barriers retained |
| P4 render owner | Renderer-owned joined worker, cache gate, reserved handoff and ordered water GPU reset controls implemented | Broader controls/provider teardown, shared source services and full concurrent-owner isolation |
| P5 CPU lookahead | **Not enabled** | All ownership gates, bounded credits and actual positive active-work overlap evidence |
| P6 GPU optimizations | **Not claimed** | Complete resource-use proofs and Metal measurements before enabling changes |

Host tests exercise actual selected production C++ components with ASan/UBSan; the completion mailbox also passed TSan concurrency tests. The integrated SceneBridge C++ translation unit passed host syntax checking; structural wiring checks and host C++ checks are not Objective-C++ compilation. No Mac build, Metal execution, visual parity or FPS gain is claimed. Snapshot typed-cache storage increases by 992 bytes per immutable owner; runtime tradeoffs remain unmeasured.

Ordinary synchronous UI/postprocess/cloud admission is implemented; the remaining source work is extending proven ownership across the whole world and shield dependencies. The complete design must: reserve all journal/completion slots and validate every owner before transferring UI/postprocess/cloud journals and publishing work. Preserve before-Begin rollback or pending restoration, durable begun evidence, and begun-failure quarantine without successful retirement. The ordinary worker Begin-notice seam remains synchronous: main waits, then applies notices on both return and exception before existing receipts. Direct readback prefixes retain their immediate main-thread routing; neither path supplies future frame admission or lookahead. Exact cube cancellation is implemented as a prerequisite, not a dependency-aware cancellation journal. Exact serial shield/MRT/shadow/refraction ownership and execution-aware cancellation are implemented; a younger-capture dependency/version graph is still required before overlapping capture with older rendering. Existing synchronous execution remains the reference until ownership gates are met. Full application build/restart is deferred to the integrated candidate as requested.

Latest P3/P4 source checkpoint: [normal producer resource/epoch ownership](NORMAL-PRODUCER-EPOCH.md).
Three map-normal consumers, the existing producer and retirement now pin the same
physical resource and readiness epoch. This does not close the remaining whole-world
ownership audit or change P5 default/Metal acceptance requirements.

Shared-service audit checkpoint: [MetalInfo resource generation](METAL-INFO-GENERATION.md)
pins prepared completion to its exact target/source-cache generation while retaining
the pipeline cache. WorldInfo already had a generation boundary; P5 remains gated.

P5 source extension: [optional sealed water-model reads](OPTIONAL-WATER-CPU-READ.md)
adds only already-captured model read rows to existing opt-in mode2/3, with separate
speculative failure rejection. Particle/alpha/water/Lua capture drain is unchanged.

P6 source bookkeeping: [Grass/ResourceSpots owner rosters](EFFECT-OWNER-ROSTERS.md)
seals the exact first-owner indices once during preparation. Per-draw readability,
phase callbacks, resource order, GPU barriers and default P5 remain unchanged.

P6 grouped-model checkpoint: [Deferred owner roster](DEFERRED-OWNER-ROSTER.md)
seals one global before/after first-owner census, preserving null exclusion,
phase-live callbacks and grouped/ordered completion rules. Native acceptance remains pending.

P5 evidence instrumentation: [CPU-read worker timing](CPU-READ-WORKER-TIMING.md)
adds real worker wall endpoints and valid per-thread CPU totals to the opt-in trace.
Elapsed wall-minus-tail time is not labeled actual CPU execution or overlap proof.

CPU preparation checkpoint: [shadow rigid proof reuse](SHADOW-RIGID-PROOF.md)
reuses positive geometry classification from the exact immutable captured model.
Per-draw guards and unsupported/unclassified detailed checks remain unchanged.
