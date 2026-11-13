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

## Follow-up: bounded per-role physical pairs

Reflection, refraction and pre-water copy now each use the tested two-slot idle
pool. Every active role acquires an exact color/depth pair and pins it in the
selected bundle through runtime retirement. An externally pinned pair is never
reused. The three pools cannot substitute resources across roles; compatibility
includes device, extent, format, type, sample count, private storage and required
usage. Pre-copy color remains shader-write capable; planar color remains a render
target. Both allocations and retention complete before a pool entry publishes.
The complete visible selection still publishes only after all draw preparation.

Existing validation requires current producers for enabled planar roles. Every
Bump frame still acquires its pre-copy pair, preserving existing behavior even
when a particular copy is disabled. Preserved inactive raw aliases are not used
as substitutes for active producers. Family controls clear the corresponding
pools; static reset clears all three. Existing barriers remain for uncounted raw
aliases and other dependencies. Two slots per role bound the cache, not total
live images/bytes. More memory versus a single scratch set is possible; real
native allocation count and performance have not been measured. Earlier text
about unchanged physical reuse describes the prior selection-only checkpoint.

Production generic pool sanitizer tests remain applicable. Additional native
pair acquisition/pinning and family reset wiring are structural checks only;
Objective-C++ compilation and GPU allocation failure execution remain pending.
