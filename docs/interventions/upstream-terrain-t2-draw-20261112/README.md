# T2 draw snapshot definition sharing

Baseline `765e59fc833e1b5dcbfe571bdd90ab48e7226ec1`, isolated upstream branch. This is a production storage change, not another standalone model and not a declaration of full T2/T3 completion.

## Implemented

`NativeTerrainProgramDefinition` moves to the backend source-data header so source and draw consumers share its immutable identity and existing consumer owner. `NativeTerrainProgramSnapshot` now retains that definition plus the immutable uniform snapshot. Capture no longer constructs a fresh value copy of identity strings and compile defines for every draw.

`Identity()` and `Consumer()` borrow const references from the retained definition and reject a missing definition. Callers must retain the snapshot while using those references. Source SubmitSquare and Metal DrawUniform/Prepare use the new accessors. SensorPreview uses a different program type and is unchanged. Historical overlay records are not patched.

All source liveness, exact-publication, state, consumer and context checks preceding Capture remain intact. Submit validation remains. Metal Prepare retains the original native220 conditional validation reuse and all actual owner/factory, mesh, image, target, pipeline and sample checks. This patch does not turn a publicly constructible definition into a certified readiness token or use one to bypass validation. Shader compiler source, shader math, draw order, queue depth and eager uniform writes are unchanged.

## What this proves / does not prove

The source harness executes the production Capture path and declarations with external shader/compiler/validation services stubbed. It checks sharing, old snapshots, reload/import and deleted-writer behavior. Any zero-allocation observation there applies only to the harness's stubbed validation: actual validation can still allocate. There is no measured FPS/CPU benefit claim.

The internal snapshot layout/aggregate changes, so all source and renderer consumers must be rebuilt together. No binary compatibility with old objects is promised. The full Objective-C++/Metal engine build, actual Lua callbacks, rendering parity and performance require the user-selected Mac and remain unverified. No new environment was accessed.

Full T2 validation-certificate migration and T3 lazy storage remain gated. This removes an existing per-draw definition value copy without weakening validation ahead of those gates.

Independent review found no blocking issue in this bounded storage change and reran 93 source checks plus 11 extracted readiness checks. The actual (not extracted) backend header also passed `printf '#include "Rendering/Backend/NativeWaterGroundSourceData.h"\n' | g++ -std=c++20 -fsyntax-only -x c++ -Iengine/rts -`. The whole capture translation-unit attempt encountered existing `std::from_range` support errors in NativeTerrainPositionCopy.inc on the cloud compiler; it is not recorded as a successful consumer build.
