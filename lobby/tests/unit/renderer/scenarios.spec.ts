// SPDX-FileCopyrightText: 2026 The BAR Lobby Authors
// SPDX-License-Identifier: MIT

import { beforeEach, describe, expect, it, vi } from "vitest";
import { flushPromises, mount } from "@vue/test-utils";
import { nextTick } from "vue";
import type { Scenario } from "@main/content/game/scenario";

vi.mock("@renderer/store/game.store", async () => {
    const { reactive } = await import("vue");
    return { GameStatus: { CLOSED: 2, LOADING: 0, RUNNING: 1 }, gameStore: reactive({ status: 2, selectedGameVersion: undefined as { gameVersion: string } | undefined }) };
});
vi.mock("@renderer/store/engine.store", () => ({ enginesStore: { selectedEngineVersion: { id: "native-test-engine" } } }));
vi.mock("@renderer/store/db", () => ({ db: {} }));
vi.mock("@renderer/api/notifications", () => ({ notificationsApi: { alert: vi.fn() } }));
vi.mock("@renderer/composables/useDexieLiveQuery", async () => {
    const { computed } = await import("vue");
    return { useDexieLiveQueryWithDeps: (deps: import("vue").Ref<Scenario | undefined>[]) => computed(() => (deps[0].value ? { springName: deps[0].value.mapfilename } : undefined)) };
});
vi.mock("@renderer/components/controls/Button.vue", () => ({ default: { template: "<button><slot /></button>" } }));
vi.mock("@renderer/components/controls/DownloadContentButton.vue", () => ({ default: { props: ["games", "maps", "engines"], template: '<button class="download-launch"><slot /></button>' } }));
vi.mock("@renderer/components/controls/Select.vue", () => ({ default: { props: ["options", "modelValue"], template: '<div class="select" />' } }));
vi.mock("@renderer/components/misc/ScenarioTile.vue", () => ({ default: { props: ["scenario"], template: '<button class="tile">{{ scenario.title }}</button>' } }));
vi.mock("@renderer/components/common/Panel.vue", () => ({ default: { template: "<div><slot /></div>" } }));
vi.mock("@renderer/components/misc/Markdown.vue", () => ({ default: { props: ["source"], template: "<div>{{ source }}</div>" } }));

import Scenarios from "@renderer/views/play/scenarios.vue";
import { GameStatus, gameStore } from "@renderer/store/game.store";
import { notificationsApi } from "@renderer/api/notifications";

const scenario = (title: string): Scenario => ({
    index: 1,
    author: "Test",
    isnew: false,
    imagepath: "",
    imageflavor: "",
    playerstartx: "0",
    playerstarty: "0",
    partime: 0,
    parresources: 0,
    difficulty: 1,
    title,
    scenarioid: title,
    version: "1",
    summary: title,
    briefing: "Briefing",
    mapfilename: "Scenario Map",
    victorycondition: "Win",
    losscondition: "Lose",
    allowedsides: ["Cortex"],
    defaultdifficulty: "Normal",
    difficulties: [{ name: "Normal", playerhandicap: 0, enemyhandicap: 25 }],
    unitlimits: { armcom: 0 },
    scenariooptions: { scenarioid: title, disablefactionpicker: true, unitloadout: [] },
    startscript:
        "[Game]{ GameType=__BARVERSION__; MapName=__MAPNAME__; Side=__PLAYERSIDE__; Handicap=__ENEMYHANDICAP__; scenariooptions=__SCENARIOOPTIONS__; NumRestrictions=__NUMRESTRICTIONS__; __RESTRICTEDUNITS__ }",
});

let getScenarios: ReturnType<typeof vi.fn>;
let launchScenario: ReturnType<typeof vi.fn>;
beforeEach(() => {
    gameStore.status = GameStatus.CLOSED;
    gameStore.selectedGameVersion = undefined;
    getScenarios = vi.fn().mockResolvedValue([]);
    launchScenario = vi.fn().mockResolvedValue(undefined);
    Object.assign(window, { game: { getScenarios, launchScenario } });
    vi.mocked(notificationsApi.alert).mockClear();
});

describe("scenario selection and launch", () => {
    it("renders safely without an installed game, then loads the selected version", async () => {
        const view = mount(Scenarios);
        await flushPromises();
        expect(view.find(".scenario-preview").exists()).toBe(false);
        expect(view.text()).toContain("No scenarios are available");
        getScenarios.mockResolvedValue([scenario("Mission")]);
        gameStore.selectedGameVersion = { gameVersion: "selected-local26" } as never;
        await flushPromises();
        expect(getScenarios).toHaveBeenCalledWith("selected-local26");
        expect(view.find(".scenario-preview").text()).toContain("Mission");
        await view.find(".download-launch").trigger("click");
        await flushPromises();
        expect(launchScenario).toHaveBeenCalledWith(expect.stringContaining("GameType=selected-local26"), "selected-local26", "native-test-engine");
        expect(launchScenario.mock.calls[0][0]).toContain("Side=Cortex");
        expect(launchScenario.mock.calls[0][0]).toContain("Handicap=25");
        expect(launchScenario.mock.calls[0][0]).toContain("unit0=armcom");
        expect(launchScenario.mock.calls[0][0]).not.toMatch(/__[A-Z]+__/);
        view.unmount();
    });

    it("ignores a stale request after switching game versions", async () => {
        let resolveOld!: (value: Scenario[]) => void;
        getScenarios.mockImplementation((version) =>
            version === "old"
                ? new Promise((resolve) => {
                      resolveOld = resolve;
                  })
                : Promise.resolve([scenario("New mission")])
        );
        gameStore.selectedGameVersion = { gameVersion: "old" } as never;
        const view = mount(Scenarios);
        await nextTick();
        gameStore.selectedGameVersion = { gameVersion: "new" } as never;
        await flushPromises();
        resolveOld([scenario("Old mission")]);
        await flushPromises();
        expect(view.text()).toContain("New mission");
        expect(view.text()).not.toContain("Old mission");
        view.unmount();
    });

    it("reports launch failure and permits retry", async () => {
        gameStore.selectedGameVersion = { gameVersion: "selected-local26" } as never;
        getScenarios.mockResolvedValue([scenario("Mission")]);
        launchScenario.mockRejectedValue(new Error("Native launch failed"));
        const view = mount(Scenarios);
        await flushPromises();
        await view.find(".download-launch").trigger("click");
        await flushPromises();
        expect(gameStore.status).toBe(GameStatus.CLOSED);
        expect(notificationsApi.alert).toHaveBeenCalledWith({ text: "Native launch failed", severity: "error" });
        view.unmount();
    });
});
