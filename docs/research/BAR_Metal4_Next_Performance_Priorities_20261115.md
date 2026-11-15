# BAR Native Metal4 — MVK3 추월을 위한 다음 성능 개선 우선순위

**작성일:** 2026-11-15  
**용도:** 코드 에디터에게 전달하는 분석·실험 인계서  
**주 실행 정책:** PREPARE  
**작성 범위:** 직전 대화의 분석을 문서화한 자료. 이번 문서화 과정에서는 새 벤치마크, 원본 trace 재집계, 소스 수정 또는 GitHub 변경을 수행하지 않았다.

---

## 1. 결론

다음 큰 성능 향상을 기대할 우선 후보는 다음 두 가지다.

1. **표시용 drawable 확보·GPU 제출·완료 정산 때문에, 그와 독립적으로 진행할 수 있는 다음 프레임의 CPU 작업까지 멈추는 경계를 줄인다.**
2. **CUS·지형의 공통 재질 준비를 draw마다 다시 수행하지 않고, 정확한 내용·자원 버전·실행 범위에 맞춰 재사용한다.**

단, 첫 번째는 **정상 화면에서 먼저 확인할 가설**이다. 공개 v1.11.3 진단의 큰 drawable 대기는 잠금 화면에서 관측됐고, 약 185 FPS를 기록한 실험과 장면도 다르다. 그 수치를 정상 플레이의 제거 가능한 비용으로 취급하면 안 된다.

정상 조건에서 drawable 대기가 작다면 이 가설의 우선순위를 즉시 내린다. 실제 worker CPU 준비 또는 GPU 패스·제출·회수 중 현재 큰 비용으로 이동한다.

**Sepia 이식이나 범용 GLSL/Lua 경로 전체를 완성하는 일을 현재 성능 개선의 필수 선행 조건으로 두지 않는다.** 공통 재질·바인딩 계약은 현재 실행되는 CUS 또는 지형의 좁은 경로에서 반복 준비를 실제로 줄이는 데 사용한다.

---

## 2. 검토 기준과 증거의 범위

### 2.1 서로 다른 기준을 섞지 않는다

| 구분 | 기준 | 이 문서에서의 용도 |
|---|---|---|
| 현재 배포·진단 소스 | v1.11.3, 배포 태그 commit `0b0fd234af3bf5dd073ab7094350ec63fc3a7e04` | 현재 완료·제출·계측 경로 확인 |
| 배포본의 실제 컴파일 소스 | `ddd31f55961d6e715283ea48d03a2b35559128f2` | H + J 계측 보완이 포함된 엔진. 배포 문서상 태그와 engine tree가 동일 |
| 약 185 FPS 성능 기준 | H/I 체크포인트 `e9acbc66115c5fecd872f3385db248371362b97b` | 같은 체크포인트의 BEGIN/PREPARE 비교와 후속 목표 예산 |
| 과거 MVK3 기록 | native220 비교 캠페인의 약 226.02 FPS | 역사적 참고 목표. 현재 matched MVK3 결과가 아님 |

v1.11.3은 H 체크포인트에 J 계측 보완을 통합했으며, K 최적화와 v2 변경은 제외돼 있다. 기본 정책은 PREPARE이고 일반 실행에서는 trace와 Metal Validation이 꺼진다. 배포 확인은 다른 장면에서 수행됐으며, 기존 185 FPS와의 새로운 전후 성능 비교가 아니다. [배포 설명][release-readme]

### 2.2 현재 자료로 말할 수 있는 것과 없는 것

**말할 수 있는 것**

- Main의 `FinishCPUWork()`는 renderer job의 완료를 기다리며, 그 job에는 CPU 준비 외의 표시·제출·정산 단계도 포함될 수 있다.
- 공개된 잠금 화면 진단의 일부 작업에서는 실제 thread CPU 시간보다 worker 경과 시간이 훨씬 길고, drawable 획득을 둘러싼 구간이 길었다.
- 현재 CUS의 material cache 조회보다 앞에 provider resolution·자원 등록 등의 준비가 있다.
- 현재 source credit 수와 GPU pending depth는 서로 다른 개념이다.

