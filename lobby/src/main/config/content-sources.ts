// SPDX-FileCopyrightText: 2025 The BAR Lobby Authors
//
// SPDX-License-Identifier: MIT

import axios from "axios";
import { configService } from "@main/services/config.service";

export interface EngineReleaseInfo {
    filename: string;
    springname: string;
    md5: string;
    category: string;
    version: string;
    path: string;
    tags: string[];
    size: number;
    timestamp: string;
    mirrors: string[];
}

export function getEngineReleaseCategory(platform: NodeJS.Platform = process.platform): string {
    if (platform === "win32") return "engine_windows64";
    if (platform === "linux") return "engine_linux64";
    if (platform === "darwin") {
        throw new Error("Native macOS engine downloads are not available yet; refusing to use engine_linux64 on macOS.");
    }
    throw new Error(`Unsupported engine download platform: ${platform}`);
}

export const findEngineReleaseUrl = (engineVersion: string) => {
    const archStr = getEngineReleaseCategory();
    const url = new URL(configService.getConfig().engineReleaseUrl);
    url.searchParams.set("category", archStr);
    url.searchParams.set("springname", engineVersion);
    return url;
};

export const getEngineReleaseInfo = async (engineVersion: string) => {
    const engineReleaseUrl = findEngineReleaseUrl(engineVersion);
    const engineResponse = await axios.get(engineReleaseUrl.toString());
    const engineInfo: EngineReleaseInfo = engineResponse.data[0];
    return engineInfo;
};
