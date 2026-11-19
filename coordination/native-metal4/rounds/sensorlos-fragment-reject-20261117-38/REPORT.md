# Round 38 · SensorLOS fragment reject

**64.733 → 64.720 FPS (-0.02%)** in one R03/candidate pair.

| Metric | R03 original | Fragment reject |
|---|---:|---:|
| Average FPS | 64.733 | 64.720 |
| Median frame time (ms) | 16.000 | 16.000 |
| p95 frame time (ms) | 17.000 | 17.000 |
| p99 frame time (ms) | 18.000 | 18.000 |
| 1% low FPS | 54.054 | 55.437 |

The existing R03 scenario used ticks 540–1740, AA off, a single-sample target, a 1512 × 950 viewport and the same combat seed and camera.

The synced combat and interval unit counts matched.

The same 19 non-system libraries loaded by resolved path, architecture and SHA-256.

The existing log did not prove a nonempty SensorLOS outline submission in both measured windows.

Both arms used the same log-only witness, bounded to four samples per game. This is one whole-game pair. Independent SensorLOS timing and repeatability remain unmeasured.

[Raw FPS bins](FPS-BINS.csv) · [Frame timing](FPS-EVIDENCE.json) · [Activation evidence](SENSORLOS-ACTIVATION-EVIDENCE.json)

The single-sample fragment keeps the implicit-derivative mask sample ahead of the first discard. Surviving fragments use the original color, alpha and depth expressions. The fan, MSAA and R03 coverage functions retain their original bytes.

The existing fixture compared the patched shader to exact R03 under Metal API validation. All 12,065 color, depth and stencil comparisons matched, including mipmapped-mask and asymmetric stencil cases.

macOS SDK 27.2 candidate SHA-256 4df93fbda60c3c1d0fc930db951c120adf5341976963846b79846dc8a096ce75. The linked executable contains the exact patched fragment.

Configure 1, engine build 1, helper compile 1, GPU helper 1, games 2. Captures, profiles, screenshots and retries were zero.

[Patch](SENSORLOS-FRAGMENT.patch) · [Source review](SOURCE-REVIEW.md) · [GPU fixture](HELPER-RESULT.json) · [Link evidence](SOURCE-OBJECT-LINK.json) · [Cleanup](CLEANUP-EVIDENCE.json)
