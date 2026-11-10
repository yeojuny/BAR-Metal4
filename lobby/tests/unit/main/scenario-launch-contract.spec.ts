// SPDX-FileCopyrightText: 2026 The BAR Lobby Authors
// SPDX-License-Identifier: MIT

import { EventEmitter } from "node:events";
import { PassThrough } from "node:stream";
import fs from "node:fs";
import path from "node:path";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { parseLuaTable } from "@main/utils/parse-lua-table";
import type { NativeRuntime } from "@main/game/native-runtime";

const state = vi.hoisted(() => ({ runtime: undefined as NativeRuntime | undefined }));
vi.mock("@main/config/app", () => ({
    WRITE_DATA_PATH: "/unused/bar-scenario-contract",
    REPLAYS_PATH: "/unused/replays",
    getEnginePath: () => "/unused/engine",
    getAssetsPath: () => "/unused/assets",
    SCENARIO_IMAGE_PATH: "/unused/images",
}));
vi.mock("@main/content/content-api", () => ({ contentAPI: { ensure: vi.fn().mockResolvedValue(undefined), markUsed: vi.fn().mockResolvedValue(undefined) } }));
vi.mock("@main/content/content-write-coordinator", () => ({ requireContentWritesOpen: vi.fn() }));
vi.mock("@main/utils/checksums", () => ({ acquireChecksumHold: vi.fn().mockResolvedValue(() => {}) }));
vi.mock("@main/game/springsettings", () => ({ applyDefaultSpringsettings: vi.fn() }));
vi.mock("@main/game/packaged-runtime", () => ({ getPackagedNativeRuntime: () => state.runtime }));
vi.mock("@main/game/native-runtime", () => ({ requireNativeVersion: vi.fn(), nativeProcessEnv: () => ({}) }));
vi.mock("@main/game/native-version", () => ({ assertNativeSyncVersion: vi.fn().mockResolvedValue(undefined) }));
vi.mock("@main/game/native-launch-display", () => ({ getNativeLaunchDisplayEnv: () => ({}) }));
vi.mock("child_process", () => ({ spawn: vi.fn() }));

import { spawn } from "child_process";
import { GameAPI } from "@main/game/game";
import { contentAPI } from "@main/content/content-api";
import { applyDefaultSpringsettings } from "@main/game/springsettings";

const engineVersion = "native-installed-engine";
const gameVersion = "Beyond All Reason test-31691-a617033-local26";
const script = `[Game]\n{\n IsHost = 1;\n HostIP = 127.0.0.1;\n HostPort = 0;\n GameType = ${gameVersion};\n MapName = Pinewood_Derby_V1;\n}\n`;
const foreground = "[NativeWindowPresentation] foreground=1";
const frame = "[Metal4Scene] First presented simulation frame 0: map\n";
const createChild = () => Object.assign(new EventEmitter(), { pid: 123, stdout: new PassThrough(), stderr: new PassThrough() });
let child: ReturnType<typeof createChild>;

beforeEach(() => {
    vi.clearAllMocks();
    vi.stubGlobal("process", { ...process, platform: "darwin", arch: "arm64", env: { ...process.env, BAR_METAL4_EXPERIMENTAL_SINGLEPLAYER: "1" } });
    state.runtime = {
        binary: "/unused/native/spring",
        dataDirectory: "/unused/native/data",
        manifest: { engine: { id: engineVersion, capabilities: { gameplay: false, luaMenu: false }, experimentalOfflineScene: true } },
    } as NativeRuntime;
    child = createChild();
    vi.mocked(spawn).mockImplementation(() => {
        queueMicrotask(() => child.stdout.write(frame));
        return child as unknown as ReturnType<typeof spawn>;
    });
    vi.spyOn(fs.promises, "writeFile").mockResolvedValue(undefined);
    vi.spyOn(fs.promises, "mkdir").mockResolvedValue(undefined);
});
afterEach(() => {
    child.emit("close", 0);
    vi.restoreAllMocks();
    vi.unstubAllGlobals();
});

function settings(value = script) {
    return { engineVersion, gameVersion, script: value };
}

