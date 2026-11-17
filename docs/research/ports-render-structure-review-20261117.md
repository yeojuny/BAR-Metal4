# D2R·Zelda 렌더 구조: BAR R03 대응 심층 1차 검토

작성: 2026-11-17. 공개 소스 읽기 전용 조사. 게임·빌드·벤치마크 실행 없음. 사용자 Mac/HQ에 접근하거나 실행을 간섭하지 않았다.

## 1. 고정한 기준과 검토 범위

| 구분 | 저장소와 고정 commit | 실제 읽은 파일 |
|---|---|---|
| D2R | `d2r-mobile/d2r-mobile` @ `209dcd667fe937d23a153c88fd8fff97bdadc242` | `dm/gfx/nvn/nvn_mtl.m`, 6,603행 |
| Zelda | `ly05010419/MacNativeGames` @ `31aa41b4101a413f69113985120ba095e1c9586f` | `games/totk/third_party/d2r_gfx/gfx/nvn/nvn_mtl.m`, 25,470행 |
| BAR R03 | `yeojuny/BAR-Metal4` @ `0780bd7c530b0696067b717428b49a416eda556a` | `engine/rts/Rendering/Metal4/Metal4RuntimeNative.mm`, `Metal4WorldRenderer.mm` |

파일 전체를 확보한 뒤 제출 시작, 완료 처리, CPU 대기, 자원 회수, 업로드 관련 구간을 줄·블록별로 검토했다. **모든 32,000여 행의 전수 감사가 아니다.** 이 문서의 줄번호는 위 고정 commit 기준이다. earlier/main 링크와 검색 캐시는 줄번호가 다를 수 있으므로 재사용하지 않는다. BAR의 현재 Texture 후보가 아니라 R03을 대응 기준으로 삼았다.

### 영구 소스 링크

- D: https://github.com/d2r-mobile/d2r-mobile/blob/209dcd667fe937d23a153c88fd8fff97bdadc242/dm/gfx/nvn/nvn_mtl.m
- Z: https://github.com/ly05010419/MacNativeGames/blob/31aa41b4101a413f69113985120ba095e1c9586f/games/totk/third_party/d2r_gfx/gfx/nvn/nvn_mtl.m
- B-Runtime: https://github.com/yeojuny/BAR-Metal4/blob/0780bd7c530b0696067b717428b49a416eda556a/engine/rts/Rendering/Metal4/Metal4RuntimeNative.mm
- B-World: https://github.com/yeojuny/BAR-Metal4/blob/0780bd7c530b0696067b717428b49a416eda556a/engine/rts/Rendering/Metal4/Metal4WorldRenderer.mm

## 2. 결론부터: 가져올 것은 ARM 변환이 아니라 대기 위치와 자원 수명

Switch 기반 포팅의 CPU 쪽 장점은 기존 ARM64 프로그램을 실행한다는 점이다. BAR는 이미 소스에서 ARM64 빌드할 수 있으므로 NSO 재작성, OS 시스템콜 대체, Maxwell shader 번역기를 들여올 이유가 없다.

직접적인 참고점은 다음 세 가지다.

1. 완료된 제출 슬롯을 우선 재사용하고, 여유 슬롯이 있으면 이전 GPU의 종료를 기다리지 않는 방식.
2. GPU 작업 완료, CPU에서의 결과 관측, 참조 해제, residency 해제를 한 덩어리로 묶지 않는 방식.
3. 앞 패스의 fragment만 기다려도 되는 다음 패스의 vertex까지 멈추지 않도록 의존성을 구체화하는 방식.

그러나 D2R/Zelda 코드는 서로 독립된 두 성공 증거가 아니다. Zelda가 D2R 코드를 파생·확장했다. 또한 해당 소스에는 보수적 전체 barrier, 게임별 예외, 실험 기능이 공존한다. 통째로 가져오면 BAR의 정확성·성능을 보장하지 않는다.

## 3. 제출 시작: 슬롯이 있다는 것과 동시에 제출한다는 것

