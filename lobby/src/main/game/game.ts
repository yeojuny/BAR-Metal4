import { acquireChecksumHold } from "@main/utils/checksums";
import { requireContentWritesOpen } from "@main/content/content-write-coordinator";
import { nativeProcessEnv } from "./native-runtime";
import { getNativeLaunchDisplayEnv } from "./native-launch-display";
import { assertNativeSyncVersion } from "./native-version";
// SPDX-FileCopyrightText: 2025 The BAR Lobby Authors
//
// SPDX-License-Identifier: MIT

import type { ChildProcess } from "child_process";
import { spawn } from "child_process";
import * as fs from "fs";
import { Signal } from "$/jaz-ts-utils/signal";
import * as path from "path";

import { contentAPI } from "@main/content/content-api";
import { applyDefaultSpringsettings } from "@main/game/springsettings";
import { startScriptConverter } from "@main/utils/start-script-converter";
import { logger } from "@main/utils/logger";
import { WRITE_DATA_PATH, REPLAYS_PATH, getEnginePath, getAssetsPath } from "@main/config/app";
import { BattleWithMetadata } from "@main/game/battle/battle-types";
import { Replay } from "@main/replays/replay";
import { getPackagedNativeRuntime } from "./packaged-runtime";
import { requireNativeVersion, type NativeRuntime } from "./native-runtime";
import { engineProcessSlot } from "./engine-process-slot";
import { authorizeExperimentalOfflineScene, stopExperimentalSceneProcess, waitForExperimentalScene } from "./experimental-scene";

interface GameLaunchSettings {
    engineVersion?: string;
    gameVersion?: string;
    mapSpringName?: string;
    launchArg?: string;
    script?: string;
}

const log = logger("main/game/game.ts");
const engineLogger = logger("[RECOIL ENGINE]", { separator: "\n", level: "info" });

export interface ScriptLaunchSettings {
    engineVersion: string;
    gameVersion: string;
    script: string;
}

export interface MultiplayerLaunchSettings {
    engineVersion: string;
    gameVersion: string;
    springString: string;
}

export class GameAPI {
    public onGameLaunched = new Signal<void>();
    public onGameClosed: Signal<number | null> = new Signal();
    public readonly startScriptName = "script.txt";

    protected gameProcess: ChildProcess | null = null;
    private experimentalGameProcess: ChildProcess | null = null;

    public async launchBattle(battle: BattleWithMetadata): Promise<void> {
        if (battle.isOnline) throw new Error("Online battles are not supported yet");
        const script = await startScriptConverter.generateScriptStr(battle);
        const scriptPath = path.join(WRITE_DATA_PATH, this.startScriptName);
        await this.launchPrepared(
            {
                engineVersion: battle.battleOptions.engineVersion,
                gameVersion: battle.battleOptions.gameVersion,
                mapSpringName: battle.battleOptions.map?.springName,
                launchArg: scriptPath,
                script,
            },
            true
        );
    }

    public async launchReplay(replay: Replay) {
        return this.launch({
            engineVersion: replay.engineVersion,
            gameVersion: replay.gameVersion,
            mapSpringName: replay.mapSpringName,
            launchArg: replay.filePath ? replay.filePath : path.join(REPLAYS_PATH, replay.fileName),
        });
    }

    public async launchScript({ engineVersion, gameVersion, script }: ScriptLaunchSettings) {
        log.debug(`Launching game with script: ${script}`);
        const scriptGameVersion = script.match(/gametype\s*=\s*(.*);/)?.[1];
        const mapSpringName = script.match(/mapname\s*=\s*(.*);/)?.[1];
        if (!mapSpringName) {
            throw new Error("Could not parse map name from script");
        }
        const scriptPath = path.join(WRITE_DATA_PATH, this.startScriptName);
        await this.launch({
            engineVersion,
            gameVersion: scriptGameVersion || gameVersion, // using script's game version if available
            mapSpringName,
            launchArg: scriptPath,
            script,
        });
    }

    public async launchMultiplayer({ engineVersion, gameVersion, springString }: MultiplayerLaunchSettings) {
        return this.launch({
            engineVersion,
            gameVersion,
            launchArg: springString,
        });
    }

