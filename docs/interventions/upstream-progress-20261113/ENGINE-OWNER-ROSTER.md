# P6: engine-forward non-null first-owner roster

Source bookkeeping only. PreparedEngineModelForward previously rebuilt a raw
owner identity set at optional pass inspection, dependency encoding and completion.
EngineModelForwardGPU::Prepare now seals an index roster after all final draw/image
insertions. All returned phases have stable image arrays; draw execution only changes
encoded/finished state, not images. The existing shared roster accepts skipNull=true;
its default remains false for Terrain and CUS.

The distinction matters: engine forward ignores every null-owner image in all three
lifecycle phases, even if it has callbacks or a physical texture. Non-null first empty
callbacks still mask later callbacks of the same raw owner. Aliased shared owners
with different raw addresses remain distinct. Inspection uses a monotone index cursor
and preserves the original pointer-push/root-lookup interleaving. Raster/cull-dependent
indexed counts, root alias detection, phase-live producer queries, per-draw readability
and GPU binding/retention/ordering are unchanged. No subresource or barrier inference
is introduced. Completion requires the original finished flag; callback exceptions
retain original retry behavior.

One census allocation replaces two mandatory and optionally one additional census.
Failure is deliberately moved into preparation before publication, not claimed to be
exception-timing identical. Partially built rosters do not publish; source/lease owners
follow existing cleanup. Roster indices retain no extra owners.

Validation: run_engine_owner_roster_test.py extracts the actual three production
functions and compares 2000 randomized cases against the original non-null-owner
reference, including inspection pointer ordering, root lists, raster/cull counts,
callback failures and retry, null-only callbacks, alias pointers, lifetime, and every
census allocation fault. Metal objects/root traversal are host stand-ins; no native
GPU assertion or measured performance claim. Regression runners for both Terrain
and CUS verify unchanged default-null semantics. No full app build/run or publication.
