// SPDX-FileCopyrightText: 2025 The BAR Lobby Authors
//
// SPDX-License-Identifier: MIT

import { Downloader } from "@main/content/abstract-content";
import { getAutoUpdaterInitializationSkipReason } from "@main/config/auto-updater";
import { logger } from "@main/utils/logger";
import { app } from "electron";
import electronUpdater from "electron-updater";
import fs from "fs";
const { autoUpdater } = electronUpdater;

const log = logger("auto-updater.ts");

export type UpdateInfo = electronUpdater.UpdateInfo;

export class AutoUpdaterAPI extends Downloader {
    private updateInfo?: UpdateInfo;
    private initialized: boolean = false;
    private updateAvailable: boolean = false;
    private checkRequest?: Promise<boolean>;
    private downloadRequest?: Promise<void>;
    public readonly currentVersion: string = autoUpdater.currentVersion.toString();

    public async init() {
        if (this.initialized) return this;
        log.info("Initializing AutoUpdaterAPI");
        // electron-updater may default this to true. A skipped initialization
        // must not leave installation on quit armed in a fresh macOS package.
        autoUpdater.autoInstallOnAppQuit = false;
        const skipReason = getAutoUpdaterInitializationSkipReason({
            isPackaged: app.isPackaged,
            platform: process.platform,
            resourcesPath: process.resourcesPath,
            pathExists: fs.existsSync,
        });
        if (skipReason) {
            log.info(`${skipReason}. Skipping initialization`);
            return this;
        }
        autoUpdater.autoDownload = false;
        autoUpdater.logger = log;
        autoUpdater.autoInstallOnAppQuit = true;
        autoUpdater.allowPrerelease = false;
        this.initialized = true;
        return this;
    }

    public get updateInformation(): UpdateInfo | null {
        return this.updateInfo ?? null;
    }

    public checkForUpdates(): Promise<boolean> {
        if (!this.initialized) return Promise.resolve(false);
        if (this.checkRequest) return this.checkRequest;
        // A check may replace the provider/version used by a download. Serialize
        // different operations and share the entire underlying request for duplicates.
        const request = Promise.resolve(this.downloadRequest).catch(() => undefined).then(() => this.checkOnce()).finally(() => {
            this.checkRequest = undefined;
        });
        this.checkRequest = request;
        return request;
    }

    private async checkOnce(): Promise<boolean> {
        this.updateAvailable = false;
        let emittedError = false;
        const onError = (error: Error, message?: string) => {
            emittedError = true;
            log.error(error, `Error checking for updates. ${message ?? ""}`);
        };
        autoUpdater.on("error", onError);
        try {
            const result = await autoUpdater.checkForUpdates();
            this.updateInfo = result?.updateInfo;
            this.updateAvailable = !emittedError && result?.isUpdateAvailable === true;
            return this.updateAvailable;
        } catch (error) {
            this.updateInfo = undefined;
            if (!emittedError) log.error(error, "Error checking for updates");
            return false;
        } finally {
            autoUpdater.removeListener("error", onError);
        }
    }

    public downloadUpdate(): Promise<void> {
        if (!this.initialized) return Promise.resolve();
        if (this.downloadRequest) return this.downloadRequest;
        const request = Promise.resolve(this.checkRequest).then(() => this.downloadOnce()).finally(() => {
            this.downloadRequest = undefined;
        });
        this.downloadRequest = request;
        return request;
    }

    private async downloadOnce(): Promise<void> {
        const info = this.updateInfo;
        if (!info || !this.updateAvailable) throw Error("Tried to download unavailable update");
        let downloadedInfo: UpdateInfo | undefined;
        let emittedError = false;
        const onProgress = (progress: electronUpdater.ProgressInfo) => {
            void this.downloadProgress({ type: "update",
                id: info.version, name: info.version, currentBytes: progress.transferred, totalBytes: progress.total, progress: progress.percent }).catch((error) => {
                log.error(error, "Error notifying update download progress");
            });
        };
        const onDownloaded = (result: UpdateInfo) => { downloadedInfo = result; };
        const onError = (error: Error, message?: string) => {
            emittedError = true;
            log.error(error, `Error downloading update. ${message ?? ""}`);
        };
        autoUpdater.on("download-progress", onProgress);
        autoUpdater.on("update-downloaded", onDownloaded);
        autoUpdater.on("error", onError);
        try {
            // On macOS the downloaded event precedes completion of native staging.
            // Keep the request and listeners alive until the returned promise settles.
            await autoUpdater.downloadUpdate();
            if (emittedError || !downloadedInfo) throw Error("Update download did not complete successfully");
            this.updateInfo = downloadedInfo;
            await this.downloadComplete({ type: "update",
                id: info.version, name: info.version, currentBytes: 1, totalBytes: 1, progress: 1 });
        } catch (error) {
            if (!emittedError) log.error(error, "Error downloading update");
            await this.downloadFailed({ type: "update",
                id: info.version, name: info.version, currentBytes: 0, totalBytes: 0, progress: 0 });
        } finally {
            autoUpdater.removeListener("download-progress", onProgress);
            autoUpdater.removeListener("update-downloaded", onDownloaded);
            autoUpdater.removeListener("error", onError);
        }
    }

    public installUpdates(): void {
        if (!this.initialized) return;
        autoUpdater.quitAndInstall(true, true);
    }

    public quitAndInstall(): void {
        if (!this.initialized) return;
        autoUpdater.quitAndInstall();
    }
}

export const autoUpdaterAPI = new AutoUpdaterAPI();
