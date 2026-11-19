# R02 SensorLOS winding regression

R02 preserves G01 heavy-battle performance while correcting the Y-major winding contract. This is a correctness regression result, not a new FPS gain.

| Condition | Run1 FPS | Run2 FPS | Run3 FPS | Arithmetic mean | Mean1% low |
| --- | ---: | ---: | ---: | ---: | ---: |
| G01 | 62.148447 | 62.300000 | 62.289013 | 62.245820 | 52.967531 |
| R02 | 62.240663 | 62.200000 | 62.243675 | 62.228113 | 52.274860 |

Candidate versus baseline: -0.028448%. All six valid runs retained. CPU-observed DrawScreen FPS, not GPU/present FPS. Sequence A B B A A B. Each count covers observed simulation540..1740, about40 wall seconds. Loading about16.7–17.0s and observed warmup about22.0s are excluded. Existing pacing was preserved; this was not exactly10s wall warmup.

Ultra AA0, actual main samples1, viewport1512x950, drawable1512x982, VSync/VSyncGame0, AC normal mode. PREPARE1 BEGIN0 worker1; S01/G01 enabled in both; D/Q/U/storage/sharedCUS/Sepia and clean validation/traces/probes disabled. All40 unit/camera/speed/pause/simulation bins match in each run, same seeds/initial state/command, created600/destroyed433.

One separate R02 Metal API Validation game passed with actual enabled log and no matched errors. S01 split and G01 indexed draw positive. Seven actual post-count PNGs inspected: no obvious new missing geometry/lines/corruption; animated effects and timing text vary. Dedicated prior GPU helper covered asymmetric stencil; no exhaustive image equivalence or AA4 claim here.

The label-based runner receipt leaves nativeFinalColorExtents null. Raw NativeFinalColorStage was manually verified in all7 games: sample1,1512x982. Runner unchanged. Frame-wise MRT/COW/copy totals and RAM/GPU timings were not instrumented.

All7 games exited normally and owned groups ended. Both awake keepers released. No retry, build, helper or capture. Original app/main/releases/Homebrew unchanged. JINI decides adoption and next source work.

Full40-bin samples, frame statistics, launch/settings/power/cleanup receipts and raw interval CSVs are included under receipts. EVIDENCE-INDEX.json pins preserved local raw logs and PNGs by SHA256 and path; these larger files are not claimed uploaded. Source and runner pins are in SUMMARY.json and prior build round03.

Source provenance is unchanged: SensorLOS shader lineage and existing contributor credits remain with the fixed candidate; this result packet adds no engine implementation or new authorship claim.
