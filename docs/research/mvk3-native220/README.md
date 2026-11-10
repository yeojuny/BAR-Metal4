# MVK3 versus Native Metal 4 220

This packet is for investigating where the measured throughput gap arises. It includes the stock comparisons, the failed locked-display diagnostic controls, the unlocked collector OFF/ON runs, both command-buffer traces, and the follow-up CPU, wait, GPU-stage and resource-lineage studies. The test was on one Mac on November 5, 2026 Pacific. It predates the separate local Native 233 benchmark.

[Korean comparison report](original/README.md) · [Korean feedback-wait analysis](FEEDBACK-ANALYSIS-KO.md) · [Data inventory and hashes](DATA-MANIFEST.json) · [Independent review questions](REVIEW.md)

## What ran

The paired workloads used the same recorded macro, seed and fixed camera in an AI-free scene, 1512×950 logical viewport, Ultra, AA 0 and VSync 0. Measurement covered simulation frames 600–1800 and normal exit was at 2250. FPS here is engine DrawScreen throughput. The frame interval files contain CPU-observed elapsed seconds between DrawScreen calls, not GPU frame times or screen refresh measurements.

| Experiment | MVK3 | Native 220 | Interpretation |
| --- | ---: | ---: | --- |
| Stock products, 2 runs each | 226.02 FPS | 161.23 FPS | Native was 28.67% lower in this scene. p99 was 7 vs 8 ms. The BC213 frame limiter widget was loaded in this experiment. |
| Unlocked diagnostic, collector OFF, one run each | 224.61 FPS | 161.27 FPS | The limiter was inactive. The gap persisted in this separate experiment. |
| Unlocked diagnostic, collector ON, one run each | 224.68 FPS | 159.70 FPS | Used for command-buffer timing. The fixed OFF/ON order and one run per condition cannot separate collection overhead from order or normal variation. |
| Initial locked diagnostic, four runs | See file | See file | Screen lock and active limiter made this run set unsuitable for attributing the gap. Preserved for audit. |

The stock run set also measured MoltenVK's Metal 4 mode at 99.24 FPS. That route is distinct from Native 220. In the MVK3/MVK4 diagnostic, the named engine Draw profiler differed by about 0.104 ms/draw while the whole DrawScreen interval differed by about 5.652 ms. That scope cannot explain the larger difference by itself. The named profiler scopes can overlap and are not exclusive CPU time. See [stock](original/stock.json), [unlocked](original/diagnostic-unlocked.json), [locked](original/diagnostic-locked.json), and [MVK3/MVK4 follow-up](analysis/mvk3-mvk4.json).

## Where work and waiting were observed

| Evidence | Observation | What remains unknown |
| --- | --- | --- |
| Collector ON, command-buffer interval union | MVK3 work-only **3.213 ms/draw**, Native deferred main **5.130 ms/draw**, difference **1.917 ms/draw**. Excluding MVK presentation buffers changes the difference by just 0.000019 ms/draw. | Interval union includes dependencies and waits. The buffer organizations and coverage differ. No exclusive shader cost or physical GPU utilization follows. |
| Gaps inside the observed command streams | MVK3 **1.238 ms/draw**, with the split at host submit about **0.923 + 0.315 ms/draw**. Native **1.132 ms/draw**, of which previous GPU end → next host commit begin averaged about **0.948 ms** and the remaining normalized portion was about **0.184 ms/draw**. | These are gaps in each captured stream, not matched GPU idle times. The 0.948 ms includes host scheduling and work before the commit. |
| Native prior event + feedback wait | Over **6,357** trimmed waits, mean **0.0510 ms**, p99 **0.7657 ms**. GPU end → callback averaged **0.1463 ms**. Only **0.0162 ms** on average overlapped actual waiting after GPU end. Waiting before GPU end averaged **0.0332 ms**. The post-GPU overlap occurred **810/6,357** times. | Callback latency is not directly removable frame time. This measures the existing submission scheme and cannot predict deeper GPU submission or change the need to retain resources through completion. |
| MVK3 CPU sampling | 1,219 main-thread samples, including 469 queue-fence path witnesses and 169 Mesa glthread-finish witnesses. Worker stacks separately showed Zink/Vulkan descriptor work and MoltenVK/Metal encoding. | Stack path counts overlap and are not exclusive time. Native sampling had only 3 useful main-thread samples plus a 10.668-second intrusive gap. CPU-share comparison is invalid. |
| Native stage timings | At the sim1200 anchor, median main command start → pre-water **2.669 ms**. Pre-water → postprocess/UI end **2.271 ms**. Opaque MRT marker span **0.500 ms**. | These are medians from a small set of same-owner marker observations. They are neither exclusive costs nor matched with MVK. MVK issued all five markers in eight owners, but zero owners yielded a complete valid timestamp sequence. Zero query results were invalid, not zero-time passes. |
| Separate MVK replay exploration | Selected replay encoders reported sums of vertex+fragment stages of **1.2055 ms** for a forward CUS candidate, **1.0382 ms** for five-color CUS MRT and **0.1832 ms** for combined CAS+UI. | Replay timing and stage sums are not live frame duration. The matching Native profile failed, so this cannot attribute the live gap or isolate CAS. |

