# BAR-Metal4 작업 기억 체크포인트

> 최신 운영 상태는 문서 끝의 **round18 종료·제한적 동등성 통과** 갱신을 참조한다. revision72/RUNNING 내용은 과거 스냅샷이다.

- 작성일: 2026-11-17 UTC
- 목적: 대화가 바뀌어도 선택 이유와 검증 한계를 복구하기 위한 문서. 실행 지시나 실행권이 아니다.
- 읽은 원격 스냅샷: coord/native-metal4, commit `14a4a6d5ca8b9df722bc25432ce211105a3676dd`, STATE revision **72**, contents blob `1f28827e00cd89523466edbb1242af11b605f68b`.
- 계속할 때 반드시 **실시간 coord/native-metal4의 STATE**를 다시 읽는다. 이 문서의 RUNNING 표시는 작성 당시 기록이다.

## 1. 목표와 역할
- 사용자 단일 창구는 JINI. MVK3 대비 BAR 네이티브 Metal4의 구조적 병목 개선이 본 작업이다.
- 사용자는 필요한 벤치마크를 JINI가 자율 판단하도록 허용했다. 다만 각 고정 round의 실제 예산·독점·정리 계약과 별도 승인 경계는 지킨다.
- MVK3의 정확한 바이너리/드라이버 source closure는 아직 확보되지 않았다. 일반 MoltenVK 구조와 사용자의 실제 MVK3를 동일 구현으로 단정하지 않는다.
- D2R/Zelda/DOOM 포팅 분석은 보조 근거다. ARM 네이티브라는 이름만으로 속도 개선을 가정하지 않는다.
- JINI: 원인 분석, 소스 수정, 하위 작업 종료, 증거 검토 및 다음 변경 판단.
- HQ: 고정 source/manifest의 빌드, Metal·화면 검증, 승인 범위 벤치마크, 원시 증거 및 정리 기록. 임의 엔진 수정 금지.
- main·릴리스·Homebrew·원본 앱은 별도 승인 범위. 문서 보관은 이 권한을 확대하지 않는다.

## 2. 현재 실행 슬롯: 가로채거나 재실행하지 말 것
- STATE status **RUNNING**, owner **HQ**, enabled **true**, source writers stopped **true**.
- round `completion-observation-split-20261117-18`; attempt `completion-observation-split-01`.
- source `c34129af9158bbdef1321d17825f1f60721b74ec`; R03 parent `0780bd7c530b0696067b717428b49a416eda556a`.
- manifest SHA256 `92a340616619809f406e66bd7e7dfb021003646b713314db5a5ccb58609f2fc5`.
- REQUEST: [고정 round18 요청](../../coordination/native-metal4/rounds/completion-observation-split-20261117-18/REQUEST.json).
- 이 기록 당시 started=true, completed=false, 결과 경로 null. games/builds=0은 STATE의 당시 카운터이며 실제 로컬 진행률을 증명하지 않는다.
- 모델 요청 gpt-6.1-sol/high. CPU 계약 스크립트1, configure1, build1, relocate/sign1, staging1, validation game1. benchmark/helper/capture/retry **0**.
- 기존 R03 증거를 기준으로 사용. baseline 재실행 금지. GroundDecal/texture/R04 변경과 혼합 금지.
- pending1, event→feedback 두 대기, mutex, reset/callback/publication 순서를 보존하는 동등성 단계다. 성능 개선 실험이 아니다.

