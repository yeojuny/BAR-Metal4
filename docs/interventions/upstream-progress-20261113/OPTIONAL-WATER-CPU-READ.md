# Optional sealed water-model CPU reads (P5 source checkpoint)

The post-main-capture boundary is NOT removed. Transitive audit found these live
writers, which remain in original main-thread order after drain:

| Capture | Live dependency / writer |
|---|---|
| Deferred particles | particle capture and model/source bookkeeping; exact draw/camera validation |
| Alpha models | recorder world stage/viewport, engine hooks, legacy materials, original CaptureAlpha callbacks |
| Water pre/post | original DrawWorldPreParticles / DrawWaterPost Lua event callbacks and recorder state |
| Water views | original camera/clip/fog/texture-history scopes, opaque/alpha capture, particles and reflection/refraction callbacks |

The safe extension uses only owners already sealed by those captures. Existing
opt-in CPURead mode2/3 worker can now prewarm model read rows from water view
opaque models and alphaModels[0]. It appends them AFTER the old mandatory
opaque → main alpha → shadow prefix. Water view order is actual renderer order:
refraction → reflection. No groundCUS, water Lua/effect bundles, callbacks, capture
reruns, extra visibility scan, GPU work or drain deletion is introduced. Default
mode0 and early synchronous mode1 keep their original source set.

## Ownership and failure rules
- [x] Retain exact waterScene, view, model and CUS source owners plus draw frame.
- [x] Match those identities and source order after worker Join before adoption.
- [x] Keep the existing4096-entry/32MiB budget and mandatory-prefix priority.
- [x] Optional setup failure leaves the mandatory tail available.
- [x] Optional Prime failure sets optionalFailed and does NOT seal a partial packet.
- [x] Reject that receipt, then use original main prefix priming and original
  water demand validation. Do not promote a new speculative error to job.error.
- [x] Mandatory prefix errors retain their existing consume-boundary delivery.
- [x] Mode3 successful optional work compares the same roster. Optional reference
  failure remains unverified, retaining usable main rows without early rethrow.
- [x] Diagnostics append optional_sources and optional_failed. Accepted identity
  is not identical to verified equivalence; no performance claim is made.
- [ ] Native Metal/compiler/runtime parity and equal-work performance measurement.

Prewarming intentionally changes cache residency/work placement. This is not a
claim of identical allocation timing or measured speedup. Existing strict source
checks and renderer demand validation remain. P5 overall remains incomplete and
render-worker/CPURead opt-in defaults are unchanged.

## Verification
- Actual optional worker VisitSources helper:1,000 publication/cancel/default/
  mandatory-error/optional-fallback cycles, ordering and source lifetime,
  C++23/-Werror/ASan/UBSan.
- Actual extracted SceneBridge receipt/launch/tail plus real CPURead production:
  existing100 launch/owner/error cycles, existing capacity/cancel cases and new100
  water owner/view/model/frame/list rejection + same-row comparisons.
- New checks require renderer refraction/reflection priority and alpha[0] only.
- Begin-ready15 eligibility gates /100 worker cycles pass.
- Full SceneBridge C++ translation-unit syntax check with actual SDL3 headers,
  RECOIL_USE_SDL3/RECOIL_METAL4_LIVE_SCENE/SYNCCHECK/STREFLOP_SSE and format warnings.
  This is not Objective-C++ compilation or a full app build.
- Independent review caught water priority and unused alpha-slot overcollection;
  both were corrected before final checks. No further blocker found.

LSan disabled due executor ptrace limitation; ASan/UBSan remain enabled. The original
application, main branch and unrelated local changes were preserved.
