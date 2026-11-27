# BAR Metal4 v1.11.61

Experimental single-player BAR with native Metal 4 for Apple Silicon, macOS 27.2 or later.
This maintenance release fixes 2× MSAA lines, large PiP weapon-range viewports, ground-decals across resolution changes, and shutdown error handling, and includes selected CPU-side optimizations.
Normal launch uses **PREPARE with CAP2**; diagnostics and Metal Validation are **off** by default.
New profiles retain 2× health bar scale and height, anti-aliasing off, and ordinary/adaptive VSync off.

```sh
brew tap yeojuny/bar-metal4
brew trust --tap yeojuny/bar-metal4
brew install --cask yeojuny/bar-metal4/bar-metal4
open "/Applications/BAR Metal4.app"
```

Already installed? Run `brew update` and `brew upgrade --cask yeojuny/bar-metal4/bar-metal4`.

[Release and downloads](https://github.com/yeojuny/BAR-Metal4/releases/tag/v1.11.61) · [Installation and checks](https://github.com/yeojuny/BAR-Metal4/blob/v1.11.61/docs/releases/v1.11.61/README.md) · [Licenses and provenance](NOTICE.md)

## Performance research

[MVK3 versus Native 220](docs/research/mvk3-native220/README.md) · [Native 233–242 diagnostic packet](https://github.com/yeojuny/BAR-Metal4/blob/80d173cdd354272bf4a7d407cbd6b9662fbf6a79/docs/research/native233/diagnostic/README.md) · [BEGIN/PREPARE source checkpoint and historical comparisons](https://github.com/yeojuny/BAR-Metal4/blob/e9acbc66115c5fecd872f3385db248371362b97b/docs/checkpoints/begin-prepare-20261114/README.md)
