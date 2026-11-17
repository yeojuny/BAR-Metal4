# R03 bounded early minor-range rejection

Parent source commit: `0382d0f3833d63b3ff1a78c46b108c99539656d0`.

Production change: retain SensorLineCovered byte-identically, add SensorLineCoveredEarly and select it only at the original one-sample fragment call. Its bounded branch moves the exact count/lower/actual rejection ahead of center and SensorDiamond. Unsupported inputs call original function unchanged. No vertex, winding, resource, depth expression, mask sample or discard-order changes; MSAA function untouched.

## Bound and equivalence proof

Guard requires samples=1, finite float3 endpoints/fragment/width, endpoint XY magnitude <=16384, fragment magnitude <=32768, 0<width<=64. Quantization is float32 scale256/add0.5/floor, not ideal float64 geometry. Quantized endpoints <=2^22, deltas <=2^23; nonzero major delta has |di/dm|<=1. Numerator <2^48, positive denominator <=2^31; doubled numerator minus width term and FloorDivide negative correction fit below2^49. Center/lower/count additions fit signed32; fraction comparisons fit signed64 at these bounds. Zero quantized delta still returns false before division.

Both predicates are pure: exact minor-range AND diamond. Their short-circuit order can change without changing covered output when arithmetic is defined. Depth code executes for the same survivors and is byte-identical. Early false is only a function return, never fragment discard; implicit-derivative mask sampling and final discard stay in their original positions. Nonfinite endpoint Z also forces unchanged fallback.

## Actual CPU checks

32,406 cases PASS; 17,035 guarded and15,371 fallback cases. Includes reversed endpoints, X/Y ties, zero/tiny quantized delta, negatives, clipping-like extrapolation, width rounding, unsupported widths/sample4 and coordinate guards. Explicit np.nextafter neighbors at +/-16384, +/-32768, width64 and half-integer widths, and half/256 endpoint quantization. NaN/Inf width/endpoints/fragment check guard rejection only, not undefined legacy conversions. Source reconstruction assertion proves retained fallback and all unrelated source are byte-identical.

Float32 input/scale/add/width rounding are explicit numpy.float32. Following long operations use exact integers with signed64 bounds assertions; floor division agrees with retained FloorDivide for positive denominator. CPU test proves coverage predicate ordering, not compiler/Metal behavior or full depth interpolation. 67,611 versus45,188 modeled diamond calls are this synthetic oracle workload ONLY, not GPU speed evidence.

## GPU gate prepared, not run

Existing corrected R02 helper is extended, not replaced. It builds candidate and reference MSL libraries. Reference substitutes the two exact fragment coverage call sites (ordinary and facing probe) back to retained original SensorLineCovered. Explicit typed copied pipeline descriptors preserve the prior compile fix. Every Render invokes identical geometry/state/textures against reference and candidate, compares color/depth/stencil byte-for-byte, then retains all original 3,600 geometry cases, asymmetric1,024 suite, depth/stencil witnesses and legacy winding negative controls. It prints R03 comparison count. Shader source markers fail closed if call count changes. This is necessary: merely rerunning original AABB/tight comparisons with the same modified fragment on both sides would not test R03 correctness.

No Mac, Objective-C++ compilation, MSL compilation, GPU, engine build, game or benchmark executed. The helper's new code needs HQ compilation and actual GPU verification. Guard sample4 coverage remains guard-only, not true MSAA4.

## Risks and next gate

Hoisted lower division may cost more at major endpoints that originally fail diamond. Guards and code duplication can change register pressure/inlining. Current capture has no instruction/register/fragment invocation counters proving gain. Run one fixed helper correctness attempt first; only a successful fixed candidate should proceed to isolated build and matched benchmark. No speed claim or release adoption.

Large +/-16384 endpoint and +/-32768 fragment guard boundaries are CPU-only cases in this candidate. The existing rendered fixture uses its small framebuffer and existing clipping/width/sample cases; no new large-coordinate GPU boundary fixture was added. Nonfinite Z fallback is source-verified, not executed through the legacy undefined-input path.
