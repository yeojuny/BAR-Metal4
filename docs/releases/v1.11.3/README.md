# v1.11.3 — BEGIN/PREPARE handoff and diagnostic integration

## Changes and defaults

The engine combines the frozen H source checkpoint with the reviewed J critical-path instrumentation correction. It keeps the capture-ready boundary and repaired BEGIN preparation overlap. J tracks incoming request, executing job and retired lease/submission identities separately; the bounded trace includes CPU reclamation and records emission outside the requested simulation window. K optimizations and V2 changes are excluded.

Normal launch selects PREPARE (worker=1, PREPARE=1, BEGIN=0). Capture/frame/prepare overlap remains enabled. Trace and Metal Validation remain off. Existing scenario selection, fullscreen resolution controls, original five AI choices, local content bootstrap/delta, 2× health bars and AA/VSync defaults are retained. The foreground handoff lobby ASAR is paired with its native AppKit foreground marker. Existing saved preferences are not globally overwritten.

## Install or upgrade

```sh
brew tap yeojuny/bar-metal4
brew trust --tap yeojuny/bar-metal4
brew install --cask yeojuny/bar-metal4/bar-metal4
# For an existing installation:
brew update
brew upgrade --cask yeojuny/bar-metal4/bar-metal4
open "/Applications/BAR Metal4.app"
```

Apple Silicon and macOS 27.2 or later are required. The separate Homebrew content formula is unchanged; first launch uses the local content installation. Allow at least 10 GB free. No GitHub authentication is required. The app is ad-hoc signed and not notarized; if macOS blocks it, approve the app under System Settings → Privacy & Security.

## Optional diagnostic launches

Quit the existing app before launching a different mode. These settings apply only to the directly launched process:

```sh
BAR_METAL4_HANDOFF_MODE=begin "/Applications/BAR Metal4.app/Contents/MacOS/BAR-Metal4"
BAR_METAL4_DIAGNOSTICS=critical-path "/Applications/BAR Metal4.app/Contents/MacOS/BAR-Metal4"
BAR_METAL4_HANDOFF_MODE=begin BAR_METAL4_DIAGNOSTICS=critical-path BAR_METAL4_METAL_VALIDATION=1 "/Applications/BAR Metal4.app/Contents/MacOS/BAR-Metal4"
```

The release launcher clears stale BAR_NATIVE pilot variables, then applies its explicit policy. `critical-path` collects every loop only for simulation 300–306. Trace records are CPU wall envelopes, not exclusive GPU activity or removable frame time. [Launcher policy](WRAPPER-POLICY.json).

## What was checked

One fresh integrated Ninja build completed 1,578 steps. The archive audit compared 1,538 objects across 35 archives with zero mismatches; the final target reported no work. Source hashes, depfiles and selected object/source identity were checked. The packaged engine relocates nine Homebrew imports to the existing bundled libraries and is signed afterward, so raw and deployed engine hashes differ. All 690 package manifest entries, symlinks and modes matched the ZIP restoration. Deep strict signature verification passed. [Build receipt](BUILD-RESULT.json) · [Archive closure](ARCHIVE-CLOSURE.json) · [Toolchain](toolchain.txt) · [Package manifest](MANIFEST.json).

Four purposeful app-first GUI scenario launches used one isolated profile, Pinewood_Derby_V1, one Barbarian opponent and actual automatic render size 1512×982. All four engines reached their completion target, shut down normally and logged lobby return. Physical final color samples were 1, effective MSAA was 0, VSync was off and health bars were 2×. Scenario map download, original five AI choices and unchanged content-delta inode/ctime reuse were observed.

| Case | Engine and diagnostic result | Outer application |
| --- | --- | --- |
| Clean PREPARE, trace off, Validation off | Simulation 300–1500: 4,797 draws / 39.9949989 seconds; gross smoke only | Fixture terminated Electron after an overly strict AA auto-value assertion |
| PREPARE, Metal Validation on | Normal engine shutdown; actual Validation banner, no detected Validation error | Foreground witness timed out while display was locked; fixture terminated Electron |
| BEGIN, Metal Validation and bounded trace on | 825 records / 50 buffers; 25 CPU reclaim kind-11 records; 168 complete three-identity rows; no dropped records; after-window emission checks passed | Actual game:closed IPC, lobby return and normal window close; exit 0 |
| PREPARE, bounded trace on, Validation off | 837 records / 54 buffers; highest kind 29; 182 complete three-identity rows; no dropped records; after-window emission checks passed | Actual game:closed IPC, lobby return and normal window close; exit 0 |

The first two raw FAIL results remain unchanged. The fixture correction accepted effective AA=0 with physical samples=1, and separated locked-display foreground observation from game:closed IPC. Neither correction changed the product or triggered repeat games. The display was locked, so foreground appearance, comparison screenshots and whole-frame visual equivalence were not verified. No new cross-Mac gameplay, long-duration stability or statistical performance-equivalence claim is made. The clean smoke is a different scene/population from historical ~185 FPS H/I tests and is not a before/after comparison. [Complete runtime receipt](RUNTIME-RESULT.json) · [Fixture revision](FIXTURE-REVISION.json).

The signed app retains an immutable internal runtime=PENDING field written before execution. This external receipt supersedes that field for the checks above without changing the tested signed bytes.

## Source restoration and provenance

The exact compiled source is commit `ddd31f55961d6e715283ea48d03a2b35559128f2`, engine tree `596f6604ab7ed636dbe2cd5fa36fdc01c02f342e`, based on H checkpoint `e9acbc66115c5fecd872f3385db248371362b97b`. The release tag adds distribution policy and documentation; its engine tree is identical. The 10,231-entry engine source archive includes the four vendored simdjson dotfiles that the earlier handoff archive omitted. Upstream engine VERSION remains `2026.09.01-24-g01efdf4 master`; the app version is 1.11.3/build11103.

[Source freeze](SOURCE-FREEZE.json) · [Full engine tree](ENGINE-TREE.json) · [J patch](prepare-critical-path-diagnostic.patch) · [J source file hashes](J-SOURCE-FILES.json) · [J ownership review](J-OWNERSHIP-REVIEW.json) · [J host checks](J-HOST-CHECKS.json).

To reconstruct the engine build, restore `engine/` from the source commit/tag, install the external dependencies referenced by the frozen `initial-cache.cmake`, and use the recorded Xcode/SDK/toolchain versions. Configure a new build directory with `cmake -S engine -B build -G Ninja -C docs/releases/v1.11.3/initial-cache.cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`, then `cmake --build build --target spring-metal4-scene -j 8`. This is the recorded configuration, not a claim of bit-for-bit reproduction on an independent computer. The source and dependency provenance must remain with redistributed binaries. Packaging uses the v1.11.2 canonical app/content/19 libraries, the source-matched foreground lobby ASAR `566ed180…`, the new engine, the release launcher, app version metadata, relocated imports and nested ad-hoc signing. [Native engine manifest](NATIVE-ENGINE-MANIFEST.json).

The inherited LuaVBO invalid-index diagnostic argument correction is adapted from tomjn/RecoilEngine PR #3695, commit `29b950671fd883d2e01bc2cf1f6f6d0b7168280a`. The historical PR #3035 affinity and benbreen QoS experiments are not present in this H/J engine lineage. [Contribution applicability ledger](https://github.com/yeojuny/BAR-Metal4/blob/e9acbc66115c5fecd872f3385db248371362b97b/docs/checkpoints/begin-prepare-20261114/UPSTREAM-CONTRIBUTIONS.json). Existing BAR/Recoil/lobby and bundled dependency notices remain in the package and repository.
