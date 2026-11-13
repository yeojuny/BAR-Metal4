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

Persistent PBR Lua compile callback now acquires the renderer gate before its
RuntimeLease (the connection-time stack guard did not cover later invocations).
It fails unsupported ownership early; no concurrent capture permission follows.
Worker UI DescribeBinding's live source context/name fallback remains a concrete
next captured-descriptor handoff boundary.

Initial UI allocation metadata is now retained as AllocationOnly backing on its
exact immutable object; DescribeBinding no longer follows a mutable same-name
cell. Actual registry1000-case sanitizer tests and whole UI history C++ syntax
pass. Live context/external descriptor service access remains to be isolated.

UI descriptor follow-up: main captures an owning resolver service before worker
handoff; pure retained-binding resolution no longer reads live context/registry
on ordinary-model/StartBox worker paths. Main-only compatibility fallback remains
outside draw scopes. Existing typed execution/readability dependencies stay;
this is not complete source-family capture overlap readiness.

### MRT209 normalized frontier boundary (2026-11-13)

The main dispatch now normalizes and retains the six exact F publication tokens
before the joined worker handoff. MRT209 compares those retained frontier values
instead of calling mutable `NativeTextureRegistry::Find` inside preparation.
The capture retains no extra U publication or U backing: both exclusive-owner
counts remain exactly two. Absent/malformed/mismatching frontier or optional
normalization allocation failure falls back to the existing COW path.
Direct callers with a `previousRetired` callback disable this optional reuse:
that callback may mutate source state after capture. Worker retirement remains
a flag delivered on main only after joining. This is not a timeless seal and
permits neither younger capture nor CPU/GPU lookahead.

Validation: actual registry normalization/frontier lifetime/U-count sanitizer
fixture 1,000 iterations, actual owned-worker handoff 1,000 iterations,
SceneBridge C++ syntax, independent review. Full successful reuse issuance and
native Metal execution are not claimed. Remaining worker source reads include
model geometry metadata and registry context-generation checks; capture these
next without weakening context validity or the existing serial barriers.

### Worker source-context metadata boundary (2026-11-13)

Joined worker dispatch now captures context initialization/generation and model
GBuffer depth-presence/width/height on main. `Name(5)` means depth attachment,
not combined color. Both ground-context checks and fallback model-depth metadata
consume this value snapshot. Missing/zero context fails at actual use; a valid
context with empty model geometry remains valid. Direct calls retain lazy live
reads at the original use sites, after any retirement callback, and explicitly
require the actual main thread before live access. The worker cannot silently
fall back to mutable source state.

Validation: 1,000 sanitizer cases exercise the actual production capture template
with a controlled fake context adapter (not the real engine context), plus source
handoff/use-site assertions, actual owned-worker 1,000 handoffs, SceneBridge syntax
and independent review. Native runtime verification remains deferred. Snapshot
lifetime still relies on joined dispatch; no concurrent context teardown, younger
source capture, or lookahead is permitted. The next architectural gate remains
shared-source version dependencies and whole-world admission/rollback ownership.

### MRT capture receipt allocation ordering (2026-11-13)

Both map and opaque-model capture previously constructed a shared receipt with
`make_shared<Phase>(BeginPhase(...))`: Begin could publish registry versions,
then the outer owner allocation could fail without leaving a cancellation receipt.
They now use `OwnNativeCUSDeferredPhase`, allocating the empty shared owner first,
then invoking the actual Begin publisher and noexcept move-assigning its result.
Begin's internal partial-publication rollback remains responsible for its failures;
no allocating/type-erased callback or throwing operation follows successful Begin
before receipt ownership reaches the existing outer rollback guard.

Validation: real map/model Begin publishers, registry, and production ownership
helper under allocator fault injection: 41 failing points rolled back and 89
successful receipts retained/cancelled. Map tests include the seventh combined
attachment. A failure armed after successful Begin causes no later allocation;
the old ordering negative control fails the expected receipt assertion. Both
actual EngineBridge translation units pass C++ syntax checks; independent review
passed. Native GPU execution and concurrent capture remain outside this evidence.

