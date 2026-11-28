# BAR-Metal4 범용 최적화 실행 지시서

작성일: 2026-11-22. 사용자 지시: 독립적인 후보를 안전하게 바로 수정할 것, GPU/사람 확인이 필요한 것, 실제 풀 맵에서 확인해야 할 것으로 나누고, 수정 가능한 것부터 진행한다.

## 실행 원칙

이 문서는 후속 native 실행 담당자가 바로 사용할 작업 순서다. 문서가 GitHub에 올라갔다는 사실은 Mac 작업이나 자동화가 실행됐다는 뜻이 아니다. 실제 실행 결과를 기록하기 전에는 완료 표시를 바꾸지 않는다.

- 맵 이름, tick, seed, 고정 카메라/해상도, 관측 draw 수를 런타임 선택 조건으로 사용하지 않는다.
- 실제 현재 native baseline의 소스와 바이너리를 고정하고 패치를 적용한다. 감사 기준과 로컬 Mac 최신 source가 다르면 차이를 확인해 이식한다. 오래된 파일로 전체 덮어쓰지 않는다.
- A군은 별도의 사용자 승인 대기 없이 수정·기본 검증을 진행한다. 자동 검증이 가능한 B군도 사람 승인을 매번 요구하지 않는다.
- C군은 양성 grass 장면 확보가 필요하다. 사람이 보는 검사는 자동 정확성 검증을 대체하지 않는다.
- 그래픽 효과 생략, 품질/해상도/업데이트 빈도 축소, 원래 유닛 목록 교체, 프레임/자원 수명 완화는 승인된 변경이 아니다.
- 서로 다른 원인군을 한 FPS 비교에 섞지 않는다. 이번 A군은 하나의 GroundFlash 준비 중복 제거 묶음이다. 필요하면 행렬/바인딩/depth 부분을 별도 commit/실험으로 분리해 비용을 귀속시킨다.

## A — CPU 준비 의미가 같아 먼저 수정할 수 있는 것

| ID | 작업 | 현재 상태 | 다음 gate |
|---|---|---|---|
| A1 | 같은 Prepare의 GroundFlash에 대해 동일한 272바이트 UBO와 실제 동일 resource binding 재사용 | **소스 구현 및 독립 검토 완료** | 현재 macOS/Metal SDK 빌드, 기본 출력 회귀 |
| A2 | 같은 Prepare에서 GroundFlash depthTest/depthWrite 정확한 조합별 immutable depth state 재사용 | **소스 구현 및 독립 검토 완료** | depth 조합별 원래 draw state 유지 확인 |
| A3 | GroundFlash 공통 view/projection 구성·곱과 입력 finite 검사를 최초 한 번만 수행 | **소스 구현 및 독립 검토 완료** | main 및 해당 준비 호출의 카메라 의미 유지 확인 |

변경 파일: [`Metal4WorldParticles.mm`](../../../engine/rts/Rendering/Metal4/Metal4WorldParticles.mm), `WorldParticlesGPU::Prepare` 내부만.

### 구현 계약

1. 재사용 대상은 `GroundFlashes` stage이고 ground atlas를 사용하는 range이다. 특정 맵이나 227개라는 수를 검사하지 않는다.
2. 상수는 전체 272바이트를 비교한다. hash 일치나 float epsilon으로 같다고 취급하지 않는다.
3. vertex buffer, atlas texture, terrain depth의 실제 객체와 GPU binding 값이 일치해야 한다.
4. cache miss면 기존 방식으로 새 UBO/table을 생성한다. cache 교체 시 기존 draw가 참조하는 table이나 buffer 내용을 수정하지 않는다.
5. cache는 Prepare 내부의 마지막 GroundFlash binding 한 개다. frame-global 상태가 아니며 A→B→A에서 마지막 A는 miss일 수 있다.
6. 모든 range에서 기존 `atlas()` 경로를 수행해 residency, pending mipmap, producer readiness를 보존한다.
7. UBO는 기존 `Buffer()`/`lease.AddAllocation`, table/depth는 `lease.Retain`으로 수명을 유지한다.
8. draw 수·순서·geometry·index range·pipeline 및 draw별 depth binding은 유지한다. non-GroundFlash는 기존 allocation 경로다.

### A군 검증 현황

- `git diff --check`: 통과.
- 수정 범위: 파일 하나, `Prepare` 내부. shader/Encode/LuaNano 경로 변경 없음.
- 독립 소스 검토: blocker 발견 없음.
- 감사 source와 main의 원본 대상 blob: 둘 다 `5197b0030d47a46c0c74e880798f80ae3a7935f6`.
- **macOS/Metal compile: 미실행. GPU parity: 미실행. FPS: 미측정.**

다음 담당자는 기존 검증된 native 빌드 절차로 컴파일하고, 값/자원 변화에 따른 cache miss, depth test/write 조합, 같은 Prepare 내 non-GroundFlash 혼합, 여러 frame 및 카메라 전환을 확인한다. 소스상 의미 보존이 입증됐다고 실제 SDK 컴파일 성공을 추정하지 않는다. 정상 빌드/기본 회귀가 통과하면 다른 맵 전용 승인을 기다리지 않고 A군 후속 통합을 진행할 수 있다.

## B — GPU 의미·의존성을 검증한 뒤 채택할 것

