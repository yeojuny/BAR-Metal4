# 외부 Metal 사례 후속 검토 대기열

1. D2R 기반 SensorLOS 외곽선 입력 슬라이스는 이 회차에서 단독 평가합니다.
2. D2R의 같은 인코더 내 PSO/depth setter 생략은 EngineModelForward의 두 ordered loop에서 기존 model176·Screen 개선과 겹치는 부분을 먼저 구분합니다. 전달받은 고정 소스의 setter 패턴과 BAR의 실제 반복 빈도를 확인하고 Lua·callback·encoder 재개 경계를 닫은 경우만 별도 후보로 준비합니다.
3. Screen coverage의 컬러·깊이 보존 복사 두 개를 조건부 한 compute encoder에 담는 후보. 이미 묶인 Metal4CUSDeferred·encoder220과 대조합니다. 두 복사, 비별칭, 순서, residency, barrier 의미가 근거입니다. SDL GPU 출처 고정 링크는 확인 대기입니다.
4. DeferredColor 및 CUSOutlineInputs의 불변 sampler 재사용. 기존 Bloom·DoF·PBR 키와 중복을 확인합니다. 대상 device와 의미 있는 descriptor 필드 및 소비 lease 소유가 필요합니다. Dolphin/Filament 출처 고정 링크는 확인 대기입니다.
5. MoltenVK의 identity swizzle 빠른 경로를 참고한 Bloom 13-tap specialization. 실제 identity 사용 빈도와 컴파일러 최적화 여부를 먼저 확인합니다. [고정 원문](https://github.com/KhronosGroup/MoltenVK/blob/5189f8de86ec365860f63bf1da736815cfaea5cd/MoltenVK/MoltenVK/GPUObjects/MVKImage.mm#L2193-L2195)
6. Postprocess의 화면→효과 입력 CopyColor를 일부 1:1 복사에서 native blit으로 대체. 포맷, 정수좌표, 방향, 크기, 비별칭이 정확히 맞는 구간 후보입니다. [Dolphin 고정 원문](https://github.com/dolphin-emu/dolphin/blob/5140482a6f0e59bec1fbe843163f280ed4a2f29c/Source/Core/VideoBackends/Metal/MTLTexture.mm#L24-L45)
7. Bloom·SSAO의 warm PSO cache lookup 전에 반복하는 map 복사와 define 포매팅. 실제 호출 빈도와 full key의 이름·순서·float bit·옵션 계약을 검토 대기 중입니다. Dolphin 사례의 원문 위치는 확인 대기입니다.

8. UI batching 기존 보류 항목. EncodeRange의 연속 index 범위인 solid HUD 삼각형에서 동일 상태·binding·주소 범위와 원래 primitive 순서가 보존되는 경우만 조사합니다. Callback, clear, readback 경계를 유지합니다. 실제 연속 hit와 CPU draw 비용을 확인한 뒤 우선순위를 정합니다. [D2R 고정 원문](https://github.com/PowerBeef/d2r-metal/blob/602db5fc30d4ce50683435cb0745cb171fce2064/dm/gfx/nvn/nvn_mtl.m#L5761-L5769)

9. GroundDecal minimap sample은 현재 R03에서 폭발 decal에만 결과를 쓰더라도 모든 fragment가 수행합니다. 기존 G03의 유한 UV·override·IEEE mix 증명과 중복됩니다. 해당 조건이 해결되기 전에는 별도 신규 구현 후보로 계산하지 않습니다.
10. Screen EncodeRange는 각 범위에서 모든 prepared draw를 돌며 원래 명령 번호로 걸러냅니다. Postprocess screen resume은 좁은 범위를 여러 번 호출할 수 있습니다. 원래 명령 번호가 단조임을 모든 생성 분기에서 확정하면 작은 range lookup 후보입니다. lock, empty range diagnostic, 초기 encoder setters와 barrier를 유지해야 합니다.
11. Grass bend 공유는 같은 데이터를 공유하는 blade 범위와 N이 미확인입니다. 가설로 보존합니다.
12. GameDecal 동일 UV diffuse sample 재사용은 잠정 후순위입니다. 고정 DDS 자산이 전체 13-mip 체인과 맞고 mip bleed padding을 사용합니다. single-mip 전제는 현재 자료로 성립하지 않으며, 실제 런타임 LOD와 implicit gradient 동등성은 미확인입니다.

13. GroundDecal의 zero-glow BlackBody 계산 생략 가설. 유효 UV/layer, atlas·override, 모든 BlackBody 입력/출력 유한성, 음의 0과 원래 동작, SIMD 분기 비용을 닫아야 합니다. RGBA8Unorm 할당 단독으로는 충분하지 않습니다. hit 비중 미확인입니다.
14. Grass bend AABB 가설. 실제 bend count 양수와 AABB 밖 patch가 함께 관측되는지 먼저 확인합니다. SSBO 범위, 음수/비유한 반경, 부동소수 경계, 실제 수식의 보수적 외접 조건과 안쪽 누적 순서를 유지해야 합니다. Grass bend 계산 공유 및 기존 cull-run과 범위가 다릅니다.

15. GameDecal TBN varying 축소 가설. 상수 normal과 XZ tangent를 기준으로 보간/정밀도와 NaN을 유지하며 재구성할 수 있는지, 실제 varying payload가 줄어드는지 미검증입니다. 외부 Filament 고정 원문 확인 대기입니다.
16. GroundFlash 연속 범위 병합. 현 후보 중 slab 이후 첫 검토로 올립니다. R02 encoder 9의 api5549–8261: 한 프레임에서 227 Triangle draws, 각각 UInt32 index 6개, 같은 index buf2499에 24B씩 연속 offset, instanceCount 1. rps54/dss1과 VBO/texture는 반복되지만 slot 1 UBO는 buf2500–2726입니다. Raw pointer 정보로 값 동등성은 증명되지 않습니다.
17. GroundFlash와 별도로 AppendNativeParticleGeometry가 각 append에서 정확한 용량을 reserve하는 부분의 growth 후보. 그 범위의 source span alias 복제, uint32 상한, 세 벡터를 논리 변경 전 reserve하는 실패 동작, 동일 렌더 출력 계약을 보존해야 합니다. 병합과 같은 실험에 넣지 않습니다.

18. LuaVBO 생산 CPU flat loop 가설. headlessGLStubs의 glBufferSubData는 GPU 업로드를 하지 않으므로 GL mirror 가설을 제외합니다. 모든 필드가 연속 float32이고 padding이 없는 경우만 기존 SafeCast, validation 시점과 dirty snapshot을 유지한 평탄화를 검토합니다. 실제 빈도·비용은 미확인입니다.

19. NativeBufferData validated capacity 재사용. Define 성공 후 소유권·실패·move/delete 계약과 매 span/offset/revision 검사를 유지하면서 immutable layout의 속성 쌍 검증만 줄일 수 있는지 검토합니다. raw 검증 API와 변경 가능한 layout에는 적용하지 않습니다. 현재 반복 빈도·CPU 비용은 미확인입니다.

## 우선순위를 바꾼 근거

**GroundFlash 우선.** GL ProjectileDrawer는 같은 depth test/write run에서 이어 그립니다. Native는 가시성 검사 후 flash마다 Clear→Draw→Append합니다. Metal Prepare는 range별 uniform, argument table 및 depth 준비를 반복하고, EncodeDraw는 range마다 상태와 draw를 발행합니다. 현재 코드에서 uniform은 frame·view 공통값과 range의 stage/soften/alpha로 결정됩니다. 실제 범위 전부의 동일성을 이 계약으로 입증해야 합니다. Candidate 접근은 각 flash의 Draw와 Append 검증을 유지한 뒤 동일한 연속 range metadata만 합치는 생산 경계가 우선입니다. 불가능하면 독립 Metal Prepare가 대안이고 둘을 함께 적용하지 않습니다. 기존 index logical bounds와 geometry/순서를 보존합니다. 227→1과 FPS 이득은 실측 전 미확인입니다. 당시 shader 비용은 FS 0.157%, VS 0.03383%로 낮아 이 후보의 주 대상은 CPU Prepare·API encode입니다. 현재 전투에서 반복 횟수는 미확인입니다.

- EngineModelForward는 model176이 이미 줄인 cull/fill과 별개로 매 prepared draw에서 PSO/depth를 여전히 설정합니다. 기존 R02 shader export에는 NativeEngineForwardGL4/Legacy 행이 없어 타깃 반복 횟수를 묶을 수 없었습니다. Round 41 clean log에도 model176 전투 계수가 없습니다. phase-local state를 쓰고 Lua·callback·command에서 지우는 경계는 설계 가능하나 측정 우선순위는 내립니다.
- BloomDownsample에는 실제로 13-tap 동적 uniform swizzle이 남아 있습니다. 과거 R02 export에서 5 draws와 상대 fragment 비용 0.42596%를 확인했습니다. GPU 경로 후보 가운데 좁고 재현 가능한 쪽이지만 현재 identity 빈도와 이득은 미확인입니다. CopyColor는 당시 DOFCopyColor 1 draw, 0.06040%여서 우선순위를 낮췄습니다. 해당 비중은 과거 한 프레임의 상대 비용입니다.
- Screen copy 두 분기는 장벽 구조가 같고 CUSDeferred의 인코더 하나에 여러 복사하는 선례를 확인했습니다. 두 버전 복사가 같은 Screen target에서 함께 발생한다는 전투 증거가 필요합니다.
- DeferredColor와 CUSOutlineInputs는 allocation마다 sampler를 생성하는 소스 경로가 남아 있습니다. 두 변환은 비mip, 비comparison, anisotropy=1의 제한을 먼저 적용하고 같은 주소/필터/경계색을 번역합니다. 실제 해당 소비자 호출과 반복이 관측될 때만 좁은 descriptor cache 후보로 순위를 올릴 수 있습니다.
- GroundFlash 다음은 Screen EncodeRange 방문 범위와 BloomDownsample identity 특수화를 비교합니다. 다른 목록은 실제 호출이나 의미 보존 증명이 필요한 상태입니다.

RTT depth Store 생략은 현재 평가 범위에서 제외합니다. 항목 2–19은 별도 평가 전까지 FPS 미확인입니다.
