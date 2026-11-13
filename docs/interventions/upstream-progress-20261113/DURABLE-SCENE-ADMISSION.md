# Durable exact scene admission record

The actual joined DrawWorld path now retains a noncopyable heap admission record
in Session before transferring any source owners. The record owns its sealed
submission, context/mailbox epoch and ticket, source settlement, and cloud/water
adapters. Its transaction is reconstructed from these durable exact identities,
not DrawWorld's local adapters or borrowed options. Early stack preflight remains
only a preflight and is not retained across renderer invocation.

Restore validates source destinations before any ledger restore. Before-Begin
failure restores the exact group; after-Begin failure keeps it quarantined.
Accept requires the exact deferred renderer draw, actual Begin, presentation
submission and no synchronous completion claims before accepting the mailbox.
Completion may be published first, but cannot pop before acceptance. Each action
checks the current Session, context, mailbox, admission anchor, and transferred
source anchor. An old record cannot reset a replacement record in the same scene.

Tests: verbatim extracted production record with real admission transaction,
mailbox and Begin receipt; modeled engine/source adapters; 100 sanitizer cycles
cover restore, quarantine, acceptance ordering, occupied restore and stale owner
rejection. Independent review and separate sanitizer run: GO. Existing real
shield/context/registry/main-source settlement sanitizer fixture also passes;
updated structural checks tie it to the new production record. SceneBridge host
syntax passes. Full native app build/execution remains deferred.

Not P5 activation: DrawWorld still joins. General Game exception abort, telemetry
and synchronous-current-source retirement paths are not safe historical async
settlement yet. Next is a source callback after CPU Join that settles this exact
record before a control can trigger GPU retirement, then bounded begin-ready
queued return with eligible-source restrictions and measured overlap evidence.
