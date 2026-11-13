# P5 surface-control prerequisite

The original handoff's U5/resize-close rule applies before native window/layer
mutation, not merely before a later framebuffer resize. The Begin-ready pilot
exposed three actual paths that could bypass CPU settlement:

- Engine events called Surface Refresh (drawableSize/displaySyncEnabled writes)
  before the resize handler reached a drained renderer control.
- PrepareWorld refreshed the layer before renderer preparation's eventual Join.
- Config changes called SDL SetWindowAttributes before UpdateNativeWindowGeometry.

Shared SDL event classification now identifies the exact owned-window refresh/close
set used by Metal4WindowSurface. Scene drains CPU job, exact source notices and GPU
receipts before those actions. Global display add/remove drains before the engine's
subsequent geometry update, including equal-size changes. Other windows, input,
focus and unrelated events do not force a drain. Close is still forwarded only
following the engine's accepted RequestQuit, preserving Lua veto semantics.
Foreign-window events also leave the engine window branch before geometry/quit.
Native dual-screen ConfigNotify only queues an atomic flag; UpdateWindow drains
outside the config mutex before applying dual-screen geometry and ResizeEvent.
Config-driven native window changes drain after the unchanged/null-window fast
paths and before SetWindowAttributes, not once per ordinary loop.

When the Begin-ready pilot is requested, both Scene Refresh sites explicitly drain.
This intentionally limits possible overlap to the earlier capture prefix; it does
not claim concurrent layer mutation is safe because an old layer is retained.
Default joined-path refresh behavior is unchanged. Worker snapshot publication for
an eligible pilot already follows the pilot's explicit prior-GPU drain. SDL/AppKit
may themselves resize native state; these source guards cover engine-owned writes,
not a substitute for real macOS visual/event stress validation.

Validation: shared production classifier/order helper, 15 relevant event kinds x
1000 iterations, failed-drain prevention, foreign-window/noop/topology cases under
ASan/UBSan; source-wiring guards including close veto and config fast path;
SceneBridge and HEADLESS GlobalRendering host syntax; existing Begin-ready
worker/credit and 1000 CPU-job settlement regressions. Independent review: GO. No app build or runtime/positive-overlap/performance claim.

P6 stays proof-gated: the original explicitly requires producer invocation,
content/storage version, view/sampler, subresource/access/order and lifecycle proof
before dedup/copy/barrier changes. This safety prerequisite does not alter barriers,
GPU queue depth, MRT preservation or claim full P3/P4/P5/P6 completion.
