# Terrain immutable binding rows

Base: 9f2959e7ecd5ede940235910bff2fa7cfd9ca291.

## Completed
- [x] History retains 32 immutable unit rows, each containing six resolved target bindings.
- [x] Terrain snapshots share unchanged rows instead of resolving/copying every binding every capture.
- [x] Bind/default-bind/enable invalidate one row. Registry object changes, deletion, default sampling/storage, context/resolver changes and Pop invalidate all rows.
- [x] Successful content cancellation invalidates without changing event sequence or allocating. Old snapshots retain old content.
- [x] Bounded weak-owner/context row proofs skip repeat static validation of unchanged rows; actual sampled-object/readiness checks unchanged.
- [x] Real production history, registry and terrain source linked with repository headers, C++20 -Wall -Wextra -Werror and ASan/UBSan: 92 checks passed.
- [x] Production capture/proof extraction regression: 211 checks passed.
- [x] Independent read-only mutation/proof review: no correctness blocker found.

The real-code fixture covers same-state reuse, single-row rebuild, multiple bindings of one object, sampler/default changes, push/pop, cancellation without sequence change, retained old content, delete/reset/close, resolver failure and retry. No full game/Metal paths execute.

## Limits
Initial capture allocates 32 rows plus one table. Stable subsequent capture copies 32 shared owner pointers and allocates one table, not 192 binding records. A single bind/enable rebuilds six bindings; general object mutation conservatively rebuilds all rows. Frequent content publications can limit this optimization. No FPS/CPU timing claim is made.

- [ ] Mac/Metal compilation, game/visual testing and real performance measurements.
- [ ] Per-object targeted invalidation only if measurements warrant it.
- [ ] Full P5 capture/prepare overlap and GPU optimization remain separate.

Existing main, installed app and unrelated dirty vendor files are preserved.
