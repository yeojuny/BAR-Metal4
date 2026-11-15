# Current native source and independent-maintainer handoff

This is the complete **P → Q → S** development checkpoint on v1.11.3. It is intended for another maintainer or LLM to modify and diagnose. The public release, default `main` engine and Homebrew are unchanged. No new build, game or benchmark was performed to publish this checkpoint.

Start with [SOURCE GUIDE](SOURCE-GUIDE.md), [results and limits](RESULTS.md), [reproduction](REPRODUCTION.md), then [remaining problems](NEXT.md). Do not begin by reading all 10k files. The large [FULL-SOURCE-FILES manifest](FULL-SOURCE-FILES.json) is verification metadata, not new implementation.

| Identity | Value |
| --- | --- |
| Release base | `v1.11.3`, commit `0b0fd234af3bf5dd073ab7094350ec63fc3a7e04` |
| P source commit | `2b82374573dff256fafc2e7c1733c867de7aafae` |
| Q source commit | `10f09d4e7d24502cd28aab284cb6392b1fd95a0a` |
| S source commit | `8412adc758db31b0abed71db1f0a800d9bb68a1b` |
| Current engine tree | `624267e4ee411b87e145ac3d2cd12ed857f411c9` |
| Actual tested signed S engine | `5069655ce90fbd301dce953f277d264af7f15f18aeff5ca16dd9651681d884a7` |

The full frozen S snapshot has 10,248 input files. Every input byte/mode was compared with the Git tree; the only publication change is two personal path prefixes in one non-build provenance JSON. Original/published hashes are recorded. Four historical simdjson dotfiles missing from the local archives are retained, yielding 10,252 Git entries. No compiled C++/Objective-C++, GLSL, MSL, Lua, generated layout or test payload is modified. [Source closure](SOURCE-CLOSURE.json).

The final source delta from the release is 34 paths: 13 modified and 21 added. P has one path, Q 29 and S five, with overlap between phases. No undeclared source changes were found. K is excluded by this source chain; this publication does not claim to have independently tested K's implementation.

## Evidence index

- [P final decision](evidence/P-final/RESULT.json), [original four-run completion](evidence/P/measurement/clean_paced/COMPLETION.json), [supplemental OFF](evidence/P-final/SUPPLEMENTAL-OFF.json), [slow-tail analysis](evidence/P-final/tail-analysis/ANALYSIS.json).
- [Q functional result](evidence/Q/RESULT.json), [integration adjustments](evidence/Q/INTEGRATION-ADJUSTMENTS.json), [F2 partial/failed relink](evidence/Q/measurement/F2-PARTIAL-AUDIT.json), [original packet](evidence/Q-design/CHECKPOINT.md).
- [R/P2 all-thread analysis](evidence/R/outputs/offline-profile-p2/ANALYSIS.json), [lossless normalized XML](evidence/R/outputs/offline-profile-p2/time-profile.xml.gz), [P1 export failure/recovery](evidence/R/outputs/offline-profile/EXPORT-RECOVERY.json).
- [S handoff](evidence/S/HANDOFF.json), [result](evidence/S/RESULT.json), [private adoption](evidence/S/ADOPTED-LOCAL-BASELINE.json), [diagnostic work/Metal check](evidence/S/DIAGNOSTIC-RESULT.json), [clean four-run completion](evidence/S/measurement/clean_paced/COMPLETION.json), [host sanitizer log](evidence/S/checks/host-run.log).
- [Every selected evidence file and original/published hashes](EVIDENCE-MANIFEST.json), [large omitted artifacts/checksums](OMITTED-ARTIFACTS.json), [exact prelaunch fixture verification](reproduction/FROZEN-INPUT-VERIFICATION.json).

Pending fields in original pre-runtime snapshots are retained; final results and adoption records provide the later disposition. Original raw failures are not overwritten by later success. Path tokens `$P/$Q/$R/$S` identify the corresponding `evidence/` phase. Some immutable launch records refer to private original paths; the actual small portable inputs are included under `reproduction/frozen-inputs/`.

[Separate current-J wall-span context](RESULTS.md#current-j-background-kept-separate-from-pqrs) is included to distinguish the older locked/Pinewood runs and broader retirement/encoding questions.

## Credits and public scope

Existing engine/native work keeps its original authors including yeojuny. The two LuaVBO invalid-index format arguments inherited from H/J remain credited to tomjn, RecoilEngine PR #3695, commit `29b950671fd883d2e01bc2cf1f6f6d0b7168280a`; this is not a new import. PR #3035 affinity and benbreen's QoS experiment are not present in S. Beherith's Sepia widget/GLSL copyright (2023.01.05), GPL v2 or later, original stages, hashes and notices are retained. [Original contribution records](UPSTREAM-CONTRIBUTIONS.original-record.json) · [applicability](evidence/Q-design/PROVENANCE-APPLICABILITY.json) · [source provenance](../../../engine/tools/sepia-pilot/PROVENANCE.json).

Public evidence normalizes local paths. Original records changed by normalization are preserved privately by the repository manager, keyed by manifest hashes. Unchanged CSVs/images are stored once by Git blob identity. No raw conversations, account data, engine binaries, object/debug files, full user profiles, game pool or trace container is included.
