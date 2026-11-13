# P6 completed Cube read receipts

This is a bounded read-only subset of Cube, not general producer graph sharing.
`ResolveNativeCubeTexture` now shares an outer prepared receipt only inside one
RuntimeLease, for the exact immutable texture object, resource, actual sampler and
submission. Eligibility requires a non-null supplied sampler, completed content,
no retained prior, non-allocation-only content, and an already uploaded Resource
for Specular. Eligibility is evaluated before original preparation and again on
every resolution; a first upload/producer cannot be promoted during that call.

The repeated route exists in `Metal4TerrainSourceGPU.mm`: forward terrain with
`SMF_SKY_REFLECTIONS` resolves its Cube sampler for each source square through
`provider(lease,binding,unit,sampler)`. Its sampler cache supplies stable actual
sampler identity. `Metal4WorldRenderer::Impl::OriginalTerrainImage` dispatches that
binding to this resolver. Eligible stable completed snapshots can therefore share
wrappers across squares. Actual hit rates and performance are not measured.

Binding/storage/submission validation remains on every resolution. A cache hit
still independently pins CubeStorage, validates actualSampler's device, retains
that sampler, and registers both color and optional depth allocations in original
order. A miss executes the unchanged original PrepareNativeCubeTexture path.
Cache publication occurs only after successful preparation and construction of
all callbacks and the dependency query. The 32-entry cache falls back when full
or optional allocation fails. Copying std::function-bearing receipts may still
allocate/throw, as existing TerrainInfo/Shading receipt caches do.

No changes are made to PrepareNativeCubeTexture, recursive prior preparation,
face/mip copy policy, FinishReflection, borrowed-predecessor ordering, barriers or
GPU pending depth. Environment face writers do not use this cache. Completed,
prior-free reads have no remaining producer/prior transition: sharing DidComplete's
local idempotence does not suppress any unfinished producer completion. The cached
receipt owns its PreparedCubeTexture; dependencyQuery160.context points to that
same owner and remains live after cache/lease destruction while a consumer keeps
its receipt. Strong cache ownership can prolong an exact completed resource to
lease destruction; native memory/retention measurements remain required.

Validation: extracted actual cache/eligibility/Readable code with 1000 lifetime
and query-context cycles, all four key distinctions, foreign lease, capacity,
failed insertion/retry and cached-copy failure. Extracted actual
InspectDependencies160, Image and DidComplete methods additionally check repeated
completed-read receipts preserve original producer flags, borrowed completion
still rejects an uncompleted predecessor, and query context survives via its exact
owner. Metal storage/lease adapters are modeled, not native calls. Production
wiring assertions cover hit retention/registration and unchanged writer routing.
Wrong-device and residency/pin calls are checked structurally, not exercised
against a real Metal device in this fixture. Independent review and separate
ASan/UBSan rerun passed. No full application build, Metal runtime run, visual validation
or measured speedup is claimed. Broad uncompleted Cube receipt reuse is deferred.
