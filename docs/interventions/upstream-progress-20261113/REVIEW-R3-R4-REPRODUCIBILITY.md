# Review R3/R4: reproducible focused acceptance runners

The three runners no longer depend on a historical local-only Git object or
obsolete direct Session mailbox acceptance wiring.

- Completion mailbox: checks the current `ScenePendingAdmission` Transfer,
  exact-owner Accept, failure Quarantine/Restore, and CPU-settlement callback
  wiring. These are **source assertions**, not compiled SceneBridge execution.
  Separately compiles the production mailbox header and executes the existing
  concurrency fixture under ASan/UBSan: **23,041 checks passed**.
- Map publication: compiles actual publication/registry/history translation
  units. **11 allocation failures restored; 71 successes; retry/reused-normal
  preservation passed**. A deterministic current-source mutant publishes each
  image immediately without pair staging/rollback; the fixture rejects it at
  its exact prior-backing identity assertion. Actual registry sequence-overflow
  preservation also passes and the old Replace-order mutant is rejected.
- Worker publication: compiles the current production worker/gate headers.
  **5 allocation failures and 1,000 exception/recovery pairs passed**. Its
  deterministic mutant removes construction-credit rollback; the next Invoke
  is rejected with `no task credit`, proving the recovery test detects the leak.

The mutants are reconstructed from current source using exact, asserted splice
anchors. Each runner writes `negative-provenance.json` containing the production
source SHA-256 and mutation description. This preserves the relevant regression
semantics without claiming the mutant is a byte-identical historical revision.
Unpublished SHAs 0779432d... and 62485fde... are no longer required.

Commands:

```
python3 engine/tools/metal4-contracts/run_native_frame_completion_mailbox_test.py --output /tmp/bar-mailbox-review
python3 engine/tools/metal4-contracts/run_native_map_publication_transaction_test.py
python3 engine/tools/metal4-contracts/run_native_worker_publication_test.py
```

Scope: host components and explicitly identified source-wiring assertions only.
No full Objective-C++ target, Metal device, P5 game execution, performance claim,
or visual-equivalence proof is implied. GCC 14 warns about deprecated shared_ptr
atomic free functions introduced by the separate Apple compatibility patch;
these warnings did not prevent compilation or execution.
