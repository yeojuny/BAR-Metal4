# Original handoff scope reconciliation

Source: `docs/research/BAR_Native_Metal4_Upstream_Refactor_Handoff_20261112.md`
on GitHub main, blob `f3d256edc12bec6964065095d4e409412eb88a87` (read 2026-11-13).
This branch predates that document; the original research packet is not overwritten.

The eight implementation areas are P1-A, P1-B, P1-C, P2, P3, P4, P5 and P6.
P0 is an additional baseline prerequisite, not evidence of optimization completion.

| Original area | Current evidence / boundary | Completion still required |
|---|---|---|
| P1-A Terrain T1–T4 | Exact-change fast path, linkage certificates, lazy observation and typed source base have source implementations/tests | Integrated compilation, original counter/error/visual parity and equal-work measurements |
| P1-B Postprocess index | Exact-snapshot indexed graph implemented | Full prefix/suffix/readback workload verification |
| P1-C Immutable CUS roster | Shared ordered roster and exact owner proofs implemented | Integrated mutation/old-draw parity and allocation measurement |
| P2 Payload/receipt sharing | Uniform, manifest, phase selection and exact read-key sharing implemented | Audit remaining payload copies and runtime retention; not blanket elimination |
| P3 Owned frame/journals | Owned world/screen packet, bounded UI/post/cloud ledgers, typed Begin/completion notices; dynamic-water exact receipts and recovery added; world texture preparation lists now belong to their exact RuntimeLease with ordered interrupted-request recovery; named particle atlas rosters/IDs are also lease-owned with prior-request transition validation; screen mip queues are lease/GPU-owned with exclusive inactive-request ordered recovery; map normal consumers/producer/retirement now pin exact physical resource and readiness epoch | Complete transitive source-family seal and dependency-safe rollback for queued younger work |
| P4 Capture/render ownership | Joined executor, access gate/ordered controls; captured environment/PBR/UI/context/frontier/shadow fallback; source-owned sealed shield journal; physical attachment/source pair isolation; screen/PiP provider submission identities now bind once to the exact RuntimeLease before early preparation, with serial reentry guards retained; captured UI descriptor resolvers are exact lease owners including screen continuation | Remaining global/provider/control audit and paused-render/concurrent-capture proof; a gate is not an exclusive asynchronous executor |
| P5 Actual bounded CPU lookahead | Default remains joined. Opt-in Begin-ready encoding-tail pilot implemented with actual queued return, exact CPU/scene settlement, two retained source credits and owned result consumption; source sanitizer tests pass. Surface controls drain; an exactly unchanged-surface path overlaps only the terrain/material/model snapshot prefix, with particle writers deferred until drain. Existing opt-in mode2/3 opaque CPURead work launches after that prefix and before drain; successful receipts now retain a join guard across later original Lua/UI capture until DrawWorld consumption, with abort/control/teardown joins. A single sealed alpha/shadow continuation preserves original read order and capacity in that worker scope. Shadow terrain independent preparation now travels as an exact typed request owner, not an ambient weak latest slot. Other modes retain their old boundary. Exact draw/wall/thread-CPU diagnostics implemented | Native compilation/visual parity and positive active-work overlap measurement; complete eligibility/provider audit before widening beyond the restricted pilot or preparation overlap |
| P6 Resource-use GPU optimization | TerrainInfo, TerrainShading and completed prior-free Cube read-only exact lease-local outer receipt reuse implemented with source tests; validation/readability/retirement preserved. Exact terrain, CUS source and non-null engine-forward first-owner rosters are sealed once in preparation instead of rebuilding lifecycle phase censuses. Two exact immutable PBR sampler variants are cached per retained device. Full area NOT claimed | Explicit producer/subresource/access/order proof and separate eligibility/Metal evidence before broader queue/barrier/copy changes |

P5 is explicitly in the requested architecture, not an optional optimization that
can be declared complete by moving work to a joined thread. P6 is explicitly
incremental and proof-gated; the document does not authorize removing every
barrier or rewriting all rendering into one framegraph. Full acceptance additionally
requires the handoff's P0 fixed control, native Metal validation, visual/game parity,
and clean paired throughput/tail/latency measurements. Source tests alone do not
satisfy those runtime gates.

Dependency priority: preserve existing source work (including the handoff Appendix
C section 8 CPURead TLS receipt issue), finish real transitive ownership/control
boundaries, then bounded CPU enqueue; retain GPU pending limit one until separate
resource-use eligibility is established. No future.get removal is bundled into
an ownership prerequisite or used as a shortcut to report higher enqueue FPS.
