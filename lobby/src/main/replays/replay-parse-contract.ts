// SPDX-FileCopyrightText: 2025 The BAR Lobby Authors
//
// SPDX-License-Identifier: MIT

import { promises as fs } from "fs";
import type { Replay } from "@main/replays/replay";

export const REPLAY_FILE_CHANGED = "REPLAY_FILE_CHANGED";

export interface ReplayParseRequest {
    id: number;
    replayFilePath: string;
    generation: string;
}

export interface ReplayParseFailure {
    name: string;
    message: string;
    stack?: string;
    code?: string;
}

export type ReplayParseResponse =
    | { id: number; replay: Replay; error?: never }
    | { id: number; replay?: never; error: ReplayParseFailure };

// Nanosecond metadata and inode identity distinguish a rewrite or replacement
// from another request for the same, still unchanged replay.
export async function replayFileGeneration(replayFilePath: string): Promise<string> {
    const stat = await fs.stat(replayFilePath, { bigint: true });
    return [stat.dev, stat.ino, stat.size, stat.mtimeNs, stat.ctimeNs].join(":");
}

export function replayParseError(message: string, code: string): Error & { code: string } {
    return Object.assign(new Error(message), { code });
}

export function serializeReplayError(error: unknown): ReplayParseFailure {
    const value = error instanceof Error ? error : new Error(String(error));
    const code = "code" in value && typeof value.code === "string" ? value.code : undefined;
    return { name: value.name, message: value.message, stack: value.stack, code };
}

export function deserializeReplayError(error: ReplayParseFailure): Error {
    return Object.assign(new Error(error.message), error);
}

export function replayFailureCategory(error: unknown): string {
    const { code } = serializeReplayError(error);
    if (code === REPLAY_FILE_CHANGED) return "file_changed";
    if (code === "REPLAY_WORKER_FAILED" || code === "REPLAY_WORKER_EXITED") return "worker_failed";
    if (code === "REPLAY_WORKER_SEND_FAILED") return "worker_send_failed";
    if (code === "REPLAY_INVALID_GAME_ID") return "invalid_game_id";
    if (code?.startsWith("Z_")) return "compressed_data_error";
    if (code === "ENOENT") return "file_missing";
    if (code === "EACCES" || code === "EPERM") return "file_access";
    return "parse_failed";
}
