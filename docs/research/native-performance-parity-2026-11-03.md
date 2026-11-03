# Native Metal 4 performance and Linux parity research

Research baseline: candidate 204, source commit 206f1498d50b7da23b956fea1db496594bb04cd2. This is an investigation, not an optimization or measured speedup. The user reports native ~155 FPS and MVK >220 FPS; equivalence of scene and settings remains to be verified. 155 and 220 FPS correspond to 6.45 and 4.55 ms/frame respectively.

## Evidence and constraints
- Release 1.11.0 reports 3 matched pairs of candidates 201 and 204, pooled 154.60 and 155.48 FPS in one scene. This is not a matched MVK test, nor proof of Linux parity.
- Current code already provides persistent pipeline caches, buffer pooling and CPU/GPU overlap. Do not propose recreating these as new work.
- Avoid feature removal, reduced resolution or sample count, altered terrain detail, weakened validation, or stale-resource reuse as speedups.
- Linux original output is the correctness reference. MVK on the same Mac is a performance comparison, not automatically the Linux visual oracle.

## Hypotheses, not measured bottlenecks
1. ROAM local grid positions appear immutable while topology changes invalidate the combined position/index snapshot. Investigate shared immutable grid with separately versioned indices, keeping border meshes, index order, transforms and in-flight ownership correct. See engine/rts/Map/SMF/ROAM/Patch.cpp and engine/rts/Rendering/Backend/NativeTerrainMeshSnapshot.h. Verify every producer before changing.
2. Skinned CUS read-set validation traverses submitted indices per instance, repeatedly reads first-bone transform rows, then deduplicates. Cache geometry descriptors or per-instance first-bone scale reads only while preserving all current branching, invalid inputs, publication identities and rejection semantics. See engine/rts/Rendering/Backend/NativeCUSRenderInputs.cpp.
3. Terrain creates vertex and fragment argument tables per draw. Measure creation count and critical-path cost before introducing completion-owned pooling.
4. Terrain single-sample depth preparation may be unnecessary when no consumer needs it. Prove all particle, decal and other consumers, preserve MSAA semantics and producer lifetime before avoiding any resolve.
5. Writable MRT copy-on-write traffic is a larger, higher-risk possibility. An alias is safe only after proving all old-version readers and lifetimes; existing logical-byte counters do not measure physical DRAM traffic.

## Measurement traps and plan
Existing resource and overlap probes sample bounded early windows and may not cover the same frames. Deferred GPU timestamps are captured by the overlap probe, not the ordinary GPU metric. A zero when timing is disabled is not zero cost. CPU encoding timings are not GPU pass timings, and nested scopes cannot simply be added.

First run matched, repeated uninstrumented native/MVK trials on identical machine, content, replay, physical render dimensions, settings, VSync and power state. Separate warmup, report frame-time distributions and variability. Use diagnostic probes in separate runs and correlate submission identities before attributing costs. Check quiet, unit-heavy, effects-heavy, water and terrain-changing scenes.

For every proposed change, first test exact data and rejection behavior against candidate204, then compare temporal screenshots against unchanged candidate204 and Linux, including animated units, terrain edits, water, shadows, UI and resize. Declare numerical tolerances before comparison. No FPS gain or 99.9999% parity is established by this note.