**아직 말할 수 없는 것**

- 정상 플레이에서도 `nextDrawable`이 지배적인 병목이다.
- 잠금 화면에서 관측한 4~5 ms를 그대로 제거할 수 있다.
- 재질 준비 공통화만으로 특정 FPS에 도달한다.
- 185 FPS가 Native Metal4의 한계다.
- 현재 Native와 현재 MVK3의 동일 조건 격차가 정확히 41 FPS다.

근거: [배포 실행 기록][runtime-result], [H/I 체크포인트][checkpoint-readme], [현재 CUS 준비 소스][cus-source].

---

## 3. 새로운 단서: 공통 Join에 drawable 획득 대기가 들어 있다

### 3.1 직전 분석에서 연결한 두 작업 기록

다음은 v1.11.3 PREPARE 진단에서 직전 분석이 대조한 두 작업의 값이다. **전체 평균이 아니며, 이 문서 작성 단계에서 원본을 새로 재집계한 값도 아니다.**

| 항목 | Source draw 1580 | Source draw 1581 |
|---|---:|---:|
| Renderer 작업의 경과 시간 | 7.603 ms | 6.511 ms |
| 해당 구간의 실제 thread CPU 사용 시간 | 2.091 ms | 2.079 ms |
| Drawable 획득을 둘러싼 구간 | 5.549 ms | 4.140 ms |
| 명시적 이전 GPU event + feedback 대기 | 약 0.001 ms | 약 0.001 ms |

Source draw 1580을 기다린 다음 main 작업의 `TaskJoin`은 6.902 ms였다. Worker의 긴 경과 시간과 main의 긴 Join을 독립적인 비용으로 더하면 같은 시간을 중복 계산하게 된다.

관련 원자료 위치: [PREPARE 진단 폴더][prepare-diagnostic], [통합 runtime 결과][runtime-result].

### 3.2 코드에서의 측정 경계

직전 분석에서 확인한 핵심 경계는 다음 형태다.

```cpp
cpuPhase.Mark(4);

drawable = [layer nextDrawable];

cpuPhase.Mark(5);
```

따라서 이 구간을 CUS 검증·uniform packing의 CPU 계산 시간으로 분류하면 안 된다. 표시용 drawable을 얻는 호출과 그 주변 구간을 포함한다. [WorldRenderer][world-renderer]

### 3.3 이 관측의 가장 중요한 제한

이 진단은 **디스플레이 잠금 상태**였고, 기존 약 185 FPS 실험과 다른 장면에서 수행됐다. 공개 배포 문서도 foreground appearance 및 전체 화면 동등성이 관측되지 않았다고 명시한다. [배포 검증 범위][release-readme]

그러므로 다음과 같이 해석해야 한다.

> 정상 화면에서도 이 대기가 main의 진행에 크게 노출되는지 먼저 확인할 가치가 있다. 그러나 이 자료만으로 정상 플레이의 주병목이나 제거 가능한 시간을 확정할 수는 없다.

또한 `nextDrawable` 경과 시간만으로 drawable pool 부족, 표시 정책, GPU 사용 지연, 스케줄링 중 어느 하나를 원인으로 확정하지 않는다. 가용 drawable을 기다릴 수 있다는 API 성격과, 이 엔진에서의 실제 원인은 구분한다. [Apple `nextDrawable` 문서][apple-nextdrawable]

---

## 4. 큰 점프 후보 A — 표시·제출 경계와 독립적인 CPU 작업 분리

### 4.1 현재의 문제 가능성

현재 main은 이전 renderer job의 완료를 기다린다. 그 job 안에 다음이 함께 들어갈 수 있다.

- 렌더 준비와 명령 기록.
- 표시용 drawable 확보.
- 이전 GPU submission 완료 확인과 정산.
- 현재 submission 제출.
- 완료 통지와 source 소유권 처리.

