# D2R·Zelda 렌더 구조: BAR R03 대응 심층 1차 검토

> 최신 기록: round25 캡처 경계 질의 시간초과. 세션 상태 미확인, 서비스 재시작 사용자 승인 대기. 마지막 절 참조.

> 최신 운영 상태는 문서 끝의 **round18 종료·제한적 동등성 통과** 갱신을 참조한다. revision72/RUNNING 내용은 과거 스냅샷이다.

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

## 최신 상태 갱신: round18 종료·제한적 동등성 통과 (2026-11-17)

이 절은 위 revision72/RUNNING 스냅샷을 **현재 상태에 한해 대체**한다. 이전 판단과 기록은 역사 보존이며 새 실행 허가가 아니다.

- 실제 재조회: `coord/native-metal4`, STATE **revision74**, status **DONE**, owner **SUPER_JINI_MK2**. STATE blob `8fab9dac48488c966574deefc4cf58cfad72a7be`.
- round `completion-observation-split-20261117-18`; source `c34129af9158bbdef1321d17825f1f60721b74ec`; artifact `8a26fda1d784c57bdcc242d7a83f0899cafb93cf`.
- 결정: **ACCEPT_BOUNDED_STRUCTURAL_EQUIVALENCE_GATE_NOT_FULL_LIFETIME_PROOF**. HQ turn 종료와 cleanup 검증이 DECISION에 기록됐다.
- CPU receipt9/model36 PASS, production build exit0. 모델 테스트는 live Metal fault injection이 아니다.
- 실제 child Metal API validation1, texture input/trace0, 기록된 Metal 오류0, 정상 종료 및 owned group 종료 확인.
- readback: raw H1NativeImage success=true, simulation1740. 후보와 기존 R03 화면을 직접 비교한 결정 기록: 뚜렷한 새 정적 지형/건물 손상 없음. 동적 연기·조명·발사체 차이 있음. pixel/depth/stencil 동등성은 주장하지 않는다.
- world 각40행 중 **38행 완전 동일**, shared end ticks39에서 same-tick state differences0. 첫 bin은 R03 end577/units171, 후보 end570/units172이며 다음 행 start에도 영향. wall-time 표본 경계 차이이므로 **40행 전부 일치라고 쓰지 않는다**.
- 검증된 범위: pending1을 유지한 추출의 실제 엔진 compile 및 ordinary scene/readback/shutdown 한 회.
- 미검증: **early drain live coverage, detach, live fault injection, exact callback counts, 전체 lifetime/context/error 경로**.
- receipt는 event+feedback 관측을 기록할 뿐 GPU 성공, renderer retirement, readback readiness, presentation과 동의어가 아니다.
- **FPS 개선, pending2 안전성, 새 실행 권한은 없음.** 무조건 재실행/승격 금지. main·release·Homebrew 보호 유지.

### 다음 한 단계와 완료 체크
- [x] STATE revision74 및 round18 DECISION 원격 읽음.
- [x] 제한적 동등성 통과와 미검증 경계를 분리해 보존.
- [ ] scheduling/release boundary 변경 전 정확한 resource ownership 및 실패/quarantine 계약을 소스로 증명.
- [ ] 필요한 경우 별도 고정 source·manifest·새 round·제한 예산으로 gate 정의. 알림이나 이 문서만으로 실행하지 않기.
- [ ] 이어받는 시점에는 최신 STATE를 다시 읽어 중복 실행·소유권 충돌 방지.

