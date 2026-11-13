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
| P3 Owned frame/journals | Owned world/screen packet, bounded UI/post/cloud ledgers, typed Begin/completion notices; dynamic-water exact receipts and recovery added | Complete transitive source-family seal and dependency-safe rollback for queued younger work |
| P4 Capture/render ownership | Joined executor, access gate/ordered controls; captured environment/PBR/UI/context/frontier; physical attachment/source pair isolation | Remaining global/provider/control audit and paused-render/concurrent-capture proof; a gate is not an exclusive asynchronous executor |
| P5 Actual bounded CPU lookahead | NOT enabled; ordinary worker still joins | P3/P4 gates, total-source credit bound, enqueue/data-notice lifecycle, measured positive active-work overlap (joined read-receipt/trace continuity now source-tested) |
| P6 Resource-use GPU optimization | Some family-specific proofs exist; full area NOT claimed | Explicit producer/subresource/access/order proof and separate eligibility/Metal evidence before broader queue/barrier/copy changes |

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
