# S10 계획: 날짜·시각, RTC와 네트워크 동기화 Page

작성: 2026-11-11. 갱신: 2026-11-12. 상태: `complete` (D0~D4 `complete`).
의존: S04 Wi-Fi 기반과 sample-suite 공통 UI. S08과 독립적이며 S09 완료를 요구하지 않는다.

2026-11-11 사용자 결정으로 실기기 확인은 현재 수행할 수 없다. 소프트웨어 구현과 자동
검증을 먼저 진행하고, RTC 칩 식별·CR1220 안전 bit·전원 재인가·실제 SNTP 수용은 D4의
하드웨어 gate로 이동한다. 칩 호환성이 확인되기 전 HMI는 RTC register adapter를 활성화하지
않고 `미검출/미지원` 상태를 명시적으로 제공한다.

## 1. 목적과 결정 요약

`date-time` 독립 page에서 현재 날짜·시각, 시간 출처, RTC 상태와 네트워크 동기화 상태를
확인하고 수동으로 날짜·시각을 설정할 수 있게 한다. 단순 RTC read/write 예제가 아니라
202609에서 바뀐 다음 정책을 도메인 중립 기반으로 먼저 이식한다.

- 유효한 RTC가 있으면 부팅 시 system clock과 화면의 초기값으로 사용한다.
- IP를 얻으면 SNTP로 보정하고 성공 후 24시간마다 다시 동기화한다.
- 네트워크 시각과 RTC 차이가 정책 threshold보다 클 때만 RTC를 기록한다.
- 수동 설정은 system clock을 먼저 적용하고 RTC 기록 실패를 부분 성공으로 구분한다.
- 날짜·시각을 NVS fallback으로 저장하지 않는다.
- Windows는 OS 시각을 변경하지 않는 가상 port와 결정적인 fixture를 사용한다.

## 2. 202609에서 가져올 기반과 제외 범위

| 구분 | 가져올 내용 | 그대로 가져오지 않는 내용 |
| --- | --- | --- |
| 날짜 모델 | `DateTimeValue`, 윤년·월 길이·요일·유효성 검사 | 제품 화면 문구와 설정 저장 순서 |
| 동기화 core | `TimeSyncController`, source/status/snapshot, retry·24시간 정책 | product profile 전역 객체와 화면 flow |
| port | RTC/system/network/connectivity/update/manual port | ESP/Windows 구현을 공통 module에 혼합 |
| RTC facade | `Clock` read/write/status와 명시적 오류 | `TwoWire`, register, 칩 주소의 공개 노출 |
| HMI RTC | RX8130 계열 register adapter와 CR1220 비충전 안전 설정 | 보드 revision 확인 없이 칩을 단정한 복사 |
| HMI network | ESP system clock·SNTP adapter, 기존 Wi-Fi `has_ip` 연결 | Wi-Fi credential/scan 정책 복제 |
| Windows | fake RTC/system/network/connectivity fixture | PC OS clock 변경 |
| UI | 날짜·시각 편집기와 상태 표시 패턴 | 202609 제품명·상품 header·설정 화면 |

202609 소프트웨어는 I2C `0x32`를 RX8130CE로 다룬다. JC1060P470C 공식 사양은 RTC와
CR1220 holder를 확인하지만 공개 자료와 board revision에 따라 칩 표기가 다를 수 있다.
따라서 이식 전 실제 보드 revision·칩 식별 또는 register 호환 근거를 D0 gate에서 확정한다.

## 3. 목표 구조와 책임

- `modules/time/`: 날짜 계산, 동기화 정책, port와 bounded snapshot을 소유한다.
- `boards/jc1060p470c/`: Board-owned RTC facade와 private register driver를 소유한다.
- `platform/hmi/time/`: RTC facade, ESP system clock, SNTP와 connectivity adapter를 조립한다.
- `platform/windows/time/`: OS 비변경 fake port와 실패·지연 fixture를 제공한다.
- `samples/suite/pages/date_time/`: page state, 편집 초안, 상태 표시와 사용자 action을 소유한다.
- sample-suite composition root가 controller·adapter 수명과 tick 순서를 조립한다.

UI는 현재 날짜·시각, `RTC`·`수동 설정`·`네트워크`·`미설정` source, RTC 오류,
동기화 중/성공/재시도 상태를 표시한다. 사용자는 날짜·시각을 편집하고 적용할 수 있지만,
임의 timezone 선택과 NTP server 편집은 이번 범위에서 제외한다. 기본 timezone은 profile이
명시하는 `Asia/Seoul` 정책으로 둔다.

