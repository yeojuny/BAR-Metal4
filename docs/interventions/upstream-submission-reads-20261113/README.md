# Exact external-image submission read routing

Bloom, SSAO, deferred color and shield depth external image consumers now capture
an explicit renderer invocation's submission identity instead of consulting the
mutable `shieldPending` publication. The serial RAII scope spans preparation and
encoding and clears on normal return or exception. Closures retain the identity
by value, so clearing the scope cannot change a prepared read's producer checks.
Zero identities and overlapping scopes are rejected without replacing the old
binding. Outside an invocation the binding is zero, not the latest capture.

The four actual callers are world DrawPrepared, independent target readback,
maintenance, and screen continuation. Each binds its own actual submission;
continuations must use already-retired world outputs rather than borrowing the
world prefix identity for their new command submission. Existing producer
readability/initialization checks remain unchanged.

Validation: production helper under ASan/UBSan, 1,000 normal/exception/nested
rejection/retained-identity cycles; structural checks cover four call sites and
world binding before preparation. Existing 3,000 Begin-receipt worker regression
cases pass. Leak detection disabled because the host runs under ptrace.

Limit: no Objective-C++ compile or actual Metal run in this cloud environment.
This is serial exact-routing infrastructure, not full renderer exclusivity,
shield rollback ownership, mutable cache isolation or CPU lookahead enablement.

Independent review: GO. Rebuilt production helper sanitizer fixture, checked
world preparation ordering, all four provider call paths, captured-by-value
readiness closures, and continuation delegation before the world scope.
