# Snapshot-owned terrain height/normal pair

A new immutable terrain-height owner now receives a distinct corner-height buffer
and normal texture, even when dimensions match the prior version. Previously the
same buffer/texture were overwritten after retirement; a retained old map-image
owner could therefore still refer to storage later reused for different contents.

The complete MapImage owns its source snapshot, corner buffer, normal texture and
completion bit. A serial source/resource cache calls the factory before committing
either owner. Failure before/between/after allocations preserves the old pair;
only complete non-null resources are published. Unchanged source identity reuses
the version. Static reset clears the cache without invalidating externally retained
versions. The runtime still retains allocations through GPU completion.

The old deformation/map retirement barrier remains. This does not assert that all
other terrain/model/main attachments are isolated or that lookahead can start.
Additional allocation cost and GPU performance are unmeasured.

Actual cache ASan/UBSan tests exercise 3000 failure cases, same-size source change,
source lifetime, unchanged reuse and reset/recreation. Existing normal backing
ownership tests remain. Objective-C++ factory/commit order and barrier placement
are checked structurally; no Metal execution or native texture validation is
claimed. The guarantee is pair publication, not transactionality of the entire
map-cache rebuild that precedes it.

## Follow-up: whole tile/pair publication boundary

Tile batches now remain staged while the height/normal pair is created. Allocation
retention is also prepared before publication: retention can allocate and fail.
The actual source/resource cache accepts a throwing preparation and an enforced
no-throw companion commit. Only after preparation succeeds are tiles, pair and
renderer aliases switched. Cache hits also run preparation/companion commit, so a
new tile owner with unchanged heights is not accidentally skipped. The MapCache
move assignment is statically required to be no-throw.

The actual cache test adds 2000 preparation failures across cache-hit and changed
source cases, plus successful companion commits for both paths. Native staging,
allocation-retention order and alias publication are structural checks only.
Previously recorded upload work and residency/retention bookkeeping may remain in the current failed runtime
lease; this does not promise runtime rollback or make failed leases reusable.
No full build, Metal execution, barrier removal or overlap claim is made.
