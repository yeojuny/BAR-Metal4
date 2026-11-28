# Round26 R03 / MVK3 matched-frame diagnostic

Outcome: **INSUFFICIENT — FAIL_MANDATORY_GAME_DECAL_SEMANTIC_WORKLOAD**. Both one-frame captures succeeded and their bound world uniforms prove simulation tick1590. The captured decal input data differs, so the requested matched-workload comparison is not valid. Profile commands: **0 / 0**. No retry or replacement scene was run.

Round `r03-mvk3-matched-frame-20261117-26`; attempt `r03-mvk3-framepair-02`. Reservation STATE102, one CAS from READY101. This is separate from the closed round25 attempt.

## Decisive evidence

The actual captured shader inputs use the same five float4 decal fields and 80-byte per-instance stride: `lengthwidthrotation`, `uvoffsets`, `alphastart_alphadecay_heatstart_heatdecay`, `worldPos`, `parameters`. Captured API bindings select the first row in both arms; the native uniform confirms `options.z=0`.

| Evidence | R03 | MVK3 |
|---|---|---|
| Captured decal draw | `/commands/cb0/re15/draw0`, api5272 | `/commands/cb3/re103/draw0`, api2786 |
| Input resource | `@buf310`, vertex slot0, offset0 | `@buf394`, slots29–25, offsets0/16/32/48/64 |
| Submitted instances | 3,541 | 4,511 |
| Geometry per instance | 96 non-indexed vertices | 96 UInt16 indices |
| First row position and birth tick | (5325, 329.9962158203125, 3072), tick60 | same |
| First row rotation | 2.533071517944336 | 4.408550262451172 |
| First row alpha-decay coefficient | 0.0029761907644569874 | 0.0006944444612599909 |
| Alpha at1590, calculated from captured shader formula | 0 | 1 |

The first row is inside the captured native shader's XY sphere-culling bound. The alpha values are an offline calculation of the captured formula, not a measured pixel count. Exact 80-byte row multiset overlap is **0**; position+birth overlap is **3,523**, with **18 R03-only** and **988 MVK3-only** entries. Therefore the failed gate is supported by differing logical input values, not merely by different renderer draw counts or object IDs. The source of those differences is **UNKNOWN**; wall-time drift, unsynced random state, and renderer behavior are not established causes.

The full buffers, captured shader sources, pipeline layouts, draw info and binding API excerpts are preserved under `analysis/R03` and `analysis/MVK3`. See `PAIR-GATE.json` and `OFFLINE-FINAL-VERIFICATION.json`.

## What matched, and what remains unproved

- **Actual capture tick:** R03 main-world draw `/commands/cb0/re17/draw0` binds `@buf2610` to vertex buffer3. Captured MSL places `timeInfo.x` at byte16: **1590.0**. MVK3 main-world draw `/commands/cb3/re108/draw0` binds descriptor `@buf57` at offset64. Its actual pointers at bytes72/80 resolve to inspected `@buf651`/`@buf652` addresses; captured MSL loads UBO0 word4: **1590.0**. This is bound-buffer proof, beyond a pause marker.
- **CPU scene:** all four before/after witnesses match byte-for-byte after removing only the draw counter: 168 units, 401 features, camera, selected and visible lists, positions, directions, health and other recorded fields. This witness does not establish every unsynced GPU input.
- **View:** both bound global buffers have viewGeometry1512×950; main-world attachments are1512×950 and displayed targets1512×982. Captured cameraView, cameraViewInv and cameraBillboardView match exactly. Projection x/y/w rows match exactly; the depth rows numerically follow z→(z+w)/2 (projection exact, view-projection max absolute difference0.0001220703125). Inverse projections were not accepted as bit-equal; no blanket all-matrix equality claim.
- **Time:** bound timeInfo is R03 `[1590,73.53400421142578,53,0]`, MVK3 `[1590,79.28500366210938,53,0]`. Draw-seconds differ by5.750999450683594. Particle/animation state and SensorLOS semantic input equivalence were not completed before the mandatory decal stop.

## Requested component table: comparison not accepted

All VS/FS costs, GPU active intervals and hardware counters below are **UNKNOWN / not profiled**. Available command descriptions are retained as observations, not a complete matching map.

