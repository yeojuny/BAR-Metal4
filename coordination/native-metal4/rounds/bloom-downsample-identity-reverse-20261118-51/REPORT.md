# BloomDownsample 13탭 역순 재측정

**OFF 65.007 FPS → ON 64.945 FPS (-0.094%).**

| 지표 | OFF | ON |
|---|---:|---:|
| 집계 드로우 | 2,600 | 2,598 |
| 집계 시간 | 39.996초 | 40.003초 |
| 1% low | 53.94 FPS | 55.44 FPS |
| p99 | 17.0 ms | 17.0 ms |
| 최대 프레임 간격 | 33.0 ms | 20.0 ms |

Seths Ravine에서 50회차와 동일한 바이너리를 다시 빌드하지 않고 사용했습니다. 순서를 바꿔 ON 한 번, OFF 한 번 실행했습니다. AA OFF, 1512×950, 시뮬레이션 tick 540–1740입니다. 튀는 프레임까지 집계된 드로우와 기록된 프레임 간격을 모두 반영했습니다. 앞선 OFF → ON 실행의 차이는 -1.176%였습니다. 두 쌍에서 OFF 대비 ON 차이의 방향은 같았습니다. OFF의 집계 시작은 프로세스 로그 시계 기준 ON보다 27.73초 늦었습니다. 두 실행 모두 tick 540–1740 전체를 집계했습니다.

바이너리, 씬, 시뮬레이션, 의존성, 대상 외 플래그는 이전 회차의 각 조건과 일치합니다. EncodeRange와 GroundFlash는 양쪽 OFF였습니다. 앞선 회차의 컴파일 검증을 함께 보존했습니다.

- BLOOM_OFF에서는 첫 Downsample 드로우가 `generic`를 인코딩했습니다. 기록 시점은 집계 구간 밖입니다.
- BLOOM_ON에서는 첫 Downsample 드로우가 `identity_rgba`를 인코딩했습니다. 기록 시점은 집계 구간 밖입니다.

바이너리 SHA-256은 `ce68647f1149a55b6bf7ab4271178d86b58f6282393720e495297d01d2d1c638`입니다. [이전 패치](../bloom-downsample-identity-fps-20261118-50/BLOOM-DOWNSAMPLE-IDENTITY.patch) · [FPS 원자료](FPS-EVIDENCE.json) · [구간별 FPS](FPS-BINS.csv) · [실제 인코딩 로그](BLOOM-ACTIVATION.json) · [컴파일 검증](BLOOM-COMPILER-VERIFICATION.json)

