# P6 exact TerrainSource first-owner roster

TerrainSource previously traversed every prepared square/image and allocated a
fresh std::set in both EncodeDependencies and DidComplete. Prepare already seals
all draw/image vectors before either phase. It now builds one ordered roster of
(draw index, image index) pairs after the last append, then both phases traverse
that roster. Work is bounded by this prepared source's images, not a global cache.
This reduces two hot-phase censuses to one preparation census plus retained index
storage; it is not a zero-allocation or measured-speedup claim.

Identity remains exactly image.owner.get(), in original nested-loop order. First
occurrence wins even when its callback is empty or owner is null. Different raw
pointers sharing a control block stay distinct, as before. No texture aliasing or
resource state is inferred. Both callbacks remain live and are invoked at their
original phases; NoCommands is neither queried nor cached. Per-draw requireReadable
remains unchanged for every actual draw. No GPU queue, barrier or producer rule
changes. Existing encode-attempt, draw and retirement guards remain intact.

The roster stores indices, not addresses into movable vectors, and does not own
resources itself. The original draw images remain the only image owners. There
are no later image insertions/removals in PreparedTerrainSource. The temporary
roster must finish successfully before assignment/publication; allocation failure
aborts Prepare rather than publishing a partial roster. This deliberately moves
census allocation failure earlier than the old post-Begin census.

Validation: actual portable roster plus extracted production EncodeDependencies
and DidComplete methods compared against the original nested-loop reference over
2,000 randomized draws, callback presence and shared/null owners. Explicit first
empty callback masking, aliasing control blocks, vector moves, dynamic callback
state, owner release, encode failure/no retry, completion failure and every census
allocation failure point are covered under ASan/UBSan. Production wiring checks
ensure sealing occurs after all images and draw-time readability remains intact.
Metal command/callback dependencies are modeled; no full app build or native
threading, GPU, visual or performance validation was performed.

Independent source review and separate sanitizer/reference rerun: GO.
