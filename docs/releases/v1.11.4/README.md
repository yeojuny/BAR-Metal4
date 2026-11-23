# BAR Metal4 v1.11.4

This release enables the Native Metal 4 CAP2 profile on the default PREPARE path. It uses two pending game submissions and presents before the next drawable acquisition. The slot-preserving Grass fix keeps live instances in their original slots and trims only trailing zero-scale entries.

Seths Ravine testing used the same packaged engine for both Grass runs, with other settings fixed, one minute between runs, and all frame intervals included.

| Grass setting | Average FPS | Engine 1% low FPS |
| --- | ---: | ---: |
| Full instance ranges | 86.78 | 60.03 |
| Slot-preserving trim | 87.50 | 60.66 |

The earlier experimental 90.35 FPS run used dense Grass compaction. Same-input replay isolated 34 color and two depth differences for that compaction method. Repeated same-input checks on the replacement found zero pixels whose values separated by mode in the two tested Seths scenes. The release uses the replacement, keeps terrain zero-weight material skipping and track clipping disabled, and keeps the three audit switches disabled. GroundDecal world-position reuse, flat-wrap, cubemap subresource reuse, the Grass bend shortlist and the byte-matched Lua Nano uniform reuse are enabled. The Grass result is one paired comparison of a specific battle. Pixel checks cover specific passes and scenes.

The signed application was launched through its lobby and completed an isolated Pinewood Derby scenario, returned to the lobby, and exited normally. The attached ZIP and source archive identify the exact release engine. The source was built with the attached [CMake settings](initial-cache.cmake). See [runtime and performance checks](CHECKS.json), [raw Grass OFF frames](grass-off-frames.csv), and [raw Grass ON frames](grass-on-frames.csv).

The inherited LuaVBO diagnostic argument correction comes from [Tom J Nowell's RecoilEngine PR #3695](https://github.com/beyond-all-reason/RecoilEngine/pull/3695). The source includes the inherited contribution and license records.

## Update

```sh
brew update
brew upgrade --cask yeojuny/bar-metal4/bar-metal4
```

New users can install via `brew tap yeojuny/bar-metal4`, `brew trust --tap yeojuny/bar-metal4` and `brew install --cask yeojuny/bar-metal4/bar-metal4`. The app is ad-hoc signed and requires Apple Silicon and macOS 27.2 or later.
