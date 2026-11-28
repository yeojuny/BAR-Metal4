# CPU post-completion tail: future read-only analysis contract

This is a specification, not a run result. No parser was run against newly produced
trace data, and no performance value is claimed. Keep the complete original
`engine.log` and `infolog.txt`; select one as the authoritative trace stream and
do not count duplicated records from both logs.

1. Parse `[NativeCPUTrace]`, `[NativeCPUTraceBuffer]` and
   `[NativeCPUTraceEmission]` as separate record types. Preserve raw line and line
   number, every key, all IDs as exact integers, and nanoseconds as integers.
   Preserve `id,parent,kind,frame,request,request_draw,request_sim,job,job_request,
   job_draw,job_sim,lease,submission,event,runtime_index` and all `retired_*`
   fields, `reason,status,draw_valid,source_draw,thread_cpu_valid,thread_cpu_ns`.
   Reject duplicate IDs, malformed/truncated records, negative intervals and
   ambiguous identity joins. Log emission order is not execution order.

2. Check the manifest-delimited buffer cardinality against `records`, require
   `dropped=0`, and retain every missing/dropped count. Capacity is 128 records
   per buffer, 128 main-thread joined owners, global 8,192 records/4 MiB; global
   budget also counts manifests/emissions and uses formatted payload length +160
   bytes. A missing row/emission, unmatched manifest count, cap exhaustion or
   closure before the requested window is not a complete observation. Require
   an emission receipt with `outside_window=1` after the admitted window closes.
   An already admitted worker may finish after closure: retain its full record
   chain and classify its provenance by request/job simulation. Do not clip its
   wall interval or replace its recorded identity with the log-prefix frame.

3. Use the fixed source's kinds: Encode 6, RuntimeRetire 18, EventWait 19,
   FeedbackWait 20, AllocatorReset 21, GPUCallback 22, ResidencyCleanup 23,
   ResourceCommit 24, Submit 25, ReadChecks 29. Retain other kinds for ancestry,
   especially CompletedOwnerRelease 26 and WorkerJob 27. `status=0` is ordinary;
   1/2/3/4 mean no job/failure/no predecessor/missing sampled job. Do not turn a
   boundary status 4 or zero retired identity into an ordinary successor chain.

4. Build the parent tree and exact job/request relationships before calculating
   time. A prior Submit(N) must match RuntimeRetire's complete nonzero retired
   identity (job, request, draw, sim, lease, submission, event, runtime index).
   EventWait/FeedbackWait and allocator/callback/residency scopes belong to that
   retire operation by ancestry and retired identity. Identify Submit(N+1) in
   the same successor job/worker operation as that retirement, including its
   exact request/job relation and consistent lease/submission/event fields.
   Trace scope `.Identity()` populates the Submit sample, not every surrounding
   context; preserve unset fields and use proven parentage to relate them.
   `request` and `job_request` can name different incoming/joined work: never
   assume equality. Never join only by adjacent source draw or numeric N+1.

5. For an ordinary deferred chain require ordered EventWait then FeedbackWait,
   successful retirement and a successful successor Submit. Confirm successor
   Encode(6) ended before the observed retirement/Submit boundary, using the
   successor job and parent tree. In this source Encode begins at
   Metal4WorldRenderer.mm:4634 and ends at :5761; it excludes subsequent waits
   and submission. Separate early drains, readback, non-present, fallback,
   missing-predecessor and failure paths. ResourceCommit(24) can precede the
   retirement; assign it by its actual endpoints, not by an assumed order.

6. Calculate only same-clock CPU monotonic differences:

   `post_observation_tail_ns = Submit(N+1).start_ns - FeedbackWait(N).end_ns`

   `submit_call_ns = Submit(N+1).end_ns - Submit(N+1).start_ns`

   The first interval includes error checking and subsequent CPU settlement,
   read checks and uninstrumented submit preparation. It starts when the host
   finishes the feedback wait, not at actual GPU completion. Successful later
   scopes/normal exit must corroborate that observation; a feedback error is
   not a successful completion receipt. Submit-call time includes drawable
   waiting and commit/present/signal work and is a separate measurement.

7. Intersect allocator/callback/residency/read-check scope intervals with the
   outer tail. Never sum an outer RuntimeRetire with nested children, count
   overlapping child envelopes twice, subtract before-window work, or call
   wall duration active CPU time. Retain interval unions and unclassified
   remainder. Report valid thread CPU brackets separately. Completed owner
   graph release after Submit is outside the pre-submit tail.

8. Report all admitted buffers/records, complete ordinary chains, boundary-
   censored chains, missing predecessors, early drains, status failures, drops,
   malformed records and ROI coverage. Report distributions and occurrence
   counts only from exact chains, with their denominator and coverage limits.
   Do not label the whole diagnostic complete while concealing lost or censored
   chains. If caps, missing internal ancestry or mismatched retired identity
   prevent a repeated ordinary-chain conclusion, return INSUFFICIENT and stop.
   A few isolated rows, ROI-external witnesses or reconstructed timestamps do
   not establish the requested result. No automatic second game or source fix.

9. Preserve the original scene checks: Seths Ravine Remake 1.3.1, Ultra preset 4,
   AA0, fixed camera/fullview, view 1512x950/drawable 1512x982, fixed RNG seed
   154658555, benchmark seed 7654321, combat command at 240, original pacing,
   540/1740 anchors, existing controller shutdown and normal engine exit.
   Verify actual future child environment and absence of validation/probe
   activation. Do not demand positive G01 probe or Metal-validation markers:
   both are intentionally OFF. The unchanged controller's one post-count
   `gl.SaveImage` witness remains; it is not a GPU-tool capture/replay. Keep its
   raw status. Do not enforce a clean FPS duration/performance gate on trace
   overhead, clip slow bins or pool diagnostic FPS with prior measurements.

10. Only a repeatedly substantial, exactly joined tail supports investigating
    successful read-ready versus reclaim separation and moving settlement while
    preserving pending GPU limit 1. A small tail points back to SensorLOS/R04 GPU
    work; it does not disprove every CPU bottleneck. An insufficient result
    ends this attempt with no retry. No numerical optimization threshold or
    predicted FPS benefit is invented before observing the data.

Source references are the already reverified c341 snapshot:
`rts/Rendering/Backend/NativeCPUPhaseTrace.h`,
`rts/Rendering/Backend/NativeSubmissionTrace.h`,
`rts/Rendering/Metal4/Metal4RuntimeNative.mm:167` (collection),
`:1030` (deferred submission), `:1173` (completion), `:1190` (settlement),
`:1221` (retirement), `:856` (read checks), and
`rts/Rendering/Metal4/Metal4WorldRenderer.mm:4634` (Encode).
