# SaveImage receipt snapshot repair

Native compilation identified a stale `ScreenRenderTargetPass::actualSubmission`
reference in independent readback. Execution descriptors no longer own mutable
receipts; simply removing the check would permit replay of already-submitted work.

CaptureReadbackPrefix now retains an immutable copy of the capture journal receipts
alongside its execution descriptors. A newly closed active target adds a zero
receipt before modifying the live journal. The renderer resolves the exact color
and depth identities and rejects missing receipts or nonzero submission identity.
The current SaveImage flow remains immediate/synchronous; this does not authorize
reuse of an old prefix after later source activity.

Actual prefix/journal ASan+UBSan test covers before/after submission, journal
record/clear isolation, wrong depth, missing identity, continuation and 1000
copies. Full Recorder and SceneBridge C++ syntax checks accompany the source fix.
Objective-C++ compilation/runtime remains deferred with the user's build pause.
