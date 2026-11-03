// SPDX-FileCopyrightText: 2025 The BAR Lobby Authors
//
// SPDX-License-Identifier: MIT

import { Signal } from "$/jaz-ts-utils/signal";
import { REPLAYS_PATH } from "@main/config/app";
import { contentAPI } from "@main/content/content-api";
import { asyncParseReplay } from "@main/replays/parse-replay";
import { replayFailureCategory } from "@main/replays/replay-parse-contract";
import { Replay } from "@main/replays/replay";
import { gameAPI } from "@main/game/game";
import { isReplayFile } from "@main/config/replay-extensions";
import { logger } from "@main/utils/logger";
import chokidar from "chokidar";
import fs from "fs";

import path from "path";

const log = logger("replay-content.ts");

export class ReplaysAPI {
    public readonly onReplayCachingStarted: Signal<string> = new Signal();
    public readonly onReplayCached: Signal<Replay> = new Signal();
    public readonly onReplayDeleted: Signal<string> = new Signal();

    protected readonly replayCacheQueue: Set<string> = new Set();
    protected readonly filesBeingCopied: Set<string> = new Set(); // Track files being copied
    protected cachingReplays = false;

    public async init() {
        await fs.promises.mkdir(REPLAYS_PATH, { recursive: true });
        this.startWatchingReplayFolder();
        return this;
    }

    protected startWatchingReplayFolder() {
        const cacheChangedReplay = (filepath: string) => {
            if (!isReplayFile(filepath) || this.filesBeingCopied.has(path.basename(filepath))) {
                return;
            }
            void this.cacheReplay(filepath);
        };
        chokidar
            .watch(REPLAYS_PATH, {
                ignoreInitial: true,
                awaitWriteFinish: true,
            })
            .on("add", cacheChangedReplay)
            .on("change", cacheChangedReplay)
            .on("unlink", (filepath) => {
                if (!isReplayFile(filepath)) {
                    return;
                }
                this.onReplayDeleted.dispatch(path.basename(filepath));
            });
    }

    public async copyParseReplay(filePath: string) {
        const sourcePath = path.resolve(filePath);
        const replayDirectory = path.resolve(REPLAYS_PATH);
        const relativePath = path.relative(replayDirectory, sourcePath);
        // Only a direct member is represented by scanFolderForReplays and the
        // renderer's fileName key. Prefix siblings and nested imports are copied.
        const isManagedReplay = relativePath !== "" && relativePath === path.basename(relativePath);
        const replayPath = isManagedReplay ? sourcePath : path.join(replayDirectory, path.basename(sourcePath));
        const fileName = path.basename(replayPath);
        if (!isManagedReplay) this.filesBeingCopied.add(fileName);
        try {
            if (!isManagedReplay) await fs.promises.copyFile(sourcePath, replayPath);
            const replay = await asyncParseReplay(replayPath);
            this.onReplayCached.dispatch(replay);
            await this.fetchReplayMap(replay);
        } finally {
            if (!isManagedReplay) this.filesBeingCopied.delete(fileName);
        }
    }

    // Having the map makes the replay previewable, but the replay is what the caller asked to add, so a
    // map that will not download must not keep it out of the list.
    private async fetchReplayMap(replay: Replay) {
        try {
            await contentAPI.ensure([{ type: "map", id: replay.mapSpringName }]);
        } catch (err) {
            log.error({ err, mapSpringName: replay.mapSpringName, replayFilePath: replay.filePath, category: "map_fetch_failed" }, "Could not fetch replay map");
        }
    }

    public async sync(replayFileNames: string[]) {
        const existingFiles = await this.scanFolderForReplays();
        const replaysToDelete = replayFileNames.filter((fileName) => !existingFiles.includes(fileName));
        replaysToDelete.forEach((fileName) => this.onReplayDeleted.dispatch(fileName));
        existingFiles
            .filter((fileName) => !replayFileNames.includes(fileName))
            .map((fileName) => path.join(REPLAYS_PATH, fileName))
            .forEach((fileName) => this.cacheReplay(fileName));
    }

    protected async scanFolderForReplays() {
        let replayFiles = await fs.promises.readdir(REPLAYS_PATH);
        replayFiles = replayFiles.filter((replayFile) => isReplayFile(replayFile));
        return replayFiles;
    }

    public async deleteReplay(fileName: string) {
        try {
            await fs.promises.rm(path.join(REPLAYS_PATH, fileName));
        } catch (err) {
            log.error({ err, replayFilePath: path.join(REPLAYS_PATH, fileName), category: "delete_failed" }, "Error deleting replay");
        }
    }

    protected async cacheReplay(replayFilePath: string) {
        if (gameAPI.isGameRunning()) {
            this.replayCacheQueue.add(replayFilePath);
            return;
        }

        try {
            const replayData = await asyncParseReplay(replayFilePath);
            // TODO handle this use case
            // const conflictingReplay = await this.getReplayByGameId(replayData.gameId);
            // if (conflictingReplay) {
            //     // when leaving a game and rejoining it, 2 demo files are created with the same gameId. below we delete the shortest replay
            //     if (conflictingReplay.gameDurationMs > replayData.gameDurationMs) {
            //         await fs.promises.rm(replayFilePath);
            //     } else {
            //         const filePath = path.join(REPLAYS_PATH, conflictingReplay.fileName);
            //         await fs.promises.rm(filePath);
            //     }
            // }
            // Store the cached replay and dispatch the event
            this.onReplayCached.dispatch(replayData);
        } catch (err) {
            log.error({ err, replayFilePath, category: replayFailureCategory(err) }, "Error caching replay");
        }
    }

    public async cacheReplaysInQueue() {
        for (const replayFilePath of this.replayCacheQueue) {
            await this.cacheReplay(replayFilePath);
            this.replayCacheQueue.delete(replayFilePath);
        }
    }
}

export const replaysAPI = new ReplaysAPI();
