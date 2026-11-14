# Terrain common uniform / texSquare overlay

Base: eea50492532dadc25db422462eb38fc591f8599e on the approved refactor branch.

## Completed
- [x] Share one immutable common uniform snapshot across observed texSquare changes.
- [x] Keep each square's exact value/type/shape as an override, with no chain of preceding square snapshots.
- [x] Materialize effective values only for general writes, copy/import/relink operations.
- [x] Preserve typed readers, unknown uniforms, shadow matrix, signed zero, stale-publication repair and earlier snapshots.
- [x] Host ASan/UBSan production writer fixture: 306 checks; eager-observation fixture: 301 checks. Both exact observation fingerprints: 9120704849664684142.
- [x] Existing production slot/value-reader fixture: 38,159 checks.
- [x] Independent read-only review: no remaining blocker found.

The 32 observed square-write fixture performs zero full uniform map copies. This is structural evidence, not timing or an FPS claim. Eager reference changes observation scheduling on the same implementation; it is not an independent legacy algorithm oracle. Lazy-only allocation-fault probes align their final logical state before shared trace comparison.

## API and scope
`values` remains materialized writer/import storage and is empty on an overlay. Effective consumers use `Find`/`FindName`, `ForEachValue`, or `ValuesCopy`; copy construction and assignment materialize automatically. Production direct-map users were inspected and writer paths updated. Overlay creation freezes the prior root by replacing the writer snapshot. GPU resource readiness, binding snapshots, texture transport, submission registration and P5 admission are unchanged.

## Remaining
- [ ] Mac/Metal compilation and game validation (not run in this task).
- [ ] Frame timing, GPU/FPS comparison and visual equivalence measurements.
- [ ] Broader common binding/per-square texture separation, if warranted.
- [ ] Full CPU prepare/capture overlap and measured GPU optimization remain separate work.

No game, full application build, performance measurement, main modification or installed-app replacement was performed. Existing unrelated vendor line-ending changes were excluded.
