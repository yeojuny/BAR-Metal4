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
