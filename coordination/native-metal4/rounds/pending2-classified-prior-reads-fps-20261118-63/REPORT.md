# Classified prior reads and actual depth two in the Seths game

| Cap | Average FPS | 1% low FPS | Draws | Longest interval |
| --- | ---: | ---: | ---: | ---: |
| cap1 | 64.670 | 49.808 | 2587 | 51.0 ms |
| cap2 | 66.168 | 48.128 | 2647 | 50.0 ms |

Average FPS changed by +2.32% and 1% low by -3.37% in this single same-binary pair. Both games included all 40 measurement bins and all recorded frame intervals. Both exited normally and recorded successful drawable presentation requests. The test did not measure input latency, physical scanout, or injected GPU failures.

The cap2 run had 2,646 adjacent eligible frames and 2,646 joined submissions. In 2,645 of those joined submissions the predecessor GPU event was still unsignaled after the second commit. Cap1 recorded zero joined submissions and zero outstanding pairs. Cap2 recorded zero CPU prior-read fallback frames in every family, including unclassified. Cap1 recorded 2,586 serialized frames, with overlapping cube, screen RTT, Bloom and postprocess counts of 2,586 in each. Its unclassified count was zero. The cap2 scene reclaimed the oldest ticket 4,781 times across the entire game, including warmup.

The 13 input hashes per arm and environment matched the preceding Seths benchmark. This run used one configure, one build and one game per cap, with no captures, profiles or retries. The signed candidate and the preceding evidence were preserved. The active benchmark and production default remain at cap1. The temporary display assertion PID 58639 was released after both games.
