# Terrain T2: definition and link-readiness boundaries

AI-assisted experimental source work. Same isolated branch as T1; main, releases and Homebrew unchanged. This follows the [upstream handoff](https://github.com/yeojuny/BAR-Metal4/blob/7f37af54f8a95926a8474f8acb595a68e98ae50a/docs/research/BAR_Native_Metal4_Upstream_Refactor_Handoff_20261112.md). Baseline T1 commit: `11b97606c8ed18742bdc869ce0221ef307a86789`.

## T2a: shared definition, separate uniform content

Uniform publications now retain a shared immutable `NativeTerrainProgramDefinition` containing identity and consumer, independently of their uniform snapshot. The definition is built lazily at the first publication of an owner state; uniform writes reuse it. Reload/source lifetime transitions, Release and unsupported/import transitions invalidate it. The current-publication check retains identity/state/consumer/content checks and also checks exact definition ownership.

The downstream `NativeTerrainProgramSnapshot` remains unchanged and is assembled at capture. Consequently, this removes definition copying **per uniform publication**, not per draw capture. It deliberately preserves all existing definition/consumer validation boundaries and does not claim a complete draw-level immutable certificate migration.

`definitionCopies` is a diagnostic-only counter alongside the T1 counters; it counts definition materializations, including the first Enable. A changed uniform after Enable should produce mapCopies=1/publications=1/definitionCopies=0. T1's aligned no-op remains allocation-free. Named inactive values and link defaults remain untouched.

## Remaining gates

Source definition sharing by itself does not make a consumer deeply immutable. `OriginalProgram::LibraryGeneration/Supports` originally read mutable `pipelines`; the separate T2b change addresses only that source-readiness seam. Actual target/MSAA/image specialization and device validation must remain in prepare.

Neither source tests nor syntax extraction establish Objective-C++/Metal compilation, GPU failures, graphical parity, gameplay determinism, or a performance gain. These remain required on the selected Mac before acceptance. T3 lazy sealing cannot proceed by merely moving a snapshot to Draw: all Current/Restore observation and save/restore callers must first be classified. T4 packing and later worker/queue changes are not implemented here.

## T2a host result

`python3 engine/tools/metal4-contracts/run_native_terrain_program_test.py --output /tmp/terrain-t2a-tests`

59 checks pass with ASan/UBSan and diagnostics-off syntax compilation. Tests add changed-write definition sharing, reload/failed-compile separation, unsupported/invalid/observed Lua imports, and Release. Actual Lua VM and Metal compiler are stand-ins. `T2A-RESULTS.json` records exact source digest and compiler. Earlier T1 results are historical evidence for commit 11b9760, not new claims about this updated source.
