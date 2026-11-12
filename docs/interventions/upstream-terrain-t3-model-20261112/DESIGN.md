# T3 eager/lazy observation contract (design only)

Baseline: `dbbcc63cd6fd85b418b5861934485cad759217a1` on the isolated upstream branch. No production storage or scheduling change is authorized by a passing model test. The companion model is an executable design experiment, not an implementation of the full engine or a proof of rendering equivalence.

## Proposed state separation

Keep definition/lifetime, mutable named uniform values, logical write revision, and selected binding provenance separate. A saved selection contains immutable content and a definition owner. It does not own the source writer. Dirty writer content is not interchangeable with a restored historic selection. Track explicit selection-to-writer alignment; never infer alignment solely from equal values or a raw pointer.

An eventual lazy writer would advance its logical revision at exactly the same accepted setter calls as the current eager writer. Allocation/materialization time may differ; logical revision and failure ordering may not. In particular, writing the current writer value after restoring an older selection repairs publication and advances revision. Do not collapse this to an aligned no-op.

## Boundary matrix

| Operation | Proposed materialization | Required observable behavior |
| --- | --- | --- |
| Internal owner/kind inspection | None | Immediate provenance/kind only; no borrowed reference retained across mutation |
| Valid aligned exact write | None | Preserve revision and identity; all prior error checks still run |
| Changed write | Defer immutable copy | Validate first, update named state and revision, select current writer |
| Current / SaveCurrent | Seal selected live dirty state | Return an owning immutable value with exact logical revision |
| CaptureCurrentNativeTerrainProgram | Seal then existing validation | Reject stale restored state if it no longer matches writer; retain full existing readiness/context validation |
| Restore | Do not rewind writer | Restore exact saved selection, including non-terrain kind; do not replace it with latest writer |
| Reload / import / release | Explicit lifetime transition | Preserve existing named-value/default policy and unbinding rules; invalidate definition identity |
| Delete / teardown | No allocation or seal | Never dereference saved raw provenance after destruction |

The existing `InspectCurrent()` return type exposes publication fields. A lazy implementation cannot simply leave those fields stale and claim compatibility. It must either provide an explicit selected-writer inspection token or migrate every consumer with a documented contract. Likewise a lazy seal can allocate and throw, while teardown and kind inspection must remain non-throwing. These API decisions remain unresolved, intentionally outside this patch.

## Differential oracle

The model uses an unbounded logical writer-version witness, not production uint64 revision arithmetic or physical publication IDs. Overflow and actual revision-field compatibility remain source-test gates. Compare semantic observations, not addresses or publication allocation counts: selected kind, definition/lifetime, exact typed values/shape, logical revision, successful or rejected capture, saved-history contents, and validation failures. Earlier retained observations must never mutate. A failing implementation must report the seed and event prefix to reproduce it. Allocation reductions in a Python model are not engine allocation or timing measurements.

Directed cases must include repeated writes before an observation, signed zero and type/shape changes, nested save/other-bind/restore, failed writes with unchanged state, stale restore followed by equal-to-writer repair, reload invalidation and deletion with saved selections. Random traces supplement these cases; they do not replace source-level tests.

The reference must match the actual baseline's conservative rule: `Save(A), Write(B), Restore(A), Capture` rejects because A no longer matches writer B. `Current` can still retain A. After `Write(B)` repairs selection, capture succeeds at the incremented revision. Allowing a historic capture would be a separate semantic change, not a T3 optimization.

## Gates before production lazy storage

1. Complete T2 provenance/certificate migration without weakening source or target validation.
2. Full actual Mac build and Metal acceptance on the user-selected machine. No substitute computer is selected by this work.
3. Implement source-backed eager/lazy differential tests using the real writer, including actual Lua callback normal/error paths; the standalone model does not satisfy this gate.
4. Close the inspection-token and exception-order contracts above, including allocation failure transaction boundaries.
5. Exercise inactive named uniforms, defaults, generation/revision overflow, context closure and pointer reuse against real production code.
6. Matched rendering/workload capture, visual parity and CPU/allocation measurements before any speed claim. GPU queue depth, fences, RNG/callback order and shader math stay unchanged.

Until then production remains synchronous and eager. T3 is not complete.

## Recorded host result

`RESULTS.json` records eight directed traces, 100 deterministic seeds / 100,000 random events, and two detected deliberately faulty lazy policies. Both models share a simplified state machine; common mistakes can therefore pass differential comparison. Directed semantic oracles and negative controls reduce, but do not eliminate, that limitation. Typed shape metadata is opaque in this model, not validated with the real ABI. Its seal counts are simulated materializations, not production allocation savings.

The unchanged production-source harness was also rerun (79 checks), along with the extracted readiness certificate harness (11 checks). Neither performs a full Mac/Metal build. No new environment was accessed.

Independent review found no blocking issue within this declared abstract subset. Additional exclusions: modeled Enable permits repeated enable, whereas production EnableRaw rejects an already-bound writer; modeled reload represents only a successful builtin transition and does not emulate actual link defaults or source/uniform revision arithmetic. These cases must be covered in a future production-backed differential suite. Passing the model cannot clear those gates.
