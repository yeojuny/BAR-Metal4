# Material preparation: two independent performance candidates

**Base:** `977c88d15adf5217e05b4a3ad8ef1176432515bf` (diagnostic PR #5, stacked on PR #4).  
**Status:** source candidates; both new switches default OFF. No Mac engine build, Metal runtime, visual validation or FPS measurement performed for these changes. Main, v1.11.3, release policy and Homebrew are not changed.

## 목적과 우선순위

계측만 추가하는 변경이 아니라 실제 반복 CPU 작업을 생략하는 두 후보다. 그렇다고 두 후보가 현재 약 185 FPS의 병목이라는 사실이나 FPS 이득을 주장하지 않는다.

**먼저 시험할 것은 D(현재 CUS material buffer 경로)**다. U는 기존 Q의 completed-model pair 재사용 파일럿이 켜져야 실행된다. Q는 기존 clean 실험에서 OFF였으므로 U를 185 FPS 경로의 개선이라고 설명하면 안 된다. 실행 순서는 이미지 준비/재사용(U) → material buffer key/lookup(D)이지만, 개발·벤치마크의 선행 조건은 서로 없다.

기존 SELECTION의 표본 수는 OriginalModelImage-under-CUS 265/942, ModelMaterialsGet-under-CUS 170이다. 포함 표본은 서로 겹칠 수 있으며 시간/절약량이 아니다. S의 타입 판별 캐시는 FPS 향상을 입증하지 못했다. 이 묶음은 S 캐시를 더 크게 만들지 않고 다른 반복 작업을 겨냥한다.

## D — 현재 CUS의 Metal resource-ID key 준비와 최근 buffer 조회

`BAR_NATIVE_MATERIAL_IDENTITY_LOOKUP=1`

변경 파일:
- `engine/rts/Rendering/Backend/NativeExactBindingMemo.h`
- `engine/rts/Rendering/Metal4/Metal4ModelMaterial156.h`

기존 `ModelMaterials156::Get()`은 매번 texture 12개와 sampler 12개의 `gpuResourceID`를 읽은 뒤 24개 ID의 map key를 조회한다. 새 경로는 같은 batch에서 **정확히 같은 실제 객체인 slot**의 ID를 다시 읽지 않는다. 일부 slot만 바뀌면 그 slot만 읽는다. 완성된 ID key가 직전 buffer와 같으면 map 조회도 생략한다. 다른 경우 기존 128-entry map/encoder/eviction 경로를 그대로 사용한다.

- Texture view와 sampler의 정확한 객체를 비교한다. 이름, pixels pointer, 해시 충돌만으로 동일시하지 않는다.
- 고정 크기 memo가 최대 texture 12개, sampler 12개, buffer 1개를 owning handle로 보유한다. ARC의 strong ownership을 syntax/IR fixture로 확인했다. raw pointer 재활용을 identity 증명으로 쓰지 않는다.
- 새 GPU resource나 texture copy는 생성하지 않는다. owner retain/release와 비교 비용은 추가되므로 적중이 적으면 손해일 수 있다.
- `BeginBatch`와 `Clear`에서 reset된다. context/device의 기존 검사도 유지한다.
- **성공하는 매 Get에서 `lease.AddAllocation(buffer)`는 그대로 수행한다.** 앞에서 실행한 provider, texture/sampler 등록, original draw의 `requireReadable`도 변경하지 않는다.
- Texture 내용, producer readiness, 완료 여부, barrier나 재사용 권한을 저장하지 않는다. ID key는 이미 준비된 argument-buffer handle 표를 찾기 위한 값일 뿐이다.
- 기존 hits/created/encodedBytes 의미를 유지한다. `Stats()`에 `identityHits`, `identityMisses`, `resourceIDReads`, `omittedResourceIDReads`를 추가했다. 새 필드는 ON에 대한 batch-local 작업 계수이며 **기존 로그 문자열에는 자동 출력되지 않는다**. native 실험에서 필요하면 기존 집계 지점에 연결하되 per-draw 로그를 만들지 않는다.

참고: Apple Metal 4 문서는 각 texture view의 고유 `MTLResourceID`와 texture의 `gpuResourceID`를 설명한다. 이 변경은 정확한 살아 있는 texture/sampler 객체의 handle ID만 재사용한다.
https://developer.apple.com/documentation/metal/understanding-the-metal-4-core-api

## U — 기존 completed-model pair 조회의 임시 target 복사 제거

`BAR_METAL4_SHARED_CUS_PILOT=1` **and** `BAR_NATIVE_SHARED_PAIR_KEY_VIEW=1`

변경 파일: `engine/rts/Rendering/Metal4/Metal4SharedProgramPilot.h`

기존 Find는 target vector를 가진 lookup key를 만들고, hit 후 ExecutionIdentity 비교를 위해 target vector를 다시 복사한다. ON에서는 lookup 중에만 target을 빌리는 transparent key와 field-wise execution 비교를 사용한다.

- 저장되는 key와 ExecutionIdentity는 여전히 target 값을 소유한다. 참조 key를 map에 저장하지 않는다.
- exact program, 두 source owner/control block, context/device/lease/submission/journal/target 검사는 유지한다.
- 두 `completedModelProof->ValidFor()` 호출과 draw-time readability는 그대로다.
- admission은 원래 provider와 caller의 texture/sampler 등록 이후에만 가능하다. capacity/fallback/미완료 입력의 거절을 넓히지 않는다.
- 새 provider-result cache가 아니다. **기존 Q 재사용 경로의 조회 비용을 줄이는 후보**다.
- Q의 draw별 MaterialContent/PreparedSlice 생성이나 모든 준비 비용을 제거한 것은 아니다.
- Q를 처음 켠 결과와 U 자체의 효과를 혼동하지 않는다. Q ON/U OFF와 Q ON/U ON을 비교해야 한다.

## 호스트에서 실제 수행한 검증

```sh
python3 engine/tools/material-preparation-perf/run_tests.py
# macOS: 같은 의미 시험 뒤 설치된 Apple SDK로 header syntax도 검사
python3 engine/tools/material-preparation-perf/run_tests.py --native-syntax
```

Linux Clang 17, C++20, ASan/UBSan:

| 시험 | 결과 |
|---|---|
| U의 actual production scope + actual shared-program schema, runtime/texture는 모델 | OFF 1,000 hit 조회: target 관련 allocation 2,000; ON: 0 |
| 동일 U 시험의 completion proof 검사 | OFF/ON 모두 2,000회 |
| U의 target/owner/program/journal/context/device/submission 변경, 128-entry 한도 | PASS; 다른 owner control block과 미완료 proof 거절 |
| D의 actual portable memo, original map/lease는 모델 | 동일 입력 10,000회 ID reader 호출 240,000 → 24; 등록 모델은 양쪽 10,000 |
| D의 texture 두 slot만 번갈아 변경 | 추가 10,000회 중 ID reader 호출 20,000; 나머지 22개 slot은 재사용 |
| D의 view/sampler/nil/내용 변경, reset, owner 수명, 실패한 getter | PASS |
| 실제 Metal4ModelMaterial156 header의 Objective-C++ ARC template instantiation | 선언용 Metal stub으로 syntax/LLVM IR 확인; Apple framework 실행 아님 |

이 수치는 **통제된 입력의 작업량 검증**이다. 게임의 반복 빈도, Apple CPU 시간, frame time, FPS 추정에 곱하거나 외삽하지 않는다. 실제 `CompletedS3OImageProof`의 Metal 동작은 모델 proof로 대체했으므로 native validation이 필요하다.

## 실행·비교 순서

하나의 별도 private candidate를 Mac에서 full build한다. PREPARE1/BEGIN0, worker1, Sepia OFF, FINAL_COLOR_STAGE1을 유지한다. TYPE_FACTS와 SCREEN_STATE_ELISION 등 상속된 flag는 양쪽 동일하게 고정한다. release launcher는 BAR_NATIVE 변수를 지우므로 outer wrapper에 export만 하고 실험이 켜졌다고 간주하지 않는다. **실제 child environment를 확인한다.**

| Cell | SHARED_CUS_PILOT | SHARED_PAIR_KEY_VIEW | MATERIAL_IDENTITY_LOOKUP | 용도 |
|---|---:|---:|---:|---|
| D0 | 0 | 0 | 0 | 현재 경로 기준 |
| D1 | 0 | 0 | 1 | 첫 독립 성능 후보 |
| U0 | 1 | 0 | 0 | 기존 Q를 먼저 native 검증한 별도 기준 |
| U1 | 1 | 1 | 0 | Q 동일 조건에서 U의 효과 |
| Combined (후속) | 1 | 1 | 1 | 독립 결과 이후 상호작용 확인; 이득 합산 금지 |

D0/D1부터 동일 입력의 clean 반복 비교를 한다. U0/U1은 기존 Q의 화면·완료 계약을 통과한 뒤 시험한다. 정상 foreground/잠금 해제, 전원 상태, 실제 target 해상도, 장면/카메라/seed, shader warmup, 내용 hash를 고정한다. Clean은 trace/Validation OFF; 진단·Validation은 별도 실행이다. 불리한 실행도 보존한다.

## 수락 기준과 다음 작업

1. 정확한 production TU/engine build와 Metal Validation, 대표 화면/resize/종료 확인.
2. 작업량 감소와 실행 시간 감소를 구분한다. memo 추가의 owner 비용과 메모리도 기록한다.
3. 표본 및 작은 평균 차이만으로 이득/동등성을 선언하지 않는다. 평균 FPS와 tail을 함께 보존한다.
4. 성능 이득이 없으면 public 기본값으로 승격하지 않는다. 기본 OFF와 기존 baseline을 유지한다.
5. #5의 provider 시간 분해와 공통 Join/제출 관계도 계속 사용한다. D/U는 이를 대체하는 확정 병목 진단이 아니다.

더 큰 변경은 아직 넣지 않았다: broad provider-result cache, 일반 MRT209 권한 확대, GPU inflight 확대, drawable/Join 재배치는 별도 수명·작업량 증거가 필요하다. 이 첫 묶음으로 그것들이 해결됐다고 주장하지 않는다.
