# T3 bounded lazy terrain publication

Baseline `1bd0ee7607326debc650bf97e786618a502a8aef`. Following the owner's instruction to complete source changes before one integrated app build, this is source-tested implementation; the earlier design's Mac acceptance gate is deferred to that final integrated build, not marked passed.

## Implemented boundary

Only linked consumers explicitly promising an immutable definition use lazy writes. Generic, unsupported and unlinked consumers keep eager publication. First changed write after a seal clones the named map into a private writer map. Subsequent accepted writes mutate only that private map; each still advances its logical revision. No use_count threshold or mutation of externally published snapshots is used. Inactive named values and existing link-default merge remain.

The binding has a separate private pending-observation token with a weak lifetime and callback. It is not part of Selection. Current/SaveCurrent seal accepted state; actual draw Capture already calls Current. InspectCurrent/CurrentKind do not seal. While pending, inspection's terrain publication is provenance only, not a uniform observation. Existing source-call inventory applies.

Restore clears pending selection without rewinding the writer. Old owning snapshots stay immutable. Historic A after writer B remains rejected by draw capture; an equal-to-writer B setter repairs selected state and advances revision. Malformed-kind restoration repair canonicalizes Terrain kind/consumer at the setter boundary. Generic bind/unbind clear pending tokens.

Observation seals accepted state without adding consumer/context validation to Current/Save. Capture retains its existing validation. Before reload/import/release mutate identity, an active pending publication is sealed. Destruction unbinds and destroys its lifetime token without allocating/sealing. Observer validates live source membership before dereferencing the writer. Saved Selection objects do not retain callbacks.

## Failure and resource scope

Value allocation and required validation precede private-map changes. Failed publication allocation leaves a pending private writer for retry; previously saved snapshots are unaffected. Allocation location intentionally moves from every setter to the first changed writer copy and later observation. Thus `bad_alloc` may surface at Current/Save/lifecycle observation; exact eager allocation-failure timing is not claimed. No shader callback, RNG, GPU command, queue/fence or thread scheduling change occurs.

This is not the final typed T4 packing plan, full-frame async ownership or performance validation. Full app/Metal build, actual Lua callback execution, rendered parity and performance remain pending. No app was built, launched, stopped or deleted during these source tests.

## Source evidence

The production-source harness runs the actual writer/binding declarations and implementation with external compiler/consumer services stubbed. Its eager-reference mode injects Current after each accepted setter only into the generated test translation unit; there is no production eager-mode macro. This compares the same setter implementation with eager versus lazy observation boundaries, not an independently rebuilt historical engine.

Directed tests preserve exact observed type/shape/value/revision fingerprints, nested/historic restore, malformed-kind repair, reload/deletion/context handling and saved snapshot immutability. Three unobserved changed writes use one private map copy and no publication until capture; eager-reference uses three copies/publications. These are source counters, not runtime game FPS measurements. Injected allocation failures check selected first-copy/value/update/seal failure points and retry, not every possible allocator failure.

```
python3 engine/tools/metal4-contracts/run_native_terrain_program_test.py --output /tmp/terrain-lazy-tests
python3 engine/tools/metal4-contracts/run_native_terrain_program_test.py --eager-reference --output /tmp/terrain-eager-tests
```
