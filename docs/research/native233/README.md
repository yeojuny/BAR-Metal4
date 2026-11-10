# Native Metal 4 development after v1.11.2

## Fullscreen and lobby handoff

The engine reports foreground readiness once its game window is visible, key, and active. The lobby hides on that signal, before the first presented simulation frame, and restores on exit. Reopening the lobby during a running game no longer steals foreground focus. The engine hides the macOS menu bar and Dock in fullscreen and restores the previous presentation settings when it leaves fullscreen.

On one Mac and its main display, a clean start and a relaunch each reached simulation frame 300 and returned to the lobby. The WindowServer observer recorded 375 fullscreen samples across the two runs with the lobby hidden and no menu layer window on screen. An intentional engine exit before the first frame also restored the lobby. The launcher had 16 targeted tests. These results cover the tested app and display conditions.

[Fullscreen validation record](../../validation/foreground-after-v1.11.2.json)

## Local benchmark 233

233 is the locally approved benchmark for the subsequent diagnostics. It adds a guarded floating-point estimate for weapon-range division, corrects the result with an exact integer residual, and retains integer division outside the guard. Its parent already includes the bounded terrain source definition reuse from 232 and the preceding opt-in native work.

| Change | Observed result | Disposition |
| --- | --- | --- |
| 220 follow-ups | CPU read packet, residency and terrain uniform release, RTT/H5 reuse, particle upload reuse, and render boundary work were tested in successive local builds. | The 233 manifest pins the cumulative source and [the results](RESULTS.json) pin the flags used. Individual experiments had separate decisions. |
| 232 terrain definition | Mean FPS +2.249% and 1% low +0.423% across three matched pairs. | Validated locally, then used as the parent for 233. |
| 233 weapon-range division | 165.949 → 167.035 FPS (+0.654%) across three matched pairs. The paired FPS changes were +0.854%, +0.429%, and +0.682%. Pooled 1% low was 111.794 → 112.228 FPS. | Approved as the local benchmark. |
| 234 submission timeline | One diagnostic run measured a mean 0.685 ms from the previous GPU end to the next CPU-ready point across 3,485 pairs. | Used to locate the next investigation. This interval is neither total GPU idle time nor a savings estimate. |
| 235 cubemap copy batching | 173.951 → 173.751 FPS and 112.608 → 113.772 FPS for 1% low. | Held. Batching did not establish lower transfer volume or a repeatable FPS gain. |
| 236 intermediate drain | No mid-frame drain observed in 3,461 frames. | Diagnostic only. |
| 237 empty alpha resume | 174.846 → 174.732 FPS and 112.782 → 110.005 FPS for 1% low across three matched pairs. | Held. The observed empty resumptions did not translate to a repeatable gain. |
| 238 frame loop | 5.776 ms mean app-loop spacing. In the 0.806 ms from previous GPU end to next commit, encoding accounted for 0.448 ms and preparation for 0.261 ms. | Diagnostic only. These are measured stage times, not removable time. |
| 239 Lua display lists | Mean 19.2 replays and 683.5 vertices per frame across 111 sampled frames. Replay CPU time averaged 0.0442 ms per frame. | Below the gate for a reuse candidate in this scene. |
| 240 Lua VBO copies | 3,787 CPU copies and 18,024,816 bytes across 1,695 frames, averaging 1.65 µs of CPU copy time per frame. | No performance candidate. This does not measure all Metal uploads or memory high water. |

These are fixed-scene measurements on the test Mac. The 233 comparisons skipped the audio gate by request. Full-frame pixel equivalence and clean-machine release qualification remain open. This source review does not change the distributed app.

## Source and attribution

[Overlay manifest](OVERLAY-MANIFEST.json) maps 289 distinct inputs. It supplies 31 engine files that differ from the pinned v1.11.2 source tree, references byte-identical inputs at that commit, and pins the two fmt dependency headers by hash. The manifest records the original compiler overlay hash. The local link inputs and toolchain are separate, so the overlay alone does not reproduce the measured executable.

LuaVBO invalid-index diagnostics include a correction adapted from [tomjn's RecoilEngine PR #3695](https://github.com/beyond-all-reason/RecoilEngine/pull/3695). The macOS affinity QoS handling draws on [Tom J Nowell's RecoilEngine PR #3035](https://github.com/beyond-all-reason/RecoilEngine/pull/3035). The [provenance record](UPSTREAM-CONTRIBUTIONS.json) distinguishes those changes from benbreen's worker-entry QoS experiment, which was a private build and was not adopted.
