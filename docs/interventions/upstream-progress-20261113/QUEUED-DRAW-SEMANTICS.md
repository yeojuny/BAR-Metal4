# Accepted CPU frame versus presented frame

The Game/SwapBuffers boundary now uses `DrawResult::AdvanceCPUFrame()` rather
than treating every accepted frame as a real Metal presentation. Existing
synchronous results are unchanged. No producer returns queued work yet.

A future queued result must have its separate queued postprocess serial and
cannot carry presentation, completed source, completed targets, or completed
postprocess serial claims. Game validates before any source retirement. Accepted
CPU work can advance the original copied per-frame render buffers without
inventing GPU completion. FrameMark/CPU draw time remain CPU admission timing,
not display-throughput evidence.

Actual header sanitizer fixture covers all existing aggregate flag combinations,
queued CPU advancement, all completion-flag conflicts, wrong/missing serials,
and intentionally permits draw zero. Independent reviewer: GO. SceneBridge host
syntax passes. No native/app build, async producer, or measured overlap.

The durable exact scene admission record and bounded begin-ready return remain
required before enabling P5.
