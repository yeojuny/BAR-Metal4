# Experimental native Metal4 app

Requires Apple Silicon and macOS 27 or newer; the requesting user's other Mac runs 27.2. Install using the cask command in the README. Existing source-only formula remains for contributors.

The cask installs `BAR Metal4.app`, a relocatable launcher containing the unchanged pre-performance BAR lobby and engine. The separate `bar-metal4-content` formula supplies the exact local26 game SDP and required pool objects plus the local07 test map. First launch copies content into the app's own writable state. It does not ship a developer account, credentials, saved settings, logs, replays, shader cache or the subsequent performance campaign.

The native engine SHA256 remains `4024cdbde86ec97cdb39341718b41187c1ac125ab6fb2c358144e09a9b11bb92`. Original lobby app.asar SHA256 remains `e72080bef183cf12eacf3c8d9c2b004b97ee21c74e8adde9efb5b7396132d18a`. Added launcher/config/Info.plist sources are in `distribution/`. Original license notices are retained, with additional native-component license texts in the app. Underlying engine/game/media retain their original terms; this packaging does not relicense them.

Validation: deep strict code-signature check; no external symlinks; 38 Mach-O slices inspected without `/opt`, `/Users` or `/usr/local` absolute library dependencies; engine and 92 adapter hashes verified. In a fresh manager-owned state directory, the staged app acquired its instance lock, initialized native content, discovered the packaged map and local26 game, created the lobby window, then exited through SIGTERM. No match was started during packaging verification. This is local startup evidence, not clean-machine gameplay/full parity acceptance. Official-multiplayer absence is expected; Chobby is unsupported by this experimental runtime.

The first launch requires roughly 2 GB of additional writable space for content preparation. Homebrew content, copied content and app together need roughly 4.5 GB. Updates use the Homebrew cask; in-app updater metadata is absent. Uninstalling the cask does not erase user data.

The package is ad-hoc signed and not Developer ID notarized. macOS may require explicit approval in Privacy & Security for the first opening; no system-wide security disabling is required. A different Mac's Gatekeeper approval and actual gameplay are still to be confirmed by its user.
