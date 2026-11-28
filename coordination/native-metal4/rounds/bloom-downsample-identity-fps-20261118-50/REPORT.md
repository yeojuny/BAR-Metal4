# BloomDownsample RGBA 13탭

**OFF 65.639 FPS → ON 64.867 FPS (-1.176%).**

| 지표 | OFF | ON |
|---|---:|---:|
| 집계 드로우 | 2,626 | 2,595 |
| 집계 시간 | 40.007초 | 40.005초 |
| 1% low | 56.25 FPS | 54.51 FPS |
| p99 | 17.0 ms | 17.0 ms |
| 최대 프레임 간격 | 21.0 ms | 37.0 ms |

Seths Ravine에서 동일 바이너리로 OFF 한 번과 ON 한 번을 순서대로 실행했습니다. AA OFF, 1512×950, 시뮬레이션 tick 540–1740입니다. 튀는 프레임까지 집계된 드로우와 기록된 프레임 간격을 모두 반영했습니다. 한 쌍의 결과입니다. 이번 실행에서는 평균과 1% low가 모두 하락해 후보 기본값은 OFF입니다. 파일 정리 종료 및 Asahi 대기 상태를 확인한 뒤 두 게임을 시작했습니다.

색상 소스의 실제 RGBA 순서가 일치할 때만 별도 셰이더를 사용합니다. 두 가중치 경로, 13탭 좌표, 수직 반전, 다른 채널 순서 및 깊이 텍스처 동작을 보존했습니다. Metal 오프라인 AIR에서 기존 셰이더와 후보 OFF의 명령이 일치했고 후보 ON은 13번 직접 샘플링하며 채널 매핑을 읽지 않았습니다. 최종 GPU 기계어와 픽셀 출력은 비교 범위 밖입니다. EncodeRange와 GroundFlash는 양쪽 OFF였습니다.

- BLOOM_OFF에서는 첫 Downsample 드로우가 `generic`를 인코딩했습니다. 기록 시점은 집계 구간 밖입니다.
- BLOOM_ON에서는 첫 Downsample 드로우가 `identity_rgba`를 인코딩했습니다. 기록 시점은 집계 구간 밖입니다.

바이너리 SHA-256은 `ce68647f1149a55b6bf7ab4271178d86b58f6282393720e495297d01d2d1c638`입니다. [패치](BLOOM-DOWNSAMPLE-IDENTITY.patch) · [FPS 원자료](FPS-EVIDENCE.json) · [구간별 FPS](FPS-BINS.csv) · [실제 인코딩 로그](BLOOM-ACTIVATION.json) · [컴파일 검증](BLOOM-COMPILER-VERIFICATION.json)

채널 순서가 같은 경우의 빠른 경로는 MoltenVK의 아이디어를 참고했습니다. [고정 원문](https://github.com/KhronosGroup/MoltenVK/blob/5189f8de86ec365860f63bf1da736815cfaea5cd/MoltenVK/MoltenVK/GPUObjects/MVKImage.mm#L2193-L2195)
