# BAR Lua graphics / Metal material contracts

Local Codex-assisted implementation, based only on official pinned BAR/Recoil
sources. No synced Lua changes, general GLSL compiler, engine API replacement or
full material/visual acceptance is supplied by this directory.

- `BarMaterialSemantics.h`: portable value functions in
  `spring::metal4::bar`, explicit vertex upload fields and bit decoders.
- `BarMaterialSemantics.metal`: matching `Bar*` functions, suitable for inclusion
  or concatenation before a native diagnostic shader.
- `bar-material-contract.json`: equations, input/channel/packing/coordinate
  contract and explicit unresolved material behaviors.
- `lua-graphics-inventory.json`: source hashes, engine registrations, observed
  game API references, aliases, shader-stage candidates and required operations.
- `inventory.py`: deterministic regeneration from pinned source trees plus the
  five official `modules/graphics` supplements. Reads sources; writes only the
  requested output. Detects actual blob differences; separately records CRLF
  checkout normalization. It does not fetch or modify reference repositories.

The C++ and MSL material structs describe values. **They are not a shared buffer
ABI.** In particular, MSL `float3` layout must not be inferred from a C++ array.
Use explicit vertex descriptors/packing. The base decoder excludes define
overrides, damage and normal-map roughness adjustment, and preserves continuous
alpha separately from deferred binary alpha. Gamma follows the game's explicit
power law; automatically enabling sRGB sampling would change its default.

The Fresnel/visibility/distribution/tonemap functions are a narrowly translated
subset. They do not assemble BAR lighting, lookup textures, shadows, exposure or
postprocessing. GGX visibility uses a branch at zero denominator to avoid
evaluating an undefined division inside GLSL `mix`; this small numeric adaptation
is recorded in the contract.

From the repository root, run the portable checks with a native C++ compiler:

```sh
contract_build_dir="${TMPDIR:-/tmp}/bar-metal4-contracts"
mkdir -p "$contract_build_dir"
c++ -std=c++20 -Wall -Wextra -Werror -pedantic -O2 \
  tools/metal4-contracts/TestBarMaterialSemantics.cpp \
  -o "$contract_build_dir/bar-material-semantics-test"
"$contract_build_dir/bar-material-semantics-test"
python3 tools/metal4-contracts/test_inventory.py -v
```

Regenerate the inventory using read-only official checkouts at the source pins
below. Replace the example paths with your own checkout and supplement locations:

```sh
python3 tools/metal4-contracts/inventory.py \
  --bar-root /path/to/Beyond-All-Reason \
  --recoil-root /path/to/RecoilEngine \
  --bar-supplement-root /path/to/bar-069f573 \
  --output tools/metal4-contracts/lua-graphics-inventory.json
```

The supplements are exact official raw files downloaded at BAR commit
`069f57319503b635698ebb855f11255443e4018b`; their content is verified against the
pinned Git blob IDs in the inventory. Missing sparse files, unresolved references,
and file-level alias candidates are deliberately visible. This is a static
prioritization/contract artifact, not a complete call graph or measured usage.

The first native HQ Metal4 probe successfully compiled and used this exact MSL
include through its runtime shader library. That validates inclusion/compilation
and the exercised diagnostic path; GPU scalar fixture equivalence and full BAR
material/visual parity remain pending. The standalone offline `metal` toolchain
component was unavailable and was not installed.

Source pins:

- BAR `069f57319503b635698ebb855f11255443e4018b`:
  `modelmaterials_gl4/templates/cus_gl4.frag.glsl` / `.vert.glsl`,
  `defaultMaterialTemplate.lua`, `001_units_s3o_assimp.lua`,
  `luarules/gadgets/cus_gl4.lua`, `units/armcom.lua` and graphics helpers.
- Recoil `de69361239d8c8b1012dba3f5aa3122954ea4da3`:
  `rts/Lua/LuaOpenGL.cpp`, shader/buffer/pass registrations, `VertexData.hpp`,
  `myGL.h`, `S3OParser.cpp` and `Bitmap.cpp`.

The material shader credits Copyright (c) 2025 Beherith
(`mysterme@gmail.com`), MIT license. Adapted equations retain that attribution;
new engine-side code follows the repository GPL v2-or-later header. Outside
submissions remain subject to `AGENTS.md` / `AI_POLICY.md`, including disclosure,
accepted issues and human testing/review. This work has not been submitted.
