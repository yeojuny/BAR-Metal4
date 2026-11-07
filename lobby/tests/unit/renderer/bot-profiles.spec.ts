// SPDX-FileCopyrightText: 2026 The BAR Lobby Authors
// SPDX-License-Identifier: MIT

import fs from "node:fs";
import { beforeEach, describe, expect, it, vi } from "vitest";
import { mount, flushPromises } from "@vue/test-utils";
import { nextTick } from "vue";
import type { BattleWithMetadata, Bot } from "@main/game/battle/battle-types";
import { GameModeID, StartPosType } from "@main/game/battle/battle-types";
import { parseLuaOptions } from "@main/utils/parse-lua-options";
import { startScriptConverter } from "@main/utils/start-script-converter";

vi.mock("@main/utils/logger", () => ({ logger: () => ({ warn() {} }) }));
vi.mock("@renderer/store/db", () => ({ db: {} }));
vi.mock("@renderer/store/game.store", () => ({ gameStore: { selectedGameVersion: { ais: [] } }, startBattle: vi.fn() }));
vi.mock("@renderer/store/engine.store", () => ({ enginesStore: { selectedEngineVersion: { ais: [] as unknown[] } } }));
vi.mock("@renderer/store/maps.store", () => ({ getRandomMap: vi.fn() }));
vi.mock("@renderer/store/me.store", () => ({ me: { userId: "1", username: "Player", status: "menu", battleRoomState: {} } }));
vi.mock("@renderer/api/notifications", () => ({ notificationsApi: { alert: vi.fn() } }));
vi.mock("@renderer/components/battle/TeamParticipant.vue", () => ({ default: { template: "<div><slot /></div>" } }));
vi.mock("@renderer/components/battle/FactionSelect.vue", () => ({ default: { template: "<div />" } }));
vi.mock("@renderer/components/icons/GameIconsVelociraptor.vue", () => ({ default: { template: "<div />" } }));
vi.mock("@renderer/components/common/Modal.vue", () => ({ default: { template: '<div><slot /><slot name="footer" /></div>' } }));
vi.mock("@renderer/components/common/TabView.vue", () => ({ default: { template: "<div><slot /></div>" } }));
vi.mock("primevue/tabpanel", () => ({ default: { template: "<div><slot /></div>" } }));
vi.mock("@renderer/components/controls/Select.vue", () => ({
    default: {
        props: ["modelValue", "options"],
        emits: ["update:modelValue"],
        template:
            '<select :value="modelValue" @change="$emit(\'update:modelValue\', $event.target.value)"><option v-for="option of options" :key="option.key" :value="option.key">{{ option.name }}</option></select>',
    },
}));
vi.mock("@renderer/components/controls/Button.vue", () => ({ default: { template: "<button><slot /></button>" } }));
vi.mock("@renderer/components/controls/Range.vue", () => ({ default: { template: "<div />" } }));
vi.mock("@renderer/components/controls/Checkbox.vue", () => ({ default: { template: "<div />" } }));
vi.mock("@renderer/components/controls/Textarea.vue", () => ({ default: { template: "<div />" } }));
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
import { battleStore } from "@renderer/store/battle.store";
import { enginesStore } from "@renderer/store/engine.store";

const optionsPath = process.env.BAR_AI_OPTIONS_PATH;
describe.skipIf(!optionsPath)("pinned BARb metadata through lobby options and startscript", () => {
    beforeEach(() => {
        enginesStore.selectedEngineVersion!.ais = [{ shortName: "BARb", name: "BARbarIAn", version: "stable", description: "", options: parseLuaOptions(fs.readFileSync(optionsPath!)) }];
    });

    it.each(["easy", "medium", "hard"])("writes the actual %s profile key for the native AI", async (profile) => {
        const bot: Bot = { id: "1", host: "0", aiShortName: "BARb", name: "BARbarIAn (2)", aiOptions: {} };
        const player = { id: "0", user: { username: "Player", userId: "1" }, contentSyncState: { engine: 1, game: 1, map: 1 } } as never;
        battleStore.me = player;
        battleStore.spectators = [];
        battleStore.teams = [{ participants: [player] }, { participants: [bot] }];
        battleStore.battleOptions = {
            gameVersion: "selected-local26",
            gameMode: { id: GameModeID.SKIRMISH, options: {} },
            map: { springName: "Map", startboxesSet: [], playerCountMax: 2 } as never,
            mapOptions: {
                startPosType: StartPosType.Boxes,
                customStartBoxes: [
                    { left: 0, top: 0, right: 0.2, bottom: 1 },
                    { left: 0.8, top: 0, right: 1, bottom: 1 },
                ],
            },
            restrictions: [],
        };
        const view = mount(BotParticipant, { props: { bot, teamId: "1" }, global: { directives: { tooltip: {} } } });
        await view.trigger("contextmenu");
        await flushPromises();
        const select = view.get("select");
        expect(select.findAll("option").map((option) => option.attributes("value"))).toEqual(["hard", "medium", "easy", "dev"]);
        expect(select.element.value).toBe("hard");
        // Force a nondefault first so choosing the hard default also exercises reset.
        await select.setValue("medium");
        await select.setValue(profile);
        await nextTick();
        expect(bot.aiOptions).toEqual(profile === "hard" ? {} : { profile });
        const script = await startScriptConverter.generateScriptStr(battleStore as unknown as BattleWithMetadata);
        if (profile === "hard") expect(script).not.toMatch(/profile=/);
        else expect(script).toContain(`profile=${profile};`);
        expect(script).toContain("shortname=BARb;");
        view.unmount();
    });
});
