# Terrain CPU compile portability

Baseline `bbd734ee0d5cc0d923e50edb8995c5193958b668`; same isolated upstream branch.

The cloud g++ 14.2 / libstdc++ (`__GLIBCXX__=20250315`) does not provide vector's C++23 range constructor. C++23 mode exposes `std::from_range_t`, but the constructor is still absent and `__cpp_lib_containers_ranges` is undefined. This was not resolved merely by changing the previous C++20 check to the project's C++23 mode. No replacement toolchain installation was needed.

`NativeTerrainPositionCopy.inc` now keeps its original range constructor when `__cpp_lib_containers_ranges >= 202202L`; otherwise it reserves the known count and appends each transformed position. The fallback does not zero-fill or repeatedly grow the vector. Both original stride paths, memcpy alignment safety and finite-coordinate checks remain; output stays local until success. Caller extent/stride preconditions remain unchanged. This is a portability change, not a measured optimization claim.

Full CPU capture translation unit now passes:

```
g++ -std=c++23 -fsyntax-only -Iengine/rts engine/rts/Rendering/EngineBridge/NativeWaterGroundSourceCapture.cpp
```

Independent review also passed C++20 syntax. These checks include the real headers and whole translation unit, not extracted methods, but do not link or run the engine. The native range-constructor branch cannot be compiled with this host library and remains untested here. Mac/Metal compilation, gameplay and rendering/performance remain unverified. No new machine was accessed.

Production include test passed 141 checks with ASan/UBSan and warnings-as-errors:

```
g++ -std=c++23 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -Iengine/rts engine/tools/metal4-contracts/TestNativeTerrainPositionCopy.cpp -o /tmp/terrain-position-test
ASAN_OPTIONS=detect_leaks=0 /tmp/terrain-position-test
```

Checks cover contiguous/strided/misaligned bytes, signed zero/subnormals and exact float bits, first/last NaN or infinities, unchanged input bytes and zero-count null input. LeakSanitizer is disabled due to sandbox ptrace; address and undefined-behavior sanitizers remain enabled. Source program 93 checks and extracted readiness 11 checks were rerun successfully. These do not imply caller extent validation or the full game was executed.