describe("explicit offline scenario launch contract", () => {
    it("dispatches foreground only once before first-frame readiness and restores on close", async () => {
        const api = new GameAPI();
        const events: string[] = [];
        api.onNativeGameForeground.add(() => events.push("foreground"));
        api.onGameLaunched.add(() => events.push("ready"));
        api.onGameClosed.add(() => events.push("closed"));
        vi.mocked(spawn).mockReturnValue(child as unknown as ReturnType<typeof spawn>);

        const launch = api.launchScenario(settings());
        await vi.waitFor(() => expect(spawn).toHaveBeenCalledOnce());
        child.stderr.write(foreground + "\n");
        child.stdout.write(`log: ${foreground}\n[NativeWindowPresentation] foreground=10\n`);
        child.stdout.write("x".repeat(20000) + foreground);
        child.stdout.write("\n[NativeWindowPresentation] fore");
        expect(events).toEqual([]);
        child.stdout.write("ground=1\r\n" + foreground + "\n");
        expect(events).toEqual(["foreground"]);
        child.stdout.write(frame);
        await launch;
        child.emit("close", 0);
        expect(events).toEqual(["foreground", "ready", "closed"]);
    });

    it("accepts foreground after the first frame without using the frame as a foreground signal", async () => {
        const api = new GameAPI();
        const onForeground = vi.fn();
        api.onNativeGameForeground.add(onForeground);
        await api.launchScenario(settings());
        expect(onForeground).not.toHaveBeenCalled();
        child.stdout.write(foreground + "\n");
        expect(onForeground).toHaveBeenCalledOnce();
    });

    it.each([false, true])("closes and rejects startup when the engine exits early (foreground=%s)", async (enteredForeground) => {
        const api = new GameAPI();
        const onClosed = vi.fn();
        const onReady = vi.fn();
        const onForeground = vi.fn();
        api.onGameClosed.add(onClosed);
        api.onGameLaunched.add(onReady);
        api.onNativeGameForeground.add(onForeground);
        vi.mocked(spawn).mockReturnValue(child as unknown as ReturnType<typeof spawn>);

        const rejected = expect(api.launchScenario(settings())).rejects.toThrow("code 21");
        await vi.waitFor(() => expect(spawn).toHaveBeenCalledOnce());
        if (enteredForeground) child.stdout.write(foreground + "\n");
        child.emit("close", 21, null);
        await rejected;
        child.stdout.write(foreground + "\n" + frame);
        expect(onReady).not.toHaveBeenCalled();
        expect(onForeground).toHaveBeenCalledTimes(Number(enteredForeground));
        expect(onClosed).toHaveBeenCalledExactlyOnceWith(21);
        expect(api.isExperimentalSceneRunning()).toBe(false);
    });

    it("cancels launch and ignores pending foreground output during shutdown", async () => {
        const api = new GameAPI();
        const onForeground = vi.fn();
        const onClosed = vi.fn();
        api.onNativeGameForeground.add(onForeground);
        api.onGameClosed.add(onClosed);
        const kill = vi.fn();
        Object.assign(child, { kill });
        vi.mocked(spawn).mockReturnValue(child as unknown as ReturnType<typeof spawn>);

        const rejected = expect(api.launchScenario(settings())).rejects.toThrow("startup was cancelled");
        await vi.waitFor(() => expect(spawn).toHaveBeenCalledOnce());
        const stopping = api.stopExperimentalScene();
        child.stdout.write(foreground + "\n" + frame);
        expect(onForeground).not.toHaveBeenCalled();
        expect(kill).toHaveBeenCalledWith("SIGTERM");
        expect(api.isExperimentalSceneRunning()).toBe(true);
        child.emit("close", null, "SIGTERM");
        await Promise.all([stopping, rejected]);
        expect(onClosed).toHaveBeenCalledOnce();
        expect(api.isExperimentalSceneRunning()).toBe(false);
    });

    it("uses the existing opt-in offline path without claiming gameplay readiness", async () => {
        const api = new GameAPI();
        await api.launchScenario(settings());
        expect(spawn).toHaveBeenCalledWith(
            "/unused/native/spring",
            ["--write-dir", "/unused/bar-scenario-contract", "--isolation", "--only-local", "/unused/bar-scenario-contract/script.txt"],
            expect.objectContaining({ detached: false })
        );
        expect(fs.promises.writeFile).toHaveBeenCalledWith("/unused/bar-scenario-contract/script.txt", script);
        expect(contentAPI.ensure).toHaveBeenCalledWith([
            { type: "engine", id: engineVersion },
            { type: "game", id: gameVersion },
        ]);
        expect(applyDefaultSpringsettings).toHaveBeenCalledOnce();
        expect(state.runtime!.manifest.engine.capabilities.gameplay).toBe(false);
    });

    it("leaves generic scripts and multiplayer behind the original readiness gate", async () => {
        const api = new GameAPI();
        await expect(api.launchScript(settings(script.replace("MapName", "mapname").replace("GameType", "gametype")))).rejects.toThrow("not ready for gameplay");
        await expect(api.launchMultiplayer({ engineVersion, gameVersion, springString: "spring://remote.invalid" })).rejects.toThrow("not ready for gameplay");
        expect(spawn).not.toHaveBeenCalled();
        expect(fs.promises.writeFile).not.toHaveBeenCalled();
    });

    it("still requires the explicit experimental singleplayer opt-in", async () => {
        delete process.env.BAR_METAL4_EXPERIMENTAL_SINGLEPLAYER;
        await expect(new GameAPI().launchScenario(settings())).rejects.toThrow("not ready for gameplay");
        expect(spawn).not.toHaveBeenCalled();
        expect(fs.promises.writeFile).not.toHaveBeenCalled();
    });

    it("still requires a manifest containing the experimental offline scene", async () => {
        state.runtime!.manifest.engine.experimentalOfflineScene = false;
        await expect(new GameAPI().launchScenario(settings())).rejects.toThrow("does not contain the experimental");
        expect(spawn).not.toHaveBeenCalled();
    });

    it.each([
        ["client script", script.replace("IsHost = 1", "IsHost = 0"), "local host"],
        ["external host", script.replace("127.0.0.1", "203.0.113.2"), "local host"],
        ["external autohost", script.replace("HostPort = 0;", "HostPort = 0;\n AutohostIP = 203.0.113.2;"), "external autohost"],
        ["different game", script.replace(gameVersion, "different-installed-game"), "selected installed version"],
    ])("rejects %s before any native handoff", async (_name, value, error) => {
        await expect(new GameAPI().launchScenario(settings(value))).rejects.toThrow(error);
        expect(spawn).not.toHaveBeenCalled();
        expect(contentAPI.ensure).not.toHaveBeenCalled();
        expect(fs.promises.writeFile).not.toHaveBeenCalled();
    });

    const referenceRoot = process.env.BAR_SCENARIO_REFERENCE_ROOT;
    it.skipIf(!referenceRoot)("accepts local-host contracts of all 24 actual pinned scenarios", async () => {
        const root = path.join(referenceRoot!, "singleplayer/scenarios");
        const files = fs.readdirSync(root).filter((name) => /^scenario.*\.lua$/.test(name));
        expect(files).toHaveLength(24);
        for (const file of files) {
            child = createChild();
            const scenario = parseLuaTable(fs.readFileSync(path.join(root, file)));
            const scenarioScript = scenario.startscript
                .slice(1, -1)
                .replaceAll("__BARVERSION__", gameVersion)
                .replaceAll("__MAPNAME__", scenario.mapfilename)
                .replaceAll("__PLAYERNAME__", "Player")
                .replaceAll("__PLAYERSIDE__", "Armada")
                .replaceAll("__SCENARIOOPTIONS__", "e30=")
                .replaceAll("__RESTRICTEDUNITS__", "")
                .replace(/__[A-Z]+__/g, "0");
            await new GameAPI().launchScenario(settings(scenarioScript)).catch((error) => {
                throw new Error(`${file}: ${String(error)}`);
            });
            child.emit("close", 0);
        }
        expect(spawn).toHaveBeenCalledTimes(24);
    });
});
