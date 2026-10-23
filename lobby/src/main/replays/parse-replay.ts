// SPDX-FileCopyrightText: 2025 The BAR Lobby Authors
//
// SPDX-License-Identifier: MIT

import type { Replay } from "@main/replays/replay";
import path from "path";
import { Worker } from "worker_threads";
import { logger } from "@main/utils/logger";
import {
    REPLAY_FILE_CHANGED,
    deserializeReplayError,
    replayFileGeneration,
    replayParseError,
    serializeReplayError,
} from "@main/replays/replay-parse-contract";
import type { ReplayParseRequest, ReplayParseResponse } from "@main/replays/replay-parse-contract";

const log = logger("parse-replay.ts");
let worker: Worker | undefined;
let nextJobId = 0;

interface ParseJob {
    owner: Worker;
    key: string;
    replayFilePath: string;
    promise: Promise<Replay>;
    resolve: (replay: Replay) => void;
    reject: (reason: Error) => void;
}

const jobs = new Map<number, ParseJob>();
const inFlight = new Map<string, ParseJob>();

function removeJob(id: number, job: ParseJob) {
    jobs.delete(id);
    if (inFlight.get(job.key) === job) inFlight.delete(job.key);
}

function failWorker(owner: Worker, error: Error) {
    // A delayed exit from an old worker must not reject its replacement's jobs.
    if (worker === owner) worker = undefined;
    for (const [id, job] of jobs) {
        if (job.owner !== owner) continue;
        removeJob(id, job);
        job.reject(error);
    }
}

function getWorker(): Worker {
    if (worker) return worker;
    const owner = new Worker(path.join(__dirname, "parse-replay-worker.cjs"));
    worker = owner;
    owner.on("message", (response: ReplayParseResponse) => {
        const job = jobs.get(response.id);
        if (!job || job.owner !== owner) return;
        removeJob(response.id, job);
        if (response.error) job.reject(deserializeReplayError(response.error));
        else job.resolve(response.replay);
    });
    owner.on("error", (cause: Error) => {
        const error = Object.assign(replayParseError("Replay worker failed", "REPLAY_WORKER_FAILED"), { cause });
        log.error({ err: error, category: "worker_failed" }, "Replay parser worker failed");
        failWorker(owner, error);
    });
    owner.on("exit", (exitCode: number) => {
        failWorker(owner, replayParseError(`Replay worker exited with code ${exitCode}`, "REPLAY_WORKER_EXITED"));
    });
    return owner;
}

function parseGeneration(replayFilePath: string, generation: string): Promise<Replay> {
    const key = JSON.stringify([replayFilePath, generation]);
    const existing = inFlight.get(key);
    if (existing) return existing.promise;

    const owner = getWorker();
    const id = ++nextJobId;
    let resolve!: (replay: Replay) => void;
    let reject!: (reason: Error) => void;
    const promise = new Promise<Replay>((success, failure) => {
        resolve = success;
        reject = failure;
    });
    const job: ParseJob = { owner, key, replayFilePath, promise, resolve, reject };
    jobs.set(id, job);
    inFlight.set(key, job);
    const request: ReplayParseRequest = { id, replayFilePath, generation };
    try {
        owner.postMessage(request);
    } catch (cause) {
        removeJob(id, job);
        reject(Object.assign(replayParseError("Could not send replay to worker", "REPLAY_WORKER_SEND_FAILED"), { cause }));
    }
    return promise;
}

export async function asyncParseReplay(replayFilePath: string): Promise<Replay> {
    const normalizedPath = path.resolve(replayFilePath);
    for (let attempt = 0; ; ++attempt) {
        const generation = await replayFileGeneration(normalizedPath);
        try {
            const replay = await parseGeneration(normalizedPath, generation);
            if ((await replayFileGeneration(normalizedPath)) !== generation) {
                throw replayParseError("Replay changed before its result was delivered", REPLAY_FILE_CHANGED);
            }
            return replay;
        } catch (error) {
            // Only a changed input gets one new attempt. Stable corrupt gzip,
            // invalid IDs and worker failures remain failures with their cause.
            if (attempt === 0 && serializeReplayError(error).code === REPLAY_FILE_CHANGED) continue;
            throw error;
        }
    }
}
