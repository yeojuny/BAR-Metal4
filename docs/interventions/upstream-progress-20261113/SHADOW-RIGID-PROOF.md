# Exact capture rigid-layout success reuse

## Checklist
- [x] Trace model creation, immutable publication and classification predicate.
- [x] Preserve per-draw guards, local successful set and unsupported fallback.
- [x] Extract actual shadow and capture predicates for sanitizer reference tests.
- [ ] Native integration and performance measurement; not claimed by this step.

EngineShadow's Rigid used to repeat every drawable vertex's bone-layout check for
each model on every Prepare. WorldFrameCapture::Model already computes the exact
positive condition while copying geometry under CModelsLock. It compares the
copied first bone ID with the current piece index and weights with 255/0/0/0.
HasGeometryData is indices.size() >= 3, exactly matching the shadow fallback's
indices.size() < 3 exclusion. Both inspect all drawable-piece vertices, including
unreferenced vertices; emitter-only pieces keep their unassigned bones.

The copy obtains the same indices vector, rejects out-of-range indices, and seals
layout before insertion into a shared_ptr<const WorldModel> cache. Unit, Feature
and Static engine object capture all use this Model provider. The audited engine
source has one make_shared<WorldModel> construction site and one non-default
layout assignment. The positive enum is already consumed by the engine-model
validation path. This change reuses that existing trusted publication contract.

Rigid still performs its per-draw object/model/modelType/pose-size Require before
checking the per-Prepare successful set. Only RigidPerPiece skips the vertex
loop, and still performs the same set insertion. Thus its allocation failure
boundary remains at the successful insertion, and previously validated models
still cannot bypass later draws' format/pose checks. Unclassified/Unsupported
models execute the unchanged fallback, including its original detailed error.
No shader/profile checks, geometry uploads, producer order or barriers change.

This is NOT a tamper-proof certificate. WorldModel has public fields; a manually
fabricated RigidPerPiece value on bad geometry violates the capture contract and
would bypass the scan. Tests explicitly demonstrate this caveat rather than
pretend the enum cryptographically authenticates geometry. A new model creation,
mutation or classification path must maintain the same exact invariant. The
runner gates known construction/writer sites and original capture ordering, but
textual provenance checks are not a complete C++ alias analysis.

Validation: run_shadow_rigid_proof_test.py extracts actual Rigid and the actual
capture predicate. It compares the fastpath against the unchanged prior Rigid
body (fixed SHA checked) for 4000 randomized models, and additionally covers
empty/emitter pieces, unreferenced malformed vertices, format/pose guards after
cached success, null objects/models, Unclassified/Unsupported fallback diagnostic
identity, raw-model set results, allocation failure at insertion and forged enum
behavior. A test-only counter inserted into each extracted vertex loop verifies
zero fastpath visits versus four reference visits for a valid four-vertex model;
this is work-elimination evidence, not elapsed-time measurement. The capture source gates verify HasGeometryData and GetIndicesVec use
the same vector, lock/copy/index-check/classify/publication ordering and creator
locations. Host fixtures use representative structs, not a Metal runtime.

No full application build or Metal execution. This removes redundant CPU work;
it does not by itself widen P5 overlap or establish a measured performance gain.
