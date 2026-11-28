# Round 29 — R03 main game-decal draw toggle

R03 produced normal and skip traces in one game, using PID 7809 and Layer 12. The original action emitted `Decals GL4 skipdraw set to, true` once at simulation frame 1590, and the render counter advanced before the second capture. The host accepted the acknowledgement with the one-line comma fix. The original round28 log and checks for false, duplicate, missing and mixed responses passed before launch.

All four CPU witnesses recorded paused frame 1590 and matched except for their draw counters. They cover the camera, selection, 168 units and 401 features. The GPU debugger reported session 11 ready for the normal trace. Skip session 14 still reported `loading` after 24 status checks, with 14.316 seconds between the first and last checks. The executor ended both sessions when this wait budget expired.

The bound GPU tick, main GameDecal draw removal and equality of other GPU pass inputs remain unverified. No profile was run. The debugger's total draw counts are aggregate counts and do not identify the removed draws or their cost. The intended action targets the main game-decal draw branch while leaving producers, pending uploads, PiP and engine GroundDecal outside that branch. No GPU cost or FPS inference is supported by this attempt.

| Execution | Count |
|---|---:|
| R03 game | 1 |
| Capture commands and completed frames | 2 / 2 |
| Inspection sessions / profiles | 2 / 0 |
| Retries / builds | 0 / 0 |

The engine exited normally. The game, capture clients and owned wake process were gone, and the debugger listed zero active sessions. The user's `caffeinate -d` remained present. All 132 protected hashes matched. Both original traces and individual file hashes are retained under the local round29 evidence path.

- [Original log lines](RAW-OBSERVATIONS.txt)
- [Capture receipts, scene witnesses and inspection status](RUNTIME-EVIDENCE.json)
- [Cleanup](CLEANUP-EVIDENCE.json)
- [Protected hashes](PROTECTED-HASHES.json)
- [One-line runner delta](RUNNER-DELTA.diff) and [host checks](HOST-CHECK.json)
