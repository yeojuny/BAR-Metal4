# P6: exact lease-local cube sampler reuse

The fallback cube sampler path formerly created a Metal sampler for every prepare,
including recursive prior preparation. It now reuses immutable sampler states for
an exactly equal complete NativeTextureSampling value in the same RuntimeLease.
The cache is a fixed 32-entry array; saturation uses the original creation path.
The lease owns its one runtime/device and existing serialized request lock. There
is no global state, cross-device reuse, new lock, or concurrency permission.

Every original descriptor assignment and validation still executes before cache
lookup, including unsupported comparison, border, addressing and filter rejection.
The entire original validation/descriptor prefix is frozen by the source test.
Even fields not consumed by this descriptor are conservatively included in key
equality. NaN therefore misses rather than acquiring a new equivalence rule.
Successful creation alone is cached; nil creation still throws the original error.
Optional cache allocation failure falls back to original creation. Every hit and
miss retains its sampler in the requesting lease. The bounded cache extends no
owner beyond that lease; it cannot retain textures or alter producer receipts.

Supplied actualSampler still bypasses this fallback, retains its existing device
check and retention. Cube allocation/upload, predecessor ordering, dependencies,
commands, copies, barriers, readability and completion are untouched. Descriptor
allocation still occurs on a hit; this change only avoids redundant Metal sampler
creation, not all CPU work. Benefits depend on fallback traffic, not asserted FPS.

Validation: run_cube_lease_samplers_test.py executes actual extracted cache code
with the real NativeTextureSampling definition and shared ownership stand-ins.
ASan/UBSan passes 2,000 hits, exact-key misses, 32-entry saturation, nil handling,
NaN misses, independent leases and release lifetime. Structural checks verify
unchanged descriptor validation, success-before-cache insertion and retain paths.
Existing completed cube lease receipt test also passes. Actual ARC/Metal sampler
execution, integrated native build and performance remain unverified. No full app
build/run, GPU pending-depth change, gate relaxation or default-on pilot change.
