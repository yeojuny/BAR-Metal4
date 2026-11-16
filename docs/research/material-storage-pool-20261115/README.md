# Material argument-buffer storage reuse — source candidate

Base: PR #6, `236c44ea1a0a60750e6cb473ce58658c623978c2`.
Status: default-OFF performance candidate; portable ownership tests and actual-header ARC syntax passed. **No new Mac engine build, Metal game, image comparison or FPS benchmark was run here.** Main, release, Homebrew, source order, shaders and GPU pending limit remain unchanged.

## Why this follows the Mac results

The [two-scene results](../material-preparation-perf-20261115/mac-results/README.md) did not establish an FPS benefit for D: construction 185.257872 -> 184.981935 FPS; battle 55.326432 -> 55.357659 FPS. Recent-buffer hits were zero in both diagnostics. Reader-call omissions included nil slots and were not net CPU-time savings.

The [construction diagnostic gate](../material-preparation-perf-20261115/mac-results/measurement/DIAGNOSTIC-GATE.json) reports 16 Gets, 8 content-cache hits, 8 creations and 1,536 encoded bytes per reported pair. At the pinned base, each creation executes `newBufferWithLength` in `ModelMaterials156::Get`. Thus physical argument-buffer creation is directly present in this sampled workload. This does NOT establish its exclusive time, its contribution to the critical path, or eight creations per every game frame. No cause for changing material keys is asserted.

This candidate targets allocation churn, **not another faster last-key lookup**. Even when successive material keys differ, storage may be reused after its old content-cache and all prepared/submitted/quarantined readers have released their ownership. There is no broader image-result cache and no Q/Sepia dependency.

## Upstream-to-downstream ownership change

1. `NativeRetiredBufferStorage` wraps the existing `NativeRetiredResourcePool`. It pools exact buffer extents; the existing sole-shared-owner test is the only write permission.
2. A `ModelMaterialBinding156` carries BOTH the Metal buffer and its pool write-exclusion owner. A native object retain alone keeps allocation alive but does not prevent rewriting it.
3. The existing 128-entry material-content map stores this binding; the optional D memo also stores the complete binding, not only its buffer.
4. `PreparedCUSSource::Impl::Draw` retains the binding through the same prepared owner used for existing uniform/argument owners. Existing opaque/deferred/shadow owners and `M4OriginalSourceLifetime` keep those prepared draws in the event+feedback/failure graph. No new completion shortcut is added.
5. Only when the pool is the sole storage owner may a different key overwrite the buffer. Every miss still rewrites all 12 texture and 12 sampler slots, including nil. Hits, registration, source image providers, sampler/readability checks, draw ordering and barriers are unchanged.

`BeginBatch` only resets the bounded scan cursor. It is NOT completion. Cache eviction, CPU Join, frame number, or one GPU signal alone cannot make a busy slot reusable. Context/Clear drops pool/cache ownership without invalidating an outstanding draw. Full or busy pools allocate safely through the original storage mode instead of waiting.

The native memory lifetime contract still needs full Apple engine and failure-path validation. Portable tests model the prepared/submitted/late-feedback/quarantine holders; they do not execute GPU work.

## Controls and bounds

- `BAR_NATIVE_MATERIAL_STORAGE_POOL=1`: enable storage reuse; compiled default OFF.
- `BAR_NATIVE_MATERIAL_STORAGE_DIAGNOSTICS=1`: emit one aggregate group per completed material batch, only in the existing bounded sampled job buffer. Default OFF; no per-Get log or new timer.
- Existing D, U, Q, type-facts, screen-state and PREPARE controls are not changed.
- Pool retention is capped at 256 storage tokens per material consumer, including cache-held and busy slots. This is NOT a cap on total live allocations: safe uncached spills and outstanding graphs remain possible.
- Logical argument layout still must be nonzero and <=4,096 bytes. Physical `allocatedSize` may be larger. Pool-retained bytes overlap cache/draw ownership, exclude spills, and must not be added to content-cache bytes as disjoint memory.
- An extra `shared_ptr` token, larger returned binding, pool scanning and object retention have costs. OFF keeps direct allocation, but is not claimed instruction-for-instruction identical to the older binary. Compare a rebuilt OFF baseline before attributing an ON effect.

