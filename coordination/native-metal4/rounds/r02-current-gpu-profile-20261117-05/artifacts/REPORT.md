# Current R02 GPU profile — one captured frame

Current replay still ranks OriginalSensorLOSFragment first (26.342153% relative replay cost), GroundDecalFragment second (14.741725%), GameDecalFragment third (8.599261%). These percentages are relative replay costs, not whole-frame milliseconds or predicted FPS. No old S01 profile was rerun or pooled.

| Target | Pipeline | Actual draw | Performance encoder | Command-tree encoder |
| --- | --- | --- | --- | --- |
| SensorLOS outline | rps61 | api9766:378 triangle vertices,165 instances =63 segments per instance | re32, label33 | re41, label33 |
| GroundDecal | rps71 | api5530:36 UInt16 indices,72bytes,32364 instances | re8, label9 | re13, label9 |

G01 is actually active: immutable cube-index label and indexed draw are present. SensorLOS and GroundDecal shader/pipeline mappings come directly from current exports. Node IDs belong to different namespaces; matching re numbers alone would be wrong. MAPPING.json preserves replay-local stage bounds, encoder cost rows and actual API calls. Bounds are not pure activity sums; overlapping intervals were never added.

One next source hypothesis: existing SensorLineCovered performs up to3 exact SensorDiamond calls before checking the exact minor pixel interval. Investigate performing the existing lower/count/actual rejection first for numerically proven input bounds, retaining original fallback otherwise. Keep the same integer coverage, width, depth/stencil, winding, mask-sampling and GPU resource contracts. This changes no desired covered pixels and is not a simpler line approximation. It is not implemented or proved here: additional early integer division can also cost time on formerly early-failing pixels, so gain is unknown. Test all source coverage/asymmetric-stencil cases and guard AA4 before adoption. No other hypothesis or source change is bundled.

One game, one dynamic Layer boundary12 discovered in that owned process, one captured frame, one new owned replay session120, one default/overlapping profile. Source10269 hashes, engine and19 libraries verified; script/input manifest frozen before launch. Actual Ultra AA0, viewport1512x950, drawable1512x982, sample1, sync off, PREPARE1/BEGIN0/worker1/S01/G01=1. Exact captured simulation frame UNKNOWN; existing controller requested at1590, tool launch delay0.029414s.

The diagnostic FPS parser asserted after capture; FPS is null and excluded, raw failure retained. The unchanged template independently accepted actual capture success and normal game exit. No retry or parser fix. No clean benchmark, build, helper or source change.

Capture trace:1223 files,2788204457 logical bytes, tree SHA2565ae339d460ddb46089783227ba115d37ae4a0e3c4327d81e99c665741a8ea228. Full trace remains local; TRACE-MANIFEST.json pins each file. All9 raw exports and capture/profile/cleanup receipts are published. Current instruction/register/fragment-invocation counters remain UNKNOWN; old S01 figures are not current evidence.

Game/capture/awake PIDs ended before replay. Owned session120 terminated. Immediate session-list output had an empty1-session header; later verification says no active sessions and status120 says no such session. Original receipts are preserved; no unrelated termination occurred. Replay awake PID ended. Original app/main/release/Homebrew unchanged.

JINI: review this evidence, then prepare one bounded source hypothesis if justified. This HQ history is heavy; use a fresh section for next execution with minimal pinned handoff if useful, and specify HQ model/reasoning effort at that task boundary. Current settings were kept per delegation.

This packet makes no new source authorship claim. Existing SensorLOS/ground decal and contributor provenance remains with the fixed source.
