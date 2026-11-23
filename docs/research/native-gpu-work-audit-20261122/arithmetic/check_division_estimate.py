"""Audit-only arithmetic check; no engine or Metal benchmark is performed."""
import itertools
import json
import pathlib
import numpy as np

Q = 1 << 22
rng = np.random.default_rng(20261121)
summary = {"seed": 20261121, "scope": "Host numpy float32 estimates plus exact int64 residual checks. Not an MSL compiler/GPU test.", "groups": []}

def check(name, n, d):
    assert (d > 0).all() and (d <= (1 << 31)).all()
    assert (np.abs(n) <= ((1 << 24) + 128) * (d // 256)).all()
    reference_q = n // d
    reference_r = n - reference_q * d
    q0 = np.floor(n.astype(np.float32) / d.astype(np.float32)).astype(np.int64)
    q = q0.copy()
    r = n - q * d
    corrections = np.zeros(n.shape, np.int32)
    while (r < 0).any():
        m = r < 0
        q[m] -= 1
        r[m] += d[m]
        corrections[m] += 1
    while (r >= d).any():
        m = r >= d
        q[m] += 1
        r[m] -= d[m]
        corrections[m] += 1
    assert np.array_equal(q, reference_q)
    assert np.array_equal(r, reference_r)
    assert ((q*d <= n) & (n < (q+1)*d)).all()
    # Verify the R75 lower-edge formula against its original exact floor expression.
    width = rng.integers(1, 65, n.size, dtype=np.int64)
    bias = np.where(width & 1, -(r == 0).astype(np.int64), (2*r > d).astype(np.int64))
    lower = q - width//2 + bias
    reference_lower = (2*n - (width-1)*d - 1) // (2*d)
    assert np.array_equal(lower, reference_lower)
    summary["groups"].append({"name": name, "cases": int(n.size), "quotient_remainder_matches": int(n.size), "wide_line_lower_matches": int(n.size), "estimate_needed_correction": int((corrections > 0).sum()), "maximum_observed_corrections": int(corrections.max()), "exact_integer_ties": int((r == 0).sum()), "exact_half_ties": int((2*r == d).sum()), "negative_numerators": int((n < 0).sum()), "max_abs_numerator": int(np.abs(n).max()), "max_denominator": int(d.max())})

def segment_group(name, a, b, major_pixel):
    delta = b-a
    nonzero = (delta != 0).any(axis=1)
    a, b, delta, major_pixel = a[nonzero], b[nonzero], delta[nonzero], major_pixel[nonzero]
    xmajor = np.abs(delta[:, 0]) >= np.abs(delta[:, 1])
    ma = np.where(xmajor, a[:, 0], a[:, 1])
    mi = np.where(xmajor, a[:, 1], a[:, 0])
    dm = np.where(xmajor, delta[:, 0], delta[:, 1])
    di = np.where(xmajor, delta[:, 1], delta[:, 0])
    n = mi*dm + (major_pixel*256+128-ma)*di
    d = dm*256
    negative = d < 0
    n[negative] *= -1
    d[negative] *= -1
    check(name, n, d)

vals = [-Q, -Q+1, -256, -129, -128, -1, 0, 1, 127, 128, 129, 255, 256, Q-1, Q]
major_pixels = [-32768, -32767, -16384, -16383, -1, 0, 1, 16383, 16384, 32767, 32768]
tuples = np.asarray(list(itertools.product(vals, repeat=4)), dtype=np.int64)
segments = np.repeat(tuples, len(major_pixels), axis=0)
segment_group("endpoint_boundary_grid", segments[:,:2], segments[:,2:], np.tile(major_pixels, tuples.shape[0]))

count = 1_000_000
segment_group("random_valid_quantized_segments", rng.integers(-Q, Q+1, (count,2), dtype=np.int64), rng.integers(-Q, Q+1, (count,2), dtype=np.int64), rng.integers(-32768,32769,count,dtype=np.int64))

rows = []
for dm, quotient in itertools.product([1,2,3,127,128,129,255,256,257,(1<<22)-1,1<<22,(1<<23)-1,1<<23], [-65536,-32768,-16384,-1,0,1,16384,32768,65535]):
    d = dm*256
    for remainder in [0,1,d//2-1,d//2,d//2+1,d-1]:
        rows.append((quotient*d+remainder, d))
nd = np.asarray(rows, dtype=np.int64)
check("quotient_and_remainder_ties", nd[:,0], nd[:,1])
summary["total_cases"] = sum(g["cases"] for g in summary["groups"])
summary["result"] = "PASS"
summary["proof_distinction"] = "Residual invariant is mathematical; one correction maximum is observed here only. Retain correction loop/fallback rather than assuming all MSL targets obey this observed bound."
path = pathlib.Path(__file__).with_name("division_estimate_results.json")
path.write_text(json.dumps(summary, indent=2)+"\n")
print(json.dumps(summary, indent=2))
