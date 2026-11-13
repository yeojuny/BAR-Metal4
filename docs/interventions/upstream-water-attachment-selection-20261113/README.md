# Complete water attachment selection

The six reflection/refraction and pre-water color/depth attachments are selected
into a draw-owned bundle before preparation. Allocation and retention failures no
longer overwrite individual live cache fields with nil or publish a partial new
water set. Compatibility checks include device and required usage as well as
extent and format. Unrequested views retain their previous selection, matching
existing serial semantics rather than inventing a producer for an absent view.

Water view encoding, dynamic/legacy/bump-water preparation, and pre-water copy
encoders use the exact selected bundle instead of looking up the mutable latest
cache again. The existing runtime lifetime graph pins the bundle before Begin.
Only after its retention succeeds are all six cache aliases published together.
Existing controls clear the same fields and existing retirement barriers remain.

This is selected-owner/failure-boundary work, NOT physical storage isolation:
compatible textures still reuse serial scratch storage. Distinct bounded slots
and dependency-safe reuse remain prerequisites for overlapping frames. No CPU
lookahead, FPS improvement or native validation is claimed.

Validation: structural checks of all six selection/publication fields, downstream
routing and pre-Begin lifetime order; existing actual gate/control sanitizer tests.
The new Objective-C++ allocation path has not been compiled/executed on Metal, so
native allocation-failure execution remains unverified. Residency side effects
are still governed by RuntimeLease failure handling, not rolled back here.

Independent review: GO; independently reran gate/control sanitizer regressions.
Later scene-color retention or resource commit can still fail after a complete
set has been published; this change does not roll back every later Draw failure.
