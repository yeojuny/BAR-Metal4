// SPDX-FileCopyrightText: 2026 The BAR Lobby Authors
//
// SPDX-License-Identifier: MIT

import fs from "node:fs";
import path from "node:path";
import { createHash } from "node:crypto";
import { parseLuaTable } from "@main/utils/parse-lua-table";

export type NativeRuntimeLane = "development" | "official-multiplayer";

export interface NativeRuntimeManifest {
    schemaVersion: 1;
    platform: "darwin";
    architecture: "arm64";
    renderer: "metal4";
    engine: {
        id: string;
        binary: string;
        sha256: string;
        sourceCommit: string;
        // Internal package role is separate from the real engine/network version.
        lane?: NativeRuntimeLane;
        upstreamTag?: string | null;
        executableVersion?: string;
        syncVersion?: string;
        nativePortPatchManifestSHA256?: string;
        capabilities: { luaMenu: boolean; gameplay: boolean };
        // Identifies the opt-in experimental world target; this is not gameplay acceptance.
        experimentalOfflineScene?: boolean;
        checksumBinary?: string;
        downloaderBinary?: string;
    };
    dataDirectory: string;
    chobby: { directory?: string; archiveName: string; sourceCommit?: string; format?: "rapid"; packagePath?: string; sdpSHA256?: string; modinfoMD5?: string };
    officialContent?: { gameArchiveName: string; gamePackagePath: string; gameSDPSHA256: string };
}

export interface NativeRuntime {
    manifest: NativeRuntimeManifest;
    root: string;
    binary: string;
    dataDirectory: string;
    checksumBinary?: string;
    downloaderBinary?: string;
}

export function nativeManifestPath(resourcesPath: string, override?: string): string {
    if (override) {
        if (!path.isAbsolute(override)) throw new Error("BAR_NATIVE_ENGINE_MANIFEST must be an absolute path.");
        return override;
    }
    return path.join(resourcesPath, "native-engine", "manifest.json");
}

function requiredString(value: unknown, name: string, pattern?: RegExp): asserts value is string {
    if (typeof value !== "string" || value.trim() === "" || value.includes("\0") || (pattern && !pattern.test(value))) throw new Error(`Invalid native runtime manifest field: ${name}.`);
}

function containedPath(root: string, relative: string, name: string): string {
    requiredString(relative, name);
    if (path.isAbsolute(relative)) throw new Error(`Native runtime ${name} must be relative to its manifest.`);
    const candidate = path.resolve(root, relative);
    if (candidate === root || !candidate.startsWith(root + path.sep)) throw new Error(`Native runtime ${name} escapes its resource directory.`);
    const resolved = fs.realpathSync(candidate);
    if (!resolved.startsWith(root + path.sep)) throw new Error(`Native runtime ${name} follows a link outside its resource directory.`);
    return resolved;
}

function executable(root: string, relative: string, name: string): string {
    const binary = containedPath(root, relative, name);
    if (!fs.statSync(binary).isFile()) throw new Error(`Native runtime ${name} is not a file.`);
    fs.accessSync(binary, fs.constants.X_OK);
    // This lane deliberately packages a thin ARM64 Mach-O, never a Linux ELF or
    // an Intel executable selected solely because its filename is "spring".
    const header = Buffer.alloc(32);
    const handle = fs.openSync(binary, "r");
    try {
        if (fs.readSync(handle, header, 0, header.length, 0) !== header.length || header.readUInt32LE(0) !== 0xfeedfacf || header.readUInt32LE(4) !== 0x0100000c || header.readUInt32LE(12) !== 2) {
            throw new Error(`Native runtime ${name} is not an ARM64 Mach-O executable.`);
        }
    } finally {
        fs.closeSync(handle);
    }
    return binary;
}

export function loadNativeRuntime(manifestPath: string): NativeRuntime {
    if (!path.isAbsolute(manifestPath)) throw new Error("Native runtime manifest path must be absolute.");
    let manifest: NativeRuntimeManifest;
    try {
        manifest = JSON.parse(fs.readFileSync(manifestPath, "utf8"));
    } catch (error) {
        throw new Error(`Packaged native engine manifest is unavailable: ${error instanceof Error ? error.message : String(error)}`);
    }
    if (manifest?.schemaVersion !== 1 || manifest.platform !== "darwin" || manifest.architecture !== "arm64" || manifest.renderer !== "metal4")
        throw new Error("Native runtime manifest must describe the Darwin ARM64 Metal4 package.");
    requiredString(manifest.engine?.id, "engine.id");
    requiredString(manifest.engine?.sha256, "engine.sha256", /^[a-f0-9]{64}$/);
    requiredString(manifest.engine?.sourceCommit, "engine.sourceCommit", /^[a-f0-9]{40}$/);
    if (typeof manifest.engine.capabilities?.luaMenu !== "boolean" || typeof manifest.engine.capabilities?.gameplay !== "boolean")
        throw new Error("Native runtime manifest must explicitly declare LuaMenu and gameplay acceptance.");
    if (manifest.engine.experimentalOfflineScene !== undefined && typeof manifest.engine.experimentalOfflineScene !== "boolean")
        throw new Error("Native runtime experimentalOfflineScene must be a boolean.");
    if (manifest.engine.lane !== undefined) {
        if (manifest.engine.lane !== "development" && manifest.engine.lane !== "official-multiplayer") throw new Error("Invalid native engine lane.");
        requiredString(manifest.engine.executableVersion, "engine.executableVersion");
        requiredString(manifest.engine.syncVersion, "engine.syncVersion");
        requiredString(manifest.engine.nativePortPatchManifestSHA256, "engine.nativePortPatchManifestSHA256", /^[a-f0-9]{64}$/);
        if (manifest.engine.lane === "official-multiplayer") {
            requiredString(manifest.engine.upstreamTag, "engine.upstreamTag");
            if (manifest.engine.syncVersion !== manifest.engine.upstreamTag) throw new Error("Official engine sync version must match its actual upstream tag.");
        }
    }
    if (manifest.chobby?.format !== "rapid") requiredString(manifest.chobby?.sourceCommit, "chobby.sourceCommit", /^[a-f0-9]{40}$/);
    requiredString(manifest.chobby?.archiveName, "chobby.archiveName");
    const root = fs.realpathSync(path.dirname(manifestPath));
    const binary = executable(root, manifest.engine.binary, "engine.binary");
    const digest = createHash("sha256").update(fs.readFileSync(binary)).digest("hex");
    if (digest !== manifest.engine.sha256) throw new Error("Packaged native engine does not match its manifest SHA-256.");
    const dataDirectory = containedPath(root, manifest.dataDirectory, "dataDirectory");
    if (!fs.statSync(dataDirectory).isDirectory()) throw new Error("Native runtime dataDirectory is not a directory.");
    return {
        manifest,
        root,
        binary,
        dataDirectory,
        checksumBinary: manifest.engine.checksumBinary ? executable(root, manifest.engine.checksumBinary, "engine.checksumBinary") : undefined,
        downloaderBinary: manifest.engine.downloaderBinary ? executable(root, manifest.engine.downloaderBinary, "engine.downloaderBinary") : undefined,
    };
}