### Dynamic-water exact admission ownership milestone (2026-11-13)

Dynamic-water capture now retains bounded exact receipts keyed by material owner,
source draw, and through-update sequence instead of one mutable reserved/finalized
pair. Its real capture/finalization paths use the ledger; exact retirement removes
only the matching receipt and update prefix. A fourth admission owner participates
in collective validation before no-throw transfer, all-owner restoration validation,
and post-Begin quarantine. Synchronous completion restores Held to Finalized before
retiring; deferred completion requires Held plus the matching scene draw before
any completion owner is mutated. Missing dynamic water is an explicit no-op adapter.
Reset rejects held/quarantined receipts rather than dropping their owners.

The ledger can preserve younger receipts in tests, but production still rejects
Update/new-draw capture while any reservation exists and retains the dynamic-water
early drain and GPU recurrence barriers. `firstDraw` changes only on exact successful
retirement, as before. Therefore this milestone does NOT enable capture overlap or
claim once-only bootstrap under concurrent admission; that remains a later gate.

Validation: production ledger and four-owner transaction sanitizer tests use actual
water frames and controlled other-journal adapters; actual postprocess/cloud owner
regression passes 1,073 checks. Water cases cover wrong tuples/order/capacity,
older retirement preserving younger owners, validation-before-moves, pre-Begin
restore/retry, synchronous/deferred settlement, quarantine and reset rejection.
Both actual DynamicWaterCapture and SceneBridge translation units pass C++ syntax
checks. Independent review and source wiring assertions cover the integration;
full native application/Metal execution remains deferred.

Dynamic-water recoverable capture-abort follow-up: Session retains the exact derived
receipt immediately after capture, before water-view/finalization work can throw
and independently of moving the prepared WorldFrame into submission ownership.
After existing failed-Begun/continuation guards, failed preparation and no-Begin
fallback may remove only that Captured/Finalized receipt. Held/Quarantined and a
same-draw replacement owner are refused; stale missing receipts are harmless.
Pending update/impulse data, sequence, retired prefix and first-draw state are not
changed by this abort, so the next source capture can retry rather than remaining
permanently blocked. Completion clears the Session token only on exact tuple match;
validated reset clears it after the source reset succeeds.

Additional 1,000 exact-abort ledger cases cover partial/finalized capture, wrong
owner, stale/replaced owner, newer receipt preservation and held/quarantine refusal.
Actual capture/SceneBridge syntax and independent source-wiring review pass. This
still does not relax any overlap barrier or implement a speculative younger-frame
bootstrap protocol.

Dynamic-water bootstrap-intent follow-up: captured frames explicitly mark the
optional derived first-draw detail initialization. Preparation validates that the
marker matches the exact [height] or [height, detail] source sequence. Only this
marked bootstrap suffix may be omitted when an exact same-material committed
recurrence is already retained and its detail image is copied into staging.
Regular per-update detail-normal work is unchanged. Without a committed predecessor
(including a pre-Begin rejected predecessor), the captured bootstrap is preserved.
This is a preparation prerequisite, not permission for younger capture; failed-
Begun quarantine, active recurrence exclusivity and all drain gates remain intact.
Validation: actual derived-selection policy sanitizer tests (1,000 iterations),
malformed marker/order rejection and unchanged-update checks, actual capture C++
syntax, structural renderer ordering/material/copy checks and independent review.
The native Metal translation unit has not been compiled or executed here.

### Paired map source-publication transaction (2026-11-13)

Map normals/height publication previously could commit the first registry output
then throw allocating/publishing the second, leaving a mixed source pair. Both
source validations and new immutable backings now stage before the first write;
any later failure reverses only this attempt's exact new publications. Reused
outputs are not treated as new producers or rolled back. Each changed output
reserves version capacity for a rollback increment. This protects source registry
contents, separate from the previously implemented physical GPU pair transaction.

