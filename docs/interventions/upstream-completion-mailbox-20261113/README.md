# Data-only completion mailbox (U2 lifetime prerequisite)

SceneBridge deferred completion callbacks no longer capture a raw `Session*`,
read the global `session`, or mutate a live Session deque. They retain an
independently owned mailbox and a shared one-shot ticket containing immutable
epoch/draw/postprocess IDs. The ticket is allocated before invoking the renderer.
Copies of a callback share its publication flag, guarded by the mailbox mutex.
Both queued callback duplicates and replay after Pop are rejected before any
retirement. This guards copies/replays of one ticket, not separate Prepare calls
with identical IDs; the current Draw path creates one ticket per callback.
The mailbox uses the actual native texture context generation as its epoch.

The receipt copies only the fields consumed by main's existing retirement path:
source completion and frame IDs, dynamic-water generation/sequence/source frame,
world-info IDs and water-update IDs. Main pops receipts and applies the original
retirements in their original order. No retirement occurs in the callback.

A mutex serializes fixed two-slot FIFO publication, pop and Close. Valid publication
has no dynamic queue allocation. Wrong epoch, mismatched identity, incomplete GPU
result, overflow and closed-mailbox publication throw the existing deterministic
logic error. Session closes its mailbox before teardown; a late retained callback
can neither dereference the old Session nor mutate its replacement. Construction
is inside the existing initialization try/catch so allocation failure retains
native-context cleanup.

## Verification

```sh
python engine/tools/metal4-contracts/run_native_frame_completion_mailbox_test.py --output /tmp/mailbox
python engine/tools/metal4-contracts/run_native_frame_completion_mailbox_test.py --output /tmp/mailbox-tsan --tsan
```

Both ASan/UBSan and TSan pass 23,018 production-header checks independently:
- FIFO2, exact copied data, empty and bounded overflow;
- epoch, identity, incomplete-result and duplicate/replayed callback rejection;
- retained callback after closed owner, independent replacement epoch;
- 20,000 producer/consumer publications;
- 1,000 Close races with producer publication and consumer pop.

The runner checks that the actual callback contains no Session/global access and
that main consumes this mailbox. Independent code review and both sanitizer runs
passed. Full current SceneBridge TU syntax also passes with the actual SDL3
headers, SDL3/Metal scene/SYNCCHECK/STREFLOP_SSE configuration and the real
TimeProfiler PCH include (including the integrated PBR wiring). No implementation
stubs were used. Objective-C++, Mac, Metal runtime and frame parity/performance
are not tested by these host checks.

This change does not start a worker queue, increase overlap, reserve pre-submit
capacity or remove the renderer's other live-owner accesses. It is not completion
of P3–P5 or proof that the entire renderer is thread safe. The existing safe-GPU-
teardown requirement remains mandatory; Close is a delivery gate, not GPU cancel.
