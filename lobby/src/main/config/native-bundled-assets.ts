// SPDX-License-Identifier: MIT
// Bootstrap paths must not import runtime validation, which depends on logging.
import fs from "node:fs";
import path from "node:path";

export function getBundledAssetsPath(resourcesPath: string): string {
    const lanes = path.join(resourcesPath, "native-engines");
    return path.join(fs.existsSync(lanes) ? path.join(lanes, "development") : path.join(resourcesPath, "native-engine"), "data");
}