    public async launch(settings: GameLaunchSettings): Promise<void> {
        return this.launchPrepared(settings);
    }

    private async launchPrepared({ engineVersion, gameVersion, mapSpringName, launchArg, script }: GameLaunchSettings, offlineBattle = false): Promise<void> {
        if (!engineVersion || !gameVersion || !launchArg) {
            throw new Error("Engine Version, Game Version and launch Arguments need to be specified");
        }

        log.info(`Launching game with engine: ${engineVersion}, game: ${gameVersion}`);

        requireContentWritesOpen();
        const releaseSlot = engineProcessSlot.acquire("game");
        let releaseChecksums: (() => void) | undefined;
        const releaseOwnership = () => { releaseChecksums?.(); releaseSlot(); };
        let nativeRuntime: NativeRuntime | undefined;
        let experimentalScene = false;
        try {
            if (process.platform === "darwin") {
                nativeRuntime = getPackagedNativeRuntime();
                requireNativeVersion(nativeRuntime, engineVersion);
                if (!nativeRuntime.manifest.engine.capabilities.gameplay) {
                    authorizeExperimentalOfflineScene(nativeRuntime, offlineBattle);
                    experimentalScene = true;
                    log.warn("Opening experimental singleplayer scene; original in-game UI and full gameplay remain incomplete.");
                }
            }
            // Shared script.txt is written only after obtaining the global slot,
            // so a refused simultaneous launch cannot replace a pending script.
            if (script !== undefined) {
                if (nativeRuntime) await fs.promises.mkdir(WRITE_DATA_PATH, { recursive: true });
                await fs.promises.writeFile(launchArg, script);
            }

            // Playing something is the clearest sign it is still wanted, and it is the one thing every
            // launch path has in common.
            void contentAPI.markUsed([{ type: "engine", id: engineVersion }, { type: "game", id: gameVersion }, ...(mapSpringName ? [{ type: "map" as const, id: mapSpringName }] : [])]);
            await this.fetchMissingContent(engineVersion, gameVersion); // TODO preload anything needed through the UI before launching. Remove this step

            if (nativeRuntime) {
                releaseChecksums = await acquireChecksumHold();
                requireContentWritesOpen();
                await assertNativeSyncVersion(nativeRuntime);
            }
            applyDefaultSpringsettings();
        } catch (error) {
            releaseOwnership();
            throw error;
        }
        const enginePath = nativeRuntime ? path.dirname(nativeRuntime.binary) : path.join(getEnginePath(), engineVersion).replaceAll("\\", "/");
        const args = ["--write-dir", WRITE_DATA_PATH, "--isolation", ...(experimentalScene ? ["--only-local"] : []), launchArg];
        const binaryName = nativeRuntime ? nativeRuntime.binary : process.platform === "win32" ? "spring.exe" : "./spring";
        log.debug(`Running binary: ${path.join(enginePath, binaryName)}, args: ${args}`);

        if (experimentalScene) {
            let child: ChildProcess | undefined;
            try {
                requireContentWritesOpen();
                child = spawn(binaryName, args, {
                    cwd: enginePath,
                    stdio: "pipe",
                    detached: false,
                    env: { ...nativeProcessEnv(nativeRuntime!, getAssetsPath()), ...getNativeLaunchDisplayEnv() },
                });
                this.gameProcess = child;
                this.experimentalGameProcess = child;
                const spawnedChild = child;
                // The close handler retains both slots even after a spawn error.
                spawnedChild.once("close", (code) => {
                    releaseOwnership();
                    if (this.gameProcess === spawnedChild) this.gameProcess = null;
                    if (this.experimentalGameProcess === spawnedChild) this.experimentalGameProcess = null;
                    this.onGameClosed.dispatch(code);
                });
                child.stdout?.on("data", (data) => engineLogger.info(`${data}`));
                child.stderr?.on("data", (data) => engineLogger.error(`${data}`));
                await waitForExperimentalScene(child, () => this.onGameLaunched.dispatch());
                return;
            } catch (error) {
                if (child?.pid && this.experimentalGameProcess === child) {
                    await stopExperimentalSceneProcess(child).catch((stopError) => log.error(stopError));
                } else if (!child) releaseOwnership();
                throw error;
            }
        }

        return new Promise<void>((resolve, reject) => {
            let isGameRunning = false;
            try {
                requireContentWritesOpen();
                this.gameProcess = spawn(binaryName, args, {
                    cwd: enginePath,
                    stdio: "pipe",
                    detached: true,
                    env: nativeRuntime ? { ...nativeProcessEnv(nativeRuntime, getAssetsPath()), ...getNativeLaunchDisplayEnv() } : { ...process.env, SPRING_DATADIR: getAssetsPath() },
                });
                const child = this.gameProcess;
                child.once("close", () => {
                    releaseOwnership();
                    if (this.gameProcess === child) this.gameProcess = null;
                    if (nativeRuntime && isGameRunning) this.onGameClosed.dispatch(null);
                });
                if (!this.gameProcess.stdout || !this.gameProcess.stderr) throw new Error("failed to access game process stream");
                this.gameProcess.stdout.on("data", (data) => {
                    engineLogger.info(`${data}`);
                    // Check if the game has started
                    // This is a heuristic based on the log output, as the engine does not provide a specific event for game launch
                    //TODO replace this with some ipc event from the engine when it is available
                    if (!isGameRunning && data.toString().includes("[Game::Load]")) {
                        isGameRunning = true;
                        this.onGameLaunched.dispatch();
                        resolve();
                    }
                    //TODO this is a workaround for the fact that the engine does not exit right away when the game is closed (can take a few seconds on Linux)
                    // See https://github.com/beyond-all-reason/RecoilEngine/issues/2450
                    if (!nativeRuntime && isGameRunning && data.toString().includes("[SpringApp::Kill][8]")) {
                        log.info("Game process requested to close");
                        this.gameProcess = null;
                        isGameRunning = false;
                        this.onGameClosed.dispatch(null);
                    }
                });
                this.gameProcess.stderr.on("data", (data) => {
                    engineLogger.error(`${data}`);
                });
                this.gameProcess.addListener("error", (err) => {
                    log.error(err);
                    reject(err);
                    // Keep ownership until close, including spawn errors.
                });
                this.gameProcess.addListener("exit", (code) => {
                    if (code !== 0) {
                        log.error(`Game process exited with code: ${code}`);
                        reject(new Error(`Game process exited with code: ${code}`));
                    } else {
                        log.info(`Game process exited with code: ${code}`);
                        if (!isGameRunning) reject(new Error("Engine exited before the game loaded."));
                    }
                    if (!nativeRuntime && isGameRunning) this.onGameClosed.dispatch(code); // Dispatch if the game closed unexpectedly
                });
                this.gameProcess.addListener("spawn", () => {
                    log.debug(`Game process spawned successfully`);
                });
                this.gameProcess.addListener("close", (exitCode) => {
                    log.debug(`Game process closed with exit code: ${exitCode}`);
                });
                log.debug(`Game process PID: ${this.gameProcess.pid}`);
            } catch (err) {
                log.error(`Failed to launch game: ${err}`);
                if (this.gameProcess) this.gameProcess.kill("SIGTERM");
                else releaseOwnership();
                reject(err);
            }
        });
    }

    public isGameRunning() {
        return this.gameProcess !== null;
    }

    public isExperimentalSceneRunning() {
        return this.experimentalGameProcess !== null;
    }

    public async stopOwnedNativeEngine(): Promise<void> {
        if (process.platform === "darwin" && this.gameProcess) await stopExperimentalSceneProcess(this.gameProcess, 2000);
    }

    public async stopExperimentalScene(): Promise<void> {
        if (this.experimentalGameProcess) await stopExperimentalSceneProcess(this.experimentalGameProcess);
    }

    //TODO not handling maps, not sure if needed if we always come from the lobby's UI
    protected async fetchMissingContent(engineVersion?: string, gameVersion?: string) {
        if (!engineVersion || !gameVersion) {
            throw new Error("Engine Version and Game Version need to be specified");
        }

        await contentAPI.ensure([
            { type: "engine", id: engineVersion },
            { type: "game", id: gameVersion },
        ]);
    }
}

export const gameAPI = new GameAPI();
