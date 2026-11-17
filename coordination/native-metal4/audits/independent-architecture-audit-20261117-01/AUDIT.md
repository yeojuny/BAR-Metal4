# BAR-Metal4 독립 구조 감사

작성일: 2026-11-17 UTC  
감사 채팅: 01a1e6ed-8d9a-7981-8332-3ce78c9a0af0  
요청 설정: gpt-6-astra / ultra, 단일 감사. 추가 에이전트·채팅 없음.  
대상: 실제 측정된 R02와 저장된 MVK3 증거. R03은 정확성 검사까지 완료된 별도 후보.

## 1. 핵심 판단 — 다섯 가지

1. **현재 남은 큰 문제는 단순한 API 번역 비용으로 설명되지 않는다.** SensorLOS는 하드웨어 선 대신 삼각형 영역의 fragment마다 원본 선 규칙을 재구성한다. GroundDecal은 원본 GL의 변경 범위 갱신을 전체 immutable revision 복사·버퍼 생성으로 바꾼다. 프레임 제출은 다음 프레임을 준비해도 이전 제출의 event·feedback·생산자 정산을 먼저 완료한다. 서로 다른 세 원인이므로 하나의 GLSL 변환기나 worker 증설로 함께 해결되지 않는다. [Sensor 구현][S1], [데칼 캡처][S2], [데칼 버퍼][S3], [제출 경계][S4]

2. **SensorLOS의 반복 계산은 가장 명확한 GPU 작업 감소 후보다. 그러나 저장된 MVK3를 그대로 정답으로 삼을 수 없다.** 현재 R02 replay에서 해당 fragment의 상대 비용은 26.3422%로 가장 크다. MVK3 캡처는 Metal Line, 폭 5를 사용하고 fragment가 훨씬 단순하다. 동시에 그 캡처의 VS/FS에는 native가 보존하는 거리별 thinning/fade가 없다. 따라서 “MVK처럼 Line으로 바꾸면 동일 화면을 싸게 그린다”는 결론은 성립하지 않는다. 정확한 계약을 유지하면서 segment/major-column 단위로 반복 작업을 줄이는 것이 독립적인 큰 후보다. [현재 비용][R3], [MVK API][M2], [MVK VS][M3], [MVK FS][M4], [원본 프로그램 계약][S5]

3. **32,364개의 GroundDecal 인스턴스를 대부분 낭비라고 부를 근거는 없다.** 원본 GL도 실제 decals.size() 전체를 그리며 저장된 MVK3 캡처는 32,545개다. native VS에는 이미 alpha/type 기반 조기 거부가 있다. 더 확실한 차이는 일부 변경에도 전체 CPU snapshot과 전체 GPU 버퍼 초기화가 발생하는 publication 방식이다. 현재 캡처에서 6,213,888바이트 인스턴스 버퍼 생성이 확인된다. 이것은 한 프레임의 관측이며 초당 대역폭이나 ROI 전체 할당량은 아니다. [원본 GL draw][S6], [현재 API][R1], [데칼 VS][S7]

4. **제출 전 이전 GPU 작업을 기다리는 것은 현재 계약에서 필수지만, 그 위치와 단일 pending 제한까지 물리 법칙은 아니다.** 저장소를 다시 쓰거나 allocator를 reset하기 전 GPU 소비가 끝나야 한다는 조건과, 모든 다음 제출을 이전 프레임의 CPU 정산 뒤로 미루는 정책을 구분해야 한다. 현재 소스에는 추적되지 않은 pending borrow, postprocess 버전, source/UI/post 정산, mutable pool 위험에 대한 구체적인 방어가 있다. 이를 증명 없이 제거하면 수명 오류가 된다. 반대로 현재 이 대기가 실시간 임계 경로를 얼마나 차지하는지는 UNKNOWN이다. [Runtime][S4], [프레임 자원][S8], [postprocess][S9], [main 정산][S10]

5. **R03은 한 번의 한정된 성능 비교까지 계속하되, 구조 개선의 대체물로 삼지 않는다.** R03은 기존 계산 순서를 바꾸어 minor 범위를 먼저 거부한다. 12,065 old/new 비교 통과는 성능 효과나 전체 게임 동등성을 증명하지 않는다. 첫 실험은 고정 R03 대 고정 R02의 동일 Ultra AA0 비교로 한정한다. 새 큰 후보는 아래 세 개이며, 그중 다중 GPU 제출은 현재 runtime 임계 경로 관측과 자원 계약 정리가 선행되어야 한다. 이 감사는 실행·소스 변경·상태 전환을 승인하거나 수행하지 않았다. [R03 결과][R7], [R03 헤더][S11]

## 2. 감사 범위와 증거의 강도

이 문서에서 **관측**은 저장된 raw API/셰이더/측정 기록에서 직접 확인한 사실, **소스 사실**은 고정 소스의 분기·자료구조·소유 관계, **추론**은 그 둘을 연결한 설명, **UNKNOWN**은 필요한 자료가 없는 부분이다. 기존 보고서의 결론을 그대로 권위로 사용하지 않고 주요 수치와 호출·소스 위치를 대조했다.

로컬 읽기 배치마다 GitHub coord/native-metal4의 coordination/native-metal4/STATE.json을 먼저 확인했다. 감사 후반의 권위 상태는 revision 31, READY / READY_R03_ISOLATED_BUILD_STAGING_NOT_STARTED였다. HQ가 R03 실행을 시작하지 않고 감사를 기다린다는 별도 통지가 있었다. 권한·owner·enabled·STATE는 변경하지 않았다. 측정 또는 실행의 RUNNING 상태는 관측하지 않았다.

수행한 것은 텍스트·JSON·저장된 셰이더·raw export 읽기, 제한된 파일 해시 계산, 이 새 문서 작성이다. 엔진/셰이더/설정/runner 수정, build/helper/game/benchmark/capture/replay, live gpudebug, Git 쓰기, 배포 및 기존 증거 변경은 0회다. 이전 전체 10,269파일 검증 기록은 읽었으며, 이 감사가 전체 트리를 다시 해시 검증했다는 주장은 하지 않는다.

### 2.1 현재 R02의 신원

| 항목 | 고정 값 / 범위 |
| --- | --- |
| source commit | 0382d0f3833d63b3ff1a78c46b108c99539656d0 |
| signed engine SHA-256 | 21b2e9df8f331b9c4f8bb04f5abaad44978e39e97d9171c5380a6df38658e5da |
| source closure | 기록상 예상/관측 10,269파일, build 전후 PASS |
| source manifest SHA-256 | 3c6659b05c0353cd6a506f481d58a38a390afbd8773aca25aa7ebc2d6a36dca3 |
| dependency closure | 기록상 19 dylib 고정 |
| R02 clean evidence commit | 87d780578fc39f7c0f5608f6b0d94d6d08c3f557 |
| R02 capture evidence commit | aa558a5c5b1858db54fa31470909d1aa25411ff6 |
| capture tree SHA-256 | 5ae339d460ddb46089783227ba115d37ae4a0e3c4327d81e99c665741a8ea228 |
| capture/replay | 1 game, 1 captured frame, default/overlapping replay 1회; 이 감사에서는 재실행하지 않음 |

근거: [CANDIDATE][I1], [build 후 closure][I2], [clean summary][R4], [capture summary][R5].

### 2.2 실제 성능 비교가 말하는 범위

