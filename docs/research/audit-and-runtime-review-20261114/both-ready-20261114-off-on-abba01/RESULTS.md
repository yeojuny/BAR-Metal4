> Publication copy: original conclusions and historical version scope preserved. Personal absolute paths were neutralized; source links are pinned to the audited a33ebfe revision. Raw logs, JSON manifests, binaries, screenshots and the original plan are not included in this Markdown-only export. See the parent README for later runtime results and limitations.

두 파일럿 동시 OFF/ON 비교: 엔진 오류로 중단

순서 OFF→ON→ON→OFF, 기존10초 예열+40초 집계를 시작했다. 첫OFF만 정상완료했으며 평균 **178.55 FPS**,1%low **111.28 FPS**였다.

첫 동시ON은 예열을 통과해 집계가 시작됐으나 sim825(t45.321s, 집계시작t27.812s)에서 **Renderer handoff access conflict**의 std::logic_error로 종료(-6)했다. 40초 집계를 끝내지 못해 유효ON평균·합산변화율은 없다. 부분FPS를 비교에 쓰지 않았다. 실제자식 프로세스의BEGIN=1/PREPARE=1/worker1·공통overlap설정은 집계전에 확인했다. 이 오류는 예열 경계 문제가 아니므로 복구/재시도 없이 전체중단했고 나머지2게임은 실행하지 않았다.

같은 signedad5853c0… 엔진과 원래private초기프로필·프리셋·장면,AA0/양VSyncOff. 별도 외부실행설정에서 wrapperexports 뒤 두 pilot만 override했고 원본앱/래퍼/개인프로필/통합pin은 보존했다. 기존FPS나앞선pilot단독실험은 합산하지 않았다. 추가원인연구·빌드·프로브·Validation 없음. CPU관찰FPS의부분결과만 기록한다.

실패로그/부분간격원자료 보존. 두실행의owned그룹소멸, 네준비단계의임시제어/게임/설정위젯 제거, awake해제, 종료idlePASS. 원자료는 preparation/runs/A1_OFF 및 B1_ON의engine.log,infolog.txt,CHILD-MODE.json,CLEANUP.json, B1_ON/FAILURE.json. SUMMARY.json에 정확한부분결과/실제모드/오류줄 저장.
