# Evidence index

Start with [SUMMARY.json](SUMMARY.json), [numeric verification](evidence/NUMERIC-VERIFICATION.json), [provenance](PROVENANCE.json), and [reproduction boundaries](REPRODUCING.md). The underlying records are preserved with their original stage names. Personal path prefixes are standardized; original and published SHA-256 values are separate in PROVENANCE.json.

`draws` in the benchmark metric means **DrawScreen frames**, not Metal draw calls. FPS is DrawScreen frames divided by measured wall seconds. Frame intervals are CPU-observed callback intervals, not GPU duration or presentation latency.

## Independent high-load comparisons

| Comparison | Final result | Six-run raw record | Frame intervals and per-run conditions |
| --- | --- | --- | --- |
| R01: +12.167391456% | [RESULT](evidence/sensorlos-region-fix-20261116-01/report/RESULT.json) | [RUNS](evidence/sensorlos-region-fix-20261116-01/measurement/runs/sensorlos_clean01/clean_paced/RUNS.json) | [runs](evidence/sensorlos-region-fix-20261116-01/measurement/runs/sensorlos_clean01/clean_paced/preparation/runs/) |
| S01: +9.179513046% | [RESULT](evidence/decal-grass-boundary-s01-20261115-01/report/RESULT.json) | [RUNS](evidence/decal-grass-boundary-s01-20261115-01/measurement/runs/s01_clean01/clean_paced/RUNS.json) | [runs](evidence/decal-grass-boundary-s01-20261115-01/measurement/runs/s01_clean01/clean_paced/preparation/runs/) |
| G01: +7.184183162% | [RESULT](evidence/ground-decal-index-g01-20261116-01/report/RESULT.json) | [RUNS](evidence/ground-decal-index-g01-20261116-01/measurement/runs/g01_clean01/clean_paced/RUNS.json) | [runs](evidence/ground-decal-index-g01-20261116-01/measurement/runs/g01_clean01/clean_paced/preparation/runs/) |
| G02: −0.084434720% | [RESULT](evidence/ground-decal-upload-g02-20261116-01/report/RESULT.json) | [RUNS](evidence/ground-decal-upload-g02-20261116-01/measurement/runs/g02_high_clean01/clean_paced/RUNS.json) | [runs](evidence/ground-decal-upload-g02-20261116-01/measurement/runs/g02_high_clean01/clean_paced/preparation/runs/) |

These are separate fresh comparisons; do not add or multiply the percentages into an observed cumulative result. All 24 valid runs, 960 one-second rows, and their complete frame-interval CSVs are included. Each run directory contains RESULT, CHILD-MODE, LAUNCH, lifecycle/cleanup, raw `engine.log`, and `TERRAIN211-FRAMES.csv`. All G01/G02 clean screenshots are included; R01/S01 retain the declared A1/B3 representative screenshots. No metric sample was removed for being slow.

The clean experiments' actual matched simulation interval is 540–1740. Some original LAUNCH records still say planned 600–1800; this historical metadata discrepancy was documented in the final reports and is not silently corrected here. Per-run 1% low averages are means of run-level values, not pooled frame intervals.

## All That Simmers: historical single runs

| Historical stage | FPS | Full 40-row result and frame intervals |
| --- | --- | --- |
| R01 / pre-S01 | 185.15462632441086 | [O1_SENSOR](evidence/sensorlos-region-fix-20261116-01/measurement/ordinary_once01/ordinary_paced/preparation/runs/O1_SENSOR/) |
| S01 | 185.46390215154858 | [O1_S01](evidence/decal-grass-boundary-s01-20261115-01/measurement/ordinary_once01/ordinary_paced/preparation/runs/O1_S01/) |
| G01 | 183.0 | [O1_G01](evidence/ground-decal-index-g01-20261116-01/measurement/ordinary_once01/ordinary_paced/preparation/runs/O1_G01/) |

These are separated-in-time single observations, not a paired causal benchmark. The actual indexed-route marker in the G01 ordinary observation is UNKNOWN. The R01 run's original parser failure and subsequent raw-data recovery are both retained in its run directory and [recovery record](evidence/sensorlos-region-fix-20261116-01/measurement/ordinary_once01/RECOVERY-RESULT.json).

[All 40-row differences](evidence/ground-decal-index-g01-20261116-01/analysis-next/SIMMERS-DRAW-DISTRIBUTION.json) show that S01→G01 lost 98 DrawScreen frames in total; the post-hoc 17–23-second window accounts for 86 of that net difference. The other 34 bins account for 12. This describes the distribution; it does not identify a cause, justify dropping bins, or establish a G01 regression.

