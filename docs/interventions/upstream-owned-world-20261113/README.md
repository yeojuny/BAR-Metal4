# Owned normal world dispatch packet

SceneBridge moves WorldFrame into shared storage without copying its geometry,
finishes source attachments, then seals a const packet before transferring the
UI/postprocess/cloud journals. The mutable owning alias is released; later result
and abort checks read the packet's const frame. Packet allocation failure precedes
journal transfer.

The packet copies render options and the ScreenFrameView descriptor. It retains
the screen's immutable storage and independent shared postprocess handle, and
checks exact root pointer plus target span against that storage. Its options point
to its own descriptor; copy/move of the packet is prohibited. Missing PBR capture,
borrowed readback and borrowed screen geometry are rejected, not silently retained.
Engaged-empty PBR receipts stay empty.

Only owned dispatch enters the render worker. Borrowed direct APIs continue on the
main synchronous path. The worker captures the packet by value instead of borrowed
WorldFrame/options references. Surface snapshot, presentation policy, target notice,
previous-retirement callback and handoff reservation keep their existing synchronous
lifetimes: Invoke must join on success and failure. This is NOT asynchronous queue
readiness. The packet pins CPU dispatch input; the runtime's existing retained graph
still owns resources through deferred GPU completion.

Host ASan/UBSan tests cover move identity, caller-storage destruction, independent
screen descriptor lifetime, invalid borrowed inputs, null screen and 1000 serial
worker handoffs. Full SceneBridge C++ syntax and structural pre-transfer/worker
wiring checks accompany them. No Objective-C++/Metal execution or measured speedup
is claimed. Transitive mutable registry/services, control scheduling, GPU cache
versions and dependent younger capture still need separate ownership treatment.
