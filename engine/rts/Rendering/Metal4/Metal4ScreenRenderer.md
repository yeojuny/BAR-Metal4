# Direct Metal4 ScreenDrawList consumer

2026-10-14. Codex assisted this source implementation. This owner performed strict syntax checks only. HQ owns linking, actual GPU/capture execution and acceptance. The accepted asset path remains an original-asset diagnostic; the new screen slice does not establish a complete UI, Lua graphics backend or Linux visual parity.

`RenderMetal4Screen(const ScreenRenderRequest&)` consumes the frozen engine `ScreenDrawList` as an immutable completed list. Width/height are explicit pixels. The public header is ordinary C++; returned pixels/timings share `rendering::RenderReadbackResult`, and the existing `AssetDiagnosticResult` name remains an alias. Existing camera/bounds initialization and warmup-frame semantics were retained.

The screen path covers untextured planar geometry with `z==0`, smooth per-vertex color, per-command blend Add with the recorded independent RGB/alpha factors, and optional scissors. Fonts, textures, matrix stacks, arbitrary z projection and general legacy quad/provoking-vertex parity remain unsupported. No hidden backend or simpler graphics route is substituted.

MSL explicitly declares packed position/color fields matching the frozen 28-byte `ScreenVertex` layout (color offset 12). Pixel x/y are mapped to clip coordinates with constant w=1; bottom-left y is converted by Metal's viewport mapping. All indices, command order, the recorder's 0–2 quad diagonal and degenerate submitted geometry are preserved. Culling is disabled, no depth attachment is created, and the encoder has no depth-stencil state. Each command selects its exact blend-state pipeline and sets a scissor/full framebuffer rectangle anew, avoiding state leakage after clipped draws.

Scissor x/y are signed bottom-left coordinates and extents are unsigned. `Metal4ScreenScissor.h` calculates endpoints in signed 64-bit space, clamps all endpoints to the framebuffer and converts y to a top-left rectangle. Empty/fully clipped rectangles skip their draw instead of submitting invalid zero/outside Metal scissors. The recorded geometry remains unchanged. The CPU fixture covers negative origins, right/top overflow, empty/offscreen rectangles, absent scissor and INT32/UINT32 extremes.

Dimensions of 1–4096, 1–300 measured frames, 2 million vertices, 12 million indices and 100,000 commands are explicit diagnostic allocation limits. They are not advertised product resolution limits; future game settings, including explicit ultrawide/5k values, need device-based validation. No implicit Retina multiplication occurs.

## Shared runtime and lifetime

`Metal4RuntimeNative.h/.mm` holds the single actual process-wide default native device, MTL4 queue, allocator, command buffer, compiler, shared completion event and residency set. Both the asset and screen renderers hold a serialized request lease and use those same objects. Request resources populate that shared residency set and are removed only after completed submission/readback. The private runtime device is available to native code so later surface attachment can inject this same selected device.

The queue's completion event values increase monotonically across both render families and every request. After commit, both the shared event and commit feedback must complete before allocator reset. A recording/timeout/GPU error poisons the runtime; later requests reject reuse rather than reset potentially live command memory. The residency allocations of an unsafe request remain intact. Each feedback callback captures an immutable strong graph containing all GPU resources plus queue, allocator, command, event, compiler and residency, preserving lifetime even when a CPU wait times out and the callback arrives later. The callback does not access a possibly destroyed C++ lease.

Capture remains device-scoped and separate from shader-validation runs on the host that rejects their combination. Pipeline shader validation is selected per request/pipeline, not globally inferred from SDK support. GPU feedback execution duration and CPU preparation/submission/wait/readback remain separate clocks; simulation remains absent. This serialized offscreen tool does not provide game FPS or prove asynchronous surface presentation.

HQ reported the shared-runtime asset regression passed with Metal API and shader validation: `commander-shared-runtime-09.png` matched `extracted-08` at 1024×1024 with **zero differing RGBA channels and maximum delta 0**. AssetBoundary and SurfaceState CTests passed. This validates asset extraction behavior after the common-runtime change; it does not yet validate the new screen shader.

## Actual FlowUI fixture and independent pixels

