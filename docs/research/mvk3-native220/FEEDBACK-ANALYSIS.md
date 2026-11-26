# Native 220 prior-submission wait breakdown

This reanalysis uses the Native collection-ON run from the earlier unlocked diagnostic experiment. No game was relaunched and no build was performed. The original is the [GPU trace](original/gpu/native-budget192.csv.gz) in the comparison packet; its uncompressed SHA-256 is `88fb94eb5b01ca9d9c405dc7dd66774641f0a32ef571d1f9a5aa64820e07cf40`.

All 6,388 submission record groups followed the sequence submission envelope 20, previous event/feedback wait 21, GPU commit 10, completed-owner release 23. Each kind 21 was linked to the preceding group's kind 10. Completion flags, errors, thread identity and timestamp order were verified. The first and last 100 ms of the original simulation window 600–1800 were removed; the analysis includes only the 6,357 complete waits inside the resulting window. The overlap between waiting and the post-GPU feedback interval is `max(0, min(wait.end, previous.callback) - max(wait.begin, previous.gpu_end))`.

| Interval | Mean | Median | p99 |
| --- | ---: | ---: | ---: |
| Actual combined event and feedback wait | 0.0510ms | 0.0006ms | 0.7657ms |
| GPU completion to feedback callback | 0.1463ms | 0.1437ms | 0.2498ms |
| Actual wait overlapping the post-GPU callback interval | 0.0162ms | 0ms | 0.1679ms |
| Portion waited before GPU completion | 0.0332ms | 0ms | 0.5975ms |

The post-GPU callback interval overlapped waiting in 810 of 6,357 cases. This does not support the hypothesis that the full feedback delay is exposed every frame in this scene. Since callback and event order can differ, 0.0162ms is overlap, not measured recoverable callback wait. This trace also cannot estimate gains from a different submission structure, including preparation outside this wait or engine previous-frame completion requirements. To test GPU submission depth two, use a separate A/B with independent slot ownership and measure submission, presentation and frame intervals together.
