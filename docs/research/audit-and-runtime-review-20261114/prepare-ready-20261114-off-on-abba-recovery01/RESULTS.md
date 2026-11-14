> Publication copy: original conclusions and historical version scope preserved. Personal absolute paths were neutralized; source links are pinned to the audited a33ebfe revision. Raw logs, JSON manifests, binaries, screenshots and the original plan are not included in this Markdown-only export. See the parent README for later runtime results and limitations.

동일한 새 앱 PrepareReady OFF/ON: 기존 예열 경계 실패 뒤 허용된1회 복구 블록. 기존 실패 폴더/원자료는 변경하지 않았으며 포함하지 않았다. 제어기·9.8초 기준·장면·프리셋·바이너리 그대로 각10초 예열/40초 집계.

| 순서 | 평균 FPS |1% low |
|---|---:|---:|
|A1_OFF|179.31|112.68|
|B1_ON|102.55|79.85|
|B2_ON|102.58|80.15|
|A2_OFF|178.25|112.50|

| 이번 각2회 합산 | 최고¹ | 평균 FPS | 최저¹ |1% low |
|---|---:|---:|---:|---:|
|OFF|183.08|**178.78**|169.34|112.51|
|ON|105.68|**102.57**|95.67|79.96|

ON의 합산 평균 변화 **-42.63%**. 첫쌍 -42.81%, 둘째쌍 -42.45%. OFF로 돌아온 마지막 실행에서 FPS가 다시 높아졌다. 이번4회 모두 유효하며 낮은ON결과를 포함했다. 원래 실패 및 앞선 후보/기준판 비교FPS는 합산하지 않았다.

¹ 최고/최저는 기존 약1초 구간. 합산 평균=총draws/총집계시간.1%low는 두 실행의 원래 CPU관찰 DrawScreen 간격 표본을 합쳐 느린1%의 평균 역수. 실제 화면표시나GPU처리량 수치가 아니다.

같은 signed engine ad5853c0…/source13cd5d6, worker1/BEGIN_READY_PILOT0/원래FRAME·CAPTURE·PREPARE_OVERLAP1. 유일한 모드 차이는PREPARE_READY_PILOT0/1. 원래강제OFF래퍼는 변경하지 않고 별도 직접 실행 설정에서 exports 뒤 override했다. 네 실행 모두 집계 전 실제 자식 프로세스의 선택6개 옵션 확인. 초기private 프로필과 콘텐츠·명령 이벤트·seed/gameID·카메라·사이트좌표·해상도·AA0·동기화OFF·품질설정 일치. 전면/메뉴 조건 통과.

기존 로그로 실제ReleaseForPreparation 적격 경로 수행 및 메인캡처/worker준비의 양의 시간중첩을 확인하지 못했다(UNVERIFIED). MRT/COW·복사·storage 변화도미계측이다. 따라서 전체P5옵션의 순성능 효과만 관찰했으며 CPU-only원인으로 단정하지 않는다. 새 시각검증/스크린샷은 수행하지 않았다.

4회 정상 종료, 추가 예열 실패/프로세스 중단 없음. 모든 owned그룹/임시 위젯·게임 오버레이 정리, awake 해제, 종료idle PASS. 원본앱/래퍼/개인프로필/통합pin/배포판 변경 없음. 빌드·추가프로브·코드변경·추가복구 없음.

원자료: preparation/runs/{A1_OFF,B1_ON,B2_ON,A2_OFF}/RESULT.json, infolog.txt, LAUNCH.json, CHILD-MODE.json, MENU-WITNESS.jsonl, TERRAIN211-FRAMES.csv. SUMMARY.json/CHILD-MODES.json에 집계·조건·실제모드 영수증 저장.
