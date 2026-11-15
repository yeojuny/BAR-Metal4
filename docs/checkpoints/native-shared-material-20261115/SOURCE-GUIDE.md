# Read these source entry points first

| Question | Entry point |
| --- | --- |
| The three shared contracts | [NativeSharedProgramPilot.h](../../../engine/rts/Rendering/Backend/NativeSharedProgramPilot.h): `ProgramInterface` (generation/layout/stage/slot/resources), `MaterialContent` (immutable bytes and version owners), `PreparedSlice` (execution identity and retained owners). |
| Actual S type evidence | [NativePositiveBackingFacts.h](../../../engine/rts/Rendering/Backend/NativePositiveBackingFacts.h): private proof from a successful real cast; weak exact address/control-block owner; lease/context/submission; bounded 128 slots. |
| Actual S active consumer | [Metal4ModelTypeFacts.inc](../../../engine/rts/Rendering/Metal4/Metal4ModelTypeFacts.inc) and [WorldRenderer](../../../engine/rts/Rendering/Metal4/Metal4WorldRenderer.mm): `OriginalModelImage`, the local `ModelTypeFactsScope`, and the synchronous `sourceImages` callbacks at both provider sites. |
| Q shader source of truth | [compile_subset.py](../../../engine/tools/sepia-pilot/compile_subset.py), [variant GLSL](../../../engine/tools/sepia-pilot/identity-exposure-bias.frag.glsl), [generated 160B layout](../../../engine/rts/Rendering/Metal4/NativeSepiaGeneratedProgram.json). |
| Lua → native Sepia path | [staged widget](../../../engine/data/LuaUI/NativeWidgets/gfx_sepia_tone.lua), [generated Lua module](../../../engine/data/LuaUI/NativeWidgets/sepia_pilot_program.lua), [NativeSepiaLuaContext](../../../engine/rts/Lua/NativeSepiaLuaContext.h), [strict loader](../../../engine/rts/Lua/NativeWidgetAdapters.h), [postprocess adapter](../../../engine/rts/Rendering/Metal4/Metal4PostprocessNative.mm). Buffer/metadata/argument descriptor/C++ binding all use slot 1. |
| Q disabled prototype | [Metal4SharedProgramPilot.h](../../../engine/rts/Rendering/Metal4/Metal4SharedProgramPilot.h), [Metal4CUSSource.mm](../../../engine/rts/Rendering/Metal4/Metal4CUSSource.mm). The CUS binding-reuse pilot is OFF in every current test; do not attribute its hoped-for effects to S. |
| P UI state elision | [Metal4ScreenNative.mm](../../../engine/rts/Rendering/Metal4/Metal4ScreenNative.mm), invocation-local state proof; compiled default 0. |
| S host contract tests | [test_facts.cpp](../../../engine/tools/model-type-facts/test_facts.cpp), including ABA/control-block/expiry/scope/cap/fallback and 1,000 allocation-free hits. |

Read the bounded commit diffs before expanding:

- [release → P](https://github.com/yeojuny/BAR-Metal4/compare/0b0fd234af3bf5dd073ab7094350ec63fc3a7e04...2b82374573dff256fafc2e7c1733c867de7aafae)
- [P → Q](https://github.com/yeojuny/BAR-Metal4/compare/2b82374573dff256fafc2e7c1733c867de7aafae...10f09d4e7d24502cd28aab284cb6392b1fd95a0a)
- [Q → S](https://github.com/yeojuny/BAR-Metal4/compare/10f09d4e7d24502cd28aab284cb6392b1fd95a0a...8412adc758db31b0abed71db1f0a800d9bb68a1b)

Source changes are roughly two thousand added lines including tests, original widget reference and generated output. The initial PR's much larger line count is mostly the 10k-file verification manifest, not that much new renderer code. Subsequent commits add selected evidence, CSVs and fixtures without changing the frozen engine tree.
