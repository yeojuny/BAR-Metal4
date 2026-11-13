# T4-A: original terrain ABI slot seam

This is a bounded T4 substep, not completion of typed GPU block packing.

- The exact supported original SMF ABI has 44 named slots. Linked sampler profiles retain enum slots; GPU scalar, matrix and sampler readers use enum slots rather than string-map lookups.
- Source publication builds a pointer index in a single ordered merge, with no allocation or value validation. Unrecognized/inactive names remain untouched in the original map. Re-publication of an already indexed owner is a no-op.
- Index pointers belong to the snapshot's own map. Copy/assignment reset the index; the source writer invalidates it before a private dirty update. No pointer refers into a previous owner. Imported/unsealed snapshots retain a map-lookup fallback.
- Missing input, shape, type, nonfinite and transpose checks still occur in the same GPU reader calls and order. Target/image transport, camera/clip/fog, inverse computation and shader/GPU bytes are not cached or changed.

Validation: production writer translation unit, production snapshot and GPU reader extraction with SIMD storage stand-ins, ASan/UBSan. No full application build, Objective-C++/Metal compilation, rendering parity or FPS measurement has been performed for this change. The fixed index adds 44 pointers plus bookkeeping per snapshot; benefit and retained-memory cost need the final Mac run. Do not claim a measured speedup.

Still required for full T4: a validated typed base/packing plan that reuses converted source values without moving failures ahead of their original consumption boundary.