| 비교 | 관측값 | 허용되는 결론 |
| --- | --- | --- |
| 현재 G01 대 R02, ABBAAB 각 3회 | 62.245820 대 62.228113 FPS, −0.028448% | 이 교전에서 R02의 의미 있는 평균 FPS 변화는 입증되지 않음. correctness 수정의 가치는 별도 |
| 역사적 MVK3 대 b25 native, 같은 Ultra AA0 유형, ABBA 각 2회 | MVK3 93.866822, 당시 native 47.622023 FPS | 당시 고정 비교의 차이. 현재 R02와 동시 측정한 결과가 아님 |
| R03 | helper old/new 비교 12,065 통과 | 해당 검사 범위의 정확성 근거. 엔진 성능 결과 없음 |

FPS는 **CPU에서 관측한 DrawScreen 콜백 수 / 측정 wall time**이다. GPU 실행시간·실제 표시율·Metal draw-call 수가 아니다. 현재 clean 비교는 Seths Ravine Remake 1.3.1, BAR test-31691-a617033-local26, 공식 armbull 교전, game seed 154658555 / benchmark seed 7654321, 생성 600 / 파괴 433, simulation 540–1740의 약 40초다. 실제 warmup은 약 22초이며 loading 약 17초는 별도로 제외된다. 여섯 실행의 40개 world/camera/speed/pause 구간이 일치한다. [현재 clean][R4], [WORLD-CHECK][R6]

양쪽 현재 조건은 Ultra index 4, AA0 / 실제 main sample1, viewport 1512×950, drawable 1512×982, origin (0,32), VSync/VSyncGame0, AC normal, PREPARE1 BEGIN0 worker1, S01/G01 ON, D/Q/U/storage/sharedCUS/Sepia OFF다. viewport와 drawable의 차이는 이 고정 실험의 사실이며 일반적인 notch 정책으로 확대하지 않는다.

역사적 MVK3의 93.866822 FPS는 비교 가능한 AA0 자료를 찾았다는 의미가 있다. 다른 Ultra AA4 수치나 Direct50/다른 맵의 200 FPS대 수치를 섞어 “현재 격차”를 계산하면 안 된다. 현재 R02의 62.228113과 역사적 MVK3 평균을 나눈 값을 새로운 측정 결과로 표시하지 않는다. 원래 MVK3 clean의 메뉴/Dock 노출 관련 교란 기록, 개별 Ultra 옵션 및 셰이더 variant 동등성의 불완전함도 남는다. MVK3 캡처의 foreground 정리와 clean 비교의 교란은 별개다. [역사적 AA0 비교][M1]

현재 R02 validation 1회 및 기존 7개 PNG 검토는 명백한 새 누락·붕괴가 없었다는 제한된 근거다. 이 감사는 PNG를 새로 전수 검토하지 않았고, 동적 효과까지 포함한 전체 프레임 pixel parity로 승격하지 않는다. 캡처의 정확한 simulation frame은 UNKNOWN이며 요청 marker1590과 요청 지연 약 0.029414초만 남아 있다. 캡처 실행의 FPS parser 실패는 보존되어 있고 diagnostic FPS는 제외되었다. [R4][R4], [R5][R5]

### 2.3 현재 raw export를 읽을 때의 함정

현재 JSONL에는 제한된 첫 결과와 전체 결과가 함께 있다. 본 감사는 children 수가 큰 전체 export를 사용했다. 첫 20행만 읽으면 Grass 또는 SensorLOS가 없다고 오판할 수 있다. 또한 performance node ID와 command-tree node ID는 같은 번호 체계가 아니다.

| 대상 | 현재 raw 식별 |
| --- | --- |
| GroundDecal | pipeline rps71; api5530; performance re8 / command re13; label MTL4RenderCommandEncoder9 |
| Grass | command re14; label MTL4RenderCommandEncoder10; indexed draw 8개 |
| SensorLOS | pipeline rps61; api9766; performance re32 / command re41; label MTL4RenderCommandEncoder33 |
| GameDecal | pipeline rps16; GroundDecal과 별도 shader family |

현재 replay의 상대 비용은 SensorLOS FS 26.342153%, GroundDecal FS 14.741725%, GameDecal FS 8.599261%, GroundDecal VS 2.928513%다. 이는 이 replay의 상대 측정량이며 CPU FPS의 분해, 실시간 GPU ms, 최대로 회수할 FPS 비율이 아니다. 이전 b25/S01의 instruction/register 수를 현재 R02 값으로 옮기지 않는다. 현재 instruction·register·fragment invocation counter는 export에 없다. [비용 raw][R3], [API raw][R1], [stage raw][R2]

## 3. GPU 작업을 만드는 상위 원인

### 3.1 SensorLOS: 선 하나의 규칙을 픽셀마다 다시 계산한다

**관측.** 현재 호출은 Triangle 378 vertices × 165 instances다. 63개 segment마다 6 vertices를 만들며, API의 native Metal Line draw가 아니다. stencil은 별도 depth/stencil 상태와 함께 유지된다. 반면 저장된 MVK3는 indexed Line 126 indices × 171 instances, setLineWidth 5다. 서로 다른 캡처 시점·인스턴스 수를 가진 자료다. [R1][R1], [M2][M2]

**소스 사실.** native vertex는 원본 두 끝점의 높이 보정·clip·원근 자료를 준비하고, R01의 보수적 픽셀 영역을 삼각형으로 만든다. sample1, 유한 좌표, 폭/좌표 상한 등의 guard 안에서만 축소된 영역을 쓰며 guard 밖은 fallback이다. R02는 이 영역의 Y-major 방향 winding 문제를 수정했다. 따라서 “전체 화면 사각형을 작은 사각형으로 바꾸자”는 이미 R01이 처리한 문제다. [Sensor 28–130행][S1]

남은 fragment 경로는 다음 일을 수행한다.

- 끝점 양자화: floor(x×256+0.5), delta 및 x-major/y-major 선택.
- 64-bit numerator/denominator와 floor division으로 중심 minor 계산.
- 최대 세 후보 minor에 대해 SensorDiamond의 두 축 slab/fraction 교차 판정. 분수 비교는 epsilon 항까지 lexicographic하게 처리한다.
- 폭 반올림 및 또 다른 floor division으로 lower/count를 계산한 뒤 실제 pixel minor 거부.
- 원본 alpha·mask·원근 보간·거리별 thinning과 출력 depth를 계산하고 discard/stencil 결과를 결정.

fragment마다 계산되는 a,b,delta,major 축, 분모, 정규화 방향, 폭 count는 같은 segment 안에서 불변이다. 세 diamond 후보의 판정도 동일 segment의 동일 major-column 안에서는 실제 pixel minor와 독립이다. **이 중복은 소스에서 직접 도출되는 작업 중복이다.** 단, 실제 compiler가 어느 부분을 공통화하는지, instruction/occupancy에 미치는 크기는 현재 counter가 없어 UNKNOWN이다. [153–199행][S1]

“mask가 없으면 먼저 버리자”는 손쉬운 대체가 아니다. 현재 mask의 implicit derivative를 계산한 뒤 discard하며, coverage가 없는 픽셀이 stencil에 표시되지 않게 한다. coverage 내부의 major-axis depth 검사와 최종 dot-product t 기반 depth 출력은 서로 다른 식이다. 이것을 하나로 합치거나 depth(any)를 삭제하거나 fast-math를 켜는 것은 이 감사의 제안이 아니다.

