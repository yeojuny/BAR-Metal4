// SPDX-FileCopyrightText: 2026 The BAR Lobby Authors
//
// SPDX-License-Identifier: MIT

import * as path from "path";
import { spawn } from "child_process";
import { logger } from "@main/utils/logger";
import { getEnginePath, WRITE_DATA_PATH, getAssetsPath, BUNDLED_ASSETS_PATH } from "@main/config/app";
import { getNativeEngineProcessEnv, resolveNativeEngineRunner } from "@main/config/native-engine-runner";
import { waitForContentWriterClose, withContentWrite } from "@main/content/content-write-coordinator";

const log = logger("checksums.ts");

let checksumQueue: Promise<unknown> = Promise.resolve();

// A rejection must not poison the chain, or one failed removal would stop every later checksum.
function enqueue<T>(task: () => Promise<T>): Promise<T> {
    const result = checksumQueue.then(task);
    checksumQueue = result.catch(() => undefined);

    return result;
}

export function calcChecksum(engineVersion: string, archiveName: string): Promise<void> {
    return enqueue(() => process.platform === "darwin"
        ? withContentWrite(getAssetsPath(), () => runChecksumProcess(engineVersion, archiveName)).catch((error) => {
            log.warn(`calcChecksum did not run for ${archiveName}: ${error}`);
        })
        : runChecksumProcess(engineVersion, archiveName));
}

/**
 * Runs work with the checksum queue held, so nothing is checksumming while it does.
 *
 * The engine holds an archive open while checksumming it, so deleting that archive fails outright on
 * Windows. Waiting for the queue to drain is not enough: anything may queue a checksum the moment the
 * wait resolves, so removal takes its turn in the queue instead of watching it.
 */
export function holdChecksums<T>(work: () => Promise<T>): Promise<T> {
    return enqueue(work);
}

/** Keep checksum processes out of a native game's shared cache and infolog until it closes. */
export async function acquireChecksumHold(): Promise<() => void> {
    let grant!: (release: () => void) => void;
    let deny!: (error: unknown) => void;
    const ready = new Promise<() => void>((resolve, reject) => { grant = resolve; deny = reject; });
    void holdChecksums(() => new Promise<void>((release) => grant(release))).catch(deny);
    return ready;
}

function runChecksumProcess(engineVersion: string, archiveName: string): Promise<void> {
    let enginePath = path.join(getEnginePath(), engineVersion).replaceAll("\\", "/");
    let binaryName = process.platform === "win32" ? "spring-headless.exe" : "./spring-headless";
    let env: NodeJS.ProcessEnv = { ...process.env, SPRING_DATADIR: getAssetsPath() };
    if (process.platform === "darwin") {
        // The native bundle supplies spring, whose --calc-checksum path exits
        // before graphical initialization. Resolve the requested version just
        // like gameplay; do not assume a separate spring-headless was bundled.
        try {
            const paths = { assetsPath: getAssetsPath(), bundledAssetsPath: BUNDLED_ASSETS_PATH };
            const runner = resolveNativeEngineRunner({ ...paths, engineVersion });
            enginePath = runner.enginePath;
            binaryName = runner.spawnCommand;
            env = getNativeEngineProcessEnv({
                ...paths,
                enginePath,
                engineVersion,
                writeDataPath: WRITE_DATA_PATH,
                includeBundledAssetsInSpringDataDir: false,
                includeBundledEngineSupportInSpringDataDir: true,
                includeBundledMacOSLuaUIOverlayInSpringDataDir: false,
            });
        } catch (error) {
            log.warn(`calcChecksum could not resolve native engine ${engineVersion}: ${error}`);
            return Promise.resolve();
        }
    }
    // --calc-checksum does not return a value to us; it causes the engine to write a checksum cache
    // file to disk that speeds up subsequent archive loading (avoids re-scanning on next launch).
    const args = ["--write-dir", WRITE_DATA_PATH, "--isolation", "--calc-checksum", archiveName];

    log.debug(`Calculating checksum for: ${archiveName}`);
    return new Promise<void>((resolve) => {
        const proc = spawn(binaryName, args, {
            cwd: enginePath,
            // Output is not consumed; avoid filling an unread pipe on large scans.
            stdio: "ignore",
            env,
        });
        if (process.platform === "darwin") {
            // These children also write native cache/log files. Register them
            // with the same close/drain owner as downloads before returning.
            waitForContentWriterClose(proc).then(({ code }) => {
                if (code !== 0) log.warn(`calcChecksum exited with code ${code} for: ${archiveName}`);
                resolve();
            }, (error) => {
                log.warn(`calcChecksum failed for ${archiveName}: ${error}`);
                resolve();
            });
            return;
        }
        proc.on("exit", (code) => {
            if (code !== 0) log.warn(`calcChecksum exited with code ${code} for: ${archiveName}`);
            resolve();
        });
        proc.on("error", (err) => {
            log.warn(`calcChecksum failed for ${archiveName}: ${err}`);
            resolve();
        });
    });
}
