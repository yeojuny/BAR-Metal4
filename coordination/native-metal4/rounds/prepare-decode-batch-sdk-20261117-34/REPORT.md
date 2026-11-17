# Round 34 · Prepare-local batch Mac SDK build

The pinned seven-optimization patch compiled and linked on its first isolated build. CMake and Ninja built the arm64 Spring Metal4 engine with macOS SDK 27.2, AppleClang 21.0.0.21000401 and RelWithDebInfo. Configure found OpenAL Soft 1.25.2 through the same child-process package path used in round 32.

The patch applies exactly 18 file changes to R03 commit `0780bd7c530b0696067b717428b49a416eda556a`. It reuses validated decode results for BuildSquare, Commands FX, Sensor31657Circle (radar/sonar), StartBox and WeaponRange. EnergyExplosion and LuaNano use fixed index tables. The bundle is pinned to `f7faa7afb134b56dd673180f660df6add5600cae`. [Patch verification](PATCH-VERIFICATION.json)

All 12 changed production translation units compiled. The final executable links 6 objects directly and 6 through `librecoil-metal4-world-renderer.a`. All six archived members match the hashes of their compiled objects. A defined symbol from each of the 12 objects appears in the executable. The Mach-O linker metadata records SDK 27.2. The linker emitted a warning about duplicate library arguments and returned exit 0. [Object and link evidence](SOURCE-OBJECT-LINK.json) · [Log excerpts](SDK-RAW.txt)

**Executable SHA-256** `49e84bb92cb055c74310549d9e158d092241f4898e014fd51c40ad9717198b1d`

CMake configure and the engine build each ran once. Game launches, GPU runs, helper builds, captures, profiles and retries were all zero. This run verifies compilation and linking. Rendering parity and performance remain unmeasured.

All 10,270 original R03 source files, the original signed engine, 134 prior artifact hashes and all 142 launcher data hashes matched. Both owned process groups ended, the active GPU-session count was zero and the user's `6618 caffeinate -d` process was present. [Protection evidence](PROTECTED-HASHES.json) · [Cleanup evidence](CLEANUP-EVIDENCE.json)

[Build receipts and full log hashes](BUILD-EVIDENCE.json) · [Completion record](DONE.json)
