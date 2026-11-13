# CPU result settles the exact source admission before controls

The owned renderer job now retains an optional source-settlement callback outside
its sealed packet/options. This is an acyclic job -> admission -> submission
ownership path. Join, handoff release, target notice, and prior-retirement notice
precede that callback. It still runs inside Settling, so recursive controls are
rejected. Enqueue failure also reports failure after releasing its reservation.
The first render/receipt error survives any source-settlement error.

Scene's durable record accepts a validated deferred result and releases only its
own source/admission anchors at this boundary. Pre-Begin failure restores once;
post-Begin failure keeps the exact held frame quarantined. Synchronous completion
and no-Begin fallback retain the original joined restore/current-source path.
The joined DrawWorld tail does not re-accept an already CPU-settled record. Error
fallback skips accepted/handled records and preserves the original exception if
restoration itself refuses; the durable held record remains for diagnosis.

Direct/readback compatibility uses the same single-attempt notification helper.
Its DrawPrepared scopes and outer cache gate are gone before source notification;
a source-notice scope rejects reentrant renderer controls. A thrown notification
is never replayed as a second failure callback.

Validation: actual job sanitizer 1,000 cycles plus direct scope-unwind, callback
failure and first-error preservation; extracted production scene record 100
cycles including CompleteCPU; independent unique-output review/runs GO; dispatch
and queued-result regressions; SceneBridge host syntax. No native app build/run.

P5 remains disabled/joined. Remaining: bounded Begin-ready eligibility and source
credits, retained queued job result/telemetry consumption, old-frame failure versus
younger capture policy, MRT209 exclusion and measured active CPU overlap. This
checkpoint establishes ordering, not proof that all source reads overlap safely.
