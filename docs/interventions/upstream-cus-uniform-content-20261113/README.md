# CUS immutable program-uniform content

Partial P2 follow-through, not completion of the full upstream handoff.

Each source Program now retains one mutable builder and one lazy immutable snapshot.
Actual active-program/type/finite/integer/arity checks still execute before every
write, including inactive-location handling. Identical bit patterns do not
invalidate the content owner; signed zeros remain distinct. Submit seals only
after changed content. Draws and existing forward/deferred copies share that
uniform owner rather than deep-copying both maps per draw. Engine uniforms,
texture target transport, source sequence, roster, and pose ownership are unchanged.

Validation:
- Production builder and `NativeCUSProgramData.cpp` packing fixture passed
  ASan/UBSan with warnings as errors: defaults, same-content identity, old-owner
  stability, multiple writes, reinitialize, signed zeros, float/int packing,
  late clip transport, and missing-source error.
- Existing six-TU CUS roster/read-receipt suite passed all 48 checks after the
  data-contract change, ASan/UBSan.
- Independent code review found no blocking defect.

No Lua VM, Objective-C++ Metal compilation, macOS app, visual parity, FPS, or
full frame lifecycle validation has run. Typed CUS field packing and frame
manifest sharing remain later work. This does not enable asynchronous capture.
