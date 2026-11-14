> Publication copy: original conclusions and historical version scope preserved. Personal absolute paths were neutralized; source links are pinned to the audited a33ebfe revision. Raw logs, JSON manifests, binaries, screenshots and the original plan are not included in this Markdown-only export. See the parent README for later runtime results and limitations.

# CPU source 계약 독립 감사

감사일: 2026-11-13. 읽기 전용 소스·기존 기록 검토. 새 게임, 빌드, 테스트, Git fetch/checkout/reset은 실행하지 않았다. 이 보고서만 새로 작성했다.

검토 대상은 `LOCAL-SOURCE`의 working files이다. 부모 감사에서 HEAD `6e9ce8f89afb14d632cce33e4742bab11b76b60f` 대비 19 production 변경이 원격 `a33ebfe888bc8abe0053df05e5e2892c0788d6a8`와 byte-exact임을 확인했다. 그 근거는 같은 evidence 디렉터리의 `remote-working-file-map.json`이다. 후속 원격 문서/runner는 `remote-files/`에서 읽었다. 계획은 `PLAN-original.md` I부 T1–T4/P1-B/P1-C/P2, II부 해당 CPU 분석, 부록 A 전체를 대조했다.

## 판정

검토한 **현재 production CPU 경로에서 실행 가능한 새 차단 결함은 확정하지 않았다**. Terrain writer, immutable roster, source selection, Postprocess index 및 R5의 한정된 변경은 계획의 핵심 반례를 고려하고 있다. 이것은 전체 제품 승인이나 runtime parity 판정이 아니다. P1-B/P2 전체를 완료로 볼 수 없으며, 데이터 표현이 줄인 작업과 뒤로 옮긴 작업을 구분해야 한다.

- T1/T2/T3와 Terrain typed source base의 주요 계약은 구현되었다. 변경되지 않은 setter의 할당 제거, link proof 공유, 관측까지 publication 지연은 실제 생산자 변경이다.
- CUS roster·uniform-content·manifest·read-key·phase selection 공유는 실제 중복 materialization을 줄인다. full draw structural seal, CUS typed packing, engine constant content identity 공유는 남아 있다.
- Postprocess는 exact active root당 정렬/index를 공유한다. 전체 수명 동안 snapshot당 한 번, static overwrite proof 공유, exact lease 내 output preparation 중복 제거까지 구현된 것은 아니다.
- census는 준비 시 한 번 수행하여 다음 phase들의 중복 set을 없앤다. census 자체가 없어지지는 않는다. 현재 CUS 위치는 trace timing 안에 포함된다.
- mac compile patch 9파일에서 현재 호출 범위의 의미 변화는 찾지 못했다.

## 1. Terrain T1–T4

### 보존된 경계

`NativeTerrainProgramSource.cpp:269–334`를 실제 production writer 기준으로 읽었다.

1. `Write`는 context/bound 검사 후 `location==-1 || count==0`를 반환하고, 그 밖의 location/count/input, current ownership, revision overflow, extent overflow, finite 검사를 수행한다(`271–280`). exact no-op은 그 뒤이다. count0를 일반 shape 검사로 보내거나 NaN/Inf를 no-op으로 통과시키지 않는다.
2. variant type, rows, columns, count, transpose, 전체 원소 수를 맞춘 뒤 원소별 `memcmp`한다(`284–295`). `+0/-0`, array 뒷부분, matrix transpose, integer/float variant가 구분된다. 비교용 임시 vector/map은 없다.
3. 값이 같아도 canonical binding kind/consumer와 pending 또는 `MatchesCurrentPublication`을 확인한다(`296–309`). `Current(A) → Write(B) → Restore(A) → Write(B)`는 stale fallback으로 revision을 올리고 다시 publication을 맞춘다. `OwnsCurrent`만으로 skip하지 않는다.
4. lazy writer는 첫 changed write에서 map을 복사하고(`326–329`), 외부로 관측되지 않은 dirty map만 제자리 변경한다(`319–324`). `Current/SaveCurrent`에서 seal하고, 이미 저장된 Selection에는 pending callback을 넣지 않는다. `Restore`는 pending selection만 지우며 writer를 되감지 않는다(`NativeSourceProgramBinding.h:46–66`).
5. reload/import/release 전에 pending을 seal한다(`SourceProgram`의 `SealBeforeMutation`, `74`, `115`, `152`). `CompileRequest:241–245`의 `try_emplace`는 기존 inactive 이름 값을 지우지 않는다.
6. deletion 후 restore/capture는 provenance raw pointer를 먼저 live list에서 찾고 정확한 publication과 대조한다(`374–395`). 이미 죽은 포인터에 dynamic_cast부터 실행하지 않는다. address reuse도 program generation/definition 대조로 거절된다.
7. `LuaShaders.cpp:887`과 `LuaNativeUnitGlass.cpp:70`의 실제 saved-state 경계가 `SaveCurrent`를 사용한다. 일부 단순 kind 검사들은 `CurrentKind/InspectCurrent`로 분리되었다. 모든 `Current`가 제거된 것은 아니며 다른 family의 실제 owning observation은 남는다.