**MVK의 낮은 비용을 해석하는 제한.** 저장된 MVK FS는 mask sample, smoothstep, inboundsness와 alpha 곱, alpha discard 중심이고 depth 출력도 없다. 저장된 VS/FS에서는 native의 LINE_THIN_START/END, 거리별 width와 fadeStrength 계산을 확인할 수 없다. native는 source family 검증과 embedded 원본 GLSL에서 thinning 3000–8000, scale .4, min .7 및 fade 4000–8000 계약을 명시적으로 유지한다. 이 차이는 실제 저장된 shader의 차이다. MVK의 어떤 define/overlay/variant 선택 때문에 그 계산이 빠졌는지는 source-to-capture 연결 자료가 부족하여 UNKNOWN이다. 이 코드 차이가 해당 고정 카메라에서 실제 픽셀을 얼마나 바꿨는지도 UNKNOWN이다. “native만 잘못 기능을 추가했다”거나 “MVK가 언제나 해당 기능을 누락한다”고 단정하지 않는다. [M3][M3], [M4][M4], [S5][S5], [source-family gate][S12]

Mesa/Zink 소스는 rectangular/smooth/Bresenham/default 분기를 지원하고, MoltenVK 소스는 private API 허용 및 selector 확인 뒤 line width를 설정하는 경로를 가진다. 로컬 MoltenVK 파일 해시는 Small 검토 기록의 postimage와 일치하고 해당 검토는 signed MVK abf4fb1…의 __TEXT 연결을 남긴다. 이것과 raw Line/width 호출은 하드웨어 선 경로 사용의 근거다. 그러나 이것만으로 실행 당시 Vulkan line rasterization 분기나 native diamond-exit 규칙과 모든 경계에서의 동일성을 증명하지 않는다. d715… 엔진과 전체 Mesa/MVK 스택의 정확한 source closure를 이번 감사에서 완전히 재구성하지도 않았다. 오래된 Formal1.0.3 SOURCE-MAP의 다른 MVK 바이너리를 Small의 신원으로 대체하지 않았다. [Zink state][M5], [Zink pipeline][M6], [MVK line width][M7], [Small 검토][M8]

### 3.2 GroundDecal: 인스턴스 수와 publication 비용을 구분해야 한다

**관측.** api5530은 G01의 36 UInt16 indices, 32,364 instances다. index buffer는 72바이트이고 별도 uniform은 368바이트다. api792는 6,213,888바이트 인스턴스 버퍼를 생성한다. 32,364×192와 일치한다. 저장된 MVK3의 GroundDecal은 32,545 instances다. [R1][R1], [M2][M2]

**소스 사실.** 원본 GL draw도 decals.size() 전체를 36 vertices로 그린다. reserve(65536)는 capacity이며 draw instance count가 아니다. native capture가 예약 용량 전부를 잘못 그린다는 주장은 기각한다. invalid/expired 제거와 visibility 갱신은 원본 handler의 정책에 따른다. native VS의 alphaMax<=0 또는 type<=0 분기는 이미 clip 밖으로 보내 fragment 생성을 막는다. 이 조건으로 active list를 만들더라도 먼저 줄어드는 것은 CPU/VS 일이며 14.74%의 FS 비용이 그대로 없어지는 것은 아니다. [원본 handler][S6], [VS][S7]

단순히 “현재 유닛 뒤에 가려졌으므로 제거”할 수도 없다. GroundDecal은 같은 프레임의 **terrain-only depth snapshot**에서 world position을 재구성한다. snapshot은 units/water 이전 경계에서 만들어지며, GroundDecal 자체의 depth-write/test와 원본 blend 순서를 보존해야 한다. 모든 후속 장면의 depth를 대신 쓰면 원래 의미를 바꾼다. fragment는 depth 역투영, 평면 투영, atlas·normal·minimap·shadow·lighting·water 처리를 한다. footprint 축소가 FS를 줄일 가능성은 있지만 현재 instance별 유효/숨김/overdraw 분포는 없다. [snapshot 경계][S13], [Ground shader][S7]

**더 강한 차이는 변경 전파다.** 원본 GL은 UpdateList의 변경 범위를 BufferSubData한다. native Capture는 길이가 같아도 변경이 하나라도 있으면 snapshot->values 전체를 새 vector로 복사한 뒤 dirty range를 덮는다. 길이가 바뀌면 전체 rows를 복사한다. 새 revision은 GPU 준비에서 새 전체 버퍼를 초기화한다. 변경이 없는 revision은 재사용하므로 “무조건 매 프레임 6MB”라는 주장은 틀리다. 이 캡처에서 전체 생성은 보이지만 clean ROI에서 몇 번 발생했는지는 UNKNOWN이다. [Capture 72–91행][S2], [GPU 준비 160행][S3], [GL dirty upload 815–824행][S6]

이는 immutable publication을 위해 선택한 **구현 정책의 증폭**이다. GPU가 읽는 이전 바이트를 보호해야 한다는 필요는 유지되지만, 한 row의 변경을 매번 전체 CPU 복사와 전체 새 버퍼로 표현해야 하는 것은 필수 조건이 아니다. 기존 published-buffer/page 및 pin 원칙을 좁게 적용할 수 있다. Apple 통합 메모리에서 이 newBuffer 초기화를 곧바로 “PCIe로 6MB 전송”이라고 부르지도 않는다.

원본 compaction 부분에는 별도 정적 이상도 보인다. 주석은 invalid 비율 25%를 말하지만 조건은 size/numToDelete<=.25다. numToDelete가 양수이고 size 이하라면 참이 될 수 없다. 이는 감사에서 발견한 **주석/조건 불일치**이며 실제 제거 시점 정책을 따로 검토할 사항이다. 양 backend가 공유하는 handler이므로 native–MVK 격차의 원인으로 채택하지 않고, 이번 감사에서 고치지도 않는다. [CompactDecalsVector 1468–1548행][S6]

### 3.3 S01 이후에도 “패스를 나누면 된다”는 제안을 반복하면 안 된다

현재 raw api8360–8363은 decal encoder 종료와 grass encoder 시작 양쪽에 Fragment→Fragment 순서 보장을 둔다. 소스는 기존 terrain depth snapshot 및 attachments를 유지하면서 S01 경계를 사용한다. Grass VS를 앞선 decal FS 전체 완료에 묶는 넓은 의존성은 이미 완화된 상태다. [S01 소스][S14], [R1][R1]

| 현재 stage | raw 시작–끝, ns | interval count |
| --- | --- | --- |
| GameDecalFragment | 4,723,291–10,884,535 | 1 |
| GroundDecalFragment | 4,806,433–11,084,292 | 2 |
| GrassVertex | 4,725,958–8,510,676 | 29 |
| SensorLOSFragment | 15,997,958–24,334,274 | 1 |

Grass VS의 envelope는 decal FS 구간 안에 들어간다. 이는 S01의 의도와 일치하지만 29개 active interval 전체가 export되지 않은 envelope만으로 정확한 동시 실행량을 계산하지 않는다. 두 stage의 구간을 더하거나 빼서 runtime 절감 ms를 만들지 않는다. attachment load/store의 실제 메모리 트래픽도 현재 미측정이다. 이미 순이익이 측정된 S01을 “pass 수가 늘었다”는 이유로 되돌리거나, 동일 split을 새 개선으로 제시하지 않는다. [R2][R2], [역사적 S01 측정][P1]

GameDecal FS 8.60%는 별도 family다. GroundDecal의 index 변경·instance publication으로 자동 해결되지 않으며, 두 family의 atlas/geometry/coverage 계약을 섞어 하나의 “decal 최적화”로 보고하지 않는다.


## 4. CPU–GPU 제출·회수와 수명 계약

### 4.1 현재 경로의 의존 관계

아래 N은 simulation 번호가 아니라 source request / runtime submission의 순서를 뜻한다. 실제 연결은 request, sourceDraw, job, lease, submission, event 및 resource generation으로 해야 한다. GPU 완료, worker 완료, 화면 표시 완료는 서로 다르다.

