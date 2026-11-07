// SPDX-FileCopyrightText: 2026 The BAR Lobby Authors
// SPDX-License-Identifier: MIT

import { describe, expect, it, vi } from "vitest";
import { mount, flushPromises } from "@vue/test-utils";
import type { Bot } from "@main/game/battle/battle-types";

vi.mock("@renderer/store/battle.store", () => ({ battleActions: { updateBotOptions: vi.fn() } }));
vi.mock("@renderer/store/engine.store", () => ({
    enginesStore: { selectedEngineVersion: { ais: [{ shortName: "BARb", name: "BARbarIAn", options: [{ key: "actual-engine-options", hidden: false, options: [] }] }] } },
}));
vi.mock("@renderer/store/game.store", () => ({ gameStore: { selectedGameVersion: { ais: [] } } }));
vi.mock("@renderer/components/battle/TeamParticipant.vue", () => ({ default: { template: "<div><slot /></div>" } }));
vi.mock("@renderer/components/battle/FactionSelect.vue", () => ({ default: { template: "<div />" } }));
vi.mock("@renderer/components/icons/GameIconsVelociraptor.vue", () => ({ default: { template: "<div />" } }));
vi.mock("@renderer/components/battle/LuaOptionsModal.vue", () => ({
    default: { props: ["sections", "modelValue"], template: '<div class="bot-options">{{ sections[0]?.key }} {{ modelValue }}</div>' },
}));
vi.mock("primevue/contextmenu", async () => {
    const { defineComponent } = await import("vue");
    return {
        default: defineComponent({
            props: ["model"],
            setup(props, { expose }) {
                expose({ show: () => props.model[0].command() });
            },
            template: "<div />",
        }),
    };
});

import BotParticipant from "@renderer/components/battle/BotParticipant.vue";

describe("bot configuration", () => {
    it("opens the engine's actual options by AI identity after a display-name change", async () => {
        const bot: Bot = { id: "bot-2", host: "1", aiShortName: "BARb", name: "BARbarIAn (2)", aiOptions: {} };
        const view = mount(BotParticipant, { props: { bot, teamId: "0" } });
        await view.trigger("contextmenu");
        await flushPromises();
        expect(view.get(".bot-options").text()).toContain("actual-engine-options true");
        expect(view.get(".bot-options").attributes("id")).toBe("configure-bot-bot-2");
        view.unmount();
    });
});
