# P4 → P5: owned Postprocess read-provider boundary

## Completed
- [x] Postprocess exposes preparation-time `CaptureReadability`: verify the exact version/allocation identity once and own its encoded/completed flags.
- [x] Sensor preview, shared screen-image and target previous-image callbacks now retain this receipt rather than a raw renderer/cache pointer.
- [x] Invocation remains at the original encode/read boundary. Receipt creation does not require or publish producer readiness; later invocation still enforces `completed || encoded`.
- [x] CPU-content versions retain their existing no-GPU-check behavior while the receipt owns the version.
- [x] Physical textures/storage remain retained through the existing ResolveImage/ImageFor lease lifetime graph. GPU pending-one and predecessor completion gates are unchanged.

This removes three actual encode-time mutable cache/provider crossings. The old access gate already serialized those accesses; this is not a claim of a demonstrated data race. The preparation provider itself still uses its guarded renderer cache. This is one typed family boundary, not a general source/provider eligibility certificate.

## Verification
10 scenarios compile the actual receipt method with ASan/UBSan against modeled native objects: producer flags initially false, all encoded/completed combinations, later flag transition, cache replacement/destruction, exact owner lifetime, CPU images and missing/mismatched allocations. Three production callback sites are checked. Existing 13-case pending-input receipt regression also passes. Actual native Metal/GPU behavior remains untested; no full app build/run or speedup claim.

## Remaining architectural work
- [ ] Complete provider-family ownership/eligibility audit for general preparation overlap.
- [ ] Main-thread final-destruction/rollback ownership proof for every source family, including Bloom/SSAO/UnitStencil; no bug has been established merely from their rollback callback presence.
- [ ] Whole-world dependent rollback/rebuild beyond terminal source-lineage quarantine.
- [ ] Native compilation, paused-worker/capture/error integration, active-work overlap and latency/performance acceptance.
