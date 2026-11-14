> Publication copy: original conclusions and historical version scope preserved. Personal absolute paths were neutralized; source links are pinned to the audited a33ebfe revision. Raw logs, JSON manifests, binaries, screenshots and the original plan are not included in this Markdown-only export. See the parent README for later runtime results and limitations.

# BAR-Metal4 전체 상위 계약 감사

2026-11-13 America/Los_Angeles · Astra ultra 주담당 + CPU/GPU 독립 검토 2명 · 읽기 전용 정적 감사

**현재 공개 `a33ebfe` 및 그와 일치하는 로컬 production 소스에서 새로 확정한 P0/P1 정확성·GPU 수명 결함은 없다. 그러나 원계획 전체 완료나 현재 앱의 실행·성능·화면 동등성을 승인할 근거도 없다.** Terrain T1–T4와 CUS roster의 핵심 생산자 변경은 실재한다. Postprocess/P2는 일부 반복 작업을 줄였고, owned packet·bounded journal·실패 격리·GPU pending 1개 제한도 구현됐다. P3–P5의 일반 비동기 실행 경계와 P6의 전체 자원 접근 증명은 여전히 부분 구현이다.

가장 중요한 구분은 **실행된 Minimal = `6e9ce8f + mac9`**, **현재 검토 소스 = `6e9ce8f + mac9 + review8 + completion2`**라는 점이다. 첫 앱의 정상 종료를 후속 앱의 통과로 전이할 수 없다. 현재 원격에는 9개 Mac 수정도 이미 포함돼 있다.

## 1. 구체적 발견과 우선순위

이 절의 P0–P3는 결함 중요도다. 뒤의 원계획 P0–P6 단계명과 구분한다. 현재 결함, 구버전에서 수정된 조건부 결함, 설계 미완료, 실행 증거 부족을 따로 표시한다.

### F1 — 구버전 조건부 P1: 외부 Runtime 제어가 미정산 worker handoff와 충돌; 현재 수정됨

**대상:** 최초 Minimal의 base+mac9. 현재 19파일/a33에는 R1 정산 경계가 있다. 실제 게임에서 이 실패를 재현했다는 주장은 아니다.

최소 성립 조건은 BeginReady ON 프레임 N이 deferred submit을 마쳤지만 main이 아직 worker task/handoff를 join·해제하지 않은 상태에서, main의 active MetalInfo 갱신 또는 unit-uniform copy가 별도 `RuntimeLease`에 들어가는 것이다. worker가 이미 CPU 실행을 끝냈어도 reservation은 정산 전까지 남을 수 있다. GPU가 물리적으로 이미 끝났더라도 deferred ticket의 CPU retirement가 남아 있으면 이 조건이 성립한다. 외부 lease가 그 ticket을 retire하면 WorldRenderer completion callback은 main에서 renderer access gate를 요구한다. gate는 main에 허가되지 않은 미해제 reservation을 거절하므로 `logic_error`와 runtime poison으로 진행이 중단되고 재시작이 필요할 수 있다. 메모리 파손이 입증됐다는 의미는 아니다. 단순 mutex 추가는 이 소유권 순서를 해결하지 않는다.

