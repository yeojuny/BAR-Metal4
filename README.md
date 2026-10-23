# BAR — experimental native Metal 4 research

An independent, AI-assisted Apple Silicon port experiment for Beyond All Reason / RecoilEngine. This is not an official BAR release and is not ready for production or multiplayer compatibility claims.

This public repository starts with a **2026-10-23 pre-performance baseline**: cumulative engine fixes through selection-state fix 25, plus paired naval AI/content changes from package 26. The subsequent native-performance campaign is excluded. See [the exact boundary](docs/BASELINE.json). No private backup history or conversation records are published.

## Get the source with Homebrew

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

This installs the contributor source kit and prints its location. **It does not install a playable BAR.app.** A native Metal 4 app cask is pending a portable, redistributable app with matching complete source. The existing `bar-apple-silicon` cask is a different port and is not this renderer.

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
