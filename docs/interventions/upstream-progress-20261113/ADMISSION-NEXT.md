# Next required lifecycle transaction

The ordinary synchronous path now uses a collective UI/postprocess/cloud transaction before renderer invocation. It prevalidates all owners, reserves completion capacity, transfers without throwing gaps, routes Begin to exact held UI ownership, restores no-Begin/fallback pending owners, and quarantines actual-Begin failures. Do not enable younger capture: world/shield dependency ownership is still incomplete.

Required main-owned whole-frame state, keyed by epoch/source draw/postprocess:

1. Reserve one bounded whole-frame slot and mailbox result capacity, validate all
   source identities/closed scopes, allocate all receipts. Failure changes nothing.
2. Transfer UI/postprocess/cloud journals without allocation or throwing steps
   between moves. Pending-version queries must include all admitted owners.
3. Record durable Begun evidence immediately after actual command Begin. Apply
   exact publication matches to that owner, not the latest recorder.
4. Before Begin only, restore pending ownership for hidden-surface fallback or
   roll back exact never-submitted publications. Do not restore after a younger
   capture has begun.
5. Accept runtime submission separately from GPU completion. Early completion
   cannot release the slot until acceptance is reconciled.
6. Validate all owners before completion side effects. After-Begin failure keeps
   owners quarantined and poisons the context; it never permits rollback/reuse.

States: Reserved -> Admitted -> Begun -> Submitted -> Completed -> Retired.
Only pre-Begin states can cancel; Begun/Submitted failure becomes Quarantined.

Keep SaveImage prefix/suffix routing separate: prefix retirement is not whole-frame
retirement. Source-dependent UI targets roll back differently from independent
screen targets; postprocess rolls back never-submitted changes in reverse order.

Shield migration must carry exact map/model/cube publications, registry/context,
camera, target names/extents and preparation submission identity. The latter is
needed before Begin by external image providers. Exact cube cancellation alone
cannot prevent A being restored when younger B rolls back to A; dependent younger
capture must remain prohibited until dependency-aware cancellation is designed.

Current completion processing prevalidates all three journal owners before
water/world retirement. The callback mailbox reserves bounded capacity and gates
completed notices on main acceptance. Terminal drain rejects any blocked queued
notice. Owned actual Begin evidence classifies both renderer and post-return
settlement failures. The ordinary journal transaction is implemented, including
synchronous completed fallback to Game.cpp's pending-retirement contract.

Remaining: all world/shield source-family ownership and dependency-safe rollback,
exclusive renderer/cache ownership, ordered control/teardown and actual Metal
verification. No concurrent younger capture or CPU lookahead is enabled. Do not
infer whole-world atomic admission from the three-journal coordinator.

External-image read routing follow-up: the preparation submission identity now
comes from an explicit per-invocation serial scope, not shieldPending. World,
maintenance, independent readback and continuation each bind their own identity.
This removes provider inference only; shield publication/cancellation still needs
exact source-family ownership before concurrent younger capture can be admitted.

Cube rollback follow-up: all world abort callers now pass the captured environment
receipt; cube cancellation no longer chooses CurrentReflectionUpdate. Exact CPU
capture guards cover failures before a receipt is installed, and submitted cube/
paired-mip work is preserved. Younger dependent capture remains prohibited;
this is not a dependency-aware version graph or full source-family transaction.

Shield rollback follow-up: Session now retains the exact capture from the map
boundary (before model capture can throw), passes it to abort/clear, and releases
it on success or safe abort only. Failed actual Begin rejects abort before family
resets; stale/absent receipts cannot cancel a different pending shield owner.
This is serial capture ownership, not concurrent mutable cache safety.

Normal cache follow-up: readiness is owned by the exact retained normal MapImage
instead of a renderer-global flag. Deferred completion cannot mark a newer owner
ready. Height/normal GPU storage remains in-place under its existing retirement
barrier; actual resource renaming/exclusive renderer ownership is still required
before lookahead. This ObjC++ step has structural review only until Mac validation.

Worker publication follow-up: all potentially throwing callable allocation now
precedes busy-slot commit. Actual allocation-fault/recovery tests expose the old
leaked busy slot and pass with noexcept publication. Worker remains synchronous
and bounded; no new control queue/lookahead admission is enabled.

Renderer access follow-up: cache/control/render entries, retained raw-owner
providers and deferred completion now share a fail-fast serial CPU ownership gate.
Same-thread retirement nesting is supported; cross-thread conflicts quarantine/
reject rather than wait. DrawDispatch releases snapshot ownership before worker
handoff. This enforces a boundary, not a control scheduling queue or source/global
service thread-safety; lookahead remains disabled until those remaining gates.

Renderer handoff follow-up: a reservation now bridges main snapshot release to
worker acquisition and remains until synchronous Invoke joins. Only the exact
worker token can enter; ordinary controls cannot invalidate the snapshot in the
gap. Main receipt callbacks run after reservation cancellation. Allocation/worker
failure cancellation and concurrent rejected-probe teardown are host-tested.
This is exclusive serial handoff, not queued controls or younger source rollback;
shared sources and GPU resource lifetime/versioning gates remain before lookahead.

