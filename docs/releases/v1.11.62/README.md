# BAR Metal4 v1.11.62

Experimental single-player native Metal 4 for Apple Silicon. This release selects the visual changes and the current 23/19-field ModelKey246 update on top of v1.11.61.

## Visual changes

- Remove the extra persistent solid selected-unit radar ring in the native world and selected-only PiP. BAR's normal dotted allied radar coverage remains, as do sonar, original command/hover previews and other ranges. The removed overlapping ring was a Metal4 addition, not the original BAR contract.
- Intentionally omit Recoil's vertical area-command center marker in native Metal4 (including E/reclaim, R/repair and A/attack). Preserve the original area tint, stencil volume, interaction and outgoing render state. This is an intentional presentation preference, not an upstream parity fix; the legacy GL path is unchanged.

## ModelKey246

Replace legacy temporary vector/string pipeline keys with exact fixed-size 23-field CUS and 19-field Forward keys. The current keys retain all precision and indirect-execution distinctions, original pipeline/fragment ownership and shader descriptors. The independent audit and 752,849 equality comparisons passed.

| Jade, clean ABBA | Average FPS | 1% low |
| --- | ---: | ---: |
| Off | 63.9875 | 49.29 |
| On | 64.0088 | 48.10 |
| Change | +0.033% | -2.41% |

In the separate live allocation-capacity witness, CUS cache-hit temporary requests fell from 373,371 (76,665,512 cumulative payload bytes) to zero, and Forward from 3,090 (143,904 cumulative payload bytes, lower bound) to zero. The measurements cover complete runs including startup/warmup; hit counts differ with the number of prepared frames, and the per-hit CUS request reduction was 3 (616 bytes) to zero. These are request/payload counts, not resident RAM, disk I/O, GPU submissions, or measured CPU time. This is selected as a structural CPU-work reduction, not an FPS gain. The observed low difference is retained; median and P99 frame times were 16 and 19 ms in both arms.

## Verification and limits

Actual local M4 Pro Simmers skirmish, 1920×1080, MSAA 2×: Legion placement and spawn, selected/deselected radar, E/R/A area preview, building preview, maximum PiP zoom and normal exit at simulation frame 7708. The preceding [v1.11.61 resolution and MSAA verification](https://github.com/yeojuny/BAR-Metal4/blob/v1.11.61/docs/releases/v1.11.61/CHECKS.json) remains applicable to the unchanged coordinate/attachment fixes; this release did not repeat every previous resolution combination. See [current checks](CHECKS.json). Mac Studio M1 Ultra and iMac have not been directly retested; no M1-specific performance or full-frame bit-exact parity claim. The previously deferred Terrain232 certificate candidate remains disabled.

This is an experimental single-player port, not an official BAR build. Multiplayer is not supported. The source archive contains the actual selected overlays and verified linked-member hashes, not a new clean-machine rebuild. Upstream contribution records remain in NOTICE.md and UPSTREAM-CONTRIBUTIONS.json.

## Update

```sh
brew update
brew upgrade --cask yeojuny/bar-metal4/bar-metal4
```

Requires Apple Silicon and macOS 27.2 or later. The app is ad-hoc signed.
