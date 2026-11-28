# BAR-Metal4 GPU 작업 감사와 범용 수정 계획

- 작성일: 2026-11-22 (America/Los_Angeles)
- 대상: `yeojuny/BAR-Metal4`
- 소스 감사 기준: `80110c032156888f930182b6d510c97c83115992` (`coord/native-metal4`)
- 문서/수정 브랜치의 기준: `main`의 `a0963d4b2e39a6a2bdf97c9eb5f639e71f97afa6`
- 후속 실행 지시: [IMPLEMENTATION-QUEUE.ko.md](IMPLEMENTATION-QUEUE.ko.md)
- 풀 장면 검증 지시: [GRASS-VALIDATION.ko.md](GRASS-VALIDATION.ko.md)
- 증거 구분: [SOURCE-PROVENANCE.json](SOURCE-PROVENANCE.json)

## 1. 결론과 현재 상태

새 캡처 관측을 반영한 우선순위는 **Grass의 불필요한 유닛 탐색 제거 → Distortion 연속 패스 통합 → GroundFlash의 동일 상수 재사용**이다. 다만 구현·검증 난도는 이 순서와 다르다. 먼저 자동으로 수정할 수 있는 좁은 CPU 준비 경로를 A군으로 분리하고, GPU 출력 검증 또는 실제 풀 상호작용 검증이 필요한 변경을 B/C군으로 둔다.

이 브랜치에는 A군 GroundFlash 준비 비용 감소의 구현과 감사 문서를 함께 보존한다. A군도 Metal 빌드와 실행을 완료한 것으로 간주하지 않는다. 감사 환경은 macOS/Metal 런타임이 아니므로 Metal 컴파일, GPU 픽셀 비교, 새로운 게임 FPS 측정은 수행하지 않았다. B/C군은 구현·검증 지시이며 완료된 성능 개선이 아니다.

기존 GroundDecal flat-wrap, SensorLOS 정수 나눗셈, 큐브맵, Bloom 단서는 유지한다. 과거 개선율을 새 후보의 추가 이득으로 더하지 않는다.

## 2. 범용성 계약: Seths나 특정 틱을 위한 패치가 아니다

사용자의 추가 요구는 **Simmer를 포함한 다른 맵과 일반 플레이에서도 적용 가능한 수정**이다. Seths 캡처는 중복 작업을 발견한 증거이며 런타임 선택 조건이 아니다.

다음 조건을 모든 구현에 적용한다.

1. 맵 이름, 벤치마크 seed, 특정 simulation tick, 정해진 카메라 위치로 최적화를 켜거나 끄지 않는다.
2. 관측된 `32 draws`, `227 draws`, `756×475`, 네 개의 샘플 틱을 코드 조건으로 넣지 않는다.
3. Grass는 매 요청의 실제 instance 범위와 원래 유닛 목록으로 판단한다. 네 틱에서 영향이 0이었다는 결과를 이후 프레임에 고정하지 않는다.
4. Distortion은 실제 attachment, 포맷, sample count, 전역 그리기 순서, 입력 의존성 및 버전 연결을 검사한다. `Clear → Volume…`의 길이는 가변이며 Volume 두 개에 한정하지 않는다.
5. GroundFlash는 현재 준비 호출 안에서 실제 상수 바이트와 바인딩 자원이 같을 때 공유한다. 프레임 번호만으로 서로 다른 카메라나 패스를 같은 것으로 취급하지 않는다.
6. 정확성을 증명할 수 없는 입력·경계·레이아웃은 기존 경로를 사용한다. 지원 포맷/셰이더 ABI와 같은 기술적 조건은 맵별 예외가 아니다.
7. **범용적으로 올바른 것과 모든 장면에서 빨라지는 것은 다르다.** 후보가 거의 줄지 않거나 해당 효과가 적은 장면에서는 이득이 작거나 없을 수 있다. 정확성과 준비 비용을 함께 확인한다.

새로운 맵별 예외, 틱별 분기, 그래픽 효과 생략, 풀 개체 수 감소, 해상도 축소, 업데이트 빈도 감소는 이 작업 범위에 포함하지 않는다.

## 3. 증거의 범위

### 3.1 사용자가 전달한 2026-11-22 캡처 관측

원자료 식별자: `seths-cap2-battle-gpu-passes-20261122-oanitk44`.

