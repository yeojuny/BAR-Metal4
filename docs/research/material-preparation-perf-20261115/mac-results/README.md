# PR6 D — Mac results, two scenes

Tested source: PR6 `c63eae437fe4a0545f9821e666e1220edcffd607` plus the attached **two-file bounded aggregate diagnostic patch**. Both patched Git files were byte-compared with the actual local compilation inputs. The patch adds sampled kinds43–46 and one aggregate per completed material batch, emitted after Join; it does not change D's lookup algorithm or defaults. [Source/build identity](BUILD-EVIDENCE.json) · [Exact selected-file hashes](MANIFEST.json) · [Patch](local-diagnostic.patch).

Signed engine: `5c37f7554fc8588783f27558dfa9363299fc3c6ec3d80a8e9804fa902ebf1e2a`. Existing Mac full build/link passed; archive audit1,538objects/35archives/0mismatches. No build, game or profiling was run to publish these records.

## All clean runs

| Scene / run | D | FPS | 1% low FPS |
| --- | --- | ---: | ---: |
| Construction A1_MATERIAL_OFF | OFF | 185.240743052 | 139.925373 |
| Construction B1_MATERIAL_ON | ON | 185.134251626 | 136.116152 |
| Construction B2_MATERIAL_ON | ON | 184.829618204 | 131.907308 |
| Construction A2_MATERIAL_OFF | OFF | 185.275000000 | 130.662021 |
| Battle A1_BATTLE_OFF | OFF | 55.333398545 | 45.725646 |
| Battle B1_BATTLE_ON | ON | 55.337550613 | 46.464646 |
| Battle B2_BATTLE_ON | ON | 55.377767367 | 44.747081 |
| Battle A2_BATTLE_OFF | OFF | 55.319465817 | 45.725646 |

Construction mean OFF **185.257872** → ON **184.981935** FPS (**−0.148947%**). Battle mean OFF **55.326432** → ON **55.357659** (**+0.056441%**). All eight valid runs/tails remain, two runs per mode per scene. These are CPU-observed DrawScreen intervals, not display presents or exclusive GPU execution time. FPS improvement is not established; degradation, equivalence and non-regression are not proved either. [Construction FPS.csv](FPS.csv) · [Battle FPS.csv](battle-measurement/FPS.csv). Eight 40-second frame CSVs are under the corresponding clean run directories.

## Work and correctness

- Construction OFF/ON diagnostic runs: Metal Validation enabled, zero detected errors/dropped records, representative paused stills reviewed with no new visible regression. ON ID-reader invocations4,117; omitted13,547 (**76.6927%**, including nil callbacks). Most-recent material-buffer hits0. The original positive-buffer-hit efficacy assertion failed; [original failure](measurement/diagnostic_paced/preparation/runs/D1_MATERIAL_ON/FAILURE.json) is preserved. Its later correctness interpretation did not rerun the diagnostic or change the algorithm. [Gate](measurement/DIAGNOSTIC-GATE.json).
- Battle: one ON diagnostic, zero detected Metal errors/drops, populated tank combat/projectiles/explosions observed. It is **not an OFF/ON pixel-equivalence test**. ID-reader calls1,521; omitted2,223 (**59.375%** of24/Get baseline); recent-buffer hits0. [Gate and picture](battle-measurement/DIAGNOSTIC-GATE.json).
- Omitted reader callbacks include nil slots. They are not equivalent counts of nonnil Objective-C properties, GPU operations or net CPU time saved. Net CPU/RAM/GPU savings remain unproved; exact-object comparisons and additional retained objects have costs.

## Exact tested conditions

D `BAR_NATIVE_MATERIAL_IDENTITY_LOOKUP` is the only compared candidate flag. U `BAR_NATIVE_SHARED_PAIR_KEY_VIEW=0`, Q CUS `BAR_METAL4_SHARED_CUS_PILOT=0`, Sepia OFF, MODEL_TYPE_FACTS1, SCREEN_STATE_ELISION1, PREPARE1/BEGIN0, FINAL_COLOR1, worker1. Clean runs have trace/Validation off, AA0/VSyncOff, native1512×950 and AC mode0. Diagnostics are separately flagged; [actual diagnostic child environments](CHILD-ENVIRONMENTS.json) retain their whitelist receipts. The shipped wrapper clears BAR_NATIVE settings; these private tests used explicit child settings, not parent-shell assumptions.

Battle map: **Seths Ravine Remake1.3.1**. Official command `luarules benchmark armbull armbull 80 8 1400`, benchmark seed **7654321**, game seed154658555. Same initial commander/camera in all arms; command at sim240,10s combat warmup,40s counted sim540–1740. Each run observed608 created/444 destroyed over the full combat-active50s, including warmup; these are not40s-only counts. Current native content/graphics were used; exact pairing with historical MVK3/MVK4 is not established. [Scene source, checksums and timing](battle-scene/SCENE-SOURCE.json).

Selected original diagnostic log lines carry original line numbers; their original full-file hashes are in MANIFEST. PNGs and frame CSV bytes are unchanged. Only public text path prefixes are normalized; original evidence stays untouched. No game content, binaries, build cache or full profile is included.

## Disposition and credits

Preserve the verified lookup-call reduction as a candidate for future interaction/synergy evaluation. **D remains default OFF**, the prior S baseline and public deployment remain unchanged. U remains **Mac untested**. No automatic adoption, release or Homebrew update follows from these results.

Existing authorship and license notices are unchanged. The inherited tomjn PR#3695 LuaVBO diagnostic refinement and Beherith's GPLv2-or-later Sepia source remain credited in the existing provenance. No new external implementation is imported by the local aggregate patch.
