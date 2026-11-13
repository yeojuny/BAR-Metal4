# Capture-time UI readiness without mutable GPU reads

Ordinary BoundImage capture consults the installed external image descriptor.
The prior descriptor accessed renderer-fulfilled shared resource pointers and
plain producer completion flags, despite already retaining an exact source
binding. The joined worker hid this race; it was not safe for CPU lookahead.

Six source families now expose synchronized readiness:
- UnitStencil, Bloom, SSAO: exact source producer's atomic completion witness.
- Map: one-shot acquired resource snapshot and atomic completion witness.
- DeferredColor and Shield: one-shot acquired resource snapshot and atomic
  completion on the shared original depth producer.

Resource fulfillment is a compare/exchange release publication that rejects
null/duplicate installation and never overwrites a retained version. Capture
uses only the virtual completion query, immutable source metadata and exact
binding. It does not inspect renderer resource contents. Atomic fields make
producer state noncopyable; the sole aggregate move construction was replaced
with initialization before publication. Existing GPU consumers retain their
same exact backing. Returning references inside fulfilled resources remains
valid because each backing permanently retains its installed resource.

Height upload is complete before its resource publication, as before. Normal
images still become complete at retirement. Allocation-only UnitStencil and
geometry bindings retain their existing independent-target behavior; allocation
is not reclassified as completed pixels. New source versions start incomplete;
rollback restores the old exact owner and never clears its readiness witness.

Only completion is source-readable. Submission identities, encoded state,
resource caches, cancellation and reset still require serialized ownership.
This does not authorize concurrent cancellation, unjoined work or P5 admission.

Validation: ASan/UBSan portable production backings/producers: 300 publication /
completion / duplicate / null transitions, 300 producer completion transitions,
and 100 ready-at-upload height versions. Another 100 publication polls have no
promise-based publication edge; 100 competing fulfillments require exactly one
winner and preserve old-ready/new-incomplete version separation. Publication is
thread-safe, not claimed lock-free. Descriptor/native GPU wiring is checked
structurally, not executed. Exact geometry rollback: 26 allocator failures + 46
successes, shield cancellation 100 cycles, map publication 11 failures + 71
successes and negative controls, renderer gate 1000 nested / 100 cross-thread
cycles passed. SceneBridge host C++ syntax passed. Native Objective-C++/Metal
compile, execution, visual parity and performance remain unverified; full app
build remains deferred by instruction.
