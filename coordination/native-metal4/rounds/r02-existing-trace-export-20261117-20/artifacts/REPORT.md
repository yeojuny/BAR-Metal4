# R02 existing-capture export: available evidence and hard limits

Round `r02-existing-trace-export-20261117-20`, attempt `r02-existing-trace-export-01`. This is CPU-only analysis of previously saved R02 capture exports, not a new runtime or replay profile. Source `0382d0f3833d63b3ff1a78c46b108c99539656d0`; later R03 and completion/texture candidates are not represented in this capture.

## Result

The 52 encoder nodes were mapped in creation order, with encoder type and all 42 render draw counts checked. Fourteen requested encoder labels (9–12, 33–42) have separate raw API slices. Explicit barriers, pipeline/depth-state bindings, recorded shader bounds and the final texture-copy edge were exported. Attachment descriptors, complete resource hazards, full dss12/dss15 state and individual active stage intervals remain **UNAVAILABLE** in the saved exports. They have not been substituted with source-based guesses.

No inspection session was opened. The installed gpudebug help provides trace/session/replay-device options but no documented zero-GPU-execution loading mode. A historical open reported `replayer.state=loading`; this does not prove opening submits GPU work, but it does not establish the guarantee required by this round. Both session-list checks returned `No active sessions.` Games, builds, captures, GPU replay and profiling were all zero.

The original trace was verified before and after: 1,223 files, 2,788,204,457 logical bytes, tree SHA256 `5ae339d460ddb46089783227ba115d37ae4a0e3c4327d81e99c665741a8ea228`. Definition: SHA256 over sorted UTF8 relativePath + NUL + fileSHA256 + newline. All nine copied raw exports also match their originals.

## Encoder namespaces and association

| Saved label number | Command node | Performance node | API rows | Draws |
|---|---|---|---|---|
| 9 | re13 | re8 | 5453–8361 | 233 |
| 10 | re14 | re9 | 8362–8669 | 8 |
| 11 | re15 | re10 | 8670–8846 | 58 |
| 12 | re16 | re11 | 8847–9021 | 58 |
| 33 | re41 | re32 | 9715–10172 | 13 |
| 34 | re42 | re33 | 10173–10192 | 1 |
| 35 | re43 | re34 | 10193–10224 | 1 |
| 36 | re44 | re35 | 10225–10256 | 1 |
| 37 | re45 | re36 | 10257–10278 | 1 |
| 38 | re46 | re37 | 10279–10303 | 1 |
| 39 | re47 | re38 | 10304–10306 | 0 |
| 40 | re48 | re39 | 10307–10324 | 1 |
| 41 | re49 | re40 | 10325–10956 | 78 |
| 42 | re50 | re41 | 10957–10979 | 1 |

Command/performance/timeline records join by exact encoder label. API segments are associated by creation order, encoder type and matching draw counts; the listing does not expose an encoder receiver handle on every API row. See `ENCODER-ID-MAP.json` and `encoder-api/`.

## Observations useful for the next decision

1. **The decal→grass boundary is already narrowed.** Label9 exits with Fragment→queue Fragment; label10 enters with queue Fragment→Fragment. This explicit wait does not include Vertex. A recommendation to split these passes or simply remove a Vertex wait here would repeat an already implemented change. This is a statement about recorded calls, not proof of all hidden hazards or measured overlap duration.
2. **Other broad waits are present, but necessity is unproved.** Labels11/12 and33 onward include Fragment/Blit/Dispatch→Vertex|Fragment masks. `BARRIERS.json` decodes them against the installed official SDK stage constants. Without attachment bindings and resource last writers, a broad mask alone does not establish that narrowing it is safe.
3. **SensorLOS remains a large historical replay target.** Label33 binds dss15 at api9754 and draws rps61 at api9766 (378 vertices ×165 instances). Its subsequent dss12 binding at api9776 applies to rps15 at api9786, not another assumed SensorLOS draw. Full depth/stencil comparison, writes, masks and operations are unavailable. The historical relative shader-cost shares were SensorLOS26.34215%, GroundDecal14.74173%, GameDecal8.59926%; these are not runtime frame fractions or predicted savings.
4. **A final copy edge is explicit.** api10982 in compute node ce51 copies tex99→tex2, followed by command completion, queue commit and drawable signal/present calls. The upstream render writer of tex99 is unavailable because attachment descriptors were omitted. The sequencing supports a presentation association but does not reveal exact drawable texture identity. Residency registration and `isAliasable` queries do not prove reads, writes or aliasing. Numeric argument-table texture IDs are not assumed to equal trace @tex ordinals.
5. **Saved timeline bounds are insufficient to measure continuous overlap.** Shader rows contain one bounds pair and an undocumented trailing field. These are not the individual constituent active intervals. Unit/column schema was not retained, so raw duration values are preserved unscaled. A zero-draw label39 has a whole-frame-looking range; interpreting that range as GPU occupation would be wrong. Bounds intersections are not summed or reported as measured overlap.

## What is available

- `raw/saved-export/`: exact copies of all nine original JSONL exports; `SAVED-EXPORT-INDEX.json` records provenance and hashes. The full response is selected rather than the truncated first page.
- `ENCODER-ID-MAP.json`, `encoder-api/`, `DRAW-PIPELINE-DSS.json`: namespace map, requested raw call slices and explicit draw bindings.
- `SDK-STAGE-MASKS.json`, `BARRIERS.json`: verified mask constants and raw/decoded barriers.
- `RECORDED-STAGE-RANGES.json`, `RECORDED-ENCODER-RANGES.json`: old replay bounds with interpretation limits.
- `TEXTURE-COPY-LINEAGE.json`, `COMMAND-RESOURCE-TIMELINE.json`: explicit copy/view commands and binding/call sequence, not a complete resource hazard graph.
- `DEPTH-STENCIL-12-15.json`, `AVAILABILITY.json`: available bindings and missing descriptor/state fields.
- `TRACE-PIN-BEFORE.json`, `TRACE-PIN-AFTER.json`, `TRACE-FILES.json`, `DERIVED-EXPORT-CHECK.json`, `COMMANDS.json`, `CLEANUP.json`: integrity, processing and session receipts. The 2.79GB trace itself remains local.

## Consequence

This round improves the pass/API map and prevents duplicate work, but does not close the attachment/last-writer proof needed for new barrier or in-place-write changes. Missing fields require a separately authorized supported inspection path if one can be established; they are not permission to replay now. No source repair, timing estimate or new performance claim follows from this export.

A separate user-requested Astra ultra follow-up audit has started in thread `01a1e87e-0748-7293-9316-f5c1bfed92bc`. It may read these files and current fixed sources, writes only its own audit document, and has no game/build/GPU execution or repository mutation authority.
