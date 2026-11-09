# S09 계획: RS485 · I/O · USB Command 기반 안내 Page

작성: 2026-11-08. 상태: `pending`. 선행 조건: S08 `wifi-ap-web` complete.

## 1. 목표와 범위

RS485, GPIO/PWM I/O, USB command를 실제로 동작시키는 샘플 대신 한 page의 세 section에서
무엇을 연결할 수 있는지, 202609에 어떤 기반이 있는지, 현재 starter에서 무엇이 구현되지
않았는지를 명확히 설명한다.

- RS485: transport/facade 기반만 소개. frame codec, device protocol, worker는 구현하지 않음.
- I/O: GPIO/PWM public facade 후보만 소개. pin toggle, sensor read는 구현하지 않음.
- USB command: CDC transport/dispatcher 경계를 소개. command protocol/CLI는 구현하지 않음.
- S05 USB MSC와 기존 asset updater는 실제 존재하는 별도 기능으로 구분해 표시.

참조: `../dev_202609_hmi/src/hardware/rs485.*`,
`../dev_202609_hmi/src/hardware/gpio.*`,
`../dev_202609_hmi/src/platform/hmi/assets/`.

## 2. 기반 이식 원칙

- 실제 후속 기능이 include할 수 있는 작은 public contract만 가져온다.
- 보드 pin, UART handle, GPIO number, USB descriptor와 제품 protocol은 가져오지 않는다.
- 사용되지 않는 worker/driver/source를 미리 복사하지 않는다.
- capability availability와 `not_implemented`를 명시적으로 표현해 UI가 가짜 준비 상태를
  표시하지 않게 한다.
- 세 계약은 서로 합치지 않으며 공통인 결과/status 타입만 공유한다.

## 3. 단계

### I0. 202609 기반 감사

각 기능의 public facade, internal driver, 제품 결합, 현재 보드 지원 여부를 표로 정리한다.
가져올 header/type과 제외할 source를 확정하고 include dependency test를 만든다. 죽은 구현을
복사하지 않았음을 검토해야 I1로 간다.

### I1. 최소 public contracts

후속 구현용 capability descriptor와 좁은 port skeleton을 추가한다. unsupported adapter는
결정적인 `not_implemented`를 반환한다. compile/interface test와 Board internal 비노출 검사가
통과해야 I2로 간다.

### I2. 안내 page

RS485, I/O, USB command 세 card/section에 용도, 현재 상태, 향후 adapter 위치, 제한을 표시한다.
실행 button이나 성공처럼 보이는 mock control은 두지 않는다. S05 MSC와 asset updater 링크는
실제 기능 상태에 맞게 안내한다. 두 locale과 navigation test가 통과해야 I3로 간다.

### I3. 문서/수용

Windows/HMI build와 isolation check를 수행한다. 사용자가 설명과 지원/미지원 구분을 확인하면
S09를 완료한다. 실제 cable, transceiver, GPIO 부하, USB command 검증은 종료 조건이 아니다.

### 단계 게이트 요약

| 단계 | 목표·범위 | 산출물 | 자동 확인 | 수동 확인 | 종료 조건·의존 |
| --- | --- | --- | --- | --- | --- |
| I0 | 202609 facade와 제품/driver 결합 감사 | 이식/제외 표, dependency graph | source/include 경계 검사 | 기반과 제외 범위 검토 | public contract 후보 확정. S08 complete |
| I1 | 최소 공개 계약과 unsupported 상태 | descriptor, port skeleton, unsupported adapter | compile/interface/dead source 검사 | 후속 확장성 검토 | internal handle 0, 실제 driver 0. I0 complete |
| I2 | 세 section 안내 UI | RS485/I/O/USB 설명·상태·제한 | locale/navigation/layout/destruction test | 지원/미지원 표현 검토 | 가짜 실행 control 0. I1 complete |
| I3 | build와 문서 수용 | build/지원표/사용자 검토 기록 | Windows/HMI build, isolation | 화면 가독성과 오해 가능성 확인 | 사용자 승인. I2 complete |

## Automated Verification Checklist

- [ ] 이식 표가 public contract와 제외할 internal source를 구분한다.
- [ ] 실제로 사용되지 않는 worker/driver가 build 입력에 없다.
- [ ] unsupported adapter가 `not_implemented`를 일관되게 반환한다.
- [ ] page에 실행 가능한 가짜 RS485/I/O/USB command control이 없다.
- [ ] S05 MSC와 USB command 설명이 혼동되지 않는다.
- [ ] 두 locale, navigation/re-entry/destruction test가 통과한다.
- [ ] Windows/HMI build와 isolation check가 통과한다.
- [ ] UI/module에 Board internal·UART/GPIO/TinyUSB handle이 노출되지 않는다.

## Manual Verification Checklist

- [ ] 세 section의 목적과 미구현 상태가 한눈에 구분된다.
- [ ] RS485·I/O·USB command를 실제 지원한다고 오해할 표현이 없다.
- [ ] USB MSC와 asset updater의 현재 지원 범위가 정확히 설명된다.
- [ ] page 왕복과 locale/theme 전환에서 layout 문제가 없다.