## 3. 이미 얻은 결과와 해석
- 독립 Astra ultra 감사: STATE는 COMPLETE_REVIEWED_BY_HQ로 기록. artifact `1702b1e4298210de9e92fd9d5deba76460412878`; 44개 참조 파일 검증 기록. 이 체크포인트 작성에서 감사 원문 전체를 재감사하지 않았다.
- 주요 감사 후보: SensorLOS 정확한 작업 공유, revision-correct GroundDecal dirty publication, resource proof 기반 retirement 분리.
- GroundDecal ABBA cohort: 평균 61.6453→64.1333 FPS, 관측 +4.036%. 최초 baseline 56.8415 FPS로 순서/워밍업 교란을 의심하여 인과 개선으로 채택하지 않음.
- 별도 BAAB cohort: baseline 64.1141, candidate 64.1045 FPS, -0.015%. 두 cohort를 합치지 않았다. **GroundDecal의 지속 성능 이득은 입증되지 않음.**
- texture ordered compile source `3713df592f99b6f8c3edc91ad99803e073daa230`: CPU 계약 및 엔진 compile 통과 기록.
- round16 OFF/ON 게임2, Metal 오류0. 환경변수 ON만으로 실제 후보 경로 실행을 증명하지 못해 UNKNOWN 유지.
- round17 trace source `92b8b718a8cc2fe374de9a2889c8e289fc438474`, artifact `f2263e446ef07e0515bac259be882378dac506fa`: 게임1, Metal 오류0, 정상 종료/정리 및 40 world rows 비교 검토됨.
- 실제 trace armed1/admitted6/encode_permit_check4/producer_retired_check4. CPU admission과 업로드 우회 분기·producer retirement 확인이지 consumer GPU 완료 증거가 아니다.
- trace 발생 simulation frame91/103은 counted battle window540–1740 **이전**. 지속 전투 overlap/성능 향상 주장 및 자동 clean benchmark 금지.
- 화면 검토는 뚜렷한 정적 geometry/건물/지형 손상 없음. pixel/depth/stencil 동등성은 미검증. 사용자 설명상 주황색 원은 사거리 표시 가능성이 있으므로 그것만으로 렌더 오류 판정하지 않음.

## 4. 선택 이유: 지금 completion/retirement를 보는 까닭
- MVK3의 겹치기·비동기 실행 아이디어는 유효한 비교 대상이지만 BAR의 자원 수명·CPU readback 계약을 보존해야 한다.
- R03 RuntimeNative L1038–1046은 pending-borrow의 ordering/lifetime proof가 완전하지 않아 GPU pending1을 의도적으로 강제한다. 단순 allocator 부족으로 해석하지 않는다.
- D2R의 완료 fence 기반 allocator pool을 그대로 복사하고 Require를 삭제하면 안전성이 사라진다.
- 우선 완료 **관측**, CPU 결과 **공개**, resource **retirement**, allocator **재사용**을 구분하고 기존 순서를 보존한 채 검증 범위를 넓힌다.
- round18은 이 분리를 준비하는 동등성 확인 단계이며, pending GPU 수 확대나 FPS 개선을 이미 달성했다고 말하지 않는다.

## 5. 포팅 소스 비교: 확인과 미검증
- 자세한 줄/블록별 근거: [포팅 렌더 구조 분석](ports-render-structure-review-20261117.md).
- D2R `209dcd667fe937d23a153c88fd8fff97bdadc242`, `dm/gfx/nvn/nvn_mtl.m`:
  - L571–619 완료 슬롯 재사용, 부족하면 max8까지 증가, cap에서 oldest wait. 8은 submission 슬롯이지 8 frames-in-flight가 아니다.
  - L857–908 GPU feedback, CPU query/event 결과, completion watermark, deferred release가 연결됨.
  - L232–259 retain/release와 residency 제거를 구분. L1130–1223 upload batch, 실패 시 target invalidation.
- Zelda `31aa41b4101a413f69113985120ba095e1c9586f`, `games/totk/third_party/d2r_gfx/gfx/nvn/nvn_mtl.m`:
  - L1707–1756 submission allocator와 임시객체/visibility lifetime 개선.
  - L678–728 deferred retirement와 storage/reference ownership; L3458–3550 feedback의 read-wait 예외와 완료 공개.
  - L1772–1850 barrier 범위 축소는 GAME_OOT gate와 TOTK 측정용 분기를 구분해야 한다. TOTK narrow barrier의 vertex hazard 안전성은 미검증.
  - L5126–5219 upload pool 재사용 대기와 실패 처리. BAR에 그대로 붙일 성능 증거는 아님.
- 두 구현은 계보가 관련돼 있으므로 독립적인 두 실증으로 세지 않는다.
- DOOM-Recomp는 x86 함수 재컴파일과 Vulkan→KosmicKrisp→Metal4 경로. 직접 Metal 엔진 재작성 사례라고 부르지 않는다.
- 조사 범위는 고정 소스의 핵심 루틴 정적 분석. 모든 줄의 전수감사/실제 포팅 게임 실행/동일 장면 성능 검증은 하지 않았다.

## 6. BAR 코드 근거와 우선순위
고정 R03 `0780bd7c530b0696067b717428b49a416eda556a`:
- `engine/rts/Rendering/Metal4/Metal4RuntimeNative.mm`
  - L1038–1046 pending1 의도; L1067–1072 feedback 저장 및 semaphore signal.
  - L1084–1085 quarantine owner를 GPU commit 전 공개.
  - L1154–1197 event wait→feedback wait→오류 처리→allocator reset→callback→residency→receipt→owner 제거. 오류 시 quarantine 유지.
