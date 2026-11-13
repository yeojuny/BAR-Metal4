# Owned screen execution packet prerequisite

`NativeUIRecorder::EndFrame` now returns a ScreenFrameView retaining an immutable
storage owner. Root geometry is moved, not copied; target execution descriptors
are copied out of the independent mutable submission journal. Retiring or
clearing the journal does not invalidate returned target spans. Existing direct
synchronous callers may continue using externally owned views.

A single producer cache reclaims root geometry capacity only when no returned
view still owns the storage. A retained frame forces distinct storage for the
next frame. Current producer draw state is preserved across transfers. This is
not a queue, multi-frame completion ledger, or thread-safety mechanism.

Validation: real ScreenDrawList.cpp linked with ASan/UBSan. Tested geometry pointer
move identity, view-copy lifetime, old target retention, next-frame isolation,
sole-owner capacity reuse, destruction of producer cache, signed-zero state,
and 1,000 subsequent frames while retaining a prior view. Full recorder TU syntax
passes. Existing UI journal 108,472 checks and fatal controls rerun successfully.
Independent review found no blocking bug within the current single-producer use.

Limits: full recorder runtime/Metal execution remains unverified. Target descriptor
copy and new allocation when retained views overlap are deliberate ownership
costs, not a demonstrated performance win. RestoreState validates and may throw;
no blanket noexcept guarantee is claimed. Actual frame ownership ledger, live
renderer access separation, serial executor and bounded lookahead remain pending.
No application build, restart, GPU parity comparison or FPS test was performed.

Run `python engine/tools/metal4-contracts/run_native_owned_screen_test.py`.
