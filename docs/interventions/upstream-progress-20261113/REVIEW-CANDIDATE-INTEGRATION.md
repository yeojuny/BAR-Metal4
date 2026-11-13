# Review candidate integration, 2026-11-13

## Fixed candidate provenance

- Prior Mac successful source: `6e9ce8f89afb14d632cce33e4742bab11b76b60f` plus nine-file compile patch.
- Original patch SHA256: `c7dfd4753ac97d87510a8f00204a2e64aff3d58f13c49b95da01611e56abe974`.
- Prior built binary SHA256 recorded by Mac build: `e1d1d12e7bc51872747c0e368426c820e5692413fc029e37727bad9e14caef50`.
- Baseline test: render worker 1, BeginReady pilot 0. Other experiments were not established by that run. Two normal exits, last simulation frame 2002 / 10619 presented frames; no visual-parity or performance acceptance.
- Current integration starts at `5afe4c1` (retains exact Postprocess provider state ownership), not an assertion that the old binary includes that change.

## Compile patch review (R2)

Nine files integrated without reimplementing the same fixes:
1. Three resource backings: shared_ptr atomic free functions replace unsupported Apple libc++ atomic<shared_ptr> specialization. Acquire reads, release one-shot CAS and relaxed failure ordering retained (six files).
2. Opaque model friend declaration matches implementation MRTFrontier209 pointer (one file).
3. CUS DeleteShader resets builder with Initialize({},{}). Builder releases its sealed cache; captured shared immutable snapshots survive (one file).
4. Explicit exception include for terminate (one file).

The patch contains no MetalInfo/UnitUniformCopy runtime control fix. R1 remained independently applicable.

## R5 measurement boundary

BuildTerrainOwnerRoster runs after the last draw/image mutation but before the existing prepareCPUus sample. This includes mandatory census cost while avoiding later reporting cost. Existing diagnostic exclusion remains unchanged. The roster regression now asserts this boundary.

## Verification and remaining gate

- CUS immutable uniform production packing host test: passed.
- CUS actual three-phase roster: 2000 randomized cases plus lifetime/fault paths passed.
- R1 control test and R3/R4 reproducibility evidence: adjacent reports.
- Full updated Mac compile: started in the separate existing test build; Mac disconnected before result could be observed. Read build-review-r1-r5.log and check the existing process before restarting.
- Existing gate regression updated for 8 live + 3 captured provider boundaries, passes. MetalInfo fixture compilation passed; runtime fixture still requires the pinned original BAR shader (stock engine shader is not equivalent).
- Updated game launch, P5 ON/OFF, equal-workload performance and visual parity: not run; user requested preparation only before execution.

No new optimization feature or broad P5 expansion is included. Existing main, installed app, and unrelated vendor changes are preserved.
