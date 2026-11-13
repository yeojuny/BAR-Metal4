# P5 prerequisite: owned synchronous overlay fallback

- [x] Bind the actual selected command to its retained immutable frame owner.
- [x] Copy the exact mapped viewport and logical/physical target extents/samples.
- [x] Own generated mesh or the original exception in a one-shot invocation.
- [x] Route production fallback through Execute/Take at its original position.
- [ ] Prove transitive typed/TLS eligibility before any worker execution.
- [ ] Integrate cancellation/join with an actual asynchronous owner, if justified.
- [ ] Native compilation, parity and useful active-work overlap measurements.

Only the existing fallback after Find/TryBuild/Borrow selection uses this payload.
SourceLines, cached geometry, exact packets and borrowed geometry do not create it.
The constructor uses an aliasing shared_ptr from selection.Owner() to the exact
command reference returned by selection.At. It retains frame lifetime without
copying command buffers or performing another metrics-mutating selection lookup.
No cache/provider/RuntimeLease is stored. Constructor copies and alias ownership
require no heap allocation or new geometry validation.

Each immovable invocation stores one immutable source alias and target. Execute
runs the original BuildNativeWorldDrawGeometry once. Take consumes its mesh once,
or rethrows the same exception before later timing/statistics/GPU preparation.
There is no reusable lookup key, cross-invocation cache or comparison of floating
viewports; NaN and signed-zero bits reach original geometry validation unchanged.
The same unique request object binds computation and consumption, so no result
from a different source or target can be supplied to Take.

Production remains synchronous: Execute is immediately followed by Take in the
original fallback branch. This is an actual source ownership/error boundary, not
an asynchronous worker implementation or a claimed optimization. Control calls
are not thread-safe; an external executor must join before Take or destruction.
No executor, cancellation or premature owner release is introduced here.

A retained outer frame does not prove independent typed validators worker-safe.
General polygon conversion can also update thread-local producer diagnostics.
These remain explicit eligibility/attribution gates, and the existing surface
and capture drains remain unchanged. Full application build is still deferred.

Validation: the production payload header and real command/frame data types are
compiled with a deliberately mocked raster builder. ASan/UBSan cover retained
alias lifetime, allocation-free alias/payload construction, exact command pointer
and viewport bits (NaN payload/signed zero), mesh move, original exception types,
and duplicate execution/consumption rejection. An explicitly joined helper-thread
exercise checks this payload's basic transfer discipline, not eligibility of real
typed production geometry. The existing extracted selection regression now uses
the actual payload with mock raster/cache/packet/borrow operations and preserves
all branch short-circuits, failure stops and exact target forwarding. Independent
review and reruns passed. No actual raster/Metal/full-app result is claimed.
