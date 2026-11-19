# All That Simmers cap1 frame intervals

The November 18 Simmers match recorded 183.509 average FPS and 96.229 FPS for the slowest 1% of DrawScreen intervals. The November 14 PREPARE cap1 reference was 185.106 average and 140.492 1% low across four matches, with zero intervals over 16.667 ms. That reference shares the map, game archive, start script and timed controller. The engine build, options, warmup procedure and run count differ. Both were cap1.

| Elapsed seconds (approx.) | Interval ms | Draw | One-second bin | Simulation bin |
| ---: | ---: | ---: | ---: | --- |
| 4.557 | 20 | 1804 | 5 | 420–450 |
| 9.714 | 18 | 2771 | 10 | 570–600 |
| 14.870 | 18 | 3728 | 15 | 720–750 |
| 15.872 | 17 | 3907 | 16 | 750–780 |
| 15.955 | 23 | 3915 | 16 | 750–780 |
| 16.047 | 32 | 3923 | 17 | 780–810 |
| 16.147 | 23 | 3932 | 17 | 780–810 |
| 20.020 | 19 | 4646 | 20 | 870–900 |
| 25.176 | 19 | 5589 | 26 | 1050–1080 |
| 30.338 | 18 | 6527 | 31 | 1200–1230 |
| 35.485 | 17 | 7466 | 36 | 1350–1380 |

Four long intervals clustered around 15.87–16.15 seconds, straddling bins 16 and 17. The other seven show a roughly 5.15-second spacing. The matcher found no timestamped engine event within 0.25 seconds of the listed intervals, using the original clock anchors and cumulative draw intervals. The regular spacing is an observation and does not identify the process or operation responsible. All 74 intervals in the slowest 1% were included, averaging 10.392 ms. The other 63 of those intervals averaged 8.651 ms, so the 11 listed intervals are only part of the tail. Start and final preflight both observed an active unlocked display.

[Fixed earlier comparison](https://github.com/yeojuny/BAR-on-Metal4/blob/4fa216893c847f60e09cf0d1952eff4f5b3de38a/docs/preservation/begin-prepare-20261114/comparison/owner/REPORT.md)
