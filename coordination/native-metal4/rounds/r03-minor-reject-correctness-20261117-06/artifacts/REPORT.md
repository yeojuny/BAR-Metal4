# R03 bounded minor-range rejection GPU gate

PASS: one fixed helper compile and one GPU correctness run, actual Metal API Validation enabled. Original and candidate fragment pipelines were separate. All12065 old-versus-new renders preserved color/depth/stencil byte-for-byte.

- Existing3600 cases:2234 optimized,1869 visible,427 depth,29 clipped,12 seam,1199X-major,629Y-major.
- Existing1024 asymmetric cases:96 negative-stencil witnesses,24 negative-facing witnesses; X48/Y24/clipped24, guardFallback512.
- R03 old/new pixel comparisons12065; no matched validation error, mismatch or failed assertion.

Compile1 exited0; GPU1 exited0 in30.334713s. Binary SHA256659e8e3dcbbd5ee0f2375b98df9380955944bcc42099f2dbeddce7d580daf998. Full fixed inputs and logs are included, with all hashes in RESULT and evidence index. No source edit, assertion weakening or retry.

This is a standalone small render fixture. Large coordinate guard boundaries remain CPU-only evidence from JINI. Actual MSAA4 was not run; AA4 guard checks use sampleCount1. No full engine/game/FPS/performance claim. Helper readbacks waitUntilCompleted; native runtime lifetime contracts were untouched.

Both owned processes were reaped and absent, GPU completed before readback/normal exit, no awake keeper or extra worker created. Original app/main/release/Homebrew unchanged.

JINI: review correctness and choose the next bounded stage. This HQ history is heavy; route the next execution to a fresh section with only state/round/attempt/source/manifest/results/prohibitions, and explicitly specify model/reasoning at the next task boundary. No settings change was made in this round.

Fixed source text and its existing copyright/contributor credits are preserved byte-for-byte; HQ adds execution evidence, not a new engine implementation authorship claim.