### link certificate와 mutable PSO

`NativeTerrainSourceData.h:58–93`의 certificate는 private validating factory만 발급한다. immutable consumer에는 retained owner와 context/library generation을 확인하고, generic consumer에는 full validation으로 돌아간다.

`Metal4TerrainSourceGPU.mm:265–317`의 `OriginalProgram`은 실제 초기 library/PSO 성공 후 `SealInitialLink`를 수행한다(`689–706`). `LibraryGeneration/HasImmutableDefinition/Supports`는 그 certificate를 읽으며 mutable `imageLibraries/pipelines`의 emptiness를 main source validation에서 읽지 않는다. target format/MSAA/image type 및 pipeline specialization은 `Pipeline/Prepare`에 남아 있다. 이 점은 계획의 P4 선행 조건도 충족하는 구체적인 개선이다.

다만 `MatchesCurrentPublication:130–132`에는 정의 포인터가 같아도 identity의 문자열/defines 전체 비교가 여전히 있다. `OriginalValid/Validate`에도 full definition 검사가 남는다. T2를 모든 identity 비교 제거라고 설명하면 과장이다.

### typed base와 비용

`NativeWaterGroundSourceData.h:30–137`은 44개 고정 ABI slot을 만들고 source-owned float4/int4, shadow matrix의 성공한 변환을 보관한다. unknown/inactive 이름은 map에 남는다. map copy/assignment는 포인터 index를 무효화한다. writer의 single-slot 변경은 typed bytes만 재사용하고 변경 slot을 무효화한다. `SealSlots`는 throwing 검증을 추가하지 않으며 부적합 shape/type/nonfinite는 기존 GPU reader로 돌린다.

GPU `Float/Int/Matrix`의 packed 경로는 기존 reader 호출 위치에 있다(`Metal4TerrainSourceGPU.mm:397–445`). `DrawUniform:450–499`의 camera/model-view/projection/inverse, target clip convention, alpha/fog/clip 및 image transport는 매 draw의 실제 입력으로 처리된다. simulation frame 또는 uniform owner 하나로 전체 block을 잘못 재사용하지 않는다.

남는 작업은 중요하다. map의 첫 changed write 복사, source slot pointer 재연결, typed bytes 복사, 최종 GPU block 작성 및 view별 inverse는 계속 있다. square마다 관측하고 `texSquare`가 바뀌면 매 square의 map copy도 남는다. 기존 typed-base receipt의 host sizeof는 432→1424 bytes(+992)이다. 메모리·복사량 증가와 reader 비용 감소 중 어느 쪽이 실제 workload에 유리한지는 기존 source tests로 결정할 수 없다.

### 한정된 API 경계 문제 — production 차단 결함으로 분류하지 않음

`SourceProgram::Write:305–306`의 aligned no-op은 `318`의 definition validation보다 먼저 반환한다. `HasImmutableDefinition()==false`인 외부/custom consumer가 같은 context/library generation에서 `Supports`를 true→false로 바꿀 수 있다면 다음 반례가 성립한다.

```
generic consumer로 link/enable
Write(value=1)와 Current 저장
consumer.Supports = false  // owner와 source identity는 동일
Write(value=1)            // 현재는 no-op 성공, 과거 eager Publish는 validation에서 실패
CaptureCurrent           // 여기서는 뒤늦게 실패
```

이는 header가 명시한 generic/imported consumer의 live-validation fallback과 맞지 않는 좁은 오류 시점 차이다. 정적으로 가능한 API 반례이며 실행하지 않았다. 현재 tree의 실제 구현은 `OriginalProgram` 한 종류이고 성공 후 immutable certificate를 제공하므로 이 반례가 현재 게임 경로에 도달한다는 증거는 없다. 안전한 보완은 nonimmutable linked consumer에만 aligned no-op 전에 기존 validation을 실행하는 것이다. 최소 검증은 generic mutable consumer fixture에서 동일 값/다른 값의 accept/reject 및 오류 시점을 비교하고 immutable no-op의 zero allocation을 유지하는 것이다.

