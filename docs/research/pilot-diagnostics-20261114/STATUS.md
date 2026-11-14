# Minimal pilot regression diagnostics (default off)

Built on the reserved handoff admission fix. These changes observe existing behavior; they do not remove drains, restore async MRT209 reuse, increase GPU queue depth, or enable a pilot. No game or performance run was performed.

## Existing FRAME_TRACE extended
Enable the existing `BAR_NATIVE_FRAME_TRACE=1` only in a separately authorized diagnostic run. The existing sample/budget limits remain. `BAR_NATIVE_FRAME_TRACE_EVERY_LOOP=1` changes sampling, not policy. Diagnostic overhead means these runs are not directly comparable to uninstrumented FPS runs.

| NativeCPUTrace kind | Meaning | Fields |
|---|---|---|
| 7 | Dispatch admission envelope | count_a bits0 Begin allowed/requested, bit1 Prepare allowed/requested, bit2 actual eligibility, bit3 actual queued return; count_b rejection bitmask |
| 8 | Surface-boundary capture prefix | source_draw is the current capture draw |
| 9 | RequireRetiredGPUWork envelope | p0 optional source CPU-reader join; p1 render CPU Join/settlement; p2 runtime GPU drain and retirement callbacks; p3 main receipt application/check |
| 10 | Surface-boundary drain caller | parent of kind9 at this boundary; other kind9 callers lack this specific parent |

Kind7 options are admitted Scene options, not raw environment requests. Rejection bits0–14 correspond to requested, worker, owned, visible, admitted, sourceCredit, deferred, screen, cpuSettlement, readback, continuation, dynamicWater, legacyWater, arbitraryCallback, previousRetirement. For the first9, false rejects; for the last6, true rejects. Preparation mode ignores previousRetirement. An exception before queued return leaves bit3 unset.

Kind9 source_draw labels the *current caller draw*, not a claim that the retired GPU frame has that same identity. Relate predecessor identity using existing submission traces. p1 can include waiting for a worker that itself waits on GPU/runtime work; it is not pure CPU computation. p2 includes runtime locking/callbacks as well as event wait; it is **not pure GPU execution time**. No new pending-submission query or source owner was introduced.

The new kind7 becomes the worker trace's inherited direct parent; prefix descendants can similarly have kind8 as parent. Parsers must traverse ancestry rather than assume the old direct-parent kind. Existing kinds4 Prepare,5 capture,6 Encode provide monotonic endpoints and thread CPU durations. Wall interval intersection alone does not prove active work overlap. Truncated budgets and unsampled frames are not zero-cost frames.

## Existing MRT174 extended
`BAR_NATIVE_GEOMETRY_MRT174=1` retains the original source_draw window2048–2111 and512-row quota. Added fields: reuse209, reuse209_permission, cow_storage, pool_requested, pool_hit, pool_new_bytes.

- reuse209 is actual writing-storage reuse selection.
- permission is object availability, not successful permission consumption.
- cow_storage identifies the allocation/reuse fallback, not actual preservation-copy issuance.
- pool_new_bytes sums newly allocated textures' allocatedSize; zero on pool hit. This is allocation size, not GPU traffic/time.
- Existing copy_encoder/calls/logical_source_bytes distinguish actual copy issuance.

The optional pool observation stores scalars only and is enabled only inside MRT174's window. It retains no storage owner and changes no use_count eligibility.

## Validation and limits
- Exhaustive32768 policy combinations: diagnostic mask agrees with both unchanged eligibility functions.
- New CPU trace kinds tested through extracted actual runtime implementation with trace off, sampled on, and every-loop on, including ancestry and formatting.
- Existing PrepareReady/settlement and external-runtime-control source suites pass.
- Mac syntax-only compilation with the existing native compiler recipe passed SceneBridge.cpp, WorldRenderer.mm, RuntimeNative.mm and CUSDeferred.mm (the latter includes the storage pool).
- Independent read-only diagnostic diff review found no policy/ownership or trace-stack blocker.
- Full engine relink, actual trace capture, visual/runtime acceptance and performance attribution remain pending.

Next measured questions: how often release really occurs; prefix CPU versus runtime drain wait; whether OFF writing209 becomes ON COW; whether pool misses or actual copy bytes rise. None is answered by these source changes alone.