[Grass applicability](evidence/decal-grass-boundary-s01-20261115-01/measurement/ordinary_once01/APPLICABILITY.json) records the original game's camera-height cull: minimum height 3682.61110397 exceeds cutoff 3599.99992. [Original grass Lua](evidence/decal-grass-boundary-s01-20261115-01/measurement/ordinary_once01/grass-source-reference.lua) and [map metadata](evidence/decal-grass-boundary-s01-20261115-01/measurement/ordinary_once01/mapinfo-reference.lua) accompany it. The map is not claimed to lack grass generally.

## G02: measured cost reduction without measured FPS gain

- [Disposition](evidence/ground-decal-upload-g02-20261116-01/DISPOSITION.json): G01 remains the performance baseline; G02 is retained as a cost-reduction candidate.
- [Diagnostic gate](evidence/ground-decal-upload-g02-20261116-01/measurement/FIRST-GATE-CLOSED.json): high-load owner-lifetime Prepare/packed/combined counts 3174; one actual indexed draw receipt reports shared uniform/index buffer, offset368, upload_buffers1, no fallback. Total Ground buffer creations4449 also include other resources. These are owner-lifetime counts, including warmup, not 40-second ROI counters or draw counts.
- [High API validation run](evidence/ground-decal-upload-g02-20261116-01/measurement/runs/g02_high_valid01/validation_paced/preparation/runs/V_G02/): actual flags, packed receipt, zero matched API errors, PNG and normal exit.
- [Low owner summary](evidence/ground-decal-upload-g02-20261116-01/measurement/low_diagnostic01/ordinary_paced/preparation/runs/O1_G02/OWNER-SUMMARY.json): owner created1, Prepare0 and buffer creations0. The [raw log](evidence/ground-decal-upload-g02-20261116-01/measurement/low_diagnostic01/ordinary_paced/preparation/runs/O1_G02/engine.log) retains the positive owner-summary record. This is not an inference from missing logging.
- [Low selection decision](evidence/ground-decal-upload-g02-20261116-01/measurement/CLEAN-SCOPE-DECISION.json): no low clean comparison was selected before high clean measurements began. This is not post-hoc rejection of a slow result.
- [GPU helper result](evidence/ground-decal-upload-g02-20261116-01/validation/runs/g02_parity01/HELPER-RESULT.json), [stdout](evidence/ground-decal-upload-g02-20261116-01/validation/runs/g02_parity01/helper.stdout.log), and [actual helper source](evidence/ground-decal-upload-g02-20261116-01/validation/gpu_upload_parity.mm): 32 synthetic old/G01/G02 color/depth/stencil cases. Read the scope limits; this is not whole-game/Ultra pixel parity.

R01 [GPU parity](evidence/sensorlos-region-fix-20261116-01/validation/GPU-PARITY-RESULT.json), S01 [protocol parity](evidence/decal-grass-boundary-s01-20261115-01/validation/runs/s01_first_gate01/HELPER-RESULT.json), and G01 [index parity](evidence/ground-decal-index-g01-20261116-01/validation/runs/g01_first_gate01/HELPER-RESULT.json) remain separate gates. S01's first diagnostic attempt and accepted second attempt are both retained.

## Saved capture and unfinished candidates

[Saved S01 capture analysis](evidence/decal-grass-boundary-s01-20261115-01/measurement/capture_s01_01/analysis/) contains API calls, pipeline identities, shader costs and timeline extracts from the 9a44 binary. These are a single saved replay's observations, not current G01/G02 frame-time percentages. [EXCLUDED.json](evidence/EXCLUDED.json) pins the omitted full trace and lists other intentional exclusions.

G03's conditional minimap-fetch hypothesis is held; see [proof domain](evidence/ground-decal-index-g01-20261116-01/analysis-next/FS-PROOF-DOMAIN.json). G04 has source/CPU preparation only and is frozen [validation incomplete](evidence/ground-decal-hoist-g04-20261116-01/AUDIT-FREEZE.json). It has no GPU helper execution, engine build, game run or FPS result. Neither belongs in the tested performance chain.

## Source and attribution

[CHAIN.json](patches/CHAIN.json) enumerates complete source deltas and successful replay checks from public a3 to the frozen tested trees. [source-snapshots](source-snapshots/) contains complete hash manifests plus actual changed/core files; it is intentionally not a full engine checkout. [Original patch records](patches/original-records/) retain the incremental experiment history, including S01 revision02.

[Contributor records](licenses/UPSTREAM-CONTRIBUTIONS.json), [repository notice](licenses/a3-repository/NOTICE.md), [engine license](licenses/LICENSE.html), and original headers preserve attribution and licensing. Earlier records keep the status they had when written; use each final report and disposition for the latest completed result rather than reading an earlier UNKNOWN as the final state.