~~~mermaid
flowchart TD
  A["Main: UpdateUnsynced + source credit"] --> B["같은 draw의 World / UI / Post capture"]
  B --> C["immutable request publication + resource pins"]
  C --> D["serial worker: Prepare / Encode N+1"]
  P["이전 제출 N의 GPU 실행"] --> E["event N 완료"]
  P --> F["feedback N: error 및 완료 통지"]
  D --> R["Submit N+1 직전 RetirePreviousSubmission"]
  E --> R
  F --> R
  R --> Q["allocator reset / producer receipt / read checks"]
  Q --> S["queue commit N+1 / signal event"]
  S --> T["완료한 N의 owner graph 해제"]
  S --> U["GPU N+1"]
  D --> J["worker callable 종료 / main join 및 release"]
  U --> V["GPU 완료 후 retirement receipt"]
  V --> W["main: 정확한 World/UI/Post serial 정산"]
  J --> W
  W --> X["source credit / 이전 publication 소유권 회수"]
~~~

그림은 모든 프레임이 반드시 같은 긴 대기를 갖는다는 뜻이 아니다. early drain, rollback, 동기 fallback 및 resource 재사용 요구에 따라 경계가 앞당겨질 수 있다. **현재 PREPARE1은 N의 GPU 실행과 N+1의 준비·기록을 겹칠 수 있지만, N+1 GPU 제출은 N retirement 뒤다.** 동일 Runtime의 deferred presentation pending 제한은 1이다. recording slot이나 source credit 수를 3으로 늘렸다고 GPU 3프레임이 동시에 제출되는 것이 아니다. [SceneBridge][S15], [World dispatch][S16], [Runtime][S4], [credits][S17]

main의 DrawScreen 콜백은 World dispatch 이전에 관측될 수 있다. queued/presented 플래그는 sourceFrameCompleted와 분리되며, 실제 retire 때만 해당 source serial의 World/UI/Post를 정산한다. 따라서 DrawScreen FPS와 GPU 또는 display FPS를 같은 완료 개념으로 해석하면 안 된다. [Game][S10], [World submit 5844–5990행][S16]

### 4.2 대기마다 필요한 조건과 현재 정책

| 경계 / 소스 | 반드시 보존할 조건 | 현재 구현에서 선택한 정책 | 이번 감사의 판정 |
| --- | --- | --- | --- |
| source credit 획득, SceneBridge 265 / 991–1004행 | 살아 있는 source/UI/post snapshot을 너무 일찍 재사용·정산하면 안 됨 | PREPARE 사용 시 credit 3; BEGIN 기반 CPU-only reclaim이 안 되면 부족 시 full drain | 정확성상 현재 필요. credit 수 증가 자체는 workload 감소가 아님 |
| serial worker 재사용, NativeSerialRenderWorker 14 / 31–40 / 72–93행 | 이전 callable과 borrowed CPU owner가 끝난 후 같은 mutable context를 사용 | single worker, outstanding claim 1; future ready만으로 release하지 않음 | 단순 worker 추가·join 삭제 기각. immutable worker 입력 분리가 먼저 |
| Dispatch 이전 FinishCPUWork, World 2507행 | 준비/기록 context와 이전 CPU 작업 충돌 방지 | 매 dispatch 전에 이전 CPU 작업 정산 | GPU 완료 대기와 동일하지 않음. wall envelope를 active CPU 시간으로 세면 안 됨 |
| ReleaseForPreparation, Settlement 52행 | source와 worker의 소유·오류 전달 유지 | Running 상태에서는 조기 반환 가능; 준비 완료까지 기다리는 API가 아님 | PREPARE1을 “아무 overlap 없는 동기 경로”로 설명하는 것은 틀림 |
| resource reuse / read-ready, Runtime 699–865행 및 Postprocess 480–515행 | 정확한 logical version의 producer/consumer 순서, 물리 저장소 pin, overwrite 금지 | 이전 pending read를 등록하고 필요 시 drain. pending postprocess 입력을 successor submit 전에 completed로 확인 | dependency 자체는 필수. CPU wait 위치를 옮기려면 모든 borrow 범위 증명이 필요 |
| SubmitAndDefer 1030–1045행 | 미완료 producer를 잘못 읽거나 mutable storage를 덮으면 안 됨 | N+1 기록 후 N 전체 retire, pending empty 확인 후 submit | 계약 제약으로 생긴 직렬화. 임계 경로 비용은 UNKNOWN |
| event + feedback, Runtime 1166–1187행 | GPU 소비 완료 및 성공/실패 판정 뒤 allocator·receipt·residency 재사용 | event wait 뒤 feedback semaphore wait, reset/retire/callback/cleanup | 같은 GPU 실행시간을 두 번 더하면 안 됨. feedback error 판정은 event 값만으로 대체 불가 |
| completed owner release, Runtime 1113–1126행 | GPU가 끝난 owner만 해제하고 pool reuse와 경쟁하지 않음 | 이전 완료 graph 파괴를 다음 submit 뒤 같은 호출자로 이동 | 이미 구현됨. “destructor를 submit 뒤로 옮기자” 중복 제안 제외 |
| waitForDrawable / present, Runtime 1097–1100행 | drawable 순서 및 사용권 | queue의 drawable wait/commit/signal 및 present | 이름만 보고 CPU blocking ms라고 단정 불가. nextDrawable CPU 대기와 별도 |
| main receipt retirement, SceneBridge 921–945행 / Game 1780–1789행 | GPU 완료한 정확한 sourceDraw 및 UI/Post serial만 회수 | 완료 receipt를 main이 검증 후 release | callback에 소스 정산을 임의 이동하면 thread·순서 계약을 바꿈 |
| 실패/poison 경로 | 성공하지 않은 제출의 소유자를 조기 파괴하거나 결과를 성공으로 publish하지 않음 | quarantine owner를 commit 전에 등록, 실패 시 fail-closed | 성능 패치가 반드시 유지해야 하는 수명 조건 |

근거: [worker][S18], [settlement][S19], [bridge][S15], [runtime][S4], [world][S16], [postprocess][S9], [credits][S17].

### 4.3 retain, cache, pin, COW는 같은 말이 아니다

MTLBuffer/MTLTexture 객체를 retain해 두어도 그 내부 바이트를 다른 CPU/GPU 작업이 쓰는 것은 막지 못한다. 프레임 자원 소스가 이 차이를 명시한다. 필요 조건은 **논리 버전의 동일성**, **물리 저장소의 생존**, **소비 기간의 쓰기 금지**, **producer→consumer 순서**, **성공/실패 후 정산**이다. 하나의 shared_ptr만으로 전부 충족되지는 않는다. [S8][S8]

현재 published request cache는 최신 key별 cache와 이미 그 요청에서 사용한 publication pin 목록을 분리한다. 같은 key가 나중에 바뀌더라도 앞서 encode한 GPU address의 owner는 남겨야 한다. published buffer pool도 sole ownership이 확인된 slot만 다시 쓴다. 이 원칙은 중복 비용의 원인이기도 하지만 잘못 제거하면 use-after-free보다 더 찾기 어려운 “객체는 살아 있고 내용만 바뀐” 오류를 만든다. [request cache][S20], [buffer pool][S21]

