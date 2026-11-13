# Upstream intervention status (2026-11-13)

This is a source implementation ledger, **not a runtime acceptance report or a completion percentage**. Main and the installed application remain unchanged. Work targets `refactor/upstream-terrain-t1-20261112`.

| Handoff area | Source status | Still required |
|---|---|---|
| P0 reproducible control | Prior baseline records retained | Verify actual Mac app/flags/toolchain and capture control measurements |
| P1-A T1–T4 terrain | Exact-change, immutable linkage, lazy publication, fixed slots and typed source base implemented | Integrated Mac compilation, visual/error parity and allocation/time measurement; late dynamic packing remains intentional |
| P1-B postprocess | Snapshot graph index implemented | Integrated workload validation |
| P1-C CUS roster | Immutable ordered roster implemented | Integrated workload validation |
| P2 payload/receipts | Uniform content, manifest/partition and exact receipt sharing implemented | Audit remaining per-phase copies; do not claim all payload transport eliminated |
| P3 owned frames | Owned screen storage implemented | Complete combined submission packet, source-family seal validation and ownership audit |
| P3 journals | UI/postprocess/cloud exact 2-slot held-frame ledger implemented | Pre-submit admission, epoch-aware notices and failure/quarantine lifecycle |
| P4 input separation | Presentation policy and preunit environment receipt isolated | PBR/shield staging, allocation services, live registries/read proofs and global UI submission writes |
| P4 render owner | Existing serial worker retained with synchronous wait | Exclusive backend ownership and ordered controls, including teardown |
| P5 CPU lookahead | **Not enabled** | All ownership gates, bounded credits and actual positive active-work overlap evidence |
| P6 GPU optimizations | **Not claimed** | Complete resource-use proofs and Metal measurements before enabling changes |

Host tests exercise actual selected production C++ components with ASan/UBSan; structural wiring checks are not Objective-C++ compilation. No Mac build, Metal execution, visual parity or FPS gain is claimed. Snapshot typed-cache storage increases by 992 bytes per immutable owner; runtime tradeoffs remain unmeasured.

The next source work is the completion mailbox and immutable PBR declaration cutoff. Existing synchronous execution remains the reference until ownership gates are met. Full application build/restart is deferred to the integrated candidate as requested.
