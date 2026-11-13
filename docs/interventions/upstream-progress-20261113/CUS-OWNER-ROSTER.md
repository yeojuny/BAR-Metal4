# P6: exact CUS first-owner roster

Source-only bookkeeping optimization. No queue, encoder, barrier, resource identity,
subresource hazard or NoCommands rule changes.

CUSSourceGPU::Prepare seals the existing BuildTerrainOwnerRoster index census
after the final draw/image insertion and before returning. Its private Impl has no
post-return draw/image insertion API. PreparePassInspection160 is called only
on the returned prepared source (opaque preparation or late alpha attachment).
Encoding changes only draw.encoded, never image storage or ownership.

The three actual repeated routes are pass inspection (when enabled), dependency
encoding, and completion. Each previously allocated a set and scanned every image
for exact first raw-owner identity. Now one prepare-time census serves all three.
Inspection still scans all images for physical texture roots and every draw for
indexed count; root alias fail-closed behavior is unchanged. HostOnlyNoDependencyCommands220
still dynamically inspects every image, not only first owners.

First occurrence includes null owners and first empty callbacks. Neither texture
identity nor shared control-block identity substitutes for raw owner.get().
Encode/completion read callbacks at invocation time. Readability remains at every
original draw. Roster indices borrow no addresses and add no owner lifetime.
Original retry semantics are retained: unlike Terrain's attempt guard, CUS permits
retry after a throwing dependency callback because dependenciesEncoded remains false.
Completion also retains its original incomplete flag on throw.

Census allocation failure now occurs during preparation before source publication,
rather than first during encoding/completion. No partially built roster is published.
Native retained resource state already created during preparation follows existing
lease cleanup. This is a deliberate failure-time shift, not exception-timing equality.

Validation: actual production EncodeDependencies, DidComplete, and
PreparePassInspection160 are extracted into the narrow host fixture. It compares
2000 randomized draws against original first-owner reference, roots/counts and
inspection pointer order, empty/null/alias owners, phase-live callbacks, exception
retry/retirement and lifetime. Every census allocation fault is injected before
publication. ASan/UBSan runner: run_cus_owner_roster_test.py.
Metal texture traversal is represented by host pointers; production root traversal
is unchanged. Native Metal execution, performance savings and GPU correctness
have not been measured or newly validated. No full app build/run.

Pass inspection uses a monotone index cursor through the roster while scanning
images, preserving original inspection-pointer push before each root lookup and
root-table update. Only redundant owner set allocations are removed. Existing
partially filled inspection vectors on allocation failure remain non-transactional;
no retry semantics are added.