Registry Replace now performs its throwing history-event advance before its
no-throw existing-entry assignment. A history counter overflow no longer commits
a version without delivering its receipt. Successful replacement clears deferred
restoration, so retaining its immediate Find result introduces no allocation gap.
Cancellation preserves real sampler state and advances content versions; it does
not rewind history counters or claim exact numeric version rollback. Serial source
capture remains required; this is not a younger-producer dependency graph.
Validation: actual map/registry allocator fault injection restores both prior
backings at 11 failing positions and verifies 71 successful/retry/reuse cases;
old nontransactional pair code fails the expected restoration assertion. Actual
history-overflow branch is exercised using an identical-layout test-only header
accessor, verifying preservation of the exact prior registry object. MRT frontier
regression passes 1,000 cases; actual modified source translation units pass C++
syntax checks. Native application execution remains deferred.
The old Replace ordering also fails the overflow negative-control assertion;
independent review passed for both paired rollback and registry commit ordering.

### Preserve CPU read receipts on the joined executor

The original handoff Appendix C section 8 identifies a real reuse loss: main primes
or adopts a CPU read packet, but the renderer's worker has a different TLS scope
and falls back to validating those same reads again. After independent main
preparation, SceneBridge now seals that exact packet into the owned submission.
DrawPrepared activates a read-only view on its consuming thread; exact retained
owner-tuple checks still gate lookup. The worker does not inherit a TLS pointer,
mutate the packet, move the geometry cache, or own final source destruction.
Nested and exception exit restore the previous view. Packet preparation counters
travel with the rows; this does not by itself fix all cross-thread CPU trace scopes.

Joined-worker idle publication now occurs after releasing its local task reference.
Previously Invoke could return before that release and race main's last source
owner drop, making final destruction happen on the worker. The caller's packaged
task reference now remains the last task reference until the joined call exits.
The CPU packet is not added to GPU completion/continuation lifetimes, and the main
submission/local packet pins it across both successful and exceptional return.
This preserves a named original optimization; it does not enable CPU lookahead.
Validation: actual roster/read-set fixture and serial worker pass 11,056 checks,
including 1,000 handoffs, unchanged read rows, exact-owner mismatch, nested/error
view restoration and unchanged worker geometry-cache size. A deterministic
post-idle test hook confirms final source release on main; restoring the old
worker cleanup order fails that contract. The hook exists only in the test header
overlay. Worker allocation/exception regression passes four allocation failures
and 1,000 recovery pairs. SceneBridge passes host C++ syntax; native execution
and the P5 positive active-overlap gate remain unverified.

## CPU trace ancestry across the joined handoff

The handoff Appendix C trace prerequisite now carries a value-only sampling/parent
context into the ordinary joined render task. Previously the worker's TLS
captureLoop stayed false, suppressing its encode/preparation phase records.
The worker inherits the main capture sample ID, then its local nested scopes
supply their normal parent IDs. Neither live stack pointers nor source-thread
scope ownership cross the handoff. Local active scopes cannot be overwritten;
context scopes restore on normal and exceptional exit. Per-thread interval history
remains local, and the existing global ticket/record/byte budgets remain serialized.
Direct rendering, gameplay-only sampling and the 1-in-16 default are unchanged.

Validation extracts the actual runtime block and runs the real serial worker under
ASan/UBSan: 100 invocations in default/every-loop/disabled modes, complete ordered
four-phase ancestry, nested contexts, local-scope protection and exception cleanup.
Restoring the old missing-parent rule is rejected by the same fixture. This closes
a diagnostic handoff prerequisite, not the P5 positive-overlap measurement: the
worker still joins, and full native Metal compilation/execution remains deferred.

## Exact disabled-shadow fallback selection

