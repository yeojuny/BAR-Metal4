# GroundDecal paged publication candidate (source-only)

Base: `0780bd7c530b0696067b717428b49a416eda556a` (R03).
This directory's staging Git baseline contains only the five exact upstream files touched. It is not a full checkout or a publishable descendant commit. Publish the seven manifest-listed source/test files on a branch whose real parent is the base commit. No remote branch, STATE, HQ, Mac, game, benchmark, main, release, or Homebrew action was taken here.

## Task status
- [x] Inspect original capture, dirty flags, compaction, and lease ownership.
- [x] Replace full CPU instance-vector copies with immutable 64-row copy-on-write pages.
- [x] Preserve non-dirty bytes exactly: clone the old dirty page, then apply only declared dirty ranges.
- [x] Compare target pages to each GPU slot's actual source, handling skipped and reversed revisions without revision arithmetic/history chains.
- [x] Preserve pinned GPU versions; two retained slots, extra concurrent generations transient.
- [x] Pin mutable slot in both prepared packet and RuntimeLease opaque lifetime wrapper.
- [x] Cover context changes, empty arrays, append, compaction, failed copies, malformed input and old version requests.
- [x] CPU optimized and AddressSanitizer/UndefinedBehaviorSanitizer tests, independent source review.
- [ ] Publish/fix real remote source commit and manifest (parent coordinator).
- [ ] Full engine/Metal SDK compilation, GPU residency/lifetime validation, pixel parity, matched performance measurement.

## Implemented change, not performance claim
Unchanged pages are shared by CPU snapshots. Dirty pages clone at most 64 rows per page, then apply dirty bytes. GPU copies only pages whose immutable identity differs from the sole-owned selected slot. Count changes rebuild all pages; compaction remains governed by the original all-dirty notification. The original shaders, draw order, G01 indexed/fallback behavior, atlas logic, submission retirement and worker concurrency are unchanged.

A 64-row page is 12,288 bytes for the production 192-byte instance; this is an explicit initial granularity, not a tuned optimum. Per-snapshot page pointer arrays and mutable-build bookkeeping still allocate/copy metadata. A large dirty set still copies the full payload. Size changes, context changes, and new/transient slots still require full initialization. Pool capacity is two retained slots, not a new GPU-inflight limit. Neither FPS nor CPU/GPU time improvement has been measured. This is not a claim of identical MVK internals.

## Validation
Commands run from this directory:
```
c++ -std=c++20 -O2 -Wall -Wextra -Werror -pedantic -Iengine/rts engine/tools/metal4-contracts/TestGroundDecalPages.cpp -o evidence/test-release
./evidence/test-release
c++ -std=c++20 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g -Iengine/rts engine/tools/metal4-contracts/TestGroundDecalPages.cpp -o evidence/test-ground-decal-pages
ASAN_OPTIONS=detect_leaks=0 ./evidence/test-ground-decal-pages
git diff --check
```
Both runs: `PASS GroundDecalPages checks=10680 randomized_revisions=2000`.
GCC: Debian 14.2.0-19. LeakSanitizer is disabled because this execution environment reports unsupported ptrace interaction; ASan and UBSan remain enabled. An initial allocation-exception sanitizer failure in the conditional aggregate construction was removed by completing allocation before constructing the slot; final runs above pass.

Supplementary `NativeGroundDecalData.cpp` syntax check passed using staged headers plus local supporting engine headers. A warnings-as-errors attempt flags pre-existing sign-compare/misleading-indentation warnings outside the diff; no unrelated warning fixes were made. `NativeGroundDecalCapture.cpp` syntax compilation is blocked by missing generated/dependency `glad/glad.h`. The C++ template used by capture is compiled and tested directly. No Metal SDK is available; Objective-C++ integration is source-reviewed, not compiled.

Independent review found no source-level blocker after correcting dirty-page semantics and including `<utility>`. Reviewer re-ran the optimized test binary and confirmed the final cases and diff check. Actual GPU completion, failed-submission quarantine, resource residency, and rendered equality require HQ verification later.

## Correctness test coverage
10,680 checks over 2,000 randomized capture revisions plus fixed cases: exact reuse, one-page delta/no allocation, skipped changes on separate pages, old snapshots, all slots pinned/transient allocation, lease-only pin after packet release, Clear with live pins, context split, copy failure invalidation and full repair, allocation failure, dirty range rejection, missing page, revision gaps, dirty/non-dirty isolation, page-boundary crossing, tail page, growth, compaction/reorder, reset, empty and reappearance. Each mock GPU buffer is compared to immutable CPU expected bytes; historical pinned buffers are rechecked after subsequent operations.