## Counter meaning

Kinds1-46 are unchanged. Kinds43-46 remain the original material counters. `created` and kind46 count logical material-content misses/encodes, not physical allocations when the pool is ON. The one-shot `new_buffers` log now reports actual physical allocations; encoded slots remain `created*24`.

The storage diagnostic adds a summary parent and three children:

| Kind | Meaning | count_a | count_b |
|---|---|---|---|
|47|MaterialStorageSummary|successful Gets|pool enabled0/1|
|48|MaterialStorageAllocations|new physical buffers|reused storage buffers|
|49|MaterialStorageBytes|new physical allocated bytes in batch|current pool-retained physical bytes|
|50|MaterialStorageState|pool cached slot count|capacity256|

Kinds43-46 and48-50 are children of47 when storage diagnostics are enabled. The existing 128-record buffer is unchanged. Reject dropped/incomplete groups; do not silently pool good fragments. `total_us` on these rows is counter-emission time, NOT allocation or provider time. The offline reader preserves process identity and verifies Gets=hits+encodes and encodes=new+reused. State samples use maxima, not sums. No FPS/CPU/RAM saving is inferred from these counters.

## Tests and reproduction

```sh
python3 engine/tools/material-storage-pool/run_tests.py
# Apple SDK + actual material-header preparation test; NO GPU commands submitted:
python3 engine/tools/material-storage-pool/run_tests.py --native
```

The portable test uses the actual new storage wrapper and unchanged production retirement pool with modeled buffer contents and source/cache/completion owners. A deliberately churn-heavy model (not a trace replay) executes 1,000 batches, 8,000 complete encodes, and 16,000 registrations in both modes. Physical creations were 8,000 OFF versus137 ON;7,863 slots were reused; byte results and a deliberately retained old/quarantine buffer were preserved. This is a controlled work-count test, NOT a native expected hit rate or speedup.

Tests cover busy owners, cache eviction versus retirement, late feedback/quarantine, different extents, bounded spills, Clear/context-style replacement, failed factory, partial-write abandonment and rewriting nil slots. Replacing the sole-owner gate with an unsafe unconditional reuse fails the assertion (negative control). The offline reader tests missing/duplicate/mixed counters and dropped records. Actual material header/template ARC syntax was checked with declaration-only Metal stubs; those stubs are not the Apple SDK. [Executed host receipt](HOST-RESULTS.json).

## Next Mac experiment: one candidate, same two scenes

Use a private complete build of this branch, not the shipped wrapper (which clears `BAR_NATIVE` variables). First confirm compiled inputs, actual child flags and a rebuilt OFF smoke. Keep PREPARE1/BEGIN0, worker1, FINAL_COLOR1, Sepia OFF, Q0, U0, **D0** and the previously adopted type-facts/screen-state settings fixed. Do not combine with the unmeasured U/Q prototype.

1. Run the actual-header native preparation test and full engine build.
2. Separate OFF/ON Metal Validation and representative scene checks; include resize/reload/exit and already-retained old frames. A still image is not full pixel equivalence.
3. Same matched power/display/scene diagnostic conditions in both arms. Enable the existing bounded trace and the storage diagnostic. Check drops, new+reused=encodes, unchanged successful Gets/content misses and reduced physical creations. Check pool high-water bytes and process memory separately.
4. Clean trace-OFF, Validation-OFF OFF/ON/ON/OFF for construction AND the preserved official battle scenario. Keep every valid run and adverse tail. For new game timing, use the exact frozen fixtures and run-level variance, not a synthetic allocation result.
5. Retain the default OFF unless clean evidence supports adoption. If physical allocation drops but FPS still does not, do not automatically enlarge the pool or add another cache: correlate the existing Join/worker/GPU/retirement trace and move to the next measured critical path.

```sh
python3 engine/tools/material-storage-pool/analyze.py off/infolog.txt on/infolog.txt --output material-storage.json
```

No new GPU queue depth, MRT permission, drawable ordering, image completion caching, shader translation or generic material-system rewrite is included. Existing authorship/license notices remain; the pool is an adaptation of this repository's existing ownership contract, not an imported implementation.
