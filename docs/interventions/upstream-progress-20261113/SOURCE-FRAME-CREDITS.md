# Two retained source-frame credits

Game acquires a typed source lease before UI/world capture, not at worker enqueue.
The noncopyable/nonmovable Session pool has two slots. All retained leases count,
including completed packets still owned by CPU; task Join alone never frees a
source credit. An ordinary aborted capture releases its RAII lease naturally.

Exact admission holds its lease before source transfer. A failed Take releases
that hold; pre-Begin Restore releases it after restoring every ledger. Accepted
and quarantined frames retain it. Deferred receipt application validates the held
credit before source writes, retires exact UI/post/cloud/water owners, and only
then releases the pool's hold. Weak expiry still waits for any retained packet.
When capture has no slot, it drains CPU/GPU work first and retries; if an unrelated
retained packet still occupies both slots, retry refuses instead of exceeding2.

Validation: actual pool sanitizer1000 with capture/GPU holds, duplicate/foreign
identity and thread rejection, CPU-reference retention after GPU retirement;
extracted production Scene record with real pool sanitizer100; independent GO;
SceneBridge host syntax. No native app build or pilot activation.

The first Begin-ready pilot must still drain prior GPU and main receipts before
its dispatch: Begin can otherwise coexist with N-1 GPU work. The source budget
now enforces the limit even if task/readiness timing is misleading.
