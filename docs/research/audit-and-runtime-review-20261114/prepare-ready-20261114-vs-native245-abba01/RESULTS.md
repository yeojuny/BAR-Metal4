> Publication copy: original conclusions and historical version scope preserved. Personal absolute paths were neutralized; source links are pinned to the audited a33ebfe revision. Raw logs, JSON manifests, binaries, screenshots and the original plan are not included in this Markdown-only export. See the parent README for later runtime results and limitations.

기존 장면/프리셋으로 새 ABBA 블록4회. 각각10초 예열+40초 집계. 앞선1회씩 결과는 포함하지 않았다. AA0/VSync·VSyncGame Off, safe view1512×950, 내장 화면만 사용.

| 순서 | 평균 FPS |1% low |
|---|---:|---:|
|N1_245|174.84|109.89|
|P1_PrepareReadyOFF|179.16|111.80|
|P2_PrepareReadyOFF|179.45|110.77|
|N2_245|175.00|111.64|

| 이번2회 합산 | 최고¹ | 평균 FPS | 최저¹ |1% low |
|---|---:|---:|---:|---:|
|기준판245|179.72|**174.92**|163.16|110.76|
|새 앱·파일럿OFF|183.45|**179.30**|174.17|111.28|

평균 변화 **+2.51%**. N1↔P1 +2.47%, N2↔P2 +2.55%. 두 쌍 모두 상승. 유효4회 모두 보존, 제외/재시도0. 앞선 onepass 결과는 별도 유지. 각제품의 합산 평균은 전체draws/전체집계시간으로 계산,1%low는 두 실행의 원래 간격 표본을 합쳐 느린1%를 선정했다.

¹ 최고/최저는 원래 약1초 구간. FPS/1%low 모두 CPU관찰 DrawScreen 기준으로 실제 화면 표시나 GPU 처리량을 뜻하지 않는다.

후보 ad5853c0… signed engine, 원래worker1 및 래퍼 설정 유지. BEGIN_READY_PILOT=0/PREPARE_READY_PILOT=0이며 pilotON 효과 검증이 아니다. 이전9개 intervention 또는 기준판13개 옵션을 후보에 이식하지 않았다. 기준판95929a2a…+13개 승인 설정. 맵/게임 체크섬, 명령 이벤트, seed/gameID, 카메라/사이트좌표, 공통 품질 설정 일치, 네 실행의 전면/메뉴 조건 통과.

빌드·추가 계측·API검증·RAM수집·배포/개인프로필/통합pin 변경 없음. 프로세스 정상 종료/그룹 소멸, 임시 제어·게임·설정 위젯 정리, awake 해제, 종료idle PASS.

원자료: preparation/runs/{N1_245,P1_PrepareReadyOFF,P2_PrepareReadyOFF,N2_245}/RESULT.json, infolog.txt, LAUNCH.json, MENU-WITNESS.jsonl, TERRAIN211-FRAMES.csv. RUNS.json/SUMMARY.json에 전체 수치 저장.