[Command interval aggregate](original/gpu-intervals.json) · [Host submission split](analysis/submission-overlap.json) · [Native feedback method](FEEDBACK-ANALYSIS-KO.md) · [CPU samples](analysis/cpu-sampling.json) · [Native CPU phase medians](analysis/native-cpu-phases.json) · [GPU marker coverage](analysis/gpu-stage-matching.json) · [Replay timings](analysis/replay-profile.json)

## Open boundaries and tests that constrain the next step

- **Queue depth and completion.** One CPU recording slot and one outstanding GPU submission are different constraints. Small direct wait averages rule out treating the entire callback delay as a per-frame saving. Test whether the next frame is ready before the previous GPU end, and prove that independent slot ownership and display order remain correct before changing queue depth.
- **Producer and consumer dependencies.** Sixteen sampled Native MRT0 owners recorded zero prior-image copies. The outside pass marker spans include draws and attachment work as well as any synchronization. Before changing a barrier, isolate a single producer–consumer pair and prove the same attachment version, relevant stages, order, and completion lifetime.
- **CAS input and screen-copy lineage.** One Native main-attachment → snapshot writer → CAS input boundary was linked. The MVK resource observer loaded and installed hooks but did not record the required capture-time resource and writer receipts. Cross-backend texture identity and pixel/workload equivalence remain unresolved. A 1512×982 main attachment and 1512×950 snapshot are distinct actual descriptors. See [CAS capture result](analysis/cas-lineage.json) and [screen-copy result](analysis/screen-copy.json).
- **Upload and prepare.** Both stock Native runs reported no fallback in 13 tracked pool summaries. This is not a census of general allocations. A narrow prior capture/prepare split reduced mean FPS from 160.58 to 156.37 while p99 changed from 10 to 8 ms, so fewer visible stalls did not establish higher throughput.
- **MVK3 async CPU submit wait.** `BAR_ZINK_MVK_DISABLE_ASYNC_WAIT=1` bypasses the CPU worker's `flush_completed` return wait only on the applicable threaded async flush path. GPU fences, timeline semaphores, and drawable order remain. Historical R80 runs showed +9.43% in one Metal3 pair and +1.55% in the repeat. A different R85 heavy 4× scene with AC, three runs each, measured 46.9861 → 46.9856 FPS. These are separate experiments from 220 and do not predict a Native event change.
- **Compile and visual equivalence.** The observed Native link command used `-O2` and `-ffp-contract=off`. Whole-input compiler parity and equal GPU work were not established. Build flags, per-pass rendering inputs, and driver/runtime identity need separate controlled checks.

## Raw data and how to read it

- [All 14 DrawScreen interval CSVs](original/frames) cover the stock, locked and unlocked groups. Keep those groups separate and retain tail frames.
- [MVK3 Budget192 trace](original/gpu/mvk3-budget192.csv.gz) contains 348,047 records. [Native Budget192 trace](original/gpu/native-budget192.csv.gz) contains 25,552. Both are lossless snapshots with numeric rows and zero dropped records in the headers. For command-buffer unions, merge positive-duration intervals. MVK work is kind 1, its presentation is kind 11, and Native main is kind 10. The Native prior-wait reanalysis links kind 21 with kind 10 of the previous 20→21→10→23 group. Do not subtract sums of overlapping spans or mix the two clock origins.
- The [original SHA-256 list](original/SHA256SUMS) and [publication data inventory](DATA-MANIFEST.json) record both compressed and uncompressed trace hashes. Additional JSON files contain selected analysis fields. Live stack dumps and GPU capture bundles are represented here by their verified findings and limitations.

The current Native investigation is available separately. Its 233–242 evidence uses different scenes and binaries. [Native diagnostic packet](https://github.com/yeojuny/BAR-Metal4/blob/db4725a7490ff6dcc8d130646f76e28ebb9b7eae/docs/research/native233/diagnostic/README.md)