### D2R `mtl_sub_begin`, D 571–619

| 줄 | 실제 동작 | 의미·주의 |
|---|---|---|
| 572–577 | 초기화, 제출 serial 증가, 완료 자원 drain, deferred 상태 초기화, 준비된 shader 반영 | 이전 작업 전체의 완료를 무조건 기다리는 단계가 아니라 완료된 항목 수거를 먼저 한다. |
| 578–581 | 재진입 금지, 제출 thread 기록, 오류 초기화, autorelease pool 시작 | 제출 소유 thread가 명확하다. BAR worker와 main 소유권을 무시하고 이 코드만 이식하면 안 된다. |
| 582–586 | API에 따라 command buffer 생성 | Metal 3/4의 API 차이가 공통 실행 논리 아래 분기된다. |
| 591–592 | 현재 완료 fence를 읽고 각 allocator의 fence와 비교 | 슬롯 선택 기준은 순번 modulo가 아니라 실제 완료 여부다. |
| 593–599 | 완료 슬롯이 없으면 상한까지 allocator 추가 | GPU가 느린 순간 CPU가 바로 멈추는 것을 피한다. 메모리와 지연 증가의 대가가 있다. |
| 600 | 상한까지 모두 바쁘면 가장 오래된 fence를 기다림 | 무제한 큐가 아니다. 모든 frame마다 wait하는 대신 재사용이 불가능할 때 wait한다. |
| 601–605 | 선택 슬롯 기록, 안전하게 reset, command buffer 시작 | reset은 해당 allocator를 GPU가 다 쓴 뒤에만 한다. |
| 606–608 | 이번 제출의 callback·visibility·post-op 목록 초기화 | mutable state가 제출 단위로 분리되어야 한다. |
| 609–616 | visibility result buffer도 별도 fence ring으로 선택 | command allocator만 늘려서는 충분하지 않다. 결과 버퍼도 동시에 살아 있어야 한다. |
| 617–618 | pass timing과 arena 시작 | 추가 자원들의 수명도 같은 제출 번호로 묶인다. |

주의: 주석의 CPU 시간 변화 수치는 프로젝트 작성자의 과거 측정 설명이다. 본 조사에서 재현하지 않았고 BAR 이득 추정에 사용하지 않는다. `ALLOC_MAX=8`은 제출 슬롯의 상한이며 8프레임을 앞서 보내라는 권고가 아니다.

### Zelda `mtl_sub_begin`, Z 1707–1756

- **1708–1713:** autorelease pool을 기존 D2R보다 앞에서 연다. drain·warm 처리 전에 생성된 임시 Objective-C 객체가 장시간 살아남던 문제를 줄이려는 변경이다. 단순히 GPU 객체 retain만 보면 놓치는 CPU측 수명 문제다.
- **1715–1725:** pool input 초기화, keepalive, 이미 완료된 자원 수거, heap trimming, deferred 초기화, request 적용, 제출 thread 기록. 각 기능이 실제로 필요한지는 BAR 별도 측정 대상이다.
- **1731–1739:** D2R의 완료 슬롯 선택·상한 확장·최고령 fence 대기와 동일한 골격.
- **1741:** GPU read-fence 추적에서 submit thread를 exempt한다. 이 thread가 소비자가 기다리는 fence를 commit하므로 자기 자신을 기다리는 deadlock 방지 목적이다.
- **1743–1748:** 이전 제출의 uniform update 잔여를 금지하고 visibility overflow 버퍼를 초기화 전에 retire한다. 순서 자체가 중요하다.
- **1750–1752:** compute/copy-only 제출에는 visibility query buffer를 미리 확보하지 않는다. 사용하지 않는 ring 때문에 불필요하게 wait할 가능성을 줄인다.

### BAR 대응: B-Runtime 1038–1046, 1084–1090

