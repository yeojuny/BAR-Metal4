# 저위험 묶음의 포함 범위

| 분류 | 대상 | 코드에서 닫힌 범위 또는 남은 조건 |
|---|---|---|
| 포함 | Sensor31657 레이더·소나 원 | fresh read-only 입력 3개를 인덱스 2·3·5의 슬라이스로 묶습니다. 인덱스 0·1·4는 기존 snapshot이고, 4의 기존 circles.offset을 그대로 더합니다. 실패 시 원래 순서로 개별 업로드합니다. |
| 포함 | forward MapEdge | shader argument 0·1의 uniform/mirror를 묶습니다. depth state, 높이·색상·normal·sampler 수명, strip 계획과 deferred 경로는 기존대로입니다. |
| 포함 | Blueprint 모델 및 Blockout 모델 | 각 모델의 transform/uniform 인덱스 1·2만 묶습니다. mesh/cache, 인덱스 버퍼와 길이, material, draw·depth 순서는 그대로입니다. Blueprint outline의 기존 packed 입력은 그대로입니다. |
| 보류 | Spotlight, Shield, Paralyze, Orb/Airjets | Spotlight와 Orb/Airjets는 기존 할당·검사 순서가 두 입력 사이에서 갈리고, Shield에는 소비되는 impacts 길이와 미바인딩 분기가 있습니다. Paralyze는 공유 uniform을 복제하지 않는 row별 독립 계약과 활성 확인이 남습니다. |
| 보류 | UI Panel, shadow-color/원래 shadow particle, NapalmPreview, water | dummy/미바인딩·실제 입출력·vertex/index 논리 길이와 해당 경로 활성 확인이 남습니다. main WorldParticles Upload177과 Screen 기존 업로드 풀은 중복 대상입니다. |
| 보류 | immutable sampler 및 NativeBufferData capacity, Bloom/SSAO PSO lookup, Screen range scan | device/key/lease, 실패 및 move/delete, 정의 bit·순서, 정렬과 lock/empty 계약을 각각 닫고 호출 빈도를 확인할 필요가 있습니다. |
| 별도 | GroundFlash 및 UI draw batching, PSO/depth encoder cache, Screen copy/blit, shader/coverage 계열 | 그리기·producer·encoder·정확한 shader 결과에 관한 경계를 변경합니다. 기존 증거는 각 개별 후보에만 적용됩니다. |

포함 경로는 서로 다른 prepare에서 같은 함수와 조건을 쓰지만 slab를 서로 공유하지 않습니다. 각 할당은 fresh이며, exact bytes와 전체 슬라이스 bounds를 확인한 뒤 lease에 등록합니다. 소유권과 residency는 GPU 완료까지 lease가 유지합니다. capacity, 정렬 또는 실제 할당에 실패하면 각 호출부의 기존 per-buffer 할당으로 돌아갑니다. 기존 19개 라이브러리, 통합 인덱싱 ON, 별도 SensorLOS slab 및 GroundFlash OFF를 양쪽에 고정합니다.

참고한 제출 단위 슬라이스 발상은 Patrice Déry(PowerBeef)의 D2R Metal 원문과 Round 44에서 구현한 BAR의 기존 소규모 진단입니다. [고정 원문](https://github.com/PowerBeef/d2r-metal/blob/602db5fc30d4ce50683435cb0745cb171fce2064/dm/gfx/nvn/nvn_mtl.m#L3948-L3959)