| 관측 | 전달받은 내용 | 아직 직접 확인하지 못한 항목 |
|---|---|---|
| Grass | 32개 draw 모두 유닛 128개 바인딩. 남는 풀 중 유닛 영향권 안에 든 것은 네 틱 모두 0개 | 실제 `unitBendCount`, 각 instance 범위, 원본 버퍼, 캡처 바이너리/소스 해시 |
| Distortion | 같은 756×475 텍스처에 초기화 → 왜곡 → 왜곡. 3 passes, 6 barriers, 구간 0.180–0.222 ms | 전체 command 순서, 정확한 attachment/subresource, 원본 타임스탬프 |
| GroundFlash | tick 1665의 연속 227 draws에서 다른 상수 버퍼의 272바이트 전체가 동일 | 원본 buffer dump 및 캡처 바이너리와 소스의 일치 |

의뢰서 및 원자료 이름은 `NATIVE-GPU-AUDIT-BRIEF.ko.md`, `DISTORTION-ATTACHMENTS.json`, `PARTICLE-UNIFORM-CHECK.json`이다. 전달된 Mac 로컬 파일은 감사 환경에서 접근할 수 없었으며, 확인한 공개 GitHub 브랜치에서도 찾지 못했다. 이 파일들을 직접 읽거나 업로드했다고 주장하지 않는다.

### 3.2 직접 확인한 것

고정된 GitHub 소스에서 입력 의존성, 반복 준비 경로, pass 생성, 논리적 버전 전이 및 기존 성능 실험을 대조했다. 아래 링크는 가능하면 커밋에 고정한다. CPU 산술 검증은 [arithmetic/README.txt](arithmetic/README.txt)에 별도로 보존하며 Metal/GPU 시험과 구분한다.

## 4. Grass: 동일 instance의 정점들이 같은 유닛 목록을 반복한다

### 4.1 공유 가능한 계산과 정점별 계산

[NativeGrassShaders.h의 bending 경로][grass-shader]는 다음 값을 사용한다.

```cpp
float2 diff = instance.xz - unit.xy;
float distSq = dot(diff, diff);
if (distSq < unit.z * unit.z && distSq > .0001f) {
    float dist = sqrt(distSq);
    float strength = pow(1.f - dist / unit.z, u.defines[15]) * unit.w;
    totalBend += (diff / dist) * strength;
    maxShrink = max(maxShrink, strength);
}
```

판정에 사용되는 위치는 정점의 변환된 위치가 아니라 `instance.xz`이다. 같은 중심을 공유하는 풀 instance/patch의 정점들이 동일한 값을 계산한다.

| 계산 | instance 정점 사이 공유 가능 여부 |
|---|---|
| 유닛과의 거리 및 반경 판정 | 가능 |
| `totalBend`, `maxShrink` | 원래 유닛 순서를 지키면 가능 |
| shrink, 방향, bend angle, sin/cos | instance 결과와 설정으로 결정되므로 가능 |
| heightmap 및 wind/noise 평가 | 정점별 입력을 사용하므로 기존 위치에 유지 |
| 정점 높이에 따른 실제 displacement | 정점별 유지 |
| 최종 위치, fade, facing | 변형된 정점별 유지 |

### 4.2 바인딩 용량과 실제 반복 횟수를 혼동하지 않는다

셰이더 반복 상한은 `u.unitBendCount`이다. [입력 검증][grass-data]은 이 값을 0~128로 검사하고, 바인딩 범위가 그 수의 레코드를 포함하는지 별도로 확인한다. 버퍼 용량이 128이라고 루프가 반드시 128회인 것은 아니다.

또한 vertex shader에는 bending 이전의 clip/scale 조기 제외가 존재한다. 대략적인 논리적 반복량은 **bending까지 도달한 실제 vertex invocation × 실제 count**이다. 모든 제출 index와 instance를 곧바로 곱하거나, shader read 횟수를 그대로 DRAM 트래픽으로 해석하지 않는다. 영향 조건을 통과하지 않는 경우의 `sqrt`/`pow` 비용도 단정하지 않는다.

### 4.3 첫 구현: 기존 draw별 보수적인 유닛 후보 목록

첫 변경 지점은 [GrassGPU::Prepare][grass-prepare]의 local uniform과 bend buffer binding이다. [Lua 캡처][grass-capture]가 이미 정확한 instance 및 bend 데이터의 불변 CPU 스냅샷을 제공하므로 GPU readback이 필요하지 않다.

