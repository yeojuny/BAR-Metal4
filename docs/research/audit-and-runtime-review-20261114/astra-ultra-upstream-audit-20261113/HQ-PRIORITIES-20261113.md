> Publication copy: original conclusions and historical version scope preserved. Personal absolute paths were neutralized; source links are pinned to the audited a33ebfe revision. Raw logs, JSON manifests, binaries, screenshots and the original plan are not included in this Markdown-only export. See the parent README for later runtime results and limitations.

# 전체 감사 후 우선순위와 상위 계약 수정 순서

2026-11-13 · HQ 정리 · Astra ultra 통합 감사와 별도 MRT 보충 검토 기준

## 판단 범위

현재 공개 기준은 `a33ebfe888bc8abe0053df05e5e2892c0788d6a8`이다. 이번 정적 감사에서 현재 경로의 새 P0/P1 정확성·GPU 수명 결함은 확정되지 않았다. 실제 Metal 실행, P5 ON 성능, 화면 동등성 검증은 수행하지 않았다. 아래의 ‘큰 후보’는 구조적 범위와 의존순서에 따른 우선순위이며, 실측 ms/FPS 순위가 아니다.

- [통합 감사](AUDIT.md)
- [GPU 수명 감사](GPU-AUDIT.md)
- [CPU 생산자 계약 감사](CPU-AUDIT.md)
- [BeginReady–MRT 성능 상충 보충](ADDENDUM-BEGINREADY-MRT209.md)
- [버전 대응표](VERSION-MATRIX.md)

## 가장 먼저 볼 큰 구조적 지점

### 1. Prepare가 끝나야 메인이 풀리는 제한된 비동기 경로

현재 BeginReady는 `DrawPrepared`의 준비 및 Begin 이후에 발행된다. 따라서 주로 이후 Encode 구간과 다음 프레임 capture를 겹칠 수 있다. 전체 Prepare와 다음 capture를 겹치는 일반적인 P5는 아직 완성되지 않았다. 동기적으로 준비하는 큰 부분을 그대로 둔 채 worker 스레드 수만 늘리는 접근으로는 이 경계가 바뀌지 않는다.

