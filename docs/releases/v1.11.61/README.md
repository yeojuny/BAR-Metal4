# BAR Metal4 v1.11.61

Experimental single-player native Metal 4 for Apple Silicon. This is a correctness and maintenance update to v1.11.6, including two previously validated CPU-side improvements.

## Correctness fixes

- Common line rendering now validates the actual attachment sample count, formats and extent; the pipeline and cache key use the real sample count, including 2× MSAA. Virtual line viewports are validated separately from physical attachments. No line geometry is omitted.
- Large logical PiP/minimap weapon-range viewports retain their original transform and scissor. Oversized virtual viewports use target-relative rasterization, not arbitrary coordinate clamping.
- Ground decals now map the logical world viewport to the actual cropped depth attachment. Changing resolution retires the previous work and uses the current physical depth crop and sample count without resampling. This is a coordinate-system fix, not a 1080p or 4K special case.
- Process shutdown preserves and reports the first terminal render failure after joining failed CPU work, rather than throwing it again while tearing down the engine. Normal render-time errors remain errors.

## Recent selected performance improvements

- Exact-input asynchronous WeaponRange shader prewarming.
- Bounded, retirement-aware terrain/shadow Metal argument-table reuse (ArgPool-B).

| Map (ArgPool-B only) | Off avg FPS / 1% low | On avg FPS / 1% low |
| --- | ---: | ---: |
| Mediterraneum | 143.13 / 102.04 | 145.04 / 104.56 |
| Supreme | 149.97 / 94.41 | 150.08 / 95.88 |

Prior allocation instrumentation found approximately 4.1–4.24 million physical argument-table allocations reduced to approximately 4,450. This is the number of API allocations, not bytes or peak RAM. These are measurements of the preceding selected build, not a new whole-release benchmark.

## Validation and limits

Actual local M4 Pro single-player skirmish validation used the game lobby, commander placement, Legion spawn, building preview, PiP weapon range, tracked-vehicle ground decals, and normal quit. See [verification evidence](CHECKS.json) for the exact configurations and observed resolutions. An offline cheat command was used to spawn a tracked vehicle to trigger decals deterministically; this was not a benchmark or a replay. Separate GPU and coordinate-contract tests verify multisample coverage, virtual viewport rebasing, and physical depth mapping. The reported M1 Ultra Mac Studio and iMac have **not** been directly retested. Whole-frame bit-exact parity and a new full release performance A/B were not established. Other unselected optimization candidates remain disabled. This is an experimental single-player port, not an official BAR build; multiplayer is not supported.

The engine archive records the exact selected source and linked member hashes, not a fresh clean-machine rebuild. Existing upstream contributions remain documented in NOTICE.md and UPSTREAM-CONTRIBUTIONS.json.

## Update

```sh
brew update
brew upgrade --cask yeojuny/bar-metal4/bar-metal4
```

Requires Apple Silicon and macOS 27.2 or later. The app is ad-hoc signed.
