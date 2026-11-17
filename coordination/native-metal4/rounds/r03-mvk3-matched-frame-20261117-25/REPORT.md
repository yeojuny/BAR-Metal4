# Round25 matched-frame diagnostic — stopped before capture

The first R03 game reached the requested paused simulation tick 1590, but the existing `/usr/bin/gpucapture boundaries --pid 87421` query timed out after 8 seconds. The capture worker stopped and the game exited normally. `gpucapture start` was never invoked and no `.gputrace` was produced. The mandatory capture-bound `timeInfo.x` evidence is therefore **UNKNOWN**. No MVK3 game, inspection session, or profile command followed; no retry or renderer/source repair was attempted.

## Fixed inputs and consumed budget

R03 source: `0780bd7c530b0696067b717428b49a416eda556a`; manifest: `4f0878847fd57a1616ed2bb3555424d469971d4fe2a5a232817faea3bef39deb`.

R03 signed engine: `107958872e5d962e4d27803a0fb1af0e842b18bd6982bd9b74e115d956890ab9`.

MVK3 preserved A2 engine: `d715965659b977e05b8ccafe76fae0936cade1a7481871acff7eec876366f1aa`.

Both binaries and the 23 recorded runtime libraries matched before execution and after cleanup. Both isolated arms used the same frozen controller, `148b6396a413cd6c8f310b1a7afc10cc1e5ef659a516f15e224c6157a508058d`. The existing runner supplied identical game/map/start/Ultra4/AA-off inputs; only the isolated controller's fixed-tick pause, state snapshots, and capture completion handshake were added. The frozen runner and inherited runner remain unchanged. R03 actual child environment matched the launch record, including capture enabled, validation off, worker1, PREPARE1, BEGIN0, S01/G01 enabled. MVK3 runtime execution was not tested in this round.

Consumed: **games 1 (R03), boundary queries 1, capture-start commands 0, captured frames 0, inspection sessions 0, profile commands 0, builds 0, engine-source edits 0, retries 0**. No FPS A/B or R04 capture. A profile command can internally replay multiple times; none was invoked here.

## Evidence and limits

Controller snapshots before/after the failed capture preparation report tick 1590, paused state, the fixed 1512×950 camera, 168 units and 401 features. Their recorded fields match after excluding draw number. These are CPU/controller observations, not proof of any captured GPU frame. Pair camera/workload equivalence remains untested. Boundary-query failure establishes no rendering-performance cause; its underlying tool/service cause is UNKNOWN.

| Component | Matched GPU inputs, resource/attachment/DSS/writer evidence | VS/FS cost, counters, active timing |
|---|---|---|
| Terrain | UNKNOWN — no capture | UNKNOWN |
| Units | UNKNOWN — no capture | UNKNOWN |
| Ground decal | UNKNOWN — no capture | UNKNOWN |
| Game decal | UNKNOWN — no capture | UNKNOWN |
| SensorLOS | UNKNOWN — no capture | UNKNOWN |
| Postprocess | UNKNOWN — no capture | UNKNOWN |
| UI | UNKNOWN — no capture | UNKNOWN |

No component bottleneck, exclusive GPU milliseconds, occupancy, frame-time fraction, FPS saving, or R04 slowdown mechanism is inferred.

## Cleanup

R03 PID/process group 87421 is gone; its game exited normally. The capture worker was joined, the timed-out capture client is absent, owned caffeinate PID 87413 is gone, and the shared awake receipt is RELEASED. No game, gpucapture, gpudebug or replayer process remained in the final process snapshot. No inspection session was opened by this attempt.

The final read-only `gpudebug --list-sessions` cleanup query also timed out at 15 seconds with no output. **Owned cleanup is complete; the global session listing is UNKNOWN.** Existing system `gputoolsserviced` and unrelated caffeinate were not terminated. No unowned session was touched. `bar` remains PAUSED. Staged MVK3 files remain unused and do not authorize another launch.

Raw failure, frozen inputs, controller diff, actual child environment, CPU world snapshots, and cleanup receipts accompany this report. Return the closed, insufficient-evidence result to JINI; any further execution requires a new round. Existing contribution provenance is preserved in UPSTREAM-CONTRIBUTIONS.json; no engine implementation was changed.
