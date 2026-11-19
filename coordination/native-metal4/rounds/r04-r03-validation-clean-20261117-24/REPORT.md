# R04 versus R03: validation passed; R04 observed throughput lower

The authorized validation plus six clean games completed normally. R03 arithmetic mean: **63.810819 FPS**. R04: **59.072506 FPS**, **-7.425563%** versus R03. All three R04 values are below all three R03 values in this fixed fixture. This evidence does not support adopting R04 as a performance improvement.

FPS here is CPU-observed DrawScreen frames divided by measured wall seconds. It is not GPU execution rate or display-present FPS. Three runs per arm establish this bounded comparison; the cause of the observed regression was not isolated. No extra run, source change, capture, replay, or release promotion followed.

| Order | Run | FPS | Draws / seconds | 1% low FPS | >16.667ms | >33.333ms | Image |
| --- | --- | ---: | --- | ---: | ---: | ---: | --- |
| 1 | A1_R03 | 63.823405 | 2553 / 40.000999451 | 53.061227 | 643 | 0 | [PNG](raw/A1_R03/scene.png) |
| 2 | B1_R04 | 59.080798 | 2364 / 40.013000488 | 52.287584 | 1564 | 0 | [PNG](raw/B1_R04/scene.png) |
| 3 | B2_R04 | 59.123523 | 2365 / 40.000999451 | 51.391865 | 1559 | 0 | [PNG](raw/B2_R04/scene.png) |
| 4 | A2_R03 | 63.796812 | 2552 / 40.001998901 | 54.507340 | 661 | 0 | [PNG](raw/A2_R03/scene.png) |
| 5 | A3_R03 | 63.812238 | 2553 / 40.007999420 | 53.388093 | 655 | 1 | [PNG](raw/A3_R03/scene.png) |
| 6 | B3_R04 | 59.013198 | 2361 / 40.007999420 | 50.420170 | 1562 | 0 | [PNG](raw/B3_R04/scene.png) |

The reported arm means are the arithmetic means of the three complete per-run FPS values. No historical samples, validation FPS, trimming, or reruns were used. Supporting frame intervals use the unchanged original observer, whose overhead was not independently quantified. All valid slow runs and slow frames are retained.

## Validation and matched conditions

The production R04 validation game had Metal API Validation enabled in the observed child and engine log, no matched Metal validation errors, and no shader/pipeline failure lines. The existing inactive widget/include `no GetInfo()` skip messages matched the R03 validation reference. Actual main target samples=1, drawable1512x982 and safe view1512x950 were observed. Native origin[0,32] follows the same native coordinate contract from the logged geometry. The actual recorded child flags matched expectations before count; PREPARE1/BEGIN0/worker1 and S01/G01 production were enabled. S01 positive split and G01 indexed draw witnesses passed. The game exited normally, its process group was gone and awake assertion released before clean started. See [validation gate](VALIDATION-GATE.json) and [validation image](raw/V_R04/scene.png).

All six clean runs retained Ultra preset4, AA0/requestedMSAALevel-1, effective main sample1, VSync0, idle/offscreen limits disabled, the same nineteen libraries and production flags. Validation, trace and diagnostic probes were off in clean; no clean G01 probe or CPU trace rows appeared. Existing preflight receipts observed AC power and active/unlocked/awake display. The controller, parser, pacing, preflight and cleanup logic were unchanged.

**All40 simulation/unit/camera/speed/pause rows match exactly across all6 clean runs.** Seeds, initial unit state, benchmark command, map and created/destroyed counts also match. Count is simulation540..1740, forty bins. Warmup, loading and teardown are recorded separately in [TIMING-CHECK.json](TIMING-CHECK.json). Historical planning metadata600..1800 was corrected in new LAUNCH records while preserving the original metadata; the controller remained byte-identical. See [world check](WORLD-CHECK.json) and [environment/geometry check](ENVIRONMENT-GEOMETRY-CHECK.json).

## Images and limits

All seven actual post-count PNGs were inspected and are published with raw evidence. Terrain, ravine, structures, tanks, shadows, markings and visible sensor/range lines are present; no obvious R04-only fixed-geometry corruption or omission was observed. Smoke, particles, water/cloud shading and timing overlays vary. This is bounded visual inspection, not exhaustive pixel/stencil equivalence.

The orange/red range circle is present in all six current clean images: R03 3/3 and R04 3/3. Its historical appearance/absence is still unexplained; it is not silently classified as natural animation. Post-count snapshots and matched world rows do not prove identical unsynced UI state or every rendered pixel throughout count. No image or run was excluded or repeated. See [IMAGE-REVIEW.json](IMAGE-REVIEW.json).

## Fixed inputs and closure

Source `5d20bd842b3c881166abb58f361482a5d7bb19e7`; manifest `9e022fbfc7e2aad66c37aabd1b117f17b3c3318cf66c1280b14edf11d7beaf70`. R03 signed binary `107958872e5d962e4d27803a0fb1af0e842b18bd6982bd9b74e115d956890ab9`; R04 signed binary `4ff86017e4c16b71a19c77cbfcfa77bd52e631b99e694cef7bd547c38f625d86`. Runner `d1c4467a2dd4fd8bf1650a812111ae99d643fc8e8677a4beec359317e6f8fb74` differs from the successful R03 runner only in fourteen declared string literals for product pins, arm/suite labels and descriptions; full AST equivalence after the literal mapping is recorded.

The candidate private runtime copies the signed R04 bytes and the same nineteen libraries, with142 data files identical to R03. Original sources, binaries, runner and prior consumed profiles were preserved. No build/sign/helper or new source work occurred in this round.

Actual consumption: validation1 + clean6 =7 games; builds/helpers/captures/replays/retries/source edits0. The existing native post-count SaveImage procedure is the authorized image evidence, not a GPU tool capture. All seven process groups are gone; both campaign controllers closed and their awake assertions were released. `bar` stays PAUSED and was not retargeted. Raw logs, all forty sample rows, frame intervals, execution receipts, frozen launch/env hashes and original PNGs accompany this report.

Return to JINI for the next decision. No implementation, extra experiment, or adoption is authorized by this result. Existing SensorLOS/Beherith MIT and engine GPL notices remain preserved. [UPSTREAM-CONTRIBUTIONS.json](UPSTREAM-CONTRIBUTIONS.json) retains Tom J Nowell (tomjn) Recoil PR #3695 LuaVBO and PR #3035 macOS affinity/QoS credits plus existing benbreen/yeojuny provenance and scope; no new external refinement was adopted.
