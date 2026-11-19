# Round 28 — R03 main game-decal draw toggle

The game produced one normal-draw trace. At the paused simulation frame 1590 the original action emitted `Decals GL4 skipdraw set to, true` once and the render counter subsequently advanced. The helper's recognition pattern looked for whitespace immediately after `set to`. It missed the comma in that actual log format, falsely rejected the acknowledgement and stopped before issuing the skip capture.

The existing CPU scene snapshots recorded the same paused tick, camera, selection, 168 units and 401 features across normal-before, normal-after and the two later snapshots, with only the draw counter different. The later snapshots show what the game reported before it quit. They do not establish that a skip trace was captured. The baseline trace is retained locally. Bound GPU tick, main-draw removal and retained GPU inputs are unverified. No inspection session or profile was opened after the failed gate.

| Execution | Count |
|---|---:|
| R03 game | 1 |
| Capture commands and completed frames | 1 / 1 |
| Skip capture commands | 0 |
| Inspection sessions / profiles | 0 / 0 |
| Retries / builds | 0 / 0 |

The planned control targeted the main game-decal draws. Original decal production, pending uploads and the separate PiP and GroundDecal paths were outside that change. This attempt yields no measured draw cost, dependency cost or FPS effect. The original acknowledgement confirms only the action state.

The game exited normally. The owned game process group, capture client and wake process were gone. No GPU inspection sessions were active. The preflight saw the current user `caffeinate -d` at PID 6618. It was still present after cleanup and no user wake process was signalled. The launcher and finish-branch deltas are attached. All 132 protected hashes matched.

- [Verbatim observations](RAW-OBSERVATIONS.txt)
- [Capture receipt, failure and witnesses](RUNTIME-EVIDENCE.json)
- [Owned cleanup](CLEANUP-EVIDENCE.json)
- [Protected and staged hashes](PROTECTED-HASHES.json)
- [Controller delta](CONTROLLER-DELTA.diff) and [runner delta](RUNNER-DELTA.diff)
