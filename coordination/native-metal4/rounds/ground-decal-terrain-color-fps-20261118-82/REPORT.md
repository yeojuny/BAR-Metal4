# GroundDecal terrain color experiment

The fixed round stopped at its single Metal parity helper. There is no FPS comparison. The helper compiled once and ran once. Engine configure, engine build and games remained at zero.

## Applicability

The preserved native R03 draw at simulation frame 1590 uses the same Seths Ravine archive and game package. API 5327 binds the immutable GroundDecal data and API 5342 draws 32,279 instances. The preserved device resource records the corresponding 6,197,568 active bytes and the original GroundDecal immutable revision label. Decoding the low four bits of each 192-byte row at byte offset 144 gives 32,279 type-3 instances. The producer assigns type 3 in `AddTrack`. The measured visibility and current live-frame branch coverage remain unknown.

## Candidate and outcome

The isolated source copies the R81 baseline and adds a default-off AA0 shader variant for safe math and known normalized minimap formats. Known plate, track and Lua decal types with finite main color and minimap coordinates skip the explicit-LOD terrain-color read. The existing calculation handles explosions, other types, and exceptional inputs. Atlas sampling, materials, bindings, direct draw order, blend, resource lifetimes and default flags were preserved.

The Metal helper reached the end of 72 indexed GroundDecal draw comparisons across shadows, three normalized formats, and distinct type domains. The compared BGRA8 color, depth and stencil arrays matched the frozen source before the helper failed its aggregate positive-witness assertion. At least one of the depth-write, stencil-write and explosion terrain-color sensitivity counters was zero. The helper logs the combined assertion and did not preserve the individual counts. The standalone float/NaN comparisons were after that gate and were never reached. This is an incomplete correctness result.

The display was locked and asleep at preflight and cleanup, with no active display or active GPU debug sessions. No display assertion was acquired. The previous source, captured evidence, and failed helper receipts remain preserved. No retries, new capture or measurement instrumentation occurred.
