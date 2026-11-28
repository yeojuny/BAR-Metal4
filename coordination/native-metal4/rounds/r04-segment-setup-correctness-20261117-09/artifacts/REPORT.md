# R04 segment setup — fixed standalone GPU correctness

PASS: one helper compile and one GPU invocation, actual Metal API Validation enabled, no matched errors. Candidate/R03-reference/disabled-prepared MSL libraries ran inside that one process invocation.

-12065 old/new color/depth/stencil comparisons byte-identical.
-Existing3600 cases retained:2234 optimized,1869 visible,427 depth,29 clipped,12 seam,1199X-major,629Y-major.
-Existing1024 asymmetric cases retained:96 negative-stencil and24 negative-facing witnesses; X48/Y24/clipped24,512 guard fallbacks.
-New prepared-path negative control detected actual attachment difference. The helper exercised prepared coverage and did not only fall back.

Compile exited0; GPU exited0 in31.857349s. Actual binary SHA25676cd2e9c098867d384a064328fec1802baa5b386fcc0c5e9fcdf1583eca7baeb. Manifest/helper/header and all8 source-packet files plus3 fixed production headers were hash verified. No source edit, assertion change or retry.

The synthetic helper vertex populates two flat int4 fields. This is not exhaustive production vertex/height/clipping verification. True AA4 and large-coordinate GPU parity remain untested. CPU source results were preserved and verified as inputs, not rerun. No engine/game/FPS or performance result:32-byte extra varying and per-vertex setup costs must be measured later.

All owned processes/children reaped, helper GPU completion waited before readback, no awake keeper/extra workers created. Original app/main/release/Homebrew untouched. Existing source/provenance text retained; HQ adds execution evidence only.

JINI relayed the user view that the prior orange circle was a range indicator and likely ignorable. It is recorded as user interpretation, not independently proved cause, and is not used to block/retry or modify rendering. Future benchmark conditions remain matched.

Raw compile/GPU logs, exact fixed inputs and result are included. JINI reviews this bounded correctness result before any separate engine or performance stage.
