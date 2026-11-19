# SensorLOS single-sample fragment review

The patch changes only `OriginalSensorLOSFragment` in the pinned R03 header. The sample-position correction and both coverage predicates run as before. The mask UV depends on the corrected fragment position and the original uniforms. The arithmetic moved below the new discard does not feed the UV. The original implicit-derivative mask sample stays ahead of the first per-fragment discard.

A fragment becomes ineligible under the same viewport and line predicates. After the new discard, only scalar and vector arithmetic, the original alpha test, and the unchanged color and depth return remain. The shader has no later derivative, texture sample, quad operation or storage write. The edit preserves the fragment inputs, sampler, math expressions and compile options used by surviving fragments.

Metal can execute helper invocations for derivative evaluation, and the pipeline can continue a discarded invocation with its writes suppressed. The change therefore preserves source-level sampling participation, and the GPU fixture checks the output of the two compiled shaders against the same mipmapped masks. The amount of work skipped on hardware depends on compiler and GPU behavior. [Apple Metal fragment execution and helper threads](https://developer.apple.com/library/archive/documentation/Miscellaneous/Conceptual/MetalProgrammingGuide/WhatsNewiniOS10tvOS10andOSX1012/WhatsNewiniOS10tvOS10andOSX1012.html)

The early and final discard sites together reject the same fragments. The original alpha predicate, `[[depth(any)]]` result, and native depth/stencil setup remain in force. The existing fixture checks color, depth and stencil against the exact R03 shader, including asymmetric front/back stencil and mip-selection stress. Only the existing fixture's reference shader binding is changed.

The fan and multisample entry points, the R03 coverage guard and line calculations, and the native scheduling code retain their original bytes. This candidate contains no R04 vertex setup change or shared setup.
