# Normal-map completion follows its exact resource owner

Removed the renderer-global `normalsReady` flag. The current terrain height/
normal allocation retains a typed MapImage owner, and the published normal
backing shares that exact owner. Height changes or allocation replacement reset
the owner. Deferred completion updates only its retained backing's MapImage;
it cannot set a readiness flag belonging to a newer generation.

The original retire-before-height/map-change barrier remains. The GPU normal
texture and height buffer may still be reused in place, so this is completion
state ownership, not concurrent GPU resource renaming or enabled CPU lookahead.
Source version reuse is checked against the exact typed owner, not just a reused
MTLTexture pointer. An old source version cannot silently acquire newer contents.

Validation is explicitly structural/source review only for this Objective-C++
change: `check_native_normal_cache_ownership.py` verifies no global readiness
flag, owner resets, exact fulfillment/equality and retained completion routing,
and the pre-existing retirement barrier. A real Mac compile and Metal workload
remain mandatory at integrated validation; no host stand-in is claimed as GPU
execution or as an Objective-C++ compile.

Actual CPU backing test: NativeMapTextureBacking::Fulfill is linked from production
and exercised with a CPU-only Resource stub for 100 retained old-owner completion
and duplicate-fulfillment cases under ASan/UBSan. This verifies the backing's
lifetime contract, not MapImage/MTLTexture allocation or the Objective-C++ body.
Independent source review returned GO, including unchanged-owner reuse and
pre-Begin allocation failure retryability.
Final independent test review also reran the combined structural/CPU-backing
fixture and returned GO. Sanitizer fixture passed all 100 cycles.
