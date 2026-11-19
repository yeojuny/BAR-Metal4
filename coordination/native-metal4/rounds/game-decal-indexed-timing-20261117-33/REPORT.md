# Round 33 — Indexed GameDecal GPU timing and ON smoke

The indexed fixture took a median 0.148479 ms per batch versus 0.207625 ms for the original submission, 28.49% less GPU time. It was faster in 24 of 24 retained pairs. Timing came from Metal 4 command-buffer GPU start and end timestamps after four warm-up pairs. Both modes rendered the same two overlapping rows with 64 draws per batch at actual sample count one. The fixed parity cases and paired batch attachment comparisons passed. Metal validation and capture were disabled for timing.

| Submission | Median GPU ms | First–third quartile GPU ms | Min–max GPU ms |
|---|---:|---:|---:|
| Original 96 vertices | 0.207625 | 0.207229–0.209625 | 0.206042–0.444458 |
| Indexed 96 UInt16 | 0.148479 | 0.147833–0.149125 | 0.146500–0.150000 |

The median paired ON minus OFF difference was -0.059292 ms. Batches alternated order and all measurements were retained. These intervals cover the complete fixture render batches, including attachment operations. Production GPU frame time and FPS remain unmeasured.

The candidate ON smoke consumed one engine launch. Its actual environment contained the requested enable flag and validation was active. I launched the raw executable from its build directory, which exposed both the build-tree and existing runtime copies of `bitmaps.sdz`. The engine reported duplicate base content and exited with code 21 before loading the game decal path. Actual game sample count and indexed entry remain unverified. No retry was made.

The owned helper, engine and wake processes were gone after the run. No GPU debugger sessions were active. The user's caffeinate process remained active and protected input hashes matched. Engine builds, captures, trace inspections, profiles and retries were zero.

- [GPU log](HELPER-GPU.log), [raw paired timestamps](GPU-TIMING-RAW.csv), and [timing statistics](TIMING-EVIDENCE.json)
- [Original smoke error](SMOKE-RAW.txt) and [smoke receipt](SMOKE-EVIDENCE.json)
- [Protected inputs](PROTECTED-HASHES.json) and [cleanup](CLEANUP-EVIDENCE.json)
