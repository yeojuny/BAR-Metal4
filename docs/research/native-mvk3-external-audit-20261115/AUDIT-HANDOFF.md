# 외부 감사 요청: 현재185FPS까지 개선된 네이티브가 교전 부하에서 더 크게 뒤처지는 이유

작성일 2026-11-15, America/Los_Angeles. 원자료 일부는 UTC 2026-11-16 파일명을 사용한다.

## 1. 핵심 상황과 질문

**현재 네이티브는 All That Simmers 건설 장면에서 약185FPS까지 개선됐다.** 과거161FPS는 현재 빌드의 성능이 아니다. 그런데 실제 유닛 교전의 Ultra 조건에서는 AA를 꺼도 MVK3의 약절반이며, 단순 장면보다 상대 격차가 더 크게 보인다. 이 부하 의존성을 감사의 중심 질문으로 검토해 달라.

| 장면·조건 | MVK3 평균 FPS | 네이티브 평균 FPS | 비교 구분 |
|---|---:|---:|---|
| 현재 All That Simmers 건설 · AA0 | 226.02 (기존 참고값) | **184.96 (현재 빌드)** | MVK3는 같은 시점 재측정 안 함 |
| 이번 Seths 교전 · Ultra · AA0 | 93.87 | 47.62 | 같은 현재 조건의 직접 대조 |
| 이번 Seths 교전 · Ultra · 4× AA | 68.63 | 36.66 | 같은 현재 조건의 직접 대조 |


기존 MVK3 건설 기준은 약226~230FPS로 인식돼 왔고, 이번 자료에서 확인 가능한 기존 정확한 값은226.019353이다. 다만 현재 네이티브184.956933과 **같은 시점·같은 측정 구간으로 MVK3 건설을 재측정하지 않았다.** 그러므로 현재 건설의 약18~20% 참고 격차와 이번 교전의 약49% 직접 대조를 같은 수준의 통제 실험으로 제시하지 않는다. 과거에는 같은 조건의 MVK3 226.02/native220 161.23 기록도 있지만 이는 역사 자료로만 분리한다. [현재/과거 건설 기록](data/ALL-THAT-SIMMERS-REFERENCE.json).

이번 직접 비교의 네이티브/MVK3 비율은 AA0에서50.73%,4×에서53.42%다. 평균 FPS 역수로 환산한 간격은 AA0에서 MVK3 10.65ms/native 21.00ms, 차이약10.35ms이며4×의 차이는약12.71ms다. **이는 CPU 관찰 프레임 간격의 차이이지 GPU 실행 시간이나 제거 가능한 예산이 아니다.**

질문은 “왜 단순 건설 장면에서는 약185까지 개선됐는데, 유닛·교전 부하에서는 격차가 더 커지는가”다. 유닛 수에 비례하는 CPU 작업, GPU 모델/그림자/조명/후처리, 맵별 지형/셰이더 차이, 준비·제출·회수의 직렬화 중 무엇을 먼저 확인할지 근거와 순서를 제시해 달라. **장면·프리셋·시야와 비교 시점도 바뀌었으므로 유닛 수 하나의 인과 효과는 아직 분리되지 않았다.**

## 2. 직접 대조의 실제 조건

