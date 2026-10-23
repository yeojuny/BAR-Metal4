// SPDX-FileCopyrightText: 2026 The BAR Lobby Authors
//
// SPDX-License-Identifier: MIT

import * as fs from "fs";
import * as os from "os";
import * as path from "path";
import { WRITE_DATA_PATH } from "@main/config/app";
import { logger } from "@main/utils/logger";
import { configService } from "@main/services/config.service";

const log = logger("main/game/springsettings.ts");



export function mergeSpringDefaults(settings: Map<string, string>, defaults: Record<string, string | number>, platform = process.platform): boolean {
    const missingDefaults = platform === "darwin" ? { ...defaults, XResolution: 0, YResolution: 0, XResolutionWindowed: 0, YResolutionWindowed: 0, Fullscreen: 1 } : defaults;
    let changed = false;
    for (const [key, value] of Object.entries(missingDefaults)) {
        // BAR's normal native launch restores its fullscreen default before spawn.
        // Chobby's later in-engine mode selection owns explicit game/menu 0 or1;
        // it must not call this startup defaults merge again after that selection.
        const restoreMacOSFullscreen = platform === "darwin" && key === "Fullscreen" && settings.get(key) !== "1";
        if (!settings.has(key) || restoreMacOSFullscreen) {
            settings.set(key, String(value));
            changed = true;
        }
    }
    return changed;
}

function readSettings(filePath: string): Map<string, string> {
    const settings = new Map<string, string>();
    let fileContent = "";
    try {
        fileContent = fs.readFileSync(filePath, "utf-8");
    } catch (error) {
        // An unreadable existing native configuration must never be replaced
        // with defaults. Missing files can be initialized normally.
        if (process.platform === "darwin" && (error as NodeJS.ErrnoException).code !== "ENOENT") throw error;
        return settings;
    }
    for (const line of fileContent.split(/\r?\n/)) {
        const trimmed = line.trim();
        if (!trimmed || trimmed.startsWith("#")) continue;
        const eqIndex = trimmed.indexOf("=");
        if (eqIndex === -1) continue;
        settings.set(trimmed.slice(0, eqIndex).trim(), trimmed.slice(eqIndex + 1).trim());
    }
    return settings;
}

function writeSettings(settings: Map<string, string>, filePath: string): void {
    const lines: string[] = [];
    for (const [key, value] of settings) {
        lines.push(`${key} = ${value}`);
    }
    fs.writeFileSync(filePath, lines.join(os.EOL) + os.EOL, "utf-8");
}

export function applyDefaultSpringsettings(writeDirectory = WRITE_DATA_PATH): void {
    const SPRINGSETTINGS_PATH = path.join(writeDirectory, "springsettings.cfg");
    try {
        fs.mkdirSync(path.dirname(SPRINGSETTINGS_PATH), { recursive: true });
        const settings = readSettings(SPRINGSETTINGS_PATH);
        // Zero asks the engine for the selected display's logical safe area.
        // Retina scale is never multiplied in; fullscreen startup follows the fixed donor.
        const changed = mergeSpringDefaults(settings, configService.getConfig().springSettings);
        if (changed) {
            writeSettings(settings, SPRINGSETTINGS_PATH);
            log.info("Applied default springsettings");
        }
    } catch (err) {
        log.error(`Failed to apply default springsettings: ${err}`);
        if (process.platform === "darwin") throw new Error(`Could not preserve/apply native engine settings: ${err instanceof Error ? err.message : String(err)}`);
    }
}
