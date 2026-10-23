// SPDX-FileCopyrightText: 2026 The BAR Lobby Authors
//
// SPDX-License-Identifier: MIT

import { reactive } from "vue";
import { isChobbyActive, shouldLaunchChobbyForDevChange, type ChobbySessionState } from "@shared/chobby-state";
import { settingsStore } from "./settings.store";

export const chobbyStore = reactive<ChobbySessionState & { initialized: boolean; launchError?: string }>({ platform: "other", phase: "closed", initialized: false });

export async function initChobbyStore() {
    if (chobbyStore.initialized) return;
    window.chobby.onStateChanged((state) => Object.assign(chobbyStore, { error: undefined, ...state }));
    Object.assign(chobbyStore, await window.chobby.getState(), { initialized: true });
}

export const isChobbyBusy = () => isChobbyActive(chobbyStore);

export async function openChobby() {
    chobbyStore.launchError = undefined;
    try {
        const result = await window.chobby.launch();
        if (result.status === "failed") chobbyStore.launchError = result.details || result.reason;
    } catch (error) {
        chobbyStore.launchError = error instanceof Error ? error.message : String(error);
    }
}

export async function setDevModeFromUser(enabled: boolean) {
    const previous = settingsStore.devMode;
    settingsStore.devMode = enabled;
    try {
        // Await persistence before main enforces the Dev permission. This is an
        // explicit checkbox event; restored startup settings never call it.
        await window.settings.updateSettings({ devMode: enabled });
        if (shouldLaunchChobbyForDevChange(previous, enabled, chobbyStore.platform)) await openChobby();
    } catch (error) {
        chobbyStore.launchError = error instanceof Error ? error.message : String(error);
    }
}
