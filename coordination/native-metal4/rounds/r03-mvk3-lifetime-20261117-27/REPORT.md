# Round 27 — Decals GL4 lifetime at frames 0 and 1590

Both engines recorded a different Decals GL4 lifetime at their first delivered GameFrame, frame 0. R03 also recorded its lifetime at frame 1590. MVK3 exited after the existing warmup pacing widget reported a failure at frame 180, so the requested two-frame pair is incomplete.

| Engine | Simulation frame | Raw GPU memory | Raw short name (decoded) | Lifetime multiplier |
|---|---:|---:|---|---:|
| R03 Metal4 | 0 | 0.000000000000000 | BC213 | 0.699999988079071 |
| R03 Metal4 | 1590 | 0.000000000000000 | BC213 | 0.699999988079071 |
| MVK3 | 0 | 18622720.000000000000000 | BC213 | 3.000000000000000 |
| MVK3 | 1590 | UNKNOWN | UNKNOWN | UNKNOWN |

The GPU memory column preserves the numeric value exposed to Lua. Both engines emitted the short name as the raw hexadecimal string `4243323133`. The raw log payloads in infolog.txt and engine.log agree. The engine frame prefixes agree with the simulation frame in every observation.

MVK3 emitted `FAILED_PAUSE_REQUEST_STATE` at frame 180. The process exited normally. The warmup exited before the existing start marker was written, so host postprocessing also reported a missing `budget192.start` and rejected the run on its pacing-failure check. The failure line does not distinguish the individual conditions inside that check. The runner stored an empty accepted-record list for this failed arm. Its original frame-0 observation is preserved in the raw evidence. MVK3's child environment passed, and the actual process mapped the four pinned EGL, Gallium, Vulkan loader and MoltenVK libraries.

The observations establish a difference in the CPU-visible GPU memory and lifetime scalars at frame 0. R03 had the same recorded values at its two samples. MVK3's later lifetime, equality of GPU decal workloads, causality and FPS remain unmeasured here.

The executor reserved revision 105 after READY revision 104 and staged once. Each arm launched once. R03's two records, normal exit and owned cleanup passed before MVK3 launched. Both process groups closed and both owned wake processes exited. The user's iTerm wake process remained active and bar remained paused. Captures, profiles, retries, builds, source edits and system changes were all zero.

- [Original observation and pacing-failure lines](RAW-OBSERVATIONS.txt)
- [Per-arm results, child environments, MVK3 stack and raw-log hashes](RUNTIME-EVIDENCE.json)
- [Process and wake cleanup](CLEANUP-EVIDENCE.json)
- [Protected source, engine, library and archive hashes](PROTECTED-HASHES.json)
- [Fresh stage and initial BC213 checks](STAGE-VERIFICATION.json)
- [Round outcome and artifact hashes](DONE.json)
