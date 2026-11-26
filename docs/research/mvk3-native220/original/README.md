# MVK3 versus Native Metal 4 220

This comparison was run on one Mac on 2026-11-05 Pacific Time. Both used the same macro, camera, seed, scene without AI, 1512×950 viewport, Ultra, MSAA 0 and VSync 0. Simulation frames 600–1800 were measured, with normal exit at 2250. FPS represents engine DrawScreen progression; p99 is the CPU-observed interval. Pixel and shader-work equivalence, overall gameplay performance and performance on other Macs were outside the scope.

| Run group | MVK3 | Native 220 | Native / MVK3 |
| --- | ---: | ---: | ---: |
| Product builds, two trials per backend | 226.02 FPS | 161.23 FPS | −28.67% |
| Diagnostic builds, unlocked display, frame-limit widget inactive, one collection-OFF run each | 224.61 FPS | 161.27 FPS | −28.20% |
| Same diagnostic builds, one collection-ON run each | 224.68 FPS | 159.70 FPS | −28.92% |

Product-build p99 was 7 ms for MVK3 and 8 ms for Native, with 11 and one intervals over 16.667 ms respectively. The auxiliary MVK4 result from the same experiment was 99.24 FPS. The BC213 frame-limit widget was enabled in the product-build experiment and confirmed inactive at runtime in the separate diagnostic experiment. These groups are kept separate. Four initial diagnostic runs with incorrect lock and widget settings are retained separately.

In collection-ON, unioned command-buffer intervals were 3.213 ms of MVK3 work and 5.130 ms of Native main work per draw, a 1.917 ms difference. Excluding MVK3 presentation buffers increased the difference by 0.000019 ms per draw. Recorded envelopes are command-buffer-level and can include dependency waits or non-raster work. Collection coverage and command-buffer composition differ. The individual ON/OFF runs were in fixed order and cannot separate order effects, variation or instrumentation overhead.

## Optimization hypotheses and existing observations

- **CPU threading.** A separate stack sample observed Zink drawing and Vulkan descriptor operations on MVK3's `spring:gdrv0`, and MoltenVK and Apple Metal encoding on `spring:zfq0`. Sampling overhead is separate from the performance table. Work distribution is visible, but not per-core active time or its numerical contribution to the gap.
- **GPU submission depth.** Native distinguishes CPU recording slots from GPU-submitted slots; delayed submission waits for previous GPU event and feedback completion. Between recorded Native main envelopes, previous GPU completion to next host commit averaged 0.948 ms, and to next GPU start 1.132 ms. This includes preparation, host scheduling and submission conditions. MVK3 internal work-envelope gaps totaled 1.238 ms/draw. Different command-buffer grouping prevents interpreting these as equivalent hardware idle times or isolating feedback latency. Earlier pass 87 obtained +8.19% from CPU/GPU overlap with a GPU submission depth of one.
- **Residency.** Permanent, infrequently changing and transient allocations should be distinguished. 220 already has shared ownership and asynchronous commit. Long-lived sets cannot be assigned a numerical benefit before measuring actual additions/removals, commit waits and overlapping preparation.
- **Barriers and MRT copies.** Sixteen separately observed MRT0 owners in 220 had zero previous-image copies; the outer pass envelope had window medians of approximately 0.49–0.56 ms. These are not independent barrier, draw or attachment-store measurements. Begin with one exact producer–consumer boundary.
- **Upload/encoding separation.** Both product Native trials reported zero fallback across 13 tracked pool summaries, not a census of general allocation overhead. An earlier narrow capture/prepare split lowered 160.58 to 156.37 FPS while p99 changed from 10 to 8 ms. It cannot establish the effect of separating the whole render thread.
- **Build.** The preserved Native 220 link command used `-O2` and `-ffp-contract=off`. Compile options for every input and equivalence to MVK3 were not established. Changing optimization level or LTO requires its own comparison.

[Public Native submission implementation](https://github.com/yeojuny/BAR-Metal4/blob/main/engine/rts/Rendering/Metal4/Metal4RuntimeNative.mm) · [Public MRT implementation](https://github.com/yeojuny/BAR-Metal4/blob/main/engine/rts/Rendering/Metal4/Metal4CUSDeferred.mm) · [Public build recipe](https://github.com/yeojuny/BAR-Metal4/blob/main/docs/REPRODUCING.md)

## Data files

- [stock.json](stock.json), [diagnostic-unlocked.json](diagnostic-unlocked.json), and [diagnostic-locked.json](diagnostic-locked.json) contain order, measurements, limitations and original hashes.
- The 14 CSV files in [frames](frames) preserve original intervals; `seconds` is elapsed wall time between DrawScreen calls.
- [gpu-intervals.json](gpu-intervals.json) and [gpu](gpu) contain the union results and both losslessly compressed raw traces, with pre/post compression SHA-256 hashes.
- [SHA256SUMS](SHA256SUMS) records this packet's file hashes. Only numeric CSVs and necessary measurement metadata were selected.

The earlier Korean document is preserved in [the original Git snapshot](https://github.com/yeojuny/BAR-Metal4/blob/99e1e012c64d94c3e6378c1ca16e44fd01a855b6/docs/research/mvk3-native220/original/README.md). Raw traces and their hashes are unchanged.
