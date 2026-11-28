# Round 39 · Five fixed indexed geometry routes

**64.316 → 64.736 FPS (+0.65%)** in one same-binary OFF/ON pair.

| Metric | OFF | ON |
|---|---:|---:|
| Average FPS | 64.316 | 64.736 |
| Median frame time (ms) | 16.000 | 16.000 |
| p95 frame time (ms) | 17.000 | 17.000 |
| p99 frame time (ms) | 18.000 | 18.000 |
| Longest frame (ms) | 266.000 | 31.000 |
| 1% low FPS | 35.230 | 53.608 |

The existing R03 scenario used ticks 540–1740, AA off, a single-sample target, a 1512 × 950 viewport and the same combat seed and camera.

The synced combat and interval unit counts matched.

The same 19 non-system libraries loaded by resolved path, architecture and SHA-256.

The existing log did not prove a nonempty SensorLOS outline submission in both measured windows.

Both arms used the same bounded log. Existing runtime logs identify the SensorLOS outline family, but the actual index binding and the other four families cannot be inferred from the toggle or scene alone. Positive Unit Stencil uploads, if present, establish preparation only. See the per-path receipts.

The OFF run contains one 266 ms frame. The longest ON frame is 31 ms. The OFF outlier remains in the raw series and strongly affects its 1% low. Its cause is unverified. The median, p95 and p99 are the same in both arms.

This is one whole-game FPS pair. Run-to-run repeatability and per-family GPU timing remain unmeasured.

[Raw FPS bins](FPS-BINS.csv) · [Frame timing](FPS-EVIDENCE.json) · [Activation evidence](SENSORLOS-ACTIVATION-EVIDENCE.json)

The candidate uses exact R03 plus the sealed six-file source patch. The five indexed routes leave the original shader source, primitive order, instances and attachment state intact.

The existing SensorLOS fixture passed 12,065 color, depth and stencil comparisons for the indexed outline table. The existing Range199 fixture issued seven Metal4 submissions: two indexed, five direct. It reused the matching radar/sonar index alias pattern, validated the direct fallback, and checked the actual production cache allocation and reset for all five tables. These are distinct evidence scopes.

macOS SDK 27.2 candidate SHA-256 4021691207c675cd15af337979712a89016c3217921d189c322c9da7ad976f0f. The same executable hash and the actual OFF/ON environment were confirmed while both children were alive.

Configure 1, engine build 1, existing fixture compiles 2, GPU fixture invocations 2, games 2. Captures, profiles, screenshots and retries were zero.

[Patch](INDEXED-GEOMETRY.patch) · [Source and fixture pins](SOURCE-AND-FIXTURE-PINS.json) · [Fixture gate](FIXTURE-GATE.json) · [Per-path runtime evidence](PATH-ACTIVATION.json) · [Link evidence](SOURCE-OBJECT-LINK.json) · [Cleanup](CLEANUP-EVIDENCE.json)