## 4. 단계

### D0. 202609 시간 기반과 RTC 근거 감사 — `complete`

- **목표:** 현재 보드에 맞는 RTC adapter와 이식할 core 계약을 확정한다.
- **범위:** 202609 date/time model, time sync, Clock/RX8130, HMI/Windows adapter, 관련 tests,
  JC1060P470C 공식 사양과 현재 board revision.
- **산출물:** 이식/수정/제외 표, RTC 칩·register 호환 판정, startup/tick/lifetime graph.
- **자동 검증:** 202609 include/caller/test와 현재 Board I2C ownership을 검색해 경계를 기록한다.
- **수동 검증:** 없음. 실물 board revision·칩 식별·register readback은 D4로 이동한다.
- **종료 조건:** [x] core 후보 확정 [x] 현재 문서 근거와 미확정 항목 구분
  [x] 미확정 driver 비활성 정책 [x] CR1220 충전 금지 정책 유지 [x] NVS fallback 제외.
- **의존:** 없음.

**2026-11-11 감사 근거**

| 대상 | 판정 | 근거와 S10 적용 |
| --- | --- | --- |
| `DateTimeValue`와 달력 함수 | 수정 이식 | 202609의 윤년·월 길이·요일·초 진행 계약을 `modules/time/`의 고정 크기 값으로 이식한다. |
| `TimeSyncController`와 ports | 수정 이식 | RTC→수동→네트워크 우선순위, IP edge, timeout/backoff, 24시간 재동기화와 callback generation 경계를 보존한다. 현재 sample-suite 조립 계약에 맞게 namespace와 sink를 단순화한다. |
| Windows virtual ports | 수정 이식 | PC OS clock을 변경하지 않고 fault·network time을 결정적으로 주입한다. |
| 202609 제품 settings/profile | 제외 | 제품명·화면 flow·NVS `dt_*`·product/factory profile 조립을 가져오지 않는다. |
| JC1060P470C RTC 회로 | 조건부 | 회로 추출문은 `RTC_DAT/SDA1`, `RTC_CLK/SCL1`, `VRTC`, `CR1220`을 확인하지만 칩명과 register 호환성은 확인하지 못했다. |
| RX8130 register driver | D4 확인 전 비활성 | 202609의 `0x32`, VLF, INIEN/CHGEN 계약은 codec test 후보로만 유지한다. 실물/명확한 원문 근거 전에는 HMI runtime에서 선택하지 않는다. |
| Board I2C ownership | 유지 | 현재 `Board`가 GPIO 7/8의 `TwoWire touch_i2c_`와 `I2cTransaction`을 소유한다. RTC도 확인 후 이 bus를 빌려 쓰며 별도 I2C owner를 만들지 않는다. |

시작·수명 순서는 `Board shared I2C → 조건부 Clock facade → platform ports →
TimeSyncController::begin → page snapshot`으로 고정한다. 매 tick은 platform callback generation을
먼저 갱신하고 controller가 이를 소비한 뒤 page가 snapshot만 읽는다. UI와 공통 module은
`TwoWire`, ESP SNTP, Board internal을 알지 않는다.

### D1. 날짜 모델과 동기화 core — `complete`

- **목표:** 플랫폼과 UI에 독립적인 시간 정책을 이식·수정한다.
- **범위:** `modules/time/`과 unit tests. Board/UI/network SDK 제외.
- **산출물:** 날짜 계산, ports, policy, controller, snapshot, manual apply 결과와 size guard.
- **자동 검증:** 윤년·월말·연말, RTC 우선, IP 획득, timeout/retry, 24시간 재동기화,
  drift threshold, 수동 부분 성공과 `uint32_t` wraparound tests를 통과한다.
- **수동 검증:** 없음.
- **종료 조건:** [x] 동적 할당 0 [x] controller 크기 상한 통과 [x] NVS 의존 0
  [x] 결정적 fixture tests 통과.
- **의존:** D0 complete.

**2026-11-11 검증 근거:** `modules/time/`에 `DateTimeValue`, `DateTimeModel`,
`TimeSyncController`와 함수 port를 추가했다. 윤년·월말·연말, RTC→network 우선순위,
IP edge, timeout/backoff, 24시간 due, `uint32_t` wraparound, 2초 drift 경계와 수동 부분 성공을
`hmi_time_date_time`, `hmi_time_sync_controller`에서 검증했다. 직접 검사 2/2와 Windows
sample-suite 46/46, isolation 검사가 통과했다. controller는 256바이트, snapshot은 40바이트,
날짜 model은 24바이트 상한이며 동적 컨테이너·할당·플랫폼/UI/internal include 검색은 0건이다.

