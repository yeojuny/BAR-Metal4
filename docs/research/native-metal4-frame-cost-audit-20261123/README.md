# BAR Native Metal 4 외부 성능 감사

[감사 질문 27개](EXTERNAL-AUDIT.ko.md) · [현재 상태와 회차별 결과](AUDIT-HANDOFF.ko.md) · [프레임 원자료](audit-inputs/frames/)

Seths Ravine 전투에서 픽셀을 보존한 Native cap2는 평균 **87.41 FPS**, **1% low 58.19 FPS**였다. 별도 날짜의 MVK3 역순 회차는 **94.46 / 52.63 FPS**였다. 감사자는 두 경로에서 시간이 드는 단계를 대조해 같은 화면을 더 적은 작업으로 그릴 수 있는 후보 세 가지를 제안해 주면 된다.

질문서는 데칼이 깊이와 법선을 어디서 읽는지, 중간 이미지를 몇 번 저장하는지, cap2가 동시에 처리하는 프레임들 사이에서 어떤 자원을 재사용할 수 있는지 다룬다. 마지막 Grass 결합 후보는 아직 FPS를 측정하지 않았다. 선행 픽셀 검사와 마지막 진단 실행의 실패는 현재 상태 문서에 기록했다.

[측정 출처·해시](audit-inputs/SOURCES.json) · [소스 라이선스](audit-inputs/LICENSE.html) · [프로젝트 기여 기록](audit-inputs/UPSTREAM-CONTRIBUTIONS.json)
