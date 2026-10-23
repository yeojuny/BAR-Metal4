# Status — October 23, 2026, before performance optimization

## Published baseline

This release supersedes the initial October 19 source publication. The engine is reconstructed from the verified cumulative base manifest (9,980 files before publication exclusions) plus seven pinned source overrides confirmed by HQ before the native-performance campaign. It includes the later log-driven/correctness fixes:

- Native nano-spray graphical default.
- Missile/model follow-up and draw-counter work.
- Bounded model tracing used in diagnosis.
- Restoration of the original selection Draw state epilogue.
- Paired naval AI26 native MilitaryManager change and five game configuration overlays.

The renderer base also contains the earlier raster/point/CUS integration. `BASELINE.json` records input manifest hashes and the exact boundary. No source from the subsequent native-performance campaign is included: no CUS/pipeline/resource-pool/residency/terrain-view optimization patches are imported.

## Scope and remaining gaps

The source baseline corresponds to the pre-performance engine binary reference recorded in `BASELINE.json`; no new build was performed to claim byte-identical binary reproduction. Lobby source remains the prior October 19 export, not a recovered complete current app source tree. The five naval game overlays require the existing local21 content baseline; they are not a complete game package.

The original package26 receipt records successful coupled packaging and signature checks, but manual coupled AI/gameplay acceptance, full visual parity and production acceptance were still pending. Selection-state restoration has bounded runtime evidence; that does not establish full rendering parity.

Still needed: clean-machine end-to-end build, full runtime/content/source closure, redistributable portable app, matched physical-FPS measurements, visual acceptance and multiplayer synchronization validation. The Homebrew formula installs this contributor source kit only. A playable app cask remains pending.