따라서 main이 다음 capture를 수행하기 위해 정말 필요한 CPU 조건보다 더 넓은 완료 범위를 기다릴 가능성이 있다. [Worker settlement][settlement], [WorldRenderer][world-renderer], [Runtime][runtime-source]

### 4.2 목표

> Drawable 없이 진행할 수 있는 준비·offscreen 작업은 먼저 수행하고, 실제 drawable을 요구하는 최종 단계만 drawable 확보 시점에 연결한다.

현재 코드에는 `finalColorStage` 중간 색상 텍스처 경로가 있다. 그러나 중간 텍스처를 쓴다는 사실만으로 drawable 대기와 독립적으로 준비가 진행되는 것은 아니다. 직전 분석에서는 해당 경로도 먼저 `nextDrawable`을 호출한 뒤 중간 화면과 후처리 준비를 이어가는 순서를 확인했다. [WorldRenderer][world-renderer]

### 4.3 반드시 구분할 세 상태

| 상태 | 의미 |
|---|---|
| 다음 capture가 안전해짐 | 이전 worker가 다음 capture와 충돌하는 source 상태를 더 이상 사용·변경하지 않음 |
| 현재 프레임의 제출이 끝남 | GPU가 실행할 명령을 제출함 |
| GPU 사용이 끝남 | 실제 완료 조건을 만족해 관련 자원을 재사용할 수 있음 |

이 세 상태는 서로 같지 않다. 분리가 가능한 범위를 찾아야 하지만, 첫 상태를 선언했다고 두 번째·세 번째 상태의 수명 요구까지 제거되는 것은 아니다.

### 4.4 구현 전에 확인할 사항

- 실제 drawable 크기·format·sample count 없이 어느 작업까지 안전하게 준비할 수 있는가?
- Resize·화면 전환·drawable 확보 실패 시, 이미 준비한 작업과 source journal을 어떻게 처리하는가?
- Main을 먼저 진행시켰을 때 worker가 읽는 source state가 바뀌지 않는가?
- 기존 source credit와 GPU 완료 장부를 재사용할 수 있는가?
- Drawable이 얻어진 시점부터 반환되는 시점까지 불필요하게 오래 보유하는 구간이 있는가?

### 4.5 해서는 안 되는 변경

- `FinishCPUWork()`를 근거 없이 삭제한다.
- Worker가 계산을 마쳤다는 이유로 GPU 자원도 재사용한다.
- 현재 완료 장부와 별도의 임시 lifetime 체계를 만든다.
- 대기 위치만 바꿨는데 처리율 개선으로 보고한다.
- 바로 thread 수나 drawable/GPU queue depth부터 늘린다.

Drawable pool 또는 표시 경로 자체가 처리율을 제한한다면 준비를 앞당겨도 실제 표시 처리율은 그대로일 수 있다. **대기 이동, CPU 유효 작업 증가, 전체 처리율 개선을 따로 확인한다.**

### 4.6 우선순위 판정

정상 화면의 대표 장면에서도 큰 drawable 대기가 main Join에 노출된다면 첫 구조 변경 후보로 올린다. 잠금 상태에 국한되거나 정상 조건에서 작다면 이 작업을 확대하지 않고 다른 큰 비용으로 이동한다.

---

## 5. 큰 점프 후보 B — 공통 재질 준비를 draw 단위에서 변경 단위로 이동

### 5.1 현재 CUS material cache의 한계

현재 CUS 준비는 대략 다음 순서다.

```text
Source draw 검증
  → CPU read-set 확인
  → draw argument table 확보
  → 공통 buffer·publication 준비
  → active texture마다 provider 호출
  → sampler·texture 형식·좌표 처리 확인
  → 자원 등록·owner 보유
  → material cache 조회
  → draw별 uniform·pipeline 준비
```

따라서 material cache hit가 나더라도 그 앞의 provider resolution·검증·등록 작업은 이미 수행했을 수 있다. **캐시 적중 수만으로 전체 준비 비용이 줄었다고 판단하면 안 된다.** [현재 CUS 준비][cus-source]

### 5.2 목표는 공통 함수가 아니라 재사용 가능한 준비 결과다

