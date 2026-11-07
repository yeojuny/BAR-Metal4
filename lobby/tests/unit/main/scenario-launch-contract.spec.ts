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
vi.mock("@main/game/experimental-scene", async (importOriginal) => {
    const actual = await importOriginal<typeof import("@main/game/experimental-scene")>();
    return { ...actual, waitForExperimentalScene: vi.fn(async (_child, ready: () => void) => ready()) };
});

import { spawn } from "child_process";
import { GameAPI } from "@main/game/game";
import { contentAPI } from "@main/content/content-api";
import { applyDefaultSpringsettings } from "@main/game/springsettings";

const engineVersion = "native-installed-engine";
const gameVersion = "Beyond All Reason test-31691-a617033-local26";
const script = `[Game]\n{\n IsHost = 1;\n HostIP = 127.0.0.1;\n HostPort = 0;\n GameType = ${gameVersion};\n MapName = Pinewood_Derby_V1;\n}\n`;
let child: EventEmitter & { pid: number; stdout: PassThrough; stderr: PassThrough };

beforeEach(() => {
    vi.clearAllMocks();
    vi.stubGlobal("process", { ...process, platform: "darwin", arch: "arm64", env: { ...process.env, BAR_METAL4_EXPERIMENTAL_SINGLEPLAYER: "1" } });
    state.runtime = {
        binary: "/unused/native/spring",
        dataDirectory: "/unused/native/data",
        manifest: { engine: { id: engineVersion, capabilities: { gameplay: false, luaMenu: false }, experimentalOfflineScene: true } },
    } as NativeRuntime;
    child = Object.assign(new EventEmitter(), { pid: 123, stdout: new PassThrough(), stderr: new PassThrough() });
    vi.mocked(spawn).mockReturnValue(child as unknown as ReturnType<typeof spawn>);
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
            child.stdout.removeAllListeners();
            child.stderr.removeAllListeners();
        }
        expect(spawn).toHaveBeenCalledTimes(24);
    });
});
