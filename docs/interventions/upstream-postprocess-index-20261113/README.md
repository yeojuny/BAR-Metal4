# P1-B first slice: exact-root postprocess order and producer index

Baseline `3232f5e9c0c68a7876f2c4e766c462ee77963b1e`, same isolated upstream branch. The handoff's P1-B is independent of terrain lazy storage. T3's pending Mac/Metal gates are not bypassed.

## Production change

`NativePostprocessPlan` privately builds one ordered-command vector and a scalar image-object/generation to first-producer reference map. It retains the exact immutable frame root, not additional image/version/backing owners. Root matching compares both pointer and shared ownership, never just serial or frame number. Prefix/suffix snapshots with equal serial but distinct roots cannot share a plan.

`PostprocessGPU::PrepareFrameImages` builds/reuses the active root's plan, still validates runtime availability on every call, iterates its order for image preparation and installs it only after that preparation succeeds. Initialize transfers and ClearResources clears the plan with the previous declared frame. ResolveImage uses indexed producer lookup while retaining CPU-content and current retirement/lease checks.

This is a single active-root cache. Switching roots can rebuild an earlier discarded root; there is no unbounded global cache or claim of one build for a snapshot's entire lifetime. For the active exact root, repeated preparations reuse its ordered vector and consumer lookup does not sort the graph.

## Preserved boundaries

Full structural validation occurs at plan construction at the existing first preparation boundary. Frame extent/serial checks precede it. Duplicate version rejection remains in runtime frame traversal after the same command's input callbacks; the index retains the first producer just like legacy lookup. World stage/sequence, screen offset and same-offset commandSequence comparisons remain. Runtime availability, isRetired, PreparationInput, output registration and ImageFor execute anew; no readiness result or lease is cached.

Actual shaders, commands, order, clears/preserve/copies, barrier logic and texture storage reuse proof are unchanged. The original frame-taking validation APIs remain for other callers and differential tests. Published source roots must remain immutable, as already required by their snapshot contract.

## Scope remaining

This does not complete P1-B's static overwrite-proof caching, cross-root shared indexing, exact-lease preparation deduplication, or later async ownership work. Existing overwrite proof still runs at preparation. It makes no GPU overlap, frame-rate or visual-parity claim.

The actual backend translation unit is compiled on the cloud. Full Objective-C++ Metal integration compile/runtime, rendering parity and performance remain unverified until the user-selected Mac is available; no other environment was accessed.

Verification: 38 checks passed with ASan/UBSan, compiling and linking complete actual NativePostprocessData.cpp, NativeCUSUniformData.cpp and ScreenDrawList.cpp with section garbage collection and no algorithm stubs. Cases include legacy/new ordering and error parity, exact-owner and same-serial-root separation, unchanged image/version reference counts, first producer and duplicate error/callback ordering, and 100 repeated retirement callbacks. Source inspection confirms indexed consumer lookup has no full-order call; that is not renderer timing/invocation measurement. Independent backend/integration review found no blocking issue.

Run `python3 engine/tools/metal4-contracts/run_native_postprocess_plan_test.py --output /tmp/postprocess-plan-tests`. Receipt records compiler, checks and source hashes. Existing-style warning exclusions and sandbox LeakSanitizer limitation are explicit; ASan/UBSan remain enabled.
