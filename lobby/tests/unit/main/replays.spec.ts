// SPDX-FileCopyrightText: 2026 The BAR Lobby Authors
//
// SPDX-License-Identifier: MIT

import type { ReplayParseRequest } from "@main/replays/replay-parse-contract";
import { Replay } from "@main/replays/replay";
import { beforeEach, describe, expect, it, vi } from "vitest";

const { ensure, parsed, copied } = vi.hoisted(() => ({
    ensure: vi.fn(),
    parsed: { replay: null as Replay | null },
    copied: [] as string[],
}));

const replayWorker = vi.hoisted(() => ({
    on: vi.fn(),
    postMessage: vi.fn(),
    waitForPackets: vi.fn(),
    stat: vi.fn(),
}));

vi.mock("worker_threads", () => ({
    isMainThread: false,
    parentPort: { on: replayWorker.on, postMessage: replayWorker.postMessage },
}));

vi.mock("sdfz-demo-parser", () => ({
    // Model the installed parser's mutable header and constructor-owned chat
    // across its asynchronous packet parsing boundary.
    DemoParser: class {
        private currentReplay = "";
        private chatlog: { message: string }[] = [];

        async parseDemo(filePath: string) {
            this.currentReplay = filePath;
            this.chatlog.push({ message: filePath });
            await replayWorker.waitForPackets(filePath);

            return {
                header: { gameId: this.currentReplay },
                info: {
                    meta: { engine: "test-engine", game: "test-game", map: this.currentReplay, startTime: new Date(0), durationMs: 1000, winningAllyTeamIds: [] },
                    players: [{ playerId: 0, allyTeamId: 0 }],
                    ais: [],
                    allyTeams: [{ allyTeamId: 0 }, { allyTeamId: 1 }],
                    spectators: [],
                    hostSettings: {},
                    gameSettings: {},
                    mapSettings: {},
                },
                chatlog: this.chatlog,
                script: this.currentReplay,
            };
        }
    },
}));

vi.mock("@main/utils/logger", () => ({ logger: () => ({ debug: vi.fn(), info: vi.fn(), warn: vi.fn(), error: vi.fn() }) }));
vi.mock("@main/config/app", () => ({ REPLAYS_PATH: "/replays" }));
vi.mock("@main/content/content-api", () => ({ contentAPI: { ensure } }));
vi.mock("@main/replays/parse-replay", () => ({ asyncParseReplay: async () => parsed.replay }));
vi.mock("@main/game/game", () => ({ gameAPI: { isGameRunning: () => false } }));
vi.mock("chokidar", () => ({ default: { watch: () => ({ on: () => ({ on: () => ({}) }) }) } }));
vi.mock("fs", () => ({
    promises: { stat: replayWorker.stat },
    default: {
        promises: {
            copyFile: async (from: string) => void copied.push(from),
            mkdir: vi.fn(),
            readdir: async () => [],
            rm: vi.fn(),
        },
    },
}));

import { ReplaysAPI } from "@main/replays/replays";

function replay(mapSpringName: string): Replay {
    return { fileName: "2026-08-01_bar.sdfz", mapSpringName } as Replay;
}

describe("ReplaysAPI.copyParseReplay", () => {
    let replays: ReplaysAPI;
    let cached: Replay[];

    beforeEach(() => {
        ensure.mockReset().mockResolvedValue(undefined);
        copied.length = 0;
        parsed.replay = replay("Quicksilver 1.2");

        replays = new ReplaysAPI();
        cached = [];
        replays.onReplayCached.add((r) => cached.push(r));
    });

    it("copies the replay in and reports it", async () => {
        await replays.copyParseReplay("/downloads/2026-08-01_bar.sdfz");

        expect(copied).toEqual(["/downloads/2026-08-01_bar.sdfz"]);
        expect(cached).toHaveLength(1);
        expect(ensure).toHaveBeenCalledWith([{ type: "map", id: "Quicksilver 1.2" }]);
    });

    // onReplayCached is what puts the replay in front of the user, and the map is a convenience on top of
    // it. A refused map download must not swallow the replay.
    it("reports the replay even when its map cannot be fetched", async () => {
        ensure.mockRejectedValue(new Error("Not enough free space in /assets"));

        await expect(replays.copyParseReplay("/downloads/2026-08-01_bar.sdfz")).resolves.toBeUndefined();

        expect(cached).toHaveLength(1);
    });

    it("reports the replay before waiting on the map", async () => {
        const order: string[] = [];
        replays.onReplayCached.add(() => order.push("cached"));
        ensure.mockImplementation(async () => void order.push("map fetched"));

        await replays.copyParseReplay("/downloads/2026-08-01_bar.sdfz");

        expect(order).toEqual(["cached", "map fetched"]);
    });
});