근거:
- [revision74 STATE 고정본](https://github.com/yeojuny/BAR-Metal4/blob/852d42ce4dd31f85c99102aea8817513f922d1e1/coordination/native-metal4/STATE.json)
- [round18 DECISION 고정본](https://github.com/yeojuny/BAR-Metal4/blob/852d42ce4dd31f85c99102aea8817513f922d1e1/coordination/native-metal4/rounds/completion-observation-split-20261117-18/DECISION.json)

## 심층 후속: D2R Metal의 실제 렌더 최적화만 검토 (2026-11-17)

### 사용자 범위와 고정 입력
- **Frame generation과 FSR은 이번 분석/실험 범위에서 제외한다.** 업스케일·동적 해상도·효과 생략으로 숫자를 높이는 방식도 동등 출력 기준 성능 후보로 채택하지 않는다. FSR은 후순위로 보존하되 현재 구현하지 않는다.
- 메인은 MVK3/BAR의 원래 렌더 비용·제출·대기·중복 작업이다. 아래는 코드 읽기 기반 후보이며 HQ 실행 허가가 아니다.
- D2R Metal `602db5fc30d4ce50683435cb0745cb171fce2064`; BAR R03 `0780bd7c530b0696067b717428b49a416eda556a`.
- D2R 파일: `dm/gfx/nvn/hle_nvn.c`(H), `dm/gfx/nvn/nvn_mtl.m`(M).
- BAR 파일: `engine/rts/Rendering/Metal4/Metal4WorldRenderer.mm`(W), `Metal4RuntimeNative.mm`(R).
- 숫자로 된 BAR FPS 예상은 제시하지 않는다. D2R 저자 측정은 다른 게임·장면이며 전이 가능한 이득이 아니다.

### P1. 중복 상태를 기록하기 전 제거하고 실제 encoder state도 재사용
> 정정: R03 Screen UI의 7개 state elision은 이미 ON이다. 새 후보는 별도 typed model raster이며 actual battle 빈도 미측정. 문서 끝 정정 절 참조.
**블록별 실제 동작**
1. H504–526: command buffer별 shadow에 blend/color/channel mask/MSAA/polygon/depth-stencil/program/vertex layout의 마지막 값을 기록한다. 전역 마지막 값이 아니다.
2. H527–539: valid bit가 있고 bytes가 동일할 때만 생략한다. BeginRecording과 CopyCommands 뒤에는 invalidate한다. OFF→ON에서도 shadow를 버리므로 중간에 추적하지 않은 상태를 재사용하지 않는다.
3. H541–574: uniform/vertex buffer와 sampler는 slot별 identity를 비교한다. mode2는 생략하지 않고 would-skip 기록을 남겨 replay 시 현재 slot과 비교한다. texture는 replay가 바꾸므로 이 recorder filter에서 **제외**한다.
4. M4294–4333: recording 검사만 믿지 않고 replay에서 index/stage 범위 검사를 다시 한다. 값이 달라질 때만 pso/dss/args dirty를 올리고 texture/SSBO generation도 갱신한다.
5. M5998–6013: PSO lookup은 program/pass/dirty 조건 변화 때만; 실제 encoder의 PSO·depth-state가 다를 때만 set 호출한다. fixed state도 valid bit+value 비교로 처리한다.

**BAR 대응**
- W5332–5348의 legacy model loop는 piece마다 pipeline/argument texture/address/setArgumentTable을 수행한다. adjacent piece의 pipeline/material이 같은 경우 원리는 적용 가능하지만 해당 legacy 경로가 실제 counted workload에서 얼마나 쓰이는지는 미확인이다.
- CPU encoding/command memory/lookup 절약 후보이지 GPU shader 연산 감소는 아니다. GPU-bound라면 FPS 효과는 작거나0일 수 있다.
- 초기 안전 단위: 같은 encoder·같은 family에서 PSO/depth/cull/winding 등 불변 state만. encoder 재생성, 외부 family callback, Lua command, pass 변환 뒤 반드시 invalidation.
- texture ID/포인터 동일만으로 내용이나 residency 계약까지 건너뛰지 않는다. argument table mutation과 setArgumentTable의 API 관측 의미를 별도 확인한다. snapshot/dirty publication은 계속 수행한다.
- **첫 gate:** OFF / would-skip verify / ON. skipped bind 종류·수, family별 CPU encode시간, command bytes, lookup hit/miss를 함께 기록. 상태 trace mismatch0 및 Metal/화면 검증 뒤에만 clean timing. 매 draw 로그는 금지하고 진단/성능 창 분리.
- D2R README의 bind records57%/replay instructions13% 감소는 참고 맥락일 뿐 BAR 예상치가 아니다.

### P2. generation·수명 경계를 포함한 준비 결과/argument block 재사용
**블록별 실제 동작**
1. M3862–3871: resource registration generation이나 dirty epoch가 변하면 argument cache 전체를 invalidate한다. 바인딩 목록만 같아도 외부 registry 변화가 있으면 다시 만든다.
2. M3918–3946: shader driver table은 매번0xA00 bytes를 채우던 작업을 stage/bank별로 관리한다. bank2는 texture/sampler tables, bank0는 storage buffer table이므로 key가 의미 있는 데이터에 결합된다.
3. M3948–3959: 재사용 조건은 **동일 submission serial + source table generation + 유효 arena주소**. mode2는 fresh bytes를 다시 만들어 cached bytes와 비교한다. arena가 submission GPU 완료까지 살아 있어야 한다.
4. M3961–3968: shader argument block은 동일 pass serial·dirty 없음·buffer 있음일 때 재사용한다. compute stage는 이 경로에서 제외한다.
5. M4655–4704: shader별 read-slot 목록을 사전 계산하고 동일 texture bind는 registry generation까지 같을 때 생략한다. mode2는 reference scan과 결과 전체를 비교한다.

**BAR 대응**
- 준비/encoding에서 같은 metadata를 다시 계산하는 곳만 선택한다. D2R의 NVN shader driver table 자체는 BAR에 없는 번역 비용이므로 그대로 이식할 대상이 아니다.
- BAR W642–663의 immutable texture storage/borrow, R1038–1046의 pending1 및 R1154–1197의 quarantine 계약을 유지한다. cache hit가 producer 완료나 consumer GPU 완료를 의미하지 않는다.
- 성립 조건: immutable payload 또는 완전한 revision key, source identity+generation+view/range+shader/pass/slot lifetime, 모든 mutation 경로의 invalidation. 누락된 generation은 stale GPU pointer/오래된 constant를 만든다.
- 기대 효과: CPU 준비·할당·복사 byte 감소. 캐시 hit가 counted battle window에서 실제 발생하고 절약 비용이 key 비교비용보다 클 때만 이득.
- gate: reference 재생성과 byte/semantic 비교, hash collision에 의존하지 않는 equality, allocate/reuse/invalidated 원인·bytes, hit frame 범위를 기록. 초기 loading hit만 있으면 성능 후보 미커버로 판정.

### P3. render pass 간 fragment→다음 vertex 불필요 대기만 축소
**블록별 실제 동작**
1. M770–787: encoder 종류를 class별 원자적 cache로 분류하고, render이며 안전한 경우만 end barrier의 다음 vertex/object/mesh stages를 제외한다. compute/blit와 memory writer는 full publication을 유지한다.
2. M804–812: level0는 full consumer barrier. level1은 consumer vertex를 열어 앞 pass fragment와 뒤 vertex/binning 겹침을 허용한다. 게임 barrier 미처리 항목은 카운트되며 이것이 완전한 의존성 구현이라는 뜻은 아니다.
3. M818–824: pass가 shader memory write를 했으면 모든 다음 stage를 막는 producer barrier를 유지한다. 단순 render와 write render를 구분한다.
4. M5750–5759: vertex texture read 또는 memory write가 있는 draw에는 늦은 vertex barrier를 추가한다. 다른 shader의 memory write도 pass 종료조건에 반영한다.
5. 한계: M726/732–747의 encoder 내부 draw 간 barrier 미구현 및 NVN 일부 counting-only. t_bar.m은 actual encoder/counter test이나 draw0이며 race negative control을 제공하지 않는다.

**BAR 대응과 중복 도입 방지**
- W4974–5005에는 empty Load/Store pass 제거가 **이미 있다**.
- W5387에는 scoped pair220의 fragment-only consumer, W5400–5410에는 snapshot visibility 및 미래 encoder용 producer/consumer publication이 **이미 있다**. BAR가 무조건 전부 serial barrier라는 전제는 틀리다.
- 후보는 남은 특정 boundary의 실제 producer/consumer resource access를 증명한 경우뿐. 기존220 경계를 넓히거나 full barrier를 일괄 삭제하지 않는다.
- vertex texture/SSBO reads, UAV writes, alias, indirect/index/uniform access, attachment load/store, snapshot copy와 외부 callback까지 hazard map에 포함한다. Device visibility가 alias를 모두 해결한다고 가정하지 않는다.
- 기대 효과: 실제 dependency가 없는 stage overlap의 GPU idle 감소. D2R 저자 약0.54ms/8.5%는 별도 workload 수치이며 BAR 미예측.
- gate: 위험 boundary 하나만, resource/subresource/access graph와 producer stage 증명 → 정/부정 대조를 포함한 correctness fixture → 실제 scene Metal/화면/depth/stencil 비교 → GPU critical path/idle+frame time. pass 시간은 서로 겹치므로 합산 금지.

### P4. 제출 allocator pool과 대기 분리: 큰 구조 후보이나 지금은 보류
- M647–678: completed fence 슬롯 선택, 없으면 max8까지 성장, cap일 때 oldest wait. 고정3 ring의 중간 대기를 피하려는 구조이다. GPU lost일 때 stuck allocator를 reset하지 않고 별도 allocator를 고른다.
- M680–695: visibility buffer는 fence 완료뿐 아니라 이전 query가 아직 참조하는지까지 검사한다. allocator 여유가 있다고 query storage를 재사용할 수는 없다.
- BAR R1038–1046은 pending-borrow 수명 증명이 불완전해 GPU pending1을 의도적으로 유지한다. round18은 관측 추출 동등성만 통과했다. **pool8=안전한 pending8이 아니다.**
- 선행 조건: 자원별 producer/borrower ownership, CPU readback 공개, shutdown/detach/실패 quarantine, residency/allocator retirement 및 bounded backpressure.
- 기대 효과는 upload/CPU preparation과 GPU work 사이의 실제 강제 idle이 남아 있을 때뿐. 메모리·latency 증가와 실패 복구 위험이 크므로 P1/P2보다 늦춘다.
- 필요한 측정: wait 원인별 duration·resource family·simulation window, queue idle와 CPU runnable/work량, peak retained bytes. sleep 감소만으로 개선을 선언하지 않는다.

### P5. presentation 경로는 원인 확인용, 별도 queue 복사는 비추천
- M136–141 및 release2.0.4는 Mac separate present queue의 GPU hang과 shared queue 복귀를 기록한다.
- M8120–8121은 game lock 안 nextDrawable 동기 대기가 loading thread를 굶길 수 있어 별도 CPU presentation thread로 queueing한다는 설계 이유를 설명한다. **CPU thread 분리와 GPU queue 분리는 서로 다른 선택**이다.
- BAR에서 drawable acquire/present가 critical path인지 아직 이 비교만으로 확인하지 못했다. present 불필요 지연이 확인되더라도 event dependency·swap texture reuse·background/shutdown을 증명해야 한다.
- FG/displayed frames 수치는 전부 제외. 실제 rendered frame 완료와 present latency를 따로 측정한다.

### 이번에 채택하지 않는 가지
- M3445–3453의 async PSO effect draw 생략: shader 준비 지연을 없애도 같은 화면이 아니므로 제외. 대신 정확한 prewarm/PSO cache는 cold-stall이 입증되면 별도 검토.
- M5761–5769 quad-run batching은 같은 state/vertex bytes/triangle order를 검증하는 좋은 패턴이나 D2R automap 특화 workload. BAR 실제 UI draw 병목 확인 전 후순위.
- D2R ray-tracing denoise fusion, downsample/bloom 변경, FG, FSR, MetalFX upscale, render-scale 감소는 현재 BAR 구조 병목 후보와 혼합하지 않는다.

### 실행 없는 우선순위 결론
1. P1: 동일 encoder/family 중복 state의 **실제 빈도와 비용부터** 확인. 성능 예측 대신 would-skip 검증 설계.
2. P2: 반복 준비 비용이 큰 immutable/revision-correct block을 선택, fresh reference와 비교 가능한 cache.
3. P3: 이미 있는 BAR220 최적화를 존중하면서 남은 boundary 한 개의 증명 기반 stage overlap.
4. P4: pending-borrow/실패 quarantine 계약이 완성된 뒤 bounded submission lookahead.
5. P5: drawable/present wait가 실제 원인일 때만. Mac GPU queue 추가는 기본 선택 아님.
- [x] 고정 D2R Metal H/M 코드 블록 및 BAR R03 관련 블록 대조.
- [x] 사용자 FG/FSR 제외와 동일 출력 원칙 기록.
- [ ] BAR counted workload의 P1/P2 hit·CPU critical path 증거 확보. 이번 분석에서는 실행하지 않음.
- [ ] correctness와 성능 창을 분리한 새 bounded round 필요 여부는 JINI가 최신 STATE와 HQ 정리를 확인한 뒤 결정.
- HQ/Mac 접근, 코드 수정, 빌드·게임·GPU helper·벤치마크는 이번 분석에서 수행하지 않았다.

고정 출처: [D2R recorder](https://github.com/PowerBeef/d2r-metal/blob/602db5fc30d4ce50683435cb0745cb171fce2064/dm/gfx/nvn/hle_nvn.c#L504-L574), [D2R Metal backend](https://github.com/PowerBeef/d2r-metal/blob/602db5fc30d4ce50683435cb0745cb171fce2064/dm/gfx/nvn/nvn_mtl.m), [BAR R03 WorldRenderer](https://github.com/yeojuny/BAR-Metal4/blob/0780bd7c530b0696067b717428b49a416eda556a/engine/rts/Rendering/Metal4/Metal4WorldRenderer.mm), [BAR R03 RuntimeNative](https://github.com/yeojuny/BAR-Metal4/blob/0780bd7c530b0696067b717428b49a416eda556a/engine/rts/Rendering/Metal4/Metal4RuntimeNative.mm).

## 정정 체크포인트: R03 Screen state elision은 이미 존재·사용 중

- 앞선 P1을 **UI Screen에 처음 state elision을 넣자는 제안으로 읽으면 안 된다**. 고정 R03 benchmark에서 `BAR_NATIVE_SCREEN_STATE_ELISION=1`이 이미 ON임을 총괄 측 benchmark 확인 결과로 전달받았다. 이번 문서 작업에서는 benchmark를 재실행하지 않았다.
- 고정 R03 `Metal4ScreenNative.mm`을 직접 재조회했다. L971–975는 EncodeRange마다 fresh previousState와7개 상태 카운터를 만들며, resource/argument table/state를 range 경계 너머 재사용하지 않는다고 명시한다.
- L981–987은 viewport finite+bitwise equality 및 scissor equality; L995–998은 **cull/depth/stencil/viewport/scissor/depthclip/pipeline** 7개 생략 카운터와 argument binds unchanged를 기록한다.
- L1008–1011은 unitIcons/sensorLOS/sensorPreview/buildSquare/weaponRange/startBox/PiP/panelShader 등 foreign encoder 호출 전에 previousState를 invalidate한다. 기존 restore를 유지하며 foreign state를 캐시에 공개하지 않는다.
- 따라서 일반적인 Screen UI state-elision 또는 foreign encoder invalidation을 **새 최적화/새 이득으로 계산하지 않는다**. 기존 ON 기준판을 OFF로 만들어 얻은 차이는 추가 개선이 아니다.
- 남은 후보는 **별도 typed model raster 경로의 인접 draw 중복**이다. 기존 Screen 범위와 구분하고, 실제 battle에서 연속 동일 state의 빈도·CPU critical-path 기여는 아직 미측정이다. 앞선 W5332–5348 legacy 예시는 코드 관찰이지 실제 hot typed path 확인이 아니다.
- 수정된 P1 다음 단계: 정확한 typed model family/raster boundary 식별 → 기존 cache와 중복 여부 확인 → actual battle would-skip 빈도·비용 증거 확보 → 수명·foreign callback invalidation 검증. 이 절 자체는 새 계측/실행 허가가 아니다.
- 기존 이력·revision74/round18 기록은 보존한다. FG/FSR 제외, HQ 독점, main/release/Homebrew 보호는 변함없다.

[고정 R03 ScreenNative 근거](https://github.com/yeojuny/BAR-Metal4/blob/0780bd7c530b0696067b717428b49a416eda556a/engine/rts/Rendering/Metal4/Metal4ScreenNative.mm#L971-L1011)

## 최신 방향: 한 프레임 내부 의존성·병렬 실행 중심 (2026-11-17)

### 방향 전환과 R02 trace 해석
- 최신 사용자 방향은 **한 프레임 내부 pass/stage 간 의존성과 병렬 실행**을 본 작업으로 삼는 것이다. 앞선 P1/P2 목록은 비교 후보 이력이며 현재 최우선 순서를 대체하지 않는다.
- 총괄이 확인한 **R02 원본 export**에서 GroundDecal fragment stage(FS)와 Grass vertex stage(VS)가 겹치고, Grass FS는 그 뒤에 온다. 따라서 전체 pass 박스가 인접한다는 이유로 전부 직렬이라고 판단하지 않는다.
- SensorLOS로 표시된 구간에는 Bloom/DeferredLight 등 다른 작업도 공유되어 있다. 그 영역의 시간 폭을 **SensorLOS 단독 GPU 독점 비용**으로 계산하거나 제거 가능한 시간으로 환산하지 않는다.
- 위 두 관찰의 출처는 총괄의 R02 원본 export 확인 결과이다. 이번 문서 작성자가 별도 GPU capture를 실행하거나 그 export 전체를 재검증한 것은 아니다.
- 아직 **개별 자원의 producer→consumer lineage와 실제 active intervals**를 확보하지 못했다. 이름표·외곽 span만으로 진짜 dependency/대기 원인을 확정하지 않는다.
- 다음 순서는 **HQ19가 종료·정리된 뒤 기존 gputrace의 read-only export 확보/검토**가 우선이다. HQ19의 현재 진행/완료 상태는 이번 문서 작업에서 조회·확정하지 않았다.
- 기존 trace를 먼저 읽고 resource/subresource, producer/consumer stage, active interval, barrier와 wait 원인을 연결한다. 그 전 신규 capture·benchmark·pending2 확대를 이 문서로 승인하지 않는다.
- FG/FSR 제외와 원래 출력 보존, HQ 실행 중 Mac 접근 금지, 최신 STATE 실행권 원칙, main/release/Homebrew 별도 승인은 유지한다.

### 사례 조사 분류: OpenGL4.3+ → Metal4라고 말할 때의 기준
- 원래 GL 버전/실제 사용 기능, native Metal API 세대, visible-frame owner, shader translation과 runtime API translation, 안정판/실험판을 따로 확인한다. native ARM 프로세스라는 사실은 직접 Metal4 renderer 증거가 아니다.
- **Minecraft vanilla:** 공식1.17 기록은 GL3.2 core이다. Sodium의 GL4.5 드라이버 공식지원 조건은 vanilla4.3 요구나 모든 draw의4.5 기능 사용 증거가 아니다. Minecraft native Metal mod 전체를 GL4.3+→Metal4 성공 사례로 묶지 않는다.
- **Complemetal** `81defd5285bc84e0022d25221d82e8541dec5655`: 실제 MTL4 queue/allocator/graph 코드가 있으나0.4.1 안정판 Iris visible frame은 **OpenGL** 유지. +13.29% paired run도 양측 GL이며 Metal4 uplift 증거가 아니다. 실험적 MTL4 graph의 parity/lifecycle/ownership gate가 BAR 참고 대상이다.
- **MetalCraft** `a2cc82780d01a51d00a297f75cf07c221d7c8700`: Blaze3D→JNI→직접 Metal. 확인한 `src/main/native/metalcraft.mm`은 MTLCommandBuffer/MTLRenderCommandEncoder/newCommandQueue이며 MTL4 출현0. 전체 Metal4 이식으로 분류하지 않는다. 한 장치·한 고정 장면의 early prerelease이고 logic-op 무시/직접 GL mod 미지원.
- **Blender:** 공식 GL4.3급 요구와 macOS Metal-only 전환의 대형 엔진 사례. 이번 조사에서 Metal4 직접 제출 경로는 확인하지 못했다. 게임도 아니므로 동일 workload 성능 근거가 아니다.
- **Godot:**4.7 공식 문서는 지원 환경 Metal4를 명시하지만 GL Compatibility와 RenderingDevice Metal은 별도 renderer 계통이다. 조사한 master `203bf0a23fd7d90a860917e66de08e20d2596244`에는 MTL4 참조와 Metal3 context 경로가 함께 보여 실제 선택 조건/source closure는 미완료. GL4.3 renderer의 동일구현 직접 이식이라고 말하지 않는다.
- **MGL** `de0ded04ec7dc99182e27e555a17775523b26911`: GL4.6/ES3.x→Metal 호환층. incomplete/nonconformant 및 functional-not-coverage tests를 자인하며 Metal4는 미확인.
- **Zink+KosmicKrisp:** GL→Vulkan→Metal4 번역 경로. 직접 엔진 이식 아님. Mesa 공식 구현은 확인했으나 특정 게임 조합의 실행·성능을 이번에 검증하지 않았다.
- 결론: 조사한 범위에서는 **GL4.3+ 원게임/엔진→직접 Metal4 전체 화면→안정화·동일출력 성능 입증** 조건을 모두 만족하는 공개 사례를 확정하지 못했다. 없다는 전세계 부재 증명이 아니다.

### 이번 인계 체크
- [x] 프레임 내부 overlap 중심 최신 방향, R02 관찰의 출처와 한계 기록.
- [x] Minecraft/GL4.3+/Metal4 사례의 경로·성숙도 구분.
- [ ] HQ19 종료·정리 및 최신 STATE를 총괄이 확인한 뒤 기존 gputrace read-only export.
- [ ] 실제 자원 lineage·active intervals 확보 후 필요한 dependency만 재평가.
- [ ] 개별 pass 외곽시간 합산이나 일부 병렬구간 제거만으로 FPS 예상 금지.

출처:
- [Minecraft 공식1.17 GL 변경](https://feedback.minecraft.net/hc/en-us/articles/4402626897165-Minecraft-Caves-Cliffs-Part-1-1-17-Java), [Sodium README](https://github.com/CaffeineMC/sodium/blob/dev/README.md)
- [Complemetal 고정 README](https://github.com/daniiarkg/complemetal-mc/blob/81defd5285bc84e0022d25221d82e8541dec5655/README.md), [Metal4 상태](https://github.com/daniiarkg/complemetal-mc/blob/81defd5285bc84e0022d25221d82e8541dec5655/docs/METAL4_STATUS.md)
- [MetalCraft 고정 backend](https://github.com/Im-Fran/MetalCraft/blob/a2cc82780d01a51d00a297f75cf07c221d7c8700/src/main/native/metalcraft.mm)
- [Blender 공식 compatibility](https://developer.blender.org/docs/release_notes/compatibility/), [Godot4.7 architecture](https://docs.godotengine.org/en/4.7/engine_details/architecture/internal_rendering_architecture.html)
- [MGL 고정 README](https://github.com/openglonmetal/MGL/blob/de0ded04ec7dc99182e27e555a17775523b26911/README.md), [Zink](https://docs.mesa3d.org/drivers/zink.html), [KosmicKrisp](https://docs.mesa3d.org/drivers/kosmickrisp.html)

## 최신 정정: round20 저장 export 검토 완료, bounds는 실제 overlap 시간이 아님

- 실제 원격 STATE 재조회: **revision80 / DONE / SUPER_JINI_MK2**, round `r02-existing-trace-export-20261117-20`.
- artifact `9d9c34e19c137857540fb3133a8452523e74074e`의 REPORT를 직접 읽었다. capture source는 **R02 `0382d0f3833d63b3ff1a78c46b108c99539656d0`**이며 R03·후속 후보 성능을 대표하지 않는다.
- 이전 절의 “GroundDecal FS와 Grass VS가 겹친다”는 표현은 **저장된 aggregate bounds상 교차 관찰**로 한정·정정한다. 원자료의 개별 active intervals와 단위/column schema가 없으므로 실제 동시 실행 길이·연속 overlap·GPU 독점 시간을 입증하지 않는다. Grass FS의 bounds 순서도 정확한 모든 constituent interval의 순서를 뜻하지 않는다.
- label9 exit **Fragment→queue Fragment**, label10 entry **queue Fragment→Fragment**는 실제 API slice에서 확인됐다. 이 명시적 wait는 Vertex를 포함하지 않으므로 “decal→grass에서 Vertex wait를 없애자”는 제안은 이미 적용된 변경의 반복이다. 이것이 모든 hidden hazard나 overlap duration의 증명은 아니다.
- labels11/12 및33 이후 broad stage waits는 존재하지만 attachment bindings와 last-writer가 없어 안전하게 줄일 수 있는지 미확정이다.
- SensorLOS label33: api9754 dss15, api9766 rps61 draw(378 vertices×165 instances). api9776의 dss12는 api9786 rps15 draw에 적용하며 **또 다른 SensorLOS draw로 추정하지 않는다**. dss12/15 전체 compare/write/mask/op는 UNAVAILABLE.
- 과거 replay shader-cost shares SensorLOS26.34215%, GroundDecal14.74173%, GameDecal8.59926%는 runtime frame fraction도 예상 절약률도 아니다. SensorLOS 구간의 다른 작업 공유 관찰 역시 단독 독점 시간으로 바꾸지 않는다.
- api10982/ce51의 **tex99→tex2 copy edge**는 명시적이다. tex99 upstream attachment writer와 정확 drawable texture identity는 미확보. residency 등록/isAliasable query는 read/write/alias 증거가 아니며 argument-table 숫자를 trace @tex 번호와 같다고 두지 않는다.
- shader rows의 bounds pair와 문서화되지 않은 trailing field만으로 active duration을 계산하지 않는다. **zero-draw label39도 whole-frame처럼 보이는 range**를 가지므로 범위를 GPU occupation으로 읽으면 틀린다. bounds intersection을 합산하거나 measured overlap으로 보고하지 않는다.

### 이번에 확보한 것과 남은 것
- [x] encoder52개 creation order 매핑, render draw counts42개 확인, 지정 label14개의 API slices.
- [x] SDK stage constants에 따른 barrier decode, pipeline/DSS 실제 bind, copy edge, raw export9개 provenance/hashes.
- [x] 원 trace before/after 동일:1,223 files,2,788,204,457 logical bytes, tree SHA256 `5ae339d460ddb46089783227ba115d37ae4a0e3c4327d81e99c665741a8ea228`.
- [ ] attachment descriptors, complete resource hazards/last writers, full dss12/dss15 state, individual active stage intervals는 **UNAVAILABLE**. 소스 추정으로 메우지 않는다.
- HQ는 새 inspection session을 열지 않았다. gpudebug help에 zero-GPU load 보장이 없고 과거 loading 상태는 실행 여부를 확정하지 않으므로 이번 제한을 충족할 보장이 없었다. session checks2회 모두 no active sessions, games/builds/captures/replay/profiling0.
- export 검토는 pass/API map을 개선했지만 barrier/in-place-write 변경의 수명·의존성 증명을 끝내지 못했다. 지원되는 별도 inspection 경로와 필요한 권한을 확인하기 전 replay 금지.
- **소스 수정·FPS 개선·예상 timing·새 benchmark 권한 없음.** 문서 이전의 HQ19 이후 export 대기는 이 round20 완료 기록으로 갱신한다.
- REPORT에는 별도 사용자 요청 Astra ultra follow-up 감사가 시작됐다고 기록한다. 읽기+자기 감사 문서만 허용하며 게임/빌드/GPU/저장소 수정 권한은 없다. 이번 체크포인트 작성에서 감사 결과를 수신·검증했다고 주장하지 않는다.

[고정 round20 REPORT](https://github.com/yeojuny/BAR-Metal4/blob/9d9c34e19c137857540fb3133a8452523e74074e/coordination/native-metal4/rounds/r02-existing-trace-export-20261117-20/artifacts/REPORT.md)


## round21 CPU tail 원본 결과와 R04 다음 비교 (2026-11-17)

### 확인한 스냅샷과 출처
- [x] 실제 원격 STATE revision **84**, DONE/JINI, round `cpu-postcompletion-tail-20261117-21`를 읽었다. 문서 작성 시점 스냅샷이며 이후 실행권은 최신 STATE로 판단한다.
- [x] [round21 고정 REPORT](https://github.com/yeojuny/BAR-Metal4/blob/1145a599c051b24071472d131f73d793498df49d/coordination/native-metal4/rounds/cpu-postcompletion-tail-20261117-21/REPORT.md) 원문을 읽었다. 실행 source는 `c34129af9158bbdef1321d17825f1f60721b74ec`, artifact는 `1145a599c051b24071472d131f73d793498df49d`.
- [x] [기존 R04 helper DONE](https://github.com/yeojuny/BAR-Metal4/blob/coord/native-metal4/coordination/native-metal4/rounds/r04-segment-setup-correctness-20261117-09/DONE.json)을 읽어 아래 검증 범위와 한계를 대조했다. R04 source `5d20bd842b3c881166abb58f361482a5d7bb19e7`, helper artifact `45d0ada23c259df5e13ff8d68c9158932a84e065`.

### round21: 작은 CPU tail, GPU idle·성능 향상 증거 아님
- 진단 게임 1회의 simulation 900–915 짧은 구간에서 정확한 내부 predecessor→successor 체인 **33개**. FeedbackWait(N).end→Submit(N+1).start 평균 **0.032586 ms**, 중앙값 0.032458 ms, p95 0.046417 ms, 최대 0.047958 ms.
- 별도 submit 호출 평균 0.029876 ms. tail 내부 callback 평균 0.017798 ms, 네 하위 구간 합집합 0.021861 ms, 나머지 0.010725 ms. RuntimeRetire와 자식 시간을 중복 합산하지 않는다.
- 시작점은 **호스트 feedback wait가 끝난 시점**이지 실제 GPU 완료 시점이 아니다. 약 0.210% cadence 비율은 문맥 정보일 뿐 제거 가능한 GPU idle이나 FPS 예상 이득이 아니다. EventWait/FeedbackWait도 절감 가능 시간으로 계산하지 않는다.
- 첫 predecessor는 창 밖이라 boundary-censored 1개, 마지막 submission의 미래 successor는 미수집. 마지막 job34 tail은 출력 emission과 겹치며 그대로 포함했다. 인위적 보정·제외·재실행 없음.
- trace 1,496행/68 buffers, dropped0/identity errors0. 내부 체인 정합성은 확인됐으나 전체 경계·장시간 전투·다른 장면으로 일반화하지 않는다. Early-drain 사례는 이 deferred 표본에 포함되지 않았다.
- 게임 정상 종료/정리 완료, 기존 증거 불변. 빌드·capture·replay·helper·FPS A/B·retry·source edit는 모두 0. 진단 FPS는 계산하거나 성능 증거로 채택하지 않았다.
- 판단: 이 관측만으로 CPU settlement 이동을 큰 현재 병목이라고 주장할 수 없다. 한 프레임 내부 의존성/자원 lineage 검토는 계속하되, round20 aggregate bounds≠실제 active overlap 정정은 유지한다.

### R04: 검증된 helper와 미측정 게임 성능을 분리
- R04 SensorLOS segment setup 공유 후보는 **제한된 helper GPU 정확성 통과**. color/depth/stencil old/new 비교 12,065개, prepared 경로를 끄면 차이를 감지하는 negative control 통과, 실제 Metal Validation 오류 0.
- 그러나 synthetic ParityVS helper이며 production OriginalSensorLOSVertex 전체 경로의 전수 검증이 아니다. 실제 MSAA4 미검증, 큰 좌표 GPU 동등성 미검증. 추가 32-byte varying 및 vertex 계산 비용이 fragment 절약을 상쇄할 수 있다.
- **R04의 실제 게임 성능은 아직 미측정**이다. helper 통과나 replay SensorLOS 비율만으로 향상률·승격을 말하지 않는다.
- [ ] 다음 비교 대상은 R03 `0780bd7c530b0696067b717428b49a416eda556a` 대 고정 R04. 별도 결정/인계에서 고정 source·manifest·바이너리와 동일 장면·품질 조건을 확정하고, 화면/Metal 검증 후 성능 비교한다. 이것은 계획이지 실행 완료나 이 문서 자체의 실행 허가가 아니다.
- [ ] full production/경계/자원 수명/다른 장면 미검증 항목은 남긴다. HQ 소유 중 JINI·보조 작업이 로컬 실행에 간섭하지 않는다.
- MVK3/BAR가 메인, 포팅 분석은 보조. FSR/frame generation 제외, main·release·Homebrew 보호 유지. 이 갱신은 별도 docs branch와 클라우드 로컬 Markdown만 변경한다.


## round24: R04 게임 검증 완료, 성능 회귀로 미채택 (2026-11-17)

### 실제 읽은 근거와 완료 상태
- [x] STATE revision **94**, round `r04-r03-validation-clean-20261117-24`의 완료 기록과 [고정 REPORT](https://github.com/yeojuny/BAR-Metal4/blob/c47add111fee80fba87b41cfbae012743033b163/coordination/native-metal4/rounds/r04-r03-validation-clean-20261117-24/REPORT.md)를 읽었다. artifact `c47add111fee80fba87b41cfbae012743033b163`.
- 이 절은 앞선 “R04 게임 성능 미측정/비교 예정”을 갱신한다. 과거 helper·round21 기록은 당시 상태로 보존한다.
- 고정 R04 source `5d20bd842b3c881166abb58f361482a5d7bb19e7`, R03 `0780bd7c530b0696067b717428b49a416eda556a`. 두 네이티브 후보 간 비교이며 **MVK3 비교가 아니다**.

### 성능: 3회씩 비교한 제한된 회귀 증거
- [x] R04 validation 1회 통과 후 clean 6회, 순서 **R03, R04, R04, R03, R03, R04**. 각 arm 3회의 산술평균 R03 **63.81081856**, R04 **59.07250632**, R04 상대 변화 **−7.42556254%**.
- R04 세 결과 모두 R03 세 결과보다 낮았다. 유효한 느린 실행·프레임 모두 유지, 재실행/제외/역사 표본 합산 없음. validation FPS 제외.
- 단위는 **CPU 관측 DrawScreen 프레임 수÷측정 wall 시간**이며 GPU 실행률이나 display-present FPS가 아니다. 측정 observer의 오버헤드도 별도 정량화하지 않았다.
- 동일 장면/품질 조건에서 40개 simulation·unit·camera·speed·pause 행이 clean6 전체에서 일치. 이는 bounded fixture 비교 근거이지 모든 장면·모든 렌더 상태의 동등성 증명은 아니다.
- **실제 회귀 원인은 미확정**. 추가 varying/vertex 계산, 자원 압박, shader 실행 변화 등은 분리 측정 전 가설일 뿐이다. replay 비중이나 이전 aggregate bounds로 원인을 확정하지 않는다.

### 검증·화면과 한계
- [x] 실제 R04 production validation에서 Metal API Validation 오류 **0**, shader/pipeline 실패 행 없음, 정상 종료 후 clean 시작. clean에서는 validation/trace/diagnostic probes OFF.
- [x] HQ 원본 REPORT의 실제 PNG 7장 검토에 따르면 지형·구조물·차량·그림자·표식 등에 명백한 R04 전용 결손/고정 지오메트리 손상 없음. 이 문서 갱신 담당은 REPORT를 읽었으며 PNG를 별도로 전수 재감사한 것은 아니다.
- smoke/particles/water/cloud/overlay 변화가 있으며 pixel·depth·stencil 완전 동등성이나 counted 전 구간 unsynced UI 일치를 주장하지 않는다. 현재 range circle은 R03 3/3, R04 3/3; 역사적 출현 차이 원인은 여전히 미확정.
- [x] 게임 총7, build/helper/capture/replay/retry/source edit 모두0. HQ 기록상 process groups/controller/awake 정리 완료, 기존 입력·증거 불변, bar PAUSED 유지.

### 결정과 다음 판단 경계
- **R04를 성능 개선으로 채택하지 않는다.** R03 기준판을 보존하고 R04는 실패/회귀 실험 증거로 남긴다. main·release·Homebrew 승격 없음.
- [ ] 회귀의 실제 원인 분리는 별도 고정 계획/예산/실행권 아래에서만 진행. 이 기록이 추가 프로파일·벤치·소스 변경을 허가하지 않는다.
- MVK3/BAR 비교가 메인이고 포팅 사례는 보조. 한 프레임 내부 의존성 검토, aggregate bounds≠active overlap, 미확보 resource lineage 등의 한계도 유지한다. FSR/frame generation 제외.
- [x] 본 갱신은 docs 전용 branch와 클라우드 로컬 Markdown 두 사본만 수정하며 STATE·coord branch·소스·사용자 Mac은 변경하지 않는다.


## round25 진행 중단 기록: 캡처 경계 질의 시간초과

- 출처: 조정 담당 JINI가 전달한 현재 진행 상태. 이 문서 갱신에서 Mac이나 세션을 직접 조회하지 않았다.
- round25 캡처 경계 질의가 시간초과되었다. **세션 상태는 미확인**이며 종료·정리·안전한 재실행 가능 상태로 간주하지 않는다.
- 서비스 재시작은 **사용자 승인 대기**이다. 승인이나 재시작 완료를 기록하지 않는다.
- [ ] 사용자 승인 여부와 이후 실제 복구/세션 상태 확인 결과를 다음 체크포인트에 남길 것. 현재 기록은 재시작·재실행·소스 변경을 허가하지 않는다.
- 이전 round24 R04 미채택 및 성능 회귀 원인 미확정 판단은 그대로 유지한다. 본 갱신은 docs branch/클라우드 로컬 Markdown만 변경하며 STATE·소스·Mac은 변경하지 않는다.
