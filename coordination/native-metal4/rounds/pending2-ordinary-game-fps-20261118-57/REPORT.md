# Ordinary game cap1 and cap2

| Configured depth | Average FPS | 1% low FPS | Draws | Longest interval |
| --- | ---: | ---: | ---: | ---: |
| cap1 | 64.665 | 52.738 | 2587 | 27.0 ms |
| cap2 | 64.602 | 51.793 | 2584 | 45.0 ms |

The configured cap2 run changed average FPS by -0.10% and 1% low by -1.79%. Both completed normally and recorded a successful retirement followed by a drawable presentation request. No ordinary-game sample observed two GPU submissions still outstanding after the second commit. This pair does not measure the effect of achieved depth two. Physical scanout, device-error behavior and input latency were not measured.

Both arms used the same isolated binary, scene, settings, FIFO retirement-ordered presentation and all forty bins. Neither discarded frames. The two accepted Bloom flags and EncodeRange were on, while the GroundFlash experiment stayed off. A duplicate claim in the launcher was corrected before a game process started. Exactly one game ran in each arm, with zero extra captures, profiles or game retries.

Astra's exact-read audit supplied the source identity, allocation retry, private storage and partial-copy counterexamples. A separate cloud patch was not applied. The production working-benchmark binary was unchanged.
