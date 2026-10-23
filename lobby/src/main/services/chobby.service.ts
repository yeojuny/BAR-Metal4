import path from "node:path";
import { acquireChecksumHold } from "@main/utils/checksums";
import { requireContentWritesOpen } from "@main/content/content-write-coordinator";
import { assertNativeSyncVersion } from "@main/game/native-version";
// SPDX-FileCopyrightText: 2026 The BAR Lobby Authors
//
// SPDX-License-Identifier: MIT

import { ipcMain, type BarIpcWebContents, type IpcResult } from "@main/typed-ipc";
import { settingsService } from "./settings.service";
import { WRITE_DATA_PATH, getAssetsPath } from "@main/config/app";
import { getPackagedNativeRuntime } from "@main/game/packaged-runtime";
import { luaMenuLaunchPlan } from "@main/game/native-runtime";
import { getNativeLaunchDisplayEnv } from "@main/game/native-launch-display";
import { prepareChobbyDisplaySettings } from "@main/game/chobby-display-settings";
import { LuaMenuProcess } from "@main/game/luamenu-process";
import { engineProcessSlot } from "@main/game/engine-process-slot";
import { applyDefaultSpringsettings } from "@main/game/springsettings";
import { logger } from "@main/utils/logger";

const log = logger("chobby-engine");
export const chobbyProcess = new LuaMenuProcess(undefined, 30000, (text) => log.info(text));

async function launch(): Promise<IpcResult> {
    let release: (() => void) | undefined;
    try {
        if (!settingsService.getSettings().devMode) throw new Error("Enable Dev mode before opening Chobby.");
        requireContentWritesOpen();
        const releaseSlot = engineProcessSlot.acquire("chobby");
        let releaseChecksums: (() => void) | undefined;
        release = () => { releaseChecksums?.(); releaseSlot(); };
        const runtime = getPackagedNativeRuntime("official-multiplayer");
        const writeDirectory = path.join(WRITE_DATA_PATH, "engine-lanes", "official-multiplayer", runtime.manifest.engine.sourceCommit.slice(0, 12));
        const plan = luaMenuLaunchPlan(runtime, writeDirectory, getAssetsPath());
        releaseChecksums = await acquireChecksumHold();
        requireContentWritesOpen();
        await assertNativeSyncVersion(runtime);
        applyDefaultSpringsettings(writeDirectory);
        await prepareChobbyDisplaySettings(writeDirectory, settingsService.getSettings().fullscreen);
        const launched = chobbyProcess.launch({ ...plan, env: { ...plan.env, ...getNativeLaunchDisplayEnv() } }, release);
        release = undefined; // The process now owns the slot until confirmed close.
        await launched;
        return { status: "success", data: undefined };
    } catch (error) {
        release?.();
        if (chobbyProcess.state.phase !== "starting" && chobbyProcess.state.phase !== "running") chobbyProcess.failed(error);
        return { status: "failed", reason: "chobby-unavailable", details: error instanceof Error ? error.message : String(error) };
    }
}

export const chobbyService = {
    launch,
    registerIpcHandlers(webContents: BarIpcWebContents) {
        ipcMain.handle("chobby:state", () => chobbyProcess.state);
        ipcMain.handle("chobby:launch", launch);
        chobbyProcess.onState.add((state) => {
            if (!webContents.isDestroyed()) webContents.send("chobby:stateChanged", state);
        });
    },
};
