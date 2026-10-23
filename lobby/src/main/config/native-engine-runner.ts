// SPDX-License-Identifier: MIT
// Native Metal4 resolver. No OpenGL/Mesa/Zink/Vulkan/MoltenVK runner is imported.
import path from "node:path";
import { getNativeRuntimeForVersion } from "@main/game/packaged-runtime";
import { nativeProcessEnv } from "@main/game/native-runtime";

type Paths = { assetsPath: string; bundledAssetsPath?: string; engineVersion: string; platform?: NodeJS.Platform };
export { getBundledAssetsPath } from "./native-bundled-assets";
export function getEngineRootSearchPaths(options: Pick<Paths, "assetsPath" | "bundledAssetsPath">): string[] {
    return [path.join(options.assetsPath, "engine")];
}
export function resolveEngineVersionPath(options: Paths): string {
    if ((options.platform ?? process.platform) === "darwin") return path.dirname(getNativeRuntimeForVersion(options.engineVersion).binary);
    return path.join(options.assetsPath, "engine", options.engineVersion);
}
export function resolveNativeEngineRunner(options: Paths) {
    const platform = options.platform ?? process.platform;
    const enginePath = resolveEngineVersionPath(options);
    if (platform === "darwin") {
        const runtime = getNativeRuntimeForVersion(options.engineVersion);
        if (!runtime.checksumBinary) throw new Error("The native engine has no packaged checksum command.");
        return { enginePath, spawnCommand: runtime.checksumBinary, binaryFileName: path.basename(runtime.checksumBinary) };
    }
    return { enginePath, spawnCommand: platform === "win32" ? "spring.exe" : "./spring", binaryFileName: platform === "win32" ? "spring.exe" : "spring" };
}
export function resolvePrDownloaderPath(options: Paths): string {
    if ((options.platform ?? process.platform) === "darwin") {
        const binary = getNativeRuntimeForVersion(options.engineVersion).downloaderBinary;
        if (!binary) throw new Error("The selected native engine has no packaged downloader.");
        return binary;
    }
    return path.join(resolveEngineVersionPath(options), process.platform === "win32" ? "pr-downloader.exe" : "pr-downloader");
}
export function getNativeEngineProcessEnv(options: {
    assetsPath: string; enginePath: string; engineVersion: string; bundledAssetsPath?: string; writeDataPath: string;
    includeBundledAssetsInSpringDataDir?: boolean; includeBundledEngineSupportInSpringDataDir?: boolean;
    includeBundledMacOSLuaUIOverlayInSpringDataDir?: boolean;
}): NodeJS.ProcessEnv {
    const runtime = getNativeRuntimeForVersion(options.engineVersion);
    if (path.dirname(runtime.binary) !== options.enginePath) throw new Error("The checksum writer does not belong to the requested native engine.");
    return nativeProcessEnv(runtime, options.assetsPath);
}
