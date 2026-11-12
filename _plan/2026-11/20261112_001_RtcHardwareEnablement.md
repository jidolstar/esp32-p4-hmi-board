# RTC 하드웨어 활성화 후속 계획

작성: 2026-11-12. 상태: `pending`.
관계: 완료된 S10 `date-time` page의 범위 밖 하드웨어 후속 작업.

## 1. 목적과 현재 상태

실제 보드 revision과 RTC 칩/register 호환성을 확인한 뒤에만 RTC adapter를 활성화한다.
현재 HMI는 system clock과 SNTP를 사용하며, 확인되지 않은 RTC register에는 접근하지 않고
화면에 `RTC 칩 확인 전` 상태를 표시한다. S10의 네트워크 기반 날짜·시각 갱신과 수동 설정
완료 여부에는 이 후속 작업이 영향을 주지 않는다.

## 2. 범위와 경계

- 실제 보드 revision, RTC 칩 marking·주소·register 호환 근거를 확인한다.
- CR1220을 충전하지 않는 설정과 readback 실패 시 비활성 정책을 확정한다.
- `Board`가 소유한 공용 I2C 경계를 통해 RTC facade와 private driver를 구현한다.
- 부팅 시 RTC→system clock, 네트워크 보정 뒤 조건부 RTC 기록과 전원 재인가 유지를 검증한다.
- UI, 공통 time module과 platform adapter에 `TwoWire` 또는 raw register를 노출하지 않는다.

## 3. 단계

### R0. 보드·칩 근거 확인 — `pending`

- **목표:** RTC register 접근을 허용할 수 있는 물리적 근거를 확보한다.
- **범위:** 보드 revision, 칩 marking, I2C 주소, datasheet, CR1220 회로.
- **산출물:** 칩 식별과 호환 판정, 충전 금지 bit, 활성화/비활성 결정 기록.
- **자동 검증:** 관련 회로·datasheet와 현재 I2C ownership 경계를 정적 검사한다.
- **수동 검증:** 사용자가 실물 marking과 안전한 I2C read-only probe 결과를 확인한다.
- **종료 조건:** [ ] 칩 식별 [ ] register 호환 [ ] CR1220 안전 정책 [ ] 사용자 확인.
- **의존:** 실제 보드 접근.

### R1. RTC adapter 구현 — `pending`

- **목표:** 확인된 칩에 한해 안전한 read/write adapter를 제공한다.
- **범위:** Board public facade, private register driver, HMI time composition과 codec tests.
- **산출물:** bounded RTC facade, 오류 상태, charge-disable/readback, 조건부 조립.
- **자동 검증:** codec·오류·경계 tests, Windows 회귀, HMI compile과 isolation을 통과한다.
- **수동 검증:** 없음.
- **종료 조건:** [ ] public boundary [ ] charge-disable [ ] tests [ ] compile.
- **의존:** R0 complete.

### R2. 실기기 수용 — `pending`

- **목표:** RTC 유지 시각과 SNTP 보정 경로를 실제 장치에서 확인한다.
- **범위:** 부팅값, 수동 설정, 네트워크 보정, drift 조건, 전원 재인가와 오류 상태.
- **산출물:** 실기기 로그와 사용자 수용 기록.
- **자동 검증:** 최종 Windows check, HMI compile과 isolation을 다시 통과한다.
- **수동 검증:** CR1220 장착, 충전 금지 bit, 전원 재인가 유지와 필요한 경우에만 RTC가
  기록되는지 확인한다.
- **종료 조건:** [ ] 실제 RTC 검출 [ ] 전원 유지 [ ] SNTP 보정 [ ] 오류 처리
  [ ] 사용자 승인.
- **의존:** R1 complete.

## 위험과 완화

| 위험 | 완화 |
| --- | --- |
| 칩·revision 오판 | R0 근거가 없으면 adapter를 계속 비활성화한다. |
| CR1220 충전 | charge-disable 설정과 readback을 활성화의 필수 gate로 둔다. |
| UI/driver 결합 | UI는 time snapshot만 소비하고 raw I2C는 Board 내부에 둔다. |
| 잦은 RTC 쓰기 | 기존 drift threshold와 24시간 동기화 정책을 유지한다. |

## Automated Verification Checklist

- [ ] RTC public header에 Arduino, `Wire`, Board internal type이 노출되지 않는다.
- [ ] register codec, VLF와 read/write 오류 tests가 통과한다.
- [ ] `python tools/project.py check --host windows --profile sample-suite`가 통과한다.
- [ ] `python tools/project.py build --host hmi --profile sample-suite`가 통과한다.
- [ ] `python tools/project.py check --scope isolation`이 통과한다.
- [ ] upload·flash는 Codex가 실행하지 않는다.

## Manual Verification Checklist

- [ ] 실제 보드 revision과 RTC 칩 marking을 확인한다.
- [ ] CR1220 충전 enable bit가 꺼져 있고 readback이 일치한다.
- [ ] 전원 재인가 뒤 RTC 시각으로 system clock이 시작한다.
- [ ] 네트워크 보정은 필요한 경우에만 RTC를 기록한다.
- [ ] RTC 미검출·시각 무효·I2C 오류를 화면과 로그가 정확히 설명한다.
- [ ] 사용자가 실제 RTC 활성화를 승인한다.
