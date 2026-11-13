# P3/P4 prerequisite: request-owned named particle atlas preparation

Named source atlas preparation no longer stores its consuming roster and ID in
WorldParticlesGPU::Impl's sourceAtlases/sourceSubmission pair. A RuntimeLease-owned
ParticleAtlasPreparationJournal retains the ordered shared Atlas owners (including
immutable source publication and strong texture/view resources). AtlasImage,
EncodeAtlasImages and sync/detached completion consume only that exact lease's
journal. Their explicit submission must match the lease's once-bound source ID.

The previous request journal is retained only for Prepare's transition checks and
same-submission roster recovery. Same current request/ID preparation is idempotent.
A distinct journal with the same ID inherits the same ordered roster without a new
encoded transition check, preserving the old same-submission fast path. On that
new-lease path only, GetAtlas rechecks exact cached identity, device/retirement and
registers texture/view residency before the journal/previous-owner publication.
A residency failure leaves the prior journal authoritative and new roster unbound.
Same-lease idempotent Prepare performs no new residency work. A different ID still
rejects every uninitialized previously encoded atlas before changing current state.
Returning A→B→A does not bypass B's unretired guard merely because A remembers its
old ID. Unencoded failed preparation is discarded at the next different request,
as before; it is not replayed implicitly. Failed transition leaves previous owner
and roster intact. Consumers do not fall back to the latest journal: their list
ownership is now explicit. This is not permission to enter the renderer concurrently.

All three preparation/encoding routes, two sync completion routes and one detached
completion route pass the actual RuntimeLease. The persistent atlas allocation
cache, GetAtlas's prior-producer drain/device/residency rules, image validation,
readability callbacks, EncodeAtlasMipmaps and CompleteAtlasMipmaps are unchanged.
Lists are independent but the exact Atlas producer state remains intentionally
shared under the existing gates. Encode still rejects a different unretired producer.

TakeCompletion moves that request's full atlas roster into its callback; clearing a
later request cannot alter that callback. Empty sync completion still does nothing,
while empty Take still validates and clears its preparation ID. Existing sequential
CompleteAtlasMipmaps semantics are preserved: if a later entry fails validation,
earlier entries may already be marked initialized. This is not transactional rollback
and tests explicitly preserve it; existing runtime failure/quarantine handling remains.

Validation: actual Atlas state, preparation journal, transition wrapper and completion
function are extracted and compiled C++23 -Wall/-Wextra/-Werror with ASan/UBSan.
1,000 A/B and A/B/A cases cover ordered dedup, same-ID idempotence/recovery, unretired
transition rejection, request-list independence, failed/unencoded preparation, wrong
IDs, empty/partial completion and full-owner callback lifetime. Source hashes freeze
the complete GPU mip producer/retirement, GetAtlas and image callback bodies. All
production call sites are audited; lease-identity and texture-journal regressions pass.

No native Metal execution, full app build, visual or performance claim. This removes
one owner dependency toward P5; shared caches, remaining providers and runtime gates
still prevent a claim of general concurrent preparation or full P5 completion.
