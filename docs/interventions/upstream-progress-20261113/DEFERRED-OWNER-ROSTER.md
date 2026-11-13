# P6: grouped engine-deferred first-owner roster

## Checklist
- [x] Seal exact non-null owner census across before-Lua then after-Lua draws.
- [x] Reuse indices in dependency encoding and completion, preserving callbacks.
- [x] Extract production lifecycle/ordered-finish methods into sanitizer fixture.
- [x] Preserve per-draw readability, encode order, flags, barriers and main app.
- [ ] Native Metal execution and performance measurement (not part of this step).

The prior dependency and completion phases independently allocated a set and
walked the same immutable before/after image lists. BuildGroupedOwnerRoster uses
a transient two-list view over the existing BuildTerrainOwnerRoster helper with
skipNull=true. One census spans both lists; independent per-list deduplication
would incorrectly invoke duplicate owners twice. Flat draw/image indices are
resolved through the existing Impl::At mapping. No draw, callback or owner is
copied into the roster, and no borrowed view survives preparation.

Preparation seals after both prepareList calls and the original uniformArena
report, before returning the prepared source. Private lists have no published
insertion or owner mutation API. Encoding changes flags/state, not image storage.
First non-null raw owner.get() wins even when its callback is empty. Null owners
remain excluded; distinct raw aliases of the same control block remain distinct.
Callbacks are obtained from retained images at each lifecycle invocation.

All producer ordering, per-draw requireReadable, grouped/ordered draw operations,
empty-list encoder state and queue/barrier behavior remain unchanged. Exceptions
in dependency/completion callbacks retain their original unset success flags and
retry behavior. The completion flag is still set BEFORE setterProbe176->Report;
a throwing report therefore leaves completed=true and cannot be retried. The
original completion guard is preserved exactly, without adding new checks.

Allocation failure in owner census deliberately moves from encoding/completion
to preparation after the original report and before publication. A failed build
cannot publish a partial roster. This is not exception-timing equivalence. It is
a source-only CPU bookkeeping improvement, not a demonstrated concurrent-owner
bug fix or a broader GPU resource-use eligibility proof.

run_deferred_owner_roster_test.py extracts actual EncodeDependencies,
DidComplete, DrawCount and FinishOrderedDraws into a host fixture with the real
production grouped/helper headers. ASan/UBSan validation includes 2000 randomized
before/after reference comparisons, cross-boundary first-empty duplicates, null
owners, raw alias identities, callback state at invocation, phase throw/retry,
report-after-completed failure, ordered finish prerequisites/count/duplicate
rejection, lifetime and every census allocation boundary. Structural checks keep
sealing after the final preparation/report and preserve per-draw readability.
No full app build, Metal execution or performance gain is claimed.
