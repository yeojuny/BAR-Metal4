# GroundFlash의 연속 depth 상태 범위 병합

**OFF 65.04 → ON 64.98 FPS (-0.08%).** 한 쌍의 전투 FPS에서 이득을 확인하지 못했습니다.

| 지표 | OFF | ON |
|---|---:|---:|
| 1% low | 55.56 FPS | 54.62 FPS |
| 중앙값 / p95 / p99 | 16 / 17 / 17 ms | 16 / 17 / 18 ms |
| 최대 프레임 타임 | 26ms | 26ms |

두 실행의 측정 전 tick 80에 GroundFlash 범위 4개를 관측했습니다. OFF는 4번, ON은 1번 그렸습니다. 전체 측정 구간의 draw 수나 CPU 시간은 별도로 계측하지 않았습니다. 과거 R02의 227개는 별도 캡처의 원시 결과입니다. 같은 index buffer의 연속 주소를 썼지만 각 slot 1 UBO의 객체가 달랐고, 원시 기록 자체로 값 동일성을 확인할 수 없습니다.

원래 GL은 depth 상태가 바뀔 때 GroundFlash를 끊었습니다. 이 패치는 native의 각 flash Draw와 유효성 검사, 인덱스 추가 순서를 유지하고 나서 연속 범위만 합칩니다. 같은 frame과 atlas, stage, depth, alpha·soften 값의 정확한 bit 및 연속 logical index 범위에 한정합니다. 외부 draw나 다른 stage를 넘지 않습니다. 같은 frame에서 카메라·fog·텍스처는 공통이고 나머지 UBO 입력은 검증한 범위 상태로 결정됩니다.

기존 particle CPU fixture를 확장해 동일 상태 병합, 상태가 다른 범위 분리, 겹친 범위 거부, 원래 vertex 바이트와 triangle order를 확인했습니다. ASan/UBSan 686 검사와 기존 잘못된 삼각형 변이 검출이 통과했습니다. 생산 바이너리의 3개 변경 오브젝트를 확인했습니다. 통합 인덱싱은 양쪽 ON, slab는 양쪽 OFF입니다. 원래 장면과 tick 540–1740의 모든 프레임을 보존했고 두 게임은 정상 종료했습니다.
