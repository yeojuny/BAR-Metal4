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

## Successful receipt continuation through later original capture

The opt-in early mode 2/3 path now transfers its local Join into the completed
capture's CPUReadReceipt220 rather than waiting at PrepareWorld return. This
extends immutable opaque-row preparation across Game.cpp's subsequent original
DrawWorld, effects/overlay, DrawScreenEffects, screen/minimap and DrawScreenPost
capture. No callback moves off main; renderer Encode and alpha/water capture
ownership gates are unchanged. Modes 0/1 and non-early mode 2/3 retain the old join.

The receipt's guard is declared after Job, so destruction joins before job and
source owners release. The local guard still joins on prefix/drain/particle or
later PrepareWorld failure before transfer. DrawWorld explicitly joins before
Matches, RowDigest, error access or packet adoption. Hidden/skipped receipts join
on reset; native Game catch joins before its first Lua abort; AbortPreparedWorld
joins before quarantine checks or source rollback; common renderer control drain
joins first. Session resets receipt before worker destruction and source detach.
Worker Build never owns or destroys the receipt, so these paths cannot self-join.
Captured worker errors still surface only at the existing accepted DrawWorld
receipt consumption; abort/hidden paths discard them as before. Timing mainEndNs
still measures PrepareWorld capture only, not the newly overlapped later callins.

Transitive ownership evidence: NativeCUSModelCapture assigns geometry through
WorldFrameCapture::CaptureModel. State::Model copies each vertex field and index
vector under CModelsLock into a new WorldModel, then caches const owners, not
original S3DModelPiece pointers. NativeCUSPublishedStorage::Publish allocates a
new buffer and copies modified pages; NativeCUSUniformStorage publishes fresh
matrix/parameter/publication objects. Instance snapshot and sealed roster are
retained inputs. ValidateRenderReads traverses those snapshots only; its geometry
cache and active read scope are thread-local. Source callbacks can publish new
values without mutating the worker's retained values.

Validation extends the extracted actual receipt/launch fixture with transferred
consume and hidden/abort destruction, main-thread last-owner release, and repeated
live transform/parameter publications after handoff while old captured bytes stay
unchanged. Existing distinct old/new packet/TLS tests remain. This proves source
ownership and sanitizer-tested behavior, not positive native overlap, Metal
correctness, visual parity or a performance gain. General renderer preparation
parallelism remains unfinished.

Verification for this continuation: extracted actual receipt/launch ASan/UBSan
fixture PASS independently; capture-boundary 1,000 and surface-control 15,000
regression cases PASS. Full SceneBridge.cpp and Game.cpp Linux host translation
unit syntax checks PASS with actual SDL3/include dependencies, no replacement
stubs. These are not Objective-C++/Metal builds or app execution.

## Ordered alpha/shadow continuation

Successful early mode 2/3 jobs now retain one CPU read scope across opaque reads
and a single sealed continuation. At the end of PrepareWorld, main publishes
already-captured alpha models in original array/pass order (lua, then MRT lua),
followed by originalShadow CUS. These are exactly the sources formerly primed on
main in DrawWorld; reflection/refraction CUS scans are not added. All alpha/water/
Lua producer callbacks remain main-thread, at their original positions.

The same scope preserves deduplication, EntryLimit and ByteLimit across the
boundary. Job owns both immutable source lists; worker never consumes a live
provider. Tail publication/cancellation use one mutex and terminal state:
Waiting becomes Published or Cancelled, never both; duplicate/late publication
is rejected. Every Join cancels a still-missing tail before waiting, so capture
failure, control, hidden and teardown cannot strand the worker. Published empty
tails are valid. Setup/publication allocation failure cancels/joins and disables
acceleration, retaining the original main-side fallback. No second job is queued.

Receipt acceptance checks exact alpha/shadow owners, source draw IDs, ordered
source lists and Published state. Mode 3 compares the full original opaque→alpha→
shadow scope. Accepted tails are not primed twice. Worker exceptions are consumed
at DrawWorld before renderer preparation, as before; a tail exception (including allocation failure) now precedes
the diagnostic trace and mode-3 synchronous re-priming instead of following them, with no renderer/source action
between those old/new diagnostic positions. Modes 0/1 and non-early mode 2/3 keep
the old synchronous alpha/shadow boundary.

`tail_wait_ns` is separate from active `work_ns`: waiting for the main publication
is subtracted from work time. Prior `main_end_ns` still marks PrepareWorld, not
later Lua/UI capture. This is CPU preparation overlap, not concurrent GPU Prepare:
DrawPrepared still exclusively owns impl map/model/texture caches, RuntimeLease,
submissionReads and pendingMipTextures. No renderer access gate was weakened.

Tests use actual extracted launch and tail-publish code, real read-row validation,
Published/Cancelled/empty/duplicate cases, simultaneous publish/cancel races,
exact owner/draw mismatch rejection, and a 4,095 opaque-row prefix plus duplicate
and new tail rows crossing EntryLimit 4,096 against one original synchronous
scope. Extracted actual launch/tail fixture ASan/UBSan PASS; independent isolated
Linux ThreadSanitizer run also PASS without race reports. Full SceneBridge Linux
host translation-unit syntax PASS. Native Metal execution, native TSAN, visual
parity and measured positive overlap/performance remain unverified.
