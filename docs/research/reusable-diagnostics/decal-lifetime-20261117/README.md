# 재사용용 데칼 수명 관측기 보존 기록

사용자 요청에 따라 기존 최종 후보를 바꾸지 않고 한 번만 보존했다. 새 observer나 성능 기능을 추가하지 않았다.

## 보존 내용
- 원본 후보 `decal-lifetime-integration-20261117-02` 전체 20파일: narrow helper, controller, runner, Lua/Python tests, 기존 결과, source references, manifest, 보호검증, 기여기록.
- 이전 실행계획은 context/previous-execution-plan.md로 보존했다. 이 계획의 0프레임 HOLD는 -02 최소수정으로 해소되었으며, 원문은 기록 목적으로 그대로 남겼다.
- 후보 manifest SHA256: `7fc7113945056d7949b4213575639aad3dda1200765748d3cec40034afe725d3`
- gzip archive SHA256: `7ec581a1f417217c4beab51d513affc36dbe9efdbdcc0d4edf505e92dca1e8a2` (73126 bytes)
- 원본 Mac 파일은 이동·삭제·수정하지 않았다.

## 복원 및 무결성
SNAPSHOT.tar.gz.b64를 표준 Base64로 디코딩한 gzip tar를 별도의 새 폴더에 푼다. 압축 파일 SHA256을 위 값과 비교하고, 복원된 MANIFEST.json의 파일별 SHA256도 검증한다. archive에는 상대 경로의 일반 파일만 들어 있다. 원본 증거에 덮어쓰지 않는다.

## 용도와 재사용 조건
이 도구는 원본 게임이 공개하는 순수 GetLifeTimeMult getter와 raw Platform.gpuMemorySize / Game.gameShortName 값을 두 시점에 읽는 CPU 진단이다. raw WG 경로, getter의 순수성, 초기화 시점, 실제 게임 shortName, 첫 callback이 0 또는 1인 계약과 종료 경로를 새 대상에서 다시 확인해야 한다. WG 프록시가 rawget을 지원하지 않으면 우회하지 않고 UNKNOWN이다.

narrow helper는 독립 재사용 가능하지만 controller와 runner는 BC213, 프레임1590, 특정 맵·시드·원본 장면·binary/runtime·경로에 묶여 있다. 이를 다른 프로젝트에서 그대로 실행 가능한 범용 도구라고 간주하지 않는다. 절대경로·해시·source compatibility를 먼저 검토한다. run_tests.py는 /opt/homebrew/bin/lua 및 luac를 사용하며 결과 파일을 쓴다. 보존본은 읽기 전용으로 두고 테스트는 별도 작업 복사본에서 실행한다. assembler는 이전 후보 경로를 참조하므로 원형 기록용이며 다른 환경에서는 경로 검토가 필요하다.

## 검증 범위와 한계
- CPU 모의검사 50개 = Lua33 + Python17. 기존44에서 0프레임 회귀6개 추가. 이는 게임50회/성능계측50회가 아니다.
- host Lua5.4 검사, target Recoil Lua5.1의 완전 호환성 증명 아님.
- RNG·lifetime 값을 쓰지 않고 VBO getter도 호출하지 않는다.
- 두 scalar의 관측이지 GPU 작업량·픽셀동등성·원인·FPS 증명이 아니다.
- frame0 버그를 놓친 이전44개 통과 기록의 한계를 숨기지 않는다.
- backend 영수증 테스트는 정상 성공경로이며 누락영수증 음성사례까지 모두 입증하지 않는다.

## 실제 사용 기록
[round27 보고서](../../../../coordination/native-metal4/rounds/r03-mvk3-lifetime-20261117-27/REPORT.md)
- R03 frame0/1590: GPUmemory raw0, lifetime0.699999988079071.
- MVK3 frame0: raw18622720, lifetime3. frame180 warmup 실패로1590 UNKNOWN.
- 게임2, 캡처/프로파일/재시도/빌드0, 정리 완료. 추가 lifetime observer 실행은 계획하지 않는다.
- 재사용 보존은 새로운 실행권이나 게임 예산을 부여하지 않는다.

원본 저자·라이선스와 UPSTREAM-CONTRIBUTIONS.json을 그대로 유지했다. 실제 GPU 병목 비교는 별도 최소 통제쌍으로 이어간다.