## 2. CUS roster·pose·event·selection

### 실제 제거된 생산자 작업

- `LuaNativeCUSSource.cpp:107–120`: AddOne은 model/bin을 실제 Add 시점에 확보하고, 세 mutable vector의 성공한 commit 뒤 즉시 `roster.reset()`한다. Add(table)의 뒤 원소가 실패해도 앞 committed prefix는 캐시를 무효화한다. rollback 후의 실패 원소는 이전 cached roster를 훼손하지 않는다.
- `126–128`: Remove는 기존 swap-with-last 순서를 세 배열 모두에 보존한다. Clear/Delete는 cached roster를 해제한다. 이전 draw의 shared roster는 남는다.
- `134`: 첫 Submit만 `NativeCUSSubmissionRoster::Seal`한다. factory(`NativeCUSModelInputs.cpp:18–45`)는 object/submission identity와 전체 geometry index-count를 확인하고 목록을 한 번 소유한다. roster에는 frame-specific instance snapshot, pose publication, uniforms, texture/GPU owner가 없다.
- `137–144`: Submit마다 instance snapshot, texture binding, world sequence, model publications, engine uniforms는 따로 capture한다. renderer가 나중 live unit lookup으로 대체하지 않는다.
- `NativeCUSTextureHistory.cpp:129–142`: roster 경로의 texture snapshot은 object-order vector를 다시 복사하지 않는다. owning sampler records는 그대로 보관한다. eventSequence의 Advance와 별도 `NextNativeWorldDrawSequence`는 유지된다.
- `NativeCUSModelInputs.cpp:73–80`: per-event wrapper allocation은 남지만 submission vector copy/geometry-piece 재합산은 제거되었다.

`NativeCUSDrawRuns147.h:18–27`은 adjacent 동일 geometry/index first/count만 합치고 원래 first offset을 consumer에 전달한다. `Metal4CUSSource.mm:475–479,243`은 그 first를 baseInstance로 사용한다. source roster 정렬이나 instance rebasing은 없다. Add 시점의 model/bin retention으로 destroyed-object shadow 경로를 보존한다.

### read-set identity와 skinning

`NativeCUSCPUReadScope.h:52–65`의 key는 `{program, roster, transforms, modelUniforms, engineUniforms, instances}`이며 lookup 결과에는 raw pointer뿐 아니라 control-block owner 대조가 있다. 새 ModelInputs wrapper 주소 때문에 생기던 miss는 제거된다. roster span을 쓰는 동안 packet/draw가 owner를 유지한다.

`NativeCUSRenderInputs.cpp:66–117`에서 family, observed rows, team255 sentinel, interpolation, scale sign/zero branch, first bone low byte, 원래 bi=1,2 loop를 유지한다. rigid FirstBones geometry cache도 유지된다. params publication을 key에 넣어 skinning interpolation/observed-team mask를 빠뜨리지 않는다. byte/entry budget 때문에 계산 후 미보관·fallback 재계산이 가능한 기존 경로는 남는다(`NativeCUSCPUReadScope.h:22–37`).

### P2의 구현과 미완료

`NativeCUSProgramUniforms.h:23–32`는 exact byte change에만 immutable uniform content를 다시 만든다. 같은 roster의 여러 Submit에서 uniform map copy가 반복되지 않는다. Lua setter 자체의 임시 value vector 생성(`LuaNativeCUSSource.cpp:75`)은 여전히 수행된다.

source manifest는 load/reset 변경 때만 재seal한다(`239,252–254`). phase selection은 `NativeCUSSourceData.h:74–83`에서 선택된 event envelope만 복사하며 parent frame을 잡지 않는다. ground deferred는 `NativeWaterGroundSourceCapture.cpp:118`, forward는 `Metal4WorldRenderer.mm:3635–3636`이 사용한다. deferred가 없으면 원래 immutable frame을 직접 재사용한다. 제외된 반대 phase texture/instance/pose owner를 selection 자체가 pin하지 않는다.

**P2 미완료 항목:**

