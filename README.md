# BAR — experimental native Metal 4 research

An independent, AI-assisted Apple Silicon port experiment for Beyond All Reason / RecoilEngine. This is not an official BAR release and is not ready for production or multiplayer compatibility claims.

This public repository starts with a **2026-10-23 pre-performance baseline**: cumulative engine fixes through selection-state fix 25, plus paired naval AI/content changes from package 26. The subsequent native-performance campaign is excluded. See [the exact boundary](docs/BASELINE.json). No private backup history or conversation records are published.

## Install the app (Apple Silicon, macOS 27+)

```sh
brew tap yeojuny/bar-metal4
brew trust --formula yeojuny/bar-metal4/bar-metal4-content
brew install --cask yeojuny/bar-metal4/bar-metal4
open "/Applications/BAR Metal4.app"
```

Homebrew installs the app and its separate game-data dependency. The first launch copies about 2 GB of game data into `~/Library/Application Support/BAR-Metal4`; allow time for that initial preparation. The app uses its own state directory. It is the pre-performance cumulative25 / paired AI26 baseline, with experimental offline play enabled. Public multiplayer and full visual parity are not accepted.

This experimental build is ad-hoc signed, not Apple notarized. If macOS blocks the first launch, use System Settings → Privacy & Security → Open Anyway for this app after attempting to open it. See [Apple's instructions](https://support.apple.com/en-ie/guide/mac-help/mh40617/27/mac/27). Homebrew tap trust and macOS app approval are separate checks.

[App release and downloads](https://github.com/yeojuny/BAR-Metal4/releases/tag/app-2026.10.23.1) · [Install details and validation](docs/APP-INSTALL.md)

## Contributor source package

```sh
brew install yeojuny/bar-metal4/bar-metal4-source
bar-metal4-source
```

Homebrew requires trust for third-party packages. Installing by the full name above trusts this formula only. If you previously tapped the repository and received an untrusted-formula error, use:

```sh
brew trust --formula yeojuny/bar-metal4/bar-metal4-source
brew install yeojuny/bar-metal4/bar-metal4-source
```

See [Homebrew Tap Trust](https://github.com/Homebrew/brew/blob/main/docs/Tap-Trust.md).

This installs the contributor source kit and prints its location. **It does not install a playable BAR.app.** The native app is installed with the cask above. The existing `bar-apple-silicon` cask is a different port and is not this renderer.

Or clone:

```sh
git clone https://github.com/yeojuny/BAR-Metal4.git
```

## Repository

- `engine/`: Recoil/Spring engine snapshot, including native Metal 4 renderer and scene integration.
- `lobby/`: lobby source and dependency lockfile; media assets are excluded pending redistribution review.
- [Progress and limitations](docs/STATUS.md)
- [Reproduction and build starting points](docs/REPRODUCING.md)
- [Help wanted](CONTRIBUTING.md)
- [Licenses and provenance](NOTICE.md)
- `SOURCE-MANIFEST.json`: SHA-256 of each published source file.

Source publication does not mean clean-machine build, visual parity, measured speedup, or production acceptance. Please report reproducible problems in this repository; do not send unsolicited AI-generated changes to upstream projects.
