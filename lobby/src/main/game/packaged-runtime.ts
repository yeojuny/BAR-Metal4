// SPDX-FileCopyrightText: 2026 The BAR Lobby Authors
//
// SPDX-License-Identifier: MIT

import fs from "node:fs";
import path from "node:path";
import { loadNativeRuntime, nativeManifestPath, type NativeRuntimeLane } from "./native-runtime";

export function getPackagedNativeRuntime(lane: NativeRuntimeLane = "development") {
    if (process.platform !== "darwin" || process.arch !== "arm64") throw new Error("This native engine package requires Apple Silicon macOS.");
    const lanesRoot = path.join(process.resourcesPath, "native-engines");
    if (fs.existsSync(lanesRoot)) {
        if (process.env.BAR_NATIVE_ENGINE_MANIFEST) throw new Error("A single-engine override cannot replace a packaged engine lane.");
        const runtime = loadNativeRuntime(path.join(lanesRoot, lane, "manifest.json"));
        if (runtime.manifest.engine.lane !== lane) throw new Error(`The packaged ${lane} engine has the wrong lane identity.`);
        return runtime;
    }
    if (lane === "official-multiplayer") throw new Error("The official multiplayer engine has not been packaged yet.");
    // Preserve the fixed47 single-engine checkpoint only when no new layout exists.
    return loadNativeRuntime(nativeManifestPath(process.resourcesPath, process.env.BAR_NATIVE_ENGINE_MANIFEST));
}

export function getNativeRuntimeForVersion(version: string) {
    // Select the requested role before loading its binary; another role never substitutes for it.
    const lane = version === "2026.07.04" ? "official-multiplayer" : "development";
    const runtime = getPackagedNativeRuntime(lane);
    if ((runtime.manifest.engine.syncVersion ?? runtime.manifest.engine.id) !== version) throw new Error(`No native Metal4 engine is packaged for exact sync identity ${version}.`);
    return runtime;
}
