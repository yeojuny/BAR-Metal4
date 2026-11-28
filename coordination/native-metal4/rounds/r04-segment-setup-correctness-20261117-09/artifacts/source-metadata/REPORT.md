# R04: exact per-segment coverage setup (source-only candidate)

Base: R03 `0780bd7c530b0696067b717428b49a416eda556a`. Production header blob `5916daf3b3e025ba85c3520a2215a2629b418a9b` was fetched at that commit and matched the local base. Coordination revision 38 was DONE/JINI with cleanup complete. Parent confirmed HQ turn completion. No Mac or HQ access occurred during this work.

## Change

Move AA0 segment-invariant endpoint quantization, endpoint delta, major-axis tie decision and rounded width count into vertex setup, transported through two flat int4 varyings (32 added bytes). All six vertices of a segment have the same endpoints and setup. Flat values therefore preserve both triangles' input, irrespective of provoking-vertex convention and winding. Early clipped returns have zero-initialized setup and take the R03 path if ever shaded.

The fragment uses the prepared values only for eligible endpoint/width/sample inputs and bounded finite fragment positions. Otherwise it calls the retained byte-identical R03 function. Its rational numerator/denominator, early minor-range rejection, floor division, epsilon fractions, diamond predicate and depth expressions remain identical. Zero quantized delta still returns false before division. Nonfinite endpoint Z is checked before setup. Sample counts other than 1 do not use prepared coverage. The MSAA fragment text is unchanged, although the common vertex interface grows.

No mask/implicit derivative sampling, discard placement, depth/stencil state, original viewport/tight geometry, height sampling, fan shader, lifetime contract, runner or settings changes. No floats replace exact integer/rational decisions. No shuffle/quad assumptions or inter-fragment sharing is introduced.

## Tests performed

`python3 test_prepared.py` passes 32,406 existing float32 boundary/random corpus cases. Of nonzero cases, 16,131 use prepared data, 13,667 fallback; remaining corpus cases return before division for zero delta. Three extra zero-delta setup tests and 12 extra unsupported/nonfinite-Z sentinel tests pass. Integer transport explicitly packs/unpacks 32-bit values. Prepared coverage independently uses that packed state and is checked against the R03 oracle; width/endpoint arithmetic uses float32 rounding. Guard and half-integer quantization neighbors, reversed endpoints, major-axis ties, +/-16384 endpoint and +/-32768 fragment bounds are retained.

Source reconstruction proves all old code outside the setup fields/helper, vertex setup call, new prepared function and fragment dispatch is byte-identical. The new function from pixel computation through return is exactly R03 except rounded count comes from setup. The CPU results' diamond-count counters compare historical pre-R03 order to R03, NOT a new R04 work reduction. R04 leaves diamond calls unchanged.

## GPU gate prepared, NOT run

The corrected parity helper initializes the new flat fields in its synthetic vertex entry point and compares each render of R04 against retained R03 by substituting the two exact ordinary/facing fragment call sites. It retains color/depth/stencil byte comparison and previous geometry/asymmetric/negative-control suites. Source assertion checks reference substitution and setup initialization. A third test-only library sets prepared count to zero while leaving fallback unchanged; the suite requires an observable color/depth/stencil difference from this disabled branch, proving a visible prepared path was actually exercised. This adds negative-control rendering until its first witness, not a game or benchmark.

No Objective-C++ or MSL compilation, real GPU render, engine build, game or benchmark was performed. Full production vertex floating-point lowering is not proved by CPU tests or the synthetic helper. Verify real compiler behavior before adopting. Large-coordinate guard cases remain CPU-only; actual MSAA4 parity is untested.

## Benefit and risk

This removes repeated endpoint float floor/conversion, endpoint delta/axis decision, width rounding and segment guard calculations from eligible fragments, replacing them with vertex setup at six vertices per segment. It does not remove per-fragment 64-bit division or SensorDiamond. Added flat payload and vertex work can outweigh saved arithmetic; register/interface pressure, stage-dependent floating-point lowering and driver limits require a real GPU gate and matched benchmark. No speedup, instruction reduction, or GPU-time claim is made from source operation counts.

Keep this as one reversible source-only experiment, not release adoption. If shader compilation/parity fails, return the error rather than changing source in HQ. Only after a fixed candidate handoff should HQ run its bounded correctness check; subsequent build/benchmark needs its own coordinated round. Do not rebuild R03 or re-run its completed cohort just for the known orange overlay uncertainty. main, releases and Homebrew remain untouched.
