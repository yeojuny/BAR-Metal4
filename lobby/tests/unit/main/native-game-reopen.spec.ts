// SPDX-FileCopyrightText: 2026 The BAR Lobby Authors
// SPDX-License-Identifier: MIT

import { beforeEach, describe, expect, it, vi } from "vitest";

const mocks = vi.hoisted(() => ({
    on: vi.fn(),
    getAllWindows: vi.fn(),
    isExperimentalSceneRunning: vi.fn(),
}));
vi.mock("electron", () => ({ app: { on: mocks.on }, BrowserWindow: { getAllWindows: mocks.getAllWindows } }));
vi.mock("@main/game/game", () => ({ gameAPI: { isExperimentalSceneRunning: mocks.isExperimentalSceneRunning } }));
vi.mock("@main/replays/replays", () => ({ replaysAPI: {} }));
vi.mock("@main/typed-ipc", () => ({ typedWebContents: (value: unknown) => value }));

import { navigationService } from "@main/services/navigation.service";

describe("opening another lobby instance during a native game", () => {
    const mainWindow = { isMinimized: () => true, restore: vi.fn(), focus: vi.fn(), webContents: {} };

    beforeEach(() => {
        vi.clearAllMocks();
        mocks.getAllWindows.mockReturnValue([mainWindow]);
        navigationService.registerIpcHandlers({ ipc: { handle: vi.fn() } } as unknown as Parameters<typeof navigationService.registerIpcHandlers>[0]);
    });

    it.each([true, false])("restores and focuses only if the native game is closed (running=%s)", (running) => {
        mocks.isExperimentalSceneRunning.mockReturnValue(running);
        expect(mocks.on).toHaveBeenCalledWith("second-instance", expect.any(Function));
        const secondInstance = mocks.on.mock.calls[0][1];
        secondInstance({}, ["BAR"], "/unused");
        expect(mainWindow.restore).toHaveBeenCalledTimes(running ? 0 : 1);
        expect(mainWindow.focus).toHaveBeenCalledTimes(running ? 0 : 1);
    });
});
