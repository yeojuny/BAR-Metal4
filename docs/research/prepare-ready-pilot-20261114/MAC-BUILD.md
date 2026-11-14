# Mac compile verification: preparation-ready candidate

- Source commit: `13cd5d6f90dfe24b0325180f74fbd26fc00e2329`.
- Separate clean checkout and build directory; original source/build/apps preserved.
- Apple M4 Pro, macOS 27.3; Xcode 27.2 (27B52), Apple Clang 21.0.0.
- Target: `engine-metal4-scene`; RelWithDebInfo; `BAR_NATIVE_INTERVENTION_RENDERER_OPTIMIZE=ON`; SDL3 enabled.
- Full native C++/Objective-C++ compilation and link completed with exit **0**.
- Binary: `build-native/spring-metal4-scene`, Mach-O 64-bit arm64.
- Binary SHA256: `825226825f5a21c8eb028d9dce6fb0eb568c81a6d98bf3caef8a753957792490`.
- Build log SHA256: `741372c20576a74f9b927285135487b4918980dd4fb773fef14bbc7d1da0958b`.
- Configure log SHA256: `2c1c656bb7c79eef46b194d334d86ca83ef92b43d9535c06a1c5e5c128885489`.

No source fix was needed during this Mac build. Existing Homebrew OpenAL and Xcode libunwind paths had to be supplied for configuration. Warnings remain, including upstream offsetof and duplicate-library warnings; this is not a warning-free build.

## Scope
- [x] CUS, Terrain and P5 candidate compile and link on Mac.
- [ ] Game launch and visual correctness.
- [ ] Actual CPU overlap, frame timing, GPU timing or FPS comparison.
- [ ] App packaging/signing or portable dependency closure.

The prepare-ready experiment remains default off. Its no-Begin skip/failure quarantine restriction is unchanged. Compilation is not runtime acceptance. No game or application was launched, and no performance measurements were taken.

## Configuration outline
Use a separate checkout at the source commit above. Configure with Ninja and:
`-DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_spring-metal4-scene=ON -DRECOIL_USE_SDL3=ON -DBUILD_TESTS=OFF -DBUILD_DOCS=OFF -DBAR_NATIVE_INTERVENTION_RENDERER_OPTIMIZE=ON`.

Set `PKG_CONFIG_PATH` to the installed OpenAL Soft pkgconfig directory and `LIBUNWIND_INCLUDE_DIR` / `LIBUNWIND_LIBRARY` to the selected Xcode SDK's include directory and `usr/lib/system/libunwind.tbd`. Then build only `engine-metal4-scene` with `cmake --build build-native --target engine-metal4-scene -j 6`. No install or app launch step.
