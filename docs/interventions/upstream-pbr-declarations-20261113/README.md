# Capture-owned PBR declaration batches

The Lua/source service publishes copy-on-write batches of retained original PBR
images. World, readback and target-only requests capture an immutable batch at
their source boundary and carry it to pending-work predicates and GPU Prepare.
The render consumer no longer iterates a live source vector or prunes it after
GPU completion. Later enqueue cannot add a command to an earlier request.

Actual CreateShader compilation remains synchronous through the existing
RuntimeLease and actual Allocate reserves the original GPU texture. Errors are
not deferred or replaced by synthetic success. TexRect still allocates/publishes
and enqueues transactionally; an enqueue allocation failure preserves the prior
batch and the existing registry cancellation path handles its publication.
Original command order, duplicates, BRDF/Environment selection and image atomic
encoded/completed state are preserved. Completed images can still be excluded
at consumption after an earlier submission has retired.

Completed-entry compaction is now source/main-boundary work. It is best-effort:
allocation failure retains completed entries, which consumers still filter out.
Reset or shutdown drops the source batch but cannot invalidate captured batches.
This may retain completed images until the next source capture or teardown.
Copy-on-write adds a batch allocation/vector copy on a PBR source enqueue; no
performance improvement is claimed. This is a source-ownership prerequisite,
not a render worker, CPU lookahead implementation, or full U3 completion.

Host tests compile the production declaration store and actual service Prepare
body with GPU/texture storage stand-ins. They cover prior-frame isolation,
selection/order, duplicate preservation, allocation-failure atomicity,
prune-failure semantics, exact owner lifetime and concurrent atomic completion.
Compile/allocate hooks and all source-to-consumer wiring are guarded structurally.
No Metal library compilation, full application build, GPU output comparison or
FPS measurement has been performed for this change.

Public API compatibility: an unspecified `options.pbrDeclarations` is `nullopt`.
The main-only DrawDispatch wrapper captures it before dispatch; a supplied
engaged-empty receipt is never replaced by later source work. SubmitMaintenance
retains its legacy default via a capture at its synchronous main entry. Direct
SubmitScreenTargets explicitly captures at its entry. DrawPrepared contains no
source capture fallback. In-tree Draw/DrawPreparingWhilePending callers are the
normal and SaveImage SceneBridge paths; the maintenance callers are SceneBridge
and SubmitScreenTargets. Frozen docs overlays are historical, not live callers.

Lifetime audit: `Metal4PBRSource.mm` ImageStorage/Pipeline and the reflection
CubeStorage holders have default destruction with strong Objective-C object
releases, not destructor-side source mutations, command encoding or main/render
thread assertions. PreparedPBRSource::Draw retains the actual image, pipeline
and resolved reflection owner. RuntimeLease AddAllocation/Retain pins their
actual GPU allocations and library/pipeline; RetainCubeStorage separately pins
physical cube storage through retirement. Main compaction removes only entries
whose completed atomic was published by DidComplete. Existing ResetWorld and
Shutdown already release the same producer-owned declarations on main. The
change may move the last completed declaration release from retirement to a
later main capture, but does not remove prepared/submission resource pins.
This is a source-level ownership audit, not proof of arbitrary Objective-C/Metal
thread behavior or completed native runtime validation.

Validation: production helper + extracted actual Prepare/optional-wrapper code
passed 8,027 checks under ASan/UBSan. Full SceneBridge translation-unit host
syntax passed without stand-in headers. Independent review accepted ownership,
frame cutoff and public-wrapper compatibility. Environment-stage (2,012 checks)
and presentation-policy (4,504 checks) regression suites also passed.
