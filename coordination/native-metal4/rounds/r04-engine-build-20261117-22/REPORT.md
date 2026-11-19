# R04 isolated build attempt: configuration failed, no retry

The one authorized configure attempt exited 1 because CMake/pkg-config could not find `openal>=1.18.2`. The error is at `rts/System/Sound/CMakeLists.txt:52`; the raw [CONFIGURE.log](CONFIGURE.log) records it from line 402. Engine compilation and relocation/signing were not started. No R04 executable was produced or launched.

Round `r04-engine-build-20261117-22`, attempt `r04-isolated-build-01`, reserved at STATE revision 87. Fixed source `5d20bd842b3c881166abb58f361482a5d7bb19e7`, parent R03 `0780bd7c530b0696067b717428b49a416eda556a`. The R04 manifest SHA-256 is `9e022fbfc7e2aad66c37aabd1b117f17b3c3318cf66c1280b14edf11d7beaf70`; the declared SensorLOS header is `722565c5c1ff15bfa7801615fe3bbbf9acb7de83cb5deab61aa323e20455e73f`.

## Verified inputs and scope

- The frozen R03 source closure contains 10,269 files. Every file matched its pinned Git blob before copying. Only the declared R04 SensorLOS header differs in the isolated source; all files matched the candidate's Git blobs. The four simdjson editor/Git metadata files absent from the historical R03 closure remain absent; an incidental Python cache was excluded. Both original and candidate source files were rehashed after the failed configure with no changes.
- Existing R03 signed engine SHA-256 `107958872e5d962e4d27803a0fb1af0e842b18bd6982bd9b74e115d956890ab9` and runner SHA-256 `37ec7ef5a47b5de96114f661427ca863f8d6f4f196f776af1f3f895c737112c8` matched actual bytes before and after. Neither was executed or modified.
- Initial cache and the known relocation/sign script were copied byte-for-byte. The same 19 runtime dylibs were verified and copied without modification. The relocation mapping matches R03, but relocation/signing was not attempted.
- SDK 27.2 and Apple clang 21.0.0 match the recorded R03 toolchain. The R03 configure command was reused with only the isolated root changed. Configuration failed before the normalized compile-command comparison. Full dependency discovery environment equivalence is therefore **not established**; the missing pkg-config result is not proof of an R04 source defect. No dependency installation, environment repair, source repair, or retry was performed.

## Actual consumption and cleanup

| Operation | Count |
| --- | ---: |
| Configure attempts | 1 (exit 1, 8.060 seconds) |
| Engine builds | 0 |
| Relocation/sign | 0 |
| Games / benchmarks / GPU helpers / captures / replays | 0 each |
| Source repairs / retries | 0 each |
| Profile staging | 0 |

The owned configure process group is gone, child workers are reaped, and the build controller exited. No awake assertion or GPU/game process was created. Original evidence, binaries, runner, source, and copied dylibs retain their hashes. `bar` remains PAUSED and was not retargeted.

The 12,065 prior standalone R04 helper comparisons are separate bounded correctness evidence; they were not repeated and do not establish full-engine game behavior or FPS. This attempt provides no runtime, image, Metal validation, or performance result. The failed attempt is consumed. Return to JINI for any separate future decision; do not continue or retry this attempt.

Existing SensorLOS/Beherith MIT and engine GPL notices were preserved. [UPSTREAM-CONTRIBUTIONS.json](UPSTREAM-CONTRIBUTIONS.json) retains Tom J Nowell (tomjn)'s Recoil PR #3695 LuaVBO diagnostic correction and PR #3035 macOS affinity/QoS contribution, plus the existing benbreen/yeojuny provenance and its recorded scope. No new external refinement was adopted.
