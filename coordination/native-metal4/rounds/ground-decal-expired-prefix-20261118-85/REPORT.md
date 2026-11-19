# GroundDecal expired-prefix FPS comparison

The observed average was **78.786 FPS OFF → 78.675 FPS ON (-0.14%)**. The observed 1% low increased by 2.32%. The fixed pair missed the complete bin-match requirement. The candidate stays OFF.

| Metric | OFF | ON |
| --- | ---: | ---: |
| Average FPS | 78.786 | 78.675 |
| 1% low FPS | 55.749 | 57.041 |
| p99 frame interval | 16.000 ms | 16.000 ms |
| Longest interval | 38.000 ms | 33.000 ms |
| Counted draws | 3152 | 3147 |
| Real two-pending submissions | 3149 / 3152 | 3144 / 3147 |
| Frames over 33.333 ms | 1 | 0 |

One OFF and one ON game used the same signed executable, AA0 cap2, 13 matching input hashes, stable SensorLOS and Sensor31657 ON, and WeaponRange, R233, GroundDecal terrain-color and prejoin OFF. The candidate switch was the only difference among 44 observed child settings. Maps, checksums, camera, logged gameplay and pacing events, and totals of 600 created and 433 destroyed matched. Only 36 of 40 per-bin observation rows matched. OFF recorded ticks 839 and 1649 at two internal boundaries where ON recorded 840 and 1650. The former sampled 167 vs 166 units at those different ticks. The complete 540–1739 windows and every measured interval remain included. The loading times before the counted window were 45.210 and 16.617 seconds. The available evidence does not tie this asymmetry to measured FPS. No replacement run was made.

One GPU helper run passed 144 bytewise color/depth/stencil comparisons, including direct and indexed blending order controls and all-expired cases. The CPU case included fused and split rounding, createFrameMax vs createFrameMin, exceptional inputs, age neighbors, same-size revision replacement and rewind even above the cached cutoff. Existing GPU owners and zero-survivor atlas completion are preserved by source inspection. Injected pending GPU failures are outside the test.

The historical sim-1590 buffer holds 9,401 eligible leading rows. The corresponding 1,804,992 bytes quantify that source revision if uploaded. Current type, skipped rows, upload frequency, and actual eliminated bytes in these games were unmeasured. Both games logged the normal GroundDecal path and drawable present requests. The display stayed locked and asleep at the checks, so physical scanout remains unverified. Both owned processes exited and the temporary display assertion was released.

One helper compile/run, one engine configure/build, and exactly one game per setting completed. Captures, profiles, added runtime instrumentation and retries were zero. Default cap1 and candidate OFF were preserved.
