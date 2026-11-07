// SPDX-FileCopyrightText: 2026 The BAR Lobby Authors
// SPDX-License-Identifier: MIT

import fs from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { afterEach, describe, expect, it, vi } from "vitest";
import { inheritChobbyDisplaySettings, prepareChobbyDisplaySettings } from "@main/game/chobby-display-settings";
import { mergeSpringDefaults } from "@main/game/springsettings";

vi.mock("@main/config/app", () => ({ WRITE_DATA_PATH: "/unused/bar-resolution-policy" }));
vi.mock("@main/services/config.service", () => ({ configService: { getConfig: () => ({ springSettings: {} }) } }));
vi.mock("@main/utils/logger", () => ({ logger: () => ({ info() {}, error() {} }) }));

describe("Chobby display intent", () => {
    const directories: string[] = [];
    afterEach(async () => {
        for (const dir of directories.splice(0)) await fs.rm(dir, { recursive: true, force: true });
    });

    it("only replaces the old display literals, preserving every unrelated byte", () => {
        const source = '-- retained comment\r\nreturn {["Chili lobby"]={\r\n password = "test fixture only", lobby_fullscreen = 1,\r\n game_fullscreen = 1, other = {lobby_fullscreen = 1},\r\n},["Other widget"]={lobby_fullscreen=1}}\r\n';
        expect(inheritChobbyDisplaySettings(source, true)).toBe(source.replace("lobby_fullscreen = 1,", "lobby_fullscreen = 3,").replace("game_fullscreen = 1,", "game_fullscreen = 3,"));
    });

    it.each([2, 3, 4, 5, 6])("preserves an explicit Chobby mode %i", (mode) => {
        const source = `return {["Chili lobby"]={lobby_fullscreen=${mode},game_fullscreen=${mode}}}`;
        expect(inheritChobbyDisplaySettings(source, true)).toBe(source);
    });

    it("uses the launcher mode for new settings without fixed screen dimensions", () => {
        expect(inheritChobbyDisplaySettings("return {}", true)).toContain("lobby_fullscreen = 3");
        expect(inheritChobbyDisplaySettings("return {}", false)).toContain("game_fullscreen = 2");
        expect(inheritChobbyDisplaySettings('return {["Chili lobby"]={lobby_fullscreen=1,game_fullscreen=1}}', false)).toBe('return {["Chili lobby"]={lobby_fullscreen=1,game_fullscreen=1}}');
    });

    it.each(['return {["Chili lobby"]={lobby_fullscreen=1,lobby_fullscreen=2}}', 'return {["Chili lobby"]={game_fullscreen=chooseMode()}}', 'return {["Chili lobby"]={["game_".."fullscreen"]=1}}', "execute(); return {}", "return {"])(
        "rejects ambiguous or executable layouts without evaluating them",
        (source) => {
            expect(() => inheritChobbyDisplaySettings(source, true)).toThrow(/original file preserved/);
        }
    );

    it("keeps a private rollback and does not override a later user window-mode selection", async () => {
        const root = await fs.mkdtemp(path.join(os.tmpdir(), "bar-chobby-display-"));
        directories.push(root);
        const config = path.join(root, "LuaMenu/Config/IGL_data.lua");
        await fs.mkdir(path.dirname(config), { recursive: true });
        const original = 'return {["Chili lobby"]={lobby_fullscreen=1,game_fullscreen=1,password="fixture"}}\n';
        await fs.writeFile(config, original);
        await prepareChobbyDisplaySettings(root, true);
        expect(await fs.readFile(`${config}.before-bar-display-v1`, "utf8")).toBe(original);
        expect((await fs.stat(`${config}.before-bar-display-v1`)).mode & 0o777).toBe(0o600);
        expect(await fs.readFile(config, "utf8")).toContain("game_fullscreen=3");
        const chosen = 'return {["Chili lobby"]={lobby_fullscreen=1,game_fullscreen=2,password="fixture"}}\n';
        await fs.writeFile(config, chosen);
        await prepareChobbyDisplaySettings(root, true);
        expect(await fs.readFile(config, "utf8")).toBe(chosen);
    });

    it("refuses a symlink instead of editing its target", async () => {
        const root = await fs.mkdtemp(path.join(os.tmpdir(), "bar-chobby-display-"));
        directories.push(root);
        const config = path.join(root, "LuaMenu/Config/IGL_data.lua");
        await fs.mkdir(path.dirname(config), { recursive: true });
        const target = path.join(root, "untouched.lua");
        await fs.writeFile(target, "return {}\n");
        await fs.symlink(target, config);
        await expect(prepareChobbyDisplaySettings(root, true)).rejects.toThrow(/Unsafe/);
        expect(await fs.readFile(target, "utf8")).toBe("return {}\n");
    });
});