MRT/image COW는 모든 경우 불필요하다고 할 수 없다. same-frame terrain depth snapshot, bump-water recurrence, postprocess 입력, borrowed scene input은 서로 다른 읽기 경계를 가진다. 현재 World의 MRTFrontier 특수 경로는 BEGIN/PREPARE 조기 반환이 모두 꺼진 조건에서만 허용되므로 현재 PREPARE1에 무조건 적용된다고 볼 수도 없다. 현재 clean 기록에는 MRT164 first-encoded marker가 있지만 프레임별 reuse/COW/copy 전체 장부는 없다. “매 프레임 전체 MRT가 복사된다” 또는 “MRT 복사는 이미 0이다”는 모두 UNKNOWN이다. [World 2519행·4496–4545행·5039행][S16], [clean 제한][R4]

결과적으로 다음 두 주장은 분리해야 한다.

- **입증됨:** 현행 소스는 다음 GPU 제출 전 이전 ticket의 retirement를 강제한다. 다중 GPU 제출이 차단되는 구체적인 이유가 있다.
- **입증되지 않음:** 그 정책이 지금 MVK3와의 차이에서 가장 큰 몫이다. GPU가 대부분 바쁘다면 pending 수 증가의 이득은 작을 수 있고, CPU/정산 공백이 크다면 중요해질 수 있다.

## 5. 큰 후보 세 개와 선행 조건

이 후보들은 구현 승인이나 예상 FPS가 아니다. 각 후보의 실제 workload 감소, 수명 보존 및 최종 FPS를 별도로 검증해야 한다. 새 framework·daemon·validator·protocol를 만드는 제안은 포함하지 않는다.

| 우선순위 | 후보 | 제거하려는 일 | 핵심 위험 / 선행 조건 | 현 상태 |
| --- | --- | --- | --- | --- |
| 1 | SensorLOS의 exact coverage를 segment/major-column 단위로 표현 | 동일 segment 및 동일 major-column의 양자화·64-bit 분수/diamond 판정 반복 | 원본 경계·epsilon·winding·sample·보간·depth·stencil 그대로 유지; 추가 varying/lookup/prepare 비용 상쇄 여부 | 중복 구조는 입증, 구체 구현 및 성능 UNKNOWN |
| 2 | GroundDecal의 버전별 dirty publication | 한정된 변경 때문에 생기는 전체 CPU snapshot copy·전체 새 instance-buffer 초기화 | dirty base revision, 누적 변경, resize/compaction, GPU read pin, 재사용 가능한 slot의 완전성 | 전체 갱신 경로와 실제 대형 버퍼 생성 입증; ROI 빈도·순이익 UNKNOWN |
| 3 | 정확한 자원 버전·읽기 권한에 기반한 retirement/submit 경계 분리 | GPU가 비는 경우의 제출 전 CPU 대기·정산 직렬화 | 모든 pending borrow 등록, MRT/postprocess/water alias, 실패 quarantine, main receipt·credit 순서 | 현재 제한 입증; 현재 critical-path 효과 UNKNOWN. 앞 두 후보보다 준비 범위 큼 |

### 후보 1 — fragment를 싸게 만드는 것보다 반복 횟수를 줄이는 표현

1단계는 기존 segment 안에서 불변인 양자화 끝점, major 축, 정규화 분모와 폭을 shared setup으로 올리는 것이다. 기존 6개 vertex가 같은 setup을 반복할 수 있으므로 “VS로 옮기면 한 번”이라고 부르면 안 된다. flat varying 증가 및 register/메모리 비용도 함께 따져야 한다.

더 큰 단계는 **동일 major-column의 세 diamond 판정을 폭 방향 여러 fragment가 공유하는 표현**이다. 예를 들어 기존 predicate로 정의한 column coverage/lower를 한 번 계산하는 좁은 자료 표현, 또는 모든 예외를 증명한 동등한 닫힌식이 가능성이다. 소스에서 column 독립성은 도출되지만, coverage가 항상 하나의 단순 연속 구간이라는 정리는 이번 감사에서 증명하지 않았다. subpixel/degenerate/clip/tie/half-open endpoint 사례를 무시한 단순 strip은 제안하지 않는다.

이 후보가 바꾸지 않아야 할 것은 mask sample의 derivative 위치, 거리별 thinning/fade, 원근 보간, separate depth 식, front/back stencil, 마지막 endpoint 처리다. sample1 guard 밖과 true MSAA 경로는 검증 범위가 넓어지기 전 기존 fallback을 유지한다. 새로운 GPU setup/lookup 단계가 기존 계산보다 비싸면 기각한다. **GPU exactness와 통계적 FPS 개선 모두 통과해야 채택**한다.

부록의 floor-division 항등식은 작은 보조 아이디어다. 큰 후보를 단순한 divide 한 개 삭제와 동일시하지 않는다. R03의 early reject와 연산 순서가 상충할 수 있으므로 별도 후보로 동시에 섞지 않는다.

### 후보 2 — 복사보다 먼저 publication의 증분 계약

변경된 row/page만 materialize하고, 이미 같은 base revision을 가진 writable slot에는 그 이후 변경만 적용하는 방식이 목표다. 기존 immutable owner와 published-buffer pool 원칙을 재사용하는 좁은 개선으로 시작한다.

반드시 **slot의 기준 revision 이후 누적된 모든 변경**을 알아야 한다. 최신 프레임의 dirty list만 오래된 slot에 적용하면 중간 프레임의 변경을 빠뜨린다. resize/compaction/reorder는 전체 재구성 사유가 될 수 있다. base가 불명확하면 full-copy fallback이 정답이다. in-flight 또는 request pin이 있는 저장소는 쓰지 않는다. source 인스턴스 순서·ID·G01 representative vertex/provoking 규칙은 그대로 유지한다.

“기존 GL처럼 같은 버퍼에 BufferSubData”만 이식하는 것은 native CPU lookahead와 이전 GPU 소비를 침범할 수 있다. 반대로 항상 새 버퍼를 만드는 것만이 안전한 해결책도 아니다. 소유권과 revision을 명확히 하면 기존 구조 안에서 dirty byte와 전체 byte를 구분할 수 있다.

active compaction/footprint 최적화는 이 후보와 구분한다. 분포가 없는 현재 상태에서는 복사 경로를 먼저 명확히 하고, inactive instance가 충분한지·어느 단계 비용을 줄일지 확인한 뒤 별도 판단해야 한다. G04의 vertex 불변식 hoist를 새로운 후보인 것처럼 재명명하지 않는다.

### 후보 3 — CPU lookahead와 GPU queue lookahead를 분리해 설계

첫 단계는 “GPU 2프레임 허용” 플래그를 켜는 것이 아니다. 기존 코드도 pending-borrow 증명이 없으면 second GPU ticket을 막는다. 현재 source의 read checks map이 비어 있다는 사실은 “의존성이 없다”가 아니라 “등록된 검사가 없다”일 수 있다. [Runtime 1039–1044행][S4]

필요한 최소 계약은 다음과 같다.

- 각 읽기가 참조하는 정확한 resource generation과 물리 allocation, 최종 소비 ticket.
- 해당 allocation의 writer와 alias 범위. 같은 객체 ID라도 logical version은 다를 수 있음.
- producer completion이 GPU queue 순서로 충분한 경우와 CPU에서 결과를 확인해야 하는 경우의 구별.
- 완료 전 write/reset/recycle 금지 및 실패 시 보존할 owner.
- main이 처리해야 하는 World/UI/Post serial 및 source credit의 retirement 순서.

이 계약을 existing frame resources / read scope / postprocess publication 경계에서 완결한 다음, 실제로 GPU를 비우는 CPU retirement 부분만 submit 뒤로 옮길 수 있는지 판단한다. allocator·shared MRT를 N과 N+1이 함께 쓰는 상태에서 event wait만 삭제하는 것은 허용되지 않는다. 이미 submit 뒤로 옮겨진 completed owner destruction도 다시 개선으로 세지 않는다.