BAR는 recording slot이 있어도 최종 제출 전에 `RetirePreviousSubmission`을 호출하고 pending owner와 pending queue가 모두 비었는지 검사한다. 소스 주석은 postprocess의 pending input 등 전체 borrow 수명 증명이 없기 때문에 두 번째 GPU ticket을 막는다고 명시한다. 따라서 현재 제한은 단순히 allocator가 하나여서 생긴 문제가 아니다.

**제안:** resource별 owner/version/read/write 목록을 완성한 다음, 제한된 자원 집합으로 두 번째 ticket을 허용하는 실험을 해야 한다. D2R의 슬롯 풀만 복사하거나 `Require`만 삭제하는 것은 잘못이다. BAR의 quarantine·GPU 실패·CPU main receipt 계약도 유지되어야 한다.

## 4. Commit와 완료: GPU fence, callback, CPU 결과는 구별해야 한다

### D2R D 857–908

| 줄 | 동작 | BAR 관점 |
|---|---|---|
| 858–861 | deferred draw fallback, render/blit 종료, 업로드 flush | 아직 제출되지 않은 생산자를 소비자보다 먼저 제출한다. |
| 862–868 | fence 증가, visibility ring stamp, post-op 표와 commit blocks를 복사 | callback이 전역 mutable 표가 아니라 해당 제출의 복사본을 소유한다. |
| 872–877 | callbacks 등록, GPU 시간 기록, GPU 오류 기록 | 완료 이벤트와 실패 여부를 같이 취급해야 한다. |
| 878–887 | semaphore·query·timestamp 결과를 CPU 메모리에 쓴 후 post-op 메모리 해제 | completion watermark가 CPU 결과보다 먼저 올라가면 안 된다. |
| 888–890 | 결과 반영 후 `g_done` 갱신, CPU waiters wake, deferred drain | done은 단순히 GPU event 관측 이상의 의미를 갖는다. |
| 892–901 | arena fence stamp, residency commit, buffer commit, queue event signal | GPU 명령 순서 및 residency 준비가 제출에 앞선다. |
| 904–908 | allocator fence 기록, 이번 buffer 소유권 해제, autorelease pool 닫기 | CPU는 fence를 반환받고 다음 준비로 갈 수 있다. |

### Zelda Z 3458–3550

Zelda는 같은 골격에 GPU error trace·read-fence 예외·resource sweeps 등을 더했다. **3465**의 completion thread read-wait 예외는 특히 중요하다. 완료 callback에서 GPU-reader wait를 걸면 그 callback이 올릴 완료 번호를 자신이 기다릴 수 있다. **3473–3495**의 CPU 결과 처리 뒤 **3517–3519**에서 completion bookkeeping을 끝내고 done을 올린다. **3521–3537**은 arena/residency 준비와 commit, queue event signal이다. **3538–3549**는 교체된 texture descriptor/view/cache 회수와 allocator stamp다.

다만 **3483–3487**에는 아직 shader/PSO가 준비되지 않아 draw가 빠진 경우 occlusion 결과를 visible로 올리는 게임별 타협이 있다. 이는 whole-game rendering equivalence의 증거가 아니며 BAR correctness 정책에 그대로 복사할 대상이 아니다.

### BAR B-Runtime 1063–1085, 1154–1197

- 1067–1072: callback은 error·timing을 저장하고 semaphore를 signal한다.
- 1084–1085: GPU에 실제 commit하기 전 quarantine owner를 먼저 게시한다.
- 1167–1170: drain에서 GPU shared-event와 feedback semaphore를 **둘 다** 기다린다.
- 1174–1182: GPU error를 검사하고 allocator reset, renderer retirement callback 순으로 처리한다.
- 1183–1191: residency 및 uniform receipt 완료와 pending owner 제거는 성공 경로 마지막에 수행한다.
- 1192–1196: 실패 시 owner를 보유하고 poisoned 상태로 남긴다.

**핵심 차이:** D2R는 callback 기반 완료 watermark와 deferred 자원 수거를 사용하고, BAR는 caller의 drain에 여러 retirement 행위를 모았다. 하지만 BAR의 caller/main-thread 계약을 확인하지 않고 callback으로 옮길 수는 없다. 분리 대상은 GPU 완료 인지, CPU 결과 전달, 안전한 자원 회수이며, 어느 thread에서 가능한지를 각자 증명해야 한다.

