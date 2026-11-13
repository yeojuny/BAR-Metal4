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

## Follow-up: eliminate remaining physical height/normal aliases

- [x] Map returns its exact transactional pair directly; remove renderer-wide
  `heights`, `terrainNormals` and `normalImage` aliases entirely.
- [x] Blueprint/placement use the pinned pair's immutable cornerSource; game/ground
  decals use its texture, shadow terrain uses its corner buffer.
- [x] Source cache exposes read-only exact-identity Find for preflight nil/length
  validation; no allocation/publication occurs in Find. Existing finite-value
  checks, failed allocation transaction and deformation drain boundary remain.
- [x] Update prior normal ownership regression expectations to the direct pair;
  actual backing one-shot ownership (100), cache failure boundaries (3,000 factory
  and 2,000 retention failures) and epoch tests pass with ASan/UBSan.

Remaining concrete ownership boundaries, NOT removed by these checkpoints:

| Surface | Observed protection | Why it is not general P5 closure |
|---|---|---|
| MapImage heightRRRR view | Exact source/physical image/device cache, provider access gate | Lazy cache mutation is still serial preparation, not lock-free simultaneous preparation |
| MapCache tile batches | Transactional Map update plus terrain/source deformation previous-submission drain | Shadow/main encode still reads renderer map.batches; no immutable request packet for this cache |
| SharedWorldInfoGPU / SharedMetalInfoGPU | Runtime lease and existing access/retirement contracts | Shared service Prepare/Clear remains outside the resource-pair proof; cross-provider ownership needs separate audit |
| Render pipelines/attachments | Existing lease retains and renderer access gate | Except the pinned normal compute pipeline, other paths still select renderer state; retaining alone is not concurrency eligibility |

Do not remove the deformation drain, enlarge worker eligibility, or call these
shared services concurrently based only on this normal/height ownership work.
Independent review caught a stale removed-height preflight reference; exact-cache
Find corrected it before final regression checks. No full application build/run.

## Follow-up: uploaded tile owner for shadow encoding

- [x] MapCache is a shared uploaded resource owner. A replacement owner is allocated
  and built before either cache changes; all original buffer/texture retention must
  succeed before the same noexcept commit publishes it.
- [x] Map returns a PreparedMap containing the exact normal pair AND tile owner.
  Shadow terrain encoding uses this packet's tile batch vector, not impl->map.
- [x] Old request owner survives replacement/clear without copying the tile vector.
  Existing lease persistent allocations continue to retain GPU resources through
  GPU lifetime; this change does not substitute a CPU pin for that residency.
- [x] 1,000 companion-owner failure/commit/clear/lifetime cases added to the actual
  source-cache helper regression. Existing 3,000 factory/2,000 retention failures,
  backing100 and epoch1,000 ASan/UBSan regressions pass.

This resolves the shadow `map.batches` selection noted above. Terrain-square
provider still selects the serial renderer's current cache (with exact source
checks) and mutates its source-specific view cache under the existing access gate.
That provider requires a separate lease/request binding before concurrent older
render/newer preparation could be allowed. No such concurrency is enabled here.
Existing deformation previous-submission drain remains unchanged.
