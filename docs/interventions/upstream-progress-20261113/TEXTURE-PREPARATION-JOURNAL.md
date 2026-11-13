# P3/P4 prerequisite: request-owned world texture preparation journal

The renderer-wide raw-pointer `pendingMipTextures` list has been replaced by a
journal owned by the actual RuntimeLease. Each entry owns the original WorldTexture,
strong destination/sample textures and shared upload status. Providers already
receive that lease, so registration, encoding and retirement now select the exact
request's journal instead of an ambient renderer list. Entry order and first-owner
deduplication are preserved. Reset or destruction of request A cannot clear B's list.

All four encode sites, three synchronous completion sites and the single detached
completion site pass the lease explicitly. The original producer loop is unchanged
apart from the owning iteration: copy supplied levels, blit barrier, optional mip
generation, blit barrier, optimize, queue visibility barrier, end, encoded stamp.
Original stale encoded-submission rejection and skip-ready behavior remain.
Synchronous Complete retains its stricter all-entry submission validation; detached
TakeCompletion retains its existing ready-entry exclusion. No new cross-request
encoding permission or alternate completion semantics is introduced.

Interrupted preparation recovery is deliberately preserved. The renderer retains
an explicit recovery journal owner, but encode/retirement never consults it. The
first lookup on an uninitialized lease copies the prior ordered recovery entries,
including when that lookup's texture is already ready. Maintenance also explicitly
inherits that exact journal before preparation and re-registers all destination,
view and staging allocations before Begin. Early maintenance return leaves recovery
untouched. Original clear points are explicit journal resets at the same positions,
including main's reset after postprocess PrepareFrameImages. No unordered cache scan
or inherited-entry reordering is used. Recovery copying is strong-exception-safe;
failed allocation does not replace the previous recovery owner.

Shared texture-cache and UploadState mutation still require the existing request
lock/access gate. Recovery selection also remains serialized. This separates a
transitive owner dependency; it does not make general Prepare concurrent or complete
P5. Main/app state, GPU pending limit, source credits and pilot defaults are unchanged.

Validation: actual journal, StartTexturePreparation and Texture lookup wiring are
extracted into an ASan/UBSan fixture. 1,000 two-request reset/recovery/completion
interleavings verify ready-only inheritance, ordered dedup, full entry ownership,
strict failed completion before any ready mutation, and async skip-ready behavior.
Structural audit covers every caller, maintenance residency/early return, original
reset placement and SHA-256 of the complete unchanged Metal producer-loop body.
This is source ownership verification, not execution of Metal commands. Native
Objective-C++ integration, visual parity and measured overlap remain unverified;
no whole-app build or execution was performed.