## 5. 자원 회수와 residency

### D2R D 232–259

232–238은 object·fence·keep-resident 플래그를 deferred 목록에 저장한다. 242–249는 완료 fence 이하 항목만 분리하고 lock을 해제한다. 250–258은 단순 참조 해제인지 진짜 GPU resource retirement인지 구분하여 후자에만 residency 제거를 수행한다.

**해석:** texture view cache의 참조를 버리는 일과 underlying texture가 더 이상 GPU에서 쓰이지 않는 일은 다르다. 두 사건을 같은 경로로 처리하면 살아 있는 texture의 residency를 제거할 수 있다.

### Zelda Z 678–728

- 678–689: 명시적인 retirement cutoff가 있으면 이를 사용하고, 없으면 다음 제출 fence를 보수적으로 사용한다.
- 698–704: 완료 watermark를 읽고 최소 대기 fence보다 작으면 전체 배열 스캔을 생략한다.
- 708–714: serial lock을 지금 얻지 못하면 release를 재큐잉한다. 완료 thread에서 lock 순서 교착을 피한다.
- 715–720: serial/storage가 여전히 object를 소유하면 residency 제거를 하지 않는다.
- 721–726: 재활용 spare 목록에서 제거한 뒤 실제 등록된 resource만 residency에서 뺀다.

**BAR 적용:** keepResidency와 frame-retained 목록만으로 충분한지, 교체된 view·pool·storage의 다른 소유자가 있는지 대조해야 한다. 상시 전체 resource census를 추가하기보다는 실제 retained/residency/pending 값이 어긋나는 조건을 특정하는 쪽이 우선이다.

## 6. 업로드: CPU 대기를 없애는 대신 의존성과 실패를 남긴다

### D2R D 1130–1223

1130–1167은 batch의 copy/compute 작업과 barrier를 인코딩한다. 1170–1181은 목적 texture를 callback이 추적하고 업로드 실패 시 재전송 필요 상태 및 cache generation을 갱신한다. 1185–1186에서만 slot busy/pending을 해제한다. 1188–1194는 residency 준비 뒤 queue에 제출한다. 1197–1199는 staging을 즉시 파괴하지 않고 deferred lifetime에 넣는다. 다음 batch는 1208–1210에서 3-slot 중 자기 슬롯이 아직 바쁘면 1ms polling으로 기다린다.

### Zelda Z 5126–5219

5164–5169에서 staging references와 pool 사용 mask를 해당 batch에 묶는다. 5177–5180은 실패한 목적 texture를 invalid로 만든다. 5184–5189는 완료 후 비풀 staging residency를 제거하고 풀 busy bit 및 slot 조건변수를 깨운다. 5207–5212는 D2R의 polling 대신 condition variable 대기와 대기 횟수·시간을 남긴다.

**BAR 대응 B-World 642–663:** cache hit 자체는 immutable storage를 교체하지 않지만, 첫 생산 제출의 완료 receipt가 아직 없으면 657–658에서 previous submission 전체를 retire한다. 이 위치가 현재 Texture ordered-input 후보의 직접 대상이다. 생산자가 같은 queue 앞에 있으며 texture allocation/내용/version이 고정되고 소비자 완료까지 owner가 살아 있다는 증명이 있다면 CPU 전체 drain 대신 순서로 충족할 여지가 있다. 그러나 producer failure 및 retry/cache invalidation을 없애서는 안 된다.

## 7. 패스 간 중첩: Zelda에도 아직 보수적 경로가 있다

Z 1772–1777은 encoder 양 끝에 full-stage barrier가 기본인 이유를 설명한다. 1778–1823은 OOT 전용 explicit-dependency 경로와 feature gating이다. 주석의 과거 'default off'만 읽으면 현재 동작을 오판한다. 1817에서는 허용 플랫폼에서 default-on이 설정되어 있고, 1806–1811의 `GAME_OOT` 조건 때문에 모든 Zelda 게임에 일괄 적용되지 않는다.