단일 worker의 context를 먼저 안전하게 분리하지 않고 worker 수까지 동시에 늘리면 GPU queue 개선과 CPU 병렬화 효과·오류 원인을 분리할 수 없다. 이 후보의 첫 구현이 전체 렌더러 rewrite일 필요는 없으나, proof surface가 앞 두 후보보다 넓다.

## 6. 공통 GLSL / Lua v2가 해결하는 경계

공통 shader 경로는 가치가 있다. source variant·define·layout·binding·provenance를 한 번 정의하면 수동 native 번역과 원본 GLSL 사이의 drift를 줄일 수 있다. 특히 이번 MVK/native SensorLOS의 variant 차이를 기록 없이 묵인하지 않게 하는 데 도움이 된다. 그러나 **언어를 공통화하는 것과 원래 GL primitive·동기화 의미를 저렴하게 구현하는 것은 별개**다.

| 공통화가 직접 다룰 수 있는 것 | 공통화만으로 없어지지 않는 것 |
| --- | --- |
| shader source/define 선택과 variant identity | Metal 하드웨어 Line과 원본 diamond/wide-line/epsilon의 차이 |
| layout·binding·uniform packing 중복 | exact coverage, depth/stencil, implicit derivatives, blending/order |
| 가족별 handwritten translation의 유지 비용 | GroundDecal 전체 immutable copy 정책 |
| 검증된 pipeline/cache key의 공통 규칙 | GPU가 읽는 allocation의 생존·쓰기 금지·logical version |
| Lua 호출에서 의도를 잃지 않는 typed capture | terrain-only snapshot, water recurrence, postprocess pending borrow |
| 원본/변환 shader와 resource metadata의 provenance | 현재 pending=1 제출 정책 및 실제 GPU idle 원인 |

원본 Lua가 임의 버퍼/texture를 수정·삭제·재바인딩할 수 있는 경계에서는 capture 시점의 값과 lifetime을 고정해야 한다. v2를 먼저 크게 만든 뒤 resource generation/pin 계약을 뒤늦게 붙이면 같은 COW·복사·대기를 다른 위치에서 다시 구현할 위험이 있다.

권장 선행 순서는 **원본 기능·variant 계약과 immutable publication/자원 접근 계약을 먼저 고정 → 공통 GLSL/Lua 경로가 그 계약을 생산하도록 연결 → backend별 lowering을 선택**이다. SensorLOS의 exact 연산 중복 감소는 v2 전체를 기다리지 않고 현재 좁은 shader family에서 검증할 수 있다. Ground publication은 v2와 native 양쪽이 공유할 수명 계약부터 정리하는 편이 낫다. 다중 GPU 제출은 해당 계약의 완전한 적용과 실제 runtime 근거 뒤다.

## 7. 기존 시도의 처분과 중복 방지

| 항목 | 실제 완료된 근거 | 독립 감사 처분 |
| --- | --- | --- |
| R01 | 새 ABBAAB 비교 47.4623→53.2372, +12.1674%; 별도 exact helper | 이미 적용된 conservative raster 영역 축소. 그대로 유지하고 새 제안에서 제외 |
| S01 | 새 ABBAAB 53.6924→58.6211, +9.1795%; 현재 raw에도 pass/stage 분리 확인 | 이미 적용. 추가 split/합치기 전에 실제 의존성을 증명 |
| G01 | 같은 binary OFF/ON ABBAAB 58.5897→62.7989, +7.1842%; 현재 indexed draw 확인 | 이미 적용. 36 index / 8 representative ID 및 provoking/order 유지. 실제 vertex-cache reuse의 기여량은 별도 미측정 |
| R02 | asymmetrical stencil/winding 정확성 수정, 현재 clean −0.02845% | 성능 중립과 correctness 가치를 분리. G01 대비 FPS 개선으로 보고하지 않음 |
| G02 | 368-byte uniform + 72-byte index의 두 upload 객체를 하나로, 순 FPS −0.08443% | 객체 수 감소 후보 보존. FPS·RAM·peak-memory 개선으로 채택하지 않음. logical440과 host448 구분 |
| G03 | minimap sample skip의 finite/UV/worldPos/override texture 영역 증명 불완전 | HOLD 유지. 현재 정상 UNORM atlas 일부 근거를 일반 Lua texture 계약으로 확대하지 않음 |
| G04 | source/CPU 준비 및 freeze. GPU helper·engine·game·FPS 없음 | 미검증 상태 유지. “VS invariant hoist”를 새 성과로 재제안하지 않음 |
| R03 | old/new 12,065 비교, compile1/GPU1, validation 통과 | 한정된 성능 비교까지 계속. large-coordinate CPU-only 및 true AA4 미실행 제한 유지 |
| 과거 Simmers | S01 185.4639, G01 183.0의 시간 분리 단발 기록 | 원인·회귀 UNKNOWN. −98 DrawScreen 중 86이 17–23초에 몰린 분포는 원인 증거가 아님 |
| 과거 G02 low 진단 | 실제 owner summary Prepare0 | 그 실행의 Ground 준비가 없었다는 뜻. 과거 G01 단발 차이의 원인을 소급 설명하지 못함 |
| completed-owner release / recording slots | 현재 source에 이미 있음 | 새 개선으로 중복 집계하지 않음. second GPU ticket이 활성이라는 뜻도 아님 |

R01/S01/G01/G02는 각기 독립적인 fresh cohort다. 개선율을 더하거나 곱해 누적 관측 성과를 만들지 않는다. 저부하 Simmers는 원본 camera-height grass cull이 적용된 관측이고, 고부하에서 유효한 shader 최적화가 그 장면에서도 같은 효과를 내야 한다는 전제는 없다. 과거 synthetic 64×64 sample1/no-shadow/low-quality 및 RG32 normal 검사는 production RG16/Ultra/full-game 동등성 검사가 아니다. R01의 별도 3,600-case 검사와도 합치지 않는다. [과거 evidence index·summary][P1], [G03 proof domain][P2], [G04 freeze][P3], [R03][R7]

## 8. 처음 실행할 실험 하나와 이후 판단

### 선택: 고정 R03 대 고정 R02, 동일 Ultra AA0 한정 비교

**선택 이유:** 현재 가장 큰 replay target을 겨냥하고, source 변경이 좁으며, 원본 및 비대칭 stencil 등을 포함한 old/new 정확성 검사가 이미 완료되었다. 추가적인 수명 변경·framework 작업 없이 “minor 먼저 거부”가 실제로 순이익인지 가장 작은 인과 비교로 닫을 수 있다. 큰 구조 변경을 기다리는 동안 끝없이 작은 patch를 누적하자는 뜻은 아니다.

**R03 계속/보류 판정:** 성능 확인까지 계속. 단, 이 감사 시점의 R03 engine configure/build는 0이며 지금 문서가 자동 실행 권한을 부여하지 않는다. HQ/JINI의 기존 인계 및 고정 실행 절차로 진행한다. 현재 READY를 RUNNING으로 바꾸거나 본 감사가 빌드를 시작하지 않았다.

고정 비교 조건:

