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
