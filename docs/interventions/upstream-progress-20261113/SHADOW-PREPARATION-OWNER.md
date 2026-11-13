# Exact request-owned shadow terrain preparation

The previous independent preparation returned an erased owner but also wrote
`EngineShadowGPU::Impl::terrainPreparation`, a renderer-wide weak latest slot.
PrepareTerrain later selected that slot. Serialized gates made this workable,
but it was an implicit cross-request dependency, unsuitable for widening P5
preparation overlap.

The slot is removed. A typed opaque `ShadowTerrainPreparation` shared owner now
travels explicitly through SceneBridge options, Metal4WorldSubmission's owned
options, DrawPrepared and PrepareOriginalShadowSource into PrepareTerrain.
Request A retirement/release cannot select or clear request B's preparation.
No unchecked void cast or global latest lookup remains.

The consumer verifies runtime, device, exact source pointer and shared ownership,
and geometry count before using early geometry. Missing or mismatched handles
retain the previous ordinary cache Acquire fallback; mismatched geometry is never
consumed. Prepared objects retain buffers independently of cache eviction. This
preparation can allocate Metal buffers, but does not encode GPU commands, change
residency or publish completion. Existing FinishCPUWork and renderer access gates
are unchanged. Main renderer caches still prevent general concurrent Prepare.

Validation:
- Actual preparation/Matches code extracted with Metal device/allocation sentinels:
  1,000 A/B ownership, context/device/source/count mismatch and release cases.
- Actual owned world packet fixture verifies explicit typed A/B handle retention
  through worker invocation and independent release; existing 1,000 handoffs and
  partial capture rejection remain.
- Structural checks cover the entire typed call chain, absence of ambient slot,
  preserved mismatch fallback and unchanged serialization gates.

These are source/CPU ownership tests, not native Metal allocation/command tests.
The change establishes a P3/P4 dependency for P5; it does not itself increase
concurrent renderer preparation or claim a performance improvement.

Both ownership fixtures passed ASan/UBSan, including independent rerun; independent
source review GO. Full SceneBridge Linux host translation-unit syntax PASS with
actual include dependencies. Objective-C++/Metal translation units were not built.
