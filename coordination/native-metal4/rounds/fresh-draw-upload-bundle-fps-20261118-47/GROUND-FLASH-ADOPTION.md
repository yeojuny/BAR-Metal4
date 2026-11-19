# GroundFlash 기본 적용 판단

**보류.** 현재 한 쌍은 OFF 65.03537 → ON 64.98150 FPS(−0.08283%)였습니다. 두 실행에서 워밍업 tick 80의 네 범위를 확인했고, ON은 한 번의 draw로 제출했습니다. 과거 R02의 한 캡처 프레임에는 같은 인덱스 버퍼에서 227개 연속 draw가 있었으나 uniform 버퍼는 서로 달랐습니다. 두 관측을 합쳐 현재 전투의 draw 밀도·시점별 빈도나 일반적인 퇴보/동등성을 추정할 수 없습니다.

## 비용과 발생 조건

- 단독 flash나 상태가 자주 교차하는 연속 flash에서는 범위 비교만 추가될 수 있습니다. depth-test, depth-write, alpha, soften 값에 변화가 있거나 논리 범위가 이어지지 않으면 그대로 냅니다. geometry 생성/검증과 인덱스 복사는 계속 per-flash입니다.
- 같은 상태로 화면 안에 겹쳐 살아 있는 flash가 많을 때 range 수만큼 uniform, argument table, depth 준비, setters와 draw 호출이 줄 수 있습니다. 줄이는 주된 대상은 CPU Prepare/API encode와 draw front-end입니다. R02에서 해당 particle shader의 과거 상대 비용은 FS 약 0.157%, VS 약 0.03383%였으며, 현재 CPU 시간은 분리 계측하지 않았습니다.
- 초반에는 단일 유닛의 무기 효과나 지진 신호도 flash를 만들 수 있습니다. 기본 폭발 처리와 CEG 경로는 지면 조건·flash 크기·TTL을 평가하고, 지진 효과는 별도 생성 경로입니다. 카메라 안, LOS 또는 alwaysVisible 조건을 통과한 flash만 native 목록에 들어갑니다. 적은 교전에서는 묶을 범위가 적을 가능성이 있으나 초반 빈도는 미확인입니다.
- 중반에는 같은 무기·CEG로 여러 발이 비슷한 위치에 터질 때 TTL이 겹치고 같은 depth 상태가 연속될 수 있습니다. 카메라/LOS 밖 폭발은 범위에 잡히지 않습니다. 동일 무기만 있어도 생성 순서 중 다른 효과가 끼면 병합은 끊깁니다. 빈도와 CPU 비용은 미확인입니다.
- 후반의 밀집 교전이나 다수 유닛의 동시 발사는 가장 많은 연속 범위를 만들 가능성이 있습니다. 넓게 분산된 교전, 폭발 효과가 다른 병종, 관전/플레이 시점 차이에서는 visible range와 연속성은 별개입니다. 렌더링이 GPU 연산 또는 대역폭에 걸려 있다면 CPU 호출 감소가 FPS에 나타나지 않을 수 있습니다. 후반 역시 기존 두 지점으로 빈도를 확인할 수 없습니다.

## 의미 보존과 남은 확인

- 실제 GL은 depth 상태가 바뀔 때 flush합니다. 후보는 native per-flash Draw 호출, 원래 시점의 validation과 bytes append를 그대로 실행합니다. 이 순서를 유지하므로 표준·지진 flash의 색 변경과 SimpleGroundFlash의 회전/애니메이션 업데이트를 건너뛰지 않습니다.
- 같은 frame의 atlas, 카메라·fog·terrain depth와 stage를 공유합니다. depth·soften·alpha 입력이 정확히 일치하는 두 범위만 합칩니다. 인덱스 원본은 바꾸지 않고 논리 count/offset과 uint32 상한을 확인합니다. 다른 stage나 중간 callback/GPU writer는 병합 범위 밖입니다. 일부 리소스 생성 자체가 사라지므로 새로운 게임 조건의 할당 실패 결과까지 동일하게 관측한 것은 아닙니다.
- ASan/UBSan의 기존 CPU fixture 686검사에서 동일 범위 결합, 바이트·순서, 상태 차이와 겹친 범위 거부를 확인했습니다. 게임에서는 한 번의 실제 4→1을 워밍업에서 관측했습니다. 다른 혼합 상태의 대규모 후반 전투, 전체 픽셀 동등성 및 실제 CPU 시간 이득은 미확인입니다.

GroundFlash는 별도 opt-in 후보로 보존합니다. 증거 범위에선 기본값을 바꿀 근거가 부족하며 독립 fresh-upload 묶음에서 제외합니다.

기존 전투 infolog 20건을 확인했습니다. GroundFlash 실측 횟수는 Round 46의 워밍업 tick 80 로그 두 줄에만 있으며, 540–1740 측정 구간 및 경기 단계별 횟수는 기록되어 있지 않습니다. 추가 게임 또는 계측은 진행하지 않았습니다.
