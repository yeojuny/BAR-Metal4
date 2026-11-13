# P1-C immutable CUS roster and bounded P2 read-receipt reuse

Source intervention on top of `cc39127`; no change to original gameplay or public Lua API.

## Implemented dependency order

1. `NativeCUSSubmissionRoster::Seal` privately constructs an immutable ordered
   object/submission/bin owner. It retains the exact AddToSubmission model owner,
   original index first/count and ordering, and runs the original geometry count
   proof once at seal time. The roster contains no frame, uniforms, pose, texture,
   instance snapshot or GPU resource.
2. `VertexArray` seals lazily on Submit. Each successful AddOne immediately drops
   the cached roster, including a committed prefix of an Add(table) whose later
   item fails. Swap-remove, Clear and Delete invalidate it too. Existing captured
   draws keep the old roster; there is no live-object lookup on Submit.
3. SourceDraw, ModelInputs and texture provenance share that same owner.
   Native source consumers use owner-backed const accessors, rather than copied
   vectors per draw. The legacy texture-history span overload remains available
   but is not used by CUS Submit; it does not coexist with copied order storage
   on the owned path. The mutable VAO builder is necessarily separate storage.
4. The CPU read receipt uses a conservative exact tuple of program, roster,
   instance snapshot, transform publication, model-uniform publication and engine
   uniform publication owners. A new per-event ModelInputs wrapper alone does not
   invalidate identical reads. Pointer AND control-block ownership must match.
   Interpolation/observed team rows stay tied to engineUniforms; skinning scale,
   bone branches and observed SSBO rows stay tied to publications and program.
   The existing geometry cache and all real read validation are unchanged.
5. Event sequences/frame provenance, texture snapshots and dynamic uniforms are
   still captured per Submit. The consumer still validates each SourceDraw before
   using a receipt; this patch does not suppress event/state/texture validation.

## CPU verification

Run `python3 engine/tools/metal4-contracts/run_native_cus_roster_test.py --output /tmp/cus-test`.
This links six actual production backend translation units with ASan/UBSan.
It covers sealed owner retention/order, swap-remove survival, invalid model/index
metadata, exact instance ABI, per-event envelopes, texture event ordinals, shared
storage, conservative receipt misses for every content-owner dimension, aliasing
control blocks, packet transfer, team255, missing team rows and skinning reads.
Separate source checks cover the actual Lua builder's invalidation sites; these
are not Lua VM execution tests.

The `--negative-control` variant deliberately restores wrapper-dependent cache
identity and must fail the duplicate-event receipt-count assertion.

## Remaining gates / not claimed

No full application build, Objective-C++/Metal compilation, Mac launch, GPU
parity or frame-time measurement was performed. Those are reserved for the one
final integrated Mac build after remaining interventions. CPU tests are not FPS
results. Full P2 phase payload sharing, uniform ABI/late-transport splitting and
source manifest sharing are not included. No worker/queue ownership, MRT alias
permission, sampler owning witnesses or GPU publication behavior changed.
