# BAR Metal4 v1.11.5

The default CAP2 profile enables alpha-first SensorLOS outlines, skips long empty Grass spans while keeping original instance slots, and exits zero-scale Grass vertex work early. Radar and Sonar use tighter raster bounds with corrected shared indices. The optional Radar/Sonar circle-scan early rejection, GroundDecal projection reciprocal experiment and track clipping are disabled.

## Performance

Seths Ravine, Ultra, AA off, CAP2 and AC Automatic. Both runs used the exact packaged executable and the same fixed camera and simulation window, from tick 540 through 1740. The four new options were enabled together and all frame intervals were included.

| Profile | Average FPS | 1% low FPS |
| --- | ---: | ---: |
| v1.11.4 renderer options | 88.58 | 59.11 |
| v1.11.5 renderer options | 97.42 | 65.66 |

In this paired run, average FPS changed by **+9.98%** and 1% low by **+11.07%**. The app was checked through its lobby and completed the Pinewood Derby smoke scenario. The revised Radar/Sonar geometry matched the live color, depth and stencil buffers at two sampled Seths ticks. See the [checks](CHECKS.json) and raw frame times for [OFF](seths-off-frames.csv) and [ON](seths-on-frames.csv).

The inherited LuaVBO diagnostic argument correction comes from Tom J Nowell's [RecoilEngine PR #3695](https://github.com/beyond-all-reason/RecoilEngine/pull/3695). Source and license records accompany the exact engine.

## Update

```sh
brew update
brew upgrade --cask yeojuny/bar-metal4/bar-metal4
```

Requires Apple Silicon and macOS 27.2 or later. The app is ad-hoc signed.
