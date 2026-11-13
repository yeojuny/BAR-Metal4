# P5 CPU-read worker timing evidence

- [x] Record actual worker wall endpoints in the shared steady-clock domain.
- [x] Record actual executing-thread CPU time, with explicit validity.
- [x] Keep measurements opt-in and preserve all scheduling/failure boundaries.
- [ ] Native Mac clock validation and paired workload measurements.
- [ ] Prove active simultaneous execution using scheduling evidence.

The CPU-read worker's existing work_ns is elapsed wall time minus its explicit
continuation wait. It can still contain descheduling and must not be described as
CPU execution time. New worker_begin_ns / worker_end_ns bound actual Build work
including its continuation wait, rather than estimating that envelope from queue
and join duration. They use the same steady clock as submission/main timestamps.

worker_thread_cpu_valid / worker_thread_cpu_ns use the existing per-thread clock
helper on the executing worker. The CPU total includes all measured Build stages,
including wait bookkeeping but not blocked/descheduled time. Invalid/unsupported
or reversed clock stamps produce an invalid zero diagnostic, not a claim of zero
CPU work. All clocks remain behind the existing measurement flag. Measurement-off
jobs retain zero fields. CSV columns are appended; existing columns retain order.

No capture/drain/default/prefix/tail/optional-error behavior is changed. Measurements
are read only after existing join synchronization. Positive wall intersection plus
positive thread CPU totals still does not establish simultaneous on-core slices;
scheduling traces or finer evidence remain necessary. No speedup is claimed.

Validation uses the actual worker and real CPU-read implementation dependencies:
measurement-off zeros, actual waiting-tail execution, invalid/reversed/equal clock
stamps, and whole-Build wall versus CPU accounting. The actual trace function is
extracted and checked with -Wformat=2 -Werror, and the integrated SceneBridge C++
translation unit passes host syntax checking. Existing optional-stage and launch
sanitizer regressions retain their ordering/failure contracts. These host checks
are not Objective-C++ compilation or Apple clock/runtime validation.
