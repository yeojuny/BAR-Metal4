# Exact historical UI render target game pair

| Configured cap | Average FPS | Slowest 1% FPS | Draws | Longest interval |
| --- | ---: | ---: | ---: | ---: |
| cap1 | 64.147 | 52.953 | 2566 | 36.0 ms |
| cap2 | 63.900 | 53.279 | 2556 | 26.0 ms |

The cap2 flag changed average FPS by -0.38% and 1% low by +0.61% in this single pair. Both arms used the same signed binary, scene, immutable inputs, accepted settings, and 40 bins from simulation frames 540 through 1740. All counted draws and recorded frame intervals were retained. Both games exited normally and recorded successful drawable presentation requests.

The screen render target CPU gate fell from 2,616 adjacent eligible frames in the previous cap2 run to zero. The new run had 2,555 adjacent eligible frames. All 2,555 still hit an unclassified CPU prior-read gate. The cube and world info counters were also zero. Joined submissions and genuine outstanding depth two were both zero, so this pair does not measure active cap2 performance. The candidate still uses the original fallback paths and gate on successful predecessor retirement. GPU failure injection, physical scanout and input latency were not measured.

The uncategorized call sites seen in source include the Bloom predecessor, the serial postprocess fallback and terrain-info reads. The old BumpWater site is also generic but lies outside ordinary two-frame eligibility. No source evidence by itself attributes the measured gate to any one of the remaining call sites.

One configure, one build and one game per cap completed. There were no repeats, captures or profiles. The prior display assertion was absent. The owned display assertion PID 52856 was released after both runs. Production default, cap and prior artifacts were preserved.
