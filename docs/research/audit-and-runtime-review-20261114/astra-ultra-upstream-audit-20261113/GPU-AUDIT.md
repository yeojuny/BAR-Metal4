> Publication copy: original conclusions and historical version scope preserved. Personal absolute paths were neutralized; source links are pinned to the audited a33ebfe revision. Raw logs, JSON manifests, binaries, screenshots and the original plan are not included in this Markdown-only export. See the parent README for later runtime results and limitations.

# BAR-Metal4 GPU 수명·실패·재사용 정적 감사

작성일: 2026-11-13. 소스와 기존 문서만 읽었다. 빌드, 테스트, 게임 실행, 성능 측정, 소스 수정, fetch/reset/checkout은 하지 않았다. 이 파일만 새로 작성했다.

## 판정과 증거 경계

**확인한 경로에서 새 P0/P1 GPU 수명 결함이나 실제 실행 가능한 MRT alias 파손 반례는 찾지 못했다.** 현재 구현은 원계획 부록 B/C의 두 가지 구체적 위험을 상당히 보완했다. GPU pending은 항상 1개로 제한하고, Postprocess의 이전 encoded 버전 대여는 실제 완료 검사를 등록한다. 새 공유 payload·roster·index는 검토한 범위에서 기존 MRT209의 독립 sampler 소유 증거를 없애지 않는다.

이 판정은 전체 native 실행 안전성 승인이나 P6 완료가 아니다. 전체 producer/subresource/access/order 명세는 아직 없고, 현재 소스에 대한 실제 Metal 실행·오류 주입·화면 동등성 증거를 이 감사에서 추가하지 않았다. 최소 9파일 후보의 과거 실행 성공을 R1–R5 및 후속 원격 소스의 실행 성공으로 옮겨 적지 않았다.

판독한 소스는 `LOCAL-SOURCE`의 HEAD `6e9ce8f89afb14d632cce33e4742bab11b76b60f`에 production working 변경 19개를 더한 상태다. 이 19파일은 현재 원격 `refactor/upstream-terrain-t1-20261112`의 `a33ebfe888bc8abe0053df05e5e2892c0788d6a8`과 바이트 단위로 같다는 별도 대조 기록이 있다: remote-working-file-map.json (`LOCAL-EVIDENCE/remote-working-file-map.json`; not included in this Markdown-only export). 원격 후속 docs/tests와 현재 로컬 실행물은 별도 증거 계층이다.

원계획 부록 B 시작 (`LOCAL-EVIDENCE/PLAN-original.md`; not included in this Markdown-only export)부터 부록 B 전체, 부록 C 시작 (`LOCAL-EVIDENCE/PLAN-original.md`; not included in this Markdown-only export)부터 2116행까지 전체를 읽었다. 원계획이 분석한 intervention `16e7f184…` 및 effective native233와 현재 소스의 행 번호·실행 상태를 구분했다.

## 1. 원계획의 구체적 위험은 현재 어떻게 달라졌는가

| 원계획의 위험 | 현재 코드 증거 | 판정 |
|---|---|---|
| `priorReadChecks.empty()`를 독립성 증거로 간주하여 두 번째 GPU ticket 허용 | `Metal4RuntimeNative.mm:890–897`은 무조건 `RetirePreviousSubmission`을 호출하고 pending pointer와 deque가 모두 비어야 submit | 과거 조건부 second-ticket 반례의 진입 조건 제거 |
| UI/postprocess/cloud가 각각 한 submitted slot만 보유 | UI `NativeUIRecorder.cpp:247`, postprocess `NativePostprocessRecorder.h:83`, cloud `NativeVolumetricCloudContext.h:42`는 capacity 2 ledger | 과거 single-slot 충돌을 현 구현의 결함으로 반복하면 부정확 |
| Postprocess가 encoded predecessor를 대여하면서 prior read map에 등록하지 않음 | `Metal4PostprocessNative.mm:483–499`에서 exact logical owner와 완료 flag를 캡처해 등록 | 해당 가족의 누락 보완. 전체 가족 proof 완료는 아님 |
| root-only index가 U의 실제 reader leaf ownership을 지우는 refactor 가능성 | index는 root + 숫자 command refs만 소유. sampler snapshots는 각 `Binding.object` 강소유 유지. async requested pilot은 MRTFrontier 생성 금지 | 검사한 변경에서 반례 성립 안 함 |
| 그림자/원본 resource의 mutable `shared_ptr`를 capture와 renderer가 동시에 읽고 씀 | DeferredColor/ShieldDepth/Map backing는 atomic shared_ptr load/CAS에 의한 one-shot publication | pointer publication 문제 보완. resource 내부 모든 값이 thread-safe라는 의미는 아님 |