The original P4 audit's disabled-shadow path still resolved live handler names and
source state on the joined render worker. It now captures the selected depth/color
backings on main after direct-route selection, then consumes only that receipt.
An engaged empty receipt remains empty even if the source later attaches. Actual
producer readiness is still checked after retirement at use, not guessed during
capture; the existing GPU lifetime retains the selected pair. Direct synchronous
callbacks keep their lazy source lookup at the original use site, and original
shadow producer frames bypass fallback capture entirely. This does not relax the
joined barrier or permit source publication overlap by itself.
Validation: 1,000 actual snapshot-template cases with controlled source/handler
adapters under ASan/UBSan, covering replacement/reset, absent source/owner,
missing images and live completion on the retained backing. Structural checks
cover dispatch/direct timing and existing GPU retention. SceneBridge host C++
syntax passes; independent review GO. No actual ShadowHandler/Metal run claim.

## Source-owned shield capture journal

The original P4 single `Impl::shieldPending` capture slot is removed. Session now
owns a noncopyable journal tied to the registry epoch. Map capture borrows the
exact map backing; model finalization stages its plan/publication, commits with
no-throw moves, and seals once. Duplicate finalization, camera/draw mismatches
and source-epoch replacement are rejected. Renderer preparation uses the sealed
frame receipt and never compares or clears a live source capture slot.

The immutable shield receipt is pinned in the GPU lifetime before Begin, including
failed-Begun work. Source cancellation remains main-only, behind the existing
renderer active-submission veto and all-family preflight. Successful acceptance,
synchronous completion and verified abort release Session ownership; hidden
no-Begin results retain it until rollback. An explicit accepted-result guard
reinforces the existing restoreAdmitted transition; no prior no-Begin loss is
claimed from that branch.
The shared actual submission identity allocator is unchanged. Existing serial
and GPU gates remain; this is not permission for stale-frame cancellation against
a newer journal or general younger-frame dependency rollback.
Validation uses the actual journal, NativeTextureContext, registry and source
publication code (not a mock journal): 100 begin/seal/borrow/rollback/retention
cycles, duplicate/mismatched finalization rejection, invalid-storage failure and
retry, and busy-resource cancellation refusal. Existing shield cancellation and
cube cancellation each retain their 100-case regression. Host SceneBridge and
publication C++ syntax pass. Structural checks cover the failed-Begin veto,
source-owner reset points and GPU pin before Begin; Metal execution is still not
claimed. The source publication helper's serial cancellation semantics remain;
a general rollback dependency graph is still required before younger overlap.
Final actual-context sanitizer rerun additionally verifies destroy/reinitialize
rejects the old journal epoch without cancellation. Independent review GO;
shadow-fallback and CPU-trace regressions remain passing after the ownership split.

## Ordered coverage allocation control

CreateCoverageFBO keeps its synchronous source API contract: exact R8/D16
attachment descriptors are retained in a typed request, allocation/validation and
ScreenGPU cache mutation execute on the existing serial render worker, and the
strong pair owner returns before Lua creates its userdata. Allocation is not
postponed to DrawPrepared and no placeholder success is returned. Runtime/device
initialization remains on main. The allocation-only control opens no RuntimeLease,
so it neither drains pending GPU work nor runs source retirement callbacks on the
worker. Gate reservation spans the joined call without transferring a mutex.
The worker entry requires an admitted owner; broader main-thread checks are not
relaxed. This is an immediate-result control, not CPU lookahead or a GPU submit.
Validation: actual control helper/access gate/serial worker sanitizer tests pass
1,000 success and throw/recovery cases, exact returned-owner retention, foreign
owner rejection and final source destruction on main. Nested outer-gate invocation
is explicitly unsupported and fails without deadlock; the audited production Lua
CreateCoverageFBO call chain has no such outer renderer scope. Native texture/FBO
success and failure remain structural evidence only until Metal validation.
Independent review GO. Access-gate regression passes 1,000 nested/error and 100
cross-thread cases plus handoff/inversion checks; worker allocation failure and
CPU trace regressions remain passing. No full app/Metal build was performed.
