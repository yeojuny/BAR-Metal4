# Capture-owned pre-PreUnit environment receipt (bounded U3 prerequisite)

`CaptureEnvironmentInputs` now returns a receipt and does not write renderer
active-frame fields. It retains the same main/pinned-worker, renderer and non-null
stage checks. SceneBridge stores the receipt at the original pre-PreUnit call and
copies it into both ordinary and SaveImage render options. It retains the exact
stage pointer and the original capture-time shader-validation policy, which is
not replaced by the later draw options flag.

`DrawPrepared` uses that receipt at the existing early source-work and reflection
preparation sites. Prepared metal/world-info owners are local to the request;
world-info completion captures the corresponding local owner. The existing late
identity, missing-frame, terrain identity and completed-metal guards remain at
the same consumer sites. Main no longer resets render preparation resources when
publishing the next pre-PreUnit stage. `HasPendingEnvironmentSource` receives the
explicit receipt while preserving the existing PBR check and acquire-load of cube
completion. Completed cube source still counts as source work in DrawPrepared,
matching the original distinction between source existence and pending work.

ResetWorld/Shutdown clear the scene receipt after the drained renderer reset.
Ordinary abort behavior is unchanged: the stage stays retained until replaced or
reset. The receipt itself is immutable input by convention; backing GPU completion
is still an atomic observation. This is not a certificate that all source/backing
objects are safe for concurrent access.

Validation: actual production receipt header, 2,012 ASan/UBSan checks covering
retained old stage, replacement/clearing of current capture, shader-policy
isolation, selected identity mismatch, worker consumption, and pending/completed
cube predicates. Structural checks cover both SceneBridge paths, reset sites,
original validation guards, early pin boundary and completion ownership.

Limits: no full SceneBridge/Objective-C++ compilation, Metal execution, GPU parity
or FPS validation. PBR/shield capture staging, global resource services and other
live renderer inputs remain unresolved. No async queue or CPU lookahead is enabled.
No full application build/restart or Mac access was performed.

Run `python engine/tools/metal4-contracts/run_native_environment_stage_test.py`.
