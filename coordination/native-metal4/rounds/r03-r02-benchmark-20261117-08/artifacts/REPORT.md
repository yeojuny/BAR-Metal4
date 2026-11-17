# R03 versus R02 — fixed Ultra AA0 battle

Three-run arithmetic mean CPU-observed FPS increased62.679156→64.446243 (+2.819258%). All three R03 runs exceeded all three R02 runs in this cohort. One separate engine Metal API Validation game passed with no matched errors; timing excluded. No historical pooling or rejected valid slow runs.

| Condition | Run1 FPS | Run2 FPS | Run3 FPS | Mean FPS | Mean1% low |
| --- | ---: | ---: | ---: | ---: | ---: |
| R02 |62.639038|62.693728|62.704700|62.679156|52.192892|
| R03 |64.433894|64.471778|64.433056|64.446243|53.773678|

OrderA B B A A B. Each observed simulation540..1740,40 bins/about40seconds. All40 unit/camera/speed/pause/simulation rows, seeds, initial state, command and created600/destroyed433 match. Loading about16.54–16.98s and actual warmup about21.98s excluded. Same Ultra AA0, sample1, viewport1512x950/drawable1512x982, sync off, AC normal, PREPARE1 BEGIN0 worker1, S01/G01=1 both; clean diagnostics/probes/debug/validation0. Raw sample/extent and window geometry were checked in all7 games; nativeFinalColorExtents null was not used as proof.

All7 actual post-count PNGs viewed. No obvious candidate-only geometry/line corruption or omission. Animated effects/timing text differ. An orange circle appears in A1_R02,A3_R02,B2_R03,B3_R03, absent in A2_R02,B1_R03 and validation:2/3 per clean arm. Cause and counted-window effect UNKNOWN. This balanced, non-candidate-specific snapshot variation is recorded; no run was excluded/repeated and no exhaustive pixel-parity claim is made. Prior standalone12065 color/depth/stencil comparisons are separate limited correctness evidence.

This is the total R03 scene effect; live early-reject/diamond call counts, GPU timings and RAM/copy/allocation deltas were not measured. No Simmers/low-load or true AA4 claim. Mean FPS is primary;1% low improved3.028737% as supporting evidence.

All7 games exited normally; owned groups and both awake PIDs gone. No build/helper/capture/replay/retry/source/runner change. Original app/main/release/Homebrew unchanged. JINI reviews adoption and next scope.

Full raw engine logs, interval CSVs, complete40-bin samples, launch/settings/validation/power/cleanup receipts are included. Original PNGs remain on the shared Mac at exact paths and SHA256 in EVIDENCE-INDEX.json; not claimed uploaded. Source/binary/runner pins are in SUMMARY.json and prior build round07. Existing source attribution is preserved; this packet adds execution evidence only.
