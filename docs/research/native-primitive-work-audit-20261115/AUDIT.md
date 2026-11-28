# Native primitive 작업 증폭 감사보고서

작성일: 2026-11-15, America/Los_Angeles. 상태: **초기 발견 목록과 후속 작업 분리표 — 구현 변경 없음.**

## 1. 결론과 이번 보고서의 경계

**SensorLOS에서 확인한 ‘도형을 크게 확장하고 셰이더에서 실제 포함 여부를 다시 판정하는 구조’를 선·점·채움 삼각형·사각형·다각형 및 공통 coverage 경로로 넓혀 감사한다.** CPU에서 픽셀 도형을 생성하는 경로와 GPU 셰이더가 래스터 규칙을 재현하는 경로를 구분한다.

현재 첫 수정 후보는 **SensorLOS의 기존 정확한 coverage 판정을 유지하면서, 그 판정에 들어오는 불필요한 Fragment 영역을 줄이는 것**이다. 무기 사거리·Sensor31657에는 유사 구조가 있지만 같은 장면의 비용까지 확인된 것은 아니다. 점·범용 선·다각형은 실제 활성 경로와 비용을 먼저 확인한다.

이 문서는 기존 [순차 감사 계획 PR #8][pr8]의 발견 보고서다. 계획을 다시 작성하거나 #8을 구현 PR로 바꾸지 않는다. #1, #3–#7의 소스·실험·증거를 수정하거나 병합하지 않는다. 엔진 빌드, 게임 실행, GPU 캡처, 새 FPS 측정은 이 문서 게시 과정에서 수행하지 않았다.

**저장소 전체 전수감사가 완료된 것은 아니다.** 아래 표는 직접 검토한 고정 소스의 초기 목록이다. 소스에 코드가 있다는 사실, 실제 실행됐다는 사실, 큰 비용이라는 사실, 수정 후 빨라졌다는 사실은 각각 별도 증거가 필요하다.

## 2. 기준 identity와 증거 등급

| 항목 | 고정값 / 의미 |
|---|---|
| 분석 대상 Native source | `a3c7c6bfa93d09dac25755534195932d12fff41a` |
| 제공 보고서의 Native binary SHA256 | `b25a08287679847a420c37eaf3c201c9c95ad696e90d1b1eaa18e0cb08060942` |
| 제공 보고서의 MVK3 binary SHA256 | `d715965659b977e05b8ccafe76fae0936cade1a7481871acff7eec876366f1aa` |
| 문서 브랜치 출발점 | main `a0963d4b2e39a6a2bdf97c9eb5f639e71f97afa6` |
| 기존 비교·실험 자료 | [외부 감사 인계서, 고정 publication][handoff] |
| 순차 실행 정책 | [PR #8 고정 계획][plan] 및 [후속 조사 지시][instruction8] |

**문서 브랜치의 main 기반 엔진을 측정 기준으로 빌드하면 안 된다.** 문서의 출발점과 분석한 엔진의 출발점은 다르다. 기존 측정 기준의 PREPARE1/BEGIN0, worker1, FINAL_COLOR1, MODEL_TYPE_FACTS1, SCREEN_STATE_ELISION1, STORAGE_POOL0, D/Q/U0, Sepia OFF 및 나머지 실제 child flags를 보존한다. 새 기준을 택하면 source/binary/settings identity를 다시 기록한다. [plan]

증거 등급:

- **OBSERVED-REPORTED:** 제공된 캡처·재생 분석 보고서가 실제 실행과 비용을 보고함. 이 문서 게시자가 원시 캡처를 다시 재생한 것은 아님.
- **SOURCE-CONFIRMED:** 고정 소스에서 구조를 확인함. 해당 fixture의 호출·빈도·비용은 미확인일 수 있음.
- **EXISTING-FAST-PATH / REVIEW-ONLY:** 이미 분리된 경로 또는 진단 주석이 있는 경로. 새 병목이나 새 최적화 성과로 세지 않음.

## 3. 출발점: 제공 자료에서 확인되는 관측

### 3.1 SensorLOS: 실제 경로 차이와 비용

아래 수치는 제공된 **「BAR Native / MVK3 — Fragment 처리와 중첩 비교」**의 보고값이다. 이번 게시의 새 측정값이 아니다. 출처 파일 식별은 §9에 보존한다.

| 항목 | Native | MVK3 |
|---|---|---|
| 실제 명령 / 프로파일 | cb0/re42 draw1 / re32 | cb3/re312 draw0 / re60 |
| primitive / draw | Triangle, 378 vertices × 165 instances | Line, 126 indices × 171 instances |
| 선 규칙 구현 | 축 정렬 사각형 2개 삼각형 + Fragment의 64-bit diamond-exit / 굵은 선 판정 | 실제 Line primitive, 캡처의 `setLineWidth:5`; 해당 FS software coverage 없음 |
| 폭 | uniform 5.01923084 → 정수 5 | 실제 폭 5 |
| 각 재생 내부 FS 비용 비중 | 31.624% | 1.24% |
| 컴파일된 정적 명령 / 임시 레지스터 | 1323 / 91 | 61 / 15 |
| 재생 메타데이터 SIMD groups | 292192 | 53276 |
| 출력 | color + explicit `depth(any)` | color |

양쪽 sample1, Depth LessEqual / depthWrite OFF, Stencil NotEqual / ref1 / pass Replace, alpha blend라는 공통 화면 의미가 보고됐다. 다만 별도 depth/stencil과 결합 attachment 차이, instances 165/171, 동일 simulation 순간 미확인 등의 한계가 남는다. Native 캡처 uniform의 simulation frame은 1593이다.

**금지할 해석:** 31.624 / 1.24를 속도 향상 배수로 계산하지 않는다. 서로 다른 재생의 분모다. 정적 명령은 픽셀당 동적 실행 명령 수가 아니며 SIMD groups는 정확한 Fragment 호출 수가 아니다. re42 전체는 7 draws를 포함하므로 encoder 전체 시간을 SensorLOS 단독 시간으로 부르지 않는다. MVK 캡처에서 width5가 보인다는 이유만으로 Native에서 지원되는 동일한 Metal Line 대체 경로가 확보됐다고 단정하지 않는다.

### 3.2 중첩 손실은 별도 축

제공된 MST 보고서는 [1,5)초에 완전히 포함된 GPU 프레임 MVK3 338개 / Native 177개에서 GPU activity union 11.453 / 21.933 ms, Fragment 활동 8.481 / 16.007 ms, 프레임 내부 채널 중첩 5.213 / 0.009 ms를 보고했다. 기존 clean 평균 FPS 93.87 / 47.62와 별도 측정이다.

새 재생에서는 MVK3의 지면 데칼 FS 6.939160–9.305666 ms가 풀 VS 6.681263–11.223000 ms와 2.366506 ms 겹쳤다. Native의 해당 역할 범위는 분리돼 있었다. Native는 같은 혼합 encoder에 데칼·풀 등을 넣고, MVK3는 별도 encoder와 stage별 fence 연결을 사용했다.

이 결과는 **도형 작업 증폭과 pass 구성/의존성 검토를 구분할 근거**다. 특정 barrier 하나가 원인이라는 증명이나 encoder 분할만으로 향상된다는 증명은 아니다. 모든 자원 alias가 해독된 것도 아니다. 이전 MST frame106을 새 캡처 shader에 직접 연결한 것이 아니므로, 이전 두 긴 구간의 정확한 identity 연결이 완료됐다고 보고하지 않는다.

## 4. 발견 목록: 같은 비용 구조를 기능 이름을 넘어 추적

모든 소스 링크는 분석 대상 `a3c7c6b…`에 고정했다. `구현 PR 미배정`은 이 보고서가 코드를 고치거나 담당자를 지정하지 않았다는 뜻이다.

| ID | 도형 / 경로와 소스 | 확인한 구조 및 조건 | 상태 / 다음 판단 |
|---|---|---|---|
| R01 | SensorLOS outline — [NativeSensorLOSShaders.h][los] | `OriginalSensorLOSVertex`가 각 선분의 축 정렬 bbox를 만들고, `OriginalSensorLOSFragment`가 `SensorLineCovered`로 coverage를 판정. 보간·mask sample·alpha·explicit depth도 수행 | **OBSERVED-REPORTED + SOURCE-CONFIRMED. 첫 단일 수정 후보.** 구현 PR 미배정 |
| R02 | 무기 사거리 선 — [NativeWeaponRangeShaders.h][range] | `OriginalWeaponRangeLine`, `RangeLineCovered`, `RangeDiamond`, `FloorDivide`. 선분 bbox와 정수 판정 계열이 R01과 유사. 실제 sample/entry point 구분 필요 | **SOURCE-CONFIRMED.** 해당 fixture의 활성 여부·GPU 비용·판정 계약 차이를 확인한 후 R01 결과 적용 여부 결정 |
| R03 | Sensor31657 circle — [NativeSensor31657CircleShaders.h][circle] | `OriginalSensor31657CircleVertex`의 bbox, `SensorLineCovered`의 정수 판정. `Sensor31657Outside`는 원 목록을 순회. 미분 계산 후 최종 discard | **SOURCE-CONFIRMED.** 원 목록 순회는 원래 효과 계산인지 Native 추가 비용인지 별도 대조. LOS와 동일 효과로 합치지 않음 |
| R04 | 범용 월드 단일 샘플 선 — [NativeWorldDrawGeometry.cpp][world] | `BuildGeometry`에서 `AppendNativeScreenLines` 결과의 픽셀 사각형을 순회하고 원근 보간한 색·UV로 출력 정점 생성 | **SOURCE-CONFIRMED.** CPU 시간·출력 vertices/indices/bytes·선 길이 및 이 경로를 우회하는 GPU 경로의 선택 조건 확인 |
| R05 | 점 / point sprite — [월드 geometry][world], [point coverage shader][point] | aliased 점은 snapped square로 확장. samples>1 또는 sprite 경로는 보수적 사각형과 point metadata를 만들고 `RecoilPointSampleMask`가 샘플별 disk/square 포함 여부 계산 | **SOURCE-CONFIRMED.** 1× plain / MSAA plain / sprite 분리. 사각형 확장 자체를 낭비나 대체 가능성의 증거로 삼지 않음 |
| R06 | 다각형 line/point mode, mixed front/back mode — [NativeWorldDrawGeometry.cpp][world] | clipping, 면 방향·culling 판단 후 원래 boundary를 선/점으로 재생성. 원래 quad diagonal과 clip-created boundary 의미 보존 | **SOURCE-CONFIRMED.** source primitive별 호출·출력량 확인. filled triangle 경로와 별도로 취급 |
| R07 | 채움 삼각형 / quad / fan, 사용자 clip plane — [NativeWorldDrawGeometry.cpp][world] | 앞·뒤 모두 Fill이고 inherited clip plane이 없으면 기존 vertices/indices 반환. 사용자 plane이 있으면 `BuildValidatedNativeWorldClippedFill`에서 CPU clip/재삼각화 | **SOURCE-CONFIRMED + EXISTING-FAST-PATH.** 일반 fill 전체를 software rasterization이라고 부르지 않음. 특정 clipping 경로만 비용 조사 |
| R08 | 사거리 채움 / 확장 정점의 반복 계산 — [range shader][range], [range renderer][range-renderer] | `OriginalWeaponRangeFill`은 값과 provoking color를 구하기 위해 `OriginalRangeVertex` 호출. line도 endpoints를 평가하며 해당 원래 함수에는 조건부 heightmap 반복 계산이 있음 | **SOURCE-CONFIRMED.** 실제 VS invocation·reuse·활성 분기 확인. 원래 지형 추종 알고리즘 자체를 Native 낭비로 간주하지 않음 |
| R09 | 일반 화면 채움 — [NativeScreenFilledShaders184.h][filled] | 허용된 root1× filled 전용 `ScreenFilledFragment184`는 generic point/sample-mask 처리를 생략 | **EXISTING-FAST-PATH.** 다시 같은 최적화를 제안하지 않음. 실제 route hit/miss는 별도 확인 |
| R10 | 모델 wireframe — [Metal4CUSSource.mm][cus] | `PreparedCUSSource::EncodeDraw`는 wire 조건에서 `MTLTriangleFillModeLines` 사용 | **EXISTING-FAST-PATH.** R06의 CPU boundary 재생성과 같은 경로가 아님. 하드웨어 사용만으로 전체 parity/비용 검증 완료도 아님 |
| R11 | 공통 단일/MSAA 선 helper — [NativeSourceLineRasterShaders130.h][source-line], [NativeMultisampleLineShaders.h][msaa-line] | source-line helper는 두 minor replicas를 검사하는 구현. MSAA helper는 샘플별 rectangle coverage로 단일 샘플 diamond-exit과 구분 | **SOURCE-CONFIRMED.** 실제 소비자와 sample 조건별로 연결. 모든 선에 하나의 경계/판정식을 무조건 적용하지 않음 |
| R12 | exact world line numeric prototype — [NativeExactWorldLineShaders.h][exact] | binary64 에뮬레이션 및 diagnostic/no-production-integration 주석이 있음 | **REVIEW-ONLY.** 주석만으로 활성/비활성을 확정하지 않음. 실제 호출·선택 경로 증거 전에는 현재 비용에 산입하지 않음 |

### 이미 있는 변경을 다시 만들지 않기

[사거리 renderer][range-renderer]에는 `CanIndexRangeLine` / `MakeRangeLineIndices` 및 indexed drawing 경로(RANGE199)가 이미 있다. [사거리 shader][range]에는 `RECOIL_RANGE_DIVISION233` 아래 float 추정 후 exact residual correction을 하는 나눗셈 경로도 존재한다. 조건부 구현의 존재와 현재 실제 활성 여부는 다르다.

따라서 ‘6개 정점을 4개로 줄이자’ 또는 ‘64-bit 나눗셈을 추정+보정으로 바꾸자’를 새 발견처럼 제안하지 않는다. 기존 [PR #1][pr1]의 native233 overlay도 참조 대상이지 중복 수정 대상이 아니다. R01의 우선 가설은 **판정 함수에 들어오는 영역 감소**이며, 나눗셈·인덱싱·캐시·pass 분할을 한 번에 섞지 않는다.

## 5. 앞으로 전수 목록을 완성할 방법과 아직 남은 범위

각 항목을 다음 연결로 기록한다.

`원래 primitive/state → capture/geometry 변환 → 실제 Native 도형·CPU bytes → shader/pipeline → 소비 기능 → fixture의 실행 증거 → 대응 MVK 작업 → 비용`

파일 이름 검색만으로 완료 판정을 내리지 않는다. `discard_fragment`, `sample_mask`, `depth(any)`, clip/coverage helper, CPU geometry builder 등의 검색은 **후보 탐색**일 뿐이다. 원래 GLSL에도 있는 효과 계산, 진단 코드, 빠른 경로와 fallback을 분리한다.

| 감사 축 | 필요한 기록 | 현재 상태 |
|---|---|---|
| 화면 영역 증폭 | bounds 종류, 실제 허용 coverage, fragment/sample 관련 관측 | R01은 제공 캡처 근거, R02/R03은 구조만 확인 |
| 도형 수·CPU 업로드 증폭 | input primitive → output vertices/indices/bytes, 생성 CPU 비용 | R04–R08 구조 확인, fixture별 실측 미완료 |
| 계산의 반복 위치 | draw/primitive당 값이 vertex/fragment/sample마다 다시 계산되는지 | R08 후보 확인, 실효 중복량 미확인 |
| 공통 경로의 특수 기능 | 단순 fill에 point/MSAA/alpha/clip 기능이 불필요하게 붙는지 | R09의 기존 전용 경로 확인, 다른 소비자 미완료 |
| depth/stencil/clip/sampling 재구현 | 원본 의미, native 지원 경로, 추가 비용, 보존해야 할 순서 | R01 일부 확인, 전체 소비자 감사 미완료 |
| UI·minimap·PiP·RTT·world/particles 등 소비자별 연결 | 공통 helper의 모든 사용처 및 실제 활성 route | **전수 완료 아님**. 범용 경로 검토를 전체 기능 검토로 대체하지 않음 |
| pass/attachment 의존성 | producer/consumer/version/alias, stage별 대기, store/load | 별도 S01 항목. R01 코드 변경과 혼합하지 않음 |

**완료 기준:** 각 실제 소비자에 근거·상태·후속 담당 PR이 붙고, 미검토 영역이 명시돼야 한다. 후보 수가 많다는 이유만으로 모든 후보를 동시에 수정하지 않는다. 전수 목록 보완과 R01의 제한된 검증은 병행할 수 있지만 활성 성능 가설은 하나로 유지한다.

## 6. PR 중복 방지: 기존 소유 범위와 새 보고서 범위

게시 전 GitHub에서 열린 PR **#1, #3, #4, #5, #6, #7, #8**의 제목/설명 및 전체 changed-filenames 목록을 확인했다. 이 보고서의 유일한 변경 경로인 `docs/research/native-primitive-work-audit-20261115/AUDIT.md`는 어느 목록에도 없다. 아래 구분은 **이 조회 시점의 snapshot**이며 미래 변경까지 충돌이 없다는 보장은 아니다.

| 기존 PR | 유지할 기존 범위 | 이번 보고서와의 경계 |
|---|---|---|
| [#1][pr1] | fullscreen 수정, native220/233 진단·overlay·기여 기록 | fullscreen·lobby 수정 및 과거 233 최적화를 다시 만들지 않음 |
| [#3][pr3] | BEGIN/PREPARE 준비 겹침·회수 계약·checkpoint | frame 내 GPU V/F 중첩과 CPU 준비 overlap을 혼동하지 않음. source/retirement 변경 없음 |
| [#4][pr4] | P/Q/S checkpoint, 공유 프로그램·모델 type facts 및 증거 | 해당 실험을 재구현하거나 새 기본값으로 채택하지 않음 |
| [#5][pr5] | provider timing 및 dispatch/preparation attribution | 계측 수정·trace schema 확장 없음 |
| [#6][pr6] | D/U material identity / shared-pair lookup 후보 및 Mac 결과 | 조회·memo/cache 변경 없음 |
| [#7][pr7] | retired material storage pool 및 외부 감사 packet | storage/lifetime 변경 없음. 기존 packet을 복제하거나 덮어쓰지 않음 |
| [#8][pr8] | 순차 감사 계획 및 조사 지시 | 계획의 후속 **발견 보고서**만 제공. PLAN.md 변경·중복 계획 작성·구현 누적 없음 |
| 이 문서 | R01–R12 발견 목록, 증거 한계, 범위 분리, 다음 원인 카드 | main에서 독립 분기한 docs-only 변경. 다른 PR의 engine commit을 diff에 포함하지 않음 |

### 후속 구현의 작업 단위

- **R01 구현 PR:** SensorLOS의 보수적 raster bounds 변경과 그 변경에 불가분한 테스트만. 아직 생성/배정하지 않았다. 시작 전 열린 PR의 title/body와 changed paths를 다시 확인하고 이 ID를 명시한다.
- **R02/R03:** R01에서 확인한 규칙을 재사용할 수 있는지 검토한 뒤, 같은 계약이면 후속 소비자 적용으로 연결한다. 별도 추측성 구현을 동시에 시작하지 않는다. 차이가 있으면 그 차이를 기록하고 독립 검증한다.
- **R04–R12:** 측정/소비자 연결 대기 또는 이미 존재하는 경로. 현재 우선순위 확정이나 소스 변경 예약이 아니다.
- **S01 pass 중첩:** 별도 원인 카드. `Metal4WorldRenderer.mm` 등 #3/#4와 겹칠 수 있는 파일의 변경은 별도 비교·조정 후 진행한다. R01에 barrier 제거·encoder 분할·queue-depth 확대를 넣지 않는다.

**같은 원인·같은 함수를 수정하는 PR이 이미 생겼다면 새 경쟁 PR을 만들지 않고 해당 PR에 근거를 연결한다.** 문서와 실제 엔진 기준을 구분하며, #7이 분석 source에 들어 있다는 사실을 #7 후보의 활성화·채택 승인으로 해석하지 않는다.

## 7. 첫 후속 원인 카드: R01 — SensorLOS 처리 영역 감소

**가설:** 현재 선을 둘러싼 축 정렬 사각형이 필요 이상의 Fragment 후보 영역을 만들며, 그 영역에서 정확한 coverage 판정과 후속 계산이 실행된다. 기존 의미를 유지하는 더 좁은 보수적 bounds가 작업을 줄일 수 있다. 예상 FPS 향상률은 아직 없다.

검토할 변경은 **coverage predicate 자체가 아니라 predicate 앞의 도형 영역**이다. 단순히 회전된 얇은 quad로 바꾸면 동일해진다고 가정하지 않는다. 모든 방향·끝점·선 폭·subpixel snapping·clipping·viewport 경계에서 기존 경로가 허용하는 픽셀/샘플을 보존해야 한다.

유지할 계약:

1. 기존 1× diamond-exit / width / 끝점 규칙과 원근 보간, 명시적 depth 값.
2. mask의 producer→consumer 및 version/lifetime, sample 좌표와 implicit derivatives, 최종 discard 전의 계산 의미. bounds 변경으로 helper-lane/quad 경계 동작도 달라질 수 있으므로 ‘predicate를 그대로 뒀다’만으로 화면 동등성을 주장하지 않는다.
3. depth/stencil/blend 및 color write 의미. depthWrite OFF라도 depth test와 stencil 결과를 보존한다.
4. MSAA는 별도 rectangle/sample 계약. 1× 변경을 자동 확장하지 않으며 기존 다른 sample 경로를 보존한다.

최소 검증 순서:

`기존 경로와 후보 bounds coverage 비교 → 색/마스크/depth/stencil 및 경계 사례 확인 → 한 가지 변경만 동일 fixture에서 측정 → 채택/보류/기각`

성능 계측과 clean FPS 측정을 분리한다. 가능한 실제 GPU 작업 지표와 shader 시간, 전체 처리율·하위 꼬리를 함께 기록한다. 도구가 제공하지 않는 Fragment 호출 수는 만들어 내지 않는다. 불리한 유효 실행도 보존한다. 작업량만 감소하고 FPS 향상이 없으면 그 결과를 그대로 남기며, unrelated 후보를 덧붙이지 않는다.

## 8. 비용을 과장하지 않기 위한 반례와 보류

- 사각형을 두 삼각형으로 표현한 것 자체는 병목 증거가 아니다. R07/R09/R10처럼 이미 직접·전용 경로가 있다.
- 복잡한 shader 코드가 있다고 현재 fixture에서 실행되는 것은 아니다. R12는 호출 증거 전까지 hot path로 집계하지 않는다.
- Sensor31657의 원 순회, 사거리의 heightmap 탐색, 데칼의 discard는 원래 효과 계산일 수 있다. 원본과 대조 없이 Native가 추가한 비용이라고 부르지 않는다.
- 제공된 캡처에서는 ‘매 프레임 MRT 6장 보존 복사’가 확인되지 않았다. `omitted_copies=6`을 실제 복사 6회로 해석하지 않는다. 다른 모든 프레임의 부재까지 입증한 것은 아니다.
- S type-facts의 제공 RESULT는 work reduction을 보였지만 FPSGainEstablished=false다. CPU 작업 감소를 GPU/FPS 향상으로 소급하지 않는다. 관련 원본 실험은 #4–#7에서 유지한다.
- 정확한 원시 capture/replay/API dump와 모든 source/binary 대응은 이 문서에 새로 재수집되지 않았다. 여기의 source 확인과 제공 보고서의 런타임 관측을 구분한다.

## 9. 제공 자료와 추적 가능성

아래는 이번 게시 요청에서 읽은 자료다. 원본의 실제 로컬 파일 bytes에서 SHA256을 계산했다. **hash는 파일 식별이며 원시 측정의 정확성 인증이 아니다.** 원본 HTML·이미지·trace·개인 경로가 포함된 JSON 전체는 이 PR에 복제하지 않았다. 원본 HTML의 상대 링크가 이 저장소에서 열리는 것으로 가장하지 않는다. 이 보고서에는 관측 요약·한계·source 연결만 기록했다.

| 제공 파일 | 제목 / 역할 | bytes | SHA256 |
|---|---|---:|---|
| `REPORT(1).html` | BAR Native / MVK3 — Fragment 처리와 중첩 비교; §3.1 및 새 재생 관측의 출처 | 17986 | `0dc183899c780ad6aa2aea5d9aba3291c3abcd9090c3f305506a026fa1ccdf06` |
| `REPORT.html` | Ultra AA OFF CPU–GPU 비교; MST와 CPU 통계의 출처 | 15036 | `eaea0eef5c639a9e975ebbe636041a8de50fb4598d5cadc4f4330c640f67b88c` |
| `RESULT.json` | VALIDATED_WORK_REDUCTION_NO_FPS_GAIN; S type-facts 결과 | 3536 | `aa84fa5d764e5a43646f5685605ddf1624975103596352860f4e22e8b76b09b5` |
| `SELECTION.json` | MEASURED_AREA_SELECTED_REUSE_KEY_UNPROVEN; 과거 CUS provider 후보 선택의 한계 | 1869 | `b535c2c5af902973273fcd53a254707dda87c1be39b420c2fef31f3396ff4df7` |

이미 공개된 [기존 외부 감사 packet][handoff]은 초기 비교·identity의 별도 출처다. 그 packet에 이번에 제공된 후속 GPU 캡처 원자료까지 포함됐다고 주장하지 않는다. 후속 구현에서 필요하면 정확한 원시 파일과 분석 절차를 별도로 연결해야 한다.

소스의 기존 Spring/Recoil 및 원본 widget/shader 기여·라이선스 표기는 각 고정 source link에 남아 있다. 이 문서는 원작 알고리즘이나 기존 최적화를 새 작성자의 성과로 재지정하지 않는다.

**게시 결과의 의미:** 문서상 중복 범위를 분리하고 후속 검증 가능한 원인 목록을 만들었다. 성능 개선, 그래픽 parity, 전수감사 완료 또는 기존 PR 채택을 선언한 것이 아니다.

[plan]: https://github.com/yeojuny/BAR-Metal4/blob/dea21eee99a33c2df00daeb988a9ed24963cdd06/docs/research/native-mvk3-sequential-audit-20261115/PLAN.md
[instruction8]: https://github.com/yeojuny/BAR-Metal4/pull/8#issuecomment-6907985478
[handoff]: https://github.com/yeojuny/BAR-Metal4/blob/910fa9cdf60c9f2c566722493356fe3a99027e3d/docs/research/native-mvk3-external-audit-20261115/AUDIT-HANDOFF.md
[los]: https://github.com/yeojuny/BAR-Metal4/blob/a3c7c6bfa93d09dac25755534195932d12fff41a/engine/rts/Rendering/Metal4/NativeSensorLOSShaders.h
[range]: https://github.com/yeojuny/BAR-Metal4/blob/a3c7c6bfa93d09dac25755534195932d12fff41a/engine/rts/Rendering/Metal4/NativeWeaponRangeShaders.h
[range-renderer]: https://github.com/yeojuny/BAR-Metal4/blob/a3c7c6bfa93d09dac25755534195932d12fff41a/engine/rts/Rendering/Metal4/Metal4WeaponRangeNative.mm
[circle]: https://github.com/yeojuny/BAR-Metal4/blob/a3c7c6bfa93d09dac25755534195932d12fff41a/engine/rts/Rendering/Metal4/NativeSensor31657CircleShaders.h
[world]: https://github.com/yeojuny/BAR-Metal4/blob/a3c7c6bfa93d09dac25755534195932d12fff41a/engine/rts/Rendering/Backend/NativeWorldDrawGeometry.cpp
[point]: https://github.com/yeojuny/BAR-Metal4/blob/a3c7c6bfa93d09dac25755534195932d12fff41a/engine/rts/Rendering/Metal4/NativePointCoverageShaders.h
[filled]: https://github.com/yeojuny/BAR-Metal4/blob/a3c7c6bfa93d09dac25755534195932d12fff41a/engine/rts/Rendering/Metal4/NativeScreenFilledShaders184.h
[cus]: https://github.com/yeojuny/BAR-Metal4/blob/a3c7c6bfa93d09dac25755534195932d12fff41a/engine/rts/Rendering/Metal4/Metal4CUSSource.mm
[source-line]: https://github.com/yeojuny/BAR-Metal4/blob/a3c7c6bfa93d09dac25755534195932d12fff41a/engine/rts/Rendering/Metal4/NativeSourceLineRasterShaders130.h
[msaa-line]: https://github.com/yeojuny/BAR-Metal4/blob/a3c7c6bfa93d09dac25755534195932d12fff41a/engine/rts/Rendering/Metal4/NativeMultisampleLineShaders.h
[exact]: https://github.com/yeojuny/BAR-Metal4/blob/a3c7c6bfa93d09dac25755534195932d12fff41a/engine/rts/Rendering/Metal4/NativeExactWorldLineShaders.h
[pr1]: https://github.com/yeojuny/BAR-Metal4/pull/1
[pr3]: https://github.com/yeojuny/BAR-Metal4/pull/3
[pr4]: https://github.com/yeojuny/BAR-Metal4/pull/4
[pr5]: https://github.com/yeojuny/BAR-Metal4/pull/5
[pr6]: https://github.com/yeojuny/BAR-Metal4/pull/6
[pr7]: https://github.com/yeojuny/BAR-Metal4/pull/7
[pr8]: https://github.com/yeojuny/BAR-Metal4/pull/8
