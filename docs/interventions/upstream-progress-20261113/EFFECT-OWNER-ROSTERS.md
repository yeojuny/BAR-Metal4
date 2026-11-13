# P6: Grass and ResourceSpots first-owner rosters

Source-only CPU bookkeeping optimization. Each prepared leaf now seals one exact
first-owner index roster after its last draw/image insertion. Dependency encoding
and completion reuse that roster instead of independently allocating a set and
walking every image. The private prepared types expose no post-publication image
insertion or owner replacement API. EncodeDraw changes encoded state, not images.

The existing BuildTerrainOwnerRoster helper is used with its default
skipNull=false: null raw owners participate exactly as before, first empty
callbacks mask later callbacks, and raw owner.get() identity (not texture identity
or control-block equivalence) selects the first occurrence. Callback objects are
read from the retained image at invocation time, not copied into the roster.
Indices do not add owner lifetimes or borrow movable vector addresses.

All Require guards, per-draw requireReadable calls, invisible draw bookkeeping,
producer invocation order and retirement rules remain unchanged. Callback throws
leave dependencies/completed false, retaining the original retry behavior. No
barrier, queue, command eligibility, access/subresource rule or GPU receipt is
removed or widened. This does not fix a demonstrated concurrent writer: native
preparation and lifecycle access already have their serialization/ownership gates.

Census allocation failure deliberately moves from dependency encoding/completion
to preparation before publication. It is not exception-timing equivalence. A
failed roster build cannot return a partially prepared leaf; normal ownership
cleanup applies. This is the same bounded failure-time tradeoff as the prior
Terrain/CUS/engine-forward rosters.

Validation: run_leaf_owner_roster_test.py extracts the actual EncodeDependencies
and DidComplete production bodies for both leaves and compiles each with the
existing production roster helper, ASan and UBSan. Each runs 2000 deterministic
randomized comparisons against the original nested-loop reference, explicit null
and alias owner cases, first-empty and phase-live callbacks, dependency/completion
throw retries, prerequisite/all-draw retirement guards, owner lifetime and every
census allocation boundary. Structural checks locate the seal after the final
insertion, require exactly one seal and unchanged per-draw readability. No full
app build, native Metal execution or measured performance improvement is claimed.
