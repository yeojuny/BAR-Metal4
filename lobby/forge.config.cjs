// Verify the dependency patch even when packaging calls the Forge API directly.
require("./tools/apply-replay-parser-patch.cjs").apply(__dirname, true);

const { VitePlugin } = require("@electron-forge/plugin-vite");

const config = {
    plugins: [
        new VitePlugin({
            concurrent: 1,
            build: [
                {
                    entry: "src/main/main.ts",
                    config: "vite.main.config.mts",
                    target: "main",
                },
                {
                    entry: "src/preload/preload.ts",
                    config: "vite.preload.config.mts",
                    target: "preload",
                },
            ],
            renderer: [
                {
                    name: "main_window",
                    config: "vite.renderer.config.mts",
                },
            ],
        }),
    ],
};

module.exports = config;
