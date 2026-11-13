# UI target execution / submission receipt separation

This is a bounded prerequisite for U1/U2, **not** a complete owning screen packet,
multiple-frame ledger, asynchronous renderer, or measured performance gain.

## Change

`ScreenRenderTargetPass` no longer mixes GPU submission receipts with immutable
execution descriptors. `NativeUITargetPassJournal` keeps ordered descriptors and
capture-owner receipt rows separately. Recorder append, rollback, exact prefix
retirement, full/partial retirement, and existing one-frame hold use that journal.

- Existing color-version alias matching in `RecordTargetSubmission` is preserved.
- Submission receipt writes and compaction do not allocate after GPU Begin.
- Geometric capacity growth avoids allocating on every appended target.
- Allocation failure during either vector reservation leaves both logical arrays
  unchanged. Execution-descriptor moves are statically checked non-throwing.
- Independent unsubmitted passes and already-submitted source-dependent passes
  retain the existing rollback behavior.
- The descriptor view is **still borrowed**. `EndFrame()` is intentionally not
  advertised as owning or safe for concurrent capture.

## Verification

Run from any directory:

```sh
python engine/tools/metal4-contracts/run_native_ui_target_journal_test.py --output /tmp/ui-journal-test
```

The runner compiles the complete real `NativeUIRecorder.cpp` translation unit for
syntax, then directly tests the production journal header with ASan/UBSan:

- 108,472 assertions, including 20,000 randomized operations against the former
  inline-receipt model;
- exact retirement, duplicate idempotent receipt, color/depth aliases, source vs
  independent rollback, and moved held frame isolation;
- first/second allocation failure and zero-allocation submission/retirement;
- zero/conflicting submission identities terminate in isolated child processes.

`RESULTS.json` records source hashes and test scope. LeakSanitizer is disabled in
this sandbox; AddressSanitizer and UndefinedBehaviorSanitizer remain enabled.

## Explicit remaining verification and implementation

The old full recorder target fixture uses the removed no-argument
`BeginGenesisStage()` API. A temporary compatibility update to pass `false` then
reached unresolved engine-service linkage (world-family validators and texture
history). No stubs were invented. Full recorder runtime regression remains open;
syntax success and helper tests are not a full recorder pass.

Next work must move root geometry and target descriptors into an owned packet,
separate live binding publication journals with exact frame IDs, and replace
renderer callbacks into mutable main-owned recorders with explicit notices.
The existing single submitted-frame optional remains unchanged. No Mac build,
Metal execution, framebuffer comparison, concurrent capture, or FPS test occurred.
