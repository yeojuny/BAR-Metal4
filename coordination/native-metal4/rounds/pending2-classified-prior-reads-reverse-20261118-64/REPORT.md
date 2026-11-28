# Reverse-order cap2 and cap1 on the same Seths binary

| Run order | Cap | Average FPS | 1% low FPS | Draws | Longest interval |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1 | 2 | 66.170 | 44.850 | 2,647 | 55.0 ms |
| 2 | 1 | 64.722 | 54.737 | 2,589 | 28.0 ms |

The reverse pair shows +2.24% average FPS and -18.06% 1% low for cap2. The forward pair showed +2.32% average FPS and -3.37% 1% low. Average FPS was higher and 1% low lower in both run orders. The magnitude of the slowest-1% difference varied between these two pairs. All measured frames were retained.

The reverse cap2 run observed 2,644 genuine pairs of outstanding GPU submissions after the second commit, out of 2,646 joined submissions. That compares with 2,645 of 2,646 in the forward cap2 game. All seven cap2 fallback counters and the unclassified counter were zero. Reverse cap1 retained its serial reads and had zero joined submissions. Its 2,588 serialized frames each counted in overlapping cube, screen RTT, Bloom, and postprocess families.

Both games exited normally and emitted a drawable presentation request witness. The first and final preflight snapshots for each arm matched the forward comparison on lock, sleep, display state, AC power, and thermal status. Both rounds observed a locked, asleep display. Physical scanout, input latency, and injected GPU failure were not tested.

The same executable path and SHA256 were used with 13 matching scene input hashes per arm. There were no source edits, configure steps, builds, captures, profiles or retries. The temporary display assertion PID 59301 was released. No default or deployment change was made.
