# Exact retained result reporting

Presentation reporting now takes an explicit Session, frame, result and screen
view. The previous report body is preserved verbatim apart from replacing the
implicit Session pointer with the argument. Deferred CPU settlement invokes it
from the retained submission; the joined tail skips duplicate reporting.
Synchronous/no-Begin paths keep their prior handling. This separates reporting
from resets of the current pending source graph before queued returns are added.

Exact body comparison, SceneBridge host syntax, extracted scene sanitizer100 and
independent review pass. No native execution or async activation.

Budget audit: actual command Begin can coexist with a preceding GPU submission.
First bounded2 pilot must drain prior GPU/main receipts before dispatch and must
not use previousRetired overlap preparation. Begin-ready alone is NOT credit
release evidence. Full preparation overlap with three source frames is outside
this first pilot.
