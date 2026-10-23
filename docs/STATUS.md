# Status — October 23, 2026

## Published source

The source is the October 19 export, including engine-owned SDL window integration and native Metal 4 renderer work. It is a useful inspection/contribution baseline, not a representation that the latest local performance candidates are present.

## Subsequent development evidence

The development team reports native application packaging and bounded single-player runs. Nine campaign items have source/CPU/build validation: non-skin geometry validation, immutable page publication, pipeline variants, clear programs, sweep work, depth/stencil keys, height views, shadow geometry validation, and owned-row submission matching. Eight resource-management subsets were also implemented. These results belong to later development snapshots and are not validation of this public source export.

GPU overlap, aggregate runtime hit rates/resource occupancy, physical texture pooling, Blueprint lookup, replay concurrency, Lua maintenance, Rapid lookup and AI authoring remain open campaign areas. Later candidate comparisons continue. A shadow-upload-buffer reuse comparison was rejected for insufficient benefit; this is not proof of an overall performance win.

## Unmet acceptance

- Clean-machine end-to-end build and packaging of this public export.
- Self-contained native Metal 4 app distribution with matching source and licensing closure.
- Full visual parity, multiplayer synchronization compatibility and production acceptance.
- Repeatable same-camera, same-content, same-settings physical FPS comparison against a reference.

CPU tests, diagnostic loop rates, bounded gameplay logs and subjective impressions measure different things. Nested GPU/CPU timings must not be added together as independent frame costs. No percentage performance claim is made here.
