# Runtime / worker candidate, separate intervention

Experimental flags (process-latched):

- `BAR_NATIVE_INTERVENTION_TWO_INFLIGHT=1`: three independent command allocator/residency slots; at most two submitted tickets. Oldest ticket retires only after both shared event and commit feedback. Dependency-specific prior-read checks still drain the queue. A successful second queued ticket logs `[NativeInterventionTwoInFlight]`; flag presence alone does not prove this path is reached in a given scene.
- `BAR_NATIVE_INTERVENTION_RENDER_WORKER=1`: real Draw preparation/encoding/submission runs on a dedicated serial worker for ordinary frames. Main publishes an immutable surface snapshot and retained layer, then waits. SDL/AppKit mutation, teardown, readback/continuation, arbitrary source callbacks stay on main. Deferred scene receipts execute on main after the worker returns.

**The worker currently has `main_wait=1`, `simulation_overlap=0`.** This is actual thread migration, not the proposed full asynchronous simulation/render pipeline. Full next-frame simulation overlap remains unresolved because capture paths call into shared renderer caches and Lua/UI recorder retirement mutates main-thread state. Those accesses must be separated/versioned before removing the wait.

The queue preserves poisoned-frame ownership and refuses allocator reuse until retirement. A later GPU error keeps every outstanding ticket pinned. Detach and cleanup drain all queued tickets. The SceneBridge receipt container is bounded to the two-ticket runtime contract rather than one optional frame.

Persistent-residency integration uses explicit `AddPersistentAllocation` only. Immutable published snapshots are retained before the dynamic residency worker starts; every command binds the snapshot's committed sets. Cleanup releases the registry only after the all-pending completion gate. New helper and eligibility call sites are documented by the residency intervention.

Portable worker test:

```
c++ -std=c++17 -Wall -Wextra -Werror -pedantic -pthread -Iengine/rts \
 docs/interventions/native-perf-20261112/tests/render-worker.cpp -o /tmp/render-worker
/tmp/render-worker
```

Passes thread identity, 1,000 sequential invocations, propagated exception/recovery, void invocation and recursive-call rejection/recovery. This is not Metal validation. Native compilation, queue lifetime failures, resize/close, profile isolation, source-image parity and actual throughput must still be checked.