1. 원래 draw의 `[baseInstance, baseInstance + instanceCount)` 중심 좌표 전체를 포함하는 xz 범위를 만든다. GPU camera culling을 CPU에서 새로 재현할 필요는 없다.
2. 정확한 snapshot identity/generation과 범위가 같을 때만 이 범위를 재사용한다.
3. 원래 첫 `unitBendCount`개 레코드만 검사한다. 어느 instance 중심에도 영향을 줄 수 없다고 증명되는 레코드만 제외한다.
4. 남긴 레코드의 비트와 순서를 유지한다. 기존 목록 밖에서 더 가까운 유닛을 채워 넣지 않는다.
5. 빈 목록이면 local count만 0으로 바꾸고 기존 buffer를 유지할 수 있다. 일부만 남으면 기존 lifetime 경로로 작은 파생 buffer를 만든다. 아무것도 줄지 않으면 원래 count/buffer를 사용한다.

기하학적 반경 판정은 shader float 연산에 보수적이어야 한다. 임의 epsilon이나 경계의 단순 double 비교만으로 정확성을 주장하지 않는다. 바깥 방향으로 반올림한 범위, 경계 fallback, 비유한 값과 overflow fallback을 명시한다. 음수 반경을 양수로 덮어쓰거나 `unit.w == 0`만으로 레코드를 제거하지 않는다.

느슨한 draw 경계는 빈 공간까지 포함하므로, 실제 영향받는 instance가 0이어도 후보 목록은 비어 있지 않을 수 있다. 이 경우 분할된 공간 셀 등으로 범위를 개선할지 판단한다. 처음부터 draw를 늘리거나 compute pass를 추가하지 않는다.

### 4.4 영향 0에서도 기존 산술 경로를 보존한다

`GRASS_BEND=0`으로 블록 전체를 제거하지 않고, 기존 활성 shader에서 유효 count를 0으로 만든다. 기존 활성 경로는 영향 0에서도 다음 산술을 수행하기 때문이다.

```cpp
world.y = (world.y - ground) * shrink + ground;
```

수학적인 항등식과 부동소수점 연산의 동일성은 구분한다. 첫 변경에서는 이 경로를 보존한다. 원래 순서를 유지한 후보 축소는 원래도 진입하지 않았을 조건문만 생략하는 것이어야 한다.

### 4.5 후속 구현: instance당 계산

후보 목록을 줄여도 실제 영향 유닛이 많으면 instance당 한 번 `totalBend`/`maxShrink`를 계산하는 방법을 검토한다. compute dispatch, 결과 buffer write/read, compute→vertex 의존성과 `baseInstance` 대응이 추가되므로 후보 축소와 별도 변경으로 다룬다. 원래 순서의 합산을 병렬 reduction으로 바꾸는 것은 별도의 부동소수점 변화다.

정확성 확인은 영향 0뿐 아니라 실제 bending, 반경 진입/이탈, 여러 유닛의 중첩·상쇄, shrink 포화, moving unit 및 snapshot 교체를 포함한다. 구체적인 실행 조건은 [GRASS-VALIDATION.ko.md](GRASS-VALIDATION.ko.md)에 있다.

## 5. Distortion: 같은 pass 안에서 원래 draw를 유지한다

### 5.1 소스가 뒷받침하는 통합 범위

[EncodeDraw][distortion-encode]는 non-noop operation마다 `Load/Store` pass를 만들고 앞뒤 queue barrier를 추가한다. [Volume 입력][distortion-inputs]은 map depth, model depth, 3D noise이며 누적 중인 distortion texture를 샘플하지 않는다. 따라서 전역 순서에서 연속한 `Clear → Volume…`을 하나의 encoder에 넣는 제한된 통합이 가능하다.

| 항목 | 전달된 관측 | 후보 |
|---|---:|---:|
| 초기화 및 volume draw | 3 | 3 유지 |
| render encoder | 3 | 1 |
| 이 구간의 queue barrier | 6 | 필요한 외부 의존성을 유지한 2개 검토 |
| 중간 store/load 경계 | 2곳 | 제거 검토 |
| 최종 Combine 샘플링 | 별도 | 별도 유지 |

`0.180–0.222 ms`에는 volume shader와 blending도 포함된다. 이 시간 전체를 제거 가능한 오버헤드나 예상 절감량으로 쓰지 않는다. 줄이는 것은 불필요한 pass 경계와 그에 수반되는 처리다.

