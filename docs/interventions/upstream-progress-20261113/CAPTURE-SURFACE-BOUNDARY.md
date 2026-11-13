# Bounded main snapshot beyond the unchanged surface boundary

The default-off Begin-ready pilot previously drained N before the unconditional
PrepareWorld Refresh, even when no surface property changed. A main-thread
read-only observation now compares every logical/backing/visibility field, actual
CAMetalLayer drawable dimensions, and requested/applied display-sync mode. Unknown
or changed observations still drain before the original Refresh, which reobserves
after the drain rather than applying a stale plan. Existing window/control drains
and DrawWorld's second surface refresh are unchanged. No layer setter, CPU surface
revision or VSync request is modified on the unchanged path.

Only WorldFrameCapture::Capture(Main)'s terrain/material/model snapshot prefix
moves before that drain. It returns a fresh value frame with retained immutable
geometry/material snapshots. Cache misses may parse material metadata through
LuaParser; this is not replaying DrawWorld callbacks. Particle capture is explicitly
deferred: its original flash/particle Draw, fog and nano/geometry scratch writers
run ONCE after the drain, before all later alpha/water/Lua draw callbacks. Default
Capture callers retain the original inline particle tail. No source writer is
skipped or rerun. Deferred completion checks the original draw, simulation frame,
interpolation, view/projection/position, viewport and clip-depth identity. Missing,
repeated, reentrant or failed completion is rejected; both owned-packet construction
and direct DrawDispatch reject a partially completed frame. Failed writers cannot
be retried and replay their partial side effects.

The boundary always drains before particle completion, shield model publication,
alpha callbacks, water capture, Lua DrawWorld or subsequent source writers.
The later EARLY-CPU-READ-LAUNCH milestone permits only the immutable opaque
CPURead job to start after the snapshot prefix and before this drain; successful
receipt ownership can retain that CPU-only job through later source capture.
It does not move those source writers ahead of the renderer drain. N+1 selecting another water mode therefore does not inherit N's
eligibility as permission to overlap that mode's capture.

A throwing Capture drains before the outer existing abort can restore source
publications. If this drain itself fails, its error propagates and the old worker's
failure/quarantine guards remain authoritative; no fallback refresh or rollback
is attempted by the boundary. Successful old acceptance targets its retained
source transaction, not N+1's newly prepared frame. The ordinary non-pilot path
retains refresh then capture without introducing a new drain.

Validation: actual portable production helper and exact-observation predicate,
1,000 cycles of changed dimensions/visibility/sync, invalid drawable, unknown
observation, capture and drain failures, and reobservation ordering. ASan/UBSan.
Exactly-once particle-state failure/reentrancy tests also pass.
Structural checks constrain the real SceneBridge lambda to the deferred Capture(Main) prefix, preserve
the later refresh drain, and verify native observation has no writes. Existing
surface event/config/close guards remain covered by their regression. This does
not execute SDL/AppKit/Metal or prove visual parity, native thread safety, positive
active overlap, retained-byte cost or performance. Native API/event stress and
paired timing validation remain required before enabling the pilot by default.

Full alpha/water/Lua overlap and renderer preparation overlap are NOT enabled.
No full app build, app run, barrier change or GPU queue-depth change is included.

Independent review: GO for this narrow source scope after moving particle writers
behind the drain. SceneBridge.cpp and WorldFrameCapture.cpp full translation units
pass Linux host syntax with actual SDL3 headers, not stubs; this is not ObjC++ or
Metal compilation. Exact scene-admission and queued-result regressions pass. The
owned-world runner's pre-existing stale global TransferValidated position check
also failed on unmodified HEAD; it now compares the actual DrawWorld transfer call
site after sealing, retaining the same invariant rather than changing production.
Its actual packet fixture now rejects both pending and failed particle completion.
