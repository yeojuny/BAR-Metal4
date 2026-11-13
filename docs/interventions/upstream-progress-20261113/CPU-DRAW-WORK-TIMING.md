# P5 exact draw and wait-aware timing

Production CPU trace now carries source_draw plus draw_valid, separate from the
simulation frame (multiple draws may share a simulation step). Draw zero is valid;
changing a scope's assigned draw invalidates its record rather than silently
misattributing work. Existing world/prepare records receive the captured source
identity. Frame/sample ancestry and common steady-clock endpoints remain intact.

Two additional actual scopes are wired:
- kind 5: Game recording, after source-credit acquisition, ending before DrawWorld.
  It is a recording envelope, not all Scene finalization. It can include explicit
  controls such as PrepareWorld's surface drain and cannot be called wait-free.
- kind 6: encode work after real Begin and Begin notifications, ending before
  encoding diagnostics, FinishResourceCommit and submission. It excludes known
  outer runtime lock/retirement, drawable acquisition, Begin, residency semaphore
  waits and GPU submission wait. Metal API or screen mutex stalls remain possible.

Every sampled scope also records thread_cpu_valid/thread_cpu_ns using the executing
thread's CLOCK_THREAD_CPUTIME_ID. This total excludes blocked/descheduled time.
Unsupported/failed clock reads or reversed clocks remain invalid, not zero work.
Thread CPU totals plus wall intervals do NOT reveal the location of on-core slices:
a positive wall intersection and nonzero CPU totals are still not proof that two
threads executed simultaneously. OS scheduling traces or finer wait instrumentation
are required for that assertion. These are diagnostic data, not throughput gains.

Preparation deliberately retains its original scope rather than claiming to have
removed all its conditional prior-retirement/provider/resource waits. Trace scope
capacity now accommodates nested prefix work under Game recording; existing sample
and byte/time budgets remain bounded. Native macOS compile/runtime validation has
not run. This change does not enable the pilot by default or alter GPU barriers.

Validation: actual runtime block + real worker ASan/UBSan sampled/every-loop/off;
new kinds 5/6, real Draw(0), repeat-identity rejection, explicit Finish idempotence,
blocked-sleep thread-CPU exclusion, parent/endpoint tests and old ancestry negative
control. Production source-wiring checks verify both scope boundaries.
Independent review: GO with a separate sanitizer run of the expanded driver.
Apple thread-clock and full Objective-C++ integration remain untested here.