1. `NativeCUSSourceDraw`는 public aggregate이며 full structural draw proof owner가 없다. `NativeCUSSourceData.cpp:28–59`의 `ValidateNativeCUSSourceDraw` uniform 전수검사는 반복된다. Submit `LuaNativeCUSSource.cpp:144`, CPU reads `NativeCUSRenderInputs.cpp:68`, GPU `Metal4CUSSource.mm:411`, deferred finish/prepare 호출이 남는다. roster O(N) identity 비교는 O(1) owner 비교로 줄었지만 전체 검증이 seal당 한 번 된 것은 아니다.
2. `NativeCUSProgramData.cpp:27–55`는 두 shader stage의 reflected field마다 이름 lookup, 값/shape/type 검사 및 packing을 계속한다. CUS source typed base+late transport 분리는 아직 없다.
3. `NativeCUSUniformData.cpp:19–57`의 engine matrices/params publication allocation/copy는 유지된다. source uniform builder 공유와 이 engine constant 경로는 다른 작업이다. RNG/원래 writer를 생략한 잘못된 최적화는 확인되지 않았다.
4. selection은 map/roster deep copy를 제거했지만 event envelope 자체의 vector push/copy는 남는다. read-set key도 whole engineUniforms owner를 사용하므로 매 matrix/params publication이 내용상 불필요한 miss를 만들 수 있다. 이것은 안전한 보수적 key이며 correctness 결함이 아니다.
5. legacy texture capture overload는 남지만, 원래 vector 기반 full SourceDraw validator를 독립 비교 oracle로 그대로 보존한 경로는 확인하지 못했다. 현재 fixture의 mutation wire 검사는 Lua VM 실행이 아니라 source assertions이다. 계획의 완전한 accept/reject 비교·실제 partial Add 통합 증거는 미완료이다.

## 3. Postprocess exact-root index와 runtime readability

`NativePostprocessPlan`은 exact shared frame root를 소유하고 pointer/control-block 모두로 비교한다(`NativePostprocessData.h:164–182`). scalar `(image.object,generation)`→command reference map은 leaf version/backing shared_ptr를 더 만들지 않는다. 같은 serial의 prefix/suffix라도 root가 다르면 재구축한다. `SnapshotPrefix`가 별도 frame을 만드는 사실(`NativePostprocessRecorder.cpp:40–46`)과 맞는다.

`NativePostprocessData.cpp:283–328`의 첫 structural order build, runtime availability 순회, duplicate producer 거절 순서를 확인했다. producer map은 first-in-source-order를 저장하며 duplicate는 원래 command의 input availability callback 뒤 runtime frame validation에서 거절한다. consumer의 actual stage/sequence/offset 및 same-offset commandSequence `<` 검사는 유지된다(`356–381`). retired/CPU-content fast path도 유지된다.

`Metal4PostprocessNative.mm:560–583`은 plan을 exact current root에 재사용하지만 `PreparationInput`, output registration, `ImageFor`는 매번 수행한다. plan은 준비 성공 후에만 설치한다. 초기 frame preparation과 후속 Prepare(`636`)가 같은 active root이면 sort는 반복하지 않는다. index consumer lookup에는 full sort가 없다.

다음은 아직 남는 계획 범위이다.

- **single active-root cache**이므로 A→B→A root 전환 시 A의 index를 다시 만들 수 있다. “snapshot 전체 수명당 정확히 한 번”이라는 절대적 주장은 불가하다. 현재 README도 이 한계를 명시한다.
- `ValidateNativePostprocessFrame(plan)`은 매 준비마다 produced set을 만들고 inputs/previous를 순회한다. `Key`에 따른 descriptor 검증도 남는다.
- static overwrite proof는 `PrepareFrameImages:580`에서 재실행한다. exact lease 내 동일 output preparation dedup은 없다.
- runtime readability나 실제 GPU 성공을 plan에 seal하지 않는다. 이는 빠진 최적화와 별도로 반드시 보존할 안전 조건이다.

`CaptureReadability:606–616`은 준비 시 exact version/cache entry를 확인하고 version 및 해당 encoded/completed flag owner를 capture한다. receipt 생성 시 readiness를 요구하거나 완료를 publish하지 않는다. 실행 시 `completed || encoded`를 다시 검사한다. CPU contents 경로는 version만 보관한다. 실제 세 연결은 `Metal4WorldRenderer.mm:764,1089,4299`이다. 기존 cache lookup callback을 exact flags로 바꾸었지만 GPU allocation retention은 ResolveImage/ImageFor lease 경로가 맡는다. predecessor permission은 `PreparationInput:483–499`의 exact version completion check와 별도이다. 이 변경을 “모든 provider가 immutable” 또는 동시 flag 접근 안전성 증명으로 확대할 근거는 없다.