### 5.2 범용 admission 조건

- 같은 command buffer/submission 및 준비된 frame의 전역 순서에서 바로 이어지는 작업만 허용한다.
- 실제 같은 논리적 image owner와 물리 texture/subresource를 확인한다. 해상도가 같다는 사실만으로 충분하지 않다.
- 첫 구현은 현재 구현이 다루는 RGBA16F, single sample, mip/slice 0, resolve 및 depth/stencil 없는 누적 target에 한정하고 다른 layout은 기존 경로로 보낸다. 해상도는 고정하지 않는다.
- 입력 texture가 출력과 alias하지 않는지, 원래 dependency/readability 조건을 확인한다.
- 각 draw의 pipeline, blend/write mask, scissor/viewport, argument table, raster 및 draw 범위를 원래 순서대로 다시 적용한다. 서로 다른 blend state도 같은 pass 안에서 순차 적용할 수 있다.
- 시작 `Load`와 마지막 `Store`를 유지한다. 중간에 CopyScreen, Combine, 다른 family, target 변경 또는 지원하지 않는 작업이 있으면 run을 종료한다.

### 5.3 초기화와 논리적 버전의 함정

현재 Clear는 [사각형 shader draw][distortion-clear]이며 [마스크와 scissor][distortion-state] 의미를 가진다. 무조건 `loadAction=Clear`로 치환하지 않는다.

Distortion 목록만 보지 않고 [전역 laterWorldDraws 순서][global-order]에서 adjacency를 판단한다. 서로 떨어진 작업을 끌어모으지 않는다.

각 operation은 존재하는 previous/inputs 버전을 확인하고 해당 output의 새 버전을 기록한다. 모든 operation에 previous가 있는 것은 아니다. 물리 texture가 같아도 이 전이는 유지해야 한다. 합친 encoder 안에서도 **버전 확인 → 원래 draw 또는 noop 처리 → 해당 버전 갱신**을 순차 실행한다. 미래 버전을 시작 시점에 모두 검사하거나 마지막 버전만 기록하지 않는다. completion/lifetime은 기존 frame retirement 경로를 유지한다.

[Combine][distortion-combine]은 실제 distortion texture의 소비자이므로 누적 pass 밖에 둔다. CopyScreen의 main resolve 및 별도 target도 보존한다.

### 5.4 Metal 근거

Apple의 [render pass 통합 안내][apple-merge]는 target, sampling 의존성, load/store 및 순서를 평가하도록 설명한다. [Apple GPU 문서][apple-tbdr]는 일반 blending의 draw 순서 보장을 설명한다. 이 후보는 그 순서를 이용하며, texture feedback을 새로 만들지 않는다.

Apple Silicon에서는 fragment/tile 완료를 기다리는 intrapass barrier가 지원되지 않는다. [Metal 동기화 문서][apple-intrapass]를 따른다. 제거한 pass 사이를 Fragment→Fragment intrapass barrier로 억지로 대체하는 구현은 이 후보가 아니다. 같은 command buffer의 연속 구간이므로 suspend/resume를 먼저 도입할 이유도 없다.

검증은 Combine 직전 RGBA16F 이미지와 최종 색상/관련 depth·stencil 결과를 포함한다. 부분 clear, write mask, scissor, overlapping volume, blend 변화, noop 및 중간 소비자에서의 fallback을 확인한다.

## 6. GroundFlash: A군 CPU 준비 중복 제거

[WorldParticles 준비 loop][particles-prepare]는 range마다 행렬, 공통 camera/fog 값, 272바이트 UBO, argument table 및 depth state를 만든다. [GroundFlash 캡처][groundflash-capture]는 stage, alpha, soften 값을 일정하게 설정하지만 개별 depth test/write는 달라질 수 있다. depth 값은 이 UBO 밖에 있다.

### 6.1 변경 원칙

- 같은 `Prepare` 호출 안에서만 불변 UBO와 argument binding을 공유한다.
- 상수의 전체 바이트, 실제 vertex binding, atlas 및 terrain depth 자원이 일치해야 재사용한다. 이름이나 frame 번호만으로 공유하지 않는다.
- 비교가 달라지면 원래 값으로 새 객체를 만든다. 이미 다른 draw가 참조한 객체를 덮어쓰지 않는다.
- draw 수, 순서, index 범위, geometry, blending, depth 동작, atlas/pending mipmap 처리 및 residency/lifetime을 유지한다.
- 같은 호출 안에서 depthTest/depthWrite의 정확한 조합별 depth state를 재사용한다.
- 공통 행렬 구성 및 입력 finite 검사는 동등한 준비 범위에서 한 번 수행한다.