핵심 소스: [GPU submit gate](https://github.com/yeojuny/BAR-Metal4/blob/a33ebfe888bc8abe0053df05e5e2892c0788d6a8/engine/rts/Rendering/Metal4/Metal4RuntimeNative.mm#L890), [Postprocess predecessor receipt](https://github.com/yeojuny/BAR-Metal4/blob/a33ebfe888bc8abe0053df05e5e2892c0788d6a8/engine/rts/Rendering/Metal4/Metal4PostprocessNative.mm#L483), [bounded ledger](https://github.com/yeojuny/BAR-Metal4/blob/a33ebfe888bc8abe0053df05e5e2892c0788d6a8/engine/rts/Rendering/Backend/NativeCompletedFrameLedger.h#L14).

`GPU-PENDING-RETIREMENT-GATE.md`의 “Postprocess는 등록하지 않는다”는 설명은 그 gate를 추가한 당시의 이유다. 후속 `POSTPROCESS-PENDING-INPUT-RECEIPT.md`와 현재 코드가 그 누락을 보완한다. 오래된 중간 문구를 현재 결함의 증거로 사용하면 안 된다.

## 2. 실제 event + feedback + allocator + quarantine 경로

정상 deferred submit의 순서는 다음과 같다.

1. `RuntimeLease`는 `requestMutex`를 소유하고, 이전 GPU 작업과 겹치는 CPU recording에는 별도 command/allocator/residency slot을 선택한다 (`Metal4RuntimeNative.mm:564–595`). `Begin`은 pending slot 재사용을 금지한다 (`850–860`).
2. `SubmitAndDefer`는 residency commit을 마친 뒤 이전 ticket의 **event와 feedback, producer retirement, source notice, 등록 read check**를 모두 마친다 (`881–897`, `709–718`).
3. immutable `RuntimePendingSubmission`을 구성하고, GPU commit보다 먼저 pending deque와 `frame->pending=true`를 공개한다 (`901–933`). feedback block은 그 owner를 강하게 잡으며 error/timestamp/semaphore만 기록한다 (`916–921`).
4. commit/drawable present 뒤 shared event를 signal한다 (`941–947`). 성공적인 ownership transfer를 마지막에 표시한다 (`976–980`). CPU 함수 반환은 GPU 완료가 아니다.
5. `DrainOldestLocked`는 shared event를 기다린 뒤 feedback semaphore도 기다린다 (`1004–1005`). 그 둘을 지난 뒤 `frame->pending=false`, GPU error 검사, allocator reset, 실제 producer callback 순서다 (`1007–1017`). 모든 retirement 동작이 성공한 뒤에만 pending owner를 pop한다 (`1018–1025`).

[Runtime retirement](https://github.com/yeojuny/BAR-Metal4/blob/a33ebfe888bc8abe0053df05e5e2892c0788d6a8/engine/rts/Rendering/Metal4/Metal4RuntimeNative.mm#L996)에서 timeout/error를 성공 완료로 바꾸는 경로는 찾지 못했다.

| 실패 지점 | 현 코드 행동 | 안전성 의미와 한계 |
|---|---|---|
| resource preparation 실패, Begin 전 | 실행된 command가 없으면 pending GPU 재사용 조건을 바꾸지 않음. residency 비동기 작업은 lease 소멸자가 join | CPU 실패를 GPU 완료로 취급하지 않음 |
| Begin 후 encode 실패 | `OriginalSubmissionFailure`가 `PoisonAfterBegin` 호출 (`WorldRenderer.mm:4438–4448`), lease 소멸자가 failedFrame을 보관 | 실행 여부가 불확실한 command graph를 회수해 새 frame에 쓰지 않음 |
| shared-event timeout | `pending=true`가 남고 catch가 poisoned/unsafe를 설정; pending graph 유지 (`1004`, `1026–1030`) | allocator reset, successful producer callback 없음 |
| event 성공 후 feedback timeout | 역시 `pending=true` 유지 (`1005`는 `1007`보다 먼저) | event만으로 command memory 재사용 안 함 |
| event+feedback 완료, GPU error | `pending=false` 뒤 error throw, allocator reset 및 producer callback 전에 중단 (`1007–1014`) | 물리적 GPU 작업은 끝났지만 reuse/정상 source completion은 금지. unsafe detach와 poisoned reuse를 구분 |
| producer completion callback 오류 | allocator는 실제 GPU 완료 뒤 reset되었으나 runtime은 poison. pending graph 유지하고 정상 completion handoff를 내보내지 않음 (`1014–1030`) | 일부 producer의 실제 완료 flag가 true가 될 수 있어도 whole-frame 성공이 아님 |
| shutdown | renderer destructor는 CPU job join 후 `CanSafelyDetach()` 확인; unsafe이면 terminate (`WorldRenderer.mm:543–548`) | timeout 후 정상 C++ 파괴로 live GPU owner를 방출하는 경로를 허용하지 않음 |

동기 `SubmitAndWait`도 독립 immutable feedback graph를 만들며 (`1047–1074`), 두 완료 신호 뒤에만 `pending=false`와 allocator reset에 도달한다 (`1097–1127`). 기존 timeout 뒤 늦은 callback이 lease를 직접 참조하는 구조가 아니다.

`WorldRenderer`는 완료 callback을 한 번의 try로 조기 중단시키기보다 가족별 오류를 모으며 실제 성공한 producer의 상태를 기록한다 (`5568–5641`). 최종 `sourceFrameCompleted=true`와 source result 반환은 그 오류 집계 뒤에 있다 (`5643–5646`, `5696–5705`). 이 동작은 transactional rollback이 아니다. 오류 후 전체 재시도/재사용을 허용해서는 안 되며 현재 runtime poison이 그 경계를 유지한다.

## 3. 물리적 자원 재사용은 ObjC retain만으로 허용되지 않는다

[RuntimeFrameResources](https://github.com/yeojuny/BAR-Metal4/blob/a33ebfe888bc8abe0053df05e5e2892c0788d6a8/engine/rts/Rendering/Metal4/Metal4FrameResources.h#L34)는 native resource graph와 C++ pool slot pins를 함께 소유한다. feedback 또는 실패 owner가 frame을 보유하는 동안 slot의 `use_count()==1` 조건이 성립하지 않도록 설계되어 있다.

| 자원 | 확인한 write-exclusion 및 lifetime chain |
|---|---|
| Published transform/model-uniform buffers | `NativePublishedBufferPool.h:55–69`: exact immutable storage hit만 공유하고 변경은 pool 단독 owner에 한정. `Metal4PublishedRequestCache.h:66–71`은 `state.used`에 별도 pin을 추가하여 cache-key 교체가 이전 GPU address의 pin을 제거하지 못함 |
| Shared model upload | `Metal4ModelUpload146.h:48–70`의 local arena가 각 chunk token을 prepared 결과의 owners에 저장. `NativeRetiredResourcePool.h:30–34`는 token 단독 owner일 때만 reset |
| Screen shared upload | `Metal4SharedUploadPool.h:23–46`은 busy token을 건너뛰고 fresh fallback. `M4ScreenArgumentLifetime`이 uniformSlots를 보유 (`Metal4ScreenNative.mm:112–118`) |
| Argument tables | `Metal4RetiredArguments.h:24–37`: token 단독 소유 뒤 reset, prepared owners에 token 저장, native table도 retain. texture/sampler binding reset과 보장된 buffer overwrites를 구분 |
| Material argument buffers | `Metal4ModelMaterial156.h:79–98`: 새 buffer를 채운 뒤 immutable cache로 사용. current draw의 textures/samplers는 별도 images/source owners 및 lease에 보유 (`Metal4CUSSource.mm:457–466`) |
| Postprocess copy texture pool | `Metal4PostprocessTexturePool181.h:42–43`은 slot sole-owner와 **logical version weak owner 만료 둘 다** 요구. 읽는 lease도 `ImageFor:521`에서 storage token pin, frame는 pins를 append-only 보유 |
| Deferred MRT storage | `Metal4DeferredStoragePool.inc:30,47–49`는 storage owner가 pool만 남을 때 재사용; 모든 request에 `M4DeferredStorageLifetime` pin을 추가. readonly/write-alias도 별도로 borrowed storage pin (`Metal4CUSDeferred.mm:117–121,257–260`) |
| command allocator/residency | frame slot별 별도 allocator/residency. event+feedback 전 reset 없음. async residency commit은 lease 소멸 전에 join (`Metal4RuntimeNative.mm:597–610`) |

prepared CUS owner는 `M4OriginalSourceLifetime`에 넣고 Begin 전에 lease에 retain한다 (`WorldRenderer.mm:4382–4425`). 이 체인은 argument/upload 토큰까지 도달한다. 이후 deferred complete closure 역시 prepared owners를 값/이동으로 보유한다 (`5530–5565`).

따라서 “raw `MTLTexture`/GPU address가 보인다” 또는 “shared_ptr use_count로 pool을 고른다”만으로 UAF/overwrite 결함을 판정할 수 없다. 실제 반례에는 **해당 request가 사용하는 동안 token을 잃는 구체적 callchain** 또는 **CPU/GPU owner가 남아 있는데 pool-only count로 내려가는 경로**가 필요하다. 조사한 위 경로에서는 그런 경로를 찾지 못했다.

이 표는 모든 renderer 가족의 모든 pool에 대한 전수 인증은 아니다. 새 가족을 token graph 밖에 추가하거나 동시 Prepare를 허용하면 다시 확인해야 한다.

## 4. MRTWriteReuse209와 새 roster/shared payload/index

### 현 proof가 실제로 요구하는 것

[MRTWriteReuse209](https://github.com/yeojuny/BAR-Metal4/blob/a33ebfe888bc8abe0053df05e5e2892c0788d6a8/engine/rts/Rendering/Metal4/Metal4MRTWriteReuse209.h#L31)는 sealed normal-main U/F 구조, exact frame/submission/view/allocation/publication, fresh unfulfilled backings, U object/backing의 정확한 structural count 2를 검사한다. count의 의미는 `U.current/published + F.previous` 및 `U.backing + U.current->backing`이다 (`122–127`). 추가 shader reader는 독립 leaf owner를 더해 재사용을 거부해야 한다.

현재 공유 변경이 이 전제를 깨는지 확인했다.

- `NativeCUSSubmissionRoster`는 objects, geometry submissions, uniformBins만 가진다 (`NativeCUSModelInputs.h:24–39`). texture bindings, MRT writer backing, frame pose를 빌려오는 roster가 아니다.
- `NativeCUSProgramUniformBuilder::Capture`는 숫자 uniform maps의 const copy를 공유하고 다음 쓰기는 builder를 바꾼 뒤 sealed cache를 무효화한다 (`NativeCUSProgramUniforms.h:20–32`). MRT texture leaf ownership을 숨기지 않는다.
- 각 actual draw의 `textures`는 `NativeCUSDrawTextureSnapshot`이며 `samplers[12]`와 `wreck[3]`의 `NativeCUSTextureBinding.object`가 강소유한다 (`NativeCUSTextureHistory.h:55–64`, `.cpp:135–142`). roster 공유로 sampler 객체를 writer member에 대한 raw reference로 바꾸지 않았다.
- `NativePostprocessPlan`은 frame root와 숫자 command index/version key를 보유 (`NativePostprocessData.h:164–178`). root를 하나 더 잡아도 root 내부 leaf `shared_ptr` 객체가 복제되지 않는다. 반대로 기존 owning reader record가 사라지는 변경도 아니다.
- `BuildTerrainOwnerRoster`는 `(draw,image)` 인덱스만 저장한다. CUS draw의 image vector와 실제 ownership은 그대로 남는다. `owner.get()`를 처음 만나는 순서, null/empty callback 처리도 기존 dedup 의미를 유지한다.
- `MRTFrontier209::Capture`는 **F 객체만** 보유한다 (`Metal4MRTFrontier209.h:8–25`). U leaf count를 인위적으로 늘리거나 reader를 없애지 않는다.

특히 [DrawDispatch:2397](https://github.com/yeojuny/BAR-Metal4/blob/a33ebfe888bc8abe0053df05e5e2892c0788d6a8/engine/rts/Rendering/Metal4/Metal4WorldRenderer.mm#L2397)은 `allowBeginReady` 요청에서 frontier를 만들지 않는다. 따라서 main의 다음 capture가 진행될 수 있는 pilot은 reuse209 permission 없이 COW로 간다. synchronous direct path의 임의 retirement callback 역시 frontier를 reset한다 (`2423–2425`).

이것은 root 수명만으로 semantic no-reader를 일반 증명했다는 뜻이 아니다. 예를 들어 미래 패치가 실제 shader reader의 sampler object를 `U.current`에 대한 non-owning view로 대체하면, root가 U를 살려 두면서 count 2를 유지할 수 있다. 그 경우 F가 U physical storage를 덮은 뒤 later reader가 U를 읽는 최소 반례가 성립한다. **현재 코드에서 그 변경은 확인되지 않았다.** 미래 refactor의 금지 조건/검증 사례이며 현 결함이 아니다.

### 보존 작업과 Lua의 실제 의미

`PrepareOpaqueModelSource`는 permission을 fresh MRT fulfillment 전에 발급한다 (`Metal4OpaqueModelSource.mm:303–309`). `PrepareCUSDeferred`는 실제 Lua/engine source order를 합친 뒤 readonly/write-alias/COW를 결정한다 (`235–260`). alias가 허용되어도 기존 prior U의 실제 readability를 검사하고 full MRT encoder/draws는 유지한다 (`335–378`). 삭제되는 것은 six preservation copies이지 Lua callback나 F logical version이 아니다.

partial initial clear의 per-attachment discard는 실제 첫 clear와 extent/write-mask proof에 한정된다 (`194–228`; `NativeMRTClearLoad145.h`). 나머지 attachment load/copy는 유지된다. 색상 다섯 개는 동일한 ordinary clear mask에서 함께 결정되므로 `priorColors[0]`로 copy-path 유무를 보는 현재 분기는 그 helper의 전체-color 조건과 맞는다. 임의의 attachment별 색상 discard로 확장하면 이 대표값 검사를 재검토해야 한다.

## 5. Postprocess plan, pending receipts, readability

[NativePostprocessPlan::MatchesRoot](https://github.com/yeojuny/BAR-Metal4/blob/a33ebfe888bc8abe0053df05e5e2892c0788d6a8/engine/rts/Rendering/Backend/NativePostprocessData.h#L171)는 raw address뿐 아니라 control-block ownership도 비교한다. 같은 serial의 prefix/suffix는 새 exact owner이므로 기존 plan을 그대로 재사용하지 않는다. `PrepareFrameImages`는 plan hit여도 dynamic availability 및 lease output registration을 매번 수행한다 (`Metal4PostprocessNative.mm:568–583`). static producer lookup은 first-in-source-order를 유지하며 duplicate error는 runtime frame validation 단계에 남긴다 (`NativePostprocessData.cpp:283–327`).

현재 encoded predecessor borrow는 다음 chain이다.

1. `PreparationInput`은 retired를 먼저 판정한다.
2. 아직 retired가 아니면 overlap lease + exact weak-owner lock + encoded flag를 확인한다.
3. exact version과 그 버전의 `shared_ptr<bool> completed`를 캡처하여 `OrderAfterPreviousSubmission`에 등록한다.
4. successor submit은 predecessor event+feedback+producer callback을 마친 뒤 이 closure를 검사한다.
5. `ImageFor`는 별도로 physical texture residency 및 storage pool token을 pin한다.

따라서 **lifetime, GPU 완료 확인, physical allocation retention을 각각 수행**한다. encoded를 completed로 바꾸는 우회는 없다.

R 후속 `CaptureReadability` (`606–616`)는 cache/renderer raw pointer를 나중에 다시 찾는 callback 대신 exact logical version + encoded/completed flag owners를 반환한다. flags는 atomic이 아니므로 이것을 일반 concurrent producer-state 접근 API로 해석하면 안 된다. 현재 serial render-owner 경로에서 호출되는 receipt라는 제한이 필요하다. 현 코드의 main capture는 이 receipt로 worker state를 동시에 조회하는 경로로 바뀌지 않았다.

## 6. UI/Postprocess/Cloud 장부 cardinality와 취소

세 recorder 모두 두 held frame을 담는 capture-owner 전용 ledger로 바뀌었다. `TryHold`는 capacity/duplicate 확인을 먼저 수행하고, payload의 noexcept construction을 요구한다 (`NativeCompletedFrameLedger.h:19–40`). 장부 자체에는 mutex나 epoch가 없다. epoch와 collective source reservation/notice 검증은 상위 SceneBridge의 책임이다.

- UI `RetireSubmittedFrame(id)`는 exact held id만 retire하고 unknown/duplicate는 throw한다 (`NativeUIRecorder.cpp:841–846`). N의 receipt로 현재 capture의 `targetChanges`를 지우지 않는다.
- Postprocess `OwnsPendingVersion`은 현재 `changes`뿐 아니라 **모든 held frame**을 검사한다 (`NativePostprocessRecorder.cpp:109–115`). 과거 한 slot만 검사하던 제한이 사라졌다.
- Postprocess/Cloud completion은 held id가 없을 때만 해당 recorder의 synchronous pending id를 확인한다 (`PostprocessRecorder.cpp:79–85`, `CloudContext.cpp:73–78`). source ID/serial uniqueness와 epoch 검사가 상위에서 유지되어야 한다.
- failed after-Begin held frame은 `admissionFailed=true`로 quarantine하며 후속 Begin을 거부한다 (`PostprocessRecorder.cpp:75–77`, `CloudContext.cpp:69–71`, UI `837–839`).
- Cloud ledger는 zero source ID를 허용하는 별도 template parameter를 사용한다 (`NativeVolumetricCloudContext.h:42`). UI는 zero를 허용하지 않는다. 이것만으로 cardinality 결함은 아니다. 실제 admission의 unified source identity 규칙과 구분해야 한다.

GPU 1 ticket + CPU 소스 장부 2 slots는 서로 다른 수량이다. GPU pending 1이라는 사실만으로 capture/queued/held source credit 전체를 증명할 수 없고, 장부가 2개라고 두 GPU ticket을 승인할 수도 없다. 해당 상위 reservation·credits·epoch callchain은 병행 상위 감사 범위로 분리했다.

`CanCancelUnsubmitted`는 producer의 non-atomic `encoded`를 읽는다 (`Metal4DeferredColorNative.mm:7`, `Metal4ShieldNative.mm:31`). 그러나 실제 source abort는 renderer access gate를 Try로 먼저 얻고, 얻지 못하면 즉시 false를 반환한다. gate를 얻더라도 `activeSourceSubmission`이 있으면 취소를 거부한다 (`WorldRenderer.mm:2183–2192`). 상위 `AbortPreparedWorld`는 이 false에서 source owner를 보존하고 return한다 (`SceneBridge.cpp:1841`). 따라서 현재 callchain에서 worker write와 무방비 동시 read가 성립하지 않는다. abort 자체가 항상 worker를 join한다고 표현하는 것도 부정확하다. 보호 기제는 **join된 경계 또는 access gate 취득/실패 시 보존**이며 최소 실패 검증으로 유지해야 한다.

## 7. 요청별 texture/mip/atlas 소유권

- World texture journal은 source, physical texture/view, shared upload status를 strong-own한다 (`WorldRenderer.mm:194–221`). `Texture`는 미완료된 이전 first-use producer가 있으면 predecessor를 retire한다 (`602–603`). encoder는 다른 encoded submission을 거부한다 (`611–637`). detached completion은 exact shared status를 잡고 제출 ID를 전체 검사한 뒤 ready를 설정한다.
- Particle atlas journal은 actual lease와 source submission을 묶는다 (`WorldParticles.mm:181–207`, `677–703`). 다른 submission으로 넘어갈 때 미완료 encoded atlas를 거부한다. detached completion은 roster 자체를 move-own한다. 앞 항목이 완료되고 뒤 항목 validation이 실패하는 partial callback 상태는 기존 semantics이며 runtime poison이 필요하다.
- Screen mip journal은 exact retained GPU identity별 lease capsule이다 (`ScreenNative.mm:687–733`). 이전 active lease의 nonempty queue는 뺏지 않고, inactive queue만 순서대로 move-adopt한다. capsule은 runtime lock보다 나중에 선언되어 먼저 파괴된다 (`RuntimeNative.h:232–243`).

screen mip plan에는 아직 모든 producer의 encoded-submission identity가 존재하지 않는다 (`ScreenNative.mm:735–759`). 현재 serial ownership/GPU retirement 경계에서 구체적 overwrite 반례는 찾지 못했다. 하지만 이 구조를 근거로 서로 다른 요청의 simultaneous Prepare/encode 또는 두 GPU ticket을 허용할 수는 없다. 이것은 **확장 전 proof 부재**이며 지금 활성 경로의 확인된 crash와 구분된다.

## 8. P6/pass merge와 이후 depth·stencil·Lua preservation

현재 `CUSSourceImage`/`SourceDependencyResult160`는 owner와 callbacks, `Unknown/NoCommands/CommandsRequired`를 가진다. 일반 producer ID, storage generation, content version, subresource/range, read/write stage, attachment preservation, pending producer submission을 모두 표현하는 sealed access manifest는 아니다.

따라서 현 P6 bookkeeping을 “일반 GPU optimizer 완료”라고 승인할 근거가 없다. 완료된 것은 exact-family receipt/roster 재사용과 보수적인 submit gate 등이다.

현재 pass continuation은 좁게 제한된다.

- `ModelPass160Enabled()`는 empty-pass intervention flag를 통해서만 true (`Metal4Pass160Policy.h:44–54`).
- forward/alpha는 engine/Lua의 `DrawCount()!=0`이면 거부 (`OpaqueModelSource.mm:133–138`, `170–175`). culled indexed draw를 zero callback으로 잘못 간주하지 않는다.
- callback 존재는 NoCommands 증거가 아니며, 알 수 없으면 기존 boundary를 유지한다 (`140–155`).
- color/depth/stencil attachment 모두 Load+Store, no resolve를 확인한다 (`145–149`, `176–181`). main MRT boundary는 physical attachment/subresource/extent까지 확인하고 PostDeferred draw가 있으면 거부한다 (`WorldRenderer.mm:4759–4785`).
- open encoder를 유지할 때 viewport/scissor/stencil reference/cull/depth state/argument table 등 예전 새 encoder의 초기 상태를 재설정한다 (`4705–4745`).
- source Lua/engine draw order, requireReadable, DidComplete는 별도 단계로 남는다. CUS의 sampled roots는 color뿐 아니라 depth/stencil root와도 alias 비교한다 (`Metal4CUSSource.mm:196–203`).

이 구현을 “later depth/stencil/Lua reader가 없으니 Store/Load를 없애도 된다”는 proof로 확장하면 안 된다. 현재 empty boundary 유지와 실제 attachment producer/consumer 재배치는 다르다. P6 다음 변경은 하나의 닫힌 family에서 exact resource-use/effect를 먼저 기록하고, 기존 결정과 비교한 뒤 하나의 copy/barrier/pass만 바꾸는 방식이어야 한다. Unknown은 현재 경로 유지가 맞다.

## 9. 다음 실행 승인 이후 필요한 최소 검증 — 이번에는 실행하지 않음

| 목적 | 최소 사례 | 반드시 볼 결과 |
|---|---|---|
| two-ticket 과거 반례 차단 | flag off/on, pending 0/1/2, 등록 check 없음/있음, 미등록 pending borrow mock | successor commit 전에 모든 predecessor event+feedback+retire 완료; deque 최대 1 |
| 실제 Postprocess prior read | N에서 copy/external output 생성, N+1에서 이전 버전 read와 partial overwrite; N GPU 완료를 지연 | successor submit 전 exact completed flag 검사, 다른 버전 flag로 통과 불가, source/physical owner 유지 |
| GPU failure/quarantine | event timeout, event 후 feedback timeout, GPU feedback error, producer callback 실패 각각 주입 | timeout에서 allocator/residency 재사용 0, 성공 source notice 0, 늦은 callback UAF 0, 후속 admission 거부 |
| pool pins | old frame을 GPU 미완료/실패 상태로 보유하면서 same-layout 새 buffers/argument tables/textures 요청 | busy slot overwrite 0, transient allocation 허용, exact retired owner만 재사용 |
| MRT209 reader witness | PostDeferred/Lua/UI/Shield/SSAO/Bloom이 U retained version을 읽는 경우; sampler-only clone; no-reader 경우 | 실제 reader 있을 때 alias reject, no-reader 기존 gate 유지, requested async pilot은 COW, 색상+depth parity |
| root/index sharing | same-serial prefix/full/suffix, old root 파괴/새 주소 재사용, draw payload 공유 후 후속 uniform 변화 | exact owner plan 분리, dynamic readability 매 호출, old immutable values 유지 |
| ledger와 취소 | 두 held frame + current capture, N exact completion, 중복/wrong-epoch notice; N+1 capture 중 N after-Begin 실패 | N+1 live bindings 유지, 실패한 begun frame rollback 없음, join 전 encoded-state 취소 조회 없음 |
| continuation 보존 | SaveImage prefix 후 same-frame screen suffix, world depth/stencil 사용, later Lua, 최소화/복원 | 원래 prefix가 읽는 logical pixels와 source command sequence 유지, source completion/actual submission 구분 |
| 실제 Metal pass 변경 | empty/nonempty Lua, stencil test/write, depth write, MSAA resolve, attachment alias, readback | 현재 gate 외 조건은 fallback, validation error 0, 고정 장면 color/depth/stencil 결과 비교 |
| 성능 주장 | 같은 executable provenance/flags/assets의 clean paired A/B | accepted/committed/GPU-complete/presented를 별도 ID로 계수. CPU enqueue FPS를 실제 presentation 성능으로 대체하지 않음 |

portable mock/ASan/UBSan의 기존 문서상 성공은 위 실제 Metal 실행 증거를 대신하지 않는다. 새 호스트 테스트도 이 감사 중에는 돌리지 않았다. 실행 허가 뒤에는 전체 suite를 무조건 다시 늘리기보다 변경에 직접 해당하는 실패·소유권 반례와 native 통합부터 확인하는 것이 적절하다.

## 판독 범위

완전 또는 핵심 구현 전체: PLAN 부록 B/C; `Metal4MRTWriteReuse209.h`, `Metal4MRTFrontier209.h`, `Metal4FrameResources.h`, `NativeCompletedFrameLedger.h`, `Metal4PublishedRequestCache.h`, `NativePublishedBufferPool.h`, `Metal4SharedUploadPool.h`, `Metal4RetiredArguments.h`, `Metal4ModelUpload146.h`, `Metal4PostprocessTexturePool181.h`, `Metal4DeferredStoragePool.inc`, `Metal4TerrainOwnerRoster.h`, `Metal4SourceImage.h`, `Metal4Pass160Policy.h`, `NativeCUSSourceData.h`, `NativeCUSProgramUniforms.h`, `NativeOwnedScreenFrame.h`, Postprocess/Cloud recorder의 lifecycle 전체.

관련 구간을 callchain으로 판독: Runtime 생성/slot/lease/residency/Begin/submit/defer/wait/drain/shutdown; WorldRenderer dispatch/MRTFrontier/prepare retention/Begin/encode boundaries/completion/shutdown 및 request journals; CUS source prepare/inspection/dependency/complete; CUS deferred prior/alias/COW/clear/encode/complete; OpaqueModel MRT/forward/alpha continuation; Postprocess plan/availability/ImageFor/readability; UI held journal; Screen mip and particle atlas journal; atomic backing fulfillment 및 current working 변경 diff.

소스만으로 모든 가족의 GPU access graph나 실제 output parity를 전수 증명하지 않았다. 위 구체적인 보완을 인정하면서 P6 및 native acceptance의 열린 범위를 유지하는 것이 현재 증거에 맞는 판정이다.
