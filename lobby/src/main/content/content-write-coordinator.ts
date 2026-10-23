// SPDX-FileCopyrightText: 2026 The BAR Lobby Authors
//
// SPDX-License-Identifier: MIT

import type { ChildProcess } from "node:child_process";
import { randomUUID } from "node:crypto";
import fs from "node:fs/promises";
import path from "node:path";
import { clearTimeout, setTimeout } from "node:timers";

// Shared by writers in this Electron main process only, not by other processes.
// Keys are physical assets directories, never engine versions or Rapid aliases.
const writerTails = new Map<string, Promise<void>>();
const writerChildren = new Map<ChildProcess, Promise<void>>();
let stopping = false;
let shutdown: Promise<void> | undefined;

export function requireContentWritesOpen() {
    if (stopping) throw new Error("Content writes stopped because the launcher is quitting");
}

/** Non-reentrant: wrap the actual writer, not a caller that invokes other writers. */
export async function withContentWrite<T>(assetsDirectory: string, write: (canonicalAssetsDirectory: string) => T | Promise<T>): Promise<T> {
    requireContentWritesOpen();
    const assetsPath = path.resolve(assetsDirectory);
    await fs.mkdir(assetsPath, { recursive: true });
    const canonicalAssetsPath = await fs.realpath(assetsPath);
    requireContentWritesOpen();
    const previous = writerTails.get(canonicalAssetsPath) ?? Promise.resolve();
    let release!: () => void;
    const completed = new Promise<void>((resolve) => {
        release = resolve;
    });
    writerTails.set(canonicalAssetsPath, completed);

    await previous;
    try {
        requireContentWritesOpen();
        return await write(canonicalAssetsPath);
    } finally {
        release();
        if (writerTails.get(canonicalAssetsPath) === completed) {
            writerTails.delete(canonicalAssetsPath);
        }
    }
}

/** An error can precede close; never admit another writer while the child lives. */
export function waitForContentWriterClose(child: ChildProcess): Promise<{ code: number | null; signal: NodeJS.Signals | null }> {
    let processError: Error | undefined;
    const completion = new Promise<{ code: number | null; signal: NodeJS.Signals | null }>((resolve, reject) => {
        const onError = (error: Error) => {
            processError ??= error;
        };
        child.on("error", onError);
        child.once("close", (code: number | null, signal: NodeJS.Signals | null) => {
            writerChildren.delete(child);
            child.removeListener("error", onError);
            if (processError) {
                reject(processError);
            } else {
                resolve({ code, signal });
            }
        });
    });
    // Register immediately after spawn. An error/exit or successful kill request
    // is not proof that the child and its output pipes have closed.
    writerChildren.set(child, completion.then(() => undefined, () => undefined));
    if (stopping) {
        try { child.kill("SIGKILL"); }
        catch (error) { processError ??= error instanceof Error ? error : new Error(String(error)); }
    }
    return completion;
}

/** Only registered parent content children; never detached native/helper owners. */
export function shutdownContentWriters(): Promise<void> {
    if (shutdown) return shutdown;
    stopping = true;
    let rejectTermination!: (error: unknown) => void;
    const failed = new Promise<never>((_, reject) => { rejectTermination = reject; });
    const drained = Promise.all([...writerChildren.values(), ...writerTails.values()]).then(() => undefined);
    const terminate = (signal: NodeJS.Signals) => {
        for (const child of writerChildren.keys()) {
            // child.killed means a signal was sent, not that the process exited.
            if (child.exitCode !== null || child.signalCode !== null) continue;
            try { child.kill(signal); }
            catch (error) {
                // A failed TERM still gets a KILL attempt. Failure to terminate
                // must keep the app alive instead of declaring a clean exit.
                if (signal === "SIGKILL") rejectTermination(error);
            }
        }
    };
    // Shutdown-only courtesy interval, never a download/performance threshold.
    const force = setTimeout(() => terminate("SIGKILL"), 2_000);
    // Bound the subsequent close/drain accounting too. Timeout reports failure;
    // it never discards ownership, releases writer slots, or authorizes exit.
    const deadline = setTimeout(() => rejectTermination(new Error("Owned content writers did not close within the launcher shutdown deadline")), 5_000);
    shutdown = Promise.race([drained, failed]).finally(() => {
        clearTimeout(force);
        clearTimeout(deadline);
    }).catch((error: unknown) => {
        // Keep stopping=true and every live-child/tail entry. Only the failed quit attempt is reset.
        // A later attempt re-reads actual close/drain promises; writes never reopen during shutdown.
        shutdown = undefined;
        throw error;
    });
    terminate("SIGTERM");
    return shutdown;
}

/** Fetch/decode outside this operation; only publication owns the writer slot. */
export async function publishContentMetadata(assetsDirectory: string, metadataPath: string, contents: string | Uint8Array): Promise<void> {
    await withContentWrite(assetsDirectory, async () => {
        await fs.mkdir(path.dirname(metadataPath), { recursive: true });
        const temporaryPath = `${metadataPath}.${randomUUID()}.tmp`;
        try {
            await fs.writeFile(temporaryPath, contents, { flag: "wx" });
            await fs.rename(temporaryPath, metadataPath);
        } finally {
            await fs.rm(temporaryPath, { force: true });
        }
    });
}