## 4. R5 census·companion transaction

`Metal4TerrainOwnerRoster.h:12–20`은 원래 nested order에서 `owner.get()`의 첫 occurrence 인덱스만 seal한다. null/empty callback의 첫 occurrence도 보존한다. engine forward/deferred는 기존 정책처럼 null을 제외한다(`BuildGroupedOwnerRoster` 포함). image/owner 추가 복사가 없고 vector 이동 뒤에도 index로 찾는다.

| family | seal 위치 | runtime 동작 보존 |
| --- | --- | --- |
| Terrain | `Metal4TerrainSourceGPU.mm:853` | dependencyAttempted는 callback 전에 설정(`627`), retry 금지 유지. readiness `640`은 실제 draw마다 수행 |
| CUS | `Metal4CUSSource.mm:492` | inspection/encode/completion은 same first occurrence. encode 실패 후 dependenciesEncoded=false로 retry 가능(`213–220`), 완료 실패 flag도 보존. readiness `226`은 모든 draw image에서 실행 |
| Engine forward | `Metal4EngineModelForward.mm:202` | null 제외, physical root 수집은 모든 image에서 계속하며 query는 phase-live callback |
| Engine deferred | `Metal4EngineModelDeferred.mm:148` | before/after를 이어 붙인 first occurrence index; 실제 before/after draw/readability 유지 |
| Grass / ResourceSpots | 각각 `108` / `116` | 기존 per-draw readability 유지, census만 공통 helper 사용 |

R5는 CUS census를 trace `prepareCPUus` 기록보다 앞으로 옮겼다(`492`→`493`). 따라서 census 비용을 해당 timing에서 숨기는 이전 위치 문제는 현재 수정되었다. 준비 시 1회 set/vector allocation은 남고, phase마다 발생하던 반복 set 구성을 제거한 것이다. 할당 실패 시점은 Encode/DidComplete에서 Prepare로 이동하며 문서도 이를 의도한 변화로 명시한다. 이를 “모든 exception timing 동등”으로 쓰면 틀리다.

`NativeSourceResourceCache.h:20–28`의 companion transaction은 create→prepare→noexcept commit→source/resource publish 순서이며 hit도 prepare/commit을 거친다. 실제 map caller `Metal4WorldRenderer.mm:865–913`은 tile map을 stagedMap으로 만들고, 신규 또는 기존 height/normal pair와 tile allocations의 lease retain을 `904–908`에서 마친 뒤 `909–912`에서 tile owner를 commit한다. preparation 실패 때 tile만 새로 publish하거나 cache hit에서 companion commit을 건너뛰는 경로는 확인되지 않았다. lease에 일부 retain이 들어간 뒤 failure가 나면 기존 lease cleanup 경로가 필요하며, 이 helper 자체가 전체 renderer transaction을 증명하지는 않는다.

## 5. mac-compile-fixes.patch 9파일 의미 대조

| 파일군 | 변경 | 판정 |
| --- | --- | --- |
| NativeDeferredColorData `.h/.cpp` | `atomic<shared_ptr>` → private shared_ptr + atomic free load/CAS | acquire load, release success, relaxed failure, empty expected의 one-shot fulfill, reject null/duplicate 유지 |
| NativeMapTexturePublication `.h/.cpp` | 같은 변경 | Resource load는 여전히 값으로 owned snapshot 반환. patch 내부에 nonatomic 동시 read/write 경로 추가 없음 |
| NativeShieldDepthBacking `.h/.cpp` | 같은 변경 | failure 때 기존 resource를 덮어쓰지 않으며 동일 logic_error 조건 유지 |
| LuaNativeCUSSource `.cpp:188` | 제거된 floats/ints 직접 clear → `uniforms.Initialize({},{})` | builder tables와 cached sealed owner를 함께 정리. 이전 captured draw가 보유한 uniform content는 유지 |
| NativeVolumetricCloudContext `.cpp` | `<exception>` 추가 | terminate 선언 직접 확보; 실행 동작 변경 없음 |
| Metal4OpaqueModelSource `.h:62` | friend 마지막 인자를 `const MRTFrontier209*`로 정정 | 실제 공개 declaration `67–72`와 일치. 새 runtime overload를 추가한 변경 아님 |

세 backing의 resource는 여전히 private이며 검색한 access는 atomic free 함수로 통일되어 있다. 이식 patch가 public 복사 가능성/lock-free 성질까지 원래 template과 동일하다는 전면 주장은 하지 않는다. 현재 production의 read/fulfill 동기화 의미에서 차이는 찾지 못했다.

