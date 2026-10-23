import { getPackagedNativeRuntime, getNativeRuntimeForVersion } from "@main/game/packaged-runtime";
// SPDX-FileCopyrightText: 2025 The BAR Lobby Authors
//
// SPDX-License-Identifier: MIT

import axios from "axios";
import * as fs from "fs";
import { glob } from "glob";
import { removeFromArray } from "$/jaz-ts-utils/object";
import * as path from "path";
import { EngineAI, EngineVersion } from "@main/content/engine/engine-version";
import { DownloadInfo } from "../downloads";
import { parseLuaTable } from "@main/utils/parse-lua-table";
import { parseLuaOptions } from "@main/utils/parse-lua-options";
import { logger } from "@main/utils/logger";
import { extract7z } from "@main/utils/extract-7z";
import { getEngineReleaseInfo } from "@main/config/content-sources";
import { AbstractContentAPI } from "@main/content/abstract-content";
import { BUNDLED_ASSETS_PATH, getAssetsPath, getEnginePath } from "@main/config/app";
import { configService } from "@main/services/config.service";
import { compareEngineVersions, isCompatibleEngineVersion } from "@main/content/engine/engine-version-order";
import { holdChecksums } from "@main/utils/checksums";

import { getEngineRootSearchPaths, resolveEngineVersionPath } from "@main/config/native-engine-runner";

const log = logger("engine-provider.ts");

export class EngineProvider extends AbstractContentAPI<string, EngineVersion> {
    protected get engineDirs() {
        return getEnginePath();
    }

    public override async init() {
        log.info("Initializing engine content API");
        if (process.platform === "darwin") {
            for (const lane of ["development", "official-multiplayer"] as const) {
                try {
                    const runtime = getPackagedNativeRuntime(lane);
                    const version = runtime.manifest.engine.syncVersion ?? runtime.manifest.engine.id;
                    const ais = await this.parseAis(version);
                    this.availableVersions.set(version, { id: version, ais, installed: true });
                } catch (error) {
                    log.warn(`Native ${lane} engine is unavailable: ${String(error)}`);
                }
            }
            this.checkIfDefaultIsNew();
            return this;
        }

        try {
            await fs.promises.mkdir(this.engineDirs, { recursive: true });
            const dirs = new Map<string, string>();
            const engineRoots = getEngineRootSearchPaths({ assetsPath: getAssetsPath(), bundledAssetsPath: BUNDLED_ASSETS_PATH });
            for (const engineRoot of engineRoots) {
                if (!fs.existsSync(engineRoot)) continue;
                const files = await fs.promises.readdir(engineRoot, { withFileTypes: true });
                for (const dir of files
                    .filter((file) => file.isDirectory() || file.isSymbolicLink())
                    .map((dir) => dir.name)
                    .filter((dir) => isCompatibleEngineVersion(dir))) {
                    if (!dirs.has(dir)) dirs.set(dir, path.join(engineRoot, dir));
                }
            }
            log.info(`Found ${dirs.size} installed engine versions`);
            for (const [engineVersion, enginePath] of dirs) {
                log.info(`-- Engine ${engineVersion} at ${enginePath}`);
                let ais: EngineAI[] = [];
                try {
                    ais = await this.parseAis(engineVersion);
                } catch (err) {
                    log.error(`Failed to parse engine AI definitions for ${engineVersion}: ${err instanceof Error ? err.message : String(err)}`);
                }
                this.availableVersions.set(engineVersion, { id: engineVersion, ais, installed: true });
            }
        } catch (err) {
            if ((err as NodeJS.ErrnoException).code !== "ENOENT") {
                log.error(err);
            }
        }
        this.checkIfDefaultIsNew();
        log.info(`Found ${this.availableVersions.size} engine versions total.`);
        return this;
    }

    public async reinit() {
        await fs.promises.mkdir(this.engineDirs, { recursive: true });
        this.availableVersions.clear();
        await this.init();
    }

