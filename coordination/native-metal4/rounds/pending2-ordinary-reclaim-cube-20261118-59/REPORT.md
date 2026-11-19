# Native cap1 and cap2 with scene reclaim and exact cube ordering

| Configured cap | Average FPS | 1% low FPS | Draws | Longest interval |
| --- | ---: | ---: | ---: | ---: |
| 1 | 65.205 | 35.857 | 2608 | 264.0 ms |
| 2 | 65.420 | 56.367 | 2617 | 23.0 ms |

Average FPS was +0.33% for the cap2 setting. Both used the same signed binary and all recorded samples. Cap2 did not activate. All 2616 adjacent eligible frames hit the historical UI RTT CPU retirement gate. None hit the cube gate. There were zero joined commits and zero observations of two outstanding GPU submissions. The cap1 run retained a 264 ms interval. Neither the 1% low difference nor the average shift can be attributed to active cap2.

Both games ran to normal exit, and retirement followed by a drawable presentation request was logged in both. No capture, profile or repeat ran. Input latency and physical scanout were not measured. A temporary caffeinate -d was needed because the prior display assertion was absent. The owned process 46852 was released.

The preceding independent audit identified the existing scene-ledger and cube gate. The source adds FIFO capacity reclaim and exact private cube dependency ordering, while historical screen RTT and planned WorldInfo reuse retain their CPU success requirements. The next candidate can target the measured RTT gate directly.
