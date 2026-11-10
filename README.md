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

## Performance research

[MVK3 vs Native Metal 4 220: bottlenecks, waits, and measurement data](docs/research/mvk3-native220/README.md)
