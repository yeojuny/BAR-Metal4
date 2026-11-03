# BAR Metal4 v1.11.0

Apple Silicon Mac; macOS 27 or later. Experimental single-player release based on the tested 204 combination (202+203 on 201). Candidate 205 was stopped and is not included.

## Install

```sh
brew tap yeojuny/bar-metal4
brew install --cask yeojuny/bar-metal4/bar-metal4
open "/Applications/BAR Metal4.app"
```

To upgrade an existing installation:

```sh
brew update
brew upgrade --cask yeojuny/bar-metal4/bar-metal4
```

Homebrew installs the pinned game-content formula automatically. No GitHub account or token is required. The first launch copies the installed content into each user's `~/Library/Application Support/BAR-Metal4/assets`. This uses several GB and is a local copy, not another network download. Each user has separate settings and saves. Allow at least 10 GB of free space.

AA defaults to 0x (`MSAA=0`, `MSAALevel=0`); VSync defaults to off (`VSync=0`). Existing user settings are preserved. To reset graphics defaults, quit the app and back up/rename `~/Library/Application Support/BAR-Metal4/data/springsettings.cfg` before restarting.

## Repeated content-download repair

The old distribution kept the local26 Rapid identity only in the writable index. A later online index refresh could erase that identity, so the next launch no longer recognized an already installed package and retried an unavailable local download. This release bundles the exact local26 package identity in the read-only content root; online updates cannot remove it. The bootstrap also retries a local content copy if its package or pool directory is missing despite an old ready marker. No account, settings or save deletion is needed. The same formula content remains usable.

## Validation and limits

The 204 engine is unchanged from the owner-tested product. Its signed SHA256 is `d74830cabc75ce28028e25df3741843ac3b2b454cde5c6e735a07d9693e0a337`. Six owner game runs completed normally. Three matched 201/204 pairs pooled 154.60/155.48 FPS in one scene; this is not a general speedup or a full pixel-parity claim. Release packaging checks signature and file hashes. A fresh-user lobby is launched twice with the writable Rapid index replaced between launches, checking that the pinned game remains installed. No new gameplay benchmark is part of release verification.

The app is ad-hoc signed and is not Apple notarized. If macOS blocks it, use System Settings > Privacy & Security > Open Anyway after attempting to open the app. Other-device gameplay and public multiplayer remain unverified.

## Source

`engine/` includes the effective frozen 204 source overlays on the prior public baseline; `docs/ENGINE-204-PINS.json` pins their exact bytes. `lobby/` includes available source-map originals from the packaged lobby. `distribution/` contains the shipped launcher, default config, Info.plist and pinned Rapid identity. Existing upstream copyright and licenses apply. See NOTICE.md and prior build documentation. A complete clean build has not been rerun for this publication.
