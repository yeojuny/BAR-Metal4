// SPDX-FileCopyrightText: 2026 The BAR Lobby Authors
//
// SPDX-License-Identifier: MIT

import type { BrowserWindow } from "electron";
import type { ChobbySessionState } from "@shared/chobby-state";

type HandoffWindow = Pick<BrowserWindow, "isDestroyed" | "isVisible" | "isFocused" | "hide" | "show" | "showInactive" | "focus"> & {
    webContents: Pick<BrowserWindow["webContents"], "isAudioMuted" | "setAudioMuted">;
};

export class LobbyHandoff {
    private previous?: { visible: boolean; focused: boolean; audioMuted: boolean };

    public constructor(private readonly window: HandoffWindow) {}

    public apply(state: ChobbySessionState) {
        if (this.window.isDestroyed()) return;
        if (state.phase === "starting" || state.phase === "running") {
            this.previous ??= { visible: this.window.isVisible(), focused: this.window.isFocused(), audioMuted: this.window.webContents.isAudioMuted() };
            // Cover Howler and introductory video sound without changing either
            // persisted volume preferences or individual playback/mute state.
            this.window.webContents.setAudioMuted(true);
            if (state.phase === "running") this.window.hide();
        } else if (this.previous) {
            const previous = this.previous;
            this.previous = undefined;
            this.window.webContents.setAudioMuted(previous.audioMuted);
            if (previous.visible) {
                if (previous.focused) {
                    this.window.show();
                    this.window.focus();
                } else {
                    this.window.showInactive();
                }
            } else {
                this.window.hide();
            }
        }
    }
}
