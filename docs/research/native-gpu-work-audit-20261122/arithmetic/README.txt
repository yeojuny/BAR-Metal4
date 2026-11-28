BAR-Metal4 arithmetic audit supplement — 2026-11-21

Scope
This is a host-side arithmetic validation, not an engine patch, Metal shader
compilation, GPU parity run, or BAR performance benchmark. It evaluates a
candidate for the remaining signed 64-bit quotient/remainder calculation in
the archived SensorLOS exact-interior patch.

Run
Requires Python 3 and NumPy. Run:
  python3 check_division_estimate.py
The script writes division_estimate_results.json beside itself.

Candidate
Estimate floor(n/d) in float32, compute r = n - q*d in exact int64, then adjust
q and r until 0 <= r < d. Keep the correction loops and a fallback outside
the proven input domain. SensorLOS needs both the exact quotient and exact
remainder, including r == 0 and 2*r > d boundary decisions. Do not replace
these boundary decisions with floating-point comparisons or add a second
division for the remainder.

Why this candidate is grounded in the repository
The R03 WeaponRange source already contains a bounded float-estimate plus
integer-correction strategy under RECOIL_RANGE_DIVISION233. The archived R75
SensorLOS patch still calculates numerator / denominator directly. This
supplement evaluates adapting that strategy, not changing coverage rules.

Input bounds derived from the existing SensorLOS guards
Quantized endpoint coordinates have absolute value <= 2^22.
The nonzero major delta has absolute value <= 2^23, with |minor_delta| <=
|major_delta|. The integer major pixel is in [-32768, 32768]. After sign
normalization, d = 256*|major_delta|, so 256 <= d <= 2^31. The numerator
satisfies |n| <= |major_delta|*(2^24 + 128), hence |n|/d <= 65536.5 and
|n| < 2^48. These fit the existing Range233 helper's bounded fast path.

Observed results
1,555,102 host cases passed quotient, remainder, and lower-edge formula
comparisons against an exact integer reference. Of these, 4,197 required
correction of the float32 estimate. The maximum observed correction count
was one; that observation is not a proof for every GPU compiler or target.
Coverage/color/depth/stencil parity has NOT been tested for this candidate.

Required next checks
Inspect optimized MSL output and counters for an actual reduction in integer
division cost without a larger register/branch cost. Run existing exact
coverage parity fixtures, then a frozen same-binary OFF/ON game comparison.
No FPS improvement is claimed by this audit supplement.

Sources
See sources.json for pinned repository references and provenance.
