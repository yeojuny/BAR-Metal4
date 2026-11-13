# Captured cube rollback ownership

SceneBridge now supplies its captured environment receipt to shield/world abort.
Abort cancels that exact cube publication, never `CurrentReflectionUpdate()`.
All four abort call sites carry the receipt. A new capture clears the old receipt
before creating a replacement, so failed new capture cannot select stale state.

The CPU environment capture owns an exact rollback guard immediately after the
original cube update returns, until the complete environment frame is installed.
Failures in lighting/specular/terrain/metadata capture withdraw that publication.
CubeMapHandler's reflection-face exception also uses its returned exact update.
Already completed, encoded, or dependency-submitted cube or paired mip work is
not canceled. The renderer's actual-Begin active-source guard remains in place.

Validation: actual source + registry + history under ASan/UBSan, original exact
cancellation cases plus 100 capture failures, release, encoded/dependency/completed
protection, paired mip and newer-publication preservation. Structural checks
ensure no latest-update lookup in abort and capture guard spans throwing capture.
Host C++ syntax checks pass for full SceneBridge, WorldFrameCapture and
CubeMapHandler translation units. No Objective-C++ compile or Metal runtime test.

Still serial: this does not allow younger dependent capture, rewind original sun
or cube-face cursors, or make all shield/map/model/cache state independently
owned. Descendant rollback could restore a withdrawn predecessor if concurrency
were enabled, so the admission/lookahead prohibition remains unchanged.

Review corrections: CubeMapHandler's guard also spans the final bind calls, not
just face capture; its face cursor advances only after those calls succeed.
The old environment receipt is cleared before shadow capture can throw.
ClearStaticResources requires an explicit environment receipt, so both reset and
shutdown preserve exact cancellation rather than silently omitting cube cleanup.

Independent final review: GO. Rebuilt actual-source sanitizer suite passed;
reviewed all exact receipt abort/clear callers and both exception-guard spans.
Final host syntax rerun exited 0 with no diagnostics.
