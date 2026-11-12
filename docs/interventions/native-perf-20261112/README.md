# Separate intervention: combined native performance candidate (2026-11-12)

Owner-requested, AI-assisted experimental branch. Original `main` and `review/native-progress-20261110` are preserved. Base: `db4725a7490ff6dcc8d130646f76e28ebb9b7eae`. Tracking: [#2](https://github.com/yeojuny/BAR-Metal4/issues/2).

**Source candidate, not a validated app or Homebrew release. No promised FPS gain.**

## Effective source

The 31 files provided by `docs/research/native233/OVERLAY-MANIFEST.json` were hash-verified and materialized into their real `engine/` paths before intervention. Historical overlay, results and manifests remain unchanged. This distinguishes the native233 candidate from the older root engine snapshot. The published overlay lacked two matching UI adapter inputs; the source-closure correction below pairs the available published UI sources without weakening identity checks. The corrected expanded candidate compiled and linked on the test Mac.

## Eight proposal areas and actual scope

| Proposal | Implemented here | Not implemented / remaining |
|---|---|---|
| 1. Dedicated renderer, more in-flight frames | Three recording slots, bounded two-submission queue, oldest-event **and** feedback retirement; serial render worker with pinned main-thread surface snapshot. | Dependency checks still drain; main waits for worker (`simulation_overlap=0`). Truly asynchronous next-simulation overlap is **not** implemented. |
| 2. Pools/views/uniforms | Terrain/light argument and uniform pools, bounded terrain mip and shadow-color view caches. | Reuse remains completion-owner gated with fresh fallback. |
| 3. Persistent residency | Explicit immutable cached resources use bounded immutable set snapshots, frame-retained; dynamic fallback. | 256 MiB / 8,192 allocations / 64 sets; pointer lookups and set binding remain. |
| 4. Passes/barriers | Proven CPU-only/zero-draw phases keep encoder without unsupported in-pass waits; host-only initial MRT vertex wait narrowed. | Nonempty GPU draw phases still split; no arbitrary RTT hoisting or blanket barrier removal. |
| 5. G-buffer reuse/tile shading | Sealed209 zero-reader/identity/version contract extended to Lua-only feature writers, skipping eligible copies. | No arbitrary alias, memoryless/imageblock/tile-shader redesign. |
| 6. Shader math | Process-latched fast-math experiment expanded to 27 renderer families including terrain/CUS/SSAO/water/effects. | Depth-copy/shadow/UI families and CPU simulation remain conservative; visual correctness unverified. |
| 7. Hot-loop structures | Two fixed-width PSO keys, selective guaranteed-overwrite binding resets, immutable world textures use staged Private path, renderer-only O3/FMA + supported IPO option. | Existing maps retained; optional buffers/all sampler-texture bindings still reset; no simulation optimization flags. |
| 8. Existing opt-ins | Combined profile explicitly enables existing and new experiments for one invocation. | No default distributed release promotion. |

All eight proposal areas now have an experimental path or explicit retained boundary. This does **not** mean every architectural suggestion is fully implemented. In particular, asynchronous simulation overlap, general nonempty pass fusion, and tile shading remain outside this candidate. The source diagnosis's 270–300 FPS / 20–35% forecast remains unverified.

## Lifetime and rollback properties

Argument slots keep existing zero-binding resets. Prepared objects hold pool ownership tokens; a slot is not writable while another token owns it. Uniform reuse uses existing bounded pools (64 entries / 16 MiB each) with fresh-allocation fallback. Mip views use strong source identity plus base/count; cache retention is bounded to 256 entries and 64 MiB of reported source storage, skipping unknown footprints. Each lease still registers its resources. No original safety waits or completion ownership graph were removed.

All new rendering experiments are off unless explicitly enabled. The fixed-width keys are unconditional and semantics-preserving. For baseline comparison use a separate checkout of the pinned base with the original native233 overlay materialized. Merely disabling new flags retains the materialized native233 version, not the v1.11.2 root snapshot.

## Download / build / run

The intervention source is published on the separate [`intervention/native-perf-20261112`](https://github.com/yeojuny/BAR-Metal4/tree/intervention/native-perf-20261112) branch. The initial published tree exactly matches local source snapshot `ab5586b17972957d918c1299e85b5766721c9460`; the original branches are unchanged. GitHub's source ZIP is source code, not a directly runnable Mac app. A separate test app is being packaged; it is not yet gameplay-qualified.

Read `docs/REPRODUCING.md` and upstream `engine/README.markdown` first. Repository starting recipe:

```sh
cmake -S engine -B build-native -G Ninja \
  -DRECOIL_USE_SDL3=ON -DBUILD_spring-metal4-scene=ON \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBAR_NATIVE_INTERVENTION_RENDERER_OPTIMIZE=ON \
  -DCMAKE_OSX_SYSROOT="$(xcrun --sdk macosx --show-sdk-path)"
cmake --build build-native --target engine-metal4-scene
```

The local M1 iMac had macOS 27.3 and CLT SDK 27.2; original RuntimeNative.mm syntax-check passed without full Xcode. Official Homebrew SevenZip was installed; OpenAL used its existing pkg-config path (`/opt/homebrew/opt/openal-soft/lib/pkgconfig`). Explicit SDKROOT resolved libunwind discovery. The expanded candidate compiled and linked successfully, and its version commands run. Scenario loading reaches map/AI/Lua, but a window-close request followed by SDL_QUIT ends loading before simulation frame 0, also with new experiments disabled. The request origin is unknown; exit 0 is not gameplay acceptance. No security settings changed and no installed app was overwritten.

After building, pass the actual candidate engine and your existing legitimate content/start-script arguments:

```sh
./docs/interventions/native-perf-20261112/run-candidate.sh /absolute/path/to/new/spring-metal4-scene [arguments]
```

The launcher neither downloads game assets nor creates a complete portable app. Do not pass the old installed binary expecting it to contain these changes. New flags: `BAR_NATIVE_INTERVENTION_POOLS`, `BAR_NATIVE_INTERVENTION_HOST_VERTEX`, `BAR_NATIVE_INTERVENTION_FAST_MATH`. Unset them for new-feature-off comparison. To fully roll back, continue using the unchanged installed v1.11.2 app.

## Reproducible portable checks

```sh
c++ -std=c++17 -Wall -Wextra -Werror -pedantic -Iengine/rts \
 docs/interventions/native-perf-20261112/tests/pipeline-key.cpp -o /tmp/bar-key
/tmp/bar-key
```

Key test: 10,000 deterministic old-vector/new-array cases, all masks/coverage, full-width sample keys, every field perturbation, lookup and map ordering. Pool test: busy tokens across batches/completion, clear lifetime and capacity fallback. These are host-only checks, not a Metal app test.

## Release gates

- [x] Preserve original branches, historical overlay and installed application
- [x] Host-side key and pool tests
- [x] First bounded candidate: five Objective-C++ units and full native link pass
- [x] Expanded candidate: all changed translation units compile
- [x] Full native link and changed-binary version/startup launch
- [ ] Gameplay reaches and sustains simulation frames
- [ ] Same scene/settings/camera pixel and validation checks
- [ ] Matched A/B runs including 1% low and memory high-water
- [ ] User's local trial
- [ ] Package/source/licenses/checksums and Homebrew update

No unrelated TA assets, credentials, private profiles, game packages or maps are added. The existing Homebrew cask/release stays unchanged pending the user's trial and release qualification.

## Expanded candidate flags

`run-candidate.sh` sets `BAR_NATIVE_INTERVENTION_TWO_INFLIGHT`, `RENDER_WORKER`, `PERSISTENT_RESIDENCY`, `POOLS`, `PRIVATE_TEXTURES`, `HOST_VERTEX`, `EMPTY_PASS_FUSION`, `MRT_LUA_REUSE`, and `FAST_MATH` (all share the `BAR_NATIVE_INTERVENTION_` prefix). They are explicit experiments, not validated defaults. The renderer worker retains main-thread synchronization rather than claiming CPU simulation parallelism.

Host tests additionally cover persistent-budget limits, guaranteed-overwrite reset masks, empty-pass negative cases, worker serial execution/exception propagation/recursive rejection. LeakSanitizer is unavailable under the cloud ptrace environment; ASan/UBSan runs with leak detection disabled are not a leak-test pass.

## Reproducibility correction: paired UI adapter sources

Actual map/AI startup exposed two unpublished native233 UI adapter pins: healthbars `ff3d7f…c554635` and options `51cb9f…499d237`. Their matching Lua bytes are absent from the published overlay and installed v1.11.2. The bundled public `display-overlays` Lua files and the original root adapter header are an exact SHA512 pair (`ba1bf6…1342411`, `72e144…05ddbf7`). No native healthbar implementation/shader changed in native233. The intervention therefore retains the published v1.11.2 UI pair by restoring its adapter-pin header; strict identity checks remain enabled. Historical native233 overlay is untouched. This is an explicit mixed-version UI provenance choice, not reproduction of unavailable native233 UI bytes or removal of a guard. The failed startup returned exit0 at frame-1, which is not gameplay acceptance.
