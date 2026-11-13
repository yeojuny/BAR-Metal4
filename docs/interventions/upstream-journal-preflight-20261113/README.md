# Whole-frame journal preflight prerequisite

AI-assisted source intervention; no app build, Mac execution, or new async mode.

The existing synchronous main-to-render-worker call now validates UI, postprocess
and cloud pending identities, closed scopes and held-ledger capacity before it
invokes the renderer for a deferred-eligible frame. Validation is non-mutating;
if a later recorder rejects, no earlier recorder has transferred its journals.
Actual transfer remains after the renderer accepts deferred work, as before.
Main is blocked during that call, and completion can only remove older owners.
If any ledger is full or holds the same ID, the existing GPU-retirement boundary
runs before validation. This preserves the existing optional two-in-flight path
which could otherwise have drained predecessors inside renderer submission.
It may block earlier when full; no performance equivalence is claimed.
This is preflight, **not reservation or atomic admission**.

Completion now checks that all three exact held owners exist before it calls any
water/world completion or journal retirement. Existing completion order remains
unchanged after validation. This prevents a missing later journal from causing
partial successful retirement; it does not prevalidate all external water/world
callbacks or make their failures transactional. No pending-owner fallback is
accepted by this deferred-only preflight. Synchronous and readback paths retain
their existing pending/prefix retirement behavior.

The production postprocess/cloud fixture checks invalid pending/held identities,
full capacity, missing later completion owner, retained frame/publication
lifetimes after rejection, zero-valued cloud IDs, reset/abort and slot reuse.
ASan/UBSan: 1,042 checks pass. Structural assertions verify that all-owner checks
precede renderer entry and completion side effects. The integrated SceneBridge
and actual UIRecorder/LuaCloud C++ translation units pass host syntax checking (existing
shared_ptr atomic deprecation warnings in UI headers). UI runtime linkage,
Objective-C++, GPU behavior, parity and performance remain unverified.

## Still required

Full admission needs reserved whole-frame/result capacity, no-throw collective
transfer, exact owned Begin marking, before-Begin restore, and begun-failure
quarantine. No mailbox credits were added: SceneBridge cannot yet reliably tell
whether a throwing renderer entered Begin. Retaining credit on every exception
would leak capacity on recoverable pre-Begin failures; releasing it on every
exception could lose begun work. Implement durable state before that migration.
Shield/cube dependency cancellation and teardown are still gates. Do not enable
additional lookahead or describe this checkpoint as complete admission.