[WorldRenderer:4445](https://github.com/yeojuny/BAR-Metal4/blob/a33ebfe888bc8abe0053df05e5e2892c0788d6a8/engine/rts/Rendering/Metal4/Metal4WorldRenderer.mm#L4445)

기존 238의 노출 prepare 약0.261ms·encode 약0.448ms는 당시 진단의 관측이다. 현재 후보의 절감량이나 두 구간 전체를 삭제할 수 있다는 뜻이 아니다. 현재 후보의 실제 active overlap·평균 FPS는 미측정이다.

### 2. 비동기 요청이 기존 MRT 복사 제거를 잃을 수 있음

`DrawDispatch`는 `allowBeginReady` 요청이 있으면 MRTFrontier를 만들지 않는다. 최종 pilot eligibility는 그 뒤에 계산하므로, 요청은 됐지만 pilot에는 부적격인 경로도 해당 permission을 잃을 수 있다. 다만 환경변수 ON의 모든 경로에 요청값이 자동 설정되는 것은 아니며 SceneBridge의 admission·water 조건부터 통과해야 한다.

연결은 `WorldRenderer:2397` → `OpaqueModelSource:305` → `MRTWriteReuse209:33` → `CUSDeferred:254,339`이다.

OFF라면 원래 209의 exact source/view/publication/no-reader 조건을 통과할 **쓰기 F 단계**에서 ON이 그 permission을 잃으면, COW 저장 공간 선택과 보존 복사가 되살아날 수 있다. readonly164는 유지되고, 원래 209 부적격이거나 clear/discard로 보존이 불필요한 단계는 추가 손실로 계산할 수 없다. Lua-only 쓰기는 해당 재사용 플래그와 실제 prepared draw 조건도 필요하다. 모든 프레임에서 여섯 복사 또는 새 물리 할당이 생긴다는 주장은 하지 않는다.

이 경로는 CPU Encode 겹침의 이득과 GPU 복사·저장 공간 비용을 함께 바꿀 수 있는 **고우선 조건부 성능 상충**이다. 실제 빈도·바이트·pool hit·GPU 시간·FPS 기여는 미측정이다.

기존 guard를 지우거나 동기 경계용 frontier를 그대로 비동기에 넘기는 해결은 허용하지 않는다. source/storage version·view, 현재 및 이후의 이전 내용 독자, 쓰기 권한의 예약·소비 시점, 취소·실패 시 소유권을 공유하는 좁은 MRT 계약을 P5 확대 전에 고정한다. 증명되지 않은 경로는 COW를 유지한다. 전체 P6를 먼저 완성할 필요는 없다.

### 3. source를 공유해도 CUS 전체 검증과 typed packing이 반복됨

roster와 uniform content 공유는 이미 구현됐다. 그러나 `ValidateNativeCUSSourceDraw`가 Submit·CPU read·GPU/deferred 소비 경로에서 반복되고, reflected field의 이름/shape/type 확인과 packing도 매 draw 남는다. 전체 draw의 immutable factory proof와 CUS typed source base가 미완료다.

공통 source seal과 typed layout을 먼저 정하고 각 소비자가 같은 증거를 재사용하게 해야 한다. GPU target·실제 read-set·lease readiness·retain은 소비 시점의 별도 검사로 남긴다. 단순 `validated=true` 플래그로 검사를 지우지 않는다.

[SourceData:28](https://github.com/yeojuny/BAR-Metal4/blob/a33ebfe888bc8abe0053df05e5e2892c0788d6a8/engine/rts/Rendering/Backend/NativeCUSSourceData.cpp#L28) · [ProgramData:27](https://github.com/yeojuny/BAR-Metal4/blob/a33ebfe888bc8abe0053df05e5e2892c0788d6a8/engine/rts/Rendering/Backend/NativeCUSProgramData.cpp#L27)

### 4. Postprocess는 snapshot 수명 전체의 한 번 준비까지 닫히지 않음

현재 active exact-root plan은 올바르게 재사용하지만 A→B→A root 전환에서 A의 index를 다시 만들 수 있다. runtime producer set·descriptor/overwrite 검사와 lease-local 중복 준비 일부도 남는다. root/선택별 immutable plan과 lease-local 준비를 분리해 공통 경계에서 줄인다. 같은 serial의 prefix/suffix를 합치거나 runtime readability를 정적 증거로 대체하지 않는다.

[PostprocessData:283](https://github.com/yeojuny/BAR-Metal4/blob/a33ebfe888bc8abe0053df05e5e2892c0788d6a8/engine/rts/Rendering/Backend/NativePostprocessData.cpp#L283) · [PostprocessNative:580](https://github.com/yeojuny/BAR-Metal4/blob/a33ebfe888bc8abe0053df05e5e2892c0788d6a8/engine/rts/Rendering/Metal4/Metal4PostprocessNative.mm#L580)

## GPU 수명에서 먼저 보존·확인할 경계

- **구버전 결함은 현재 수정됐다.** 최초 Minimal의 P5 ON 조건에서 미정산 worker handoff 중 외부 MetalInfo/UnitUniformCopy가 Runtime에 들어가면 access gate 충돌·poison으로 중단될 수 있다. 실제 게임 재현은 미확인이고 메모리 파손이 입증된 것은 아니다. 현재 R1은 CPU task/handoff와 source notice를 정산한 뒤 외부 제어로 들어간다. 이 수정은 재작성보다 현재 판에서의 검증이 먼저다.
- **GPU pending1, source/recorder 장부2, recording slots는 서로 다른 수량이다.** 둘 중 하나만 늘려 다른 수명 조건을 추정하지 않는다.
- shared event와 feedback, producer 완료 검사를 지난 뒤에만 정상 retirement를 한다. timeout·GPU 오류·Begin 후 실패는 성공 notice나 재사용 권한으로 변환하지 않고 관련 owner를 보존한다. 일부 producer callback 완료와 전체 프레임 성공도 구분한다.
- 실제 sampler leaf 강소유와 lease pool pin을 유지한다. `shared_ptr` 생존은 이전 픽셀의 덮어쓰기 안전성이나 GPU 가시성의 대체 증거가 아니다.
- 현재 성공 보고의 `presented=true`는 예약된 표시 의미다. 실제 drawable 표시 시각과 연결된 별도 회계가 미완성이다. GPU 완료·source retirement·실제 표시를 같은 성공 플래그로 합치지 않는다.

## 로컬 컴파일 수정과 정확한 기준

9파일 패치는 현재 a33에 이미 포함됐다. 최종 관계는 다음과 같다.

`6e9ce8f + mac9 + review8 + companion2 = 현재 production19 = 원격 a33의 production19`

따라서 a33 기준으로 작업할 때 mac9를 별도로 재적용하지 않는다. 최초 실행한 Minimal은 base+mac9이고, 후속 Review Ready는 전체19파일 판이다. 최초 앱의 worker1/pilot0 정상 종료를 후속 앱 또는 P5 ON 통과로 옮겨 적지 않는다.

9파일의 shared_ptr atomic free 함수는 확인한 production 경로에서 acquire/release/CAS·one-shot publication과 owner retention을 보존한다. DeleteShader의 새 uniform 초기화, friend 인자 정정, 예외 선언 보충도 현재 동작 차이는 발견되지 않았다. 이를 이유로 기존 수명 검증을 생략하지 않는다.

현재 Review Ready manifest의 short cloud commit 표기는 공개 full commit과 대응을 완성하지 못한다. 실제 빌드는 renderer recipe 마지막에 `-O3 -ffp-contract=fast`가 있고, runtime 옵션을 OFF해도 이 컴파일 설정은 바뀌지 않는다. source-only 비교와 pilot ON/OFF 비교를 구분해야 한다. Homebrew 의존성·UI/game/map·full environment도 기준에 포함한다.

## 두 번 고치지 않기 위한 수정·검증 순서

| 순서 | 작업 | 다음 단계로 넘길 공통 계약과 완료 기준 |
|---|---|---|
| 0 | 정확한 source/binary/control 기준 고정 | full a33·tree/patch/binary·실제 compiler/math/SDK·13flags 또는 선택한 명시적 profile·UI/assets/dependencies 연결. 비교 양쪽은 같은 조건. 기존 mac9 재적용 없음 |
| 1 | 현재 R1의 정산·실패·수명 경계 검증 | worker active 및 finished-unjoined 외부 제어, N+1 abort, 중복/wrong-epoch notice, Begin 전후 실패, event/feedback timeout에서 정확한 journal·credit·quarantine 보존. 이미 고친 코드를 다시 만드는 단계가 아님 |
| 2 | immutable source seal/static proof 및 typed layout 완성 | CUS draw 공통 proof·packing, Postprocess exact-root plan을 각각 독립 변경으로 진행. 실제 source 작업 감소와 기존 accept/reject·순서·lease 등록을 확인 |
| 3 | 남은 main/render mutable 교차 접근과 좁은 MRT 권한 경계 정리 | packet/journal과 consumer가 같은 version/reader/overwrite 계약 사용. N을 멈춘 동안 N+1 capture/abort가 관측·변경하는 상태를 대조. 증명 불가 경로는 COW |
| 4 | 현재 Encode pilot 검증 후 Prepare까지 실제 CPU lookahead 확대 | GPU pending1 유지. capture(N+1)와 worker의 실제 prepare/encode(N) 활동 구간 교집합, accepted/commit/GPU완료/실제표시, MRT reuse/COW/copy 비용을 구분. 평균FPS·메모리·지연과 함께 판단 |
| 5 | 정확한 가족별 P6와 GPU 큐 확대 | producer/storage/version/view/subresource/read/write/order proof가 닫힌 한 경로에서 copy·barrier·pass 변경 하나씩. GPU depth 확대는 모든 관련 장부·실패 owner와 함께 검토 |

2번의 CUS와 Postprocess는 독립 변경으로 나누되, 각각의 상위 source/plan 계약을 먼저 고정한다. 3번의 좁은 MRT 계약은 4번의 선행 조건이다. 5번의 전체 GPU 의존성 시스템까지 먼저 만들자는 뜻은 아니다. 고정할 공통 경계를 정한 다음 구현을 나누며, 새 패치마다 한 가설과 해당 검증 범위를 유지한다.

## 다음 실행에서 판별할 것

후속 실행은 아직 하지 않았다. 실제 승인이 있을 때 현재19파일 판의 pilot OFF/ON부터 같은 binary/profile로 비교하되, 요청·실제 eligibility와 readonly/write-reuse/COW 분기를 구분한다. 복사 횟수·논리 바이트·pool new/reuse가 달라지면 전체 FPS 차이를 CPU 겹침만의 효과로 부르지 않는다. producer 작업 삭제, CPU 스레드 이동, 실제 겹침, GPU 작업 증가를 별도 결과로 남긴다.

Terrain T1–T4, CUS roster 공유, bounded journal/mailbox, R1 정산과 Postprocess pending-input receipt의 현재 구현은 보존한다. 원래 계획의 이미 구현된 단계를 다시 작성하는 대신 미완료 공통 경계와 현재 판의 검증부터 이어간다. 각 후보의 FPS 기여도는 현재 미측정이며, 230FPS나 제거할 수 있는 ms를 미리 약속하지 않는다.
