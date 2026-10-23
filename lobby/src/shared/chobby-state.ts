// SPDX-License-Identifier: MIT
export interface ChobbySessionState {
    platform: "darwin" | "other";
    phase: "closed" | "starting" | "running" | "failed";
    error?: string;
}
export const isChobbyActive = (state: ChobbySessionState) => state.phase === "starting" || state.phase === "running";
export const shouldLaunchChobbyForDevChange = (previous: boolean, enabled: boolean, platform: ChobbySessionState["platform"]) => platform === "darwin" && !previous && enabled;