export function requireNativeVersion(runtime: NativeRuntime, engineVersion: string) {
    if ((runtime.manifest.engine.syncVersion ?? runtime.manifest.engine.id) !== engineVersion) throw new Error(`Native engine ${engineVersion} is not packaged. This app contains ${runtime.manifest.engine.id}.`);
}

export function validateChobbyArchive(runtime: NativeRuntime): string {
    const menu = runtime.manifest.chobby;
    if (menu.format === "rapid") {
        const sdp = containedPath(runtime.root, menu.packagePath!, "chobby.packagePath");
        requiredString(menu.sdpSHA256, "chobby.sdpSHA256", /^[a-f0-9]{64}$/);
        if (createHash("sha256").update(fs.readFileSync(sdp)).digest("hex") !== menu.sdpSHA256) throw new Error("Official Chobby SDP differs from its pin.");
        return menu.archiveName;
    }
    const directory = containedPath(runtime.root, menu.directory!, "chobby.directory");
    if (!directory.startsWith(runtime.dataDirectory + path.sep) || !directory.endsWith(".sdd") || !fs.statSync(directory).isDirectory()) throw new Error("Native Chobby archive must be a contained .sdd directory.");
    const fields = parseLuaTable(fs.readFileSync(path.join(directory, "modinfo.lua")));
    const name = fields.find((field) => field.key === "name")?.value;
    const version = fields.find((field) => field.key === "version")?.value;
    if (name !== "BYAR Chobby" || typeof version !== "string" || `${name} ${version}` !== menu.archiveName) throw new Error("The packaged Chobby archive identity differs from modinfo.lua.");
    return menu.archiveName;
}

export function nativeDataDirectories(runtime: NativeRuntime, assetsPath: string): string {
    return [...new Set([runtime.dataDirectory, path.resolve(assetsPath)])].join(path.delimiter);
}

export function luaMenuLaunchPlan(runtime: NativeRuntime, writeDirectory: string, assetsPath: string) {
    if (!runtime.manifest.engine.capabilities.luaMenu) throw new Error("The packaged native engine does not support Chobby yet. Stay in BAR Lobby until the native menu is ready.");
    const archiveName = validateChobbyArchive(runtime);
    return {
        binary: runtime.binary,
        cwd: path.dirname(runtime.binary),
        args: ["--write-dir", writeDirectory, "--isolation", "--menu", archiveName],
        dataDirectories: nativeDataDirectories(runtime, assetsPath),
        env: nativeProcessEnv(runtime, assetsPath),
        ...(runtime.manifest.engine.lane ? { engineLane: runtime.manifest.engine.lane } : {}),
    };
}

export function nativeProcessEnv(runtime: NativeRuntime, assetsPath: string): NodeJS.ProcessEnv {
    const env: NodeJS.ProcessEnv = { ...process.env };
    for (const name of Object.keys(env)) {
        if (/^(VK_|MESA_|GALLIUM_|ZINK_|EGL_)/.test(name) || ["DYLD_LIBRARY_PATH", "DYLD_FALLBACK_LIBRARY_PATH", "BAR_MACOS_VULKAN_BACKEND", "BAR_MACOS_METAL_RENDERER", "RECOIL_METAL4_SCENE_INPUT_PROBE", "RECOIL_METAL4_SCENE_CAPTURE_FRAME", "RECOIL_METAL4_SCENE_CAPTURE_FRAMES", "RECOIL_METAL4_SCENE_PNG", "RECOIL_METAL4_SCENE_CAPTURE_DIRECTORY"].includes(name)) delete env[name];
    }
    if (runtime.manifest.engine.lane === "official-multiplayer") delete env.BAR_METAL4_EXPERIMENTAL_SINGLEPLAYER;
    env.SPRING_DATADIR = nativeDataDirectories(runtime, assetsPath);
    return env;
}
