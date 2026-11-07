# BAR Metal4 v1.11.2

- Fixed selecting and launching single-player scenarios from the lobby.
- Added easier display mode and fullscreen render resolution selection, including Auto, 1080p, 1440p and 2160p.
- Aligned the five BAR bot choices and BARbarIAn's difficulty menu with the pinned upstream configuration. The menu shows Testing AI for BARbarIAn. Circuit is excluded from this build.
- New profiles use 2× health bar scale and height, anti-aliasing off, and both standard and Adaptive VSync off.

Requires an Apple Silicon Mac and **macOS 27.2 or later**.

```sh
brew tap yeojuny/bar-metal4
brew trust --tap yeojuny/bar-metal4
brew install --cask yeojuny/bar-metal4/bar-metal4
open "/Applications/BAR Metal4.app"
```

For an existing install, run `brew update` and `brew upgrade --cask yeojuny/bar-metal4/bar-metal4`. Homebrew supplies the pinned base game content. The update adds about 586 KB of changed game data to the per-user content directory. Allow at least 10 GB free for a first installation. The app is ad-hoc signed. If macOS blocks the first launch, attempt to open it, then use **System Settings → Privacy & Security → Open Anyway**.

The release targets experimental single-player. See the source and verification notes for the precise test scope.

## Verification

The exact release ZIP was restored and all 690 entries matched the manifest. Two launches on one Apple Silicon Mac reached frame 300 in A Safe Haven and exited normally. The first run confirmed the five bot choices and the upstream BARbarIAn menu. Health bars showed scale and height 2× and actual display sync was off in both runs. The 1440p selection persisted and the second run reused the installed content update. Testing covered scenario startup on one Mac.

## Downloads and source

The release includes the app ZIP, a source archive, a file manifest and SHA-256 checksums. The source tree includes the pinned engine, lobby source without original media, display adapters, BARbarIAn build inputs, changed game data and packaging scripts. A clean rebuild was not run.