- A=현재 R02 source/binary. B=0780bd7c530b0696067b717428b49a416eda556a의 R03만 반영한 후보. source closure·dependency·실제 binary 및 runner pins를 새로 결속한다.
- 변경 변수는 SensorLOS minor reject 순서 하나. PREPARE1 BEGIN0 worker1, S01/G01 ON과 다른 OFF 설정, game/map/seeds/camera/viewport/drawable/sample/전원 조건은 R02 실험 그대로다.
- 엔진 경로 validation이 먼저 필요하다. helper pass를 full-engine validation으로 대신하지 않는다. validation과 clean FPS는 분리한다.
- 기존 예산형식의 ABBAAB, 각 3회, simulation540–1740 / 약40초를 사용한다. 유효한 느린 실행을 제외하거나 과거 수치를 pool하지 않는다.
- 40개 world/camera/speed/pause row 및 생성/파괴·seed를 확인한다. line, front/back stencil, clipping/facing과 실제 guard/fallback 범위를 기존 정확성 증거 및 엔진 화면과 연결한다.
- clean에서는 capture/probe/validation을 끈다. diagnostic 시간이 섞인 FPS는 채택하지 않는다. build/helper/capture 재시도 권한을 자동으로 넓히지 않는다.

판정은 평균 차이만 반올림해 승패로 삼지 않는다. 각 실행 값·분산·일관성, 전 구간 결과, correctness 및 해당 경로 적용 여부를 함께 제시한다. 차이가 산포와 구분되지 않으면 성능 효과 UNKNOWN/중립으로 닫고 추가 미세 패치를 자동 연쇄하지 않는다. true AA4 개선이나 MVK3 격차 해소로 확대하지 않는다.

R03은 기존 lower floor-divide를 먼저 계산하여 바깥 fragment를 빨리 거부하지만, 이전에는 diamond 단계에서 끝나던 fragment에 division을 앞당길 수 있다. 따라서 helper pass만으로 빨라질 것이라고 가정하지 않는 이 비교가 필요하다. [R03 172–209행][S11]

### 구조 변경 전에 남은 UNKNOWN과 필요한 다음 관측 하나

**UNKNOWN:** 현재 R02의 실시간 GPU busy 구간, 다음 submit 사이의 빈 시간, CPU prepare/encode·event/feedback·retirement가 그 빈 시간과 겹치는 정도. replay stage share와 clean DrawScreen FPS는 이 질문에 답하지 못한다.

이 불확실성을 해소할 최소 관측은 **동일 workload에서 source request → worker job → runtime lease/submission/event → GPU start/end → retirement를 연결한 현재 runtime 타임라인 하나**다. 이것은 위 R03 비교에 몰래 추가하는 두 번째 실행 요청이 아니라, 후보3을 구현하기 전에 필요한 증거의 명세다. 기존 native trace identity 및 CPU phase 기록을 활용하고, current deferred path의 실제 GPU timestamps와 대응해야 한다. 동기 제출 전용 NativeSubmitTrace를 deferred 경로의 대기 수치로 잘못 읽지 않는다.

기존 FrameOverlapProbe는 1/16 sample, 최대128행이며 시작부터 소비된다. 이를 그대로 켜면 warmup에서 quota를 소진하여 원하는 교전 ROI가 없을 수 있다. CPU trace의 START_SIM/END_SIM 지원과 GPU probe의 범위는 같지 않다. 현재 도구로 필요한 구간·identity 연결을 확보할 준비가 되었는지 먼저 확인해야 하며, 기록이 부족하면 UNKNOWN으로 남긴다. 새 profiler framework를 만들거나 이번 감사에서 실행하지 않는다. [CPU trace][S22], [frame overlap probe][S23], [Runtime 1137행 이후][S4]

그 관측에서 GPU가 계속 바쁘다면 후보1의 실제 GPU 작업 감소가 우선이고, 광범위한 instance publication CPU 비용이면 후보2가 우선이다. GPU가 비어 있고 이전 retirement/CPU gate가 다음 submit을 늦춘다는 연결이 확인될 때 후보3에 비용을 투자한다. 단순 wall-time envelope의 중첩, event wait의 길이 또는 다른 오래된 workload의 GPU activity 비율만으로 이 결정을 내리지 않는다.

## 9. 부록 — 정확한 정수식의 국소 검토

이는 source 기반 수학 검토이며 코드 변경·CPU/GPU 실행·속도 검증을 한 것이 아니다. R03과 동시에 적용하자는 제안도 아니다.

현재 정규화된 numerator를 N, denominator를 D>0, count를 c>=1이라 하자. q=floor(N/D), r=N−qD이면 0<=r<D이고,

~~~text
lower = floor((2N − (c−1)D − 1) / (2D))

c=2k+1 이면: lower = q − k − [r=0]
c=2k   이면: lower = q − k + [r>D/2]   (현재 D는 256의 배수)
~~~

이는 두 번째 floor division의 정확한 동치식이다. −1 epsilon 항은 odd case의 r=0 경계와 even case의 strict greater 비교에 남는다. 음수 N에도 q를 truncation이 아닌 floor로 정의했으므로 성립한다.

현재 R03 guard처럼 endpoint 좌표 절댓값<=16384, fragment 좌표<=32768, width<=64, 유한 값 및 sample1을 가정하면 양자화 endpoint는 절댓값<=2^22, delta는 <=2^23이다. N의 보수적 크기는 2^48 미만, D는 <=2^31, lower 식의 중간값은 2^49 미만으로 64-bit 범위 안에 있다. 이것은 32-bit로 줄여도 된다는 증명이 아니다. SensorDiamond의 별도 fraction cross-product·epsilon 항은 그대로 독립 검토 대상이다.

division 하나를 줄여도 추가 remainder 곱셈, compiler lowering, register pressure, early-reject 전 계산량 때문에 순이익은 UNKNOWN이다. 특히 R03은 lower를 계산한 뒤 탈락한 fragment에서 q 계산을 생략한다. q/r 공통화를 먼저 하면 그 이익 일부를 되돌릴 수 있다. 이 항등식은 큰 구조 후보1의 보조 선택지이지 별도 채택 권고가 아니다.

## 10. 출처와 감사 한계

실제 외부 코드의 attribution은 기존 engine/GLSL/Mesa/MoltenVK header와 UPSTREAM-CONTRIBUTIONS 기록을 따른다. 원본 Sensor circle shader의 Beherith/MIT 표기, engine GPLv2-or-later, Mesa/MoltenVK의 원래 라이선스를 제거하거나 본 감사의 창작물로 재표시하지 않았다. tomjn의 LuaVBO bindingIndex 진단 인자 수정과 다른 원본 기여는 기존 provenance에 남아 있으며, 미채택 제안을 적용된 변경으로 세지 않았다. 이 문서는 감사자의 분석과 제안이며 upstream 코드의 저작권을 새로 주장하지 않는다. [기여 기록][I3]

이번 감사가 하지 않은 것은 전체 소스 재구축, MVK 전체 source/binary closure 재현, live trace 재생, current runtime critical-path 측정, 새 후보의 구현·정확성 검증이다. 따라서 “Metal4 자체가 느리다”, “GLSL v2면 격차가 사라진다”, “32k decals는 전부 불필요하다”, “wait 삭제로 몇 FPS 오른다” 같은 결론은 내리지 않는다.

### 선택 파일의 감사 시점 SHA-256

전체 10,269파일 검증의 재수행을 뜻하지 않으며, 실제 읽은 핵심 입력의 결속용이다.