1788–1797은 fragment-only 의존성과 primitive/indirect 의존성을 구분한다. fragment-only이면 다음 vertex/binning까지 이전 fragment 종료를 기다릴 필요가 없다는 설계다. 반면 1845–1849의 TOTK narrow-barrier 설명은 vertex-stage hazard 검사가 없어 측정용이라고 경고한다. 'barrier 제거 시 빨라졌다'는 수치를 correctness가 증명된 개선으로 취급하면 안 된다.

**BAR 적용 우선순위:** producer resource → consumer resource뿐 아니라 consumer stage까지 연결해, vertex가 texture/indirect data를 읽는지 확인한다. 실제 긴 fragment 두 구간의 앞뒤 dependency와 MVK3가 사용한 barrier를 먼저 매핑한다. 다른 게임의 bitmask를 BAR shader에 붙이는 방식은 부적절하다.

## 8. BAR의 별도 drain 지점

| R03 위치 | 현재 조건 | 다음 분석 질문 |
|---|---|---|
| World 657–658 | first-use texture producer receipt 미완료 | 동일 queue ordered consumer로 가능한가? producer 실패, mip generation, GPU optimization 포함되는가? |
| World 2708–2711 | terrain height owner 또는 tile/map owner 변경 | version은 분리됐더라도 dependent cache 중 제자리 갱신되는 것이 있는가? |
| World 4493–4497 | dynamic water 준비 | history/feedback texture·reflection/refraction의 실제 read/write owner는 무엇인가? |
| World 4500–4506 | legacy water 준비 | bump/shading 결과와 다음 surface 준비 사이에 필요한 것은 CPU 관측인가 GPU 순서인가? |
| Runtime 1038–1046 | 모든 최종 deferred submit | postprocess 및 unregistered borrow까지 수명 증명이 완성됐는가? |

이 표는 병목 후보 목록이지 시간 기여도 순위가 아니다. 해당 장면에서 조건이 얼마나 자주 발생하고 wait가 실제로 block했는지 측정해야 한다. 잘못된 줄을 고쳐도 FPS가 안 바뀔 수 있다는 GroundDecal 결과를 반영해야 한다.

## 9. 실행 계획 제안 — MVK3/BAR 메인 작업에만 연결

- [x] 공개 프로젝트와 실행 방식 식별
- [x] D2R·Zelda·BAR R03 기준 commit 고정
- [x] 주요 submit/complete/wait/retire/upload 블록 대응
- [ ] MVK3 동등한 producer/consumer/barrier와 이 문서의 stage-level 대응 통합
- [ ] 실제 GPU/CPU timeline에서 조기 drain 기여도 확인
- [ ] Texture 후보의 동일조건 GPU 검증 결과와 비교
- [ ] 해당 결과에 따라 terrain/water 또는 final pending-ticket 해제 중 하나 선택

한 번에 모든 대기를 제거하지 않는다. 먼저 기존 Texture 후보 결과를 받고, 다음 변경은 발생 빈도와 대기 시간을 근거로 선택한다. shader AOT/prewarm, texture transcode, MetalFX/프레임 생성은 현재 지속적인 제출 병목 해결의 대체물이 아니다.

## 10. 검증 한계

실제 공개 구현을 읽었다는 것과 이 구현의 correctness/performance를 승인했다는 것은 다르다. 이 문서는 targeted static analysis이며 실제 D2R/Zelda 실행, shader 전수 분석, race 검증, BAR 성능 재현은 수행하지 않았다. 외부 프로젝트의 실패 처리와 global state에도 별도 감사가 필요하다. 코드 차용 시 각 파일의 라이선스와 third-party notices 확인이 선행되어야 하며, 현재 게임 데이터·키·shader를 가져오거나 배포하지 않았다.

## 후속 체크포인트: 포팅 프로젝트 선택과 검증 한계 (2026-11-17)

