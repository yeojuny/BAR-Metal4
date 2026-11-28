# R04 isolated build with process-local OpenAL path: PASS

The separately authorized attempt configured, built `spring-metal4-scene` at `-j8`, and relocated/signed successfully, once each. No engine, game, GPU helper, capture, replay, or benchmark was launched. This is build and static closure evidence, not runtime correctness or FPS evidence.

Round `r04-engine-build-envfix-20261117-23`, attempt `r04-isolated-build-02`, reserved at STATE revision 90. The previous failed attempt remains unchanged at its original root.

## Fixed inputs and environment

Source `5d20bd842b3c881166abb58f361482a5d7bb19e7` is the declared R04 header change over R03 `0780bd7c530b0696067b717428b49a416eda556a`. Manifest SHA-256: `9e022fbfc7e2aad66c37aabd1b117f17b3c3318cf66c1280b14edf11d7beaf70`. SensorLOS header SHA-256: `722565c5c1ff15bfa7801615fe3bbbf9acb7de83cb5deab61aa323e20455e73f`.

Before the sole configure, process-local `PKG_CONFIG_PATH=/opt/homebrew/Cellar/openal-soft/1.25.2/lib/pkgconfig` was prepended to the inherited path (previously unset). OpenAL resolved to exactly 1.25.2. Expat 2.7.4, fontconfig 2.18.3, libcurl 8.7.1, minizip 1.4, and 66 recorded dependency paths also matched R03. SDK 27.2 and Apple clang 21.0.0 matched. No global environment, Homebrew installation, security, or power setting changed.

The R03 initial cache and relocation/sign script were copied byte-for-byte. Every one of 3,142 generated compile commands matches R03 after replacing only the isolated root path, with duplicate counts and all fields retained. Configure exited 0 in 8.065 seconds; build exited 0 in 214.948 seconds.

## Closure and resulting binaries

| Check | Result |
| --- | --- |
| Frozen source closure | 10,269 files verified before and after; only declared R04 header delta |
| Current object/archive consistency | 1,538 objects, 35 archives, zero mismatches |
| Runtime libraries | Same 19 dylibs, byte-identical to R03 |
| Relocation and signature | Known nine relocations; strict codesign verification PASS |
| Loader inspection | Engine plus 19 dylibs checked; no unexpected external or missing local library references |
| R03 engine and runner | Actual pinned hashes match before and after; neither launched |
| Previous failed attempt | All 22 published evidence files remain hash-identical |

Raw engine SHA-256: `853083e4ba5490a40ede292c88d95b213166fbbc8db70af5336707657af997d1`.

Signed engine SHA-256: `4ff86017e4c16b71a19c77cbfcfa77bd52e631b99e694cef7bd547c38f625d86`.

The signed executable is retained locally at `/Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/r04-engine-staging-20261117-02/runtime/native/bin/spring-metal4-scene`. No execution wrapper or game profiles were staged. The complete source hash list, archive audit, dependency preflight, raw configure/build/package logs, and closure receipts accompany this report.

The historical R03 source closure omits four simdjson editor/Git metadata files; these remain absent. The incidental Python cache was excluded. All retained source files match their pinned Git blobs; no source repair or unrelated candidate was introduced.

## Consumption and cleanup

Configure 1; engine build 1; relocation/sign 1. Games, benchmarks, GPU helpers, captures, replays, source repairs, automatic retries, and profile staging: 0 each. The existing read-only archive consistency script ran once as part of the R03 static closure procedure.

All owned build and packaging process groups are gone and child workers are reaped. No awake assertion was created. Original R03 source, engine, runner, and libraries are unchanged; the previous failed root is preserved. `bar` remains PAUSED and was not retargeted.

The prior 12,065 standalone R04 helper comparisons were not repeated and do not establish full-engine game behavior or FPS. Return to JINI for any separately authorized runtime decision; this build attempt is consumed.

Existing SensorLOS/Beherith MIT and engine GPL notices were preserved. [UPSTREAM-CONTRIBUTIONS.json](UPSTREAM-CONTRIBUTIONS.json) retains Tom J Nowell (tomjn)'s Recoil PR #3695 LuaVBO diagnostic correction and PR #3035 macOS affinity/QoS contribution, plus the existing benbreen/yeojuny provenance and its recorded scope. No new external refinement was adopted.
