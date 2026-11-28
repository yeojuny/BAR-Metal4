# GroundDecal ordinary validation: no Metal errors; sample-bin review required

Two fixed validation games completed exactly once, R03 then GroundDecal. Actual Metal API Validation enabled in both; original hazard/residency/assertion gate found0 errors, normal exits2. Native raw markers confirm main sample1, drawable1512x982, safeview1512x950. Ultra4/AA0, PREPARE1 BEGIN0 worker1, S01/G01 positive witnesses and fixed seed inputs preserved. No build/helper/restage/clean benchmark/capture/retry/source or runner changes. Diagnostic FPS excluded.

Both observed40 rows span simulation540..1740. They are NOT all identical: first baseline bin540..577 versus candidate540..570 (sampled units171 versus172); next start577 versus570. Remaining38 rows exactly match world/camera/speed/pause fields. First-row unit counts are taken at different simulation times and do not prove gameplay divergence. Cause of bin scheduling difference is UNKNOWN; no row was removed and no rerun performed. Initial state/camera and final600created/433destroyed agree. This requires JINI review, not an unqualified matched-world pass.

Actual two scene.png files were viewed. No obvious candidate-only missing/corrupt static terrain, buildings or ground decal pattern; occlusion appears similar. Explosions/smoke/illumination and debug timing text differ. Green sensor/range arcs in both; no candidate-only orange ring observed. This is limited RGB visual review, not exact pixel/depth/stencil equality. Image originals stay local, SHA256/path/size in IMAGE-REVIEW; PNGs are not uploaded.

Retirement/cleanup evidence: prior_retired_before_submit marker, normal engine exits, owned groups gone, image workers joined and awake released. Owned game PIDs13486/13635, runner13473 and awake13474 absent. No per-event GPU tracing was added. Ordinary gameplay/API validation does not prove every skipped/reversed revision, failed submission/quarantine, context-change or shutdown-error path. Page reuse/copy/alloc/RAM benefit and clean FPS remain unmeasured.

All raw engine/infolog/CSV, launch, lifecycle, validation/observer/cleanup receipts and40-row comparison are preserved. Binary/19 dylib/runner hashes unchanged after execution. Source and original app/main/releases/Homebrew protected; Texture/R04 untouched. Existing engine GPL/Beherith MIT and recorded tomjn/benbreen provenance preserved.

Return to JINI for bounded review. No next game or performance execution authorized by this report.
