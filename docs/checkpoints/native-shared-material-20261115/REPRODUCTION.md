# Reproduction from published source and frozen small inputs

## Build the actual integrated source

Check out this branch/immutable commit. `engine/` is the complete P/Q/S source; do not apply only `SOURCE.patch` (that patch is Q→S). The source hash/mode manifest is [FULL-SOURCE-FILES.json](FULL-SOURCE-FILES.json). The actual Mac build used Xcode 27.2 / Apple clang 21, CMake 4.5, Ninja 1.13.2 and the native arm64 dependency layout in the recorded cache. See [S build commands](evidence/S/BUILD-COMMANDS.json), [receipt](evidence/S/BUILD-RECEIPT.json), [frozen cache](evidence/S/initial-cache.cmake), [archive identities](evidence/S/ARCHIVE-CLOSURE.json), [configure log](evidence/S/configure.log), and original [missing-pkg-config configure failure](evidence/S/configure.initial-missing-pkgpath.log).

From repository root, after installing the dependencies referenced by the cache:

```sh
export PKG_CONFIG_PATH="/opt/homebrew/opt/openal-soft/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
cmake -S engine -B build-S -G Ninja -C docs/checkpoints/native-shared-material-20261115/evidence/S/initial-cache.cmake
cmake --build build-S --target spring-metal4-scene -j8
```

The recorded build performed one fresh compilation, 1,538 objects / 35 archives with zero mismatch, five changed-source freeze checks and a final no-work check. Missing OpenAL pkg-config search path was corrected before compilation; no engine source was corrected during that build. The exact Mac-compiled source is the frozen S input preserved here. Independent-machine build/bit-for-bit reproduction has not been run by this publication task. The signed historical engine is identified by SHA, not attached as a new app.

For a private runtime, use a separate copy of the v1.11.3 native directory/libraries/data, replace its engine with your built engine, apply the recorded [signing/loader relocation procedure](evidence/S/sign_private.py) with paths adapted to your copy, and retain original notices. Do not modify the shipped installation or claim the newly built hash equals the historical signed hash. [Candidate import relocations](evidence/S/CANDIDATE.json).

## Stage the two native Lua inputs together

Copy `engine/data/LuaUI/NativeWidgets/gfx_sepia_tone.lua` and `sepia_pilot_program.lua` to your private native `data/LuaUI/NativeWidgets/`. Exact copies are also under [reproduction/native-data](reproduction/native-data/LuaUI/NativeWidgets). The wrapper SHA-512 `2a2746ca…1c486fd` must match `NativeWidgetAdapters.h`; wrapper SHA-256 is `3e43f742…073d37`, generated module SHA-256 `1c77c4de…ea13a9`. Full values are in [staging receipt](evidence/Q/PACKAGING-DATA-STAGED.json). A source-only patch without installing these files is incomplete.

The historical 128-byte packet is not the current Q variant. If deliberately regenerating the current source inputs, use the original generator with its actual argument:

```sh
cd engine
python3 tools/sepia-pilot/compile_subset.py --fragment tools/sepia-pilot/identity-exposure-bias.frag.glsl
```

This emits the matching 160-byte MSL/C++ metadata/Lua combination. Do not hand-edit offsets or introduce effect-specific production branches to make a test pass. Slot 1 and offsets viewPos48/exposure64/bias80/gamma96/colorR112/colorG128/colorB144 are the current recorded layout.

## Use the exact prelaunch fixtures

The small [frozen-inputs](reproduction/frozen-inputs) directories contain source-backed controllers, macro overlay, settings, BC213/BYAR widget config, pacing widgets, and actual private generated-module override where used. These were reconstructed with the existing preparation transformations and **50 recorded prelaunch file SHA-256 checks matched**. No game was launched for this reconstruction. [Checks](reproduction/FROZEN-INPUT-VERIFICATION.json).

`reproduction/profiles/` separately retains selected post-run input archaeology; some settings/configs were serialized again during shutdown. It is not an exact prelaunch baseline. [Difference inventory](reproduction/PROFILE-INPUTS.json). Prefer `frozen-inputs/` for reproduction.

| Fixture | Purpose |
| --- | --- |
| S-clean | Sepia off, clean type-facts comparison; change only MODEL_TYPE_FACTS 0/1 across arms. |
| S-diagnostic | Same scene with sampled trace/Validation/image; retain power-transition caveat. |
| Q-F1-old | Old MSL with the new capture frontend, initial identity uniforms. |
| Q-F2-shared | Shared identity; preserves the failed same-session DisableWidget attempt. |
| Q-F3-variant | Successful fresh-profile initial exposure1.25 / bias0.1,0.2,0.3. |

Each `EFFECTIVE-LAUNCH.json` records the **actual successful historical direct-engine argv and child environment**, with only local directory slots parameterized. The shared controller is sim300–1500, original fixed seed/macro/camera, approximately14-second wall warmup (pause at180, ack by240, minimum10 seconds), then40 seconds counting. These are diagnostic engine-direct tests, not fresh app-first production acceptance tests.