- 실제 로그 하드웨어: Apple M4 Pro, RAM24GiB,10P+4E, Darwin27.3.0 ARM64. GPU 코어 수·연속 클록/온도는 별도 측정하지 않았다.
- 같은 현재 콘텐츠 `Beyond All Reason test-31691-a617033-local26`, 맵 **Seths Ravine Remake1.3.1**. 정확한 SDP·맵·엔진·라이브러리 해시는 [SOURCE-IDENTITY.json](data/SOURCE-IDENTITY.json). 로컬 콘텐츠이므로 순정 최신 BAR와 완전히 동일하다고 주장하지 않는다.
- 공식 명령 `luarules benchmark armbull armbull 80 8 1400`. 팀당80 기준,8대씩 보충하는 실제 전투. 외부 AI는 없다. 게임 시드154658555, 벤치 시드7654321.
- `spectator=1`·전체 시야, 고정 카메라. 실제 초기 상태는 **지휘관2개**, 측정 시작 로그의 `visible_units=168`은 지휘관 등을 포함한 관측값이다. 전차만 정확히168대라는 뜻은 아니다.
- 전투 시작 sim240, 전투 예열10시뮬초(sim240→540),40초 측정(sim540→1740). 모든8회가 생성600·파괴433이다. 이는 **예열 포함 전투50시뮬초** 집계이며40초만의 수치가 아니다.8회의 초기 상태·카메라도 같았다. 개별 공격 대상·사망 프레임 전수 비교는 하지 않았다.
- 전체 화면 **실제 게임 영역1512×950**, 현재 콘텐츠의 **Ultra=index4**. 옛 index5는 현재Custom이므로 최종 성공 시험에 사용하지 않았다. `graphicsPreset=ultra`와 선택index4를 런타임에서 확인했다.
- AC 일반 모드 조건, VSync/VSyncGame0, 비활성·화면 밖 FPS 감소OFF, MinSampleShadingRate0. 전원 필드 확인은 GPU 클록 고정 시험과 다르다.
- AA4×/AA0× 각각 **MVK3→native→native→MVK3**, 각2회. 매회 엔진을 완전히 종료하고 같은 초기 프로필에서 시작. AA4× 묶음 뒤 AA0× 묶음을 진행했으므로 두 AA 묶음 사이 장시간 환경 변화까지 완전 분리한 설계는 아니다.
- 최종 FPS 수집은 frame trace/Metal Validation OFF. 에이전트 로컬 작업 중단 프로토콜을 적용했다. OS 전체 부하·열 상태까지 완전 통제한 것은 아니다.

사용자는 간단한 직접 FPS 비교를 요청해, 세부 그래픽 항목 하나하나의 완전 일치 검사는 하지 않았다. **같은 Ultra 선택과 핵심 AA·해상도·시야를 확인했지만 모든 효과의 실제 실행·픽셀 동등성은 입증하지 않았다.** 옵션값이나 제한된 관찰기의 `widget_*` 값 하나만으로 효과가 실제로 켜졌거나 꺼졌다고 단정하지 말고, 관찰 API와 실제 렌더 경로를 먼저 확인해 달라.

## 3. 실행별 수치와 FPS의 의미

| AA | 순서 | 엔진 | 평균 FPS | 1% low FPS | 측정 시간 |
|---|---|---|---:|---:|---:|
| 4x | 1 | MVK3 | 68.373292 | 26.871 | 40.000999s |
| 4x | 2 | native | 36.661253 | 30.928 | 40.014999s |
| 4x | 3 | native | 36.663748 | 30.303 | 39.985001s |
| 4x | 4 | MVK3 | 68.892220 | 38.567 | 39.990002s |
| 0x | 1 | MVK3 | 94.275000 | 51.075 | 40.000000s |
| 0x | 2 | native | 47.619046 | 39.370 | 40.005001s |
| 0x | 3 | native | 47.625000 | 40.984 | 40.000000s |
| 0x | 4 | MVK3 | 93.458645 | 52.925 | 40.007000s |


평균 FPS는 GameFrame 경계 draw counter 차이/벽시계 시간이다. **CPU 관찰 DrawScreen 진행률로, 실제 화면 present 또는 GPU 완료율이 아니다.** 1%low는 raw 간격 중 가장 느린 ceil(1%) 평균의 역수다. 평균 FPS가 주 판단 기준이며 모든 유효한 느린 실행을 포함했다. 각조건2회이므로 모집단 동등성·무회귀나 원인까지 증명하지 않는다.

[data/RUNS.csv](data/RUNS.csv),[8개 원본 간격 CSV](data/frames),[런타임 증거](data/RUN-EVIDENCE.json),[원본 줄 번호가 있는 로그 발췌](data/LOG-EXCERPTS.txt)를 제공한다. 경계 때문에 per-draw CSV에는 draw counter보다 한 간격 적게 들어갈 수 있다. 평균은 start/end draw/time 앵커로,1%low는 raw CSV로 재계산해야 한다. [산술 확인](data/ARITHMETIC-CHECKS.json)에서8개의1%low를 원본으로 재계산했다.

AA를 끄면 MVK3는약36.8%, native는약29.9% 증가했지만 큰 격차는 남는다. AA만의 문제로 설명할 수 없으며, AA 변경 효과를 순수 샘플 수 비용으로 해석해서도 안 된다. 아래처럼 native의 내부 경로도 자동 변경된다.