    public isVersionInstalled(id: string): boolean {
        return this.availableVersions.get(id)?.installed ?? false;
    }

    public getDefaultEngine() {
        return this.availableVersions.get(configService.getConfig().defaultEngineVersion);
    }

    public getLobbySelectableVersions(platform: NodeJS.Platform = process.platform) {
        if (platform === "darwin") {
            const defaultEngine = this.getDefaultEngine();
            return defaultEngine ? [defaultEngine] : [];
        }

        return this.availableVersions.values().toArray();
    }

    public getInstalledVersionsNewestFirst() {
        return this.availableVersions
            .values()
            .filter((version) => version.installed)
            .toArray()
            .sort((a, b) => compareEngineVersions(b.id, a.id));
    }

    public getNewestInstalledVersion() {
        return this.getInstalledVersionsNewestFirst().at(0);
    }

    protected checkIfDefaultIsNew() {
        if (!this.availableVersions.has(configService.getConfig().defaultEngineVersion)) {
            this.availableVersions.set(configService.getConfig().defaultEngineVersion, {
                id: configService.getConfig().defaultEngineVersion,
                ais: [],
                installed: false,
            });
        }
    }

    public async downloadEngine(version?: string) {
        const engineVersion = version || configService.getConfig().defaultEngineVersion;
        if (process.platform === "darwin") {
            getNativeRuntimeForVersion(engineVersion);
            if (!this.isVersionInstalled(engineVersion)) throw new Error(`Native engine ${engineVersion} is not installed in this package.`);
            return engineVersion;
        }
        try {
            if (this.isVersionInstalled(engineVersion)) {
                return;
            }
            const engineInfo = await getEngineReleaseInfo(engineVersion);
            const downloadInfo: DownloadInfo = {
                type: "engine",
                id: engineVersion,
                name: engineVersion,
                currentBytes: 0,
                totalBytes: 1,
                progress: 0,
            };
            this.currentDownloads.push(downloadInfo);
            this.downloadStarted(downloadInfo);
            log.info(`Downloading engine: ${engineVersion}`);
            const downloadResponse = await axios({
                url: engineInfo.mirrors[0],
                method: "get",
                responseType: "arraybuffer",
                headers: { "Content-Type": "application/7z" },
                onDownloadProgress: (progress) => {
                    downloadInfo.currentBytes = progress.loaded;
                    downloadInfo.totalBytes = progress.total || -1;
                    this.downloadProgress(downloadInfo);
                },
            });
            const engine7z = downloadResponse.data as ArrayBuffer;
            const downloadedFilePath = path.join(this.engineDirs, engineInfo.filename);
            const engineDestinationPath = path.join(this.engineDirs, engineVersion);
            log.info(`Extracting <${engineInfo.filename}> to ${engineDestinationPath}`);
            await fs.promises.mkdir(this.engineDirs, { recursive: true });
            await fs.promises.writeFile(downloadedFilePath, Buffer.from(engine7z), { encoding: "binary" });
            downloadInfo.phase = "extracting";
            this.downloadProgress(downloadInfo);
            await extract7z(downloadedFilePath, engineDestinationPath);
            await fs.promises.unlink(downloadedFilePath);
            removeFromArray(this.currentDownloads, downloadInfo);
            log.info(`Extracted engine <${engineInfo.filename}>`);
            await this.downloadComplete(downloadInfo);
            log.info(`Downloaded engine: ${engineVersion}`);
            return engineVersion;
        } catch (err) {
            log.error(err);
            throw new Error(`Failed to download engine version ${engineVersion}: ${err instanceof Error ? err.message : String(err)}`);
        }
    }

