# Earlier opt-in opaque CPU-read work

With the existing Begin-ready pilot AND CPUReadPacket220 mode 2 or 3 requested,
PrepareWorld now starts the existing immutable opaque CUS read-row worker after a
successful terrain/material/model snapshot prefix, before draining the prior
render task. The same worker can therefore work while that older encoding tail
finishes and while the original particle producer runs after the drain. This is
actual earlier work publication, not a queue counter or a new permission for
source callbacks to overlap rendering. Mode 0 and mode 1 keep their old placement;
mode 2/3 outside the Begin-ready pilot also keep the old placement.

Only frame.opaqueModelFrame and its retained normal/MRT Lua sources enter the job.
The existing CPU-only Build/Prime and thread-local geometry cache are unchanged;
no provider, GPU allocation, source callback or Session access occurs on that
worker. The recipe is still consumed at its existing DrawWorld boundary with the
same exact identity matching, mode-3 comparison, fallback priming and delayed job
error delivery. Particle/alpha/water/Lua draw writers remain behind the old-job
drain, GPU pending capacity stays one, and no general renderer-Prepare overlap is
introduced.

The new receipt and Join guard live on main outside the surface boundary. Every
success/throw path joins before that local job is destroyed. Earlier Session
receipt reset remains AFTER old-render settlement; only the new local fallback
value is prepared early. Existing mainBeginNs is sampled after drain and particle
completion, not before the blocking interval. Queue/work/join diagnostics retain
the actual CPU-read job timestamps. These intervals alone are not evidence of
simultaneously active main and renderer work.

Validation: extracted actual launch lambda and receipt identity logic with the
real CPURead worker, CPUReadScope, read-row validation/publication implementations
and surface boundary. 100 cycles exercise valid nonempty younger and distinct
older rows, mode-3 exact content/capacity comparison, nested TLS read views,
prefix/drain/particle exceptions and main-thread source destruction. Invalid draws
also retain the original fallback validation path. Modes 0/1 behavior is checked.
Production ordering guards preserve delayed Session reset and consume location.
ASan/UBSan. No simulated throughput, Metal/native execution, full app build or
performance claim. Worker allocation-failure injection and native scheduling/
retained-byte/latency measurements remain outside this fixture.

Independent review: GO, including separate final sanitizer execution with valid
nonempty row receipts and exact TLS view restoration. SceneBridge full translation
unit host syntax passed with actual SDL3 headers; no ObjC++/Metal compiler or full
application execution was used. Existing surface-boundary and surface-event
regressions pass unchanged.
