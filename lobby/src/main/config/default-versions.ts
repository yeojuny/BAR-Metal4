// SPDX-FileCopyrightText: 2025 The BAR Lobby Authors
//
// SPDX-License-Identifier: MIT

/**
 * These hardcoded versions are only needed for singleplayer/offline games. Online battles simply fetch and use whichever version that autohosts are running.
 *
 * We need to define and change these manually because the latest engine and game version may be incompatible with each other, or could be untested and containing bugs.
 *
 * In the future these values should probably be set and fetched from the master server, so we don't need to deploy a new lobby release every time.
 *
 * Edit 5/20/2026: Bumped default engine version because it was not compatible with latest game versions, as was foretold. - Hectate
 */

export const LATEST = "LATEST";
export const LATEST_GAME_VERSION = "byar:test";
// Shared exports are also imported by the sandboxed renderer. Main supplies
// the platform-specific default through the existing info IPC.
export const DEFAULT_ENGINE_VERSION = "2026.07.04";
export function getDefaultEngineVersion(platform: string): string {
    return platform === "darwin" ? "2026.09.01-24-g01efdf4 master" : DEFAULT_ENGINE_VERSION;
}
// Reviewed public multiplayer fallback, separate from the polishing engine.
// BYAR-Chobby dist_cfg/config.json at bdfae3ca698d8c081a3f0bec75bce1f0a452d67e
// selects this engine for both Windows and Linux Alpha (observed 2026-09-24).
// Advancing this pin requires a matching native engine, not a version alias.
export const CHOBBY_MULTIPLAYER_ENGINE_FALLBACK_VERSION = "2026.07.04";