### D2. Windows와 HMI adapter — `complete`

- **목표:** 같은 core에 Windows fixture와 HMI system clock/SNTP를 연결하고 미확정 RTC를
  안전한 unsupported port로 표현한다.
- **범위:** `platform/{windows,hmi}/time/`, connectivity adapter와 profile wiring. Board public
  Clock과 private register driver는 D4에서 칩 호환 근거를 확보한 뒤 조건부로 추가한다.
- **산출물:** Windows virtual RTC/system/network fixture, HMI system/network/connectivity
  adapter, unsupported RTC 조립과 안전한 초기화.
- **자동 검증:** native RTC codec/status tests, public header boundary 검사, Windows fake 시나리오,
  affected Windows/HMI compile을 통과한다.
- **수동 검증:** 없음. 실제 RTC·CR1220·SNTP 확인은 D4 하드웨어 gate에서 수행한다.
- **종료 조건:** [x] UI/internal 직접 의존 0 [x] Windows OS clock 변경 0
  [x] 실제 register adapter는 확인된 board에만 enable [x] adapter tests/build 통과.
- **의존:** D1 complete. 미확정 RTC는 명시적 unsupported adapter로 조립한다.

**2026-11-11 검증 근거:** Windows는 `VirtualTimeSyncPorts`가 가상 RTC·system·network
시각을 소유하고 PC OS clock을 변경하지 않는다. HMI는 ESP system clock과 SNTP callback
generation만 adapter로 감싸며 RTC port는 비워 `not_detected`로 표현한다. profile 생성기는
`time` module을 Windows/HMI에 분리 조립하고 composition root가 `TimeRuntime` 한 포인터만
page에 전달한다. 이 추가 포인터로 활성 page 객체 상한은 기존 page별로 8바이트씩만
증가했으며, time model/controller/runtime은 각각 24/256/16바이트 상한이다. time 전용 3개
검사, Windows 전체 검사, HMI compile과 public-header/internal 경계 검색이 통과했다.

### D3. `date-time` page — `complete`

- **목표:** 현재 시각·출처·상태와 수동 설정을 한 page에서 제공한다.
- **범위:** `samples/suite/pages/date_time/`, page registry, i18n/typography와 interaction tests.
- **산출물:** 상태 summary, 날짜·시각 editor, 적용 결과, 네트워크 동기화/재시도 표시.
- **자동 검증:** 유효/무효 입력, 월말 보정, 적용 성공·RTC 부분 실패, source/status 갱신,
  locale/theme, page 재진입/destruction tests를 통과한다.
- **수동 검증:** 1024×600 Windows에서 숫자, 상태, 오류와 조작 영역을 확인했다. 실제 LCD와
  touch 확인은 D4 실기기 gate로 이동한다.
- **종료 조건:** [x] 현재값과 편집 초안 분리 [x] 저장 성공 오표시 0 [x] 두 locale parity
  [x] HOME/재진입 정상 [x] 202609 기초설정1의 초 편집·진행·연도 경계 반영
  [x] 상태 항목별 시각 구획 [x] 하단 안내 좌측 배치와 상하 중앙 정렬.
- **의존:** D2 complete.

**2026-11-11 검증 근거:** `date-time` page는 현재값과 편집 초안을 분리하고
년·월·일·시·분·초 증감, 월말/윤년 보정, 현재값 복원과 적용을 제공한다. source·RTC·network
상태와 system 적용/RTC 기록 부분 성공을 서로 다른 문구로 표시한다. 한국어/영어·theme·
재진입/destruction을 `hmi_sample_suite_date_time_state`,
`hmi_sample_suite_date_time_interaction`에서 검증했다.

202609 `기초설정1`과 다시 비교해 초 편집, 화면을 보고 있는 동안 편집 초안의 초 단위 진행,
2000~2099 연도 clamp와 월말 보정을 반영했다. 현재값/편집값 분리와 RTC 부분 성공 표시는 현재
구조가 더 명확하므로 유지했고, 제품 전용 header와 사용자명은 가져오지 않았다. 현재
sample-suite의 `+/-` 조작 형식도 다른 page와 일관성을 위해 유지했다. 사용자 피드백에 따라
현재 시각·시간 출처·RTC 상태·네트워크 동기화를 각각 테두리가 있는 독립 구획으로 분리했다.
하단 안내는 고정 높이 label 대신 내용 높이로 계산하고, 다른 page의 status처럼 좌측에 둔 채
세로축만 중앙에 맞춘다. interaction test는 네 상태가 서로 다른 구획인지, 좌측 배치인지와
footer와의 세로 중심 차이가 2px 이내인지 검증한다.
`_artifacts/s10/date-time-grouped-left-footer.png`의 1024×600 Windows 화면에서 여섯 편집
필드, 네 상태 구획과 하단 안내의 잘림·겹침·정렬을 확인했다. Windows 전체 49/49도 통과했다.

