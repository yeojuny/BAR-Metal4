# R02 SensorLOS winding correctness candidate

Request: `r02-sensorlos-winding-20261116-01`.

## Source

Reconstructed `a3c7c6bfa93d09dac25755534195932d12fff41a` engine using the R01, S01-final and G01 patches pinned in evidence commit `034339e00be3545316b056d44e470f03959bbc53`. All 10,269 source-file hashes matched G01 (the recorded incidental Python bytecode cache is excluded). R02 changes only the Y-major tight bounding triangles' index order. No G02/G03/G04, pool, cache, queue, encoder, fragment, blend or lifetime changes.

Swapping `(major, minor)` into `(minor, major)` has determinant -1. Reversing each Y-major triangle restores the AABB orientation while preserving the exact vertex set, area, diagonal and fragment inputs. X-major, equal-major ties and failed guards keep existing behavior.

## Checks actually run

`python3 candidate/test_winding.py` (Python 3 + numpy) from this evidence folder.

- 16,294 CPU geometry/source-diff cases passed.
- 5,523 Y-major negative controls reproduce old reversed winding.
- X/Y, reversed endpoints, slopes, ties, clipped endpoints, subpixel/guard fallback included.
- 3,137 sampleCount=4 guard cases preserve fallback. This is not MSAA execution.
- Full G01 source reconstruction hash match; R02 production diff limited to index selection.

This Python geometry oracle is not compiled MSL or a GPU proof. It proves the orientation/vertex-set contract and source-diff boundary. Metal compiler, raster behavior and runtime performance remain unverified.

## GPU helper prepared, NOT executed

`candidate/gpu_pixel_parity.mm` extends the existing exact-fragment parity helper. Compile only when HQ has explicit execution authorization, pointing the include path at this fixed commit's engine:

```
xcrun clang++ -std=c++20 -fobjc-arc -framework Foundation -framework Metal \
  -I <checkout>/engine/rts/Rendering/Metal4 candidate/gpu_pixel_parity.mm -o gpu_pixel_parity
```

The new helper covers separate front/back operations, compare, read/write masks and references; forced depth pass/fail and stencil fail; both front winding conventions; a front_facing probe; and an old-Y-order negative control. It requires actual witnesses, not just equality of blank targets. The probe copies the exact original fragment coverage/discard/depth path, changing only its diagnostic output color. `mode & 4` undoes the R02 Y permutation to restore original R01 vertices.

The helper's sampleCount=4 case checks the production tight-geometry guard while rasterSampleCount stays 1. Actual AA4 GPU parity is not claimed. HQ must report compile errors and observed negative-control/witness counts, not silently relax assertions.

## Remaining

No engine build, GPU helper run, game, benchmark, screen validation or FPS claim. main, release and Homebrew unchanged. This is a source candidate for HQ review, not execution authorization.
