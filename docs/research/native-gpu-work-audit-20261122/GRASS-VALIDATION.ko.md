---
task: BAR-Metal4 / Group C / NativeGrass
status: NEEDS_MAC_AND_POSITIVE_GRASS_SCENE
source_branch: coord/native-metal4
source_commit: 80110c032156888f930182b6d510c97c83115992
engine_changes: none
raw_capture_verification: unavailable
---

# Group C — Grass 후보 목록 축소 구현·검증 인계

## 결론과 적용 범위

첫 실험은 **기존 grass draw마다 영향을 줄 수 없는 유닛을 보수적으로 제외하고, 셰이더의 유효 `unitBendCount`만 줄이는 것**이다. 인스턴스별 compute prepass는 이 방식의 잔여 비용을 측정한 뒤 판단한다. 맵 이름, 특정 tick, 카메라 위치 또는 이번 샘플의 “영향 0”을 조건으로 사용하는 분기는 금지한다. 지형과 맵 크기에 독립적인 데이터 기반 최적화여야 한다.

사용자가 제공한 관찰은 “32개 draw에 128개 유닛이 바인딩되었고, 기존 grass culling 후 네 tick 모두 영향 범위 안의 인스턴스가 0개”라는 것이다. 원본 캡처와 실행 바이너리는 여기서 확인하지 못했다. **버퍼 용량 128과 실제 반복 횟수 128은 다르다.** 실제 반복 상한은 캡처된 `u.unitBendCount`이며, 셰이더의 앞선 clip/scale 조기 반환을 통과한 vertex invocation만 bending loop를 실행한다. 따라서 현재 자료만으로 절약 연산량이나 FPS 향상을 확정하지 않는다.

## 근거가 되는 소스

아래 링크는 모두 위 commit에 고정되어 있다. 향후 구현 바이너리가 이 소스 계열인지 별도로 기록해야 한다.

