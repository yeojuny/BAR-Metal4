# Caller-thread seal for closed clipped-fill inputs

## Checklist
- [x] Factor the original geometry validation prefix without reordering it.
- [x] Integrate a narrow seal at the existing selected fallback site.
- [x] Route successfully sealed work to the actual pure clipped-fill kernel.
- [x] Test owner exclusions, seal state, actual prefix errors and kernel parity.
- [ ] Useful asynchronous interval, worker scheduling and bounded cancellation.
- [ ] Native integration, floating-point environment parity and performance.

The production fallback still runs after Find -> TryBuild -> Borrow decisions.
NativeOwnedWorldGeometry now receives SealIndependentClippedFillOnCaller at that
same site, immediately before its synchronous Execute/Take. No capture or query
has moved, and no drain, default setting or executor has changed.

The structural gate accepts only canonical/source Triangles, Fill/Fill modes,
inherited clip plane and Solid material. It rejects all independent command typed
owners, all five surface shader inputs, both textures and minimapBackground.
It neither dereferences those owners nor validates rejected input. Rejected work
continues through the old generic builder, including its original GPU-matrix
rejection. Positive inputs cannot be GPU matrices; Fill/Fill makes the original
polygon-mode sprite condition true. Postprocessing in the generic wrapper only
acts on point/line/non-fill geometry and cannot affect this positive path.

Eligible work invokes the shared original prefix once: command validator, raster
extent/sample checks, finite viewport, then finite inherited plane. BuildGeometry
uses the same factored prefix; its remaining logic and wrapper rejection are
unchanged. A successful seal copies the plane into private state and retains the
exact source/frame alias and target values. Execute uses the closed kernel only
for that seal; otherwise it invokes the old builder. HasIndependentClippedFillSeal
reports this distinction and must not be inferred merely from payload ownership.

A failed seal stays unsealed and cannot be retried on that task. Validation now
throws from Seal at the original demand fallback, rather than being temporarily
captured by Execute and immediately rethrown by Take. It reaches the same renderer
exception boundary before later metrics/upload work. No new source-validation pass
is added: the positive route replaces the generic builder's prefix, not the earlier
existing overlay validation. Allocation-free alias/task construction and the
structural gate do not introduce earlier geometry allocations.

Validation:
- Actual owned header with explicit mocked builder/prefix/kernel checks all owner
  exclusions, GPU enums, positive dispatch, once-only seal/state, failed seal,
  source lifetime, bitwise target forwarding and exception propagation.
- Actual NativeWorldDrawGeometry.cpp prefix linked with only the command validator
  mocked checks source-first, extent, viewport and plane error ordering and failed
  seal rejection. This does not claim full command validator execution.
- Actual production CPP clipping kernel still passes 4000 byte-identical reference
  cases and allocation failure parity. The old caller-prefix frozen hash remains:
  the runner expands the shared prefix call back to its original text before
  comparison instead of accepting an arbitrary new baseline.
- Actual production overlay decision block is extracted with mock operations;
  caller seal precedes immediate Execute/Take and all prior short-circuits remain.

This is a live synchronous ownership/validation prerequisite, not asynchronous
completion. Generic typed work is not approved for a worker by this change. Future
execution must use the positive seal, retained immutable owner, compatible numeric
environment and a real cancellation/join discipline. No app/Metal execution or
parallel performance gain is claimed.
