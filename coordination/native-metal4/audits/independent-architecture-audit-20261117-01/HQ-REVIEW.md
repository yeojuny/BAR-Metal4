# HQ 원문 대조 및 지니 전달

Astra ultra 단일 독립 감사의 원문 SHA256은 2e39269952a18af792a6477bbb809c4941ac680d805bb5370c062c766ff6f1a7이다. 원문은 수정하지 않았다. 44개 참조 파일과 줄 존재를 재검증했으며, 아래 핵심 source/API를 HQ가 직접 대조했다.

- NativeSensorLOSShaders.h:153–170의 endpoint/axis/denominator/count 및 동일 major-column의 SensorDiamond 반복 구조. 입력 varying은 flat이다. 구체 공유 구현의 정확성/성능은 미증명이다.
- NativeGroundDecalCapture.cpp:72–92는 unchanged revision을 재사용하지만 변경 시 values 전체 복사 후 dirty 범위를 덮는다. Metal4GroundDecal.mm:160은 새 instance publication에 전체 Buffer 초기화를 한다. 원본 GroundDecalHandler.cpp:815–826은 dirty 범위만 SetBufferSubData한다.
- 현재 raw api792는6213888-byte newBufferWithBytes, api5530은36-index/32364-instance, api9766은378-vertex/165-instance이다. 한 캡처의 관측이며 매 프레임/초당 비용으로 외삽하지 않는다.
- Metal4RuntimeNative.mm:1030–1100은 N+1 기록 뒤 predecessor retirement 및 pending empty 조건을 요구한다. completed owner graph 파괴는 이미 submit 뒤다. 현재 runtime gap 기여는 미확인이다.
- 역사 Ultra AA0 raw means는 MVK3 93.86682240479439 / 당시 native47.62202317388899다. 현재 R02와 동시 비교한 값이 아니다.

현재 가장 강한 GPU 표적은 SensorLOS 반복 계산 감소다. GroundDecal publication은 복사/초기화 증폭이 확인된 별도 CPU/메모리 후보이며, 먼저 dirty base revision·누적 변경·resize/compaction·GPU pin을 유지하는 상위 계약을 고정해야 한다. 다중 GPU 제출은 그보다 proof 범위가 넓으며 현재 runtime identity 타임라인이 선행돼야 한다. 원본도 비슷한 개수의 decals를 그리므로32364 instances 자체를 낭비라고 단정하는 가설은 채택하지 않는다.

해석 제한: 저장된 MVK3 shader와 native의 variant/thinning/fade 차이를 원본 기능이 불필요하다는 증거로 사용하면 안 된다. 실제 픽셀 영향과 variant 선택 이유는 미확인이다. timeline의 마지막 count 값과 외곽 구간으로 정확한 중첩량이나 절약 ms를 계산하지 않는다.

사용자 기준: 평균 FPS를 주 지표로 보고 1% low 하락만으로 기각하지 않는다. FPS 중립이라도 실제 GPU/CPU 작업·복사량·할당/RAM 감소가 입증되면 별도 기록하고 후속 결합 후보로 보존한다. 임의의 통계 유의성 문턱을 새 승인 조건으로 만들지 않는다. 산포와 구분되지 않는 차이를 개선/회귀로 단정하지 않는다.

첫 실행 권고는 이미 정확성 gate가 끝난 고정 R03 대 R02의 독립 비교다. 현재 READY의 R03 isolated build/staging1은 그 선행 단계이며 게임/성능 권한을 확장하지 않는다. 상위 SensorLOS 공유 계산이나 Ground dirty publication을 같은 패치에 섞지 않는다.

감사와 HQ 원문 대조는 종료됐다. 소스/설정/runner 변경, 새로운 helper/game/벤치/capture/replay는 없었다. 기존 source attribution 및 Beherith/MIT, engine GPL, Mesa/MoltenVK 라이선스와 UPSTREAM-CONTRIBUTIONS 기록은 유지한다. main/릴리스/Homebrew 변경은 없다.