## 4. 감사할 소스·실행 파일과 내부 경로

네이티브는 [PR7](https://github.com/yeojuny/BAR-Metal4/pull/7)의 **전체 고정 커밋 [a3c7c6bfa93d09dac25755534195932d12fff41a](https://github.com/yeojuny/BAR-Metal4/tree/a3c7c6bfa93d09dac25755534195932d12fff41a)**. 이번 Mac 컴파일 수정0개, RelWithDebInfo 전체 빌드1회다. archive1538objects/35archives 대응0불일치가 기록됐지만 그 숫자만으로 모든 소스 의미를 증명하는 것은 아니다. 이번 후보에는 “GitHub에 안 올라간 별도 Mac 컴파일 수정”이 없다.

- 네이티브 signed binary SHA256 `b25a08287679847a420c37eaf3c201c9c95ad696e90d1b1eaa18e0cb08060942`.
- MVK3 binary SHA256 `d715965659b977e05b8ccafe76fae0936cade1a7481871acff7eec876366f1aa`, 보존 Small v104 번들/버전표기2026.09.01-24-g01efdf4 master. GL→Mesa/Zink→Vulkan→MoltenVK Metal3 경로. 실제 로드된4개 라이브러리 해시를 제공한다. **이 보존 번들의 전체 소스·로컬패치·컴파일 옵션을 이번 인계에서 완전히 역매핑한 것은 아니다.** 두 엔진의 컴파일 옵션이 같다고 가정하지 말아 달라.
- 이번 직접 비교에서 native **STORAGE_POOL=0**,D/Q/U도OFF,SepiaOFF다. PR7 코드가 포함된 새 빌드이지만 **풀을 켜서 생긴 성능 저하를 측정한 것이 아니다.** 기본 배포앱과도 구분해야 한다.
- worker1, PREPARE1/BEGIN0, FINAL_COLOR1, MODEL_TYPE_FACTS1, SCREEN_STATE_ELISION1과 나머지 실제 child 플래그는 런타임 증거 JSON에 있다.

AA4에서는 MVK3 EGL/GL4, native actual main sample4. AA0에서는 MVK3 GL0/samplebuffers0, native single sample1이다. native의 GL호환 조회0만 보고4×가 꺼졌다고 판정하지 말아 달라.

native `NativeFinalColorStage`는 설정값1이어도 **AA4에서active0, AA0에서active1**로 기록된다. 최종 합성/복사 경로도 AA에 따라 바뀌므로 이득을 순수MSAA 비용으로 귀속하지 않는다. 실제 게임 viewport는양쪽1512×950, MVK3 EGL swapchain/drawable와 native main/final drawable는1512×982로 기록됐다. native MRT는1512×950다. MVK3의 모든 별도 main/MRT 크기까지 계측한 것은 아니다. 내부 attachment 수·포맷·store/load·resolve·복사 비용이 같다는 전제는 없다.

우선 읽을 파일(모두 위 고정 커밋; 줄 번호는 해당 커밋에서 확인):

- [rts/Game/Game.cpp](https://github.com/yeojuny/BAR-Metal4/blob/a3c7c6bfa93d09dac25755534195932d12fff41a/engine/rts/Game/Game.cpp)
- [rts/Rendering/Metal4/Metal4RuntimeNative.mm](https://github.com/yeojuny/BAR-Metal4/blob/a3c7c6bfa93d09dac25755534195932d12fff41a/engine/rts/Rendering/Metal4/Metal4RuntimeNative.mm)
- [rts/Rendering/Metal4/Metal4WorldRenderer.mm](https://github.com/yeojuny/BAR-Metal4/blob/a3c7c6bfa93d09dac25755534195932d12fff41a/engine/rts/Rendering/Metal4/Metal4WorldRenderer.mm)
- [rts/Rendering/Metal4/Metal4CUSSource.mm](https://github.com/yeojuny/BAR-Metal4/blob/a3c7c6bfa93d09dac25755534195932d12fff41a/engine/rts/Rendering/Metal4/Metal4CUSSource.mm)
- [rts/Rendering/Metal4/Metal4CUSDeferred.mm](https://github.com/yeojuny/BAR-Metal4/blob/a3c7c6bfa93d09dac25755534195932d12fff41a/engine/rts/Rendering/Metal4/Metal4CUSDeferred.mm)
- [rts/Rendering/Metal4/Metal4ScreenNative.mm](https://github.com/yeojuny/BAR-Metal4/blob/a3c7c6bfa93d09dac25755534195932d12fff41a/engine/rts/Rendering/Metal4/Metal4ScreenNative.mm)
- [rts/Rendering/Metal4/Metal4TerrainSourceGPU.mm](https://github.com/yeojuny/BAR-Metal4/blob/a3c7c6bfa93d09dac25755534195932d12fff41a/engine/rts/Rendering/Metal4/Metal4TerrainSourceGPU.mm)
- [rts/Rendering/Metal4/Metal4ModelMaterial156.h](https://github.com/yeojuny/BAR-Metal4/blob/a3c7c6bfa93d09dac25755534195932d12fff41a/engine/rts/Rendering/Metal4/Metal4ModelMaterial156.h)
- [rts/Rendering/Backend/NativeRetiredBufferStorage.h](https://github.com/yeojuny/BAR-Metal4/blob/a3c7c6bfa93d09dac25755534195932d12fff41a/engine/rts/Rendering/Backend/NativeRetiredBufferStorage.h)
- [rts/Rendering/Metal4/Metal4SceneBridge.cpp](https://github.com/yeojuny/BAR-Metal4/blob/a3c7c6bfa93d09dac25755534195932d12fff41a/engine/rts/Rendering/Metal4/Metal4SceneBridge.cpp)

기존 [기여·라이선스 기록](data/UPSTREAM-CONTRIBUTIONS.json)을 보존했다. 그 기록의 과거/미채택 항목을 이번 바이너리에 전부 적용됐다고 취급하지 말고 원래 저작권·기여자를 유지해 달라.

## 5. 요청하는 감사 순서와 결과

1. **비교의 실질 동등성부터 확인:** 같은 Ultra 요청·카메라·시야·보이는 부대가 실제로 어떤 효과와 pass를 만드는가? 한쪽의 누락/추가 효과, 내부표면·final stage·shader specialization 차이가 있는가?
2. **185FPS 건설 장면과 큰 교전 격차의 부하 의존성:** transform/snapshot/packing, 재질·argument 바인딩, shadow/reflection/main 중복 준비, Lua·체력바 재생 등이 유닛 수에 따라 증가하는가? 아니면 GPU 모델·그림자·조명·overdraw가 큰가? 맵/프리셋 차이와 단위 수를 섞지 말고, 현재 같은 바이너리로 이를 가를 최소 다음 실험을 제안해 달라.
3. **상위 프레임 계약에서 실제 대기 확인:** 이전 GPU event/feedback, Join, prepare, encode, commit, drawable acquire/present 중 무엇이 다음 제출을 늦추는가? 과거 조언의 “in-flight1이면 매 프레임 완전 대기”, “G-buffer를 항상 새로 만들고 전부 복사”를 본 후보의 확정 사실로 재사용하지 말고 이미 있는 회수·재사용 최적화와 활성 조건을 확인해 달라.
4. **GPU 실제 반복 작업 확인:** render pass 재개·store/load, 넓은 배리어, MRT 버전 복사, depth/stencil snapshot, AA별resolve/final copy, `sample_id`에 따른 per-sample shading 중 무엇이 활성인가? CPU draw/상태·바인딩 반복도 같은 순서에서 비교해 달라.
5. **셰이더·드라이버 차이를 근거로 비교:** math mode/fast-math, batching/instancing, GS 대체, threaded submit, 컴파일 최적화는 후보 가설이다. 실제 소스/플래그/활성 경로로 확인하고 “Metal4 API라 느리다” 같은 결론부터 두지 말아 달라.

이번 Ultra 시험에는 Time Profiler/Metal System Trace/동일시계 CPU-GPU timeline이 없다. FPS만으로 CPU/GPU 병목이나 약10.35ms 차이 중 제거 가능한 양을 알 수 없다. **정적 사실 / 성능 가설 / 필요한 최소 측정**을 분리해 달라. 새 계측 프레임워크·대규모 재작성 대신, 가장 큰 활성 경로 하나를 가를 관측부터 제안해 달라.

발견마다 **파일·라인, 현재 활성 여부, 메커니즘, 증거, 필요한 측정, 순서·GPU 수명 증명, 수정 위험, 우선순위** 표를 원한다. 일을 두 번 하지 않도록 상위 계약 또는 독립 비용부터 순서를 잡아 달라. 원본 BAR의 화면·효과·게임 의미와 GPU reader 완료 전 재사용 금지는 지켜야 한다. retain/pin/검증을 단순 삭제하는 해법은 받지 않는다. FPS가 그대로여도 실제 작업·할당·복사·RAM 감소는 별도 성과로 기록할 수 있다.

## 6. PR7 풀 실험은 별도 참고 자료

직접 비교와 혼합하지 말아 달라. native만의 기존 AA0 프로필·플레이어 시야에서 풀만OFF/ON으로 바꾼8개 clean run이다.

- 건설184.956933→184.837492FPS(−0.06458%), 전투55.259042→55.335425FPS(+0.13823%). FPS 향상은 확인되지 않았다.
- 건설 진단16Gets/8hits/8encodes당 새 Metal buffer8→0,ON은8reuse. 전투OFF16encodes/16new,ON24encodes/0new/24reuse로 원합계 작업량은 다르다. 이 수치는 **표본 구간의 `newBufferWithLength` 호출**이며, 실행 전체 새 할당0 또는 OS 물리페이지 할당 수를 뜻하지 않는다.
- 소비자별 풀 보유량 최대는25,920B/25,344B. cache/draw ownership과 겹치고 spill을 제외하므로 앱RAM/GPU 메모리에 더하면 안 된다.
- 앱RSS 관측 피크 평균(MiB)은 건설5369.30→5167.03,전투5681.08→5025.15. 측정구간 footprint는9555.59→9654.91,10054.17→10018.33.5초 간격 관측이며 footprint의 일관된 감소가 없다. 수백MiB RSS 차이를25KiB 풀의 인과 효과로 단정하지 않는다.
- Metal API Validation 오류0/새 화면 이상 미관측. OFF resize 확인 일부 실패·ON reload 후 후처리 정정을 원본과 함께 보존했다. 실제 GPU 지연 fault injection은 하지 않았다.

[별도 결과](data/PR7-SEPARATE-RESULT.json),[건설](data/PR7-CONSTRUCTION-DIAGNOSTIC.json)/[전투](data/PR7-BATTLE-DIAGNOSTIC.json) 집계를 제공한다. 풀은 기본OFF이며 단순 풀 증설/같은 캐시 추가를 다음 큰 해법으로 먼저 제시하지 말아 달라.

## 7. 증거 범위와 한계

이 묶음은 결과표,8개 raw frame CSV,설정·child·backend 묶음,줄 번호가 있는 원로그 발췌,AA별·엔진별 대표 화면4개만 선별했다. CSV/PNG는 바이트 그대로이고 JSON/로그의 개인 절대경로만 정리했다. 원본파일SHA와 패키지SHA는 MANIFEST 및 ORIGINAL-SOURCE-REFERENCES에 있다. 게임·엔진 바이너리·전체 콘텐츠·수십 프로필은 포함하지 않는다.

잘못된 작은 창·누락 폰트·옛preset5·f240준비검사 때문에 전투가 시작되지 않은 준비 실행은 최종8회에 섞지 않았다. 유효4×A1은 옛parser가spectator0을 기대한 후처리 오류만,0×A1은MSAALevel−1→실제0 정규화 후처리 오류만 고쳐 같은 raw를 회수했다. 불리한1%low도 포함했고 좋은 값이 나올 때까지 재실행하지 않았다. 실패 원본은 로컬에 남아 있다.

시드가 같아도 모든 사건의 동일성을 이 자료가 증명한 것은 아니다. 현재 확인한 범위는8회의 같은 초기 상태·카메라·시야·집계600/433이다. AA 묶음 간에는 시간 순서·캐시·열 상태가 완전 통제되지 않았고 세부 옵션/픽셀 동등성도 미입증이다.

이 문서는 **외부 감사용 인계**이며, 작성 중 새 게임·벤치·빌드·프로파일을 실행하지 않았다. main·릴리스·Homebrew도 바꾸지 않았다. 실제 코드 변경/실행 제안은 감사 결과로 제출해 달라.
