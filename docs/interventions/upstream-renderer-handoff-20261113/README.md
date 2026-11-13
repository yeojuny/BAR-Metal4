# Renderer snapshot-to-worker reservation

The synchronous worker path now reserves the renderer access gate while the main
snapshot scope is still held. It releases the main mutex, enters the worker using
that exact stack-owned token, and allows normal nested renderer/provider/retirement
access only on that admitted thread. Ordinary cache/control access is rejected in
the previously unowned snapshot-to-worker gap and until Invoke has joined.

The token is reset before main-thread target/retirement receipt callbacks, both on
success and exceptions. Allocation/enqueue failure cancels it without worker work.
No mutex is unlocked by a different thread. Cleanup waits only for brief rejecting
probes after worker join; using try-lock-and-terminate here was rejected by an
independent concurrent stress reproduction and fixed before commit.

Validation: actual gate/worker ASan+UBSan tests cover reservation gaps, nested
retirement, duplicate admission, worker throw, pre-enqueue cancellation and 10,000
concurrent probe/cancel attempts. Actual worker allocation fault injection verifies
reservation recovery at all tested failure positions, and still rejects the old
busy-before-allocation implementation as a negative control. Existing Begin receipt
regression and full SceneBridge host C++ syntax checks remain required.

This is an ordered exclusive handoff for the current synchronous worker, not a
control command queue or GPU cache renaming. A competing control is rejected, not
silently delayed or dropped. Destruction/move of the renderer remains externally
serialized. Independent controls, shared source services and younger dependent
capture are not made safe by this token. No CPU lookahead, app build, Objective-C++
compile or Metal runtime validation is claimed by these host tests.