전달된 227개 draw가 같은 Prepare 안에 있고 UBO 전체 바이트뿐 아니라 실제 바인딩 자원도 모두 일치하는 경우 UBO 객체는 227→1, payload 복사는 61,744→272바이트가 된다. 캡처의 UBO 바이트 일치만으로 이 cache hit 수까지 입증된 것은 아니다. 61,472바이트(약 60 KiB)는 payload 감소이며 실제 Metal allocation footprint와 다르다. GPU draw 227개와 fragment 작업은 그대로다. 새 FPS 개선을 측정한 값은 없다.

구현은 **가장 최근 GroundFlash binding 한 개**만 기억하는 작은 per-Prepare cache이다. 입력이 A→B→A로 바뀌면 마지막 A에서 다시 생성하며, 임의의 모든 과거 signature를 중복 제거하는 전역 cache가 아니다. 공통 행렬도 GroundFlash에서만 최초 한 번 구성한다. non-GroundFlash의 원래 준비 경로는 유지한다.

### 6.2 이전 병합 실험과 구분

[R46][groundflash-r46]은 연속 range를 합쳐 draw 수를 줄였고 평균 FPS는 65.0353689→64.9815008, -0.0828289%였다. 이번 UBO 공유는 depth state가 다른 draw 사이에서도 가능하므로 동일한 실험은 아니다. 그러나 이전 병합도 준비 객체 수를 줄였을 수 있으므로 미발견 대형 병목이라고 주장하지 않는다.

A군은 의미를 보존하는 CPU 준비 최적화로 먼저 구현하되, 정상 Metal 빌드 및 기본 출력 회귀 검증 전에는 적용 완료/성능 검증 완료로 표시하지 않는다.

## 7. 같은 패턴을 다른 경로에 확장할 때

| 경로 | 소스에서 확인한 상태 | 다음 판단 |
|---|---|---|
| Grass uniform | run마다 상수 생성 | wind/defines/LOD/alpha 및 최적화 후 count까지 실제 같아야 공유 가능 |
| Lua Nano normal/ghost | UBO는 draw별 생성, ghost 구분은 별도 depth/stencil state | captured uniform/shape 등도 같으면 공유 후보. 두 draw와 stencil 동작 유지 |
| DeferredLights | 공통 matrices/params를 이미 owner 기반 캐시로 공유 | draw별 firstInstance/radius/shape 등까지 공통이라고 가정하지 않음 |
| Bloom | 최종 Combine에 scene pass 재사용 경로와 admission 이유가 이미 존재 | 현재 캡처의 활성화/거절 이유를 확인. downsample/upsample의 실제 sampling 의존성은 유지 |
| CubeMap | 완전히 덮어쓸 face 및 재생성 mip 복사를 생략하는 기존 경로 | 남는 복사의 이전 버전 보존 필요성과 기존 guard의 실제 활성화를 확인 |

근거: [Grass][grass-prepare], [Lua Nano][lua-nano], [DeferredLights][deferred-common], [Bloom][bloom-admission], [CubeMap][cube-copy]. 이 후보들은 GroundFlash와 의미 조건이 다르므로 일괄적인 전역 캐시 패치로 확대하지 않는다.

## 8. 앞선 감사 결과의 연결

| 후보/판단 | 기존 근거 | 현재 처리 |
|---|---|---|
| SensorLOS exact-interior | [R76][sensor-r76] +17.47%, [R77][sensor-r77] +16.97%. cap2/AA0 및 화면 잠금·절전 조건 | 이미 얻은 개선. 현재 바이너리에 적용돼 있으면 다시 추가 이득으로 계산하지 않음 |
| SensorLOS 남은 정수 나눗셈 | [R75 patch][sensor-patch]의 나눗셈에 [WeaponRange 전략][range-division] 적용 후보. [호스트 검증](arithmetic/README.txt) 1,555,102건 | B군. CPU 산술 검증은 Metal compile/parity/FPS 검증을 대체하지 않음 |
| GroundDecal flat-wrap | [독립 GPU 시험][flat-wrap] 중앙값 -5.489%, 4쌍 모두 동일 방향. 게임 실행/엔진 빌드 0 | 정확한 패치 확보 및 현재 엔진 통합이 선행. 전체 게임 FPS +5.49%가 아님 |
| cap1/cap2 | [R91][cap-r91] 활성 화면 비교에서 cap1이 평균/1% low/p99에 유리 | 새 후보를 큐 깊이 변경과 묶지 않음. 현재 플레이 기본 설정을 고정해 판단 |
| Native vs MVK3 | [입력 감사][mvk-audit]에 측정 구간/데칼 입력 등 동등성 미해결 | 과거 두 FPS의 비율을 현재 성능 목표나 보장치로 사용하지 않음 |

