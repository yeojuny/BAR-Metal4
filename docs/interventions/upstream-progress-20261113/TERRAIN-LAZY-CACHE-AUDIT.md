# Terrain lazy views: current P5 ownership audit

Decision: preserve `squareViews` and `heightRRRR` lazy resource-local caches. No
additional mutex, COW map or eager view allocation is justified by the current
execution path. This is a scoped ownership conclusion, NOT whole P4 acceptance.

| Boundary | Evidence |
|---|---|
| Next DrawDispatch | FinishCPUWork precedes gate access and another draw |
| Admitted worker | Renderer gate stays owned through DrawPrepared |
| Native preparation | RuntimeLease owns requestMutex for the request |
| Begin-ready release | Releases main's wait, not worker gate/lease ownership |
| Main overlapping prefix | Terrain/material/model CPU snapshot prefix does not invoke native view caches |
| Resource lifetime | SquareImageOwner retains array/view; MapImage owns height view; leases retain GPU allocations |
| ClearStaticResources | FinishCPUWork → gate → RuntimeCleanupLease, then cache reset |

PreparingWhilePending means prior GPU execution versus current CPU preparation,
not two CPU preparation owners mutating one cache. Late encoding-time provider
invocation remains covered by the same owner, so this conclusion does NOT depend
on all view creation occurring before Begin. Views reference the same immutable
storage; they do not rewrite original pixels. Square cache is bounded at256 per
batch; eager materialization would disrupt the current allocation/eviction policy.

- [x] Inspect lazy-cache create/use/ownership and reset paths.
- [x] Independent read-only review reaches same conclusion.
- [x] Update stale renderer-gate test for FinishCPUWork preamble and owned CPU-job
  settlement instead of the superseded synchronous worker.Invoke path.
- [x] Assert Begin-ready does not release handoff; Finish joins before releasing
  handoff and before target/retirement callbacks.
- [x] Gate, control-order and capture-surface source regressions passed.
- [ ] Native Metal/compiler/runtime parity remains deferred.
- [ ] Audit later alpha/water/Lua capture and source-control dependency boundaries
  before considering any widening of the current prefix overlap.

No renderer behavior changed in this audit checkpoint. Test modernization retains
fail-fast gate coverage and adds the current ownership release rules; it does not
simply delete failed expectations. Source tests are not full application tests.