| 파일 | SHA-256 |
| --- | --- |
| R02 CANDIDATE.json | 7148133540c840299a29e911a511af877f35d384b6a7b2982bf406688f2fa1d7 |
| R02 SOURCE-CLOSURE-AFTER-BUILD.json | ee6b0191efbfbffe7266907f98bd082743d8dc59608177b30be1fe3e2297bccb |
| R02 NativeSensorLOSShaders.h | 1fa2cf0b4f8a9cd01f9a3b6c4a3886b72297aa6de1481cf31505945cfc995b1c |
| R02 Metal4RuntimeNative.mm | 9fe13f5e3e89e97043bd897010e705eafc8687a8d12ce7d3886514f67cc38521 |
| R02 api-calls.jsonl | 2448d5a7b54b0f5e7539c61af941111abe9f417c54907c13e86b99062a136928 |
| R02 shader-stage-intervals.jsonl | de5ffb5beca54b0642d944ff970c59f0486bf3ef30a98b8e7d26d7ef13448a12 |
| R02 shader-costs.jsonl | 9810810a955c021f5d8a295680e46a03c01f17c4daa62d59bcd33488c01c42cb |
| MVK saved sensor-los-FS.metal | e5e16a749928244b7a2df52c37fef6d4d91703604528b47013c57255e434ad8c |
| MVK saved sensor-los-VS.metal | 78130fd56c896dc5093f7cf9d603801ae4235f4aa309ca4f19dc5582ac3b7ded |
| R03 helper NativeSensorLOSShaders.h | 043e006fc06d8ff14f201abf5ef9413cc7987b1c70e789e3b0da971ffe9a6c7c |

권위 상태는 GitHub의 [STATE.json](https://github.com/yeojuny/BAR-Metal4/blob/coord/native-metal4/coordination/native-metal4/STATE.json)이며 이 링크의 내용은 이후 갱신될 수 있다. 본 문서의 판단 기준은 위에 명시한 revision31 및 고정된 R02/R03 증거다.

[I1]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/r02-g01-regression-20261116-01/CANDIDATE.json
[I2]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/r02-g01-regression-20261116-01/SOURCE-CLOSURE-AFTER-BUILD.json
[I3]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/UPSTREAM-CONTRIBUTIONS.json
[R1]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/r02-g01-regression-20261116-01/measurement/capture_r02_01/analysis/api-calls.jsonl:2
[R2]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/r02-g01-regression-20261116-01/measurement/capture_r02_01/analysis/shader-stage-intervals.jsonl:2
[R3]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/r02-g01-regression-20261116-01/measurement/capture_r02_01/analysis/shader-costs.jsonl:2
[R4]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/r02-g01-regression-20261116-01/execution/r02-validation-and-clean-01/SUMMARY.json
[R5]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/r02-g01-regression-20261116-01/measurement/capture_r02_01/publication/SUMMARY.json
[R6]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/r02-g01-regression-20261116-01/execution/r02-validation-and-clean-01/WORLD-CHECK.json
[R7]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/r03-minor-reject-20261117-01/helper-execution-01/RESULT.json
[S1]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/r02-g01-regression-20261116-01/source/engine/rts/Rendering/Metal4/NativeSensorLOSShaders.h:133
[S2]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/r02-g01-regression-20261116-01/source/engine/rts/Rendering/Env/Decals/NativeGroundDecalCapture.cpp:72
[S3]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/r02-g01-regression-20261116-01/source/engine/rts/Rendering/Metal4/Metal4GroundDecal.mm:160
[S4]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/r02-g01-regression-20261116-01/source/engine/rts/Rendering/Metal4/Metal4RuntimeNative.mm:1030
[S5]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/r02-g01-regression-20261116-01/source/engine/rts/Rendering/Backend/NativeSensor31657CirclePrograms.h:160
[S6]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/r02-g01-regression-20261116-01/source/engine/rts/Rendering/Env/Decals/GroundDecalHandler.cpp:782
[S7]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/r02-g01-regression-20261116-01/source/engine/rts/Rendering/Metal4/NativeGroundDecalShaders.h:197
[S8]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/r02-g01-regression-20261116-01/source/engine/rts/Rendering/Metal4/Metal4FrameResources.h:15
[S9]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/r02-g01-regression-20261116-01/source/engine/rts/Rendering/Metal4/Metal4PostprocessNative.mm:480
[S10]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/r02-g01-regression-20261116-01/source/engine/rts/Game/Game.cpp:1529
[S11]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/r03-minor-reject-20261117-01/helper-execution-01/headers/NativeSensorLOSShaders.h:172
[S12]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/r02-g01-regression-20261116-01/source/engine/rts/Rendering/Backend/NativeSensorLOSData.cpp:44
[S13]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/r02-g01-regression-20261116-01/source/engine/rts/Rendering/Metal4/Metal4WorldRenderer.mm:5063
[S14]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/r02-g01-regression-20261116-01/source/engine/rts/Rendering/Metal4/Metal4WorldRenderer.mm:5124
[S15]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/r02-g01-regression-20261116-01/source/engine/rts/Rendering/Metal4/Metal4SceneBridge.cpp:921
[S16]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/r02-g01-regression-20261116-01/source/engine/rts/Rendering/Metal4/Metal4WorldRenderer.mm:2507
[S17]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/r02-g01-regression-20261116-01/source/engine/rts/Rendering/Backend/NativeSourceFrameCredits.h
[S18]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/r02-g01-regression-20261116-01/source/engine/rts/Rendering/Backend/NativeSerialRenderWorker.h:14
[S19]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/r02-g01-regression-20261116-01/source/engine/rts/Rendering/Backend/NativeRenderJobSettlement.h:52
[S20]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/r02-g01-regression-20261116-01/source/engine/rts/Rendering/Metal4/Metal4PublishedRequestCache.h:32
[S21]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/r02-g01-regression-20261116-01/source/engine/rts/Rendering/Backend/NativePublishedBufferPool.h:40
[S22]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/r02-g01-regression-20261116-01/source/engine/rts/Rendering/Backend/NativeCPUPhaseTrace.h:14
[S23]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/r02-g01-regression-20261116-01/source/engine/rts/Rendering/Metal4/Metal4FrameOverlapProbe.inc:1
[M1]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/material-storage-pr7-20261115-01/ultra-compare/runs/ultra0_20261116_01/RESULT.json
[M2]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/material-storage-pr7-20261115-01/ultra-compare/mvk3-focus-prep01/analysis/sensor-candidate-api.jsonl
[M3]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/material-storage-pr7-20261115-01/ultra-compare/mvk3-focus-prep01/analysis/sensor-los-VS.metal:113
[M4]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/material-storage-pr7-20261115-01/ultra-compare/mvk3-focus-prep01/analysis/sensor-los-FS.metal:31
[M5]: /Users/yeojun/Desktop/BAR-AppleSilicon-App/build/run66-mesa-src-20260903/src/gallium/drivers/zink/zink_state.c:620
[M6]: /Users/yeojun/Desktop/BAR-AppleSilicon-App/build/run66-mesa-src-20260903/src/gallium/drivers/zink/zink_pipeline.c:284
[M7]: /Users/yeojun/Desktop/BAR-AppleSilicon-App/build/run66-moltenvk-src-20260903/MoltenVK/MoltenVK/Commands/MVKCommandEncoderState.mm:1986
[M8]: /Users/yeojun/Desktop/BAR-AppleSilicon-App/artifacts/validation/v104-r-small-bundle-20261021/independent-readiness-review.md
[P1]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/unexpected-load-scaling-audit-20261116-01/package/EVIDENCE-INDEX.md
[P2]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/ground-decal-index-g01-20261116-01/analysis-next/FS-PROOF-DOMAIN.json
[P3]: /Users/yeojun/Documents/Codex/2026-10-23/bar-rae-native-performance/evidence/unexpected-load-scaling-audit-20261116-01/package/evidence/ground-decal-hoist-g04-20261116-01/AUDIT-FREEZE.json