이전 CPU 산술 검증의 1,555,102건에서 4,197건은 float 추정 몫의 정수 보정이 필요했다. 결과가 모두 일치한 것은 몫/나머지와 lower-edge 공식의 호스트 검증이며, 전체 shader parity가 아니다. 보정 횟수 최대 1회는 해당 표본의 관측이므로 GPU 구현에서 보정 loop/fallback을 제거하는 근거로 사용하지 않는다.

## 9. 채택 기준과 사람의 역할

기본 정책은 **증거가 충분한 독립 변경을 자동 진행하고, 의미가 달라지거나 실제 장면 증거가 필요한 변경에만 추가 gate를 둔다**는 것이다. 사람에게 매번 승인을 요청하는 절차를 만들지 않는다.

- A군: 코드 리뷰와 정상 빌드/기본 회귀를 통과하면 후속 통합으로 진행한다. 현재 환경의 빌드 불가를 성공으로 대체하지 않는다.
- B군: 자동 GPU parity와 의존성 검증이 먼저다. 불일치가 나오면 원인을 수정한다. 단순히 눈에 비슷하다는 이유로 실패를 통과시키지 않는다.
- C군: 실제 풀이 존재하고 유닛이 영향을 주는 장면을 확보한다. 자동 수치/이미지 검증과 짧은 인간 시각 확인을 함께 사용한다. Simmer에서도 일반 경로가 동작함을 확인하되 grass 존재/부하를 확인하지 않고 풀 검증 완료라고 하지 않는다.
- 사람의 추가 판단: 의도적인 품질/정확성 타협이 필요한 제안, 자동 결과로 판정하지 못한 시각적 이상, 원래 renderer의 동작 자체가 모호한 경우에만 요청한다. 이번 범위에서는 품질 저하를 승인받았다고 가정하지 않는다.
- 성능: 계측 실행과 clean 실행을 구분하고, 평균 FPS뿐 아니라 1% low/p99 및 CPU 준비 비용을 확인한다. 느린 유효 회차를 근거 없이 제외하지 않는다.

### 소스 및 공식 문서

