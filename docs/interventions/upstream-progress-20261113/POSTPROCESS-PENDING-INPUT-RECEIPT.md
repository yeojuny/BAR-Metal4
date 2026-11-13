# P6 Postprocess encoded predecessor receipt

## Source implementation
- [x] Pending, encoded Postprocess inputs now register an exact predecessor check in their consuming RuntimeLease, rather than returning true from an unregistered boolean eligibility check.
- [x] The check owns the exact immutable logical version and that allocation's completion flag. It never rereads a mutable cache by raw address. The existing ImageFor path separately retains/pins the physical texture and its storage in the lease's GPU lifetime graph.
- [x] Unknown/missing/expired/wrong-owner/unencoded/same-recording-slot paths reject preparation permission. Already-retired inputs keep their existing path.
- [x] Before consumer submission, event/feedback retirement and original source callbacks must make the exact captured completion flag true. Encoded alone is not completion. Independent versions cannot satisfy each other's checks; repeated same-version requests reuse one lease-local check.
- [x] GPU pending remains one unconditionally. This closes one known missing family; it is not permission to restore two-ticket admission.

## Validation
13 production-extracted eligibility/registration scenarios pass with ASan/UBSan: admission rejection matrix, cache eviction/replacement and owning lifetime, incomplete completion failure, completed success, independent producers, repeated registration, and poisoned/recording/pending lease guards. The preceding 15-case submission/retirement regression also passes.

These are host tests with mocked native texture/runtime objects. Actual Metal event/feedback and Postprocess image behavior require native integration testing; no application build/run or performance claim is made.

## Remaining requested architecture
- [ ] Full producer/subresource/access/order and transitive pending-borrow coverage for P6.
- [ ] P5 general source/provider eligibility and dependency-safe younger whole-world rollback beyond the restricted opt-in pilot.
- [ ] Native integration, visual/error parity and equal-work active overlap/latency/performance acceptance.

The known Postprocess receipt omission is fixed at source level. The above open requirements must remain visible rather than being reclassified as completed small optimizations.
