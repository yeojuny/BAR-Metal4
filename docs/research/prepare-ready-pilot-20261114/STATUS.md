# Default-off preparation-ready pilot

Base: 1eec27a1a71771ddb8c38c465aebac9bc37be702.

## Implemented
- [x] New `BAR_NATIVE_PREPARE_READY_PILOT=1` opt-in. Default remains off; existing Begin-ready behavior remains unchanged unless this separate flag is set.
- [x] Owned admitted normal frames can return after worker enqueue, before Prepare/Begin. Render worker must also be enabled. Readback, continuation, water families and arbitrary source callbacks stay excluded.
- [x] Preserve existing preparation-over-previous-GPU path. Do not force prior GPU drain merely to permit this pilot. Previous-retirement worker callback only publishes an atomic notice; main applies it after Join.
- [x] Three source generations only in opt-in mode: older GPU-held, current worker, younger capture. GPU submission depth remains unchanged.
- [x] Actual Begin target notice remains exact and is delivered after Join, not fabricated on early return.
- [x] An early-return frame that fails or skips presentation before Begin quarantines its dependent lineage instead of restoring old journals over younger capture.
- [x] Existing unchanged-surface capture-prefix boundary remains; drain still occurs before the later alpha/water/Lua suffix and mutable renderer controls. This is not unrestricted full-frame parallel execution.

## Host validation
`run_native_prepare_ready_pilot_test.py` passes:
- 5,000 default-two/opt-in-three source-credit/lineage cycles.
- 1,000 job-settlement cycles plus controlled early-release success/failure tests where main prefix advances before worker completion and notices remain deferred.
- 100 extracted production Scene admission records plus no-Begin failure/skip tests preserving younger pending state.
- 100 existing Begin-ready worker cycles and eligibility regression.
- Source-order guards for main-only predecessor delivery, exact older receipt retirement and no eager GPU drain in prepare mode.

These tests use production C++ headers and an extracted Scene admission record with modeled engine adapters. They do **not** compile the complete Objective-C++ renderer or prove native timing/visual equivalence.

## Remaining and operational limits
- [ ] Mac/Metal compile.
- [ ] Controlled game run with pilot off/on, actual CPU overlap trace, frame timing and visual comparison.
- [ ] Any wider capture scope or default enablement depends on that evidence.

A no-Begin skip after early return is intentionally terminal/quarantined in this experiment, including a surface becoming unavailable after eligibility capture. This preserves the younger state instead of pretending recovery succeeded. It is not production-ready fallback behavior. No pilot was enabled or game started by this change. Existing main and app are preserved.
