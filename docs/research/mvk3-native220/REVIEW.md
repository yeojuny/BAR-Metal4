# Questions for an independent reviewer

Use [README.md](README.md) and [DATA-MANIFEST.json](DATA-MANIFEST.json) as the index. Review the three experiment groups independently. The goal is to identify the smallest next measurement that can separate CPU-side preparation, submission, dependency waiting, and real GPU work.

1. Reproduce the DrawScreen throughput and the command-buffer interval union using the run conditions and denominators. List the parts that cannot be made comparable across MVK3 and Native 220.
2. Trace the Native prior wait and test the claim that callback delay is a meaningful part of the observed gap. Explain the difference between callback delay, its observed overlap with a wait, and the cost of the queue topology.
3. Use the CPU sample quality, Native stage markers and MVK replay evidence to rank the next two measurements. Account for the missing MVK timestamp sequence and missing matched Native replay profile.
4. If you suggest an optimization, name a falsifying measurement, define what must remain equal across the two arms, and explain how to preserve image versions, draw order, and GPU-completion lifetimes. Distinguish an experiment from a conclusion about the renderer.
