# Exact postprocess receipts and a native two-submission proof

This isolated candidate builds on Round 55 independent final color. The production ordered-input path records the physical root texture, logical version, producer source ID, exact pending event, aspect, slice and mip. Its capture retains the actual texture. This receipt still checks the existing successful CPU retirement boundary; it does not change live queue admission. Unknown versions retain the serialized path.

Under Metal API Validation, the standalone Metal4 fixture committed two separate commands on one queue. Immediately after the second commit, the producer completion event was still zero. The fixture then released its test-only start gate. The consumer waited for the exact producer event and read four expected words at slice zero and level zero. Both event and feedback checks completed before allocator reset. The runtime and native postprocess translation units passed SDK syntax checks.

The isolated test has no CAMetalDrawable, game workload, injected GPU failure or display observation. The test-only start gate makes the pending-depth observation deterministic and is not a performance measurement. The live game remains cap1. No game, FPS comparison, capture or profile ran.

Remaining live admission requirements are complete render-resource access coverage for ordinary frames, safe drain on predecessor failure, oldest-ticket successful CPU publication and FIFO presentation approval, and reclaim before acquisition can exhaust the drawable pool. JINI's CPU mailbox counterexample shows that its FIFO is publication order; that cannot establish submission order. See the pinned questions for independent review.

[GPU log](GPU-FIXTURE.log) · [Candidate patch](PENDING-POSTPROCESS-TEXTURE-RECEIPT.patch) · [Manifest](CANDIDATE-MANIFEST.json) · [Audit questions](AUDIT-QUESTIONS.json)
