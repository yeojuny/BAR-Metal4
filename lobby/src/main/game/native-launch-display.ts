// SPDX-License-Identifier: MIT
// The actual lobby display is a launch hint, independent of the engine's saved mode.
import { BrowserWindow, screen } from "electron";

export function getNativeLaunchDisplayEnv(): NodeJS.ProcessEnv {
    if (process.platform !== "darwin") return {};
    const id = Number(process.env.MAIN_WINDOW_ID);
    const window = Number.isSafeInteger(id) && id > 0 ? BrowserWindow.fromId(id) : null;
    if (!window || window.isDestroyed()) return {};
    // Electron DIP and SDL Cocoa bounds use the same top-left desktop space,
    // including negative origins. Do not pass backing scale, workArea or an index.
    const { x, y, width, height } = screen.getDisplayMatching(window.getBounds()).bounds;
    return { BAR_MACOS_TARGET_DISPLAY: [x, y, width, height].join(",") };
}
