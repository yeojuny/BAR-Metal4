# Terrain captured image-table proof

Base: c3e59987968c248d7c08172a58b8b34b1f736a96.

- [x] Capture still resolves all 32 × 6 actual history bindings and validates each.
- [x] Store that validated table in a private immutable owner. No public mutable table or separable validation flag.
- [x] Prepare checks current expected context/frame and metadata without rescanning all 192 slots.
- [x] Actual sampler object/storage checks, provider resolution, readiness and submission ownership remain unchanged.
- [x] ASan/UBSan production capture/proof extraction fixture: 211 checks. 100 validations perform no additional table scan; sampled-object validation still occurs.
- [x] Complete NativeTerrainSourceData.cpp Linux C++20 syntax compile with real repository headers succeeds.
- [x] Terrain writer regression fixture: 306 checks and unchanged exact trace 9120704849664684142.
- [x] Independent read-only review: no blocker found.

The extraction fixture stubs history/registry/object validation services. It exercises the actual production capture, table-validation switch, proof checks and sampler selection, not full history-registry or Metal integration. Tests compare every captured slot, retained old versions, copy/move, missing or malformed input, context/frame mismatch and default-sampling rejection.

## Performance tradeoff and limits
One heap allocation per capture is introduced. The proven structural reduction is the second 192-slot validation scan, not total CPU time or FPS. Each capture still resolves all slots because registry object changes can alter a binding without a bind command. Row-sharing requires real mutation/version tracking and is intentionally not guessed here.

- [ ] Mac/Metal compile, game execution, visual verification and performance comparison remain unperformed.
- [ ] Common/per-square binding copy removal and broader P5 overlap remain separate work.

No main or installed app changed. Existing unrelated vendor files excluded.
