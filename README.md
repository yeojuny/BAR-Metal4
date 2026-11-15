# BEGIN/PREPARE validated source checkpoint

This branch preserves the tested 2026-11-14 source, both current result campaigns and the earlier capture-ready/BEGIN history.

[Checkpoint, results and next strategy](docs/checkpoints/begin-prepare-20261114/README.md) · [Source restoration](docs/checkpoints/begin-prepare-20261114/reproduction/README.md)

The measurements pin signed engine fd2ba879ee5867c4ebde836689e1e53c2e699d0571b8f5efaa54ce3b0135c4b9. The adopted local performance policy remains the separately recorded 8a65879e/PREPARE baseline. The product installation information below refers to the v1.11.2 release.

# BAR Metal4 v1.11.2

Experimental single-player BAR with native Metal 4 for Apple Silicon, macOS 27.2 or later.
This update fixes scenario selection and adds fullscreen render resolution controls.
New profiles use 2× health bar scale and height, anti-aliasing off, and VSync off.

```sh
brew tap yeojuny/bar-metal4
brew trust --tap yeojuny/bar-metal4
brew install --cask yeojuny/bar-metal4/bar-metal4
open "/Applications/BAR Metal4.app"
```

Already installed? Run `brew update` and `brew upgrade --cask yeojuny/bar-metal4/bar-metal4`.

[Release and downloads](https://github.com/yeojuny/BAR-Metal4/releases/tag/v1.11.2) · [Installation and verification](docs/RELEASE-1.11.2.md) · [Licenses and provenance](NOTICE.md)
