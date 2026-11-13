# Next required lifecycle transaction

Do not move the three existing `HoldSubmittedFrame` calls earlier independently.
Sequential validation/move can leave partial ownership and the held ledgers have
no before-Begin restore or post-Begin quarantine contract yet.

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
water/world retirement. The callback mailbox now reserves bounded result capacity
before renderer invocation, and completed notices are gated on main's explicit
acceptance after all three ownership transfers. Terminal drains reject any blocked
queued result. The renderer now also supplies owned durable actual Begin evidence on failed
ordinary invocations. These are prerequisites, not early ownership transfer,
quarantine policy or whole-frame atomic admission. Renderer cache isolation,
source controls and teardown drains remain separate mandatory gates. Do not enable
CPU lookahead yet.
