# Results and limits — keep the populations separate

## P: invocation-local screen state setter elision

| Clean run | Mode | FPS | 1% low FPS |
| --- | --- | ---: | ---: |
| A1 | OFF | 184.559223 | 114.021571 |
| B1 | ON | 184.640773 | 136.531365 |
| B2 | ON | 184.836145 | 140.417457 |
| A2 | OFF | 184.840763 | 138.317757 |
| A3, separate requested supplement | OFF | 184.588837 | 133.333333 |

Original ABBA pooled OFF 184.700000 / ON 184.738460 FPS (+0.020823%). This does not establish a mean FPS gain. A1's lower tail is retained: 49 intervals at least 8ms, compared with 13 in A2 and 15 in A3; two distinct clusters, not one combined burst. A3 is not pooled into the original ABBA. P was selected for subsequent private work because actual redundant setter work was reduced with representative still/Metal checks. Compiled default remains 0; private override is ELISION1.

The initial power-mode prelaunch stop ran zero games. The separate correctness run used power mode 1 and reported raw 68.393 FPS, excluded from performance. Four clean games used AC mode 0, approximately 13.97s common warmup and 40s counting. All slow valid runs remain. Representative still inspection is not numerical or all-frame parity; pre-existing rectangular/blur bands remain.

## Q: generated shared Sepia testbed

| Arm | What was observed | Limit |
| --- | --- | --- |
| F1 old MSL | [Default image](evidence/Q/measurement/functional/preparation/runs/F1_OLD_SEPIA/v2-old-identity.png) | Both comparators share the new linking/capture frontend. |
| F2 shared MSL | [Default image](evidence/Q/measurement/functional/preparation/runs/F2_SHARED_SEPIA/v2-shared-identity.png), shared identity module loaded | Attempted DisableWidget through an unavailable user-widget proxy timed out; no same-session relink success. Original evidence remains. |
| F3 shared MSL | [Changed image](evidence/Q/measurement/functional_recovery/preparation/runs/F3_SHARED_SEPIA/v2-shared-variant.png), fresh private profile exposure=1.25, bias=(0.1,0.2,0.3) | Only initial values across fresh runs are proven. |

The actual Q source is the 29-path integrated version after a reviewed 28-path packet: the restricted typed GLSL AST generates MSL, typed metadata and Lua. Exposure default 1, bias default zero; the current block is 160 bytes and slot 1 agrees throughout. This is not a general GLSL/runtime compiler. The private GetConfigFloat module overrides the returned uniforms table; generated stage strings and production defaults remain unchanged. Direct native per-call Link/SetUniform/Delete tracing, hot reload/relink, exact pixel equivalence, FPS gain and a whole-old-production comparator are unproved. Sepia stays product-default OFF.

The separate unit/originalPass0 CUS GPU-binding-reuse prototype has BAR_METAL4_SHARED_CUS_PILOT=0 throughout Q/R/S. Actual activity and net performance are unproved, and the prototype adds per-draw material/vector/allocation work. It is not S's CPU fact cache.

## R: choosing the active CPU question

P2 is a valid 15-second all-thread Time Profiler export: 24,744 owned Running rows, 24,133 symbolized leaves (97.5307%). OriginalModelImage has 323 inclusive witnesses, 265 overlapping CUS; CUSPrepare has 942. These are statistical samples, not call counts or exclusive durations. Nested scopes overlap and must not be summed.

P1's parent killed the profiler after a 28-second finalization deadline, before valid template persistence; export failed with MissingTemplate (exit 10). P2 kept collection at 15 seconds and raised only finalization wait to 180 seconds. Both the failed artifact checksums and recovery/parser are retained. Terrain-height cost was not measured enough to select it. This selection does not settle larger GPU retirement/encoding costs.

## S: positive CPU model-type facts, same binary ABBA

| Clean run | Mode | FPS | 1% low FPS |
| --- | --- | ---: | ---: |
| A1 | OFF | 185.009245 | 134.545455 |
| B1 | ON | 184.059198 | 115.444618 |
| B2 | ON | 185.176848 | 135.623870 |
| A2 | OFF | 184.795383 | 124.369748 |

Arithmetic mean OFF **184.902314** / ON **184.618023** FPS (**−0.153752%**); duration-weighted 184.902310 / 184.618072. Pair differences are −0.950047 and +0.381465 FPS. B1 is valid and retained. With two runs per mode, FPS improvement, statistical equivalence and absence of regression are not established. This compares the S flag in the same Q-containing S binary, not whole S against public v1.11.3 or P.

Sampled preparation jobs retain 218 provider requests/job. Positive model casts are 59 → 18/job, with 41 hits/job skipping 410 RTTI operations. Admission requires the successful real final Model cast and valid pixels; entries retain weak exact-owner facts, not GPU image/readiness/completion verdicts. Original Texture/sampler/AddAllocation/requireReadable/order/lifetime work remains. The bounded 128-entry scope adds lookup and stack-storage costs.

Actual-class ASan/UBSan checks include ABA/address/control-block/expiry/cap/fallback and 1,000 hits with no allocations. Two diagnostic games had no detected Metal errors, trace drops, invalid CPU stamps or scope mismatches; representative stills showed no new visible issue. D0 transitioned power mode 1→0, D1 was mode 0, so diagnostic CPU-time comparison is invalid. Provider wallNs=0 is **UNAVAILABLE**, because the compiled Clock is off; it is not zero cost.

The user policy adopts the verified work-reduction structure with MODEL_TYPE_FACTS1 for subsequent private experiments; compiled default remains 0, and the observed negative mean is preserved. No public app/performance guarantee follows. Initial power preflight stop, Q F2 failure, P1 failed profiling export and pre-adoption decisions remain in the evidence.

## Current-J background, kept separate from P/Q/R/S

The [unlocked BC213 J analysis](evidence/current-J/ANALYSIS.json) covers 43 jobs / 42 main joins. Of 92.45ms exposed CPU Join wall span, drawable is 0.3006ms (0.325%), event32.9787ms (35.67%) plus feedback2.182ms (2.36%), encode23.8328ms (25.78%), owner release6.8796ms (7.44%). These are CPU-observed wall envelopes with nested parents; do not add overlapping spans or call them exclusive GPU time. Exact trace-window foreground alignment is unverified. The diagnostic FPS parser found 41 versus required40 samples; that failure is preserved and the diagnostic is not used for FPS comparison. A [separate single clean v1.11.3 run](evidence/current-J/CLEAN-BASELINE.json) recorded184.1296FPS. It is not pooled with P/S or evidence that S solved the larger GPU-retirement/encoding question. Older locked/Pinewood traces are a different population.
