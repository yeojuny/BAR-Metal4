# P6: fail-closed GPU pending admission

## Completed
- [x] Removed the `priorReadChecks.empty()` shortcut that admitted a second GPU submission when the TwoInFlight experiment was enabled.
- [x] Every deferred submission now completes predecessor event/feedback retirement, existing scene receipt and registered read checks before enqueue. Admission additionally requires both pending pointer and queue to be empty.
- [x] Kept CPU lookahead and the experiment's three recording slots. More recording slots do not imply permission for concurrent GPU tickets.
- [x] Diagnostic now reports GPU pending limit 1 and unproven pending-borrow coverage, rather than implying two-ticket safety.
- [x] Regression compiles the actual production retirement and submission-admission statements against a mock runtime. Covers option off/on, pending 0/1/2, checks absent/present, an unregistered encoded-input borrow, and drain/receipt/read-check failures: 15 cases, ASan/UBSan pass. Existing lease retirement policy regression also passes (1,000 control/worker cases plus policy matrix).

## Why
An empty callback map is not a complete inventory of pending resources. In particular, Postprocess pending encoded inputs rely on retire-before-next-submit without registration in that map. Until every pending borrow provides explicit producer/ordering/lifetime proof, the submission boundary must fail closed. This fixes the unsafe admission condition; it does not implement a complete borrow ledger or finish P6.

## Incomplete / limits
- [ ] Complete pending-borrow coverage and explicit eligibility proof for a future second GPU ticket.
- [ ] General typed/provider parallel work and younger-generation rollback.
- [ ] Native Metal compilation, actual Postprocess GPU lifetime/visual/error parity, and performance acceptance.

The new regression uses mock GPU and borrow lifecycle, not actual Metal/Postprocess execution. Full app build/run remains deferred at the user's request. No performance gain is claimed; the old two-ticket experiment now deliberately serializes GPU submissions. Existing main, installed apps and unrelated vendor edits are untouched.
