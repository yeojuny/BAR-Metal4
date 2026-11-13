# CUS selected source envelopes and manifest

With the roster and program uniform content now shared, forward and deferred
partitions retain only selected draw envelopes. The helper does not retain the
whole parent frame, so excluded instance/pose/texture owners are not pinned.
A forward pass without a deferred partition reuses the original immutable frame.
It no longer copies all draws just to erase the deferred subset.

Source provenance manifests are sealed after successful source loads and reused
until another successful load or reset. Paths/hashes/resolved provenance remain
unchanged. Reset releases the builder owner, not manifests held by existing frames.

ASan/UBSan production packing/selection test passed, including exact selected
sequence, manifest identity, empty selection, and excluded-owner destruction.
Independent review found no blocking issue. This is source ownership work only:
no Lua VM, Objective-C++ build, Mac execution, GPU parity or performance claim.
Full frame ownership and asynchronous execution remain incomplete.
