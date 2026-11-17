# Fixed R03 isolated build and R02 comparison staging

One configure and one fresh engine target build succeeded. No game, benchmark, helper, capture or retry was started. Only the fixed R03 SensorLOS header differs from frozen R02 production source;10269 file closure before/after passes.

3142 normalized compile commands match R02, initial cache is byte-identical, SDK27.2/Apple clang21.0.0 preserved.1538 objects/35 archives,0 stale-member mismatches. Runtime has identical19 libraries, same9 loader relocations, strict ad-hoc signature verification PASS, external Homebrew dependency0. Raw and signed engine hashes are actual build outputs in BUILD-RESULT.json.

Runner changes are14 literal tokens for binary pins, output/arm names and descriptions. Entire AST matches after this declared mapping;10 core functions unchanged. Controller/parser/pacing/preflight/measurement/ownership/cleanup and S01/G01=1 both remain intact. Baseline is signed R02, not G01.

Fresh suites r03_validation01 and r03_clean01 are prepared:V_R03, thenA1_R02 B1_R03 B2_R03 A2_R02 A3_R02 B3_R03. All13 fixture input hashes in each of7 arms match prior R02. LAUNCH hashes and full plans are included. No consumed ownership/launch/result claim exists. Do not restage.

Use observed540..1740, nominal40s/40bins, unchanged pacing. Legacy LAUNCH measurementWindow600..1800 and its old controller hash are stale planning metadata; authoritative staged controller/checksums and separate WINDOW-CONTRACT specify the actual inputs. Native sample/extent receipt limitation also remains documented; next runtime owner must check actual raw markers without changing this frozen runner.

All owned build processes/workers ended. No awake keeper or GPU work created. Accepted app/main/releases/Homebrew unchanged. Existing upstream credits, including Beherith original shader/MIT and engine GPL, and recorded tomjn contributions are preserved; see UPSTREAM-CONTRIBUTIONS.json.

JINI must review these actual hashes/closure and issue a separate validation/clean handoff before any game. Performance gain is unmeasured.