Shadow cancellation follow-up: exact depth/color publications now preflight both
written backing states; encoded or completed either half preserves the pair.
Repeated stale abort cannot erase successful current contents. Retained unlit
color is not a write and does not veto rollback. Actual source/registry tests cover
100 capture and partial-allocation failures plus encoded/completed/newer/sampler/
unlit/detach cases. Independent sanitizer review passed. Deferred MRT color/depth
resources still need equivalent execution-state-aware whole-phase preflight;
this source guard does not provide a younger-capture dependency version graph.

MRT cancellation follow-up: actual color/depth resource execution state now gates
rollback conservatively (unknown fulfilled resource refuses; unfulfilled allowed).
Every changed output and opaque pass is preflighted, and renderer scene abort
rejects blocked opaque/ground/exact-shield owners before any family mutation.
Exact blocked shield pending ownership is retained. Prepared submission IDs alone
do not block rollback. Failed-Begun rejection remains necessary; this is serial
execution safety, not a younger-dependency version graph or lookahead permission.

Legacy refraction follow-up: actual Begin marks the exact copied-color backing,
so source-exit cleanup cannot discard pending/prior owners after Begin but before
EncodeCopy. Encoded-uncompleted copies also refuse rollback, replacement and
destruction; completed cleanup releases receipts without reverting pixels.
DrawWorld exit additionally respects renderer active-source ownership. Host actual
publication/registry tests and structural Begin wiring checks cover the boundary;
Metal runtime verification and concurrent source versioning remain outstanding.

Native-build discovery follow-up: independent SaveImage still referenced the old
execution-descriptor actualSubmission field. Readback prefixes now own exact
color/depth journal receipt snapshots; renderer keeps the original submitted-work
rejection without reintroducing mutable receipts into descriptors. Prefix fixture
covers submitted/unsubmitted, missing/mismatched owner and journal mutation after
snapshot; this remains immediate synchronous readback, not delayed replay support.

Owned normal-dispatch follow-up: SceneBridge moves WorldFrame into a shared owner,
seals options plus the exact owned screen descriptor before journal transfer, and
releases its mutable owning alias. Worker captures this packet by value; borrowed
direct/readback calls remain main-synchronous. No geometry copy is introduced.
This pins CPU dispatch inputs only; existing runtime graph retains GPU owners.
Stack receipts still require joined Invoke and global source/service concurrency
remains unproven, so neither queueing nor CPU lookahead is enabled.

Ordered control follow-up: the worker is now renderer-owned and joins before
provider teardown. Water-family GPU resets reuse it after main-thread retirement,
with exact handoff and a cleanup lease that rejects pending submissions rather
than draining source callbacks on the worker. No window refresh is added. Other
controls still retain direct serial behavior; these routed water controls are not a
general control queue, asynchronous renderer lifetime or lookahead admission.

Terrain pair isolation follow-up: changed immutable heights now create a distinct
height buffer/normal texture pair, including same-size deformation. The complete
resource retains its source and is published only after both allocations succeed;
retained old normal owners no longer alias overwritten terrain storage. Same-owner
reuse remains, and static reset clears the cache. Existing height/map retirement
barrier stays until other dependency caches are proven isolated. Host allocation-
failure/cache ownership tests do not substitute for native texture validation.

### Terrain cache transaction follow-up

Tile cache staging and pair allocation/retention now precede one no-throw commit
boundary. Failure no longer publishes new tiles with the old height/normal pair.
Cache-hit preparation is intentional (tiles can change independently of heights).
Existing runtime failure handling and retirement barriers remain; this does not
roll back a runtime lease or authorize lookahead. See terrain-pair-isolation report.

### Water attachment selection boundary

Six planar/pre-water images now stage as one draw-owned bundle, retain before
publication, and route all later consumers/copies through that exact selection.
The live cache cannot publish a partial failed allocation. Physical compatible
storage still reuses serially: this is not water overlap readiness. Native Metal
allocation tests remain pending; see water-attachment-selection report.

### Main framebuffer bounded idle reuse

Prepared main descriptors now pin their immutable unbound allocation prototype.
At most two prototypes are cached; only unpinned compatible slots are reused,
with owned uncached fallback if both are held. Production CPU policy tests cover
1000 alternating frames and 3000 preparation failures. Continuation keeps exact
frame identity. Native execution/performance remains pending; raw aliases and
other caches still require existing barriers, so lookahead remains disabled.

Cropped terrain depth now also uses a bounded idle pair cache and exact snapshot
pin, replacing latest raw-texture reuse. N=1 alias and multisample distinction,
per-snapshot encode guard and original events/barriers are preserved.

Water reflection/refraction/pre-copy now have separate bounded idle pair pools;
each active selection pins its physical pair through runtime lifetime. The earlier
selection-only limitation is superseded for these three roles, not for other
caches. Existing barriers stay; cache bounds do not bound all live GPU memory.
