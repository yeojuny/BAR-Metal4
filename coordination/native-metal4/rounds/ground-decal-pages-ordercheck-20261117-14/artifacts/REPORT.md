# GroundDecal separate BAAB order check

Exactly four clean games, not pooled with previous six-run ABBAAB cohort. CPU-observed DrawScreen frames / wall duration, not GPU/display FPS. Existing warmup and nominal40s controller preserved; clean validation/probes/traces OFF. No retries or run exclusions.

| Order/run | Mean FPS | 1% low | Counted seconds |
| --- | ---: | ---: | ---: |
| B1_GDP | 64.145191753 | 55.437103286 | 40.002998352 |
| A1_R03 | 64.151602910 | 55.437103286 | 39.999000549 |
| A2_R03 | 64.076601036 | 52.525255072 | 39.999000549 |
| B2_GDP | 64.063788887 | 53.941911623 | 40.006999969 |

R03 mean64.1141019726, sampleSD0.0530343337; range64.0766010356..64.1516029096; mean1%low53.9811791792. GDP mean64.1044903200, sampleSD0.0575605191; range64.0637888866..64.1451917534; mean1%low54.6895074550. Candidate vs baseline-0.0149914797%.

First execution is GDP64.14519, not markedly below later runs. The prior first R03 slowdown is not reproduced in this BAAB cohort; neither a general first-run slowdown nor its cache/thermal/background cause is proved. Current means are essentially similar. This does not corroborate a reproducible +4% patch gain. Prior six-run +4.03596% remains a separate recorded observation including its valid low A1. No pooled/trimmed/adjusted result, significance claim or automatic repeat.

All40 existing world rows match all four runs, including same simulation ticks, speed/pause/units/camera. Fixed frame240 initial state/seed154658555, frame540 visible168/camera and frame1740 screenshot/end600created433destroyed agree. Raw actual sample1, drawable1512x982, safeview1512x950, Ultra4/AA0, PREPARE1 BEGIN0 worker1/S01/G01 conditions confirmed. No missing tick states invented.

All four actual RGB scene.png files viewed: no obvious candidate-only static terrain/building/ground decal or occlusion corruption. Animated smoke/explosions/illumination/range arcs and debug text differ. RGB screenshots do not prove exact pixel/depth/stencil equality. Originals stay local with path/size/SHA256 in IMAGE-REVIEW; PNGs not uploaded.

Original frozen runner changed only ORDER and order labels/descriptions;11 functions including entire execution run AST unchanged. Validation suite unreachable. Six-run inherited COMPLETION.json runs=6 field is stale summary metadata deliberately preserved; actual ORDER/RUNS/OWNER-CLOSED and this result prove4 games, not6. Legacy600..1800 planning metadata also remains; actual unchanged controller540..1740 is authoritative.

Before/after signed binary and19 libs unchanged. Normal exits4, owned groups gone, image workers joined, awake released, all six recorded game/runner/awake PIDs absent. Original host/preflight/lifecycle/cleanup logs included; no power/security changes, new intrusive instrumentation, builds/helpers/validation/GPU capture/retries/source changes. No Texture/R04/main/release/Homebrew/app modifications. Actual copy-byte, physical allocation, RAM, GPU time/FPS and full failure/quarantine/revision/context lifetime safety remain unmeasured.

Existing engine GPL/Beherith MIT and recorded tomjn/benbreen provenance preserved in UPSTREAM-CONTRIBUTIONS.json. Return to JINI; stop after these four runs.
