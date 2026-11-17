# Texture ordered-input execution evidence candidate

Base: `3713df592f99b6f8c3edc91ad99803e073daa230`.
Baseline WorldRenderer blob verified against GitHub: `02137a3c07712830ab35849ba88188a952234629`.
This is an independent partial-source patch workspace, not a full checkout.

## Activation and interpretation
Set `BAR_NATIVE_TEXTURE_ORDERED_TRACE=1` for bounded correctness diagnostics only. The underlying optimization still requires its existing `BAR_NATIVE_TEXTURE_ORDERED_INPUT=1`.

Events under `[NativeTextureOrderedTrace]`:
- `armed`: tracer enabled and Texture() reached. Once per process. Does not establish eligible workload or optimization entry.
- `admitted`: existing exact pending-producer/independent-slot admission succeeded. Repeated calls may report the same producer/consumer; not unique resource counts.
- `encode_permit_check`: the producer-loop borrower branch passed readability and skipped duplicate upload/mipmap production. Not proof of a consumer draw or GPU execution.
- `producer_retired_check`: the existing final prior-read callback successfully verified the exact producer completed. Not consumer GPU completion.

Pair producer/consumer identities with normal HQ validation evidence. `armed` alone means no successful entry was observed in those logs, not a claim of optimization benefit. Each non-armed event has an independent cap of 64 records; logs do not provide total frequency or one-to-one resource accounting. The log reports the actual cap (armed=1, others=64).

## Invariants
- Final drain, GPU pending limit=1, barrier and residency behavior unchanged.
- Callback adds only a scalar consumer ID capture; no additional resource owner.
- Logging happens after existing admission/readability/completion checks.
- OFF bypasses budget/logger; logger exceptions are swallowed by diagnostic wrapper.
- Performance measurements MUST run with trace unset or 0. ON results explicitly mark performance_valid=0.

## Verification
`python tests/run.py` passed C++20 -Wall -Wextra -Werror tests:
- actual production atomic budget under 8 concurrent callers; 64-record saturation and independent event caps;
- actual extracted production logging wrapper in live/nonlive builds;
- environment values unset-equivalent empty, 0, 1, true; only exact 1 logs;
- injected logger exceptions do not escape noexcept wrapper;
- stripping the narrowly listed diagnostic edits restores the baseline renderer exactly.

Patch dry-run passed. Independent read-only review found no concrete synchronization/lifetime blocker. Its event-name and armed-cap clarity suggestions were applied and tests rerun.

No Metal full compile, Mac/HQ run, GPU execution, or benchmark occurred. No source behavior improvement or performance gain is claimed. No STATE/HQ/main/release/Homebrew changes. No GitHub publication performed here.
