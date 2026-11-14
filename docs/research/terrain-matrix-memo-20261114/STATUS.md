# Terrain per-Prepare model-view inverse reuse, 2026-11-14

AI-assisted bounded change after CUS packing commit
`085cafbc0484e39bc5cabcc140ae07b7ad9d2f0d`.

## Checklist
- [x] Identify repeated determinant/inverse/finite checks in advanced forward draws.
- [x] Reuse one successful inverse for an exact 16-float bit key within Prepare.
- [x] Preserve original input checks, math operations, dynamic state and GPU lifetime.
- [x] Portable ASan/UBSan memo tests and source call-site checks.
- [ ] Mac SIMD/Objective-C++ compile and actual application validation.
- [ ] Actual hit rate, time reduction or FPS measurement.
- [ ] Broader Terrain common/per-square uniform and binding split.

`NativeTerrainMatrixMemo<simd_float4x4>` lives on the stack of Terrain Prepare.
`DrawUniform` still converts/checks modelView and projection for each square.
Only advanced non-deferred model-view determinant/inverse and result-finite
validation reuse the exact same successful result. A changed key runs the same
original math/check sequence. Key/value publish only after successful computation;
A -> B -> A rebuilds A. Signed zero is distinct, and all 16 scalars are keyed.
No source uniform owner, image owner, lease, provider result, target or shader
state is cached. Target/view/clip/fog/texture/binding and readiness paths are
unchanged. No cross-Prepare or cross-frame global cache was introduced.

The fixture verifies 100 repeated lookups/one compute, each scalar affecting the
key, signed zero, A/B/A, exception recovery, and separate Prepare instances.
This proves the reusable memo decision, **not** Apple's SIMD inverse or Metal
execution. The generated fixture uses std::array as the matrix value. Production
hashes and explicit untested scope are in `host-receipt.json`. No game, benchmark,
full app build, installed app modification or main change was performed.
