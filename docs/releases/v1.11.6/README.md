# BAR Metal4 v1.11.6

Experimental single-player native Metal 4 for Apple Silicon. This release accumulates the selected renderer improvements through the bounded WorldInfo texture-storage pool.

## What's new

- Native CAP2 renderer: selected terrain forward, dense Grass, model fragment, SSAO, BumpWater, GroundDecal, deferred light and bloom variants.
- Ordered Water4 preparation and tick transitions; SensorLOS fan specialization; model shadow buckets, direct model encoding, adaptive terrain shadow cache and resource residency updates.
- Asynchronous DeferredLight program preparation removes the previously observed cold shader compilation hitch at Seths tick 878.
- Weapon range division (233), full-clear render-target copy elision (244), CPU read-packet mode 2 (220), and WorldInfo storage reuse (245).
- WorldInfo remains versioned; the pool does not remove producer/consumer ordering or history copies. Storage is bounded and busy entries are never overwritten.

## Performance evidence

Simmers WorldInfo ABBA, same signed engine, CAP2, AC Automatic, 120-second minimum rest; averaged over the fixed simulation window. Both arms enable the allocation witness.

| WorldInfo pool | Average FPS | 1% low |
| --- | ---: | ---: |
| Off | 235.84 | 153.04 |
| On | 236.15 | 156.33 |

At equal 1,612 texture requests, new texture allocations fell from 1,612 to 3, or 16.95 GiB to 32.30 MiB of cumulative allocation bytes. This measures allocation churn, not peak RAM, disk I/O or CPU time. The production profile disables the allocation witness.

The preceding cumulative engine (with WorldInfo off) was also checked once per map:

| Map | Average FPS | 1% low |
| --- | ---: | ---: |
| Seths | 113.47 | 72.56 |
| Supreme | 149.96 | 93.60 |
| Jade | 63.97 | 48.42 |
| Shallow | 175.05 | 116.78 |
| Mediterraneum | 144.23 | 101.75 |

These are historical single-machine measurements, not a new paired comparison with the previous release or MVK3. See [verification evidence](CHECKS.json).

## Known limitations

This is an experimental single-player release, not an official BAR build. Selected high-speed shader variants and dense Grass are not guaranteed bit-exact to the previous renderer. Full cumulative final-frame pixel parity and fresh application-first acceptance were not completed. Legion faction selection/gameplay has not been verified for this release; the user deferred the test. If you encounter an error, preserve the engine infolog and lobby log. Multiplayer is not supported by this experimental release.

The engine archive contains the exact selected source overlay lineage and verified linked object hashes. It is not a new clean-machine rebuild. Inherited upstream contributions remain documented in NOTICE.md and UPSTREAM-CONTRIBUTIONS.json.

## Update

```sh
brew update
brew upgrade --cask yeojuny/bar-metal4/bar-metal4
```

Requires Apple Silicon and macOS 27.2 or later. The app is ad-hoc signed.