| 준비 내용 | 재사용 판단 단위 |
|---|---|
| 프로그램 입력 배치·타입·사용 슬롯 | 프로그램의 정확한 세대와 인터페이스 |
| 불변 uniform·재질 내용 | 내용이 실제로 바뀌는 시점 |
| Texture/view/sampler 해결 결과 | 정확한 자원 버전·sampling 상태·사용 조건 |
| Submission의 자원 등록 | 증명된 동일 lease·사용 범위 |
| Pose·instance·draw별 값 | 해당 draw 또는 실제 변경 범위 |

이 구조에서 공통 재질·바인딩 계약 v2가 실익을 갖는다.

```text
같은 함수를 사용한다
    ≠ 같은 실행을 한 번만 수행한다

정확한 불변 입력과 유효 범위를 공유한다
    → 이미 끝낸 준비 결과를 재사용할 수 있다
    → 실제 반복 작업량이 줄 수 있다
    → 그 작업이 임계 경로에 있을 때 처리율이 개선된다
```

### 5.3 반드시 남겨야 할 의미

- Texture 주소나 GL 이름이 같아도 내용의 버전·view·sampling 의미가 다르면 구분한다.
- 정적 타입·범위 검증의 재사용과 현재 producer readiness 확인을 구분한다.
- 같은 lease라는 이유만으로 원래 draw의 consumer 순서를 무시하지 않는다.
- 이미 draw가 보유한 immutable publication은 새 uniform write·relink·삭제로 변하지 않아야 한다.
- 자원 등록·completion callback을 생략하려면 그 작업의 의미까지 중복임을 증명한다.

### 5.4 첫 적용 범위

전체 GLSL/Lua 전환이나 Sepia 완료를 기다리지 않는다. 현재 사용하는 CUS 또는 지형의 좁은 재질 계열 하나를 선택해 다음을 확인한다.

1. 기존에 어떤 준비가 몇 번 반복되는가?
2. 그중 정확히 같은 입력으로 재사용 가능한 것은 무엇인가?
3. 새 준비 결과가 어떤 기존 owner·publication·lease에 종속되는가?
4. 변경 후 검증·provider 호출·binding 생성·등록 횟수가 실제로 줄었는가?
5. Worker CPU 시간과 main에 노출된 대기, clean 처리율은 각각 어떻게 바뀌었는가?

프로그램 생성·내용 identity·실행 범위를 기존 상위 계약에 연결해, 새 cache와 별도 lifetime 체계를 임시로 쌓지 않는다.

### 5.5 효과에 대한 제한

현재 약 185 FPS 장면에서 이 작업이 몇 ms를 차지하는지는 아직 확정되지 않았다. 잠금 화면 진단의 drawable 대기를 이 최적화로 없앨 수 있다고 설명해서는 안 된다.

---

## 6. GPU 쪽을 함께 열어 두어야 하는 이유

과거 통제 비교의 command-buffer interval union/draw는 MVK3 약 3.213 ms, Native220 약 5.130 ms였다. 그러나 이 구간에는 의존성·대기가 포함되고 backend별 관측 범위도 다르다. 이를 순수 shader 실행 시간 차이나 현재 엔진의 GPU 비용으로 사용하면 안 된다. [과거 비교][old-comparison]

현재도 남는 질문은 다음이다.

> 같은 화면과 작업량에서 Native가 MVK3보다 더 많은 GPU 작업이나 불필요한 실행 경계를 만드는가?

정상 조건에서 GPU가 제한 요인으로 확인되면, 다음 중 측정된 큰 원인 하나를 선택한다.

- 큰 패스의 shader 연산·texture 접근.
- 실제 보존 복사와 중간 image의 대역폭.
- 불필요한 pass 종료·재개 또는 지나치게 넓은 barrier.
- GPU 완료 이후의 feedback·residency·callback·회수 경계.

이것들은 후보이지 이미 큰 비용으로 확인된 목록이 아니다.

### MRT209와 GPU 동시 제출의 위치

