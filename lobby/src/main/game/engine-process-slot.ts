// SPDX-FileCopyrightText: 2026 The BAR Lobby Authors
//
// SPDX-License-Identifier: MIT

export class EngineProcessSlot {
    private owner: "game" | "chobby" | null = null;

    public acquire(owner: "game" | "chobby"): () => void {
        if (this.owner) throw new Error(`An engine session (${this.owner}) is already starting or running.`);
        this.owner = owner;
        let released = false;
        return () => {
            if (!released) {
                released = true;
                this.owner = null;
            }
        };
    }
}

export const engineProcessSlot = new EngineProcessSlot();
