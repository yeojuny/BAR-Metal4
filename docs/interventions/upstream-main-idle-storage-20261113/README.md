# Bounded idle main framebuffer prototypes

Main framebuffer storage now caches at most two immutable, unbound allocation
prototypes. A prepared descriptor copies destination/resolve state but retains its
exact prototype. It never mutates the cached prototype. This ownership pin follows
runtime lifetime, terrain snapshot, postprocess/distortion, readback and screen
continuation owners. Continuation still uses its existing frame and does not
re-prepare or follow resize.

The production idle-pool policy reuses only compatible entries with no external
owners. It scans all slots for a compatible idle entry before choosing an idle
mismatched replacement. Allocation and descriptor/retention preparation complete
before any cache replacement. Failed preparation preserves entries and allocation
generation counter. A reused prototype keeps its allocation generation; the
selected generation is not a monotonically increasing per-frame number.
Two busy entries yield an uncached owned allocation, not a wait or
unsafe overwrite. Clear removes cache ownership, never external pins.

This avoids the single-slot policy that would allocate afresh on every ordinary
one-pending-frame handoff. Two cached sets may consume more memory than the old
single set. This is a bound on cached sets, not total outstanding allocations or
bytes; temporary extra retained owners may require uncached fallback. Existing
retirement barriers remain because raw texture aliases are not counted. No
lookahead admission or runtime speedup is claimed.

Actual production-template ASan/UBSan tests: 1000 alternating one-outstanding
frames stabilize at two allocations; 3000 factory/retention failures across new
and cache-hit paths preserve generation/cache; copied/continuation-style pins,
compatible-slot preference, pinned-slot exclusion, busy fallback and clear.
Native descriptor/prototype lifetime and continuation wiring are structural
checks. Objective-C++ compilation, Metal execution and real GPU memory/performance
measurements remain pending. This does not roll back RuntimeLease bookkeeping.

## Cropped terrain depth follow-up

The same bounded idle policy now owns the terrain-only cropped depth pair, rather
than accepting the renderer's latest raw sample/single textures as reusable input.
Each snapshot pins its exact pair and main frame while retaining its own fresh
encoded flag. Compatibility includes required image presence, extent, device,
format, sample count, texture type and private shader-readable storage. N=1
continues to alias one image when both consumers are requested; multisample and
resolved images remain distinct. The full pair and retention prepare before pool
publication. Static clear drops cached owners only. Encoding and barriers are
unchanged. Native pair routing/selection checks are structural; the actual shared
pool policy retains the sanitizer coverage above. This is not a native Metal pass.