MRT209 일반 재사용을 회복하지 않은 상태에서도 약 185 FPS에 도달했다. 그러므로 MRT209를 복원해야만 다음 큰 개선이 가능하다고 전제하지 않는다. 실제 copy bytes·GPU 시간·allocation·pool miss를 보고 판단한다. [H/I 결과와 계약][checkpoint-readme]

GPU pending 확대도 조건부 후속 과제다. Source credit 수는 GPU inflight depth와 다르다. 더 깊은 queue가 도움이 되려면 준비된 작업이 실제로 있고, slot·allocator·residency·producer image·read-check·완료 이벤트의 수명이 설명돼야 한다.

다음 제출이 늦게 준비되는 상황에서 queue만 확대하면 처리율 개선 없이 지연과 보유 자원만 늘 수 있다. 현재 single-outstanding-submission 규칙을 성능 때문에 무조건 해제하지 않는다.

---

## 7. MVK3 추월에 필요한 시간 예산

현재 보존된 I PREPARE 결과 185.106 FPS를 시간으로 환산하면 약 **5.402 ms**다.

| 목표 | 목표 전체 주기 | 현재에서 줄여야 할 시간 | 필요한 FPS 증가 |
|---|---:|---:|---:|
| 200 FPS | 5.000 ms | 0.402 ms | +8.05% |
| 226.02 FPS | 4.424 ms | 0.978 ms | +22.10% |
| 230 FPS | 4.348 ms | 1.054 ms | +24.25% |
| 240 FPS | 4.167 ms | 1.236 ms | +29.66% |

**이 표는 요구 조건이며 성능 예측이 아니다.**

185.106 FPS는 H/I 체크포인트 기록이고, 226.02 FPS는 과거 MVK3 캠페인의 값이다. 현재 동일 장면·동일 설정에서 측정한 두 엔진의 격차가 아니다. 현재 MVK3를 넘었다는 판정은 새 matched 비교에서 해야 한다. [H/I 기준][checkpoint-readme], [과거 MVK3 기준][old-comparison]

개발상 의미는 약 **1 ms** 규모의 추가 단축이 필요하다는 것이다. 몇 µs짜리 후보를 무작정 누적하기보다 실제 프레임 진행을 막는 수백 µs급 작업이나 대기를 찾아야 한다.

동시에 다음 계산은 금지한다.

- 관측된 5 ms 대기 전체를 제거 가능한 예산으로 계산.
- Main Join 시간과 그 안에 포함된 worker wait를 합산.
- CPU 0.5 ms와 GPU 0.5 ms 감소를 무조건 전체 1 ms 단축으로 계산.
- CPU 관측 DrawScreen FPS를 실제 화면 표시율과 동일시.

---

## 8. 권장 다음 실험 — 기존 계측으로 세 경계를 연결한다

### 8.1 기준 조건

- v1.11.3의 source·deployed engine·실제 설정·프리셋을 고정한다.
- 기존 185 FPS 조건과 새 배포 smoke 장면을 섞지 않는다.
- 대표 성능 실행은 잠금 해제·전면 상태에서 수행한다.
- 실제 render 크기, AA, VSync, pacing·offscreen 제한과 전원 조건을 기록한다.
- 진단 실행과 clean FPS 실행을 분리한다.
- 기존 기준과 실패·저성능 실행도 보존한다.

기존 J 계측과 drawable 경과 구간을 먼저 사용한다. 분석에 충분한 정보가 있으면 새로운 대형 계측 체계를 만들지 않는다.

### 8.2 연결할 질문

| 구간 | 판별할 질문 |
|---|---|
| Main Join | 지금 진행하려는 capture가 어느 render job을 기다리는가? |
| Worker 준비·drawable 획득 | Main이 기다리는 동안 worker는 CPU 계산 중인가, drawable을 기다리는가? |
| 이전 GPU 정산·현재 submit | GPU 완료가 늦은가, 완료 후 feedback·등록·회수·제출이 늦은가? |

### 8.3 식별자는 서로 구분한다

다음 세 개를 같은 draw 번호라고 가정하지 않는다.

