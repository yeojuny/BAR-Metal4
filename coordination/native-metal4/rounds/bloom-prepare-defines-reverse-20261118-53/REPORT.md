# Bloom 셰이더 설정 재사용 역순 재측정

**OFF 65.522 FPS → ON 65.686 FPS (+0.251%).**

| 지표 | OFF | ON |
|---|---:|---:|
| 집계 드로우 | 2,621 | 2,627 |
| 집계 시간 | 40.002초 | 39.993초 |
| 1% low | 52.22 FPS | 56.13 FPS |
| p99 | 17.0 ms | 17.0 ms |
| 최대 프레임 간격 | 45.0 ms | 18.0 ms |

Seths Ravine에서 앞선 회차의 동일 바이너리로 ON 한 번과 OFF 한 번을 순서대로 실행했습니다. AA OFF, 1512×950, 시뮬레이션 tick 540–1740입니다. 튀는 프레임까지 집계된 드로우와 기록된 프레임 간격을 모두 반영했습니다. 앞선 OFF → ON 검증에서는 평균 FPS가 -0.071%였습니다. ON의 집계 시작은 프로세스 로그 기준 OFF보다 25.69초 늦었습니다. 두 실행 모두 tick 540–1740을 빠짐없이 집계했습니다. 이전 ON의 266ms 프레임 간격은 이번에는 관측되지 않았습니다. 채택된 BloomDownsample RGBA 설정은 양쪽 ON입니다. 새 설정 재사용만 전환했습니다.

한 프레임의 Bloom 준비에서 이름, float 비트, locale, depth mask, combine 모드가 같은 셰이더 설정 문자열을 재사용합니다. 212개 CPU 사례에서 원래 문자열과 일치했습니다. GPU 셰이더, 파이프라인 상태 키, 깊이/색상 경로와 출력 계약을 유지했습니다. EncodeRange와 GroundFlash는 양쪽 OFF였습니다.

- DEFINES_OFF에서는 첫 Downsample 드로우가 `identity_rgba`를 인코딩했습니다. 기록 시점은 집계 구간 밖입니다.
- DEFINES_ON에서는 첫 Downsample 드로우가 `identity_rgba`를 인코딩했습니다. 기록 시점은 집계 구간 밖입니다.

재사용 최초 로그: [{'line': 2186, 'text': '[t=00:00:43.053992][f=-000001] [NativeBloomPrepareDefineReuse] reused=1 depth_mask=0 combine=0'}]. 이 로그는 실제 캐시 적중을 보여주며 전체 프레임별 횟수는 집계 범위 밖입니다.

바이너리 SHA-256은 `2a3b7323b43ac04c5e08661abfe15f391a8ab2cd75fe1c6c22f644380f696517`입니다. [이전 패치](../bloom-prepare-defines-fps-20261118-52/BLOOM-PREPARE-DEFINE-REUSE.patch) · [FPS 원자료](FPS-EVIDENCE.json) · [구간별 FPS](FPS-BINS.csv) · [실제 인코딩 로그](DEFINE-ACTIVATION.json) · [컴파일 검증](BLOOM-DEFINE-VERIFICATION.json)

