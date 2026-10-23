// SPDX-License-Identifier: MIT
import { spawn } from "node:child_process";
import { getAssetsPath } from "@main/config/app";
import { requireContentWritesOpen, waitForContentWriterClose } from "@main/content/content-write-coordinator";
import { nativeProcessEnv, type NativeRuntime } from "./native-runtime";
import path from "node:path";

// An essential launch identity check, not a visual or multiplayer acceptance probe.
export async function assertNativeSyncVersion(runtime: NativeRuntime): Promise<void> {
    const expected = runtime.manifest.engine.syncVersion;
    if (!runtime.manifest.engine.lane) return; // Preserve the explicit legacy47 checkpoint.
    if (!expected) throw new Error("The selected native engine has no recorded sync identity.");
    requireContentWritesOpen();
    const child = spawn(runtime.binary, ["--sync-version"], {
        cwd: path.dirname(runtime.binary), stdio: "pipe", detached: false,
        env: nativeProcessEnv(runtime, getAssetsPath()),
    });
    const closed = waitForContentWriterClose(child);
    let output = "";
    let overflow = false;
    child.stdout?.on("data", (chunk: Buffer) => {
        output += chunk.toString();
        if (output.length > 4096) { output = output.slice(0, 4096); overflow = true; }
    });
    child.stderr?.resume();
    const terminate = setTimeout(() => child.kill("SIGTERM"), 10_000);
    const force = setTimeout(() => child.kill("SIGKILL"), 12_000);
    try {
        const result = await closed;
        if (result.code !== 0 || result.signal || overflow || output.trim() !== expected) {
            throw new Error(`Native engine sync identity differs: expected '${expected}', received '${output.trim()}'.`);
        }
    } finally {
        clearTimeout(terminate);
        clearTimeout(force);
    }
}
