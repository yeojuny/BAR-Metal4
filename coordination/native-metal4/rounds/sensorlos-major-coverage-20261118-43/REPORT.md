# SensorLOS 외곽선 판정 공유 타당성 검토

**이번 후보는 소스 검토에서 보류했습니다. 새 FPS 측정은 없습니다.** R03의 정확한 판정을 유지하면서 전체 GPU 비용까지 줄일 수 있는 작은 변경을 확인하지 못했습니다. Round 41/42 통합 바이너리와 원본은 해시가 일치했습니다.

- 독립 감사 §4.1의 R02 원본 API 행 api9766에서 378 vertices × 165 instances를, shader-costs의 frag0에서 26.34215%를 재확인했습니다. 비중은 과거 한 프레임의 외곽선 fragment 함수 전체에 해당합니다. 커버리지 산술만의 시간과 현재 GPU 점유는 분리되어 있지 않습니다.
- 별도 R04의 endpoint/setup hoist는 clean 각 세 번에서 -7.43%였습니다. 두 int4 varying으로 옮긴 설정 값과 이번에 공유하려던 열·행별 실제 판정은 범위가 다릅니다.
- R03의 NativeSensorLOSShaders.h은 하드웨어에서 얻은 높이와 잘린 endpoint를 사용합니다. 같은 선분 안에서 주축 정수 좌표가 같을 경우 covered/lower/count만 공유할 수 있습니다. 단위 너비 diamond와 부호·tie 규칙을 그대로 계산해야 합니다. 픽셀별 범위 검사, 커버리지가 쓰는 depth 검사, 실제 반환 depth, 보간과 implicit-derivative mask sample은 기존 계산을 유지해야 합니다.
- R03은 이미 lower/count로 범위 밖 fragment를 걸러낸 뒤에만 diamond를 계산합니다. 고정 위젯의 기본 raster width 4.5px를 예로 들면, 이 단계에 들어가는 위치는 주축 좌표당 최대 5개입니다. rejected fragment도 lower 계산은 수행합니다. 실제 런타임 width 분포·남는 중복량은 미측정입니다.

| 구현 검토 | 비용과 제약 |
|---|---|
| Fragment quad에서 교환 | 2×2의 가까운 이웃 한 쌍만 공유할 수 있습니다. 안전한 교환에는 같은 선분, 같은 주축 좌표와 유효한 공급 lane을 증명하거나 fallback이 필요합니다. 폭 전체를 공유하지 못하고, 실행 lane 수가 줄어도 SIMD 그룹에서 판정 명령이 빠진다는 근거는 없습니다. |
| GPU에서 보이는 span만 사전 계산 | 정확한 GPU endpoint/clip을 먼저 생산하고 span별 저장소·소비 순서·수명을 닫아야 합니다. CPU가 앞서 만든 근사 좌표로 대체할 수 없습니다. 역사적 draw를 1512열 전체로 단순 할당하면 15,717,240개 항목, 항목당 4바이트 예시로 62,868,960바이트입니다. 압축 span의 실제 크기와 GPU 쓰기·읽기 비용은 자료에 없습니다. |
| 열·행마다 기하를 방출 | 판정은 기하 단계에서 할 수 있으나 topology, primitive winding과 경계, 순서, implicit derivatives를 기존과 동일하게 유지할 구현이 필요합니다. 기존 인덱스와 삼각형 배치 안에서 끝나는 변경 범위를 벗어납니다. |

작업 기록, 정확한 원본 행과 해시를 DONE.json에 남겼습니다. 구성·빌드·GPU 검증·게임은 모두 0회입니다.
