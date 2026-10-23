import { requireContentWritesOpen } from "@main/content/content-write-coordinator";
import { registerNativeStartupCancellation, stopExperimentalSceneProcess } from "./experimental-scene";
// SPDX-FileCopyrightText: 2026 The BAR Lobby Authors
//
// SPDX-License-Identifier: MIT

import { spawn, type ChildProcess } from "node:child_process";
import { Signal } from "$/jaz-ts-utils/signal";
import type { ChobbySessionState } from "@shared/chobby-state";
import type { luaMenuLaunchPlan } from "./native-runtime";

export const LUA_MENU_READY = /LuaMenu v1\.0 \(Lua 5\.1[^)]*\)/;
// Pinned BYAR-Chobby LuaMenu/main.lua:73 logs this only after its includes and
// handler loading. This is source-entry readiness, not visual/UI acceptance.
const LUA_MENU_FAILURE = /\[LuaMenu(?:::RunCallInTraceback)?\][^\n]*(?:Error|error)|Failed to load LuaMenu|could not find any archives|failed loading archive/;

export class LuaMenuProcess {
    public readonly onState = new Signal<ChobbySessionState>();
    public state: ChobbySessionState = { platform: process.platform === "darwin" ? "darwin" : "other", phase: "closed" };
    private child: ChildProcess | null = null;

    public constructor(
        private readonly spawnProcess: typeof spawn = spawn,
        private readonly timeoutMs = 30000,
        private readonly logOutput: (text: string) => void = () => {}
    ) {}

    public failed(error: unknown) {
        this.setState("failed", error instanceof Error ? error.message : String(error));
    }

    private setState(phase: ChobbySessionState["phase"], error?: string) {
        this.state = { platform: this.state.platform, phase, ...(error ? { error } : {}) };
        this.onState.dispatch(this.state);
    }

    public launch(plan: ReturnType<typeof luaMenuLaunchPlan>, releaseSlot: () => void): Promise<void> {
        if (this.child) {
            releaseSlot();
            return Promise.reject(new Error("The previous Chobby process is still closing."));
        }
        this.setState("starting");
        return new Promise<void>((resolve, reject) => {
            let settled = false;
            let started = false;
            let buffer = "";
            let timer: NodeJS.Timeout | undefined;
            let child: ChildProcess | undefined;
            let unregisterStartupCancellation = () => {};
            const fail = (error: Error, stop = false) => {
                if (settled && !started) return;
                started = false;
                const wasPending = !settled;
                settled = true;
                clearTimeout(timer);
                unregisterStartupCancellation();
                if (wasPending) reject(error);
                this.failed(error);
                if (stop && child) void stopExperimentalSceneProcess(child, 2000).catch((failure) => this.logOutput(String(failure)));
            };
            try {
                requireContentWritesOpen();
                child = this.spawnProcess(plan.binary, plan.args, {
                    cwd: plan.cwd,
                    stdio: "pipe",
                    detached: false,
                    env: plan.env,
                });
                this.child = child;
                const spawnedChild = child;
                spawnedChild.once("error", (error) => {
                    fail(error, !!spawnedChild.pid);
                });
                spawnedChild.once("close", (code, signal) => {
                    clearTimeout(timer);
                    unregisterStartupCancellation();
                    if (this.child === spawnedChild) this.child = null;
                    releaseSlot();
                    if (!settled) {
                        fail(new Error(`The native engine exited before Chobby loaded (code ${code}, signal ${signal}).`));
                    } else if (started && code === 0) {
                        this.setState("closed");
                    } else if (started) {
                        fail(new Error(`Chobby engine exited with code ${code}, signal ${signal}.`));
                    }
                });
                unregisterStartupCancellation = registerNativeStartupCancellation(spawnedChild, (error) => fail(error));
                if (settled) return;
                if (!child.stdout || !child.stderr) throw new Error("Could not read the Chobby engine output.");
                const output = (chunk: Buffer) => {
                    const text = chunk.toString();
                    this.logOutput(text);
                    buffer = (buffer + text).slice(-16384);
                    if (LUA_MENU_FAILURE.test(buffer)) {
                        fail(new Error("The native engine failed to load Chobby. Check the engine log."), true);
                    } else if (!settled && LUA_MENU_READY.test(buffer)) {
                        clearTimeout(timer);
                        started = true;
                        settled = true;
                        unregisterStartupCancellation();
                        this.setState("running");
                        resolve();
                    }
                };
                child.stdout.on("data", output);
                child.stderr.on("data", output);
                timer = setTimeout(() => fail(new Error("Chobby did not reach its menu entry point within 30 seconds."), true), this.timeoutMs);
                timer.unref();
            } catch (error) {
                if (child) child.kill("SIGTERM");
                else releaseSlot();
                fail(error instanceof Error ? error : new Error(String(error)));
            }
        });
    }

    public async stop(): Promise<void> {
        if (!this.child) return;
        try {
            await stopExperimentalSceneProcess(this.child, 2000);
        } catch (error) {
            // Restore LobbyHandoff visibility/audio, retaining child and its slot until close.
            this.failed(error);
            throw error;
        }
    }
}
