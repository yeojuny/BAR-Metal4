# Normal producer resource/epoch ownership (P3/P4)

## Checklist
- [x] Replace renderer-global `normalsEncoded` with per-MapImage epoch selection.
- [x] Three model/terrain/StartBox consumers pin both physical resource and epoch.
- [x] Producer pins exact corner buffer/normal texture and prepared pipeline.
- [x] Retirement pins the same resource/epoch rather than looking up a latest slot.
- [x] Independent source review, portable ASan/UBSan receipt tests and regression checks.
- [ ] Native Objective-C++ compilation, Metal execution and visual parity.
- [ ] Remaining provider/control audit; full P3/P4 closure and general P5 overlap proof.

After Map preparation, before early world image providers run, the request selects
an epoch on its actual immutable MapImage. This intentionally moves readiness
selection ahead of the former late reset: otherwise early callbacks could retain
the previous renderer-global bool and fail despite the intended producer encoding.
The pre-Map postprocess image allocation step does not invoke these providers.
A retained callback cannot accidentally observe a different resource's readiness.
Failed pre-encode epochs remain false after same-resource retries. Retry chooses a
new receipt; already captured callbacks are never silently rebound. Completed
immutable resources seed a readable epoch without repeating the dispatch.

ContinueScreen returns before Map/Begin and retains the actual backing's current
epoch; standalone provider calls similarly do not reset epochs. Pipeline capture
is AFTER Pipelines preparation, including on the first cold frame. Original GPU
producer commands, ordering, dispatch size and barrier are byte-identical after
normalizing only exact owner substitutions. The resource's shared atomic completed
state is set only by the original retirement path. There is no new GPU submission,
barrier removal, resource reuse eligibility or asynchronous preparation permission.

An encoded receipt is command-order evidence, not GPU completion. Existing
encode-then-submit-failure/poison behavior is not broadened or declared solved.
Epoch selection remains serial-owner state under existing access gates. This is
one P3/P4 prerequisite, NOT full closure or justification to enable default P5.

## Validation
`ASAN_OPTIONS=detect_leaks=0 python3 engine/tools/metal4-contracts/run_resource_producer_epoch_test.py`
compiles the actual portable production epoch helper with C++23, -Wall/-Wextra/
-Werror and ASan/UBSan. 1,000 resource/epoch lifecycle iterations cover early/late
callbacks, unrelated-resource negative control, encoded versus completed,
continuation, retries, abandoned epochs and retained resource lifetime. Actual
Objective-C++ source wiring checks validate the three providers, preparation order,
pipeline snapshot order, exact retirement pins and normalized original producer
hash. This is NOT an Objective-C++ build. LeakSanitizer itself cannot run under this
executor's ptrace environment; leak detection was disabled, ASan/UBSan remained on.

Begin-ready regression: 15 eligibility gates and 100 actual worker/source-credit
cycles passed. Readback regression: exact snapshots, retirement/depth/missing
receipt/continuation and 1,000 copies passed. Independent review caught a premature
pipeline snapshot; corrected before final verification. Full app build/run remains
explicitly deferred per user instruction. No performance claim.
