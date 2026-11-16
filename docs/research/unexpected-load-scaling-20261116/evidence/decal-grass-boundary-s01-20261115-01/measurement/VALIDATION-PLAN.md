# S01 bounded runtime sequence — prepared, not launched

## Frozen identities

- Accepted SensorLOS baseline: `87df3a1bc49a5fca4c47d05de1a41a5d85f464941365b8d20f7ee2eca1e4108f`.
- Signed S01 R2 candidate: `9a44a0b109d77a561f08080dafeac1db8dd5d9655ef6002be379a17feee94aa0`. Attempt 1 remains archived at `attempts/01`; its stopped validation raw remains at `runs/s01_valid01`.
- The fresh R2 validation arm is staged at `runs/s01_valid02/validation_paced`; it has launched zero games. Its child environment explicitly contains `BAR_NATIVE_GRASS_BOUNDARY_S01=1`, bounded `BAR_NATIVE_GRASS_BOUNDARY_S01_PROBE=1`, `MTL_DEBUG_LAYER=1`, and trace/capture/profiler OFF. Clean arms use probe=0. The accepted 87df runtime and source remain intact.

## Gate 1: one candidate Metal API Validation game

Run only after fresh HQ isolation and explicit GO:

`python3 run.py --run validation_paced --suite s01_valid02`

Reuse the accepted Ultra AA0 official Seths combat fixture: full-view spectator, map/seed/camera/font/content pinned, view 1512×950, sample1, VSync0, 10 seconds combat warmup and 40 seconds counted. Check actual child binary and flag, `LimitIdleFps=0`, `option_limitoffscreenfps=false`, native final color/MRT/world draw, positive `[NativeGrassBoundaryS01] split=1` with preserved ordering, normal exit/retirement, zero new Metal API Validation errors, and the internal post-count `scene.png`. Diagnostic FPS is excluded. Preserve raw evidence and stop on a missing positive split, actual runtime fault, or visual regression. Validation and image review require CLOSED before the next GO.

## Gate 2: one separate native frame capture and offline replay

Only after Gate 1 passes and a new runtime GO, adapt the proven private `material-storage-pr7-20261115-01/ultra-compare/gpu-capture-native01/capture_native.py` to the signed S01 product and explicit S01 flag. Use the same fixture; Metal API Validation OFF, `MTL_CAPTURE_ENABLED=1`, actual Layer boundary, one frame near simulation 1590, original backtrace behavior, bounded capture finalization 180 seconds. Capture is a diagnostic game, not a clean FPS sample. If the Layer boundary is unavailable, preserve the receipt and stop without fallback or repeat. After CLOSED, use the proven bounded `analysis/run_replay_profile.py` default GPU/overlapping, 180-second cap, for relative encoder identity and stage intervals. Replayed costs do not establish uninstrumented FPS.

Validation and GPU capture remain separate because capture changes instrumentation and timing. No combined validation/capture procedure has been proven by the existing helpers.

## Later clean comparison

Only after correctness/capture review and a separate fresh GO, stage/run `clean_paced` A1 baseline, B1 S01, B2 S01, A2 baseline, A3 baseline, B3 S01. The baseline uses signed 87df and the candidate signed 9a44. Both retain the same fixture and child flags, including `BAR_NATIVE_GRASS_BOUNDARY_S01=1` (ignored by the older binary); the intentional variable is the binary. Bounded S01 probe=0 in every clean arm. Validation, trace, capture, and profiler are OFF. Keep every valid slow run and compare only these three-versus-three samples.
