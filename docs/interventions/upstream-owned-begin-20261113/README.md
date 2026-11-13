# Durable actual Begin evidence

An ordinary deferred scene invocation owns a data-only NativeFrameBeginReceipt.
The actual renderer marks it immediately after lease.Begin succeeds and before
source publication callbacks. Main retains a failed invocation's receipt, so a
thrown return can be distinguished from actual Begin. The receipt contains exact
draw/postprocess identities and atomic fields, no Session or renderer pointer.
Zero or duplicate Begin is fatal. Renderer entry rejects mismatched, already-begun
or previously failed receipts. The direct readback/continuation path is unchanged.

3000 production receipt lifecycles through the actual serial worker passed
ASan/UBSan, with zero/duplicate fatal controls. Existing target-submission-notice
regressions and 23,041 mailbox checks pass. Full SceneBridge host C++ syntax
passed; independent review GO (failed-reuse guard added). Actual Objective-C++/Metal execution remains unverified.

This is durable evidence, not successful submission or GPU completion. The
failure catch surrounds the renderer invocation, not later journal Hold/Accept.
After-Begin includes later renderer/prior-receipt exceptions; it must not be
interpreted as proof of a GPU fault. No rollback, early source transfer or
quarantine policy is enabled by this checkpoint. Full frame admission remains
incomplete and CPU lookahead stays disabled.
