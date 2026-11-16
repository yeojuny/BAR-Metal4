# Unexpected load scaling: evidence and source audit

This package records the native Metal4 renderer's uneven response to load.
It preserves measured results, diagnostic evidence, and candidate source deltas
for external review. It does not establish a single cause of the MVK/native gap.

The source snapshots and patches are review material under `docs/research/`.
Publishing them does not apply a patch to the repository's engine source.
The recorded performance baseline remains G01 (S01 ON, G01 ON); G02 is held.
This documentation publication makes no additional promotion decision.

## Read this first

The principal observation is that a change can help one heavy scene while
showing no benefit in another scene. A successful narrow check does not
establish a general performance improvement.

| Item | Expected or investigated | Observed | Unresolved or limited |
| --- | --- | --- | --- |
| R01, heavy workload | Bound SensorLOS raster coverage while retaining the fragment rule | **47.462308 → 53.237233 FPS (+12.167391%)**, six fresh runs | This is its own paired cohort; do not pool with later cohorts |
| S01, heavy workload | Change decal/grass stage boundaries while retaining dependencies | **53.692393 → 58.621093 FPS (+9.179513%)**, six fresh runs | Separate fresh cohort; isolated store/load cost was not measured |
| G01, heavy workload | Reduce the cost addressed by the ground-decal index candidate | **58.589718 → 62.798911 FPS (+7.184183%)**, six fresh OFF/ON runs | Same binary, one option, three OFF and three ON runs establish the net effect here; vertex-cache reuse and broader generality were not measured |
| G01, historical Simmers comparison | Check whether the heavy-scene benefit carries to Simmers | **185.463902 → 183.000000 FPS (−1.328508%)**, historical single runs | No statistical equivalence or regression conclusion follows from a single comparison |
| Historical Simmers frame-count distribution | Locate the recorded FPS difference | **−98 DrawScreen counts**; **−86** in measured seconds **17–23**, **−12** in the other 34 bins | These are rendered-frame counts, not a reduction of 98 GPU draw commands; cause remains unresolved |
| G02, buffer registration | Reduce registered buffer objects | **2 → 1** registered objects | An object count is not net CPU RAM, peak memory, or physical GPU memory |
| G02, performance | Determine whether the narrower buffer change helps the measured run | **62.941139 → 62.887995 FPS (−0.0844347%)**, six fresh OFF/ON runs | No measured FPS benefit; net CPU RAM and peak-memory result remain **UNKNOWN** |
| Latest Simmers diagnostic | Determine whether GroundPrepare contributes in that capture | Recorded **GroundPrepare = 0** | This is bounded to that diagnostic and does not imply global absence of ground work |
| Original grass behavior | Preserve the actual reference behavior | Original grass-height culling is part of the recorded source contract | Removing or changing it would alter the comparison; do not assume the two paths perform equivalent work |
| G03 | Candidate investigation | **HOLD** | Generic Lua/float input contract unresolved; known-UNORM alternative retained |
| G04 | Proposed follow-up | **Source/CPU preparation only; GPU, build, game and FPS unvalidated** | Paused for publication; no measured performance result |

The FPS metric is CPU-observed DrawScreen count divided by elapsed wall time,
not GPU execution duration or display-present rate. Each timed ROI is 40 seconds.
Percentages above are reported observations, not confidence intervals.
Keep each row attached to its own scene, source identity, and run conditions.
Do not combine these rows into one aggregate improvement percentage.

[SUMMARY.json](SUMMARY.json) fixes cohort rows, identities, conditions and dispositions.
[Audit questions](AUDIT-QUESTIONS.md) define the requested external review.
[EVIDENCE-INDEX.md](EVIDENCE-INDEX.md) maps each claim to its source records.
[NUMERIC-VERIFICATION.json](evidence/NUMERIC-VERIFICATION.json) preserves the numeric cross-checks.

## Keep the measurement identities separate

The historical signed binary identity abbreviated **9a44** and the latest identity
abbreviated **9626** represent different evidence contexts. Their full SHA256 values
and candidate mapping are in SUMMARY.json; they are not Git commit identifiers.
Do not present a workload share measured for 9a44 as a measurement of 9626.
A source similarity argument is an inference unless the relevant latest run
actually measured the same quantity.

The historical Simmers results are separate single runs:

| State | Mean FPS | Slowest-1% FPS |
| --- | ---: | ---: |
| R01 | 185.154626 | 129.087780 |
| S01 | 185.463902 | 140.449438 |
| G01 | 183.000000 | 87.677724 |

Actual indexed-draw activation in that ordinary G01 run is **UNKNOWN** because
its probe was disabled. Slowest-1% is the reciprocal of the mean worst 1%
CPU-observed intervals; aggregate run figures do not pool frame intervals.
The historical Simmers single comparison, latest Simmers diagnostic, and heavy
G01 comparison are separate observations. They are not repeated trials of a
single controlled experiment.

