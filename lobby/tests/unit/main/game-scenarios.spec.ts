// SPDX-FileCopyrightText: 2026 The BAR Lobby Authors
// SPDX-License-Identifier: MIT

import fs from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { afterAll, beforeAll, describe, expect, it, vi } from "vitest";

const cache = vi.hoisted(() => ({ root: "" }));
vi.mock("@main/config/app", () => ({
    get SCENARIO_IMAGE_PATH() {
        return cache.root;
    },
}));
vi.mock("@main/content/game/game-files", () => ({ getGameFiles: vi.fn() }));

import { getScenarios } from "@main/content/game/game-scenarios";
import { getGameFiles } from "@main/content/game/game-files";

beforeAll(async () => {
    cache.root = await fs.mkdtemp(path.join(os.tmpdir(), "bar-scenario-tests-"));
});
afterAll(async () => {
    await fs.rm(cache.root, { force: true, recursive: true });
});

function definition(fileName: string, data: string | Buffer) {
    return { fileName, data: Buffer.from(data), archivePath: "unused", md5: "", crc32: "", filesizeBytes: 0 };
}

describe("scenario definitions", () => {
    it("keeps complete startscript syntax and uses real fallback factions", async () => {
        vi.mocked(getGameFiles)
            .mockResolvedValueOnce([])
            .mockResolvedValueOnce([
                definition(
                    "singleplayer/scenarios/scenario001.lua",
                    `
            local scenariodata = {
                index = 1, title = "Mission", summary = [[Summary]], briefing = [[Briefing]],
                allowedsides = {},
                startscript = [[[Game]\n{\nGameType=__BARVERSION__;\n}\n]],
            }
            return scenariodata
        `
                ),
            ]);
        const [scenario] = await getScenarios("fixture");
        expect(scenario.startscript).toBe("[Game]\n{\nGameType=__BARVERSION__;\n}\n");
        expect(scenario.allowedsides).toEqual(["Armada", "Cortex", "Random"]);
        expect(scenario.summary).toBe("Summary");
    });

    const referenceRoot = process.env.BAR_SCENARIO_REFERENCE_ROOT;
    it.skipIf(!referenceRoot)("parses all 24 scenario definitions from the pinned current game", async () => {
        const directory = path.join(referenceRoot!, "singleplayer/scenarios");
        const files = (await fs.readdir(directory)).filter((name) => /^scenario.*\.lua$/.test(name));
        expect(files).toHaveLength(24);
        const definitions = await Promise.all(files.map(async (name) => definition(`singleplayer/scenarios/${name}`, await fs.readFile(path.join(directory, name)))));
        vi.mocked(getGameFiles).mockResolvedValueOnce([]).mockResolvedValueOnce(definitions);
        const scenarios = await getScenarios("current217");
        expect(scenarios).toHaveLength(24);
        for (const scenario of scenarios) {
            expect(scenario.startscript.trim()).toMatch(/^\[Game\]\s*\{/i);
            expect(scenario.startscript.trim()).toMatch(/\}$/);
            expect(scenario.difficulties.some((difficulty) => difficulty.name === scenario.defaultdifficulty)).toBe(true);
            expect(scenario.allowedsides.length).toBeGreaterThan(0);
        }
    });
});
