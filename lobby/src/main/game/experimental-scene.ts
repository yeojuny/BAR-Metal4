// SPDX-FileCopyrightText: 2026 The BAR Lobby Authors
//
// SPDX-License-Identifier: MIT

import type { ChildProcess } from "node:child_process";
import type { NativeRuntime } from "./native-runtime";

export function experimentalOfflineSceneRequested(platform = process.platform, architecture = process.arch, value = process.env.BAR_METAL4_EXPERIMENTAL_SINGLEPLAYER): boolean {
    return platform === "darwin" && architecture === "arm64" && value === "1";
}

export function authorizeExperimentalOfflineScene(runtime: NativeRuntime, offlineBattle: boolean): void {
    if (!offlineBattle || !experimentalOfflineSceneRequested()) throw new Error("The packaged native engine is not ready for gameplay yet.");
    if (runtime.manifest.engine.experimentalOfflineScene !== true) throw new Error("This native runtime does not contain the experimental singleplayer scene.");
}

// Metal4SceneBridge.cpp logs this only after the first DrawWorld submission
// retired and presented. Early Game::Load messages do not establish readiness.
const SCENE_PRESENTED = /\[Metal4Scene\] First presented simulation frame \d+:/;
export const SCENE_STARTUP_TIMEOUT_MS = 90000;
interface ChildStopState {
    closed: boolean;
    completion: Promise<void>;
    attempt?: Promise<void>;
}
const stoppingChildren = new WeakMap<ChildProcess, ChildStopState>();
const pendingStartups = new WeakMap<ChildProcess, (error: Error) => void>();

export function registerNativeStartupCancellation(child: ChildProcess, cancel: (error: Error) => void): () => void {
    if (stoppingChildren.has(child)) {
        cancel(new Error("Native engine startup was cancelled because shutdown was requested."));
    } else {
        pendingStartups.set(child, cancel);
    }
    return () => {
        if (pendingStartups.get(child) === cancel) pendingStartups.delete(child);
    };
}

export function stopExperimentalSceneProcess(child: ChildProcess, graceMs = 5000): Promise<void> {
    let state = stoppingChildren.get(child);
    if (!state) {
        let markClosed!: () => void;
        const completion = new Promise<void>((resolve) => { markClosed = resolve; });
        state = { closed: false, completion };
        const owned = state;
        // This close observer survives a timed-out quit attempt; exitCode is not close evidence.
        child.once("close", () => { owned.closed = true; markClosed(); });
        stoppingChildren.set(child, state);
    }
    // Cancel acceptance synchronously, before a termination attempt can fail.
    // Actual-close observation and slot/checksum ownership remain independent.
    const cancelStartup = pendingStartups.get(child);
    pendingStartups.delete(child);
    cancelStartup?.(new Error("Native engine startup was cancelled because shutdown was requested."));
    if (state.closed) return state.completion;
    if (state.attempt) return state.attempt;
    const owned = state;
    let force: NodeJS.Timeout | undefined;
    let deadline: NodeJS.Timeout | undefined;
    const termination = new Promise<never>((_, reject) => {
        force = setTimeout(() => {
            try { if (!owned.closed && child.exitCode == null && child.signalCode == null) child.kill("SIGKILL"); }
            catch (error) { reject(error); }
        }, graceMs);
        deadline = setTimeout(() => reject(new Error("The owned native process has not closed; the launcher remains open. Retry quit after it closes.")), Math.max(graceMs + 1000, 5000));
        force.unref();
        deadline.unref();
        try { if (!owned.closed && child.exitCode == null && child.signalCode == null) child.kill("SIGTERM"); }
        catch (error) { reject(error); }
    });
    owned.attempt = Promise.race([owned.completion, termination]).finally(() => {
        clearTimeout(force);
        clearTimeout(deadline);
    }).catch((error: unknown) => {
        owned.attempt = undefined;
        throw error;
    });
    return owned.attempt;
}

export function waitForExperimentalScene(child: ChildProcess, onReady: () => void, timeoutMs = SCENE_STARTUP_TIMEOUT_MS): Promise<void> {
    return new Promise((resolve, reject) => {
        let settled = false;
        let pendingLine = "";
        let unregisterStartupCancellation = () => {};
        const timer = setTimeout(() => {
            fail(new Error("The experimental scene did not present within 90 seconds. Check the engine log."));
            // GameAPI owns termination and retains its global slot until close.
        }, timeoutMs);
        timer.unref();

        const fail = (error: Error) => {
            if (settled) return;
            settled = true;
            clearTimeout(timer);
            unregisterStartupCancellation();
            reject(error);
        };

        child.once("error", fail);
        child.once("close", (code, signal) => {
            fail(new Error(`The native engine exited before the experimental scene presented (code ${code}, signal ${signal}).`));
        });
        unregisterStartupCancellation = registerNativeStartupCancellation(child, fail);
        if (settled) return;
        if (!child.stdout || !child.stderr) {
            fail(new Error("Could not read the experimental scene engine output."));
            child.kill("SIGTERM");
            return;
        }
        child.stdout.on("data", (chunk: Buffer) => {
            if (settled) return;
            // A readiness line may cross pipe chunks. Keep memory bounded even
            // for malformed output and wait for a complete line before accepting it.
            pendingLine += chunk.toString();
            const lines = pendingLine.split(/\r?\n/);
            pendingLine = lines.pop()!.slice(-16384);
            if (lines.some((line) => SCENE_PRESENTED.test(line))) {
                settled = true;
                clearTimeout(timer);
                unregisterStartupCancellation();
                onReady();
                resolve();
            }
        });
    });
}
