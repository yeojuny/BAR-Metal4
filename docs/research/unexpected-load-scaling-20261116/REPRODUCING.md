# Reproducing and auditing the archived experiments

This is a source/evidence audit package, not an app distribution or a claim that the full game can run from this directory alone. No game, GPU helper, capture, engine build or benchmark was performed while preparing the package. The CPU-only patch replay and arithmetic checks are new archive-integrity checks, not new performance observations.

## Source identity and complete patch order

Obtain the `engine/` tree from `yeojuny/BAR-Metal4` commit `a3c7c6bfa93d09dac25755534195932d12fff41a`. The original archive identity is in `evidence/material-storage-pr7-20261115-01/SOURCE-PIN.json`; its SHA-256 is `8781e1804e113261e10cf927e9b880c8e96bd561e6e30b6600fb455d8a293be8`. Match source bytes with `source-snapshots/a3-public-MANIFEST.json`, since archive-container formatting can differ by download route. The repository's older release-source pin is preserved for provenance and must not be substituted for the experiment's signed engine pins.

The public a3 production source and local pre-R01 production source match exactly. The only additional local file was incidental `tools/material-storage-pool/__pycache__/analyze.cpython-315.pyc`; its bytes are not distributed and are not needed to reconstruct the source. Complete local manifests retain its historical hash. Apply the numbered patches from `patches/CHAIN.json`, in order, at the engine root with strip level1:

1. `00-a3-public-to-pre-R01-local.patch` is intentionally empty: no production source delta.
2. `01-pre-R01-local-to-R01.patch` changes two files.
3. `02-R01-to-S01-final.patch` includes the complete final S01 revision, nine paths.
4. `03-S01-final-to-G01.patch` changes/adds three paths and gives the accepted performance baseline.
5. `04-G01-to-G02.patch` changes/adds three paths and gives the separately preserved cost-reduction candidate. Applying this last patch does not mean G02 was adopted.

The package preparer applied this chain to a fresh copy of the archived a3 engine and compared every resulting source file hash at every stage, excluding only the identified cache. All comparisons matched. Counts were 10265,10265,10267,10269,10271. No Git mutation was used for that check. The original experiment patches under `patches/original-records/` are historical reference; do not apply them again on top of the complete numbered chain.

Selected source snapshots are provided for review convenience. A G01-only incremental patch is insufficient to reproduce the tested lineage. The pinned public base plus the complete chain supplies its engine source. Game/map content, the Apple SDK, build toolchain and runtime dependencies remain separately required.

## Build and runtime pins

Each stage's `CANDIDATE.json` (S01 uses `CANDIDATE-R2.json`) and source/closure manifests record the actual tested signed executable, raw executable, object/archive closure, build configuration and relocations. G01 and G02 CANDIDATE records each include the complete 19-library name/SHA-256 list. Library binaries and full apps are deliberately absent.

| Stage | Tested signed executable SHA-256 |
| --- | --- |
| pre-R01 baseline | b25a08287679847a420c37eaf3c201c9c95ad696e90d1b1eaa18e0cb08060942 |
| R01 | 87df3a1bc49a5fca4c47d05de1a41a5d85f464941365b8d20f7ee2eca1e4108f |
| S01 final | 9a44a0b109d77a561f08080dafeac1db8dd5d9655ef6002be379a17feee94aa0 |
| G01 | 962637cd9a0d71bb66896bedb0b807f2940097ae5cb58c2521eebd93bbb98559 |
| G02 | f83dae3cde8cd2a752d1d7ff9fc7629f8a5820f72e3f05306f7c57ab5ec2fca2 |

Rebuilding on another SDK/toolchain need not reproduce the signed executable hash. A source match is not by itself a binary match. The archived initial-cache files contain historical toolchain settings; standardized path tokens require local substitution. No installation or dependency update is part of this package.

## Measurement contract

Read the original `LAUNCH.json`, `CHILD-MODE.json`, exact `actual-start-script.txt`, per-run RESULT and final COMPARABILITY records, rather than adopting a later default configuration. The high-load scene is Seths Ravine Remake1.3.1, Ultra/AA0, 1512×950 game view, pinned spectator camera and seeds. The official benchmark command is `luarules benchmark armbull armbull 80 8 1400` at simulation frame240. The counted interval is540–1740 with 40 one-second bins after the combat warmup. The actual runtime records supersede stale planned600–1800 fields in some launch records.

Run order is A1,B1,B2,A2,A3,B3. R01 and S01 compare separate before/after binaries; G01 compares the same candidate binary with only its index option OFF/ON and S01 ON; G02 compares the same G02 candidate binary with only its upload option OFF/ON and S01/G01 ON. Clean runs disable probes, validation, tracing and capture. Idle/offscreen FPS reduction and VSync are disabled. Diagnostics are separate and excluded from FPS. Original child receipts preserve the exact renderer settings.

The All That Simmers ordinary observations use their own fixed fixture and simulation300–1500. They are historical single runs. Their pacing receipt includes the actual wall gate (about13.97s); do not rewrite it as exactly10s merely because a ten-second pacing minimum was configured. They do not establish a controlled regression. The follow-up G02 diagnostic's positive owner-summary record established Prepare0 in that diagnostic fixture; it does not retroactively identify the cause of the earlier FPS difference.

`draws` means DrawScreen frames in benchmark results, whereas the GPU API exports use actual graphics draw-call terminology. For each run, FPS=`draws/seconds`. Compare the arithmetic means of three per-run FPS values per arm. Run-level 1% lows are averaged separately; they are not pooled intervals. The raw per-frame CSVs and all one-second rows are retained without trimming or excluding slow observations. `evidence/NUMERIC-VERIFICATION.json` records archive checks for24 clean runs/960 bins and3 ordinary runs/120 bins.

Controller and observer source snapshots are forensic references. Some controllers import historical local harness modules and rely on separately provisioned game/map/font assets; they are not presented as a portable one-command launcher. The exact game/map checksums, frame script, seed, camera, renderer flags and observed conditions needed to compare a reconstruction are in the launch/run records. Do not copy a full personal profile to recreate these tests.

## Correctness and remaining limits

GPU helper source and results are separate from actual-game API validation and screenshot review. Keep each helper's stated texture/sample/quality scope: synthetic parity does not prove full-game pixel identity. G02's counts3174 refer to owner lifetime including warmup, and buffer_creates4449 includes other resources. They are not ROI timing counters, draw counts, saved CPU time or measured RAM peaks.

G03 is held and G04 is frozen without a GPU result. No improvement is claimed for them. R01/S01/G01 percentages are independent scene-specific comparisons; G02's lower mean does not demonstrate a reliable general regression or a speedup. The package preserves the original conclusions and uncertainty.

## Publication integrity and privacy

`PROVENANCE.json` distinguishes original-byte hashes from standardized publication-copy hashes. `${AUDIT_WORKSPACE}` replaces the private workspace prefix; `${AUDIT_USER_HOME}` replaces personal home prefixes. These are documentation placeholders, not values to assign to system HOME. Raw numeric rows and interval CSV bytes are unchanged. Launch environments are the original controller's curated experiment fields, not full process environments; no authentication, UI-app list, browser state or personal profile is included.

`evidence/EXCLUDED.json` pins the omitted raw trace and lists other omissions. `MANIFEST.sha256` covers the frozen technical handoff described in `MANIFEST-SCOPE.json`; a later publishing integration that adds README/AUDIT-QUESTIONS needs its own integrated manifest. Preserve upstream code headers and the notices under `licenses/`. Existing records may say UNKNOWN or not-yet-run because they were written before later gates; final report/disposition files give the completed stage state.
