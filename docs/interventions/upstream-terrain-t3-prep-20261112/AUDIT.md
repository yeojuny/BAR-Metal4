# T3 observation-boundary audit (preparation, not lazy sealing)

Baseline: `e4beaa3eb9856115508635f1147bf8a36bc220ae`.
AI-assisted source audit; no Lua VM, Metal, gameplay, concurrency or performance result.
`CALLERS.json` freezes baseline lines, enclosing symbols, classifications, source hashes and exclusions. These lines intentionally do not follow later API-name migrations.

## Inventory

25 source locations: 8 inspect-owner, 3 inspect-kind, 3 capture/read, 6 save-restore, 5 teardown. One UnitStencil location contains two Restore expressions (normal and exceptional exits). There are three actual callback save sites, not six independent saves:

- `LuaShaders::ActiveShader`: owning selection saved before `lua_pcall`; restored after GL binding restoration, before `lua_error` propagation.
- UnitStencil `ActiveShader`: aliased `binding` saves previous selection; normal and catch paths restore it.
- UnitGlass `ActiveShader`: normal and catch paths call `RestoreGlassSelection`. The helper checks whether a saved Glass program was deleted or its context closed/expired; in that case it unbinds instead of restoring.

The injected `NativeWaterGroundSourceCapture::programs` binding reference only calls `TerrainOwner()`; it introduces no hidden Current/Restore site. `NativeParticleAtlasSource`'s `ScopedBinding::Restore()` is a **texture** guard and is excluded. Likewise uniform-capture and UI-recorder Current methods are separate state domains.

## Safe preparatory API split

- `CurrentKind`: scalar inspection, no retained selection or future content seal.
- `InspectCurrent`: borrowed, immediate inspection only. Do not retain this reference across Bind, Unbind, Restore, Lua callbacks, deletion or context reset. Copy any selected owner needed beyond that boundary.
- `SaveCurrent`: owning state observation at callback save boundaries. A future lazy writer must seal before this method returns, not after the callback.
- Keep compatibility `Current` behavior unchanged during this preparation. Give capture/read sites a separate classification even if they still call compatibility Current.

Most Active methods only check a selection, copy a typed shared owner and return it. They can use inspection without sealing a terrain writer merely because another family checks its active program. `SourceProgram::OwnsCurrent` and its aligned-no-op test are especially important: sealing at these internal checks would undo T3's intended allocation reduction.

Do not blindly replace owning copies with references. `UnitGlass::Context::Retire` copies selection before a loop that can unbind and reset program owners. Preserve its owning copy. Particle-shadow `Reset` is `noexcept`; a future seal must not be introduced there. Terrain destruction should inspect provenance and unbind without allocation or lazy seal.

`CaptureCurrentNativeTerrainProgram`, GenericShadow `Snapshot`, and UnitGlass `DrawPiece` are content/owner observations. They retain a program/uniform or typed draw payload. They are not merely kind queries.

## Lifecycle and error boundaries

Terrain source context identity is checked by `CheckContext`. Reload changes source revision; Lua import changes original-program lifetime. Release clears definition, consumer and named state; a stale selection must not silently become current. Destruction unbinds a currently selected terrain program and removes it from `livePrograms`, but previously saved selections can still exist.

Baseline risk: a saved terrain definition retains a raw `identity.originalProgram` provenance pointer, **not the SourceProgram object**. Saving, destroying the source, restoring the selection, and capturing must not dereference that expired pointer. A bounded preparation fix is to compare the saved address against existing live source entries first, then use the matched live object for `MatchesCurrentPublication`. Exact definition/context/generation comparison must remain to reject address reuse. This is not permission to weaken lifetime checks or to make raw pointer equality a content identity. The companion implementation/test must establish this guard; this audit alone is not a runtime proof.

UnitGlass explicitly suppresses restoration of deleted/closed Glass selections. UnitStencil and generic LuaShaders do not implement that same guard. Preserve those existing family-specific semantics in an API naming change; do not invent a global restoration policy in T3 prep. Typed Active methods subsequently check deleted/context state. A whole-engine context reset or owner destruction is not made safe merely by retaining a Selection.

The Current(A), Write(B), Restore(A), Write(B) counterexample remains essential. A future lazy representation must distinguish the writer's current content from the saved binding's content, and preserve previous captures unchanged. Restoring a snapshot does not automatically rewind the mutable writer.

## Next implementation gates

Before enabling lazy sealing:

1. Save/capture boundaries seal exact state; inspection does not allocate or seal.
2. Nested callback success and failure restore the exact saved selection; caller error ordering remains intact.
3. Captured A survives changed B and subsequent reload/deletion/context transitions without mutation or dereferencing expired sources.
4. Shader reload preserves named values, including temporarily inactive names, under existing link-default behavior.
5. Runtime program/target/GPU preparation checks remain distinct from source observation.
6. No source writer/RNG/Lua callback is skipped, duplicated or moved to another thread.

This preparation establishes explicit vocabulary and lifecycle tests. It does not yet reduce setter map copies via lazy snapshots, change GPU commands, prove async frame ownership, or establish a speedup.
