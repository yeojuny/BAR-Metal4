# Renderer-owned worker and ordered water GPU controls

The lazy worker belongs to the renderer Impl instead of a draw-local static.
It joins before provider teardown and GPU cache destruction. Existing external
serialization of move/destruction is still required.

After worker creation, Bump/Basic/Dynamic/Legacy water GPU resource clears uses the same synchronous
executor as world dispatch. Source reset and completion/receipt delivery remain
on main. Main first drains the runtime, then reserves renderer ownership, releases
its gate and invokes the control. The worker reacquires via exact handoff token
and uses a new cleanup-lease mode requiring an empty pending-submission queue.
That mode rejects before any cleanup side effects instead of silently moving
captured source callbacks to the worker. No artificial surface snapshot or window
refresh was added for control authorization.

Before worker creation (or existing pinned-worker caller), prior direct cleanup
behavior remains. Only water-family resets are routed by this checkpoint. Dynamic retains its
live-prepared-transaction check before attachment mutation. General
controls, source registry changes and asynchronous queues are not enabled.

Actual gate/executor ASan+UBSan protocol tests cover 1000 draw/main-retirement/
same-worker-control sequences and exception recovery; worker allocation-fault
and gate tests cover admission failure/unwind. Runtime cleanup and native clear
wiring are structurally checked, not executed as Metal code. Full SceneBridge
C++ syntax remains checked separately. No app build/runtime/FPS claim is made.
