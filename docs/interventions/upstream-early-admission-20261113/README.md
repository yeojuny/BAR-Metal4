# Early ordinary-frame journal admission

The existing synchronous deferred scene path now transfers UI, postprocess and
cloud journals before renderer invocation, not after GPU submit. CPU lookahead is
not enabled. Capture stays blocked through the call and settlement.

- All owner identities/capacities and callback allocations precede the moves.
- The shared production coordinator moves three journals without intervening
  allocations or throwing operations. Main remains the only journal mutator.
- UI Begin delivery carries an explicit held source-draw ID. There is no latest
  pending fallback when this ID is supplied. Direct readback keeps its existing
  pending path and is excluded from early admission.
- No-Begin renderer failures and hidden-surface fallback restore exact pending
  owners. Original rollback/independent-target handling then runs as before.
- Synchronous completed fallback restores for Game.cpp's original pending
  retirement path; restoration itself neither rolls back nor retires data.
- Actual Begin failures retain held journals and permanently reject new capture
  on those recorders. Reset/Abort do not silently unpoison or roll them back.
- Both renderer exceptions and post-return drain/settlement exceptions pass
  through the same actual-Begin classification. This fixes an exception-scope gap
  found in review. After three ownership transfers, main accepts completion.
- Font diagnostic reporting occurs after admission acceptance, not between moves.

Validation: actual postprocess/cloud + shared production coordinator ASan/UBSan
1,073 checks; actual UI noexcept signatures checked, full UI/SceneBridge/LuaCloud
translation-unit host syntax passed. Mailbox 23,041 checks and owned Begin 3,000
serial-worker lifecycles plus target-notice regressions pass. Independent review
GO. Tests include each validation boundary, no-Begin renderer/post-return errors,
restoration versus younger pending capture, post-Begin quarantine and lifetime.

Limits: actual UI runtime is not linked by the focused fixture. Source-wiring
assertions and host syntax do not prove Objective-C++ compilation, Metal execution,
visual parity or performance. The journal transaction is not whole-world atomic
ownership: shield dependency rollback, all world/water lifetimes, mutable caches,
exclusive render ownership and genuine asynchronous CPU overlap remain gates.
