# Central CPU job settlement before controls

Source checkpoint, not P5 enablement or native performance evidence.

The actual owned `DrawDispatch` now uses a persistent renderer-owned task
settlement rather than a local `Invoke` result. It still immediately joins.
The result survives intermediate `FinishCPUWork` calls until its scene caller
consumes it; no GPU completion or presentation is inferred from CPU completion.

Order: CPU task Join (including callable destruction and worker credit release),
reservation release, exact target notice, prior-retirement notification, then
GPU retirement/control. The renderer's source control APIs, Scene GPU drain,
PBR compiler's stable Impl callback, and lifecycle teardown explicitly settle
before entering runtime/cache locks. Worker/provider/completion scopes and
source-only capture do not implicitly join. Noexcept abort probes remain
conservative rather than acquiring a throwing drain.

The state remains Settling during callbacks and rejects recursive drain/control.
Render, target, and retirement failures preserve the first exception; each
callback is attempted at most once. Failed drains remain failed. Enqueue failure
releases its unstarted reservation and remains terminal. Discarding a running
settlement drains its task before releasing the reservation, does not deliver
success notices, and closes worker admission via the unobserved Task contract.

Validation:
- ASan/UBSan: 1,000 joined ordering/result/control cycles, recursive drain,
  pre/post callback errors, sticky failure, enqueue rejection, running discard.
- Independent full-diff review and separate sanitizer binary: GO.
- Owned task, owned dispatch receipt, allocation controls, seven compile-source
  controls, and ordered-control component regressions pass.
- SceneBridge host syntax and presentation result nothrow-move contract checked.
- No full app build, native Metal execution, visual parity, or FPS claim.

Remaining P5: exact scene acceptance/abort settlement after a queued return,
queued versus presented Game/SwapBuffers semantics, bounded source credits,
ReadyAfterBegin-only eligibility and MRT209 exclusion, capture/worker overlap
trace evidence. CPU-job settlement alone does not settle the scene admission
transaction or make a failed old frame safe to restore over a younger capture.
