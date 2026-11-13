# T4-B: immutable typed terrain source base

The source snapshot now retains successful typed float4/int4 conversions for the
44 original ABI slots and one transposed shadow matrix. `SealSlots` classifies
shape, variant, element count and finite-value eligibility without throwing.
Nonfinite detection uses exponent bits rather than floating-point arithmetic,
so even a signaling NaN does not raise a floating-point exception at publication.
Invalid, missing, unsealed or differently requested values use the original GPU
reader path, preserving its diagnostic and consumption point.

The writer reuses the previous snapshot's owned typed bytes after an exact map
copy plus a single named change. Only the changed slot is reclassified at seal.
A private lazy writer invalidates the changed slot before its next edit. Generic
snapshot copy/assignment intentionally does NOT trust the typed base; relink
and imported snapshots therefore reclassify all fields after their map edits.
Pointer slots are always rebuilt against the new map, never inherited.

GPU readers reuse the source base at the same source-consumption call sites.
They do not cache the final GPU block. Model-view/projection/inverse, target clip
convention, alpha/fog/user planes, resolved image types and sampler transport
remain late per-draw work. All source-validation failure ordering relative to
these dynamic checks is retained. No shader, allocation pool, submission order,
GPU barrier, pass, queue or frame scheduling changes are made.

Costs: host sizeof(snapshot) is 1424 bytes versus 432 before this step (+992).
A vector slot is 20 bytes, not 16 floats per slot; only the actual shadow matrix
gets a 64-byte matrix field. Typed bytes are copied across a source write, while
conversion/classification is reused for unchanged fields. This trades retained
memory/copy bandwidth for fewer repeated reader checks and conversions. It is
not a measured Metal performance win; final Mac testing must assess that trade.

Validation uses production snapshot/reader code with SIMD storage stand-ins:
2,000 randomized cases compare packed result bytes with the original unsealed
reader, including signed zero, transpose, integer payloads, component padding,
single-write reuse, previous-frame preservation, lazy invalidation, nonfinite
fallback and copy/assignment invalidation. Actual source writer/eager-boundary
and certificate fixtures are rerun. No full application build or Metal execution.
