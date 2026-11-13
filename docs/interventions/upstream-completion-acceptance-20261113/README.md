# Completion is distinct from source ownership acceptance

Main calls mailbox Accept only after all three UI/postprocess/cloud Hold calls.
An early completed GPU notice cannot retire source owners before this transfer.
Unaccepted queue heads remain blocked, preserving publication order. The terminal
GPU drain additionally requires no queued notices, so this condition cannot be
mistaken for successful retirement or silently permit teardown. An orphaned
completed notice fails explicitly. This is fail-closed, not partial-Hold rollback.
Unpublished abandoned tickets still release capacity; slot reuse resets acceptance.

Production-header ASan/UBSan and TSan each passed 23,041 checks, including early
completion, accepted unpublished abandonment/reuse, orphaned and live blocked
heads, replay, terminal-drain rejection and concurrency. Full SceneBridge host C++
syntax passed; independent review GO. Actual Metal/Objective-C++ execution is not
verified. Source-wiring assertions are not an integrated engine execution test.

This does not provide early journal transfer, durable Begun state or full atomic
admission. Main still waits during renderer invocation; no lookahead enabled.
