# Immutable main sampling capture contract

The per-frame UI raster mapping and world capture query `ActualMainSampling`.
It previously entered the renderer gate because the worker also wrote the
`allocationGeneration` member after preparing resized/reused attachments.

The renderer now publishes a `unique_ptr<const NativeMainSampling>` exactly once,
after successful initial allocation and before pipeline initialization. Capture
reads a value copy without acquiring renderer ownership. Its generation is the
initial successful allocation witness, not a live attachment generation. Actual
frame-local attachment receipts retain their own current generation. There were
no active consumers of the former live generation field. Existing no-concurrent
renderer move/destruction requirement remains.

Validation: extracted production accessor, real gate, 10,000 reads while another
thread owns the renderer gate; value-copy independence and initial witness tested
under ASan/UBSan. Structural checks cover one constructor publication, no worker
write, and actual SceneBridge capture callers. Existing gate/worker suite passed
1,000 nested/error and 100 cross-thread recovery cycles. Independent review GO.
No native Metal allocation/execution test or app build performed. CPU lookahead
remains disabled. UI backing readiness publication is the next recurring capture
boundary, not resolved by this sampling change.
