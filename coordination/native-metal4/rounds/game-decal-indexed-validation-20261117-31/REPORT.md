# Round 31 — Opt-in indexed GameDecal validation

The isolated R03 candidate matched all three patched file hashes from the pinned bundle. The first CMake configure failed before compilation at `pkg_check_modules`, which could not find `openal>=1.18.2`. This is a build environment result. The candidate Objective-C++ translation unit did not reach the macOS compiler.

OpenAL Soft 1.25.2 is installed. The default `pkg-config` lookup failed and the same read-only lookup succeeded with `/opt/homebrew/opt/openal-soft/lib/pkgconfig` on the child process's search path. The prior R03 configure log found the same version. The fresh configure invocation reused the pinned cache but omitted this package search path.

| Check | Outcome |
|---|---|
| Bundle, parent and patch hashes | PASS |
| macOS SDK engine compile | NOT REACHED |
| Indexed branch, sample count and Metal validation | UNKNOWN |
| Identical-row color, depth, stencil and coverage parity | UNMEASURED |
| Clean native OFF/ON performance | UNMEASURED |

The round used one configure invocation. Engine builds, parity helper compiles and GPU invocations, games, captures, inspections, profiles and retries were all zero. The configure group exited, no GPU debugger sessions were active, and the user's wake process remained active. R03's 10,270 original source entries and 132 protected hashes, including its signed binary, matched after the attempt.

- [Configure failure excerpt](CONFIGURE-FAILURE.txt)
- [OpenAL lookup evidence](DEPENDENCY-DIAGNOSIS.json)
- [Pinned patch and protected hashes](PROTECTED-HASHES.json)
- [Cleanup](CLEANUP-EVIDENCE.json)