| 근거 | 위치와 의미 |
|---|---|
| 실제 shader 경로 | [NativeGrassShaders.h, 44–84](https://github.com/yeojuny/BAR-Metal4/blob/80110c032156888f930182b6d510c97c83115992/engine/rts/Rendering/Metal4/NativeGrassShaders.h#L44-L84): 앞선 조기 반환, vertex별 terrain/wind 계산, instance 중심을 쓰는 bending loop 및 후처리 |
| 입력 캡처 | [LuaNativeGrass.cpp, 64–74](https://github.com/yeojuny/BAR-Metal4/blob/80110c032156888f930182b6d510c97c83115992/engine/rts/Lua/LuaNativeGrass.cpp#L64-L74): CPU에 instance snapshot, 원래 draw 범위, uniform과 bend buffer range가 이미 있음 |
| 범위·count 검증 | [NativeGrassData.cpp, 25–44](https://github.com/yeojuny/BAR-Metal4/blob/80110c032156888f930182b6d510c97c83115992/engine/rts/Rendering/Backend/NativeGrassData.cpp#L25-L44): instance stride 16, 연속 baseInstance 범위, count 0–128, 원래 bend records 유지 |
| 최소 구현 지점 | [Metal4Grass.mm, 77–104](https://github.com/yeojuny/BAR-Metal4/blob/80110c032156888f930182b6d510c97c83115992/engine/rts/Rendering/Metal4/Metal4Grass.mm#L77-L104): local uniform과 buffer 3 binding을 준비한 후 constants 업로드 |
| pipeline·draw 조건 | [Metal4Grass.mm, 35–68](https://github.com/yeojuny/BAR-Metal4/blob/80110c032156888f930182b6d510c97c83115992/engine/rts/Rendering/Metal4/Metal4Grass.mm#L35-L68): baseInstance 유지, dependency/lifetime, safe math·precise functions |

## 맵에 의존하지 않는 shortlist 알고리즘

1. 기존 draw가 참조하는 정확한 `[baseInstance, baseInstance + instanceCount)` 범위의 `instance.xz`로 bounding box를 만든다. 모든 기록된 중심을 포함하면 추가 GPU 조기 반환까지 CPU에서 재현할 필요가 없다. snapshot 세대와 범위를 키로 캐시할 수 있다.
2. **원래 목록의 첫 `unitBendCount`개만 원래 순서대로** 검사한다. draw 안의 어느 중심에도 영향을 줄 수 없다고 증명되는 유닛만 제외한다. 원래 128개 제한 밖에서 다른 유닛을 채워 넣거나 거리순으로 정렬하지 않는다.
3. 전부 유지되면 원래 count/buffer를 재사용한다. 일부만 남으면 16-byte 원본 records를 순서와 비트 그대로 복사한 파생 buffer와 줄인 local count를 사용한다. 아무것도 남지 않으면 원래 buffer를 둔 채 local count를 0으로 설정한다. 원본 snapshot은 수정하지 않는다.
4. 같은 pipeline, draw/index/instance 범위, vertex 연산 순서를 유지한다. 별도 draw, compute encoder, GPU readback은 이 첫 실험에 필요하지 않다. 파생 buffer는 기존 GPU allocation/lifetime 경로를 사용한다.

최초 bounding box 구축 비용은 읽는 instance 수에 비례한다. 이후 유닛-범위 검사는 draw 수 × 실제 count에 비례한다. 실제 count가 32개 draw 모두 128일 때 이 검사는 최대 4,096회지만, 이는 CPU 시간이나 순이익의 측정값이 아니다. draw box가 넓으면 실제 영향이 0이어도 후보가 남는다. 그 경우 먼저 캐시된 cell/block 경계를 검토하고, 무조건 draw를 늘리지는 않는다.

## 정확성 계약과 fallback

원래 포함 조건은 `dot(instance.xz - unit.xy, instance.xz - unit.xy) < unit.z * unit.z`이면서 `distSq > .0001f`이다. **mesh vertex 위치, blade 폭, scale, wind, terrain height는 이 판정에 쓰이지 않는다.** 따라서 중심 bounding box에 blade/wind 크기를 임의로 더할 필요가 없다. 안쪽 제외 영역은 broadphase에서 무시해도 안전하다. 후보를 더 남길 뿐이다.

CPU에서 다른 정밀도로 계산한 거리와 반지름을 단순 비교해 경계 유닛을 제거하면 안 된다. 구현은 셰이더 float 연산을 포함하는 바깥쪽 interval bound를 사용해, 전체 범위의 가능한 거리 제곱 하한이 반지름 제곱 상한 이상임을 보장할 때만 제거해야 한다. 축 하나의 보수적인 분리 판정부터 시작할 수 있다. 임의 epsilon을 충분한 증명으로 취급하지 않는다.

- instance 좌표가 nonfinite이거나 범위를 안전하게 구성하지 못하면 해당 draw는 원래 목록을 사용한다.
- unit 좌표·반지름이 nonfinite이거나 제곱 overflow 등으로 판정을 보장하지 못하면 해당 유닛을 유지한다. 음수 반지름도 최초 구현에서는 유지하는 정책이 단순하다. 반지름 0은 원래 엄격한 바깥 비교를 통과하지 않지만, 불확실한 입력을 교정해서는 안 된다.
- falloff가 0 또는 음수여도 원래 기하학적 포함 조건은 같다. falloff로 나누거나 “강도가 작을 것”이라는 근사로 영향 반지름을 줄이지 않는다. 남은 유닛의 `pow`, strength, clamp는 원래 경로를 그대로 실행한다. `unit.w == 0`만으로 제외하는 별도 규칙도 만들지 않는다.
- 안정적인 부분 목록은 합산 순서를 보존한다. 제외된 유닛은 그 draw의 모든 instance에 대해 원래 if-body를 실행하지 않는다는 것이 조건이다.
- count가 0이어도 **`GRASS_BEND=1` 후처리를 유지한다.** 원래의 `world.y = (world.y - ground) * shrink + ground`를 생략하거나 `GRASS_BEND=0`으로 바꾸면 반올림/예외값 의미가 달라질 수 있다.
- 캐시는 snapshot 세대, 범위와 관련 입력을 식별해야 한다. 주소만으로 재사용하지 않는다. 후보 목록까지 캐시한다면 bend snapshot/range/count 변경도 무효화 조건이다.

## 실제 grass 맵 선택 gate와 Simmer

현재 확인한 자료로 특정 맵을 grass-heavy라고 입증하지 못했으므로 이름을 추측해 추천하지 않는다. 실제 게임에서 다음 gate를 통과한 맵/장면을 검증 대상으로 등록한다.

1. 맵 이름·버전/해시와 grass 설정을 기록하고, native grass draw 및 0보다 큰 instance 수가 실제 발생하는지 확인한다.
2. bending이 활성화되어 있으며, **실제 uniform count**, 원래 유닛 목록, 조기 반환 후 instance의 중심을 사용한 판정에서 영향받는 instance가 0보다 큰지 확인한다.
3. 동일한 replay 또는 재현 가능한 tick/camera/state로 원본과 후보를 비교할 수 있어야 한다. 화면에 풀처럼 보이는 것만으로 해당 renderer 경로가 실행되었다고 판단하지 않는다.

**Simmer는 사용자가 지정한 이식성 확인 대상으로 포함한다.** Simmer에 충분한 grass가 있거나 실제 bending이 발생했다는 증거는 아직 없다. Simmer가 위 gate를 통과하지 못하면 일반 렌더링 회귀 확인에는 사용하되, Group C의 양성 grass 검증을 대신하지 못한다. 맵 전체 지원은 맵 이름 예외를 없애고 입력 범위·좌표·snapshot 계약을 지키는 것으로 설계하며, 제한된 맵 실행만으로 “모든 맵 검증 완료”라고 쓰지 않는다.

## 자동 검증, Mac GPU 검증, 제한적인 시각 확인

| 구분 | 필요한 확인 | 완료 기준 |
|---|---|---|
| 자동화 가능한 소스·오프라인 검사 | count/용량 구분, 원래 첫 count의 안정적 부분 목록, record 비트 보존, baseInstance 범위, cache invalidation, 경계·fallback 사례 | 원래 predicate가 true일 수 있는 항목을 제거하지 않음. 원본 snapshot 불변. 이 검사는 GPU 실행 증명을 대신하지 않음 |
| Mac에서 자동화할 GPU parity | 빈 목록/멀리 있는 유닛뿐 아니라 **유닛 바로 옆 실제 휨**, 여러 유닛의 영향 중첩·방향 상쇄·shrink 포화, radius 경계와 안쪽 `.0001f` 경계, 움직이며 cell 경계를 넘는 유닛 | 동일 상태에서 원본/후보의 비교 가능한 vertex 결과, depth/color 및 count를 대조. 비교 방식·정밀도·허용 기준을 기록하고 차이 원인을 해결 |
| Mac 성능 측정 | 원래 count→후보 count, 빈 shortlist draw 수, CPU Prepare 비용, upload bytes, grass GPU 시간 및 전체 frame time | 카운터 수집 run과 깨끗한 시간 측정 run을 구분. 반복 측정에서 이득과 회귀 범위를 제시하고, 실제 휨 장면에서도 비교 |
| 제한적인 사람의 시각 확인 | 같은 재현 장면에서 휨 방향/복귀, 경계 이동 시 튐, 중첩 유닛 주변, 지형 높이·바람 조합 | 자동 parity로 놓칠 수 있는 눈에 띄는 이상을 짧게 확인. 정확성이나 속도의 유일한 근거로 사용하지 않음 |

사람 확인은 이 변형 기능의 양성 장면에 대한 보조 검사다. 모든 소스 수정에 수동 승인을 요구하는 절차가 아니다. 현재 상태가 열려 있는 이유는 **Mac 실행과 실제 휨 장면의 증거가 없기 때문**이며, 이 문서는 엔진 변경이나 성능 향상 확인을 뜻하지 않는다.

## 이후 compute prepass를 검토할 조건

목록 축소 후에도 충분한 후보가 남고 반복 bending 계산이 측정상 유의미할 때만 검토한다. `totalBend`, `maxShrink`와 그로부터 만드는 계수는 instance당 공유할 수 있지만, terrain/wind 입력과 blade 높이에 대한 적용은 vertex별로 유지해야 한다. prepass는 dispatch, 결과 buffer 쓰기/읽기, compute→vertex dependency와 GPU lifetime 비용을 추가한다. 원래 유닛 합산 순서, nonzero baseInstance, 필요한 instance 범위만 처리하는 조건도 보존해야 한다. 현재 zero-influence 관찰만으로 이 추가 경로를 먼저 도입할 근거는 부족하다.