### 결정과 적용 순서
- **MVK3/BAR 비교가 메인**, 포팅 분석은 보조다. MVK3 실제 binary/driver source closure 미확보를 유지한다.
- **D2R Mobile = 원형 계약 이해 → D2R Metal = Apple GPU 최적화 우선 참조 → Zelda/MacNativeGames = 복잡한 자원·alias·반증 보조** 순서로 본다.
- BAR의 엔진 소스를 수정할 수 있으므로 ARM rewriter/Horizon/NVN 호환층 자체를 이식하지 않는다. submit, feedback, CPU 공개, retirement, cache 및 dependency 원리만 BAR 계약에 맞춰 적용한다.
- 이 선택은 분석 우선순위이며 새 코드 수정·실험 실행 지시가 아니다. HQ 실행권·STATE·진행 중 round를 바꾸지 않는다.

### 실제 재조회한 고정 버전
| 프로젝트 | main SHA | 용도 |
|---|---|---|
| D2R Mobile | `209dcd667fe937d23a153c88fd8fff97bdadc242` | 단순 원형, allocator/deferred retirement/upload |
| D2R Metal | `602db5fc30d4ce50683435cb0745cb171fce2064` | Apple 전용 개선·실패 사례·테스트 |
| MacNativeGames/Zelda | `80a5f47b9e508460c87f9dbcaa85d635ccbdbffb` | 복잡한 장면, alias/공유 저장소, 검증 방법 |
| OJZen/totk-mobile | `6d4c1d36c67c71fa303404c475c4c7ba4a7f2823` | 실험적 보조, 주 참조로 선택하지 않음 |

Zelda는 기존 줄별 분석의 `31aa41b4101a413f69113985120ba095e1c9586f`에서 2 commits 전진했다. GitHub compare에서 nvn_mtl.m도 +11/-1줄임을 확인했다. **기존 분석의 줄 번호는 기존 고정 SHA에만 적용하며 최신판 전수 재감사를 했다는 뜻이 아니다.**

### 성숙도와 숫자의 정확한 의미
- **D2R Mobile:** 개발팀 README는 44대 iPhone/iPad 다중 난이도 완주와 iPhone17 Pro Max 60 FPS를 보고한다. 우리가 실행·재측정한 결과는 아니다. 공개 unit/equivalence/regression tooling이 있다. v1.0.1 배포는 fresh-tree packaging 수정과 별개로 정확한 release shell 실기 실행은 미검증이라고 명시한다.
- **D2R Metal:** v2.1.0 (2026-11-12) 배포 및 Apple 특화 실험 기록이 있다. pass overlap 약0.54ms/8.5%, bind 기록57%/replay instructions13% 감소는 저자 장치·장면 한정이다. 대표 “60 FPS”는 **30 rendered + 30 generated**이며 BAR 순수 렌더60과 비교하지 않는다. 직접 측정 장치, GPU 부하 시뮬레이션, 타 장치 추정치가 README에서 구분된다.
- **MacNativeGames:** launcher prerelease 및 packaging tests는 GPU correctness 인증이 아니다. TOTK1.4.3의 M4 네 정지장면 frame p50 9.48–11.92ms/GPU7.64–9.89ms는 특정 날짜/소스/동적 해상도 조건의 기록이다. full-game/stable-FPS 인증이 아니며 장시간 메모리·first-use shader stalls·audio 문제는 열려 있다. iOS 미포함 shader draw 생략과 메모리 종료도 기록한다.
- **OJZen:** full playthrough 미검증, iPad 원해상도60 미달, iPhone15 Pro title까지만 검증. 조회 당시 releases 없음.

