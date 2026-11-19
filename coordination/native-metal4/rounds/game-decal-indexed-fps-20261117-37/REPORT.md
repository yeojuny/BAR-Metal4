# Round 37 · Indexed GameDecal OFF versus ON

**64.879 → 65.711 FPS (+1.28%)** in one clean in-game pair.

| Metric | OFF | ON |
|---|---:|---:|
| Average FPS | 64.879 | 65.711 |
| Median frame time (ms) | 16.000 | 15.000 |
| p95 frame time (ms) | 17.000 | 17.000 |
| p99 frame time (ms) | 17.000 | 17.000 |
| 1% low FPS | 53.719 | 58.315 |
| Draws / measured seconds | 2596 / 40.013 | 2628 / 39.993 |

Both arms used the same pinned candidate, separate runtime folders, and the existing Seths Ravine controller. The window spans simulation ticks 540–1740, with AA off, a 1512 × 950 viewport, and combat seed 7654321.

The measured scene, camera, interval unit counts, and synced combat totals matched.

Both children loaded the same 19 non-system libraries as round 36 by resolved path, architecture and SHA-256.

The existing log records a completed Lua game-decal draw in both arms. The ON child flag was observed, and the exact indexed draw API remains unverified by the available log.

The earlier GPU fixture measured 0.207625 → 0.148479 ms (-28.49%) across 24 pairs. That figure describes the fixture workload. Round 37 measures whole-game throughput over one pair, so repeatability remains unknown.

Games 2, normal exits 2, screenshots 0, rebuilds 0, helper runs 0, trace captures 0, profiles 0, retries 0.

[FPS evidence](FPS-EVIDENCE.json) · [Raw interval bins](FPS-BINS.csv) · [GameDecal evidence](GAME-DECAL-EVIDENCE.json) · [Dependency evidence](DEPENDENCY-EVIDENCE.json) · [Cleanup](CLEANUP-EVIDENCE.json)