`tools/metal4-contracts/ScreenDrawListGPUHarness.mm` reads the actual `work/validation/lua-screen/flowui-rectround.json` produced by executing the pinned official FlowUI helper through the existing Recoil Lua CPU fixture. It reconstructs the frozen command batches through `ScreenDrawList`, verifies exact vertex bits, indices, command counts and quad order, and then submits that list to the direct native screen consumer. Empty snapshots produce a clear-only native pass.

Independent analytic samples check cut versus square corners, constant-color interiors, continuous alpha, the known red→blue vertical gradient, inherited color/blend/scissor state and empty snapshots. Separate controlled draws check standard alpha, additive alpha, independent RGB/alpha factors, blend-disabled smooth interpolation, negative/empty/offscreen scissors and restoration to an unclipped command. Standard alpha is checked as `sourceAlpha*sourceAlpha + destinationAlpha*(1-sourceAlpha)`; no source-alpha factor is silently corrected to One. Sample coordinates use pixel centers away from ambiguous triangle boundaries. RGBA8 UNORM comparisons allow **2 integer channel values** for quantization/interpolation rounding; clear targets are accounted for as quantized before blending. Unsupported nonzero z and unfinished batches are rejected before native submission.

These tests use scalar equations and known fixture geometry, not the native shader as their reference. They are a scoped transport/raster check. No Linux-rendered reference was supplied, so Linux screen appearance, general legacy quad edges and complete FlowUI/font/material behavior remain unaccepted.

## Build/run handoff

HQ's existing `recoil-metal4-renderer` target must compile `Metal4RuntimeNative.mm`, `Metal4AssetRenderer.mm`, `Metal4ScreenRenderer.mm` and the frozen `Backend/ScreenDrawList.cpp`. Objective-C++ files use C++23, ARC/blocks and the selected SDK; link Foundation/Metal. The GPU harness links that renderer library plus CoreGraphics/ImageIO. No CMake file was edited by this owner.

The pure CPU scissor target uses `Metal4ScreenScissorTests.cpp` with `METAL4_SCREEN_SCISSOR_TEST_MAIN`; it needs no Apple/SDL library. The test main is guarded so an engine source glob cannot add another entry point.

Strict checks passed:

```sh
xcrun clang++ -std=c++23 -fobjc-arc -fblocks -mmacosx-version-min=26.0 -arch arm64 -fsyntax-only -Wall -Wextra -Wconversion -Werror rts/Rendering/Metal4/Metal4RuntimeNative.mm rts/Rendering/Metal4/Metal4AssetRenderer.mm rts/Rendering/Metal4/Metal4ScreenRenderer.mm tools/metal4-contracts/ScreenDrawListGPUHarness.mm
xcrun clang++ -std=c++23 -x c++ -fsyntax-only -Wall -Wextra -Wconversion -Werror -include rts/Rendering/Metal4/Metal4ScreenRenderer.h /dev/null
xcrun clang++ -std=c++23 -fsyntax-only -Wall -Wextra -Wconversion -Werror -DMETAL4_SCREEN_SCISSOR_TEST_MAIN rts/Rendering/Metal4/Metal4ScreenScissorTests.cpp
```

HQ runtime argument shape:

```sh
MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 <build>/screen-gpu-harness --fixture <work>/validation/lua-screen/flowui-rectround.json --report <work>/validation/lua-screen/gpu.json --output-dir <work>/validation/lua-screen/gpu --shader-validation on
METAL_CAPTURE_ENABLED=1 MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=0 <build>/screen-gpu-harness --fixture <work>/validation/lua-screen/flowui-rectround.json --report <work>/validation/lua-screen/capture.json --output-dir <work>/validation/lua-screen/capture --shader-validation off --capture <work>/validation/lua-screen/fresh.gputrace
```

Capture applies to the actual `cut_corners` fixture pass, then subsequent fixture/control passes reuse the same runtime without capture. All requests must succeed and all analytic checks pass for the harness's final `passed` status. HQ still must execute the GPU harness, inspect validation/trace logs and record artifacts. Actual simulation-driven terrain/unit rendering and native drawable presentation remain the next engine integration work.
