# Exact initial UI allocation descriptor

Initial Lua UI allocations now publish a retained descriptor on the exact immutable
texture object. The backing is explicitly AllocationOnly and storage remains
AllocatedUndefined. No pixel data, clear, producer completion or source-draw frame
is invented. The allocation metadata is captured after allocation/sampling state;
the declared token then points at this exact publication. The entry's content
image remains unset so its first real producer publication is not deduplicated.

DescribeBinding no longer falls back to a mutable logical-name cell's current
image. Old tokens retain their own initial metadata across a later producer,
sampler changes, deletion and name reuse. Captured producer backings continue to
use the existing exact-image path. Missing retained owners now fail rather than
selecting unrelated later state.

The production helper and real registry/history test run 1000 color/depth RTT
cases under ASan/UBSan, covering undefined semantics, sampler updates, later
producer publication and delete/recreate. Invalid metadata does not publish.
Create/resolver wiring has structural checks; the full NativeUITextureHistory.cpp
translation unit passed host C++ syntax. This is not full application/native GPU
validation or a fully instantiated UI observer integration test.

Remaining source-seal boundary: DescribeBinding still checks live context and
consults the external descriptor service. Eliminating name fallback is a necessary
version-identity step, not permission for concurrent source capture.

Independent review checked actual Create callers and passed the helper/registry
sanitizer fixture. The helper does not claim whole-Create rollback: existing
name/allocation setup may remain if a later Create allocation/emplace fails.

## Follow-up: captured resolver service

DrawDispatch now captures the UI resolver on main before releasing renderer
ownership. The resolver owns a copy of the external descriptor service and
resolves only retained bindings; its static implementation does not access the
live context, registry or name map. Both ordinary-model and StartBox paths use
this captured resolver on the worker. A scope guard restores previous resolver
state on return/exception and nested synchronous entry. Calls outside a draw may
use the original live resolver only on the actual main thread. Detached diagnostic
contexts do not require a UI service unless a binding actually needs resolution.

The configured external service is stateless and derives descriptors from retained
typed source objects; completion/readability state remains governed by existing
submission dependencies. This freezes service configuration, not all execution
state. It does not grant capture overlap or remove dependency/version gates.

The actual pure retained resolver is exercised by the registry fixture before and
after name replacement. Full NativeUITextureHistory and SceneBridge C++ syntax
checks pass; worker capture, routing and exception restoration are structural
checks. Objective-C++ execution and instantiated observer integration remain
unverified. No full application build was performed.

Additional actual CaptureResolver lifecycle test passed: install provider A,
capture, replace with B, reset and destroy the real history object, renew the
registry context, then resolve the old retained token. Only A is invoked. The
fixture links the actual observer/recorder/postprocess code, without stubs, and
runs under ASan/UBSan. Existing shared_ptr atomic deprecation warnings remain.
This supersedes the earlier missing instantiated capture-service coverage, but
not full original Lua Create event/GPU integration validation.
The context-renewal fixture proves CPU descriptor lifetime only; it does not
authorize replay after native context destruction or bypass GPU context checks.
