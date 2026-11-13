# P4 prerequisite: exact lease-owned provider submission identity

Screen/PiP external providers formerly read renderer-wide pbrSubmission or the
renderer-wide submissionReads current value. pbrSubmission remained set after
requests, and main set it later than early postprocess preparation. Those reads
could therefore select the previous request rather than the request whose lease
the provider actually received.

RuntimeLease now owns NativeSubmissionReadIdentity: initially zero, nonzero Bind
exactly once, including rejection of rebinding to the same value. Identity has no
copy/assignment/reset escape. Four source-request paths bind their exact declared
submission before provider preparation: independent readback prefix, maintenance,
screen continuation and main. Main binds immediately after lease construction,
before early postprocess preparation, not at the old late PBR marker.

All nine provider read sites use lease.SourceSubmission(). The complete provider
function is otherwise unchanged, including resource validation and readability
predicates. Deferred callbacks still capture the resulting integer by value.
Standalone/inspection leases remain zero: completed or allocation-only resources
retain their existing permitted rules, but an unretired resource cannot borrow a
stale renderer ID. There is no new fallback to the latest submitted identity.

The old four scoped bindings remain under the name submissionEntry strictly as
reentry guards; providers never consult them. In main this guard still starts
before RuntimeLease construction, preserving rejection around construction-time
GPU retirement. Existing guard reset-on-exception semantics remain. The separate
activeSourceSubmission rollback/publication owner is unchanged, as are access
gates, request lock, GPU ordering, pending depth and opt-in pilot defaults.

Tests compile the actual identity header and extracted RuntimeLease accessors as
C++23 with -Wall/-Wextra/-Werror and ASan/UBSan. 1,000 A/B lease cases cover independent
IDs, zero and repeated binding rejection, stable callback captures and standalone
zero; the original 1,000 normal/throwing/reentrant guard cases still run. Wiring
checks cover all four bind paths, nine provider reads, main's early-prepare timing,
and accessor uniqueness. A hash of the entire provider function verifies the only
provider change is the two ambient read expressions becoming exact lease reads.
The world texture preparation journal regression also passes.

This is source/header ownership validation, not Objective-C++/Metal runtime proof.
No full app build/run, visual/performance claim or general concurrent Prepare
permission is implied. Other renderer caches/providers still require serial entry.
