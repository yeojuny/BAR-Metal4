# Round 32 — Opt-in indexed GameDecal SDK build and Metal fixture

The isolated candidate compiled and linked with macOS SDK 27.2. The configure process found the existing OpenAL Soft 1.25.2 through its child-only package path. The original R03 source and binary and the round31 candidate source bytes matched their protected hashes after the run.

One Metal 4 helper invocation exercised the original GameDecal vertex and fragment shaders with the frozen candidate index table. The exact same row buffer, UBO, and textures were bound in both submissions of each case. The nonindexed submission used 96 vertices. The indexed submission used 96 UInt16 indices, 192 bytes at offset zero, and the same logical row range. All three actual fixture attachments had sample count 1. Metal API and GPU validation were enabled and the run emitted no validation errors.

| Fixture case | Covered pixels |
|---|---:|
| Tiny | 22 |
| Regular | 328 |
| Two overlapping blended rows | 486 |
| Left and right controls | 330 and 329 |
| Partial clip | 196 |
| Sphere and alpha rejects | 0 and 0 |
| Nonzero first row, first 1 count 1 | 328 |

Color, depth and stencil bytes matched in all nine pairs. Coverage was witnessed by stencil writes and cross-checked against depth and color. There were 173 overlap pixels whose blend differed from the right row alone. The nonzero first-row result also matched a separate reference containing only the expected original row.

The helper used dedicated depth and stencil writes to expose raster differences. It submitted the actual Metal 4 indexed call, but no game ran the candidate's environment flag and production packet preparation. Game runtime indexed entry, real game attachments and comparable live decal input remain unverified. The existing scene runner checks the game seed, camera and unit workload. It does not capture ordered small Lua decal rows and their UBO inputs. The original Lua decals consume the unsynced RNG, which the engine initializes independently at startup. A clean OFF/ON comparison under the authorized tools could not meet the matching-input gate, so no performance games or speed estimate were recorded.

Configure, engine build, helper compile and helper GPU invocation each ran once. Games, captures, trace inspections, profiles and retries were zero. All owned processes ended, GPU inspection sessions were zero, and the user's caffeinate process remained present.

- [SDK build and candidate hash](BUILD-EVIDENCE.json)
- [Original Metal validation and case logs](HELPER-GPU.log)
- [Fixture results and game proof limit](PARITY-EVIDENCE.json)
- [Protected inputs](PROTECTED-HASHES.json)
- [Cleanup](CLEANUP-EVIDENCE.json)