### 반드시 남길 실패 사례와 채택 금지
- D2R Metal `dm/gfx/nvn/nvn_mtl.m` L136–141: Mac separate present queue에서 GPU hang이 관측되어 Mac은 game queue로 복귀, iOS는 별도 queue 유지. 기록된 ABAB 조건은 separate5/40 대 shared0/40. shared에서도 다른 hang 사례가 있어 만능 해결이 아니다.
- 같은 파일 L3445–3453: 일부 blended effect PSO miss는 잠시 draw를 생략한다. BAR 동일화질 성능 비교에 그대로 넣지 않는다.
- 같은 파일 L732–747: 일부 NVN barrier counting-only 및 encoder 내부 draw 간 barrier 한계. 범용 안전성 입증으로 읽지 않는다.
- `dm/gfx/tests/t_bar.m`: 실제 Metal encoder와 counters를 검사하지만 draw는 발행하지 않으며 GPU race negative control이 성립하지 않아 race test를 하지 않았다고 명시한다.
- `dm/gfx/tests/t_gpu_lost.m`: GPU 상실 뒤 wait 반환과 g_done 전진을 검증한다. **GPU 완료 관측/탈출을 BAR 성공 receipt로 바꾸면 안 된다.**
- Zelda performance-followups는 copy batching counters0을 미커버로, submit budget 약0.46%를 미채택으로 기록한다. “먼저 제출”이 후속 gap을 키울 수 있다는 반례와 alias의 ResourceAlias 가시성 기록이 유용하다.

### 계보·라이선스·다음 확인
- D2R Metal과 Zelda 모두 D2R Mobile의 계보를 공유한다. 독립적인 성공 증거3개로 세지 않는다.
- 본체 표기는 D2R Mobile/Metal/MacNativeGames MIT, OJZen Apache-2.0 및 가져온 D2R 부분 MIT. 직접 코드 반영 전 파일별 고지·의존성 라이선스를 확인한다. 게임 자산 권리와 동일하지 않다.
- [x] 공개 README, release 기록, Metal 구현/테스트 및 Zelda 테스트·성능 문서 읽음.
- [ ] 실제 BAR 적용은 먼저 같은 출력·경로 hit·lifetime 증거를 세우고 별도 bounded HQ round로 판단.
- [ ] MVK3 실제 source/binary closure 확보 후 동일 대상과 대조.
- [ ] 포팅 측 저자 수치 독립 재현은 하지 않았으며 BAR 예상 FPS로 환산하지 않음.

### 고정 1차 출처
- [D2R Mobile README](https://github.com/d2r-mobile/d2r-mobile/blob/209dcd667fe937d23a153c88fd8fff97bdadc242/README.md), [release v1.0.1](https://github.com/d2r-mobile/d2r-mobile/releases/tag/v1.0.1)
- [D2R Metal README](https://github.com/PowerBeef/d2r-metal/blob/602db5fc30d4ce50683435cb0745cb171fce2064/README.md), [nvn_mtl.m](https://github.com/PowerBeef/d2r-metal/blob/602db5fc30d4ce50683435cb0745cb171fce2064/dm/gfx/nvn/nvn_mtl.m), [t_bar.m](https://github.com/PowerBeef/d2r-metal/blob/602db5fc30d4ce50683435cb0745cb171fce2064/dm/gfx/tests/t_bar.m), [t_gpu_lost.m](https://github.com/PowerBeef/d2r-metal/blob/602db5fc30d4ce50683435cb0745cb171fce2064/dm/gfx/tests/t_gpu_lost.m), [release v2.0.4](https://github.com/PowerBeef/d2r-metal/releases/tag/v2.0.4)
- [Zelda performance followups](https://github.com/ly05010419/MacNativeGames/blob/80a5f47b9e508460c87f9dbcaa85d635ccbdbffb/docs/performance-followups.md), [test records](https://github.com/ly05010419/MacNativeGames/blob/80a5f47b9e508460c87f9dbcaa85d635ccbdbffb/docs/test-records.md), [third-party notices](https://github.com/ly05010419/MacNativeGames/blob/80a5f47b9e508460c87f9dbcaa85d635ccbdbffb/THIRD_PARTY_NOTICES.md)
- [OJZen README](https://github.com/OJZen/totk-mobile/blob/6d4c1d36c67c71fa303404c475c4c7ba4a7f2823/README.md)
