# Round 36 · Clean Prepare batch FPS


**64.014 → 64.114 FPS (+0.16%)** in one clean pair.

| Metric | R03 original | Prepare batch |
|---|---:|---:|
| Average FPS | 64.014 | 64.114 |
| Median frame time (ms) | 16.000 | 16.000 |
| p95 frame time (ms) | 17.000 | 17.000 |
| p99 frame time (ms) | 18.000 | 18.000 |
| 1% low FPS | 53.061 | 55.437 |
| Draws / measured seconds | 2561 / 40.007 | 2565 / 40.007 |



Both timing windows use the existing Seths Ravine combat scene, combat seed 7654321, AA off, a 1512 × 950 viewport, and simulation ticks 540–1740.

All workload checks matched, including the visible unit counts by interval and the synced result of 600 units created and 433 destroyed.

Metal API and shader validation and Metal capture were disabled. The original paced runner handled both children. The optional screenshot adapter was omitted.

Round 35 diagnostic logs yielded **63.429 → 63.369 FPS (-0.10%)**. Metal validation was enabled, screenshots were taken after the counted window, and the two executables used different dependency packaging.

The single clean pair does not establish the distribution of run-to-run variance. The per-frame raw timing files and the interval counts preserve the measured distribution.

The dyld logs show the same 19 non-system dylibs by resolved path, architecture and SHA-256. Both children loaded all 9 expected direct Homebrew files.

Round 35 confirmed a Commands FX draw and a WeaponRange Prepare invocation. BuildSquare, Sensor31657Circle, StartBox, EnergyExplosion and LuaNano remain unverified. Its images show localized effects differences with the combat layout intact.

Games 2, normal exits 2, screenshots 0, rebuilds 0, trace captures 0, profiles 0, retries 0. [FPS evidence](FPS-EVIDENCE.json) · [Raw interval bins](FPS-BINS.csv) · [Dependency evidence](DEPENDENCY-EVIDENCE.json) · [Cleanup](CLEANUP-EVIDENCE.json)
