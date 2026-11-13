# Exact cube cancellation prerequisite

AI-assisted source intervention. No app build, Mac execution, async enablement,
or runtime performance claim.

## Implemented

`NativeCubeTextureSource::CancelReflection(expected)` consumes a retained exact
publication, including the paired sky/main mip update. Registry cancellation
retains its existing context, identity, allocation and current-content guards.
The source's latest update is cleared only when all retained publication
identities and face metadata match. The no-argument compatibility call remains.
The renderer abort path copies the update it inspects and passes that same value;
its existing completed/encoded/dependency guards and active-submission guard
remain unchanged. This does not itself make concurrent capture/abort safe.

## Verification

`python engine/tools/metal4-contracts/run_native_cube_cancellation_test.py`
links the actual cube source, texture registry and texture history, with
ASan/UBSan. Covers exact rollback; stale N vs N+1; empty receipt; paired sky/main
rollback; sampler preservation; changed allocation; name recreation; detached
source; and compatibility no-argument cancellation. Native resource allocation
is a host-only test stub, not a Metal validation.

## Remaining shield stage migration: required complete transaction

1. Capture owns a journal containing exact map/model publications, context and
   registry ownership, source frame, submission identity, camera, model target
   name and extent, plus the exact cube update. Seal a render receipt without
   retaining access to mutable capture state.
2. Replace renderer pending-pointer validation with sealed receipt identity,
   retaining source-frame/camera/backing validation at the existing boundaries.
   Standalone model-depth preparation currently reads geometry name/extent live;
   capture those values rather than moving the pending pointer alone.
3. External image providers currently read `shieldPending->submissionIdentity`
   during prepare for Bloom, SSAO, deferred color and shield depth. Supply a
   scoped render-input identity BEFORE Begin. `activeSourceSubmission` alone is
   not a replacement because it is set only after lease Begin.
4. Preserve abort semantics through exact transaction notices: unsubmitted may
   cancel, begun failures are quarantined, accepted deferred work retains all
   resources until completion. A later capture's abort must not cancel prior
   recorded work or be misdirected by the latest global cube update.
5. Source clear/shutdown must drain/retire the journals before context/resource
   invalidation. Existing synchronous allocation/control checks remain in place.

Blocking dependency for the full move is the source journal/renderer lifecycle
handoff in item 4, together with item 3's preparation identity. The newly added
exact cube cancellation is necessary but not sufficient. U3 shield migration
remains OPEN; no renderer mutable shield fields have been removed here.

Independent review: GO for this bounded synchronous prerequisite. Not a
multi-frame cancellation journal: if B retains A as its previous content,
canceling A while B is current does not tombstone A; canceling B can restore A.
Future lookahead must preserve dependency ordering or implement explicit journal
cancellation state. Existing abort inspects only the current update.
Renderer wiring was committed with mailbox integration in `6acad89`.
