# SensorLOS 외곽선 fresh upload slab

**OFF 65.39 FPS → ON 65.17 FPS (-0.34%).** 같은 바이너리와 같은 전투에서 GameDecal과 5개 경로의 인덱싱을 모두 켠 상태로 slab만 바꿨습니다.

| 지표 | slab OFF | slab ON |
|---|---:|---:|
| 1% low | 55.21 FPS | 36.34 FPS |
| 중앙값 / p95 / p99 | 15 / 17 / 17 ms | 15 / 17 / 17 ms |
| 최대 프레임 타임 | 29ms | 266ms |
| 100ms 초과 | 0 | 1 |

첫 외곽선 인덱스 드로우는 OFF에서 separate (측정 구간 밖, tick 1), ON에서 slab (측정 구간 밖, tick 29)로 기록했습니다. slab에 성공한 외곽선마다 위치·유니폼·색상 업로드는 3개에서 1개로 줄어듭니다. 장면 전체의 할당 횟수·시간은 계측하지 않았습니다. ON에서는 266ms 프레임 한 개가 나왔고 모든 통계에 포함했습니다. ON의 측정 전 구간도 약 28.73초 더 길었습니다. 두 관찰의 원인과 slab의 영향은 이번 한 쌍으로 분리할 수 없습니다.

위치·유니폼·색상을 256바이트 정렬한 별도 주소에 복사했습니다. 용량·오버플로·GPU 주소 범위를 확인하고 lease가 기존 제출 완료까지 버퍼를 강하게 소유하며 residency를 등록합니다. slab를 만들 수 없으면 기존 세 개의 업로드를 사용합니다. CUSUniformUpload의 엔진 내부 정렬과 Blueprint의 단일 fresh buffer가 내부 선례입니다. 원래 셰이더·드로우 순서·R8 팬 코드 경로를 유지했습니다.

Patrice Déry(PowerBeef)의 D2R Metal 포팅에서 제출 단위 arena slice 아이디어를 참고해 BAR의 lease에 맞게 구현했습니다. [고정 원문](https://github.com/PowerBeef/d2r-metal/blob/602db5fc30d4ce50683435cb0745cb171fce2064/dm/gfx/nvn/nvn_mtl.m#L3948-L3959)

장면·카메라·시드·40구간 유닛 수·1512×950·AA OFF/sample 1·시뮬레이션 540–1740을 확인했습니다. 구성·빌드 각 1회, 게임 각 1회, 두 게임 모두 정상 종료했습니다. 소스와 종전 바이너리 보존 해시를 확인했습니다. CPU/GPU 시간 분해와 프레임별 픽셀 동등성은 이번 측정 범위 밖입니다.
