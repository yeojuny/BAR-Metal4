# BAR Metal4 v1.11.1 — game startup repair

Fixes the v1.11.0 `game:launchBattle` early exit (code 21) caused by a duplicate local26 SDP in the read-only bundle. Recoil selected that archive instead of the per-user copy, then tried to find its adjacent content pool inside the bundle, where no pool was installed. The log reported `Dependent archive ... local26 ... not found`.

The duplicate bundled SDP is removed. The immutable bundled Rapid identity index remains, so online index refreshes cannot erase the installed game's identity. The engine now reads both the SDP and its pool from the same per-user content root. No engine code, graphics or game-content changes; same verified204 engine and AA0x/VSync0 defaults. Settings and saves are preserved; no multi-GB content re-download is needed to apply this app update.

## Install / upgrade

```sh
brew tap yeojuny/bar-metal4
brew trust --tap yeojuny/bar-metal4
brew update
brew install --cask yeojuny/bar-metal4/bar-metal4
```

If already installed, use:

```sh
brew trust --tap yeojuny/bar-metal4
brew update
brew upgrade --cask yeojuny/bar-metal4/bar-metal4
open "/Applications/BAR Metal4.app"
```

`brew trust` records the explicit trust required by Homebrew for this third-party tap. It does not replace macOS first-launch approval. Apple Silicon and macOS27+ are required. The app is ad-hoc signed, not notarized.

## Verification

Release verification uses isolated per-user state and the actual packaged lobby `game:launchBattle` path, waiting for the engine's first presented simulation frame. The app is restarted with the mutable Rapid index replaced between launches. This goes beyond the earlier lobby-only v1.11.0 check. All three isolated launches reached the first presented simulation frame: fresh launch, second launch after online-index replacement, and upgrade recovery seeded with the failed installation's ArchiveCache22.lua. Full pixel parity, broad gameplay and other-device execution are not claimed. Corresponding engine source is unchanged from v1.11.0; the complete source and packaging correction are available at the v1.11.1 Git tag.
