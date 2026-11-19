# GroundDecal two-arm validation staging

Prepared only: one copied candidate relocation/sign and one fresh validation fixture staging succeeded. Games/builds/CPU tests/GPU helpers/benches/captures/retries all0. Original raw build, R03 runtime, apps, source, main/releases/Homebrew unchanged. No R04/Texture work.

Signed candidate035d62084b6657d663a08e6422cdd696af0fafedc01a5189ba696619b15f20c4; R03 baseline107958872e5d962e4d27803a0fb1af0e842b18bd6982bd9b74e115d956890ab9. Strict adhoc signature check passed,9 accepted loader relocations,19 dylibs exactly match. Source/SDK/compiler/dependency provenance from prior fixed compile is included. Binary stays local, not uploaded.

Validation-only ORDER selects V_R03_BASELINE then V_GDP_CANDIDATE. Both have API Validation1, shader validation0, same Ultra AA0, seeds/world/camera/controller and S01/G01 inputs. Each13 fixture hashes match the previous R03 validation. Fresh claims absent. No clean suite is selectable or staged. Runner diff is limited to pins/labels/validation selection and two-arm completion metadata;9 functions AST-identical. Launch/parser/error/image/cleanup logic preserved.

Current main-display bounds0,0,1512,982 satisfy fixture. Viewport1512x950/drawable1512x982/main sample1 are future runtime requirements, not observed here. WINDOW-CONTRACT documents frozen controller540..1740 and inherited stale LAUNCH600..1800 planning metadata. Actual controller unchanged.

RESULT and TEST-PLAN contain the exact future command and all evidence/cleanup checks. Runtime GPU safety, screen equality, actual copy/alloc/RAM reduction and FPS remain unverified. Ordinary next validation cannot prove all revision/failed submission/context/quarantine/shutdown paths. All staging PIDs exited; no awake/GPU work created.

Existing engine GPL, Beherith shader/MIT and recorded tomjn/benbreen contribution provenance preserved in UPSTREAM-CONTRIBUTIONS.json.
