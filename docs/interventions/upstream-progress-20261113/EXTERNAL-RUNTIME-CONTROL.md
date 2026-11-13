# External Runtime control boundary (R1)

## Change

`scene::FinishRuntimeControl()` reuses the existing scene drain, not a new
completion mechanism: CPU-read preparation finishes; the exact renderer CPU job
joins and delivers Begin/CPU admission notices; then GPU retirement runs and the
scene applies its exact queued receipts. All of this precedes a new Runtime lock.
No gate exception is suppressed, no GPU completion is synthesized, and GPU pending
remains limited to one.

Metal-info's registered pre-control callback runs **before** `Mutation` and before
copying the next source candidate. It is not inserted inside `Consume`. Recursive
consumer updates are rejected before the callback. Inactive updates do not call
the drain, so ordinary capture without this GPU control keeps its overlap.
UnitUniformCopy compile and dispatch use the same boundary before their leases.
Metal-info resource reset and loading/reload screen draw also explicitly cross it.

## Runtime-entry audit

| Entry | Owner/order |
| --- | --- |
| MetalInfo initialize | Main; explicit boundary before lease. Initial call occurs before session publication and has no pending world job. |
| MetalInfo active update/mode select | Registered source callback before `Mutation`; consumer then opens lease. |
| MetalInfo reset/shutdown | Boundary before `CanSafelyDetach`/resource clear; shutdown removes consumer. |
| UnitUniformCopy compile/dispatch | Main check and boundary before lease. |
| Scene loading screen draw/clear | Draw explicitly drains; EndLoadingScreen already drains before cleanup lease. |
| World renderer constructor | Session construction, before worker/source callbacks are published. |
| World renderer runtime controls/cleanup | Existing `FinishCPUWork()` before gate/runtime; worker controls require prior retirement. |
| PBR source compile | Existing stable finishCPU callback before gate and runtime. |
| World draw worker | Own handoff and explicit previous-retirement receipt; not a source control entry. |
| `Metal4RendererCanSafelyDetachSurface` | Scene caller already finishes CPU first; renderer calls have their own settlement. |
| ScreenRenderer / AssetRenderer | Standalone diagnostic/harness entrypoints (`RenderMetal4Screen`, `RenderMetal4Asset`), not live Scene source controls. |
| GPU family methods accepting `RuntimeLease&` | Borrow caller's lease; do not acquire another runtime lock. |

Audit scope: actual lease constructors, cleanup leases and CanSafelyDetach callers
in Rendering/Metal4, plus their Scene/live target callsites. This is not a claim
that all unrelated engine APIs have been exhaustively proven.

## Validation

- `run_native_external_runtime_control_test.py`: PASS, ASan/UBSan with leak checks
  disabled because this host runs under ptrace. Real production gate, serial
  worker and CPU-job settlement; verbatim source `Mutation`/`PrepareControl`.
  200 controls cover pending worker and finished-but-unjoined worker, main gate
  rejection before Join, exact CPU notice before simulated retirement, inactive
  overlap, source-transaction ordering, recursion and prior failure propagation.
- Its ObjC++/Scene callsite assertions are source wiring checks only.
- Existing renderer-access runner updated from the obsolete eleven callbacks to
  eight gated live providers plus three exact owned readability receipts; PASS.
- Metal-info capture runner now has explicit WorldInfo/terrain-publication test
  adapters and a CXX/clang++/g++ compiler fallback. Its fixture compiles with g++.
  Execution requires the pinned BAR test31657 `infoMetal.lua`, not this checkout's
  different engine springcontent shader; the runner now reports this prerequisite
  explicitly rather than failing a mode assertion. The production pin is unchanged.
- Full native target compilation and actual Mac maintenance/pilot execution are
  separate requirements. No game was launched for this change.