## 6. 기존 증거와 default-OFF 비용의 한계

이번 감사는 기존 source tests를 **읽었고 재실행하지 않았다**. 저장된 receipt는 다음 범위로 해석해야 한다.

- T1의 29 checks receipt는 현재 writer SHA와 다르며 당시 revision 증거이다. T4 typed-base receipt의 writer·snapshot SHA는 현재와 일치하지만 Terrain GPU translation unit SHA는 이후 census 변경 등으로 다르다. SIMD reader stand-in 검사는 실제 Metal/target PSO/visual proof가 아니다.
- Postprocess plan 38 checks receipt의 기록된 네 `.cpp`/fixture hash는 현재와 일치한다. 이 receipt가 dependency header와 Objective-C++ integration 전체를 고정한다는 뜻은 아니다.
- CUS uniform builder receipt hash는 현재와 일치한다. CUS roster 48 checks receipt의 SourceData와 fixture는 현재 hash와 다르며 나머지 기록된 CPU translation units는 일치한다. 오래된 receipt를 현재 전체 tree 통과로 표시하면 안 된다.
- 원격 readability runner는 실제 method body를 추출하여 modeled version/cache/flags를 검사하고 세 callsite를 source assertion으로 확인한다. 10 scenario라는 문서 기록은 실제 GPU 또는 concurrent flag 접근 테스트가 아니다.
- CUS roster runner는 실제 phase body를 추출하고 randomized/null/alias/callback/retry/lifetime/allocation-failure fixture를 둔다. Prepare 전체의 Objective-C++ execution이나 native resources validation을 대신하지 않는다.

`BAR_NATIVE_TERRAIN_WRITE_STATS`는 `#ifdef`로 컴파일 시 제거되므로 ordinary build에서 setter 통계 storage/increment 비용이 없다(`NativeTerrainProgramSource.cpp:18–23`, `.h:49–58`). `TakeNativeTerrainUniformWriteStats`의 실제 사용은 해당 host fixture에서 확인되며 production per-draw-frame 보고 연결은 찾지 못했다. 따라서 실제 게임의 aligned-noop율/map-copy/publication 수는 이 소스만으로 알 수 없다.

반면 기존 preparation/model trace는 runtime opt-in이다. OFF여도 TLS/global lookup·branch와 일부 카운터 reset/집계는 남는다(`Metal4PreparationTrace.inc:56–58`, `Metal4ModelTrace.inc:35–38`, `Metal4CUSSource.mm:391–395,408–409,421,481`). disabled 상태에서 draw/texture full trace traversal은 하지 않는다. “새 T1 counter는 zero ordinary-build cost”와 “모든 기존 진단이 OFF이면 zero cost”는 다른 주장이다.

## 7. 최소 후속 검증안 — 이번에는 실행하지 않음

1. 현재 정확한 tree로 source counter/eager-observation/typed-reader fixtures의 receipt를 갱신한다. generic nonimmutable consumer 반례는 별도 낮은 우선순위 API 검사로 추가한다.
2. Lua VM이 실제 `Add({valid,invalid})`, Remove swap, Delete, destroyed-object shadow, unchanged repeated Submit을 실행하게 하여 captured old draw와 event 순서 및 originalIndex/baseInstance를 비교한다. source substring assertion만으로 통합 수락하지 않는다.
3. 실제 frame의 prefix/full/suffix/SaveImage와 취소·실패·retired input을 통과시켜 root별 order-build 수, lookup sort 수, lease registration 수를 기록한다. same-offset commandSequence equality/역전은 fail-closed로 확인한다.
4. T1 setter calls/changes/noops/stale fallback/maps/publications, CUS roster seals/copied elements/full validations/pack calls, Postprocess build/lookup/prepare 수, retained snapshot bytes를 동일 workload에서 수집한다. census 비용은 Prepare timing 안에 둔다.
5. 부록 A의 rigid/skinning branch·team255·observed row·interpolation/scale 반례와 excluded phase owner release를 현재 packet/worker integration에서 확인한다. 독립 CPU 증거와 app/Metal 증거를 분리한다.

이 CPU 범위는 P1-A/P1-C와 P1-B/P2의 실제 부분 구현을 뒷받침한다. 나머지 P2 seal/packing, 전체 source-family 수명, GPU proof, P5 active-work overlap·동일작업 성능은 별도의 미완료 항목이다.