describe("replay worker parser isolation", () => {
    beforeEach(() => {
        vi.resetModules();
        replayWorker.on.mockClear();
        replayWorker.postMessage.mockClear();
        replayWorker.waitForPackets.mockReset().mockResolvedValue(undefined);
        replayWorker.stat.mockReset().mockResolvedValue({ dev: 1n, ino: 2n, size: 3n, mtimeNs: 4n, ctimeNs: 5n });
    });

    async function messageHandler() {
        await import("@main/replays/parse-replay-worker");
        const registration = replayWorker.on.mock.calls.find(([event]) => event === "message");
        if (!registration) throw new Error("Replay worker message handler was not registered");
        return registration[1] as (request: ReplayParseRequest) => Promise<void>;
    }

    it("keeps earlier replay chat out of a later result", async () => {
        const handle = await messageHandler();
        await handle({ id: 1, replayFilePath: "/replays/first.barreplay", generation: "1:2:3:4:5" });
        await handle({ id: 2, replayFilePath: "/replays/second.barreplay", generation: "1:2:3:4:5" });

        const second = replayWorker.postMessage.mock.calls[1][0];
        expect(second.id).toBe(2);
        expect(second.replay.gameId).toBe("/replays/second.barreplay");
        expect(second.replay.chatlog).toEqual([{ message: "/replays/second.barreplay" }]);
    });

    it("keeps each request's metadata when packet parsing finishes out of order", async () => {
        const release = new Map<string, () => void>();
        replayWorker.waitForPackets.mockImplementation((filePath: string) => new Promise<void>((resolve) => release.set(filePath, resolve)));
        const handle = await messageHandler();
        const first = handle({ id: 1, replayFilePath: "/replays/first.barreplay", generation: "1:2:3:4:5" });
        const second = handle({ id: 2, replayFilePath: "/replays/second.barreplay", generation: "1:2:3:4:5" });
        await vi.waitFor(() => expect(release.size).toBe(2));

        release.get("/replays/second.barreplay")!();
        await second;
        release.get("/replays/first.barreplay")!();
        await first;

        const replies = replayWorker.postMessage.mock.calls.map(([reply]) => ({
            requested: reply.id,
            parsed: reply.replay.gameId,
            chat: reply.replay.chatlog,
        }));
        expect(replies).toEqual([
            { requested: 2, parsed: "/replays/second.barreplay", chat: [{ message: "/replays/second.barreplay" }] },
            { requested: 1, parsed: "/replays/first.barreplay", chat: [{ message: "/replays/first.barreplay" }] },
        ]);
    });
    it("rejects a stale request before parsing", async () => {
        const handle = await messageHandler();
        await handle({ id: 3, replayFilePath: "/replays/replaced.barreplay", generation: "old-generation" });
        expect(replayWorker.waitForPackets).not.toHaveBeenCalled();
        expect(replayWorker.postMessage).toHaveBeenCalledWith(expect.objectContaining({ id: 3, error: expect.objectContaining({ code: "REPLAY_FILE_CHANGED" }) }));
    });

    it("rejects a replay replaced while parsing instead of returning stale data", async () => {
        replayWorker.waitForPackets.mockImplementation(async () => {
            replayWorker.stat.mockResolvedValue({ dev: 1n, ino: 99n, size: 3n, mtimeNs: 4n, ctimeNs: 5n });
        });
        const handle = await messageHandler();
        await handle({ id: 4, replayFilePath: "/replays/replaced.barreplay", generation: "1:2:3:4:5" });
        expect(replayWorker.postMessage).toHaveBeenCalledWith(expect.objectContaining({ id: 4, error: expect.objectContaining({ code: "REPLAY_FILE_CHANGED" }) }));
    });

});