근거: base `Metal4MetalInfoBridge.mm:9–35`는 바로 lease/clear에 진입한다. 현재 [MetalInfo bridge](https://github.com/yeojuny/BAR-Metal4/blob/a33ebfe888bc8abe0053df05e5e2892c0788d6a8/engine/rts/Rendering/Metal4/Metal4MetalInfoBridge.mm#L12)는 초기화·활성 소비·reset 전에 `FinishRuntimeControl`을 호출한다. [NativeMetalInfoCapture.cpp:47](https://github.com/yeojuny/BAR-Metal4/blob/a33ebfe888bc8abe0053df05e5e2892c0788d6a8/engine/rts/Rendering/Backend/NativeMetalInfoCapture.cpp#L47)는 이 경계를 `Mutation` 및 candidate copy보다 앞에 둔다. [UnitUniformCopy.mm:52](https://github.com/yeojuny/BAR-Metal4/blob/a33ebfe888bc8abe0053df05e5e2892c0788d6a8/engine/rts/Rendering/Metal4/Metal4UnitUniformCopy.mm#L52)도 같은 순서를 지킨다. [gate:43](https://github.com/yeojuny/BAR-Metal4/blob/a33ebfe888bc8abe0053df05e5e2892c0788d6a8/engine/rts/Rendering/Backend/NativeRendererAccessGate.h#L43), [정산:55](https://github.com/yeojuny/BAR-Metal4/blob/a33ebfe888bc8abe0053df05e5e2892c0788d6a8/engine/rts/Rendering/Backend/NativeRenderJobSettlement.h#L55), [GPU callback:5566](https://github.com/yeojuny/BAR-Metal4/blob/a33ebfe888bc8abe0053df05e5e2892c0788d6a8/engine/rts/Rendering/Metal4/Metal4WorldRenderer.mm#L5566)를 연결해 확인했다.

**영향/조치:** 구버전을 P5 ON 승인 후보로 사용하지 않는다. 현재 수정은 task join → handoff 해제 → target notice 및 source retirement 정산 → 외부 제어 순서를 보존해야 한다. 최소 후속 검증은 worker 실행 중과 실행 완료·미join 상태 각각에서 active MetalInfo/UnitUniformCopy/reset을 호출하고, 이전 frame의 notice·credit가 정확히 한 번 정산되는지 확인하는 것이다. 이번 감사에서는 실행하지 않았다. 기존 worker1/pilot0 로그는 이 반례를 검사하지 않는다.

### F2 — 현재 P2: 앱 manifest와 비교 환경이 공개 재현 경계를 완성하지 못함

Review Ready manifest의 `engine.sourceCommit=937539c`는 감사 시 공개 GitHub 조회에서 422였다. 제공된 source hash표의 19파일은 현재 working 및 공개 a33과 모두 일치하지만, 해석할 수 없는 short commit과 과거 checkpoint/source manifest만으로 현재 source closure를 설명할 수는 없다. 이는 확인된 provenance 결함이며, 잘못된 binary 또는 실행 실패를 뜻하지는 않는다.

또한 실제 build.ninja:40219 (`LOCAL-EVIDENCE/build.ninja`; not included in this Markdown-only export)의 WorldRenderer recipe는 `-O2 … -ffp-contract=off … -O3 -ffp-contract=fast`다. `BAR_NATIVE_INTERVENTION_RENDERER_OPTIMIZE=ON`이므로 마지막 설정이 중요하다. 런타임 옵션 OFF로 이 compile/math profile이 원복되지 않는다. 선택한 최종 link recipe에는 `-flto`가 없다. wrapper는 measured233의 11flags나 accepted245의 13flags를 완전히 고정하지 않으며 BeginReady도 상속값을 허용한다. 두 앱의 직접 Homebrew dylib 의존성과 content 의존성이 남고 전체 전이 의존성 bytes는 미고정이다.

**최소 반례:** 공개 branch만 내려받아 현재 도구체인·Homebrew로 빌드하거나 runtime flag만 OFF한 결과를 원래 측정 control과 같다고 부르면, source identity, compiler math, UI/환경 또는 dependency bytes 중 하나가 달라져도 발견하지 못한다.

**수정 방향:** 다음 패키징에서 full public commit + local patch/tree digest + binary digest + 실제 compile/link flags + SDK/minimum OS + UI/game/map/dependency receipt를 하나로 연결한다. 비교 profile은 환경 상속을 제거하고 명시적으로 고정한다. source 수정과 compiler 변경의 성능 효과를 분리한다. 전체 값과 앱별 경로는 [VERSION-MATRIX.md](VERSION-MATRIX.md), SOURCE-BINARY-PATCH-MANIFEST.json (`LOCAL-EVIDENCE/SOURCE-BINARY-PATCH-MANIFEST.json`; not included in this Markdown-only export)에 기록했다. 원본 manifest는 수정하지 않았다.

### F3 — P2 승인 공백: BeginReady는 prepare 완료 후 Encode overlap pilot이며 원계획 P5 완료가 아님

[DrawDispatch:2381](https://github.com/yeojuny/BAR-Metal4/blob/a33ebfe888bc8abe0053df05e5e2892c0788d6a8/engine/rts/Rendering/Metal4/Metal4WorldRenderer.mm#L2381)는 이전 CPU 작업을 정산한 뒤 owned task를 발행한다. BeginReady receipt는 [DrawPrepared:4445](https://github.com/yeojuny/BAR-Metal4/blob/a33ebfe888bc8abe0053df05e5e2892c0788d6a8/engine/rts/Rendering/Metal4/Metal4WorldRenderer.mm#L4445)에서 준비 작업을 끝내고 실제 Begin/active source/target notice를 설정한 뒤 발행된다. main이 일찍 돌아올 수 있는 구간은 이후 Encode 구간이다. BeginReady 발행 뒤 encode-work scope가 시작되므로 Encode 후반만 겹친다는 뜻은 아니다. 이것은 단순한 모든 경로의 `Invoke.get`만은 아니지만, N의 전체 prepare와 N+1 capture가 겹치는 일반 P5도 아니다. eligible하지 않은 경로는 동기 정산한다.

`result.presented=true`는 [deferred submit:5704](https://github.com/yeojuny/BAR-Metal4/blob/a33ebfe888bc8abe0053df05e5e2892c0788d6a8/engine/rts/Rendering/Metal4/Metal4WorldRenderer.mm#L5704)의 예약된 presentation 의미이며 실제 drawable 표시 시간을 수신했다는 증거가 아니다. 현재 Metal4 경로에서 `addPresentedHandler`/`presentedTime` 연결을 찾지 못했다. `SceneReportPresented`와 기존 로그의 count를 actual-presented throughput으로 사용해서는 안 된다.

**최소 반례:** enqueue/Begin 반환 횟수만 증가하거나 capture가 worker active 작업 시작 전에 끝난 경우에도 `CaptureBegin(N+1) < EncodeEnd(N)`는 성립할 수 있다. 이 식 하나로 CPU overlap 또는 평균 표시 FPS 향상을 주장할 수 없다.

**수정/검증 방향:** 현재 좁은 pilot을 유지한 채 허용된 후속 실행에서 capture 및 worker active interval 양끝의 양수 교집합을 확인한다. Prepare/Encode 내부 CPU·driver wait를 구분하고, 동일 frame identity의 accepted/committed/GPU-complete/actual-presented 연결과 평균 FPS를 함께 확인한다. 기존 trace의 wall interval만으로 순수 active work 전부가 증명되지는 않는다. 이번에는 계측을 새로 만들거나 실행하지 않았다.

### F4 — P2 설계 미완료: Postprocess/P2의 제거 목표 일부가 남음

Postprocess는 [NativePostprocessData.cpp:283](https://github.com/yeojuny/BAR-Metal4/blob/a33ebfe888bc8abe0053df05e5e2892c0788d6a8/engine/rts/Rendering/Backend/NativePostprocessData.cpp#L283)의 exact owner/control-block 기준 **현재 활성 root** plan을 재사용한다. same-serial prefix/suffix가 다른 root면 재구축하므로 잘못된 serial-only cache가 아니다. 그러나 A→B→A root 교체는 A의 plan을 다시 만들며, snapshot lifetime 전체에서 build 1회라는 원계획을 일반적으로 충족하지 않는다. [Metal4PostprocessNative.mm:580](https://github.com/yeojuny/BAR-Metal4/blob/a33ebfe888bc8abe0053df05e5e2892c0788d6a8/engine/rts/Rendering/Metal4/Metal4PostprocessNative.mm#L580)의 descriptor/overwrite 관련 반복 작업도 일부 남는다. runtime producer set 반복은 [NativePostprocessData.cpp:318](https://github.com/yeojuny/BAR-Metal4/blob/a33ebfe888bc8abe0053df05e5e2892c0788d6a8/engine/rts/Rendering/Backend/NativePostprocessData.cpp#L318)에 별도로 남는다.

CUS는 roster 및 selected envelope 공유가 구현됐지만 [NativeCUSSourceData.cpp:28](https://github.com/yeojuny/BAR-Metal4/blob/a33ebfe888bc8abe0053df05e5e2892c0788d6a8/engine/rts/Rendering/Backend/NativeCUSSourceData.cpp#L28)의 full draw structural validation이 Submit/CPU/GPU/deferred 경로에 남고, [NativeCUSProgramData.cpp:27](https://github.com/yeojuny/BAR-Metal4/blob/a33ebfe888bc8abe0053df05e5e2892c0788d6a8/engine/rts/Rendering/Backend/NativeCUSProgramData.cpp#L27)의 이름/shape 기반 typed packing도 매 draw 수행된다. 기존 CPU snapshot 공유를 “전체 P2 완료”로 부를 수 없다.

**영향/수정 방향:** 이것은 현재 오답/크래시 반례가 아니라 남은 CPU 작업이다. exact immutable owner의 static proof와 lease-local runtime readiness/retention을 분리해 단계별로 줄인다. runtime readability/등록을 static plan으로 생략하지 않는다. MRT sampler leaf 강소유를 유지하고 반대 phase의 부모 frame 전체를 pin하지 않는다. 최소 검증은 A→B→A·prefix/suffix/cancel에서 build 수, lookup sort 수, validation/packing 횟수와 retained owner를 확인하는 것이다. 미측정 FPS 이득을 부여하지 않는다.

### F5 — P2 설계 미완료: P6 전체 resource-use proof가 없어 GPU 확대 승인 불가

현재 [RuntimeNative.mm:890](https://github.com/yeojuny/BAR-Metal4/blob/a33ebfe888bc8abe0053df05e5e2892c0788d6a8/engine/rts/Rendering/Metal4/Metal4RuntimeNative.mm#L890)는 무조건 이전 GPU submission을 retire한 후 다음 submit을 허용한다. 원계획의 `priorReadChecks.empty()` 기반 two-ticket 반례 진입 조건은 제거됐다. Postprocess pending encoded borrow도 exact owner+completed flag를 [483행](https://github.com/yeojuny/BAR-Metal4/blob/a33ebfe888bc8abe0053df05e5e2892c0788d6a8/engine/rts/Rendering/Metal4/Metal4PostprocessNative.mm#L483)에서 등록한다.

이 보완을 전체 producer/storage version/view/subresource/read/write/preserve/order manifest로 확대 해석할 수 없다. `Unknown/NoCommands/CommandsRequired`와 현재 좁은 clear/overwrite proof는 일반 dependency proof가 아니다. 따라서 barrier·store/load·preserve-copy 삭제 또는 GPU pending 2개 승격을 현 상태에서 승인할 수 없다. 현재 코드가 항상 잘못 동작한다는 발견은 아니다.

**최소 반례/금지할 후속 변형:** sampler reader의 강소유를 writer `U.current/F.previous`의 raw 참조로 바꾸면 `use_count==2`인데도 과거 content reader가 존재할 수 있다. 현재 roster/index 변경에서는 그 소유자 제거가 발견되지 않았고, async requested pilot은 MRTFrontier를 생성하지 않아 COW로 간다. 새 raw alias를 도입할 때부터 성립하는 반례다. 최소 검증은 한 가족의 exact producer·view·old-pixel reader·completion을 먼저 닫고, 이후 depth/stencil/Lua consumer까지 같은 출력인지 확인하는 것이다.

### F6 — P3 API 가설: 비불변 Terrain consumer의 Supports 변경

동일 값+aligned publication fast path는 consumer validation 재호출 전에 반환한다. custom nonimmutable consumer가 generation 변화 없이 `Supports=true→false`로 바뀌면 실패가 write에서 capture로 늦어질 수 있다. 실제 production consumer는 immutable linked certificate 계약이므로 현재 도달 가능한 결함으로 분류하지 않는다. generic API를 유지하려면 지원 불변성 계약 또는 guard를 명시하고 작은 host 반례로 고정할 수 있다. 상세 조건은 [CPU-AUDIT.md](CPU-AUDIT.md) 1절에 있다.

## 2. 버전·실행 증거 대응

| 대상 | 확인한 정확한 identity | 이번 감사 판정 |
|---|---|---|
| 감사 branch | `a33ebfe888bc8abe0053df05e5e2892c0788d6a8` | 로컬 base보다 1commit, 34파일: production19/docs7/tests8 |
| 원격 main | `7f37af54f8a95926a8474f8acb595a68e98ae50a` | 계획 원문 및 19대응 파일 대조; main 전체 감사 아님 |
| 로컬 base | `6e9ce8f89afb14d632cce33e4742bab11b76b60f` | production19 + vendor9 dirty 그대로 보존 |
| 최초 Minimal source | base + mac9 (`c7dfd475…`) | 현재 remote와 동일하다고 취급하면 안 됨 |
| 후속 source | base + mac9 + review8 + completion2 | 메모리 patch 합성이 working19 및 remote19와 모두 byte-exact |
| cloud 기록 | `937539c` | public commit 확인 불가; 제공19hash만 독립적으로 일치 확인 |
| 최초 signed engine | `e1d1d12e…` | 실제 app-path worker1/pilot0 과거 로그 있음 |
| 후속 link engine | `1df4cf75…` | 기존 configure/link 로그와 현재 파일 identity 기록 |
| Review Ready signed engine | `2f00fd56…` | 최초 앱과 별개; 문서상 아직 실행 안 함 |

패치별 전체 파일명·SHA256, 28 dirty 파일의 base/work SHA256, UI SHA256/SHA512, 서명 출력, wrapper, compile/link recipe, runtime log hashes는 manifest에 있다. `review-source-completion.patch`의 2파일은 **Metal4PostprocessNative.h와 Metal4WorldRenderer.mm**이다. PostprocessNative.mm은 review8에 속한다. “9+19”개 독립 변경이 아니라 base 대비 최종19개다. vendor9은 CRLF 정규화 후 동일하며 production에 포함하지 않았다.

계획 원문은 main 고정 commit의 blob `f3d256edc12bec6964065095d4e409412eb88a87`, UTF-8 220,082 bytes/2,116 lines다. 원문 사본 (`LOCAL-EVIDENCE/PLAN-original.md`; not included in this Markdown-only export)의 I/II 및 부록 A/B/C를 모두 읽고 현재 경로와 대조했다. 원문의 main5bc9/reviewdb4725/intervention16e7 및 native233 overlay31은 역사적 분석 고정점으로 유지한다.

| 증거 단계 | Minimal | Review Ready/현재 source | 해석 한계 |
|---|---|---|---|
| compile/link | 기존 build-fix4 성공 기록 | 후속 complete 로그의 link 성공 기록 | 이번 감사에서 재빌드 안 함 |
| 서명 | engine 및 outer deep/strict 검증 성공 | engine 및 outer deep/strict 검증 성공 | source 재현 또는 gameplay proof 아님 |
| `--version` | manifest의 과거 문자열 | manifest의 과거 문자열 | 이번 감사에서 binary 실행 안 함; fresh receipt 없음 |
| app-path/gameplay | 기존 sim616·sim2002 사용자 종료와 retirement | 실행 증거 없음 | pixel parity·전체 기능 수락 아님 |
| P5 ON | 없음; 실제 로그 `main_wait=1, simulation_overlap=0` | 없음 | prepare 이후 Encode overlap 미측정 |
| Metal Validation | wrapper 기본 0 | wrapper 기본 0 | smoke를 Validation PASS로 전환 불가 |
| 현재 평균 FPS/동일 출력 | controlled A/B 없음 | controlled A/B 없음 | 과거 다른 binary의 수치 전이 금지 |

Minimal lobby log의 3,605/10,619 presented-count는 source상 scheduled presentation 회계다. 실제 drawable 표시 횟수/시간 증거로 쓰지 않는다. 실제 테스트는 기존 test31691·Seths Ravine 경로이며 옛 고정 Direct50 benchmark와 동일 run이라고 간주하지 않는다.

## 3. A — 상위 소유권·상태·실제 호출 순서

현재 행 번호는 원본 SRC의 working source를 가리킨다. 변경 production 파일은 공개 a33과 일치하고 나머지는 base와 같으며, 원격 후속 docs/tests는 별도 사본으로 읽었다.

| 순서 | 실제 호출 경계 | 소유권·검사 |
|---|---|---|
| 1 | SpringApp Run/Update → controller Update/Draw → SwapBuffers (`SpringApp.cpp:933,993`) | simulation/live Lua는 main에 남음 |
| 2 | `Game.cpp:1529–1532` Draw → UpdateUnsynced(정의1301) → capture1542 | camera/model uniforms 갱신을 한 번 수행. worker에서 source callback 재실행 안 함 |
| 3 | `Game.cpp:1542` source credit → UI/Postprocess/Cloud begin → map/Genesis/environment/world capture | 원래 producer와 Lua stage 순서 보존 |
| 4 | `Game.cpp:1754–1771` EndFrame/seal → SceneBridge DrawWorld | captured world/screen/CPU read receipt 소유권 이전 |
| 5 | `SceneBridge.cpp:2094–2165` owned input → mailbox reserve → journal transfer → DrawOwned | queue 공개 전 stable admission owner와 held journal을 준비 |
| 6 | `WorldRenderer.mm:2381–2467` 이전 CPU 정산 → owned task + reserved gate → worker | 1 outstanding task, source는 최대2credits; GPU pending수와 별개 |
| 7 | `DrawPrepared:2480–4445` captured context/read receipt → RuntimeLease → prepare → Begin | mutable renderer는 gate/ordered control로 직렬화; BeginReady는 여기 이후 |
| 8 | encode → `SubmitAndDefer:881–980` | 이전 GPU 완결 후 다음 submit; command recording slot은 별개 |
| 9 | event + feedback → producer callbacks → data notice | GPU 완료와 CPU task 반환을 분리 |
| 10 | main mailbox ApplyDeferredReceipts (`SceneBridge.cpp:905`) | exact draw/post/water identity 검증 → 지정 journal retire → held source credit 반환 |
| 11 | presentation request와 report counter | actual display notification은 미완성 |
| 12 | reset/shutdown drained control | CPU 정산·GPU safe detach·notice 처리 뒤 state clear |

### seal·recorder 재사용·관측 경계

[NativeOwnedScreenFrame.h:17](https://github.com/yeojuny/BAR-Metal4/blob/a33ebfe888bc8abe0053df05e5e2892c0788d6a8/engine/rts/Rendering/Backend/NativeOwnedScreenFrame.h#L17)의 seal은 geometry root를 move하고 target descriptors를 소유한다. UI EndFrame은 다음 writable storage로 넘어가며 cache 재사용은 sole-owner 조건을 요구한다. [Metal4WorldRenderer.h:106](https://github.com/yeojuny/BAR-Metal4/blob/a33ebfe888bc8abe0053df05e5e2892c0788d6a8/engine/rts/Rendering/Metal4/Metal4WorldRenderer.h#L106)의 owned input은 root/span 관계, borrowed readback 금지, PBR/deferred-particle capture 완료를 검증한다. 이 검사는 모든 source family의 전이적 불변성을 한 번에 증명하는 universal seal은 아니다.

Terrain `Current/SaveCurrent`는 dirty builder 관측 시 freeze하며 source writer의 내부 identity 조회는 매 setter seal하지 않는다. `Current(A)→Write(B)→Restore(A)→Write(B)`는 aligned no-op 조건을 만족하지 않아 fallback한다. reload/deletion/address reuse는 live statement·generation/certificate로 분리한다. CUS의 old roster는 Delete/부분 mutation 이후도 captured draw에서 유지된다. 자세한 반례와 행 번호는 CPU 부록에 있다.

### journal·credit·notice의 정확한 소유권

[ScenePendingAdmission:789](https://github.com/yeojuny/BAR-Metal4/blob/a33ebfe888bc8abe0053df05e5e2892c0788d6a8/engine/rts/Rendering/Metal4/Metal4SceneBridge.cpp#L789)은 heap-stable owner, context/epoch, exact packet/ticket/source lease를 갖는다. Transfer는 검증 뒤 source credit Hold와 pending journal 이동을 수행한다. 작업이 Begin 전에 실패했고 destination이 그대로일 때만 restore한다. Begin 후 실패는 dependency lineage를 quarantine하며 younger capture 위에 이전 journal을 덮어쓰지 않는다.

[NativeSourceFrameCredits.h](https://github.com/yeojuny/BAR-Metal4/blob/a33ebfe888bc8abe0053df05e5e2892c0788d6a8/engine/rts/Rendering/Backend/NativeSourceFrameCredits.h#L10)는 capturing/worker/GPU-owned를 함께 최대2로 제한한다. Join이나 Accept만으로 credit를 돌려주지 않고 exact GPU receipt의 main retirement까지 held owner를 유지한다. capture ordinal은 simulation/draw 숫자와 분리돼 감소·반복되는 simulation time을 lineage 순서로 오해하지 않는다. 동시에 살아 있는 같은 draw의 중복 admission은 거절한다.

[mailbox:23](https://github.com/yeojuny/BAR-Metal4/blob/a33ebfe888bc8abe0053df05e5e2892c0788d6a8/engine/rts/Rendering/Backend/NativeFrameCompletionMailbox.h#L23)는 capacity2, 별도 retained mailbox, ticket owner+epoch+draw+post를 확인한다. 완료가 Accept보다 빨라도 Pop은 수락 전 retirement를 허용하지 않는다. 중복 publish 및 Pop 이후 replay도 거절한다. Close된 옛 mailbox는 Session을 직접 건드리지 않는다. new game마다 무조건 epoch가 증가한다는 보장은 이 소스에서 주장하지 않는다. 일부 reset은 같은 context/mailbox를 유지하지만 먼저 drain하여 미처리 notice가 새 상태로 넘어가지 않게 한다.

### main/render 경계와 TLS

CPU read packet은 explicit read view로 worker에 전달된다. phase trace도 parent/sample identity를 `CaptureCPUPhaseContext`로 전달하며 TLS pointer 자체를 다른 thread에 빌려주지 않는다. worker final captures의 파괴는 task join/reset 경계에서 다루며 GPU-retained owners는 실제 retirement까지 별도로 남는다.

ordinary owned path는 environment receipt, captured source context, shadow fallback owner, screen coverage ticket, PBR/deferred particle captures를 사용한다. live `OriginalNative*`/global state fallback은 main/direct path로 제한돼 있다. capture의 mutable cache 필요 작업은 ordered control 또는 drain을 유지한다. 따라서 broad `MainThread()`가 worker TLS도 허용한다는 이름만 보고 모든 호출이 안전하다고 판정하지 않았다.

renderer access gate는 동시 호출을 기다려서 해결하는 범용 mutex가 아니라 reservation/owner를 검증하는 fail-fast 경계다. shader/texture compile·delete, coverage, reload 등의 control은 CPU settlement 후 runtime/gate에 진입한다. 읽은 경로에서 worker가 main의 임의 Lua callback을 동기 요청하고 main이 worker를 기다리는 역방향 cycle은 확인하지 못했다. 임의 direct readback/continuation callback이 있는 경로는 pilot에서 제외되거나 drained direct path다. 이것이 모든 향후 callback에 대한 무교착 증명은 아니다.

### 제어·실패·중단 경로

| 사건 | 현재 경계와 판정 |
|---|---|
| current N+1 capture abort | Game catch가 CPU read 작업을 마친 뒤 current recorder abort. worker가 받은 N의 exact journals는 별도 owner |
| shield abort와 encoded 상태 | `AbortShieldPublications:2183`는 먼저 Try gate. 실패하면 false로 owner 보존; 성공한 경우에만 non-atomic encoded 상태 접근. 항상 join한다고 해석하면 틀림 |
| Begin 이전 worker 실패 | admission 상태를 검사하여 취소/복원. queue 공개 이후에는 current recorder reset으로 대체하지 않음 |
| Begin 이후 실패 | active source 및 후속 lineage quarantine, 재시작 필요. 성공 credit 반환 없음 |
| wrong epoch/duplicate/accept 이전 notice | mailbox가 거절 또는 수락까지 보류; 정확한 journal만 적용 |
| SaveImage prefix/suffix | `SceneBridge.cpp:1866` direct current prefix 후 drain; same serial의 다른 root/selection을 구분. async arbitrary callback으로 바꾸지 않음 |
| resize/close/surface 변경 | event drain + `Metal4CaptureSurfaceBoundary.h` actual extent/drawable/sync 비교; 변화·관측 실패 시 drain/refresh. retained layer만으로 안전을 가정하지 않음 |
| capture 예외 | surface boundary helper가 CPU 작업 drain 후 예외 전달 |
| minimized/hidden | target-only source plan 유지. offscreen에서도 실제 texture/target producers를 지우지 않음 |
| dynamic/legacy water | BeginReady eligibility에서 제외; resource/mapping 변화는 drain 경계 |
| reset/newgame/reload | `SceneBridge.cpp:2218` reset 전에 drain; Game teardown/reload도 FinishPendingFrame |
| shutdown | `SceneBridge.cpp:2332` drain 후 연결 해제; unsafe GPU detach이면 terminate. timeout을 정상 해제로 위장하지 않음 |

## 4. B — GPU 수명·순서·재사용

상세 resource별 표와 조건은 [GPU-AUDIT.md](GPU-AUDIT.md)에 있다. command allocator, upload storage, published buffer, argument table, residency, request texture, postprocess/pool 및 original-lifetime closure를 추적했다.

정상 submit은 pending graph를 commit 전에 공개하고, feedback callback은 그 graph를 retain한다. retirement는 event와 feedback을 모두 기다린다. `pending=false` → GPU error 검사 → allocator reset → 실제 producer callbacks → 성공 notice/pop 순이다. timeout이면 pending owner와 failedFrame을 보존하고 allocator reset·성공 source retirement에 도달하지 않는다. GPU error는 allocator reset 전에 중단한다. producer callback 오류는 GPU가 이미 끝났으므로 allocator가 reset됐을 수 있지만 runtime을 poison하고 전체 frame의 성공 완료는 발행하지 않는다. 일부 producer가 이미 완료된 상태를 rollback했다고 주장하지 않는다.

현재 GPU pending limit은 **1**, UI/Postprocess/Cloud와 mailbox/source credit의 bounded ledger는 **2**, runtime의 independent recording slots는 그와 별개의 정책이다. 서로 다른 수를 “두 GPU frames”로 합쳐 읽지 않는다. 과거 two-ticket 조건부 충돌은 현재 무조건 predecessor retirement로 차단됐다.

pool 재사용은 단순 Objective-C retain 여부가 아니라 frame graph가 보유한 C++ slot pin, completion, logical weak owner 만료 등 각 pool 조건으로 막힌다. shared_ptr는 객체 생존만 증명하며 GPU visibility나 과거 content overwrite 안전성까지 증명하지 않는다. Metal 4 command buffer가 resource를 retain하지 않으며 allocator reset은 GPU 완료 이후여야 한다는 계약을 기준으로 대조했다. [Apple Metal 4 core API](https://developer.apple.com/documentation/metal/understanding-the-metal-4-core-api)

MRT209의 현재 witness는 U.current/F.previous 구조와 independent sampler leaf 강소유를 포함한다. 새 roster는 geometry-only, index는 root+numeric refs, 공유 uniform content는 숫자 data로 이 reader witness를 없애지 않았다. async requested pilot은 MRTFrontier를 만들지 않고 readback 경로에서도 frontier를 만들지 않으며, direct 경로에 previousRetired callback이 있으면 frontier를 제거한다. 따라서 현재 변경이 raw-alias overwrite를 일으킨다는 실행 가능한 반례는 찾지 못했다.

P6 판단에는 attachment load/store, partial copy, later depth/stencil 및 Lua read를 포함했다. existing exact-empty 효과와 clear/full-overwrite proof 이외에 “보수적 barrier니까 제거” 결론을 내릴 근거는 없다. Metal 4 queue의 동기화는 hazardTrackingMode에 맡길 수 없다는 공식 계약도 적용된다. [Apple resource synchronization](https://developer.apple.com/documentation/metal/resource-synchronization)

## 5. C — 실제로 사라진 CPU 작업과 남은 비용

| 변경 | 정적으로 확인한 감소 | 남은 작업·제한 |
|---|---|---|
| T1 exact change | aligned same-value write에서 새 vector/map/publication 및 revision 증가 없음 | stale binding이면 fallback; 실제 workload hit율 미측정 |
| T2 linked certificate | source provenance와 linked ABI proof 공유; mutable PSO maps의 source-side 반복 관찰 분리 | actual target/format/MSAA/image/PSO 검사는 prepare에서 계속 필요 |
| T3 lazy freeze | 첫 변경 때 writable map clone, dirty 연속 write에 재사용, 관측 시 seal | 마지막 seal 이후 첫 changed write의 clone과 snapshot 비용 남음; 한 frame에서 여러 clone 가능 |
| T4 typed source base | 44 typed slots 및 source packing 재사용 | late camera/inverse/clip/fog/target pack은 유지; +992bytes 정적 owner 구조가 실제 RAM 증가량은 아님 |
| CUS roster | 성공 mutation 뒤 첫 Submit seal, unchanged roster 공유, 반복 geometry proof/배열 복사 제거 | per-event wrapper, pose/instance/texture event는 독립 갱신 |
| CUS read-set key | exact roster/program/transforms/model/engine uniforms/instances tuple로 wrapper 주소 churn 분리 | team255, bone, scale/interpolation, observed row branch 계속 필요 |
| CUS uniform/phase | unchanged numeric content 공유; selected event envelopes 사용 | setter 임시 vector, full draw validation, typed CUS pack, 일부 engine constants copy 남음 |
| Postprocess index | active exact root에서 static structural validation/order 재사용, lookup마다 full sort 제거 | root 전환 rebuild, runtime producer/descriptor/registration 일부 남음 |
| R5 census | 한 Prepare census를 phase2/3에서 재사용 | census 자체는 실제 Prepare CPU 작업; 앞당긴 allocation 실패 시점은 명시해야 함 |

R5는 CUS census를 prepare CPU 측정 scope 안에서 수행하므로 cost를 계측 밖으로 옮겨 사라진 것처럼 만들지 않는다. companion 변경은 Postprocess exact readability receipt와 필요한 callsite들을 연결한다. current/prefix/suffix root, version/completion, cancellation/readability를 확인하며 dynamic readiness를 static plan hit로 생략하지 않는다. 자세한 transaction 순서와 테스트 receipt hash 일치 범위는 [CPU-AUDIT.md](CPU-AUDIT.md) 3–6절에 있다.

CPURead·encode를 worker에 옮기는 것은 작업 이동, positive overlap은 숨김, roster/map/order 작업 제거는 실제 삭제다. 이 효과들을 같은 “ms 절감”으로 합산하지 않는다. T1 stats는 compile-time guard로 ordinary build에서 사라지지만 모든 기존 runtime-OFF 진단의 lookup/branch/counter까지 zero-cost라고 할 수는 없다.

### 성능 수치의 사용 한계

현재 audit source/P5/R1–R5에 대한 새 평균 FPS 수치는 없다. 승인 baseline은 WorldInfo245 binary `95929a2a…`+13flags다. RTT244의 4.31MB/frame 논리적 copy 제거, 245의 별도40초 1,199개 texture allocation 제거와 평균 보유량 약+14.8MiB는 해당 기존 실험의 결과로 보존한다. key246의 68요청/frame 제거와 평균 약−0.15%는 미승격 별도 후보 결과다. 누적 allocation payload를 실제 RAM/DRAM 절감량으로 바꾸지 않는다.

238의 약5.776ms app gap/5.783ms drawable reported gap와 prior GPU end→commit 약0.806ms는 해당 diagnostic run의 관측이다. 그 안의 prepare 약0.261ms·encode 약0.448ms를 제거 가능한 확정 비용으로 간주하거나 다른 run과 합산하지 않는다. drain236의 중간 drain0/3,461frames 및 event-feedback wait 약0.043708ms도 해당 run 범위다. 옛 16e7 intervention166.02/native245174.40/MVK3222.42FPS는 각각 단1회 비교로 현재 후보의 성능이 아니다.

평균 FPS를 주지표로 두고, FPS 중립이어도 확인된 copy·allocation·prepare 감소는 유지한다. 1% low 하락 하나로 자동 기각하지 않는다. tails/input latency/retained memory는 원인과 tradeoff를 밝히는 보조 지표다. 현재 자료로 230FPS나 MVK3 도달 시점을 예측하지 않는다.

## 6. D — 9개 Mac 수정과 후속 패치의 의미

3개 backing의 `atomic<shared_ptr>` → private shared_ptr + atomic free functions 변경은 acquire load / release 성공 CAS / relaxed 실패 CAS / empty-expected one-shot publication을 유지한다. resource는 private이고 확인한 동시 접근은 atomic 함수로 통일됐다. null·중복 fulfill 거절과 기존 owner retention을 보존한다. 이식 전후 lock-free 성질이나 모든 type trait까지 같다고 주장하지 않는다.

`DeleteShader`의 `uniforms.Initialize({},{})`는 새 uniform builder·cached sealed owner를 정리하며 이전 draw가 소유한 content를 파괴하지 않는다. `MRTFrontier209*` friend 정정은 실제 선언과 일치시키며 새 runtime overload를 만들지 않는다. `<exception>` 보충은 `terminate` 선언을 직접 확보한다. 9개를 이름만 보고 compile-only로 승인하지 않고 이 동작을 대조했으며, 현재 production 의미 차이는 발견하지 못했다.

후속 R1은 F1의 외부 control settlement, R5는 census timing/재사용, Postprocess 변경은 base에 있던 exact pending-input receipt를 유지하며 `CaptureReadability`와 세 callsite의 owned readability 연결을 보완한다. final19의 byte correspondence는 정확하지만 기존 Minimal 실행을 final19 검증으로 올릴 수는 없다. 전체 빌드 환경·서명·UI·Homebrew 경계는 F2 및 VERSION-MATRIX에 별도 기록했다.

## 7. 원계획 P/T/U별 구현·검증 상태

“구현”은 해당 좁은 정적 계약의 구현이며 현재 전체 앱 실행 통과를 의미하지 않는다. 기존 host fixture receipt는 현재 source hash와 일치하는 범위를 확인했고 재실행하지 않았다.

| 원계획 | 구현 상태 | 검증/남은 일 |
|---|---|---|
| P0 | 부분 | source/patch/app identity는 본 감사에서 정리. 동일 full env/toolchain/assets/control run 미완료 |
| P1-A/T1 | 핵심 구현 | bitwise type/shape/count/transpose, signed zero, stale fallback 확인. 기존29checks receipt의 writer는 현재와 다름 |
| T2 | 핵심 구현 | immutable link certificate와 prepare PSO 분리. 실제 target/MSAA/Metal integration 미검증 |
| T3 | 핵심 구현 | lazy writer/freeze 및 Current/Restore 관측 경계 확인. 현재 게임의 copies/publications 실측 없음 |
| T4 | Terrain source base 구현 | late transform/target는 유지. 기존 host reader evidence와 실제 GPU TU 범위 구분 |
| P1-B | 부분 | active exact-root index 구현. snapshot lifetime 1회/완전 lease preparation 통합은 미완료 |
| P1-C | 핵심 구현 | partial Add prefix·swap-remove·Delete·geometry proof 보존. 실제 Lua VM/Metal 전체 통합 미검증 |
| P2 | 부분 | content/read-set/phase envelopes 공유. factory full draw proof·CUS typed plan·일부 constants copy 미완료 |
| P3 | 부분 | owned world/screen/read packet, stable admission, capacity2 journals/mailbox. 모든 family transitive seal 및 actual presentation notice 미완료 |
| P4 | 부분 | gate/handoff/ordered controls/captured context, R1 control settlement. 완전 exclusive renderer service 분리 증명 없음 |
| P5 | 제한 pilot + 증거 부족 | Begin 이후 Encode 구간 release, source credits2/GPU pending1. 일반 prepare overlap/현재 ON throughput·latency·parity 미증명 |
| P6 | 부분 | one-GPU gate/pending-input receipt/기존 witness 보존. 전체 access/subresource/dependency proof 미완료 |

| 부록 C 단계 | 대응 상태 | 원계획과의 차이 |
|---|---|---|
| U0 | 부분 | truthful provenance는 개선했으나 clean 동일 baseline 실행 없음 |
| U1 | 부분 | owning screen/task 구현, universal family seal 아님 |
| U2 | 부분 | exact ID journal/data completion/acceptance 구현, actual presentation 별도 미완료 |
| U3 | 부분 | environment/shield/PBR/context/coverage 경계 개선; 전체 준비 중 arbitrary capture overlap 허용은 아님 |
| U4 | 부분 | 1serial worker+gate/ordered control; 모든 backend 변이의 일반 exclusive service 완료로 부를 수 없음 |
| U5 | 제한 pilot | proposed3이 아닌 실제2 source credits. 이는 안전한 더 좁은 bound이며 결함 아님. GPU pending1 유지 |
| U6 | 미완료, 현 상태에서 확대 금지 | no-second-ticket 정책이 현재 안전 경계. explicit family eligibility 뒤에만 확대 검토 |

부록 A는 roster/pose/read-set/selected phase와 full validation의 남은 비용에, B는 index/material/read witness/actual access proof에, C는 owner/journal/controls/three completion kinds/two-ticket 반례에 연결했다. 과거 문서의 single-slot·unregistered Postprocess borrow 설명을 현재 수정 이후의 결함으로 반복하지 않았다.

## 8. 의존순서를 지키는 다음 작업

아래는 후속 작업의 최소 범위이며 이번 감사에서 실행하거나 수정하지 않았다. 실패한 shadow split/ICB/sparse transform을 자동 재개하지 않는다.

1. **재현 manifest와 control 고정.** full a33 identity, patch/tree/binary, 실제 O3/FMA/SDK/minos, full flags/UI/assets/dependency 목록을 연결한다. 최소 확인: 별도 산출물의 receipt와 file hashes 대조. 이것이 먼저여야 이후 평균 FPS의 귀속이 가능하다.
2. **현재 R1 source의 실행 경계 검증.** Minimal이 아닌 final19 app에서 worker active·finished-unjoined 두 상태의 외부 control, wrong/duplicate notice, abort N+1, Begin 전후 실패를 좁게 확인한다. GPU event/feedback timeout은 owner quarantine/allocator 비재사용을 각각 확인한다. 일반 gameplay smoke·Metal Validation·pixel parity의 증거를 서로 분리한다.
3. **Postprocess exact-root/lease와 CUS seal을 독립 변경으로 완성.** 원계획 P1-B/P2의 남은 반복 work부터 없앤다. 각 변경에서 build/validation/pack 횟수와 excluded-phase owner release를 확인하고 same-serial prefix/suffix 및 Lua partial Add 반례를 유지한다. runtime readiness와 sampler ownership은 그대로 둔다.
4. **U3/U4 남은 mutable crossing을 한 family씩 닫기.** renderer를 pause한 N과 main N+1 capture/abort에서 source/cache 변이가 겹치지 않는지 확인한다. final owner 파괴 thread와 shutdown adapter도 유지한다. 일반 prepare overlap 전에 이 단계가 필요하다.
5. **현재 Encode overlap pilot의 실제 이득 확인 후 U5 확대 판단.** 같은 binary profile/scene/output에서 평균 FPS를 비교하고 positive active overlap, accepted→actual-presented, retained bytes/latency를 확인한다. bounded credits와 GPU pending1을 유지한다. 이득이 중립이어도 이미 제거한 생산자 작업은 자동 회귀하지 않는다.
6. **U6/P6는 마지막.** 한 family의 producer/storage/view/subresource/effect/consumer order proof 뒤 단일 copy/barrier/pass 변경을 검토한다. 모든 source ledger와 failed ownership 상태를 함께 확장하기 전 GPU depth를 늘리지 않는다.

## 9. 감사 범위와 한계

이 보고서는 요청 A–D 및 원계획의 전체 계약을 실제 호출 경로로 연결한 정적 감사다. 저장소의 모든 파일을 한 줄씩 전수 검토했다는 주장은 아니다. CPU와 GPU 독립 검토 결과를 주담당이 버전·주요 source 경계와 대조해 통합했다. 현 경로에서 P0/P1 반례를 찾지 못한 사실은 race 부재나 실제 GPU 안전성의 수학적 증명이 아니다.

읽기 전용 source/patch/base blob/remote pinned blob 비교, 기존 build/runtime/test 문서 분석, binary metadata 및 codesign 검증만 수행했다. 게임·benchmark·새 build·test·Validation·`--version` 실행, 소스/app/profile 수정, checkout/reset/clean/fetch/push/PR/issue/배포는 하지 않았다. 원본 patch 적용은 메모리에서만 재구성했다. 원격 ref 조회와 pinned 파일 다운로드는 감사 산출물 디렉터리에만 저장했다.

관련 문서: [CPU 상세 감사](CPU-AUDIT.md), [GPU 상세 감사](GPU-AUDIT.md), [버전 대응표](VERSION-MATRIX.md), source/binary/patch manifest (`LOCAL-EVIDENCE/SOURCE-BINARY-PATCH-MANIFEST.json`; not included in this Markdown-only export), 원계획 (`LOCAL-EVIDENCE/PLAN-original.md`; not included in this Markdown-only export). 종료 시 기록된 58개 원본 항목의 해시와 HEAD/dirty status가 모두 동일함을 확인했다. 원격 branch/main도 시작 고정점 그대로였다. 상세 확인 범위는 audit-end-verification.json (`LOCAL-EVIDENCE/audit-end-verification.json`; not included in this Markdown-only export)에 기록했다. 모든 app 내부 파일의 재귀적 전후 해시를 계산했다는 의미는 아니다.
