# P6 TerrainInfo outer receipt reuse

The original handoff identifies repeated TerrainInfo resolution wrappers even
though InfoResource already caches the GPU producer plan. This first implementation
shares only exact outer receipt owners within one serialized RuntimeLease. It does
not remove validation, resource registration, sampler retention, GPU commands,
barriers, or per-consumer readability checks.

The bounded 32-entry lease-local cache keys the complete immutable object identity,
resource identity, submission, actual sampler, validation mode, and resource's
registered/encoding/completed/dependencies-encoded/pending state. Exact object
identity retains its content/storage/sampler semantics; different immutable object
snapshots miss even if a raw GL name matches. Runtime/device ownership is confined
to the one lease and existing CheckTexture validation still runs before lookup.
State changes and different submissions miss. No cross-lease or backing-owned
cache exists, preventing a backing→receipt→backing ownership cycle.

Cache entries publish only after validation/Register/Retain and successful creation
of the normal or prior-ordered callbacks. The first prior edge stays registered in
the existing lease. A hit retains that exact owner and its original dynamic
readability/completion checks. Full cache or allocation failure falls back without
changing the successfully resolved image. Returned CUSSourceImage still copies
std::function values and may allocate; this is owner reuse, NOT zero allocation.
Neither reduced GPU work nor improved performance has been measured.

Validation: actual extracted cache with modeled image/object adapters, 1000 cycles,
all 10 key changes, foreign lease, capacity, dynamic readability, failed insertion
and retry, cached-image copy failure, strong-owner release. Actual InfoResource
block is separately extracted with modeled RuntimeLease/PreparedWorldInfo to test
prior-producer identity, wrong completion rejection, one-time successful encoding
and failed-encode quarantine. ASan/UBSan. Source-wiring checks retain validations,
registration, sampling and predecessor order. This is not native Metal compilation
or full integrated producer/lifetime validation. No full app build/run performed.
Independent review: GO; expanded fixture separately passed under sanitizers.
Forced allocation failure exercises callback-target copy, not every runtime
allocation site or native GPU failure.