describe("BAR startup defaults owner", () => {
    it("requests original Ultra application only for an empty native configuration", () => {
        const fresh = new Map<string, string>();
        mergeSpringDefaults(fresh, {}, "darwin");
        expect(fresh.get("NativeInitialGraphicsPreset")).toBe("ultra");
        fresh.delete("NativeInitialGraphicsPreset"); // In-game owner consumes it after applying Ultra.
        mergeSpringDefaults(fresh, {}, "darwin");
        expect(fresh.has("NativeInitialGraphicsPreset")).toBe(false);

        const custom = new Map([["Custom", "keep"]]);
        mergeSpringDefaults(custom, {}, "darwin");
        expect(custom.has("NativeInitialGraphicsPreset")).toBe(false);
        const nonNative = new Map<string, string>();
        mergeSpringDefaults(nonNative, {}, "linux");
        expect(nonNative.has("NativeInitialGraphicsPreset")).toBe(false);
    });

    it("initializes uncapped, MSAA-off Darwin defaults despite stale remote settings", () => {
        const settings = new Map<string, string>();
        const stale = { MSAA: 1, MSAALevel: 4, VSync: 1, VSyncGame: 1, VSyncGameFraction: 2, Fullscreen: 0 };
        expect(mergeSpringDefaults(settings, stale, "darwin")).toBe(true);
        expect(Object.fromEntries(settings)).toMatchObject({ MSAA: "0", MSAALevel: "-1", VSync: "0", VSyncGame: "0", VSyncGameFraction: "1", Fullscreen: "1" });
        expect(mergeSpringDefaults(settings, stale, "darwin")).toBe(false);
    });

    it("keeps explicit user MSAA and VSync choices even when they are heavier", () => {
        const chosen = { MSAA: "1", MSAALevel: "8", VSync: "1", VSyncGame: "1", VSyncGameFraction: "2", Fullscreen: "0" };
        const settings = new Map(Object.entries(chosen));
        mergeSpringDefaults(settings, {}, "darwin");
        expect(Object.fromEntries(settings)).toMatchObject(chosen);
    });

    it("preserves Darwin window mode and saved sizes across startup", () => {
        const settings = new Map([["Fullscreen", "0"], ["XResolution", "1440"], ["YResolution", "900"], ["Custom", "keep"]]);
        expect(mergeSpringDefaults(settings, { Fullscreen: 0 }, "darwin")).toBe(true);
        expect(settings.get("Fullscreen")).toBe("0");
        expect(settings.get("XResolution")).toBe("1440");
        expect(settings.get("YResolution")).toBe("900");
        expect(settings.get("Custom")).toBe("keep");
        expect(mergeSpringDefaults(settings, { Fullscreen: 0 }, "darwin")).toBe(false);
    });

    it("preserves an explicit non-Darwin window choice", () => {
        const settings = new Map([["Fullscreen", "0"]]);
        expect(mergeSpringDefaults(settings, { Fullscreen: 1 }, "linux")).toBe(false);
        expect(settings.get("Fullscreen")).toBe("0");
    });
});
