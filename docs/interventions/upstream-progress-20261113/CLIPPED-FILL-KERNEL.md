# Production pure clipped-fill geometry kernel

## Checklist
- [x] Extract actual Fill/Fill inherited-plane clipping into a production function.
- [x] Keep caller validation, topology decisions and diagnostics at original sites.
- [x] Link the actual production CPP into sanitizer/reference regression.
- [x] Check byte results and allocation/failure ordering against old fill loop.
- [ ] Eligibility-gated worker submission, cancellation and result consumption.
- [ ] Native integrated validation and active-work overlap measurements.

BuildValidatedNativeWorldClippedFill now receives immutable vertex/index spans and
a clip-plane value, returning an owned NativeWorldRasterMesh. The actual
BuildGeometry Fill/Fill inherited-plane branch calls it. This is an executing
production boundary, not an unused eligibility predicate.

The function requires already validated finite attributes/plane, an index count
multiple of three (including zero), and all indices in bounds. The caller must
retain immutable span owners until completion. It has no provider, lease,
selection iterator, recorder, diagnostic or TLS dependency. It is not itself a
source-owner receipt or a scheduler. An unaudited typed command is not thereby
made eligible for worker execution.

The exact old operations remain: each indexed triangle constructs its temporary
three-vertex vector; ClipUserPolygon retains its reserve and clipping arithmetic;
accepted polygons append vertices then fan-triangulated indices in the original
order. Mix/PlaneDistance/ClipUserPolygon are unchanged. No reserve, new validation,
rounding adjustment or overflow fix is bundled. Empty input returns empty output,
as the old loop did. Output remains fullViewport=false with no point disks.

BuildNativeWorldDrawGeometry still rejects GPU-matrix input before calling
BuildGeometry. Its command validation, extent/sample/viewport checks, finite-plane
check, topology and sprite rules precede the kernel exactly as before. The
no-inherited-plane fast return and all non-fill/line/point logic remain unchanged.

run_clipped_world_fill_test.py links the actual NativeWorldDrawGeometry.cpp using
function/data sections and linker garbage collection to avoid unrelated renderer
entrypoints. It is not a mock kernel. The old fill loop is an independent reference
with extracted original helper bodies, protected by fixed hashes; the unchanged
caller validation prefix is hash-checked too. ASan/UBSan covers 4000 randomized
finite geometry/plane cases, comparing vertex bytes, index order and output flags;
explicit all-in/out/on-plane, duplicate-corner, zero-area, reversed winding,
perspective-w and empty cases are included. Allocation counts and injected failure
at each allocation boundary match the old loop for a clipped two-triangle fixture.

Only host kernel correctness and source-boundary preservation are claimed. There
is no worker launch, drain removal, default change, app build, Metal execution or
measured parallel speedup in this step.