1. 기다리는 main의 incoming capture/request.
2. Join 대상 render job.
3. 그 job이 retire하는 predecessor lease/submission/event.

J는 이 식별자를 구분하도록 보완된 상태다. 기존 연결 정보를 사용하고, 단순히 `draw - 1`로 관계를 추정하지 않는다. [v1.11.3 J 설명][release-readme]

### 8.4 결과에 따른 의사결정

| 관측 결과 | 다음 단일 변경 |
|---|---|
| 정상 화면에서도 큰 drawable 대기가 main 진행에 노출됨 | Drawable 독립 준비·offscreen 작업과 최종 표시 단계를 분리할 좁은 경계 검토 |
| 실제 worker CPU 준비가 지배함 | CUS 또는 지형 한 경로의 공통 material 준비 결과 재사용 |
| Predecessor GPU event 또는 큰 패스가 지배함 | 해당 GPU 패스·copy·제출 의존성 하나 축소 |
| GPU는 끝났는데 feedback·정리·lock 비용이 큼 | 정확한 retirement·등록·callback 경로 축소 |
| Join에 노출된 시간이 작음 | Join 최적화를 확대하지 말고 다른 큰 구간 선택 |

정상 조건에서 drawable 가설이 지지되지 않으면 즉시 우선순위를 내린다. 새로운 단서라는 이유로 계속 고집하지 않는다.

---

## 9. 변경의 수락 기준

### 9.1 정합성

변경한 경로의 원래 draw 순서, producer–consumer 관계, uniform·texture 의미, source owner와 GPU 완료 조건을 유지한다. 실제 변경 범위에 맞춰 화면·오류·종료·필요한 resize 경로를 확인한다.

### 9.2 작업량

해당 가설에 맞는 최소 counter를 기록한다.

- 재질 준비 재사용: 검증·provider resolution·binding 생성·등록 횟수, hit/miss와 miss 이유.
- 표시 경계 분리: drawable 획득 경과, 보유 기간, 실제 조기 실행된 작업 구간, fallback 횟수.
- GPU 변경: 실제 copy·allocation·pass·barrier 변화와 해당 GPU 구간.

기존 counter가 있으면 재사용한다. 모든 후보를 위해 대규모 계측부터 추가하지 않는다.

### 9.3 성능

- 같은 workload의 반복 clean 비교에서 평균 FPS·느린 프레임 구간을 확인한다.
- 처리율 증가가 화면 작업 생략이나 달라진 설정 때문이 아닌지 확인한다.
- 큐 또는 보유 세대가 달라지는 변경은 지연과 retained memory도 함께 본다.
- 작업량이 줄었지만 FPS가 그대로인 결과도 그대로 기록한다.
- 서로 다른 가설의 큰 변경을 한 후보에 한꺼번에 넣지 않는다.

### 9.4 진행 방식

설계는 기존 공통 계약 위에서 함께 정리하되, 성능 실험은 원인 하나씩 나눈다. 구조 개선과 실제 성능 개선을 별도의 결과로 보고한다.

---

## 10. 에디터에게 전달할 실행 지시문

> 다음 작업은 Sepia나 범용 GLSL/Lua 전체 구현이 아니라, 정상 화면의 PREPARE에서 현재 공통 Join의 원인을 연결하는 것이다.
>
> v1.11.3의 기존 J trace와 drawable 획득 구간을 사용해, incoming capture → joined render job → retired predecessor submission 관계를 확인하라. 잠금 화면 진단의 4~5 ms drawable 대기를 정상 플레이의 제거 가능한 비용으로 사용하지 말라.
>
> 정상 조건에서도 drawable 획득이 큰 노출 대기라면, drawable 없이 가능한 준비·offscreen 작업과 최종 표시 단계를 분리할 최소 경계를 제안하라. Source capture 허가, CPU job 종료, GPU completion의 수명을 구분하고 기존 완료 장부를 재사용하라.
>
> 실제 CPU 준비가 더 크다면, 현재 사용하는 CUS 또는 지형의 한 경로에서 material cache 앞에 남은 반복 provider resolution·검증·등록을 줄여라. 공통 함수로 감싸는 것에 그치지 말고, 정확한 program/content/resource version/lease 조건의 준비 결과를 재사용하라.
>
> GPU event·패스·회수 비용이 지배하면 그 원인 하나를 선택하라. 공통 v2나 Sepia 완료를 그 성능 수정의 필수 선행 조건으로 만들지 말라.
>
> 한 가설과 한 변경으로 작업량, 노출 대기, clean FPS를 각각 확인하라. GPU pending 확대와 MRT209 일반 재사용은 실제 필요성과 수명 증명이 확보된 뒤 검토하라. 200·230 FPS는 목표 예산이지 보장 수치가 아니다.

