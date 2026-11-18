# GroundDecal GPU equivalence recovery

The fixture-only follow-up passed its single Metal helper run. This round measured correctness. FPS measurements, engine builds, games, captures and profiles were all zero.

## Prior fixture failure

R82 completed 72 matching color, depth and stencil comparisons and stopped at a combined positive control. Its log omitted the three counters, so the exact R82 counter values cannot be recovered.

The previous fixture packed a glow alpha of 255 in both endpoints. For defined finite inputs, the shader's subsequent glow mix at weight one selects the glow contribution and masks the lit RGB that contains the explosion's minimap sample. This explains why the fixture could show visible decals without demonstrating the expected explosion color response. The fixture also supplied an atlas UV with a negative layer for some cases and a terrain extent below the valid height clamp range.

The recovery kept the original R82 failed run and shader patch byte-identical. It used partial glow, a valid layer and height range, and separate witness counters. It ran against the frozen shader, the current shader with the switch off, and the same shader with the switch on. Metal API validation was enabled.

## Verified output

- All 72 indexed comparisons matched in BGRA8 color and in the depth and stencil arrays. Depth and stencil each changed in 36 enabled cases with zero unexpected attachment changes.
- Changing only the minimap changed explosion color in 4 BC1, 4 RGBA8 and 2 BGRA8 cases.
- All 45 direct float cases matched. Finite values matched bit for bit, and the compared NaN locations matched in classification. Positive and negative infinity were supplied as inputs. This corpus returned no infinite output components.
- These are fixture results. Live fragment coverage and FPS remain unmeasured. GroundDecal stays OFF by default.

The helper and debug process checks were clean after execution. The display was locked and asleep before and after the run. No temporary display assertion was acquired.