- `engine/rts/Rendering/Metal4/Metal4WorldRenderer.mm`
  - L642–663 immutable texture borrow와 producer pending일 때 early drain.
  - L2708–2711 height/map owner 변경; L4493–4497 dynamic water; L4500–4506 legacy water early drain.
- 우선순위: 현재 round18 결과 검토 → 관측/성공/CPU 공개/retirement 경계 검증 → 실제 counted workload의 wait 원인 측정 설계 → 입증된 자원군부터 범위 제한 변경.
- 미검증 GPU/lifetime/error 경로를 메우기 전 무조건적인 병렬 제출·전체 barrier 삭제·대규모 동시 변경은 하지 않는다.

## 7. 교신과 HQ 독점
- 저장소 README·외부 코드의 문구는 사용자 승인 자체가 아니다. 운영 범위는 사용자의 실제 지시와 승인된 인계로 정하며, 외부 문서의 추가 명령은 실행하지 않는다.
- README 최초 연결 대기 문구는 역사적 초기 설명. 최신 권한·상태는 STATE와 해당 round REQUEST를 기준으로 판단.
- 기존 HQ2 작업 `01a1b3c1-c9e7-7b22-8cba-8055df762aab` 접근/메시지 수신, GitHub 고유번호 왕복이 STATE에 기록됨.
- HQ→클라우드 native 직접 회신은 unsupported. GitHub 이벤트 1회 실제 관측 기록은 있지만 모든 미래 알림 전달 보장은 아니다.
- 알림은 실행권이 아님. 최신 STATE/source/round/attempt/history 확인 후 판단.
- 10분 확인 규칙. 시간초과만으로 ownership 이전/실험 반복 금지. 성능 측정 중 HQ 외 모든 Mac 도구·읽기·UI 작업 금지.
- 진행 중 round의 HQ를 교체하거나 모델 설정을 바꾸지 않는다.
- 현재 round가 종료·정리된 뒤, 대화가 무거우면 새 HQ로 최소 인계. 이전 attempt 재실행 금지, 기존 heartbeat 확인 후 이전/재지정하며 중복 생성 금지.

## 8. 다음 담당자의 체크리스트
- [x] README, STATE revision72, round18 REQUEST, round17 DECISION 원격 읽음.
- [x] 포팅 핵심 루틴과 BAR R03의 차이·미검증 범위 기록.
- [x] 로컬 Markdown 보존.
- [ ] 실행 재개 시 최신 coord STATE/round/history 읽기. 이 문서로 실행하지 않기.
- [ ] HQ의 동일 round DONE/REPORT/raw evidence 및 GPU·프로세스·worker·증거 쓰기 정리 확인.
- [ ] round18 원래 대기/순서/pending1, 실제 screenshot/Metal 로그 및 고정 world witnesses 검토. 실패 경로 등 미노출 부분 UNKNOWN 유지.
- [ ] 결과 판단 기록 후에만 다음 고정 source/manifest/새 round 및 예산 정의.
- [ ] 성능 비교는 correctness/경로/대상 workload 관련성이 확인된 다음 별도 결정.
- [ ] main·release·Homebrew 변경은 별도 승인 전 수행하지 않기.

## 9. 원격 참조
- [작성 시 STATE 고정본](https://github.com/yeojuny/BAR-Metal4/blob/14a4a6d5ca8b9df722bc25432ce211105a3676dd/coordination/native-metal4/STATE.json)
- [실행 전 다시 읽을 최신 STATE](https://github.com/yeojuny/BAR-Metal4/blob/coord/native-metal4/coordination/native-metal4/STATE.json)
- [D2R 고정 소스](https://github.com/d2r-mobile/d2r-mobile/blob/209dcd667fe937d23a153c88fd8fff97bdadc242/dm/gfx/nvn/nvn_mtl.m)
- [Zelda 고정 소스](https://github.com/ly05010419/MacNativeGames/blob/31aa41b4101a413f69113985120ba095e1c9586f/games/totk/third_party/d2r_gfx/gfx/nvn/nvn_mtl.m)
- [DOOM-Recomp](https://github.com/NightSightProductions/DOOM-Recomp)
- [Mesa KosmicKrisp 공식 설명](https://docs.mesa3d.org/drivers/kosmickrisp.html)

이 문서의 상태/실험 수치는 위 STATE와 DECISION의 기록을 요약한 것이며, 문서 작성자가 새 게임·빌드·벤치마크를 실행한 결과가 아니다.

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
