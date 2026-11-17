# Ordered first-use texture input: architecture candidate

Base: R03 `0780bd7c530b0696067b717428b49a416eda556a`.
Status: CPU-tested source patch; not Metal-compiled, GPU-tested, benchmarked, or published.

## Scope
- Default-off `BAR_NATIVE_TEXTURE_ORDERED_INPUT=1` permits CPU preparation/encoding to borrow an immutable texture produced by the exact sole pending submission.
- Runtime admission checks the actual pending ticket source submission and independent recording slot.
- Typed receipt checks producer context/submission, consumer submission, and weak preparation-frame ownership.
- Producer/borrow journal roles are separate. Borrowers never encode upload/mips again or publish producer completion.
- All seven existing texture readiness consumer paths now understand the ordered-input permit.
- Exact source/storage/status pins live in the final read-check callback. Abandoned preparation invalidates permits even if an interrupted journal remains alive; recovery preserves producers only.
- Completed-only S3O proof remains byte-identical.
- GPU pending limit remains one; the entire SubmitAndDefer/drain/synchronous tail remains byte-identical. This is NOT two-GPU-pending support.
- Noneligible cases retain the previous early retirement path.

## Provenance and isolation
Three baseline files were fetched from the connected GitHub repository at the exact R03 commit. Their base and candidate SHA256s are in MANIFEST.json. This directory is a partial patch workspace, not a complete repository checkout. No GroundDecal/R04 changes are included. Mac, HQ, STATE, main, releases, Homebrew were not touched. No GitHub write was performed.

## CPU verification
Run:

    ASAN_OPTIONS=detect_leaks=0 python tests/run.py

C++20 -Wall -Wextra -Werror, AddressSanitizer and UndefinedBehaviorSanitizer passed. LeakSanitizer is disabled because this environment reports ptrace incompatibility; no LSan pass is claimed.

Tests compile the production header and extract the actual production journal class, substituting only Objective-C texture pointer syntax with shared_ptr test storage. They cover foreign producer/context/preparation rejection; permit expiry; producer/borrow separation; interrupted preparation recovery; different consumer after abort; exact pins; producer failure/noncompletion; whole-journal completion validation; repeated exact Record/Complete compatibility. Source checks enforce all seven consumer migrations, unchanged completed-only proof and unchanged final drain/submission implementation. Patch dry-run against the downloaded baseline passed.

These tests do NOT compile or emulate Metal/Objective-C, real ARC ownership, the whole renderer, GPU visibility, or the real render worker scheduler.

## Independent review
Initial review found (1) stale permit retained by interrupted preparation and (2) changed exact-submission idempotence. Both were fixed and regression tested. Re-review found no further concrete blocker and classified this as a CPU structure candidate only, not GPU approval.

## Required HQ sequence, not run here
1. Apply only this patch to exact R03; verify source hashes.
2. Full native Metal compilation, flag off smoke/correctness baseline.
3. Flag on correctness on first-use texture/upload/mipmap workloads, models, terrain/Lua consumers, interrupted preparation, readback and shutdown.
4. Verify no duplicated producer or wrong completion, source credits, or mutable slot overwrite.
5. Only then same-workload OFF/ON paired timing. Record early drain count/time and actual overlap, not just FPS.

No performance gain is claimed. Steady-state effect can be zero when no first-use texture dependency is encountered.

## Remaining architectural work
This candidate introduces CPU-encoding ordered input only. Two pending requires a ticket-specific submitted-producer dependency representation, producer failure suffix quarantine, resource families beyond first-use texture (postprocess, terrain, water recurrence, mutable attachments), and capacity-driven oldest retirement. Do not remove the existing final drain based on this patch.
