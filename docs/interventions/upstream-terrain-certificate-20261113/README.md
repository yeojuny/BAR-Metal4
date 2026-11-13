# T2 retained validated terrain definition

Baseline `cc39127ecd1a23b8f8ebeaaaf6a5c3f84c357827`, isolated upstream branch. Production validation-certificate implementation; final integrated Mac acceptance remains required.

A private definition certificate can only be created by `BuildValidatedNativeTerrainDefinition`, which first executes the existing complete validator: identity/context/lifetime/source hashes/defines, consumer Matches and Supports. The resulting identity and consumer association are immutable. Public direct definition construction remains uncertified and takes the original full validation path.

Generic consumers do not qualify automatically. `HasImmutableDefinition()` defaults false; those consumers still run full live validation. The actual final Metal `OriginalProgram` opts in only after its sealed library exists. Its identity/profile are const, Matches compares the const identity, and Supports only combines Matches with sealed generation. Changing the mutable PSO cache does not change those facts.

Certified observation compares the expected nonzero context, consumer context and the exact certified nonzero library generation. Source Capture retains live-writer, exact current definition/uniform and state checks. Bind, Capture, source submission and Metal Prepare consume that retained definition; target/MSAA/texture/PSO and genuine factory-owner checks remain at preparation. The legacy native220 per-draw repeated identity check is superseded for this path; uncertified definitions now use the full validator rather than that optional reuse path. The obsolete native220 helper/bookkeeping is removed; its old flag no longer changes this migrated path.

No certification is granted merely by constructing a definition or returning a generic consumer. A deliberately false capability declaration by a custom implementation violates its API contract, just as mutation of a published const snapshot would. The default preserves generic live behavior without requiring opt-in.

This removes repeated full definition proof on the certified path, not every source comparison or allocation. Source current-publication matching still checks exact identity and lifetime. Uniform writes remain eager, draw ordering/queues unchanged, and no FPS result is claimed.

## Verification

Production-source harness: 96 checks with ASan/UBSan and diagnostics-off compilation. Actual NativeTerrainSourceData.cpp validator linked into a separate test: 30 checks, covering malformed identity/defines, null/unsupported/mismatched owners, wrong context/library, certified reuse and mutable generic fallback. Extracted actual Metal link methods: 11 checks. Complete backend and CPU source-capture translation units pass C++23 syntax checks. Independent review found no blocking algorithm defect; obsolete native220 bookkeeping was removed following review.

Commands:

```
python3 engine/tools/metal4-contracts/run_native_terrain_program_test.py --output /tmp/terrain-cert-source-tests
python3 engine/tools/metal4-contracts/run_native_terrain_link_certificate_test.py --output /tmp/terrain-cert-link-tests
g++ -std=c++23 -O1 -g -Wall -Wextra -Werror -Wno-misleading-indentation -Wno-missing-field-initializers -fsanitize=address,undefined -fno-omit-frame-pointer -ffunction-sections -fdata-sections -Wl,--gc-sections -Iengine/rts engine/tools/metal4-contracts/TestNativeTerrainDefinition.cpp engine/rts/Rendering/Backend/NativeTerrainSourceData.cpp -o /tmp/terrain-definition-test
ASAN_OPTIONS=detect_leaks=0 /tmp/terrain-definition-test
```

LeakSanitizer remains disabled for sandbox ptrace. Full final Mac/Metal build, actual Lua callbacks, rendering parity and performance remain unverified. No app was launched or rebuilt for these source checks.
