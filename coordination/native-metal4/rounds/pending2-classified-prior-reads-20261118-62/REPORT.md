# Complete pending-read caller classification and exact producer guards

The previous cap2 game hit a CPU prior-read gate on all 2,555 adjacent eligible frames. Its cube, global world-info and screen RTT counters were zero. The remaining generic call sites could not be distinguished at runtime with that summary.

| Source | Ordinary-path treatment |
| --- | --- |
| Bloom | Same-command reads retain the existing encode-order check. A historical version may use the exact pending event for its pinned private single-mip image. Storage without exact provenance waits for CPU completion. |
| Terrain info | A historical read can queue only when the wrapper and the non-Metal mip producer report the same submitting source, with a private root texture. Copy-on-write ownership, device, version, shape and full-mip checks protect the read. The actual producer stamps the submission after all updates encode. Cached, wrapper-only, Metal and ambiguous storage retain the CPU boundary. |
| Postprocess | The existing exact slice-0, mip-0 receipt can queue. An unresolved or rejected receipt falls back to the named CPU gate. |
| Cube and screen RTT | Their already verified identity gates retain their existing behavior. |
| Planned global WorldInfo and BumpWater | CPU retirement remains required for unproven historical reads. BumpWater also retains its ordinary admission exclusion. |

All seven source families now name their registrations explicitly. The existing single game summary records per-family serialized frame counts and an unclassified count. A frame can count in several families. The terminal success path checks the oldest GPU result and dependent receipts before publishing or requesting FIFO presentation. No completion, dequeue or failure path changed.

The eight affected live-scene translation units passed SDK syntax checks with the previous build's flags. Three sanitizer-backed CPU suites passed, including extracted Bloom and terrain-info receipt handling, named postprocess fallbacks and the existing terrain-info cache. The production benchmark and the preceding binary are preserved. No new engine build, game, GPU fixture, capture or profile ran in this source round.

A future authorized pair can read the same single summary to verify unclassified=0, then assess the per-family fallback counts, joined submissions and genuine outstanding depth two independently of FPS.