The DrawScreen frame-count concentration between 17 and 23 seconds is preserved because it
locates most of the observed frame-count loss in six bins; none was excluded.
It does not establish that the candidate caused the loss or identify the work
responsible. The recorded 40 rows of simulation state, units, camera, speed and pause were
compared and matched. Dynamic effects and complete pixel equality were not
established; path activation and hardware mechanism remain separate questions.

The latest G02 Simmers owner-lifetime diagnostic records ownerCreated=1,
Prepare=0, eligible=0 and bufferCreates=0, including warmup. This is not a count
restricted to the 40-second timed ROI, nor proof of the historical FPS cause.
The original grass-height cull suppresses grass in this fixed view: cameraY
4382.61110397 minus maximum terrain height 700 exceeds cutoff 3599.99992.
The map still has grass assets; this observation is view-specific.

## What the checks establish

The **G01/G02** helper evidence uses a **synthetic 64 × 64** target.
Its recorded scope is sample1, **no-shadow / low-quality**, and normal **RG32**
versus production **RG16**. G01 and G02 each record 32 synthetic cases.
Preserve these restrictions when citing equal color/depth/stencil results.

A helper result is evidence about the tested computation and selected samples.
It does not establish complete rendering equivalence in the live game.
In particular, a small synthetic target cannot stand in for all camera views,
shadow configurations, quality levels, formats, or scene histories.

R01 has a separate **3600-case GPU check**; its coverage is recorded separately
and must not be described as the G01/G02 64 × 64 helper.
The real-game view comparison is a separate evidence class.
Keep its images, conditions, and observations distinct from the helper check.
A view comparison does not become a numerical full-frame parity test merely
because the helper also passed.

## Source lineage and review boundaries

The source chain is recorded in [patches/CHAIN.json](patches/CHAIN.json).
The chain starts from public commit
[`a3c7c6bfa93d09dac25755534195932d12fff41a`](https://github.com/yeojuny/BAR-Metal4/commit/a3c7c6bfa93d09dac25755534195932d12fff41a)
and distinguishes the pre-R01 local state, R01, S01-final, G01, and G02.

| Directory | Purpose |
| --- | --- |
| [source-snapshots/](source-snapshots/) | Selected source files and per-state manifests for inspection |
| [patches/](patches/) | Ordered source deltas and their chain record |
| [evidence/](evidence/) | Allowed result records and diagnostic material supplied with this package |
| [licenses/](licenses/) | Original notices and contribution records |

The selected source files are not a standalone complete engine checkout.
Use the pinned base and documented patch chain for reconstruction.
Follow [REPRODUCING.md](REPRODUCING.md) and [PROVENANCE.json](PROVENANCE.json)
for the exact supported procedure and
its verification status; the presence of a patch file alone is not proof that
reconstruction was verified.

G02 remains a preserved candidate, not an accepted baseline.
G03 HOLD and G04 unverified must remain visible in downstream summaries.
The pre-R01 production source matches the pinned public base; one incidental
Python cache is explicitly excluded. Do not describe that cache as a code change.
No live source tree, application, stable release, or Homebrew definition is
changed by placing these files in a research documentation directory.

## Interpreting the result

G01 uses the same binary with one option switched OFF/ON, three fresh runs per
arm. This supports the whole-benchmark net FPS effect in those fixed conditions.
It does not measure vertex-cache reuse or assign a percentage to that mechanism.
The Simmers observation limits how broadly that result can be described.
The package preserves both instead of selecting the favorable scene alone.

The G02 object-count reduction establishes a narrower structural result.
The separate high-load diagnostic records 3174 prepares and 3174 packed creates,
with zero separate-index or uniform-only creates; owner lifetime includes warmup.
Uniforms are 368 bytes plus 72 index bytes: 440 logical upload bytes, while the
host aggregate is 448 bytes. The original GPU-event lease retention remains;
do not equate object count with physical allocation or RAM savings.
The FPS comparison does not show an improvement; net CPU time and physical
RAM peak savings were not measured.

No result here establishes a universal native-versus-MVK speed relationship,
a single maximum bottleneck, or complete numerical image equivalence.
The historical 9a44 GPU envelope is not a measurement of latest 9626 active
overlap; individual GrassVS active intervals and isolated store/load costs
were not exported as a complete causal accounting.
The package also does not establish statistical no-regression or a release gate.

## Attribution and handling

Preserve the original authorship, copyright, and license notices.
Applicable external contributions are recorded under `licenses/`.
Distinguish inherited code and earlier credited fixes from new candidate work.
Do not claim every historical contribution record applies to every source state.

