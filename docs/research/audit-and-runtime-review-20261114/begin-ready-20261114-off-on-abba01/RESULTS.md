> Publication copy: original conclusions and historical version scope preserved. Personal absolute paths were neutralized; source links are pinned to the audited a33ebfe revision. Raw logs, JSON manifests, binaries, screenshots and the original plan are not included in this Markdown-only export. See the parent README for later runtime results and limitations.

같은 앱 BeginReady OFF→ON→ON→OFF, 각10초 예열+40초 집계. PrepareReady0/worker1 고정. 이번4회만 비교, 앞선PrepareReady ON이나기준판FPS는 합산하지 않았다.

| 실행 | 평균 FPS |1% low |
|---|---:|---:|
|A1_OFF|179.46|113.39|
|B1_ON|103.79|74.73|
|B2_ON|104.16|82.51|
|A2_OFF|179.34|111.46|

| 각2회 합산 | 최고¹ | 평균 FPS | 최저¹ |1% low |
|---|---:|---:|---:|---:|
|OFF|184.45|**179.40**|173.87|112.41|
|ON|107.89|**103.97**|86.52|78.43|

ON 평균 변화 **-42.04%**. 두 쌍은 -42.16% / -41.92%. 마지막OFF에서 평균이 다시 높아졌다. 유효4회모두 포함. 실패/재시도/복구0.

¹ 최고/최저는 기존 약1초 구간. 합산 평균=총draws/총집계시간.1%low는 원래CPU관찰 DrawScreen 간격을 합쳐 느린1%의 평균 역수. 표시FPS/GPU처리량/CPU병렬성의 증거로 해석하지 않는다.

같은 signedad5853c0… 엔진/컴파일설정. 별도직접실행 설정에서 원래래퍼exports 적용후BeginReady만0/1 override; 원본앱/래퍼는 변경하지 않았다. 실제자식모드 영수증4회모두집계전PASS. 나머지환경은private경로를 정규화하면 동일. 동일초기프로필/콘텐츠/장면/seed/gameID/카메라/사이트/품질설정, AA0·양쪽VSyncOff, safeview1512×950, 내장화면·AC자동모드. 전면/메뉴 조건PASS.

새프로브/추가계측/빌드/검증캠페인/활성경로연구 없음. 네실행 정상종료·그룹소멸, 임시제어/게임/설정위젯 정리, awake해제, 종료idlePASS. 기존배포판·다운로드앱·개인프로필·통합pin 변경없음.

원자료: preparation/runs/{A1_OFF,B1_ON,B2_ON,A2_OFF}/RESULT.json, infolog.txt, LAUNCH.json, CHILD-MODE.json, MENU-WITNESS.jsonl, TERRAIN211-FRAMES.csv. SUMMARY.json/CHILD-MODES.json 참조.