The scene requires the original BAR content and `All That Simmers v1.1.1`. Content/game pools and large map archives are not copied into Git. Install the existing content formula and obtain that map through the lobby/content source; compare map/game checksums against the supplied run RESULT/LAUNCH records before claiming a matched benchmark. The private custom macro's two source members and exact small archive are provided under [common/macro](reproduction/common/macro) and [common archive](reproduction/common/bar-commander-control-213c1.sdz). Copy it into the fresh test data `games/` directory, not a production game installation.

For example, set NATIVE to your separately signed native copy, DATA to a fresh copy of `frozen-inputs/S-clean`, ASSETS to your isolated installed content, STATE/PRIVATE_HOME to new private directories. Use the argv already recorded:

```sh
env HOME="$PRIVATE_HOME" BAR_STATE_PATH="$STATE" BAR_ASSETS_PATH="$ASSETS" \
  SPRING_DATADIR="$NATIVE/data:$ASSETS" BAR_MACOS_TARGET_DISPLAY="0,0,1512,982" \
  BAR_METAL4_EXPERIMENTAL_SINGLEPLAYER=1 BAR_NATIVE_MODEL_TYPE_FACTS=1 \
  BAR_NATIVE_SCREEN_STATE_ELISION=1 BAR_NATIVE_PREPARE_READY_PILOT=1 \
  BAR_NATIVE_BEGIN_READY_PILOT=0 BAR_NATIVE_INTERVENTION_RENDER_WORKER=1 \
  BAR_NATIVE_CAPTURE_READY_BOUNDARY=1 BAR_NATIVE_BEGIN_PREPARE_OVERLAP=1 \
  BAR_NATIVE_FINAL_COLOR_STAGE=1 BAR_NATIVE_FRAME_OVERLAP=1 \
  BAR_NATIVE_CAPTURE_OVERLAP=1 BAR_NATIVE_PREPARE_OVERLAP=1 \
  BAR_METAL4_SHARED_CUS_PILOT=0 BAR_NATIVE_FRAME_TRACE=0 MTL_DEBUG_LAYER=0 \
  "$NATIVE/bin/spring-metal4-scene" --write-dir "$DATA" --isolation --only-local "$DATA/script.txt"
```

This abbreviated example highlights the settings; **use the complete environment in EFFECTIVE-LAUNCH.json**, including existing pass flags and all disabled trace/diagnostic settings, for a controlled comparison. Start from a clean environment so stale BAR_NATIVE/MTL flags cannot leak in. The public outer `BAR-Metal4` launcher deliberately clears BAR_NATIVE values; exporting MODEL_TYPE_FACTS/ELISION then running that wrapper does not enable the experiment. Historical runners invoked the private signed engine with an explicit environment after profile preparation. Keep that distinction.

Validate actual child settings using the supplied [CHILD-MODE examples](evidence/S/measurement/clean_paced/preparation/runs/A1_OFF/CHILD-MODE.json) and log witnesses, not parent shell assumptions. Inspect only the owned child PID and whitelist the named BAR/MTL keys; do not publish a full process environment. For S diagnostics, retain sim300–306 count kinds30–35, zero drops/CPUinvalid/scopeMismatch, and after-Join emission. No provider wall timing claim is valid while Clock is compiled off. Q CUS pilot stays0 and Sepia stays off in clean S runs.

Q's production generated module contains default uniforms. The successful functional profile installs a **separate private RAW_FIRST module** that reads Spring.GetConfigFloat values and overrides only the returned uniforms table. The shader strings stay identical. Existing portable versions of the original [assembler](reproduction/sepia/assemble_private_fixture.py) and [successful F3 assembler](reproduction/sepia/assemble_variant_once_fixture.py) are supplied; only the controller path was made relative. Their widget-proxy/hot-relink limitation is deliberately retained, not fixed during preservation.

## Recompute existing measurements without running an engine

```sh
python3 -B docs/checkpoints/native-shared-material-20261115/reproduction/analyze_p_tails.py
python3 -B docs/checkpoints/native-shared-material-20261115/reproduction/analyze_r_profile.py
```

These are the successful existing parsers with only input/output path slots adapted (and lossless gzip input for the profiler XML). They write new P/R-REANALYSIS files; `ANALYSIS_OUT` can redirect output to a scratch path. R filters the recorded owned PID, all threads and Running states, resolves ID/ref stack frames, and preserves inclusive overlaps. The original failed profiler export and parser sources remain in evidence. Trace containers themselves are omitted; their component checksums/sizes/original storage tokens are in [OMITTED-ARTIFACTS](OMITTED-ARTIFACTS.json). The public normalized XML is sufficient to reproduce the selected analysis; original XML bytes are privately preserved by manifest hash.
