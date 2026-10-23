// SPDX-FileCopyrightText: 2025 The BAR Lobby Authors
//
// SPDX-License-Identifier: MIT

import { DemoParser } from "sdfz-demo-parser";
import { Replay } from "@main/replays/replay";
import path from "path";
import { isMainThread, parentPort } from "worker_threads";
import { REPLAY_FILE_CHANGED, replayFileGeneration, replayParseError, serializeReplayError } from "@main/replays/replay-parse-contract";
import type { ReplayParseRequest, ReplayParseResponse } from "@main/replays/replay-parse-contract";

if (isMainThread) {
    throw new Error("This script should be run in worker thread.");
} else {
    async function parseReplay(replayPath: string): Promise<Replay> {
        const demoParser = new DemoParser();
        const replayData = await demoParser.parseDemo(replayPath);
        const numOfPlayers = replayData.info.players.length + replayData.info.ais.length;
        let preset: "duel" | "team" | "ffa" | "teamffa" = "duel";
        if (replayData.info.allyTeams.length > 2 && replayData.info.players.some((player) => player.playerId !== player.allyTeamId)) {
            preset = "teamffa";
        } else if (replayData.info.allyTeams.length > 2) {
            preset = "ffa";
        } else if (numOfPlayers > 2) {
            preset = "team";
        }
        return {
            gameId: replayData.header.gameId,
            fileName: path.parse(replayPath).base,
            filePath: replayPath,
            engineVersion: replayData.info.meta.engine,
            gameVersion: replayData.info.meta.game,
            mapSpringName: replayData.info.meta.map,
            startTime: replayData.info.meta.startTime,
            gameDurationMs: replayData.info.meta.durationMs,
            gameEndedNormally: replayData.info.meta.winningAllyTeamIds.length > 0 ? 1 : 0,
            chatlog: replayData.chatlog || null,
            hasBots: replayData.info.ais.length > 0 ? 1 : 0,
            preset: preset,
            winningTeamId: replayData.info.meta.winningAllyTeamIds[0],
            teams: replayData.info.allyTeams,
            contenders: [...replayData.info.players, ...replayData.info.ais],
            spectators: replayData.info.spectators,
            script: replayData.script,
            battleSettings: replayData.info.hostSettings,
            gameSettings: replayData.info.gameSettings,
            mapSettings: replayData.info.mapSettings,
            hostSettings: replayData.info.spadsSettings ?? {},
        };
    }
    // listen to messages from the main thread

    if (!parentPort) throw new Error("Parent Port is not defined");

    const narrowedParentPort = parentPort;

    narrowedParentPort.on("message", async ({ id, replayFilePath, generation }: ReplayParseRequest) => {
        const send = (response: ReplayParseResponse) => narrowedParentPort.postMessage(response);
        try {
            if ((await replayFileGeneration(replayFilePath)) !== generation) {
                throw replayParseError("Replay changed before parsing began", REPLAY_FILE_CHANGED);
            }
            let replay: Replay;
            try {
                replay = await parseReplay(replayFilePath);
            } catch (error) {
                // An unfinished write may present as a gzip/parser failure. Only
                // actual metadata change allows the parent to retry it once.
                if ((await replayFileGeneration(replayFilePath)) !== generation) {
                    throw replayParseError("Replay changed while parsing", REPLAY_FILE_CHANGED);
                }
                throw error;
            }
            if ((await replayFileGeneration(replayFilePath)) !== generation) {
                throw replayParseError("Replay changed while parsing", REPLAY_FILE_CHANGED);
            }
            if (replay.gameId === "00000000000000000000000000000000") {
                throw replayParseError(`Invalid gameId for replay: ${replayFilePath}`, "REPLAY_INVALID_GAME_ID");
            }
            send({ id, replay });
        } catch (error) {
            // Error structured cloning does not preserve custom code fields.
            send({ id, error: serializeReplayError(error) });
        }
    });
}
