# P3/P4 prerequisite: request-owned screen mip queue

ScreenGPU::Impl no longer holds the producer queue directly. Each RuntimeLease has
an opaque capsule of ScreenMipPreparationJournal owners, separately keyed by retained
typed ScreenGPU identity. A new ScreenGPU/ClearResources identity cannot alias an old
one by address reuse, and two ScreenGPUs in one lease cannot mix their queues. No
nonzero source submission is required: diagnostic/loading standalone leases remain
valid. The six existing PrepareMipmaps routes and both Texture queue sites use the
same actual lease as their texture preparation.

The persistent per-texture initialized/queued flags are retained. Queue still skips
missing/initialized/already-queued state, appends once in original order, then sets
queued=true only after successful insertion. The original early guard also remains
before journal acquisition, avoiding new capsule allocation for already-ready or
non-mip textures. PrepareMipmaps registers every queued
texture allocation before moving the entire ordered vector into the plan; only then
sets queued=false. Residency failure leaves the complete queue/order/flags untouched.
There is no initialized-entry filter, changed producer count or changed GPU command.

Abandoned/standalone preparation recovery is explicit and exclusive. The ScreenGPU
retains a recovery journal, not the lease capsule. Capsule destruction marks its
journals inactive. A different request may move-adopt the entire pending queue only
once the former lease is inactive; a nonempty active request's queue is rejected,
not copied/stolen. An empty/extracted A cannot take B's nonempty active queue on
A→B→A reentry. Moves preserve full source/texture/state owners and queued flags.
The capsule is declared after RuntimeLease's request lock, so it becomes inactive
before lock destruction/unlock. Existing poisoned-runtime gates still prevent
recovery after unsafe GPU execution failures.

PreparedScreenMipmaps::EncodeDependencies and DidComplete are byte-for-byte
unchanged: the whole plan generates its mip chains with the original barriers,
then completion publishes initialized. Taking a plan already clears queued, not
completion. Consequently, dropping an unencoded plan permits normal later requeue,
and empty queues may coexist with an uninitialized extracted plan. This change
DOES NOT prove concurrent producers safe: there is still no encoded-submission
identity for these plans. Existing gates/pending depth/worker eligibility remain
unchanged; no overlap is enabled.

Validation: actual state/journal/capsule/recovery code compiled C++23 with
-Wall/-Wextra/-Werror and ASan/UBSan. 1,000 cases cover active-owner rejection,
inactive ordered adoption, A→B→A, midway residency failure, dedup/producer counts,
whole-plan preservation, abandoned-plan requeue, multiple GPU identities, capsule
before-lock destruction and source/image/identity lifetime. Structural checks cover
all six plan callers, both queue sites, no source-ID requirement, exact original
GPU producer/retirement hash and actual RuntimeLease member/destructor ordering.
Owned-screen packet, UI-resolver and world-texture journal regressions passed.
Native Metal execution, full app build and performance acceptance remain unverified.
