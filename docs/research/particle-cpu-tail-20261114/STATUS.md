# Particle CPU tail before GPU retirement

Base: `1bee26a9d5998c3199d1f88e08791b5ea046813d`. Isolated candidate, not a runtime performance result.

## Scope

The existing default-off Begin/Prepare-ready modes defer main particle capture. The normal capture callback now joins and settles the previous renderer CPU job before completing that particle capture, while the previous GPU submission may remain pending. The existing surface-boundary full GPU drain still occurs immediately afterward, before shield publication, alpha, water, or Lua/provider callbacks. OFF behavior is unchanged.

No new flag, queue depth, resource pool, or source-owner generalization. No removal of the resize, exceptional capture, runtime-control, or teardown GPU drains. GPU ownership remains in the existing pending submission until its normal retirement. CPU settlement does not imply GPU completion.

## Consumer evidence

- `WorldFrameCapture::CompleteDeferredMainParticles` checks original draw/camera and fills the fresh frame exactly once.
- `NativeWorldParticleCapture::Capture` creates a new particle frame; geometry uses CPU-only `TypedRenderBuffer` arrays. Model geometry is copied/cached CPU data. Tracer direct GL lines and flying-piece geometry are excluded by existing unsupported branches.
- `NativeS3OTextureSource::CapturePair` retains registry objects and checks immutable texture source data; it does not upload or submit.
- `CTextureAtlas::GetNativeAtlas` returns the retained source. Nano `CaptureNative` rebuilds CPU vectors and copies instance/template values; `SyncPersistentBuffer` here does not upload a GPU buffer.
- Source fog/geometry scratch writers remain on main after the renderer CPU job settles. They do not alter the older submission's retained GPU resources.

The broader particle/alpha/water/provider chain is deliberately not opened. Arbitrary callback/resource effects in later capture are outside this change.

## Failure and ordering

The particle tail stays inside the existing capture exception fence. CPU settlement or particle failure therefore attempts the original full drain before caller rollback; failed drain retains the existing quarantine failure. Changed/unknown surfaces drain before Refresh as before. The helper still drains after capture for all pilot paths.

## Validation

- Surface boundary host test, ASan/UBSan: PASS. Includes CPU-settled/GPU-still-pending particle ordering and particle/settlement exception fences, plus original 1000 observation checks.
- Surface runner corrected an obsolete source assertion that omitted the existing Prepare-ready exception in the Begin-ready refresh guard.
- P5 source-contract suite: PASS (source credits, CPU settlement, scene admission, Begin-ready wiring).
- Mac compile: NOT RUN for this candidate.
- Game, GPU overlap, performance and image parity: NOT RUN. Potential overlap is not a demonstrated speedup.

## Integration

This is based on remote `1bee26a`, not the offline Mac diagnostics commit `ac4b9f1`. Both touch SceneBridge, but diagnostics adds tracing at the boundary and this candidate adds a particle tail inside its capture callback. Reconcile against the actual diagnostics file after Mac reconnect; do not overwrite that committed work. Other diagnostics files are untouched. Keep this candidate separate until integration review.

## Integrated verification

Diagnostics were recovered as `2e8423d3f427943d58cbfc78e5c639ef72501bfd` (tree identical to the previously offline Mac commit). The particle candidate cherry-picked without conflict. The kind-8 prefix trace scope was then closed before CPU Join/particle capture to retain its original meaning; kind-9/10 drain timings do not include this explicit earlier Join. Use parent/worker spans for total timing, not kind-9 alone.

Integrated surface ASan/UBSan, P5 source-contract and CPU trace sampled/loop/off suites passed. All four changed translation units passed Mac syntax checks. A separate cloned build recompiled all 22 translation units selected by Ninja's recorded dependencies on the changed production sources/headers, then linked the complete executable successfully. This is an incremental build, not a clean rebuild. Original build/app preserved.

Binary SHA-256: `9ee2b49a30003fe4f68faa1239f1fc80cb9f3591ae2edb8167874bf8aa089cca`. No game execution, performance comparison or visual acceptance was performed. Local build receipt retains full commands/log location; no raw local paths are published here.
