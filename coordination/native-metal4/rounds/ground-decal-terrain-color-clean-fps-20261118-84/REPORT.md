# GroundDecal terrain color performance

The same signed binary averaged **78.362 FPS OFF → 78.243 FPS ON (-0.15%)**. The 1% low changed by -2.36%. The candidate stays OFF.

| Metric | OFF | ON |
| --- | ---: | ---: |
| Average FPS | 78.362 | 78.243 |
| 1% low FPS | 55.363 | 54.054 |
| Longest frame | 31.000 ms | 40.000 ms |
| Counted draws | 3134 | 3129 |
| Actual two-pending submissions | 3130 / 3134 | 3125 / 3129 |
| Frames over 33.333 ms | 0 | 1 |

Each arm ran once, in OFF→ON order. Both raw frame sequences cover simulation 540 up to 1740 with zero trimmed outliers. All 13 input hashes, 40 simulation/unit/camera/speed bins, archived game/map and pathfinding checksums, simulation events, and benchmark seed matched. Each run created 600 units and destroyed 433. The GroundDecal switch was the sole difference among 43 checked child settings. Both runs used AA0 cap2 and packet220 mode 0, with SensorLOS and Sensor31657 enabled and WeaponRange, RANGE_DIVISION233 and prejoin disabled.

The prior native capture's bound buffer at simulation 1590 contains 32,279 track instances. Current logs confirm GroundDecals=2, standard drawer initialization, and the ground decal rendering path enabled. Live subtype counts and fragment shortcut coverage remain unmeasured.

Both games exited normally and their process groups were cleaned up. The display was locked and asleep at the recorded checks for both arms. The engine requested presentation. Physical scanout remains unverified. The run's temporary display assertion was released. Default cap1 and the default-off switch are preserved.

One configure and one engine build completed. The prior GPU parity result was reused. No helper rebuild, GPU parity rerun, capture, profile, new instrumentation or retry was performed.
