# P5 cross-thread timing evidence prerequisite

Existing NativeCPUTrace records contained relative duration/phase values and
ancestry, but omitted absolute start/end. Two disjoint scopes could therefore
look indistinguishable from overlapping scopes across threads. The production
formatter now appends `start_ns` and `end_ns` from the same steady_clock source
used by the runtime. Existing prefix, IDs, parents, phase durations, sampling and
bounded trace budget are retained. No GPU synchronization or workload changes.

These records are wall-time envelopes, NOT CPU-active intervals. Runtime waits,
controls, and source callbacks may lie inside them. A positive envelope
intersection is only a candidate for deeper analysis, not proof of P5 performance.
Exact source-draw linkage, active prepare/encode segments excluding waits, actual
presentation throughput/tails and input-to-present latency remain required by the
original handoff. No such runtime claim is made in this checkpoint.

Validation uses the actual extracted runtime block and real serial worker under
ASan/UBSan: 100 calls each with sampled/every-loop/off modes (7/100/0 complete
chains); nested endpoint ordering and identical clock domain; invalid/reversed
and truncated output rejection. The previous missing-worker-ancestry mutation
still fails. The driver's stale worker.Invoke assertion now checks the production
CPU-job Submit path. Full app/native execution remains deferred.
Independent review: GO; separate sanitizer output confirmed all three modes and
negative ancestry mutation. No native execution was performed by the reviewer.
