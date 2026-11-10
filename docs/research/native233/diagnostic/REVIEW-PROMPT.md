# Prompt for an independent reviewer

Review the BAR Metal4 renderer evidence in this directory. Begin with [README.md](README.md), then use [DATA-MANIFEST.json](DATA-MANIFEST.json), [the 233 source overlay manifest](../OVERLAY-MANIFEST.json), and the source files linked from the README. Work from the branch revision containing this file.

The aim is to choose the next independently testable change for the single-player native Metal 4 renderer. Prioritize graphics correctness and CPU/GPU resource lifetimes. The local fixed-scene throughput and its tail are the first performance checks. Treat 230 FPS as a frame-budget target, without assuming that any individual experiment can close the gap.

1. Recompute the 234 and 238 late-ready rates and inspect the distribution, including long-tail frames. Explain whether the strongest next measurement is in encoding, preparation, or a real GPU pass. Identify evidence for and against each lead.
2. Trace one removable operation through exact file and symbol names. State what the source proves about producer timing, mutation/version, first and last reader, draw order, error paths, and Metal completion. If you propose CUS batching, address binding and baseInstance contracts. If you propose PiP retention work, distinguish the CPU snapshot from its Metal buffer.
3. Treat published source, local benchmark, rejected experiments and instrumented runs as distinct. Do not add times from different windows or assume a diagnostic delta predicts clean FPS or app memory.
4. Propose at most three hypotheses. For each, give the smallest measurement that could falsify it, the source change you would try if the measurement passes, and a graphics or lifetime check that would catch a wrong result.
5. Return a ranked recommendation with cited code and evidence, measurable stop conditions, and an explicit list of missing information. If none of the changes is supported, say which single missing measurement is most valuable.
