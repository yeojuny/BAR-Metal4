import { Configuration } from "electron-builder";
import { chmod, readdir } from "fs/promises";
import path from "path";

/**
 * @see https://www.electron.build/configuration
 */
if (process.platform === "darwin" && (!process.env.BAR_NATIVE_LANES_DIR || !process.env.BAR_NATIVE_NOTICES_DIR)) {
    throw new Error("Native macOS packaging requires the two staged engines and their reviewed dependency-notice directory.");
}
const config: Configuration = {
    appId: "info.beyondallreason.lobby",
    // Should be the same as APP_NAME in src/main/config/app.ts and in
    // workaround in installer.nsh.
    productName: "BeyondAllReason",

    asar: true,
    disableDefaultIgnoredFiles: true,
    files: ["./.vite/**", "!node_modules", "./node_modules/7zip-bin/**"],
    directories: { buildResources: "buildResources" },
    asarUnpack: ["resources/**", "node_modules/7zip-bin/**"],
    // electron-builder unpacks the 7za binary from the asar but doesn't preserve its executable bit on Linux, so restore it here.
    afterPack: async ({ electronPlatformName, appOutDir, packager }) => {
        if (electronPlatformName === "darwin") {
            await chmod(path.join(appOutDir, `${packager.appInfo.productFilename}.app`, "Contents", "Resources", "app.asar.unpacked", "node_modules", "7zip-bin", "mac", "arm64", "7za"), 0o755);
            return;
        }
        if (electronPlatformName !== "linux") {
            return;
        }
        const binDir = path.join(appOutDir, "resources", "app.asar.unpacked", "node_modules", "7zip-bin", "linux");
        for (const arch of await readdir(binDir)) {
            await chmod(path.join(binDir, arch, "7za"), 0o755);
        }
    },

    publish: { provider: "github" },
    fileAssociations: [
        {
            ext: "barreplay",
            description: "BAR Replay File",
            role: "Viewer",
            icon: "icon.ico",
            name: "BAR Replay",
        },
    ],

    mac: {
        target: ["dir"], category: "public.app-category.games", icon: "buildResources/icon.icns",
        minimumSystemVersion: "27.0", identity: null,
        files: ["./.vite/**", "!.vite/**/*.map", "!node_modules/7zip-bin/!(index.js|package.json|LICENSE.txt|mac){,/**/*}", "!node_modules/7zip-bin/mac/!(arm64){,/**/*}", "!node_modules/7zip-bin/mac/arm64/!(7za){,/**/*}"],
        extraResources: [
            ...(process.env.BAR_NATIVE_LANES_DIR ? [{ from: process.env.BAR_NATIVE_LANES_DIR, to: "native-engines", filter: ["development/**", "official-multiplayer/**"] }] : []),
            ...(process.env.BAR_NATIVE_NOTICES_DIR ? [{ from: process.env.BAR_NATIVE_NOTICES_DIR, to: "legal" }] : []),
            { from: "LICENSES", to: "source-licenses" },
            { from: "LICENSE.md", to: "LICENSE.md" },
        ],
    },

    // Windows
    win: {
        target: ["nsis"],
        extraResources: [
            {
                from: "buildResources/cacert.pem",
                to: "cacert.pem",
            },
        ],
    },
    nsis: {
        artifactName: "${productName}-${version}-setup.${ext}",
        uninstallDisplayName: "Beyond All Reason",
        shortcutName: "Beyond All Reason",
        oneClick: true,
        perMachine: false,
        allowToChangeInstallationDirectory: false,
        include: "build/installer.nsh",
    },

    // Linux
    linux: {
        target: ["AppImage"],
        category: "Game",
        mimeTypes: ["application/x-barreplay"],
    },
    appImage: {},
};

export default config;
