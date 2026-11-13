# Bounded completion capacity before submission

The production completion mailbox now reserves one of its two result slots in
`Prepare`, before renderer invocation. Previously arbitrary one-shot tickets could
be issued and capacity failure was deferred until completion `Publish`.

- Reservation and publication state use the existing mailbox mutex.
- An unpublished ticket abandoned by every callback copy makes its weak slot
  reusable without a destructor reaching into a potentially destroyed mailbox.
- Published notices pin their slots until `Pop`, even after callbacks disappear.
- FIFO order is publication order, independent of reservation order.
- Identity and one-shot replay checks remain effective after slot reuse.
- SceneBridge includes mailbox capacity in its existing predecessor-drain check.
  `Prepare` rechecks capacity under the lock. This remains the synchronous,
  main-blocked producer boundary; it does not enable another capture producer.

Actual production-header fixture: 23,030 checks under ASan/UBSan and separately
TSan. Full SceneBridge C++ translation-unit host syntax passed (not Objective-C++). Independent review GO. Tests cover abandoned shared tickets, queued notices
whose callbacks are destroyed, out-of-order completion, replay and close races.

This reserves completion storage only. It does not reserve or transfer all source
journals, supply durable Begun/quarantine state, enable CPU lookahead, or prove
Metal execution, visual parity or performance. Full app build remains deferred.
