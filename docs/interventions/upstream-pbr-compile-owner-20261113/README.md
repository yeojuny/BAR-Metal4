# Persistent PBR compile callback ownership

The Lua compile service callback outlives the ConnectPBRSourceContext stack guard.
It now captures the stable renderer access gate and acquires it before entering
RuntimeLease. Unsupported foreign/reserved ownership fails before waiting on the
runtime or running retirement callbacks. Existing same-owner recursion remains
valid. The renderer still shuts down source services before destroying its owner.

This is an ownership/order guard, not evidence of an existing pipeline-map race:
Compile updates the compiler cache, while Prepare consumes pipelineOwner already
captured in each immutable program. RuntimeLease already serializes their runtime
access. This does not enable concurrent Lua capture or asynchronous compilation.

Validation: actual gate/control sanitizer regression and structural verification
of persistent callback gate-before-runtime order. Objective-C++ compilation and
live Lua/Metal callback execution remain unverified.

Next identified source handoff boundary: worker-side UI DescribeBinding still
accesses live context/service state and a mutable name/current-descriptor fallback.
It must be replaced by exact captured descriptor resolution before capture overlap;
this callback guard does not solve that separate registry/version dependency.