| ID | 작업 | 수정 가능 범위 | 채택 전 gate | 현재 상태 |
|---|---|---|---|---|
| B1 | Distortion `Clear → Volume…`을 하나의 encoder로 통합 | 전역 순서상 연속, 실제 attachment 동일, texture feedback 없음, 원래 per-draw state/버전 전이 유지 | intermediate RGBA16F 및 최종 출력 parity, masked/scissored clear, overlap/noop, 중간 소비자 fallback, 실제 pass 수 감소 | **설계 완료, 구현 대기** |
| B2 | SensorLOS의 남은 64-bit quotient/remainder를 bounded float estimate + exact integer correction으로 변경 | 기존 좌표/count guard와 fallback 유지, 정확한 remainder 비교 유지 | optimized Metal 코드/비용 확인, 기존 coverage/color/depth/stencil parity, 현재 baseline 비교 | **호스트 산술 검증만 완료** |
| B3 | GroundDecal flat-wrap의 현재 엔진 통합 | 기록된 정확한 patch/hash를 먼저 확보 | 기존 독립 시험을 현재 엔진 소스에 연결, 게임 장면에서 정확성/성능 검증 | **정확한 patch 확보 및 통합 대기** |
| B4 | Lua Nano normal/ghost 등 다른 경로의 동일 uniform 재사용 | 전체 값과 실제 binding이 같은 경우만, 두 pass/stencil 의미 유지 | 실제 중복 관측과 경로별 invariant 확인 후 좁은 구현 | **추가 근거 대기** |

**B1 상세 규칙:** Clear는 기존 draw로 유지한다. 크기 `756×475`, Volume 두 개라는 관측에 고정하지 않는다. CopyScreen/Combine/다른 family/target 변경에서 run을 끊는다. 이전 버전은 해당 operation이 존재하는 previous/inputs에 대해 순차 확인한다. output 버전 갱신과 noop 전이도 유지한다. queue barrier를 지원하지 않는 Fragment→Fragment intrapass wait로 바꾸지 않는다. 외부 producer/consumer와 frame completion은 그대로다.

**사람 확인이 필요한 경우:** 자동 비교의 불일치가 원래 undefined 동작·기준 renderer의 모호한 의미와 연결돼 추가 판단이 필요한 경우, 의도적인 품질/정확성 타협을 제안하는 경우, 자동 결과가 설명하지 못한 시각적 이상이 있는 경우다. 우선은 원인을 수정한다. 단순히 '눈으로 비슷함'을 자동 parity 실패의 면제로 사용하지 않는다.

## C — 실제 풀이 존재하고 휘는 맵/장면에서 확인할 것

| ID | 작업 | 선행 조건 | 현재 상태 |
|---|---|---|---|
| C1 | Grass draw별 보수적인 유닛 shortlist | 실제 unitBendCount와 원래 instance 범위 확인, rounding/fallback 계약, 양성 bending 장면 | **구현 지시 완료, Mac/장면 대기** |
| C2 | 남은 bending을 instance당 한 번 계산 | C1 후에도 반복 비용이 유의미하다는 계측, 추가 dispatch/buffer/dependency 비용 평가 | **C1 결과 이후 판단** |
| C3 | 다른 맵 이식성 및 실제 플레이 확인 | Simmer 포함, 실제 grass 경로/부하와 일반 플레이 상태 확인 | **미실행** |

실행 상세: [GRASS-VALIDATION.ko.md](GRASS-VALIDATION.ko.md).

풀 맵은 이름만 골라 통과로 처리하지 않는다. 설치된 맵 중 실제 native grass draw가 발생하고 유닛이 그 풀에 양성 bending을 일으키는 하나의 맵을 선택한 뒤 **맵 이름·버전/해시를 기록**한다. 현재 공개 증거만으로 다른 특정 맵이 이 조건을 만족한다고 확인하지 못했으므로 추측해서 지정하지 않았다. Simmer는 사용자 지정 이식성 확인 대상이며, 그곳에서 grass/양성 bending이 확인되면 C군 대표 맵으로 사용한다. 그렇지 않으면 별도의 확인된 풀 맵을 사용한다.

최소 장면은 영향 없음, 유닛이 실제 풀을 밀고 통과함, 여러 유닛의 중첩·상쇄, 반경/공간 셀 경계를 드나드는 경우다. 동일 상태의 자동 비교와 짧은 사람의 시각 확인을 함께 수행한다. 특정 tick은 재현 증거로 기록할 수 있지만 최적화 runtime 조건으로 사용하지 않는다.

## 실행 순서와 완료 기록

1. **A군 native 빌드/기본 회귀**부터 수행한다. 현재 Mac source가 다른 경우 semantic patch만 이식한다.
2. A군 소스 상태를 고정한 후 B1과 C1은 서로 다른 파일·독립 후보로 준비할 수 있다. GPU 성능 비교는 후보별로 분리한다.
3. C1은 양성 grass 장면과 Simmer 이식성 gate를 닫는다. B1은 pass 경계와 intermediate image parity를 닫는다.
4. B2/B3은 기존 증거를 재사용하고 필요한 미완료 단계만 수행한다. 이미 끝난 광범위 실험을 재실행하지 않는다.
5. 각 항목에 source commit, binary SHA256, flag/config delta, map/version, geometry/quality 보존, 검증 결과, CPU/GPU 비용, clean FPS/1% low/p99, 미해결 사항을 기록한다.

이 지시서는 A/B/C 작업을 분리해 실행하도록 남기는 것이다. **새 Metal 게임 실행이나 자동 작업이 이미 시작됐다고 보고하지 않는다.**
