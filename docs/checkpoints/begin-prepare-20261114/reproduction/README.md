# Restore the checkpoint source

## Checkout the recorded source

Clone the public repository and select the checkpoint branch. The source includes all three patch stages.

```sh
git clone --branch checkpoint/begin-prepare-20261114 https://github.com/yeojuny/BAR-Metal4.git BAR-Metal4-checkpoint
cd BAR-Metal4-checkpoint
git rev-parse HEAD:engine
```

The engine tree must be `ad2cfb3c1b795d1069fb4421e59b1fead785cb7c`. The [complete engine tree manifest](../FULL-ENGINE-TREE.json) records path, file mode and Git blob for all10,229 tracked entries. [Final source hashes](FINAL-SOURCE-FILES.json) cover the16 changed files.

## Recreate it from c8

In a separate empty clone, checkout `c8a8ac929cb69b40a3bdb87543daa855963b4b90`. Keep a copy of this reproduction directory outside that checkout before switching revisions. Then apply the patches in this exact order from the repository root.

```sh
git apply --directory=engine /path/to/reproduction/parent-capture-ready.patch
git apply --directory=engine /path/to/reproduction/begin-functional.patch
git apply --directory=engine /path/to/reproduction/begin-prepare-overlap.patch
```

Check each `engine/<path>` SHA-256 against FINAL-SOURCE-FILES.json. Keep the4 simdjson dotfiles from the Git base. The original source archive omitted them, and patch restoration must not treat that omission as a deletion.

## Build inputs and limits

The owner completed a normal CMake/Ninja build on its macOS toolchain. The [cache template](initial-cache.template.cmake) records that configuration and paths to its installed Xcode and Homebrew dependencies. Review dependency versions and set @SOURCE@ and @BUILD@ to the new absolute engine/build roots before using it. Any other unresolved placeholder marks a host-local input that must be supplied. Build paths, toolchain and generated debug data can change the output hash.

```sh
cmake -S /path/to/checkpoint/engine -B /path/to/new-build -G Ninja -C /path/to/filled-in-cache.cmake
cmake --build /path/to/new-build --target spring-metal4-scene -j 8
python3 /path/to/reproduction/audit_build_closure.py /path/to/new-build /path/to/archive-closure.json
```

The archive audit compares members with the current loose objects. The source manifest and normal dependency build establish source identity separately. These instructions restore engine source and the recorded local build configuration. App signing, bundled runtime dependencies, personal test profiles and game content are separate inputs.

## Mode settings

Both options require the matching engine family and worker/capture boundary configuration. Use the public measurement records to check all common flags.

| Policy | BEGIN | PREPARE | BAR_NATIVE_BEGIN_PREPARE_OVERLAP |
| --- | --- | --- | --- |
| Recommended PREPARE | 0 | 1 | 0, or1 with the gate effectively off |
| Fixed BEGIN | 1 | 0 | 1 |
| Old BEGIN comparison | 1 | 0 | 0 |

The mode variables are `BAR_NATIVE_BEGIN_READY_PILOT` and `BAR_NATIVE_PREPARE_READY_PILOT`. Common controls are `BAR_NATIVE_INTERVENTION_RENDER_WORKER=1`, `BAR_NATIVE_CAPTURE_READY_BOUNDARY=1`, `BAR_NATIVE_FRAME_OVERLAP=1`, `BAR_NATIVE_CAPTURE_OVERLAP=1`, and `BAR_NATIVE_PREPARE_OVERLAP=1`. Both mode flags ON select PREPARE in this source. Session source-credit capacity is fixed at creation. Mode changes are launch-time policy choices.
