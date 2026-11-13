# Capture-owner completion ledgers (U2 prerequisite)

UI, postprocess and cloud recorders now retain submitted publications in an
exact-identity, fixed two-slot ledger rather than a single optional/shared owner.
The bound matches SceneBridge's existing deferred completion receipt bound. The
existing renderer still determines overlap; this change does not submit extra
frames or enable CPU lookahead.

## Ownership and compatibility

- Hold performs only no-throw moves into preallocated slots, with no new heap
  allocation after a successful GPU submission.
- Zero (UI/postprocess), duplicate and full Hold requests are rejected before
  consuming the caller's resources. The cloud API's previously accepted zero
  source-frame identity remains accepted through an explicit ledger policy.
- Exact completion releases only its matching submitted owner, regardless of
  completion order. Unknown/duplicate completion retains each recorder's existing
  rejection behavior, including postprocess/cloud's synchronous pending fallback.
- `OwnsPendingVersion` checks current publications and **every** held postprocess
  journal, not only the most recently submitted frame.
- Begin/abort/reset affect current capture only. They do not roll back or release
  submitted GPU work. Destruction still releases held owners at object teardown,
  under the existing safe-GPU-teardown requirement.

## Verification

Run these source tests from the repository root:

```sh
python engine/tools/metal4-contracts/run_native_completed_frame_ledger_test.py --output /tmp/ledger
python engine/tools/metal4-contracts/run_native_frame_journal_owners_test.py --output /tmp/owners
python engine/tools/metal4-contracts/run_native_ui_target_journal_test.py --output /tmp/journal
python engine/tools/metal4-contracts/run_native_owned_screen_test.py
```

- Production ledger header: 57,881 ASan/UBSan checks, including 10,000 randomized
  operations, allocation-forbidden Hold/Retire, rejected-transfer preservation,
  out-of-order completion, owner destruction, capacity reuse and exact-ID errors.
- Actual postprocess/cloud implementation `.cpp` files are linked (section GC,
  no algorithm stubs): 1,028 checks cover all held versions, overflow then retry,
  abort/reset isolation, synchronous completion, cloud zero identity, duplicate cloud Hold with pending isolation, and 500
  repeated two-frame cycles.
- Full `NativeUIRecorder.cpp` translation-unit syntax passes; existing UI journal
  108,472 checks/fatal controls and owned-screen isolation test pass.
- Independent review found no blocker in the existing single capture-owner path.

## Not yet complete

This is **not** a concurrent queue, backpressure policy, or complete P3–P5.
No epoch/state mailbox or pre-publication reservation was added.
All ledger access must stay on its capture owner. The renderer still has live
recorder access. Before overlap is increased, slots across all owners must be
reserved **before GPU submission**; a post-submit failed Hold is not recoverable
admission control. No new overlap is enabled here.

Full UI recorder runtime linkage, Mac build, Metal correctness/parity and measured
performance remain unverified. Helper checks are not a full game test.
