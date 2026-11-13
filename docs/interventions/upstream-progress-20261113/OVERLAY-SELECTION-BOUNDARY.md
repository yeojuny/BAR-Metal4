# P5 prerequisite: executable overlay selection boundary

## Checklist
- [x] Audit original lease-local cache, exact-line, borrow and fallback order.
- [x] Extract the actual selection block into a host boundary regression.
- [x] Check source gates for mapped target order and mutable selection/cache access.
- [ ] Introduce an immutable selection receipt (not implemented here).
- [ ] Separate any work onto an asynchronous worker (not implemented here).
- [ ] Native integration, visual parity and measured active-work overlap.

This change adds tests and documentation only. It does not change production
selection, caching, providers, barriers, drain/default behavior or worker launch.
The existing block queries each choice once; no duplicate-query saving is claimed.

The fixture extracts WorldOverlayGPU::Prepare from cachedLines selection through
BuildNativeWorldDrawGeometry, leaving the actual short-circuit expressions and
statement order intact. It substitutes mock Find, TryBuild, Borrow and Build
operations and representative source/mesh structures. ASan/UBSan assertions cover:

- SourceLines skips all four operations and preserves fullViewport=true.
- A retained cache hit skips exact packet, borrow and fallback construction.
- Exact packet selection skips borrow/build; fullViewport remains samples>1.
- Borrow selection skips fallback; fallback runs Find -> Try -> Borrow -> Build.
- Each thrown operation stops later calls and propagates the same exception.
- The exact mapped viewport, explicit width/height/samples, invocation lease and
  selection ordinal are forwarded without conversion or substitution.
- A selected shared buffer owner outlives eviction of its mock cache entry.

These are boundary tests with mocks, not native cache, GPU or async correctness
proofs. Structural source assertions verify mapping follows command validation
and rectangle override, precedes selection, and keeps zero/offscreen early-outs.
They also verify Find performs ResetContext before lookup, completion acquire
before returning a cached owner, and context/device reset mutates cache storage.

## Remaining ownership boundary

NativeWorldDrawSelection167::At is const but increments shared State.metrics.
A retained const selection therefore is not automatically worker-safe. A future
receipt must resolve the exact frame owner and command index on the original
thread, then use immutable owner data rather than concurrent selection.At calls.

A minimal future receipt would retain frame owner, frame/serial/sequence/stage,
selection ordinal, original rectangle, mapped viewport, all four logical/render
extents and samples, plus its selected kind. Cached results retain the returned
shared buffer owner; exact packets move their computed value. Borrow/fallback
retain the command owner. It must not re-run Find/TryBuild/Borrow at consumption.
Lease-dependent Store/StoreGenerated and GPU allocation remain on the renderer.

TryBuild is not just an eligibility test: it can clip, allocate and build a line
packet before rejecting by budget/threshold. Moving it earlier or repeating it
would change workload and failure timing. The actual worker boundary therefore
needs its own error/cancellation/target-identity design, not a mechanical wrapping
of this block. The current serial code is not shown to have a race; the mutable
metrics and cache become hazards only if concurrency is introduced carelessly.

Follow-up: [owned synchronous fallback](OWNED-OVERLAY-FALLBACK.md) now routes the
final fallback through an actual retained source/target/result payload. The
regression uses that payload while keeping raster operations mocked. Full selection
receipt and asynchronous transfer remain unimplemented; the original audit above
predates this narrowly scoped synchronous change.