### D4. 통합과 수용 — `complete`

- **목표:** 네트워크·수동 설정 경로가 동작하고 다른 sample과 격리됨을 확인한다.
- **범위:** sample-suite Windows check/tests, HMI build, isolation, README와 네트워크 기반
  실기기 확인. 실제 RTC register adapter 활성화는 S10 범위에서 제외한다.
- **산출물:** build/test, RAM/flash, 실제 네트워크 날짜·시각 갱신과 사용자 수용 기록.
- **자동 검증:** affected tests, Windows profile check, HMI build, isolation과 diff check를 통과한다.
- **수동 검증:** Wi-Fi 연결 뒤 네트워크 기반 날짜·시각 갱신과 page 표시를 확인하고 사용자가
  S10 완료를 승인한다.
- **종료 조건:** [x] 모든 자동 검사 [x] 메모리 기록 [x] 실기기 네트워크 시간 경로
  [x] 사용자 수용.
- **의존:** D3 complete.

**2026-11-11 자동 통합 근거:** Windows profile 49/49, Python generator/manifest 58/58,
typography catalog 321개 parity/stale 검사와 네 profile isolation 검사가 통과했다. ESP32-P4
`hmi-sample-suite` compile은 RAM 88,080/512,000바이트(17.2%), Flash
1,605,382/6,553,600바이트(24.5%)로 성공했다. upload/flash는 실행하지 않았다. 남은 blocker는
2026-11-12 사용자가 실기기에서 네트워크 기반 날짜·시각 갱신을 확인하고 S10 완료를
승인했다. 실제 RTC 칩·register·CR1220 유지 동작은 검증되지 않았으며
`_plan/2026-11/20261112_001_RtcHardwareEnablement.md`의 별도 후속 gate로 이동했다.

## 위험과 완화

| 위험 | 완화 |
| --- | --- |
| RTC 칩·board revision 오판 | D0에서 실물/공식 근거를 gate로 두고 register adapter를 조건부 조립 |
| CR1220에 충전 설정 | 202609의 charge-disable/readback 검증을 보존하고 실패 시 사용 금지 |
| NTP callback에서 UI/I2C 접근 | callback은 generation만 갱신하고 controller tick에서 처리 |
| RTC·system clock·화면 값 불일치 | source/revision 한 벌을 controller가 발행하고 UI는 snapshot만 소비 |
| 수동 설정의 부분 성공 은폐 | system 적용과 RTC write 결과를 별도 enum으로 표시 |
| 장시간 timer wraparound | 단조 `uint32_t` elapsed 차 방식과 wraparound unit test |

## Automated Verification Checklist

- [x] 202609 시간 기반의 이식/수정/제외 표와 RTC compatibility 근거가 기록된다.
- [x] 날짜 모델 tests가 윤년, 월말, 연말과 잘못된 날짜를 검증한다.
- [x] 동기화 tests가 RTC 우선, IP 획득, timeout/backoff, 24시간 재동기화와 drift를 검증한다.
- [x] 수동 설정의 성공, RTC write 부분 실패와 system write 실패가 구분된다.
- [x] `TimeSyncController`와 snapshot의 size guard가 통과하고 큰 stack buffer가 없다.
- [x] public time/Clock header에 Board internal, Arduino, Wire, ESP type이 노출되지 않는다.
- [x] Windows adapter가 PC OS 시각을 변경하지 않는다.
- [x] `python tools/project.py check --host windows --profile sample-suite`가 통과한다.
- [x] `python tools/project.py check --scope isolation`이 통과한다.
- [x] 영향받은 HMI build가 통과하며 upload/flash는 실행하지 않는다.

## Manual Verification Checklist

- [x] 사용자가 실기기에서 네트워크 기반 날짜·시각 갱신을 확인한다.
- [x] 사용자가 S10의 현재 범위와 RTC 비활성 상태를 포함해 완료를 승인한다.

## S10 범위 밖 후속 작업

실제 RTC adapter 활성화, CR1220 충전 금지 설정과 전원 재인가 유지 검증은 완료된 S10을
다시 열지 않고 `_plan/2026-11/20261112_001_RtcHardwareEnablement.md`에서 별도로 진행한다.
