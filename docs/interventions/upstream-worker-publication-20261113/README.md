# Exception-safe bounded worker publication

The existing synchronous serial worker previously marked its only slot busy
before constructing the stored std::function. That construction can allocate;
allocation failure could leave busy=true with no queued work, permanently
rejecting subsequent invocations.

Invoke now constructs the owning callable before changing worker state, then
publishes it by noexcept std::function move assignment under the existing mutex,
and commits busy=true. No worker execution, source side effect or admission slot
exists when allocating the task/callable fails. Boundedness, synchronous waiting,
recursive-call rejection and exception delivery remain unchanged.

Validation: actual production worker under ASan/UBSan, allocator failure injection
at 16 caller-thread positions (four actual bad_alloc boundaries), plus 1,000
worker exception/recovery pairs. Every failed admission runs zero task work and
the next invocation succeeds. A negative-control build with the original
busy-before-allocation ordering fails the same recovery test with an outstanding
invocation error. This is actual worker lifecycle testing, not Metal execution.

This repairs an ordered-control prerequisite; it does not enable asynchronous
lookahead or establish exclusive ownership for all renderer caches/services.

Independent review: GO, including rebuilt fault-injection and negative-control
runs. Existing 3,000 actual serial-worker Begin-receipt lifecycle cases and fatal
controls also pass after the change.
