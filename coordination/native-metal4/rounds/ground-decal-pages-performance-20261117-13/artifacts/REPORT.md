# GroundDecal clean comparison: all runs retained, causal gain uncertain

CPU-observed DrawScreen FPS; not GPU/display FPS. Exactly six fresh clean games, ABBAAB, nominal40s counted per arm with existing pacing. Validation/probes/traces OFF. No historical/validation pooling, no retries or invalidated slow run.

| Run | Mean FPS | 1% low | Counted seconds |
| --- | ---: | ---: | ---: |
| A1_R03 | 56.84147304 | 32.30337033 | 40.006000519 |
| B1_GDP | 64.14037811 | 53.16973676 | 40.006000519 |
| B2_GDP | 64.10961727 | 53.49794514 | 39.993999481 |
| A2_R03 | 64.10736793 | 53.83023031 | 40.011001587 |
| A3_R03 | 63.98720349 | 48.14814984 | 40.007999420 |
| B3_GDP | 64.15000000 | 55.43710329 | 40.000000000 |

R03 three-run arithmetic mean61.6453481530 FPS (sampleSD4.1607117106; range56.8414730415..64.1073679304), mean1%low44.7605834939. Candidate mean64.1333317952 (sampleSD0.0210933415; range64.1096172744..64.15), mean1%low54.0349283954. Observed mean difference+4.0359633236%.

Interpretation: first R03 A1=56.84147 is markedly below subsequent R03 A2=64.10737/A3=63.98720; all candidate runs64.10962..64.15. A1 is valid and remains included. Its slowdown cause is UNKNOWN. Candidate is stable in this cohort, but a reproducible +4% causal patch gain is not established by these three-per-arm results. No arbitrary significance cutoff, adjusted/trimmed primary result or extra run introduced.

Workload/screen: all40 world rows (start/end simulation, speed, pause, units, camera) match all six runs. Actual frame240 initial state/seed154658555, frame540 fullview visible168/camera, frame1740 screenshot and600created/433destroyed agree. Same pinned official benchmark/controller, no invented tick records; no wall-bin jitter in this clean cohort. Raw main sample1/drawable1512x982/safeview1512x950, Ultra4/AA0, PREPARE1 BEGIN0 worker1 and S01/G01 conditions confirmed.

All six actual PNGs viewed. No obvious candidate-only static terrain/building/ground decal corruption or occlusion regression. Animated smoke/explosions/illumination, range arcs and debug timings vary. RGB inspection is not exact pixel or depth/stencil attachment equality. Original PNGs stay local; full path/size/SHA256 are in IMAGE-REVIEW.json (not uploaded).

Before games: original R03 six-arm runner adapted only for hashes/labels/paths/suite selection; ten functions including entire run AST unchanged. No controller/parser/pacing/error/cleanup changes. Runner text/diff/review, all six fresh launch hashes and13 inputs per arm frozen before execution. Existing signed binaries and19 matching libs retained.

Cleanup: normal exits6, owned process groups gone, image workers joined, awake released; all eight recorded runner/game/awake PIDs absent. No source changes, rebuilds/helpers/validation/captures/replay/retry. Main/releases/Homebrew/apps/Texture/R04 untouched. Actual page-copy bytes, physical allocations, RAM and exhaustive GPU lifetime/failure/quarantine/context/revision safety remain unmeasured. Preserve structural candidate and evidence for JINI judgment rather than claim measured memory benefit.

Existing engine GPL/Beherith MIT and recorded tomjn/benbreen provenance preserved in UPSTREAM-CONTRIBUTIONS.json.
