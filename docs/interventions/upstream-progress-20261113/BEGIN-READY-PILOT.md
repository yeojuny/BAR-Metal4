# Default-off Begin-ready encoding-tail pilot

`BAR_NATIVE_BEGIN_READY_PILOT=1` requests the restricted source pilot. The existing
render-worker flag must also be enabled. Default execution still joins.

Actual source wiring now can return a queued result after the worker has opened
its real command Begin, installed active-source ownership and published its exact
target notice. This is not merely an Enqueue wrapper: Game may advance copied CPU
render buffers and begin the next source capture while the previous worker still
encodes. No presentation/completion flag is fabricated. A worker already finished
at the signal takes the joined path instead.

Eligibility requires an owned normal-world packet, presentable surface, positive
simulation frame, held source draw/Begin/deferred mailbox, typed source credit,
owned screen/postprocess and exact source settlement callback. Readback,
continuation, dynamic/legacy water and arbitrary source callbacks are excluded.
The preceding GPU and main receipts are drained BEFORE this dispatch; the pilot
never supplies previousRetired overlap preparation. Actual Begin alone would not
prove this. MRT209's exclusive-writer optimization is disabled for requested
pilot packets, using existing copy-on-write paths.

The two-source budget begins before Game capture and persists through GPU/main
receipt retirement. Ready/Join/Accept do not release GPU-held source credit. The
worker reservation remains held after queued return. Later explicit CPU/control
boundaries Join, release reservation, apply exact notices and scene acceptance,
report from the retained frame/UI, then Consume/reset the detached CPU job. No
current younger pending graph is reset for the old result.

Pre-Begin failures are joined/restored before returning. After queued return,
failures quarantine the exact old source and keep the job terminal. A runtime
nondeferred result (including synchronous completion) or wrong draw is explicitly
quarantined, never silently discarded or restored over the younger capture.
This matters because actual defer also depends on AllTargetsEncoded at runtime.

Source validation:
- Actual policy's15 gates and100 real worker/job/source-credit handshake cycles.
- Pre-Begin fallback and100 extracted actual scene-record cycles, including
  queued-to-sync/wrong-draw quarantine with held credits retained.
- Job/dispatch/queued-result regressions and SceneBridge host syntax.
- Independent review and separate sanitizer binaries: GO for default-off pilot.

Not yet established: Objective-C++/Metal native compilation, rendered equality,
positive active-work overlap, throughput or latency improvement. Promise-blocked
portable tests establish ownership ordering, not performance. This is encoding-
tail overlap only, not general capture versus renderer preparation overlap or a
claim that the entire handoff P5 acceptance suite is finished. Full app build/run
remains deferred under the user's instruction.

Follow-up surface audit: see `SURFACE-CONTROL-DRAIN.md`. Engine-owned window/layer
changes are explicitly drained, including ordinary Scene Refresh when the pilot
is requested. Possible capture overlap ends at that earlier refresh boundary;
it is not guaranteed to cover the entire N+1 capture.
