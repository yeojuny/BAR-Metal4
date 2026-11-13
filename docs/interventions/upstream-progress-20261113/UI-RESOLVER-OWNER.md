# P4 prerequisite: exact request-owned UI binding resolver

The two UI descriptor consumers (OriginalModelImage and the StartBox image provider)
now resolve through their actual RuntimeLease rather than Impl::uiBindingResolver.
The ambient swap/restore ResolverScope is removed. RuntimeLease owns a typed
NativeUIBindingResolverOwner, bound once by value/move, including an empty capture;
rebinding is rejected, and its only accessor is const. A failing resolver propagates
its error without selecting a live fallback or altering another request's resolver.

DrawDispatch still freezes CaptureResolver on main before worker handoff, and its
actual externalImages provider remains captured by value. DrawPrepared carries that
owner as an argument across RuntimeLease construction/previous-request retirement,
then binds it before source preparation. The ContinueScreen early-return path now
passes the resolver explicitly by value/move and binds its own lease before screen
and postprocess preparation. It cannot inherit an unrelated renderer's latest slot.
Returning before lease creation merely releases the local capture as before.

Empty captures and standalone leases preserve the original behavior: a live UI
lookup is permitted only on main; a worker without a captured resolver throws the
same error. Captured resolvers take precedence even on main. Descriptor/image owners
returned by a resolver retain their original lifetime contracts; the resolver owner
is not a substitute for GPU resource retention or completion evidence.

Validation: actual owner header and extracted RuntimeLease accessors compile as
C++23 -Wall/-Wextra/-Werror with ASan/UBSan. 1,000 cases cover independent A/B owners,
continuation handoff, empty/unbound main fallback, worker rejection, rebinding,
resolver exceptions and captured-object lifetime. Wiring assertions check accessor
uniqueness/class placement, both consumer sites, both lease bindings and continuation
before its early return/preparation. The existing actual UI allocation/descriptor/
CaptureResolver fixture passed its 1,000 cases with stronger exact-lease assertions. Provider
submission identity regression updates only its accessor extraction boundary and
hash for the one StartBox helper argument change and passed; other provider logic is unchanged.

The existing gates, request lock, source identity, image caches, queue depth and
worker eligibility are unchanged. Native Objective-C++/Metal integration and full
app/visual/performance verification remain outstanding. This completes one owner
boundary, not general Prepare overlap or full P4/P5 acceptance. No app build/run.
