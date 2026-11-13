# Frozen submission presentation policy (bounded U3 prerequisite)

The `DrawDispatch` source entry captures fullscreen/dual-screen mode and the safe
viewport by value before invoking the serial render worker. Both `DrawPrepared`
and `ContinueScreen` consume that value; they no longer query the live engine
presentation fields while preparing the GPU margin pass. The eligibility test,
unsigned extent conversion, offscreen/drawable conditions, margins pass and GPU
work remain unchanged. Public `Draw` callers need no new argument.

A SaveImage prefix does not retain the final presentation policy. Each later
suffix enters `DrawDispatch` and captures the then-current source policy, which
preserves the current serial callback/event ordering. Source completion callbacks
currently retire journals rather than pump window events. A future asynchronous
submission API must capture this policy on the capture owner and retain it in the
owned packet; the present stack lifetime is safe because `worker.Invoke` waits.
The existing thread/renderer validity checks run before reading source fields.
That thread predicate also accepts a pinned worker and is not a future strict
capture-owner assertion; current ordinary SceneBridge entry remains on main.

Validation: 4,504 ASan/UBSan host checks exercise the actual value-policy header,
all flag combinations, signed positions and original unsigned extent conversion,
copy isolation, worker consumption, and distinct continuation policy. A source
wiring guard checks that both dispatch routes pass the frozen value and that the
preparation/continuation methods no longer read these live fields.

This is not full U3, no thread-safety certificate for other inputs, no surface
revision ownership change and no measured speedup. Objective-C++/Metal execution,
window resize/fullscreen transitions and actual GPU parity remain unverified.
No full app build, restart, or Mac access was performed.

Run `python engine/tools/metal4-contracts/run_native_presentation_policy_test.py`.
