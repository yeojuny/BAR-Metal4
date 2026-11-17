# Fixed-topology indexed geometry source candidate

The audit followed the vertex-ID mappings in 77 native source files and the nonindexed triangle-list dispatch in 35 encoder files. Five draw paths on exact R03 now have an opt-in UInt16 indexed route under BAR_NATIVE_FIXED_TRIANGLE_INDEX_REUSE=1.

| Path | Original IDs → distinct indexed IDs | Semantic basis |
|---|---:|---|
| Radar/sonar circles | 6 → 4 per segment | The original shader maps slots 0/3 and 2/4 to the same corner. |
| SensorLOS outline | 6 → 5 per segment | Slots 0/3 agree in the fallback AABB and in both tight strip directions. |
| SensorLOS R8 fan | 3 × (n−2) → n | The center is shared, and the preceding triangle supplies each equivalent rim ID. |
| HealthBars | 78 → 40 per instance | Duplicate IDs resolve to the same vertex inside each bar strip or glyph quad. |
| Unit Stencil | 18 → 12 per instance | The two triangles of each face share their matching strip corners. |

The counts are properties of the tables. Hardware vertex invocations and FPS remain unmeasured. Each triangle keeps its three ordered shader inputs, including the provoking input. None of the indexed triangles repeats an ID internally. The original shaders and their arithmetic, fragment outputs, texture reads, instance offsets, and attachment state keep their source bytes. The table never merges draws or remaps instances.

The immutable buffers are scoped to the renderer, Metal device, and RuntimeLease context. Each prepared draw strongly retains its buffer and registers it with the active lease before encoding. UInt16 allocation is checked for nonzero aligned address and full byte length. The existing nonindexed draw handles a disabled flag, unavailable buffer, multisampling, and sensor counts outside the bounded prefix. Unit Stencil retains the original clear triangle.

## Reviewed paths outside the patch

- Existing GameDecal, GroundDecal G01, WeaponRange line, expanded exact screen/PiP line indices, LuaNano and EnergyExplosion already have relevant index routes or frozen candidates. Their source and evidence remain preserved.
- The other SensorLOS outline corner pairs differ between the X-major and Y-major winding tables. Only the alias common to both is applied. The fragment-discard candidate from round 38 remains separate.
- The 4 × 4 terrain fallback has a reusable grid. Its legacy shadow and map-depth consumers have separate preparation and residency paths. Activation in the normal FPS scene and equivalent depth output need independent validation.
- BuildSquare and arbitrary submitted triangle lists carry source-dependent vertices or primitive layouts. Cross-record equality is unproved.
- DOF, bloom and cloud full-screen quads are small one-off draws. Their low serial reuse and extra buffer binding warrant a separate measurement before broadening this candidate.

## Verification

The static check reconstructs the ordered original vertex inputs from all five unchanged shader routes. It covers the full sensor table prefixes and both LOS tight winding directions. Four affected Objective-C++ translation units passed SDK syntax-only checks using the existing compile commands. The existing GroundDecal G01 CPU contract passed. Forward and reverse patch checks passed.

A bounded follow-up can build one candidate and reuse the existing R03 SensorLOS geometry and Range199 validation cases where their draw layouts match. The recorded CPU proof covers the fixed mappings. Actual GPU validation should check both indexed and legacy submissions before one clean, same-binary OFF/ON FPS pair. The pair can use the existing R03 paced scene, matched settings and libraries. Record the affected families that actually draw. A rendered family outside the scene remains a coverage gap.

This source delivery used zero engine builds, GPU runs and games. The four original shader files, original R03 and earlier GameDecal and SensorLOS binaries and manifests are preserved. The user’s display-awake process is still present.

Patch SHA-256 cf5e699161a1a38881d47af2ce42b034aa14e172d97bd0084b4fc4251e86569b. [Patch](INDEXED-GEOMETRY.patch) · [File hashes and route map](CANDIDATE-MANIFEST.json) · [Static checks](STATIC-VERIFICATION.json)
