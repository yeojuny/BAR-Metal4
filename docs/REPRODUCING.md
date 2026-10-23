# Reproduction starting points

## Verify the public source

From the repository root:

```sh
python3 - <<'PYCODE'
import hashlib, json, pathlib
for row in json.load(open("SOURCE-MANIFEST.json"))["files"]:
    p = pathlib.Path(row["path"])
    assert hashlib.sha256(p.read_bytes()).hexdigest() == row["sha256"], p
print("Published source hashes match")
PYCODE
```

## Engine build recipe (not clean-machine validated)

Use an Apple Silicon Mac with a Metal 4-capable macOS and matching Xcode SDK. The code uses C++23 and Objective-C++. Read `engine/README.markdown` and the CMake configuration for dependency requirements. Native integration explicitly requires SDL3, FreeType, Fontconfig and DevIL; other engine dependencies remain applicable.

The target and switches below are present in this snapshot. This is a starting recipe, not a claim that installing these prerequisites alone makes the complete app reproducible:

```sh
cmake -S engine -B build-native -G Ninja \
  -DRECOIL_USE_SDL3=ON \
  -DBUILD_spring-metal4-scene=ON \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-native --target engine-metal4-scene
```

Vendor source is included where available in the export. An export does not retain submodule Git metadata: `git submodule update` cannot recover missing files by itself. Report the exact missing dependency and configure output if blocked. Do not reuse a developer's absolute build paths or assume an incremental local link proves a clean build.

## Lobby

`lobby/package.json` records Node 22.18.0, Electron and commands; `package-lock.json` pins package resolution. Review install scripts before running `npm ci`. Upstream media is omitted, so packaging requires a separately reviewed asset set. The macOS package script alone does not establish native-engine/content/runtime closure.

## Gameplay reports

No complete portable runtime or game/map pack is shipped. A meaningful gameplay reproduction needs the exact engine SHA-256, legally obtained game/map versions, AI, seed, scenario, settings, camera, power mode and trace configuration. Record startup failure separately from rendering mismatch and performance. Compare matching conditions, keep diagnostics off for timing comparisons, and provide only a short sanitized log excerpt. Do not upload accounts, credentials, replays containing personal data, or private system paths.

## App/Homebrew release work still required

Produce a fixed portable app export, complete matching source, dependency/license inventory, clean-user launch evidence and immutable release archive SHA-256. Check minimum OS, arm64 architecture, library load paths, signing status and absence of developer-local state. Then add a separate native Metal 4 cask. No quarantine bypass or fabricated download URL is supplied.
