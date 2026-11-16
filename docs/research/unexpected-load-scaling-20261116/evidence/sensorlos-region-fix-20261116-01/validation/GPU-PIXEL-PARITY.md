# SensorLOS sample-1 GPU pixel parity (prepared, not run)

`gpu_pixel_parity.mm` is a standalone offscreen Metal helper. It includes the candidate's exact `NativeSensorLOSShaderSource`, compiles with the engine's Safe math and Precise floating-point functions, and calls the unchanged `OriginalSensorLOSFragment` in both draws. Its test-only vertex function provides identical flat line/data values and selects only the original bounding box or `SensorLOSTightPixel` geometry. An atomic counter requires at least one optimized vertex to execute.

The helper renders to sample-1 BGRA8Unorm, Depth32Float, and Stencil8 attachments and compares every byte of all three readbacks. It prefills depth and stencil with spatial patterns and uses source-alpha blending for RGB and alpha, depth LessEqual, stencil NotEqual/reference 1/Replace, and both depth-write OFF and ON. Its patterned R8 mask is 32×32 for the actual W/4 case; the actual sampler uses nearest-min/linear-mag/edge-clamp with mipmapping disabled. A separate 256×256 mip-stress case enables mip filtering and minification. Cases vary alpha thresholds/discard, widths 1/1.49/1.5/2/3/4.4999/4.5/5/5.01923084/16/64, eight compass directions plus eight shallow/steep signed directions, viewport edges, homogeneous near-plane/viewport clipping, positive-w extremes, subpixel degeneracy, and adjacent two-segment seams. Culling is disabled, as in the native renderer, so reverse directions exercise both windings. A mismatch prints the case and first differing pixel/plane and exits nonzero.

The PASS condition requires actual optimized vertices **and**, in the same optimized case, changed color and stencil relative to a no-draw prefill. Another optimized depth-write-ON case must change depth. A separate atomic counter requires an optimized case where `SensorLOSClipLine` actually changed an endpoint (`enter > 0` or `leave < 1`) and that same case changed color and stencil. Visible optimized x-major and y-major cases and a visible optimized shared seam are also required; subpixel-degenerate lines must never take the tight path. The candidate's `samples != 1` fallback is checked by source inspection and the CPU oracle; this helper tests only actual sample count 1.

After separate build GO, compile only this helper with:

```sh
xcrun clang++ -std=c++20 -fobjc-arc -framework Foundation -framework Metal validation/gpu_pixel_parity.mm -o validation/gpu_pixel_parity
```

Then, after separate GPU test GO, run `validation/gpu_pixel_parity` on the target Apple Silicon GPU. No engine rebuild, game, profiler, or capture is needed for this check. A compile/runtime failure is a test-blocker to diagnose and preserve; it is not pixel-equivalence PASS. This helper compares the geometry/fragment result, not the engine's MTL4 resource lifetime, draw scheduling, or whole-scene image.

The CPU exact-coverage oracle separately passed 8,006 cases / 170,522 accepted samples on 2026-11-15. This GPU helper is uncompiled and unrun as of preparation.