| Component | Logical input / observed geometry | Resources, state and dependencies | VS / FS / timing |
|---|---|---|---|
| Terrain | R03 NativeSMFTerrain commands exported; complete MVK3 semantic mapping not accepted | Full input/resource and last-writer comparison incomplete | UNKNOWN |
| Units | CPU168 units match; actual world tick and view proof obtained. Witness draw: R03 48 indices×3 instances, MVK3 48×1; full draw aggregation not matched | R03 globals `@buf2610/@buf2609`; MVK3 descriptor links to `@buf651/@buf652` | UNKNOWN |
| Ground decal | R03 labeled draw reports12 triangles×32,279. MVK3 has the same geometric count in a candidate command; input equivalence unproved | Not accepted on count alone | UNKNOWN |
| Game decal | **FAIL:** 3,541 versus4,511 rows, differing rotation/decay/position sets | Actual bound resources, layouts and shader formulas exported. Draw-state links: R03 `@dss4`, MVK3 `@dss3`; DSS object properties, load/store and last-writer lineage remain UNKNOWN | UNKNOWN; profile blocked |
| SensorLOS | R03 labeled fan/overlay commands exported; semantic pair match not completed | Texture lineage and dependencies UNKNOWN | UNKNOWN |
| Postprocess | R03 labeled SSAO/Bloom/Distortion/DOF commands exported; MVK3 pass mapping incomplete | Complete attachment/load/store/resolve and last-writer comparison UNKNOWN | UNKNOWN |
| UI | R03 labeled panel/screen/health-bar commands exported; full semantic pair match incomplete | Input/time/state equivalence unproved | UNKNOWN |

R03 all56 encoder child lists and both complete API-call listings were exported. MVK3 command-tree expansion is partial through cb3/re142: cb3/be143 navigation returned nonzero with no stderr, and its raw stdout was not retained. No retry was made; cb3 tail/cb5 expansion remains missing. Later targeted world/decal inspection succeeded. See `EXPORT-LIMITS.json`. No result is inferred from encoder ordinal.

## Execution and cleanup

| Arm | Games | Capture commands / frames | Inspection sessions | Profile commands | End state |
|---|---:|---:|---:|---:|---|
| R03 | 1 | 1 / 1 | 1 (session9) | 0 | Normal exit; session closed |
| MVK3 | 1 | 1 / 1 | 1 (session14) | 0 | Normal exit; session closed |

Builds0, engine-source edits0, retries0, extra counter replays0, R04 captures0, FPS A/B0. No reboot, logout, service restart or security change. Existing `bar` automation remains PAUSED.

Both game process groups, capture workers, owned capture clients, inspection sessions/replayers and owned awake processes were released. Final tool response: **No active sessions.** Existing unrelated `caffeinate -d` was left untouched. `CLEANUP-FINAL.json` is PASS_ALL_OWNED_RESOURCES_CLOSED. R03's inspection session stayed idle while the second game ran so it could be reused if the pair passed; no FPS or clean-timing claim is made from these diagnostic runs.

## Fixed inputs and reproducibility

R03 source `0780bd7c530b0696067b717428b49a416eda556a`; source manifest SHA256 `4f0878847fd57a1616ed2bb3555424d469971d4fe2a5a232817faea3bef39deb`.
R03 binary SHA256 `107958872e5d962e4d27803a0fb1af0e842b18bd6982bd9b74e115d956890ab9`.
MVK3 binary SHA256 `d715965659b977e05b8ccafe76fae0936cade1a7481871acff7eec876366f1aa`.

The only controller change from round25 moves boundary enumeration from1500 to1200; the wrapper additionally preserves elapsed/partial query output. Controller SHA256 `b4f7330162039f5e9cb1508e23d60fcf98a0a874e99290100ac0d34e2923138a`. `FREEZE.json`, the controller/wrapper diffs and actual-child environment receipts are preserved. Both engine pins,23 runtime-library pins, controller and wrapper were rehashed unchanged after cleanup (`POSTRUN-PINS.json`). Existing source and previous-round evidence were not modified. `UPSTREAM-CONTRIBUTIONS.json` and captured source attribution are preserved.

The original traces remain in this evidence root. `TRACE-MANIFEST.json` records every file hash. The published evidence archive contains all exported analysis, selected run receipts/logs, frozen setup and reports, excluding the large traces and private runtime/content copies. `ARTIFACT-MANIFEST.json` maps every included file to SHA256 and size.

## Handoff

Return this closed attempt to SUPER_JINI_MK2 for review. The mandatory workload gate failed, so no component bottleneck ranking or frame-time estimate is available from this pair. Any future capture needs a new bounded round and a defined way to freeze or validate unsynced decal/particle input; no such execution or source repair is authorized by this report. R03 remains the retained candidate; this R03/MVK3 diagnostic establishes no R04 cause and makes no release/adoption decision.