---

## 11. 최종 판단

다음 승부처는 **worker를 더 일찍 시작하는 것 자체**가 아니다.

- 시작한 worker가 표시·제출을 기다리는 동안 독립적인 유효 작업까지 멈추는지 확인한다.
- 현재 매 프레임 쓰는 재질 준비에서 같은 해석·검증·등록을 반복하지 않게 한다.
- GPU가 제한 요인이면 그 패스·복사·회수 원인을 직접 줄인다.

**185 FPS를 한계로 볼 근거는 없지만, 230 FPS에 필요한 약 1.05 ms의 절감 원인을 아직 특정하지는 못했다.** 현재는 범용 기능을 넓히기보다 그 시간을 제공할 한 경로를 정확히 선택할 단계다.

---

## 참고 자료

- [v1.11.3 배포·소스·검증 범위][release-readme]
- [v1.11.3 runtime 결과][runtime-result]
- [v1.11.3 PREPARE critical-path 자료][prepare-diagnostic]
- [H/I BEGIN–PREPARE 체크포인트][checkpoint-readme]
- [과거 MVK3–Native220 비교][old-comparison]
- [Apple: CAMetalLayer.nextDrawable][apple-nextdrawable]

[release-readme]: https://github.com/yeojuny/BAR-Metal4/blob/0b0fd234af3bf5dd073ab7094350ec63fc3a7e04/docs/releases/v1.11.3/README.md
[runtime-result]: https://github.com/yeojuny/BAR-Metal4/blob/0b0fd234af3bf5dd073ab7094350ec63fc3a7e04/docs/releases/v1.11.3/RUNTIME-RESULT.json
[prepare-diagnostic]: https://github.com/yeojuny/BAR-Metal4/tree/0b0fd234af3bf5dd073ab7094350ec63fc3a7e04/docs/releases/v1.11.3/runs/prepare-critical-path
[checkpoint-readme]: https://github.com/yeojuny/BAR-Metal4/blob/e9acbc66115c5fecd872f3385db248371362b97b/docs/checkpoints/begin-prepare-20261114/README.md
[old-comparison]: https://github.com/yeojuny/BAR-Metal4/blob/5bc9cb313574e3f86758b429975d5becfadd7dbf/docs/research/mvk3-native220/README.md
[world-renderer]: https://github.com/yeojuny/BAR-Metal4/blob/0b0fd234af3bf5dd073ab7094350ec63fc3a7e04/engine/rts/Rendering/Metal4/Metal4WorldRenderer.mm
[runtime-source]: https://github.com/yeojuny/BAR-Metal4/blob/0b0fd234af3bf5dd073ab7094350ec63fc3a7e04/engine/rts/Rendering/Metal4/Metal4RuntimeNative.mm
[settlement]: https://github.com/yeojuny/BAR-Metal4/blob/0b0fd234af3bf5dd073ab7094350ec63fc3a7e04/engine/rts/Rendering/Backend/NativeRenderJobSettlement.h
[cus-source]: https://github.com/yeojuny/BAR-Metal4/blob/0b0fd234af3bf5dd073ab7094350ec63fc3a7e04/engine/rts/Rendering/Metal4/Metal4CUSSource.mm
[apple-nextdrawable]: https://developer.apple.com/documentation/quartzcore/cametallayer/nextdrawable()