[grass-shader]: https://github.com/yeojuny/BAR-Metal4/blob/80110c032156888f930182b6d510c97c83115992/engine/rts/Rendering/Metal4/NativeGrassShaders.h#L44-L84
[grass-data]: https://github.com/yeojuny/BAR-Metal4/blob/80110c032156888f930182b6d510c97c83115992/engine/rts/Rendering/Backend/NativeGrassData.cpp#L21-L44
[grass-capture]: https://github.com/yeojuny/BAR-Metal4/blob/80110c032156888f930182b6d510c97c83115992/engine/rts/Lua/LuaNativeGrass.cpp#L64-L74
[grass-prepare]: https://github.com/yeojuny/BAR-Metal4/blob/80110c032156888f930182b6d510c97c83115992/engine/rts/Rendering/Metal4/Metal4Grass.mm#L77-L104
[distortion-encode]: https://github.com/yeojuny/BAR-Metal4/blob/80110c032156888f930182b6d510c97c83115992/engine/rts/Rendering/Metal4/Metal4Distortion.mm#L164-L226
[distortion-inputs]: https://github.com/yeojuny/BAR-Metal4/blob/80110c032156888f930182b6d510c97c83115992/engine/rts/Rendering/Metal4/Metal4Distortion.mm#L390-L415
[distortion-clear]: https://github.com/yeojuny/BAR-Metal4/blob/80110c032156888f930182b6d510c97c83115992/engine/rts/Rendering/Metal4/NativeDistortionShaders.h#L1829-L1838
[distortion-state]: https://github.com/yeojuny/BAR-Metal4/blob/80110c032156888f930182b6d510c97c83115992/engine/rts/Rendering/Metal4/Metal4Distortion.mm#L345-L374
[distortion-combine]: https://github.com/yeojuny/BAR-Metal4/blob/80110c032156888f930182b6d510c97c83115992/engine/rts/Rendering/Metal4/NativeDistortionShaders.h#L1825-L1827
[global-order]: https://github.com/yeojuny/BAR-Metal4/blob/80110c032156888f930182b6d510c97c83115992/engine/rts/Rendering/Metal4/Metal4WorldRenderer.mm#L3668-L3684
[particles-prepare]: https://github.com/yeojuny/BAR-Metal4/blob/80110c032156888f930182b6d510c97c83115992/engine/rts/Rendering/Metal4/Metal4WorldParticles.mm#L430-L442
[groundflash-capture]: https://github.com/yeojuny/BAR-Metal4/blob/80110c032156888f930182b6d510c97c83115992/engine/rts/Rendering/Backend/NativeWorldParticleCapture.cpp#L109-L119
[groundflash-r46]: https://github.com/yeojuny/BAR-Metal4/blob/80110c032156888f930182b6d510c97c83115992/coordination/native-metal4/rounds/groundflash-depth-run-fps-20261118-46/DONE.json
[lua-nano]: https://github.com/yeojuny/BAR-Metal4/blob/80110c032156888f930182b6d510c97c83115992/engine/rts/Rendering/Metal4/Metal4WorldParticles.mm#L518-L529
[deferred-common]: https://github.com/yeojuny/BAR-Metal4/blob/80110c032156888f930182b6d510c97c83115992/engine/rts/Rendering/Metal4/Metal4DeferredLights.mm#L364-L374
[bloom-admission]: https://github.com/yeojuny/BAR-Metal4/blob/80110c032156888f930182b6d510c97c83115992/engine/rts/Rendering/Metal4/Metal4Bloom.mm#L131-L213
[cube-copy]: https://github.com/yeojuny/BAR-Metal4/blob/80110c032156888f930182b6d510c97c83115992/engine/rts/Rendering/Metal4/Metal4CubeTextureSource.mm#L145-L164
[sensor-r76]: https://github.com/yeojuny/BAR-Metal4/blob/80110c032156888f930182b6d510c97c83115992/coordination/native-metal4/rounds/sensorlos-exact-interior-fps-20261118-76/DONE.json
[sensor-r77]: https://github.com/yeojuny/BAR-Metal4/blob/80110c032156888f930182b6d510c97c83115992/coordination/native-metal4/rounds/sensorlos-exact-interior-reverse-20261118-77/DONE.json
[sensor-patch]: https://github.com/yeojuny/BAR-Metal4/blob/80110c032156888f930182b6d510c97c83115992/coordination/native-metal4/rounds/sensorlos-exact-interior-coverage-20261118-75/SENSORLOS-EXACT-INTERIOR.patch
[range-division]: https://github.com/yeojuny/BAR-Metal4/blob/0780bd7c530b0696067b717428b49a416eda556a/engine/rts/Rendering/Metal4/NativeWeaponRangeShaders.h#L481-L498
[flat-wrap]: https://github.com/yeojuny/BAR-Metal4/blob/80110c032156888f930182b6d510c97c83115992/coordination/native-metal4/rounds/ground-decal-flat-wrap-shadow-witness-20261118/RESULT.json
[cap-r91]: https://github.com/yeojuny/BAR-Metal4/blob/80110c032156888f930182b6d510c97c83115992/coordination/native-metal4/rounds/seths-rank-atlas-on-cap1-20261118-91/RESULT.json
[mvk-audit]: https://github.com/yeojuny/BAR-Metal4/blob/80110c032156888f930182b6d510c97c83115992/coordination/native-metal4/rounds/mvk-parity-deferred-20261118/MVK3-INPUT-AUDIT.json
[apple-merge]: https://developer.apple.com/library/archive/documentation/3DDrawing/Conceptual/MTLBestPracticesGuide/RenderCommandEncoders.html
[apple-tbdr]: https://developer.apple.com/documentation/metal/tailor-your-apps-for-apple-gpus-and-tile-based-deferred-rendering
[apple-intrapass]: https://developer.apple.com/documentation/metal/synchronizing-stages-within-a-pass
