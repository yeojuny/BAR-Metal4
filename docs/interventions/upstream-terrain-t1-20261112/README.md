# Upstream-first intervention: Terrain T1

AI-assisted implementation, 2026-11-12. **Experimental source patch, not a released app or measured performance result.**

## Provenance and scope

- New branch: `refactor/upstream-terrain-t1-20261112`.
- Exact parent: [`16e7f1844203bdd7e26bb19a39e9e0276f1fcb74`](https://github.com/yeojuny/BAR-Metal4/tree/16e7f1844203bdd7e26bb19a39e9e0276f1fcb74), the earlier intervention source, including its inherited build/runtime changes.
- Specification: [handoff at immutable main commit](https://github.com/yeojuny/BAR-Metal4/blob/7f37af54f8a95926a8474f8acb595a68e98ae50a/docs/research/BAR_Native_Metal4_Upstream_Refactor_Handoff_20261112.md), part I, **P1-A / T1**.
- Main, the original intervention branch, historical native233 overlay, released binaries and Homebrew are not changed.
- This branch is **not** a reconstruction of the measured native233 binary. It is a narrow delta against the intervention parent.

## Implemented

`SourceProgram::Write` preserves existing context, enabled state, no-location/zero-count behavior, ownership, revision/extent overflow and finite-value validation before comparing content. It compares the complete vector's representation and type, rows, columns, count and transpose, without creating a temporary vector/map.

Only a bit-identical value **and** an aligned current publication (identity, state, consumer, uniform owner and binding kind) skips the old allocation/publication path. Signed zero is distinct. Changed values retain the existing clone/write/publish behavior.

The critical counterexample remains a real publication:

```
saved = Current(A)
Write(B)
Restore(saved)
Write(B) // writer equals B, binding still A: must clone/publish, never skip
```

Retained snapshots remain immutable; no live-state borrowing, GPU queue changes, shader math changes, callback suppression, simulation changes, or asynchronous worker changes are introduced.

## Diagnostic counters

Compile the source with `BAR_NATIVE_TERRAIN_WRITE_STATS` to expose `TakeNativeTerrainUniformWriteStats()` on the source/main thread. It returns and resets `calls`, `exactChanges`, `alignedNoOps`, `stalePublicationFallbacks`, `mapCopies`, and `publications`.

- `calls` includes rejected and no-location/zero-count calls.
- `exactChanges` and `stalePublicationFallbacks` count classification after successful validation, before allocation; an allocation/publication failure is not a successful change.
- `mapCopies` counts setter snapshot copies, not link default copies.
- `publications` counts successful `Publish`, including Enable.
- Ordinary builds compile out counters entirely. The harness groups calls into synthetic windows; **no real per-draw-frame logger is wired or measured yet**. A future diagnostic integration must sample at actual source draw boundaries, not simulation frame boundaries.

## Validation and limits

Run from the repository root:

```sh
python3 engine/tools/metal4-contracts/run_native_terrain_program_test.py --output /tmp/terrain-t1-tests
```

See `RESULTS.json` for the host compiler and actual results. The harness exercises the production source adapter with explicit external-service stand-ins; it is not a substitute for the complete engine build or GPU validation.

Not run: macOS full engine build, Metal API validation, graphical parity, synced-game/network regression, matched Native/MVK performance, presentation timing or input latency. No FPS or ms improvement is claimed. The user's selected MacBook was offline during this implementation; cloud CPU tests do not establish Mac execution.

`BASELINE.json` records the eleven historical measured flags and missing conditions. The control for this patch is a new build of its exact parent with all build/runtime inputs identical to the candidate, not the combined `run-candidate.sh` profile versus a historical binary. Diagnostic and clean performance builds must remain separate.

## Next API seam (T2, not implemented)

Introduce a factory-created immutable linked definition containing source identity/generations, actual shader hashes, sorted defines, supported source ABI and successful link certificate. Keep uniform content/revision separate. Publication retains that definition plus sealed uniform content; draw provenance compares exact retained definition ownership and scalar lifetime generations. Preserve `CompileRequest`'s named-value/default retention across reload.

The certificate must not assert target PSO or image readiness: actual target format/MSAA/texture-type checks remain in prepare. Main-side source validation must not inspect mutable renderer PSO/image-library maps. Current/Restore save semantics must be classified before T3 lazy sealing; simply sealing on draw would break nested source state restoration. This extends T1's exact-content versus current-publication distinction rather than replacing it.

P1-B postprocess indexing and P1-C immutable CUS roster can follow as separate reviewed changes. P3–P6 remain blocked on their explicit packet ownership, journal and resource-use proofs; this patch does not remove waits or expand pending GPU work.
