# Exact shield capture and failed-Begin retention

The original map boundary now returns its exact shield capture owner to Session
immediately, before later model/world capture can fail. Later model publication
updates that same retained capture. All abort and resource-clear paths receive
this receipt explicitly. Registry rollback and pending-slot release occur only
when the supplied owner matches the actual pending capture; absent or stale
receipts cannot cancel a newer pending source.

AbortShieldPublications returns false for actual begun work. AbortPreparedWorld
checks this before any family abort or resetting prepared/source owners. The two
non-presented/independent-target fallback paths also reject false before resets.
Resource clearing checks the guard before modifying caches. Successful deferred
acceptance releases the Session capture because the runtime ticket owns its exact
frame graph; successful synchronous retirement and safe abort release it too.

Validation uses actual shield cancellation + texture registry/history under
ASan/UBSan: 100 exact/absent/wrong-pointer/replay cycles, newer-owner preservation
and a partial capture. Structural assertions check actual-Begin guard before
rollback and before SceneBridge abort/reset effects. This is not a simulated GPU
success test. Full SceneBridge host syntax and cube rollback regression are also
checked. No Objective-C++ compile or actual Metal runtime validation.

Remaining: mutable source-family and backend cache ownership, dependency-safe
rollback of younger versions, and ordered controls. This serial ownership change
does not enable lookahead or turn all world publication into an atomic snapshot.

Final results: actual shield suite passes all 100 cycles with backing/storage
restoration and monotonic registry-version assertions; cube regression passes;
full SceneBridge host syntax exits 0. Independent reviewer rebuilt sanitizer
suite and returned GO, including failed-Begin retention and guard-before-reset.
