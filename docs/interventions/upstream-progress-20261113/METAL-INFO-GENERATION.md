# Shared info service generation audit (P3/P4 prerequisite)

| Item | Status |
|---|---|
| WorldInfo prepared ownership | Already pins shared Impl; Clear replaces Impl, so old completion cannot republish into the new service generation |
| MetalInfo resources | Split target/source cache into a retained resource-generation owner |
| MetalInfo Clear | Replaces only resource generation; library/pipelines/validation cache preserved |
| Prepared MetalInfo completion | Pins exact resource generation, preserving original target commit and extraction pruning |
| GPU producer and completion body | Byte-identical to previous checkpoint |
| Broader P5 concurrency | Not enabled; lease/drain/gate/default controls unchanged |

The old MetalInfo Clear emptied target/sources on the same Impl captured weakly by
prepared commit. Current serial RuntimeLease and reset/retirement contracts protect
that path; this is NOT a demonstrated present runtime race. The new generation
boundary is a prerequisite against stale completion re-populating a newer cleared
service if control/lifetime scheduling later changes. Prepared completion updates
only its original generation. No cycle exists: resources own target/extractions,
not prepared jobs/closures. Old generations may live until their prepared owner
is released; that is intentional bounded lifetime retention.

Task checklist:
- [x] Audit WorldInfo and MetalInfo Prepare/Encode/DidComplete/Clear/maintenance.
- [x] Implement exact MetalInfo resource-generation snapshot.
- [x] Preserve extraction creation, temporal blend, mip generation and pruning.
- [x] Independent source review and actual production resource/commit extraction tests.
- [ ] Native Objective-C++/Metal execution, integrated parity and performance.
- [ ] Remaining shared services/control and same-resource lazy-cache audit.

`python3 engine/tools/metal4-contracts/run_metal_info_generation_test.py` extracts
the production Resources fields, Clear assignment and commit lambda and compiles
them with C++23/-Wall/-Wextra/-Werror/ASan/UBSan. 1,000 cases cover same-generation
pruning, old completion after Clear, newer target preservation, exact-generation
lifetime, empty source roster and pending-without-commit behavior. Static hashes
verify original GPU EncodeDependencies/DidComplete and Initialize unchanged.
Allocation failure of Clear is safe by RHS-before-assignment/shared_ptr semantics;
no runtime allocation fault injection is claimed. LSan disabled for ptrace limits.
Begin-ready pilot regression also passed. These are portable source tests, not a
Metal build. Existing metal capture/controller runner was not rerun (requires
separate original shader fixture/toolchain); capture source was unchanged.