    public async uninstallVersion(version: EngineVersion | string) {
        if (process.platform === "darwin") throw new Error("Packaged native engines are immutable; replace the reviewed candidate app instead.");
        if (typeof version === "object") {
            version = version.id;
        }
        const engineDir = resolveEngineVersionPath({ engineVersion: version, assetsPath: getAssetsPath(), bundledAssetsPath: BUNDLED_ASSETS_PATH });
        if (BUNDLED_ASSETS_PATH && path.relative(BUNDLED_ASSETS_PATH, engineDir).split(path.sep)[0] !== "..") {
            throw new Error("Bundled engines cannot be removed from the app.");
        }
        // Checksums run the engine out of this directory, so deleting it underneath one fails on Windows.
        await holdChecksums(() => fs.promises.rm(engineDir, { force: true, recursive: true }));
        this.availableVersions.delete(version);
    }

    protected override async downloadComplete(downloadInfo: DownloadInfo) {
        log.debug(`Download complete: ${downloadInfo.name}`);
        const ais = await this.parseAis(downloadInfo.name);
        this.availableVersions.set(downloadInfo.name, { id: downloadInfo.name, ais, installed: true });
        super.downloadComplete(downloadInfo);
    }

    protected async parseAis(engineVersion: string): Promise<EngineAI[]> {
        const ais: EngineAI[] = [];
        const enginePath = resolveEngineVersionPath({ engineVersion, assetsPath: getAssetsPath(), bundledAssetsPath: BUNDLED_ASSETS_PATH });
        const dataPath = process.platform === "darwin" ? getNativeRuntimeForVersion(engineVersion).dataDirectory : enginePath;
        const aisPath = path.join(dataPath, "AI", "Skirmish");
        let aiDirs: string[];
        try {
            aiDirs = await fs.promises.readdir(aisPath);
        } catch (err) {
            if (typeof err === "object" && err !== null && "code" in err && err.code === "ENOENT") {
                log.warn(`No engine AI definitions found for ${engineVersion}: ${aisPath}`);
                return ais;
            }
            throw err;
        }
        for (const aiDir of aiDirs) {
            try {
                const ai = await this.parseAi(path.join(aisPath, aiDir));
                ais.push(ai);
            } catch (err) {
                console.error(`Error parsing AI: ${aiDir}`, err);
            }
        }
        return ais;
    }

    protected async parseAi(aiPath: string): Promise<EngineAI> {
        const aiDefinitions = await glob(`${aiPath}/**/{AIInfo.lua,AIOptions.lua}`, { windowsPathsNoEscape: true });
        const aiInfoPath = aiDefinitions.find((filePath) => filePath.endsWith("AIInfo.lua"));
        const aiOptionsPath = aiDefinitions.find((filePath) => filePath.endsWith("AIOptions.lua"));
        if (aiInfoPath === undefined) {
            throw new Error(`AIInfo.lua not found in ${aiPath}`);
        }
        if (aiOptionsPath === undefined) {
            throw new Error(`AIOptions.lua not found in ${aiPath}`);
        }
        const aiInfoFile = await fs.promises.readFile(aiInfoPath);
        const aiInfoFields = parseLuaTable(aiInfoFile);
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        const aiInfo: Record<string, any> = {};
        for (const field of aiInfoFields) {
            aiInfo[field.key] = field.value;
        }
        const aiOptionsFile = await fs.promises.readFile(aiOptionsPath);
        const aiOptions = parseLuaOptions(aiOptionsFile);
        return {
            shortName: aiInfo.shortName,
            name: aiInfo.name,
            description: aiInfo.description,
            version: aiInfo.version,
            options: aiOptions,
        };
    }

    //TODO move this check to front
    // protected async cleanupOldVersions() {
    //     log.info("Cleaning up old engine versions");
    //     const maxDays = 90;
    //     const oldestDate = new Date();
    //     oldestDate.setDate(oldestDate.getDate() - maxDays);
    //     const versionsToRemove = await cacheDb.selectFrom("engineVersion").where("lastLaunched", "<", oldestDate).select("id").execute();
    //     for (const version of versionsToRemove) {
    //         await this.uninstallVersion(version.id);
    //     }
    // }
}

export const engineProvider = new EngineProvider();
