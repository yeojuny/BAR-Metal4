# P6 TerrainShading exact outer receipt reuse

The next source family is TerrainShading, not Cube. Square already caches physical
views and has no callbacks; Cube adds recursive face-write and borrowed-producer
contracts needing separate evidence. Shading still creates a new owner and three
callbacks for equivalent reads despite a cached producer Resource.

This bounded 32-entry RuntimeLease-local cache shares an already resolved outer
receipt only for the same immutable object/source, Resource, prior source/resource,
plan, actual sampler, producer instance, submission, prepared/encoding/encoded
identities, validation and completed state. Different versions, routes or states
miss. Lease confinement supplies runtime/device identity; device/storage checks
still execute before lookup. Existing Prepare/Register (including recursive prior
registration), validation and Retain calls remain per-resolution.

Each consumer still calls the original Readable callback; shared owner identity
lets existing downstream owner dedup avoid duplicate dependency/completion wrapper
visits. Original Encode/Complete semantics are unchanged, including prior-pixel
copy barriers, partial-write preservation, failed-encode quarantine, recursive
completion and RetirePrior. Cache stores only after all callbacks are constructed.
No cache is attached to a backing. A bounded lease can pin its exact prior/plan
until lease destruction; GPU-completion graphs still own their returned receipts.
Capacity/optional allocation failure falls back. Copying a cached CUSSourceImage
can still allocate std::function storage and throw; no zero-allocation or measured
speedup is claimed. No GPU queue, barrier or alias policy changes.

Validation: actual extracted cache, 1000 cycles across 14 key distinctions,
foreign lease, capacity, dynamic readability, insertion/cached-copy failures and
owner release. Actual Resource/Readable/Complete blocks extracted with modeled
Metal/PreparedMiniMap dependencies: incomplete encode and wrong-prior submission
cannot retire; successful recursive completion publishes exactly once and drops
prior/plan owners. Actual Metal Encode is unchanged and not executed here.
ASan/UBSan and production wiring assertions. Full native build/runtime is deferred.

P5 remains a default-off, restricted Begin-ready encoding-tail pilot. Its safety
surface drain limits possible overlap to the capture prefix. Exact draw and thread
CPU timing are implemented, but native compilation, visual parity, positive real
active-work overlap, throughput/tails and latency evidence remain outstanding.
Independent review: GO after a separate sanitizer run. The 32-entry limit bounds
receipts, not the total number of nodes in retained recursive prior graphs. Pinning
prior/plan until lease destruction can temporarily extend memory lifetimes and
requires native retention measurement before claiming an overall performance win.
