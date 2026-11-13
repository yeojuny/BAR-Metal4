# Legacy refraction failure ownership

The old DrawWorld exit cleanup could cancel the exact copied-color publication
while its GPU copy was encoded but unfinished. It also ran on failed actual Begin
before EncodeCopy, bypassing the scene abort quarantine.

The copied-color backing now receives a durable atomic Begin mark immediately
after successful runtime Begin. Its owner is retained and checked before Begin,
so marking cannot allocate or throw. The main exit also checks renderer source
ownership. Resource encoding/completion remains separate: prepared resources are
cancellable before Begin; begun or encoded-but-uncompleted owners retain both
pending and previous receipts. Completed cleanup releases those receipts without
rolling back pixels. Copy and Destroy refuse to overwrite/delete a blocked owner.
Unknown fulfilled resource state conservatively refuses cancellation.

Actual backend publication/registry sanitizer tests cover 100 unsubmitted aborts,
100 encoded pending/replacement/destruction rejection cycles, durable Begin before
resource fulfillment/encoding, and completed cleanup followed by a new copy.
The exact actual-Begin mark and main exit predicate are structurally checked;
SceneBridge full C++ syntax is checked separately. No Objective-C++ build or Metal
runtime behavior is claimed. Existing serialized source capture is retained;
this does not add queueing, GPU buffer renaming or concurrent younger capture.
