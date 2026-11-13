# Executed MRT source cancellation guards

Color and depth resources expose a conservative serial-owner rollback query.
Unfulfilled backing is still cancellable; an unknown fulfilled resource refuses.
The actual Metal resource checks its retained producer's encoded/completed flags.
A nonzero submission identity is NOT execution evidence: it is assigned in
Prepare before Begin, and must not prevent legitimate pre-Begin rollback.

All changed outputs of an MRT phase (five colors, optional combined color, depth)
are checked before any registry mutation. The complete opaque pass batch is also
preflighted before reverse cancellation. The renderer's scene abort checks opaque,
ground and exact pending shield owners before starting other family cancellation.
A blocked exact shield cancellation returns false and keeps its pending owner.
Existing activeSourceSubmission rejection remains the failed-Begin safety barrier;
the new queries do not replace it and do not authorize concurrent capture/rollback.

Host tests run actual backend cancellation and registry code with CPU resource
substitutes; production Metal query wiring is structurally checked. They cover
unfulfilled and prepared rollback, every output independently encoded/completed,
unknown fulfilled resources, exact blocked shield owner retention, standalone
color protection and an executed earlier opaque pass with a pending later pass.
No Objective-C++ compilation or Metal execution is implied. No younger source
version graph, queued control scheduler, GPU storage renaming or lookahead is
implemented by this checkpoint.
