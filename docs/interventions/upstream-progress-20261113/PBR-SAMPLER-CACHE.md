# P6: bounded PBR immutable sampler cache

PBRSourceGPU::Image previously created a new Metal sampler for each model/UI source
lookup. Repeated production calls exist through WorldRenderer OriginalSourceImage
and the PBR ExternalSample route; both may access the same BRDF/EnvLUT many times.
The cache contains only two immutable sampler states for one strongly held device.
This is descriptor/resource bookkeeping, not a resource-producer scheduling change.

The exact original descriptor varies only with minFilter == 0x2601. Both min/mag
are that linear/nearest result. mipFilter remains NotMipmapped, all three address
modes ClampToEdge, supportArgumentBuffers YES. Every other descriptor field (including
compareFunction, borderColor, anisotropy and LOD clamps) remains untouched from the
same fresh MTLSamplerDescriptor defaults as before. No incoming sampling field besides
that boolean previously affected this descriptor; those semantics are unchanged.
ValidateNativePBRAllocation requires matching original min/mag and wrapS/T/compare;
Image still checks exact captured state == allocation.sampling before cache lookup.

Strong device identity prevents pointer-reuse hits. Different-device requests create
successfully before clearing the old two slots and publishing the new identity.
Nil or throwing creation does not cache failure or displace the old cache. Successful
old leases retain their sampler even after replacement; every consumer still executes
Storage, texture AddAllocation, exact sampling validation, sampler Retain, and a fresh
storage/submission-capturing Readable callback. Allocation-only/same-submission/completed
readability logic is unchanged. Encode compute dispatches and barriers are untouched.
Cache is owned by the existing serial PBRSourceGPU implementation; no new concurrency
or persistent global cache is introduced. At most two states plus one device persist.

run_pbr_sampler_cache_test.py extracts the actual cache with shared_ptr stand-ins for
strong Metal references: 2000 hits, distinct filter/device, strong device lifetime,
rollover preserving old consumer references, nil/throw failures and retry. Structural
assertions freeze the actual descriptor and validation/retain ordering. ASan/UBSan
PASS. Metal descriptor defaults and actual ARC/device execution are not tested on this
host; no native runtime/performance claim or full app build/run. Cold successful sampler
retention is intentionally longer (PBRSourceGPU/cache lifetime), bounded to two.

Concurrency precondition: both production routes are inside the existing renderer
invocation/provider access gate (including UI ExternalSample callback). The cache
is intentionally unsynchronized and depends on that same serialized contract as
PBRSourceGPU's pipeline/resource preparation state. Const Image does not imply
concurrent cache access permission. Independent coordinator review and sanitizer
rerun passed; no lock or gate was relaxed.
