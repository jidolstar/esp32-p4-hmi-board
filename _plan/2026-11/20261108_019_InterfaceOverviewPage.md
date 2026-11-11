# S09 계획: 인터페이스 기반 목록·상세 안내 Page

작성: 2026-11-08. 갱신: 2026-11-11. 상태: `pending`.
의존: S01 sample-suite 기반. S08 `camera-gallery`와 독립적으로 병행 가능.

## 1. 목표와 범위

`interfaces-info`는 Dashboard에 등록되는 한 개의 page다. page 내부 첫 화면은 인터페이스
목록이며, 항목을 누르면 같은 page가 소유하는 상세 화면으로 전환한다. 상세 화면의 뒤로
가기는 목록으로 돌아가고 Dashboard의 HOME 동작은 기존 sample-suite 규칙을 유지한다.

실제 후속 기능이 사용할 수 있는 도메인 중립 공개 계약은 코드로 가져오고, 제품 protocol,
pin map, worker와 실장 장치 동작은 가져오지 않는다. 이미 구현된 USB·저장소 기능은 새로
복제하지 않고 현재 위치와 사용 경계를 설명한다.

## 2. 목록 제목

| 순서 | 목록 제목 | 상태 표시 | 상세 화면의 핵심 |
| --- | --- | --- | --- |
| 1 | RS485 통신 | 기반 제공 · 장치 protocol 미구현 | 설정, frame 교환 port, worker/codec을 연결하는 위치 |
| 2 | 디지털 GPIO | 기반 제공 · pin 기능 미지정 | 입력·출력 설정, pull, read/write와 제품 pin map 분리 |
| 3 | PWM 출력 | 기반 제공 · 출력 장치 미지정 | 주파수·해상도·duty, attach/stop/detach와 bounded channel |
| 4 | USB CDC 명령 | 연결 경계 제공 · command 미구현 | 단일 USB reader와 dispatcher 확장, 명령 protocol 소유 위치 |
| 5 | 저장소 파일 접근 | 구현됨 · S05와 공유 | 공개 file API, exclusive lease, bounded stream과 SD generation |
| 6 | USB 대용량 저장소 | 구현됨 · S05에서 실행 | HMI filesystem과 PC MSC의 배타 소유권, 진입·복귀 흐름 |
| 7 | USB 자산 업데이트 | 구현됨 · profile 자산 전용 | profile root 제한, manifest/hash 검증, command/MSC와의 차이 |
| 8 | 펌웨어 업데이트 기반 | 핵심 코드 제공 · 실행 기능 미구현 | 계획 검증, 다운로드, journal, 설치·부팅 복구 core의 연결 방법 |

날짜·시각과 RTC는 S10 `date-time` 독립 page로 분리한다. 펌웨어 업데이트는 별도 page로
확장하지 않고 S09의 설명 항목으로 둔다. 다만 설명이 빈 껍데기가 되지 않도록 202609의
도메인 중립 update core는 현재 저장소 경계에 맞게 수정 이식한다. S09에서는 실제 server
조회, flash 기록, 재부팅과 rollback을 실행하는 UI는 제공하지 않는다.

TMP102 온도 센서는 이 보드의 기본 장착 기능이 아니다. 202609의 `Temperature` facade는
공유 I2C `0x48`에 외부로 연결한 케이스 온도 센서를 communication-test에서 읽기 위한
코드다. JC1060P470C 공식 사양에는 RTC·I2C 확장 단자는 있지만 TMP102 장착 근거가 없으므로
S09 목록과 공통 Board facade로 이식하지 않는다.

## 3. 상세 화면 공통 구조

모든 상세 화면은 같은 레이아웃과 정보 순서를 사용한다.

1. 제목과 `구현됨`·`기반 제공`·`미구현` 상태 badge
2. 이 기능이 해결하는 문제와 사용할 때
3. 현재 저장소에서 제공하는 공개 계약
4. 후속 adapter와 제품별 설정을 둘 위치
5. 조립 순서와 짧은 호출 예시
6. 소유권·수명·동시성·buffer 제한
7. 구현하지 않은 범위와 관련 sample page

상세 화면에는 실제 장치를 제어하는 것처럼 보이는 toggle, 송신, pin 변경 button을 두지
않는다. 코드 예시는 고정 길이의 읽기 전용 text block으로 표시하고 UI 문자열은
sample-suite catalog가 소유한다.

## 4. 기반 이식 원칙

- RS485, GPIO, PWM, USB command는 서로 다른 좁은 계약으로 유지한다.
- 공통 상태는 `available`, `not_implemented`, `busy`, `invalid_argument`, `io_error`처럼
  호출자가 처리할 수 있는 결과로 표현한다.
- 보드 pin, UART handle, GPIO number, USB descriptor, TinyUSB/Arduino type과 제품 protocol은
  `modules/interfaces/` 공개 header에 노출하지 않는다.
- HMI adapter는 `boards/jc1060p470c/` 또는 `platform/hmi/`가 소유하고 Windows는 결정적인
  unsupported/fixture adapter를 제공한다.
- UI는 공개 descriptor와 문서 model만 소비하며 Board internal을 include하지 않는다.
- 사용하지 않는 202609 worker, Modbus codec, motor adapter와 제품 command를 복사하지 않는다.
- S05 storage/MSC와 Board 자산 updater는 기존 구현을 단일 source of truth로 유지한다.
- update core는 제품명·상품/공장 흐름·운영 server schema와 UI를 제거하고 port 기반으로
  수정 이식한다. firmware write/reboot를 호출하는 HMI adapter는 S09에 조립하지 않는다.

참조 원본:

- `../dev_202609_hmi/src/hardware/rs485.*`
- `../dev_202609_hmi/src/hardware/gpio.*`
- `../dev_202609_hmi/src/hardware/pwm.*`
- `../dev_202609_hmi/src/platform/hmi/assets/asset_serial_transfer.*`
- `../dev_202609_hmi/src/app/update/update_model.*`
- `../dev_202609_hmi/src/app/update/artifact_download.*`
- `../dev_202609_hmi/src/app/update/durable_record.*`
- `../dev_202609_hmi/src/app/update/install_{journal,transaction}.*`
- `../dev_202609_hmi/src/app/update/asset_{set,store,lifecycle_policy}.*`
- `../dev_202609_hmi/src/app/update/{firmware_port,maintenance_gate,boot_coordinator}.*`
- `boards/jc1060p470c/storage.*`
- `boards/jc1060p470c/internal/asset_usb_updater.*`

## 5. 단계

### I0. 공개 기반과 제목 확정 — `pending`

- **목표:** 여덟 제목마다 가져올 계약, 기존 구현을 참조할 항목, 제외할 제품 코드를 확정한다.
- **범위:** 위 참조 원본, 현재 Board/storage/asset updater와 S05. production code 변경 없음.
- **산출물:** 이식/참조/제외 표와 include dependency graph.
- **자동 검증:** public header의 include와 실제 호출자를 검색하고 internal/platform type 노출을
  분류한다.
- **수동 검증:** 제목과 `구현됨`·`기반 제공` 상태 표현을 사용자가 확인한다.
- **종료 조건:** [ ] 여덟 제목의 근거가 있음 [ ] 온도/제품 protocol이 제외됨
  [ ] 가져올 source가 public contract 최소 집합으로 고정됨.
- **의존:** 없음. S08과 병행 가능.

### I1. 최소 interfaces module — `pending`

- **목표:** RS485/GPIO/PWM/USB command 후속 구현이 include할 공개 계약을 제공한다.
- **범위:** `modules/interfaces/`, module manifest/CMake, interface unit tests. 실제 HMI driver 제외.
- **산출물:** descriptor, 좁은 port, 공통 결과 타입, unsupported fixture와 size guard.
- **자동 검증:** compile/interface/result/unsupported tests와 공개 header 독립 include 검사를
  통과한다.
- **수동 검증:** 계약이 device protocol이나 제품 pin 의미를 소유하지 않는지 검토한다.
- **종료 조건:** [ ] internal handle 0 [ ] 실제 driver 0 [ ] deterministic unsupported
  [ ] 고정 크기 계약.
- **의존:** I0 complete.

### I2. 도메인 중립 firmware update core — `pending`

- **목표:** 실제 updater를 후속 조립할 수 있는 update core를 제품 도메인 없이 제공한다.
- **범위:** `modules/update/`, deterministic storage/network/firmware fixtures와 unit tests.
  실제 HTTP endpoint, credential, partition write, reboot와 release tooling은 제외한다.
- **산출물:** bounded update model, crypto/download/storage/firmware/control ports,
  durable record, install journal/transaction, asset selection/lifecycle, maintenance gate,
  boot coordinator와 port 기반 engine skeleton.
- **수정 이식:** 202609의 `Purpose::kProductUpdate`·factory 흐름과 `ControlClient` server schema를
  그대로 복사하지 않는다. generic update intent/target과 `UpdateControlPort`로 바꾸고
  CT-A1 ID, endpoint, operator command, 상품 화면 policy를 제거한다.
- **자동 검증:** manifest/plan binding, signature/hash, bounded download, pause/cancel,
  corrupt journal, power-cut transition, asset mismatch, boot accept/reject와 unsupported port
  tests를 통과한다.
- **수동 검증:** 이식된 API만으로 후속 server·firmware adapter를 별도 module에서 구현할 수
  있는지 검토한다.
- **종료 조건:** [ ] 제품/공장 고정값 0 [ ] 운영 endpoint/credential 0
  [ ] 실제 장치 mutation 0 [ ] bounded model/heap/stack 검증 [ ] core tests 통과.
- **의존:** I1 complete.

### I3. 목록·상세 page — `pending`

- **목표:** 한 page 안에서 여덟 항목의 목록→상세→목록 탐색을 제공한다.
- **범위:** `samples/suite/pages/interfaces_info/`, page registry, sample-suite i18n과 tests.
- **산출물:** 목록 view, 공통 상세 view, 여덟 bounded descriptor와 navigation state.
- **자동 검증:** 목록 순서, 각 상세 진입, 뒤로 가기, HOME, 재진입, locale/theme와 destruction
  tests를 통과한다.
- **수동 검증:** 1024×600에서 제목·상태·본문·예제가 읽히고 실행 기능으로 오해되지 않는다.
- **종료 조건:** [ ] 여덟 항목 모두 접근 가능 [ ] 가짜 실행 control 0
  [ ] 목록/상세 상태 누수 0 [ ] 두 locale parity.
- **의존:** I2 complete.

### I4. 관련 기능 연결 설명 — `pending`

- **목표:** storage, MSC, asset update와 update core를 복제하지 않고 실제 소유 코드와
  사용 순서를 설명한다.
- **범위:** 상세 descriptor, README 링크와 기존 S05/Board API. 기존 기능 동작 변경 제외.
- **산출물:** storage/USB 세 기능의 소유권·배타성·복구 흐름, USB CDC command와의 차이,
  update core에 server/firmware adapter를 연결하는 순서.
- **자동 검증:** 링크/경로 검사와 동일 기능의 새 구현·source 중복 부재를 확인한다.
- **수동 검증:** 사용자가 MSC, 자산 갱신, 일반 command를 서로 구분할 수 있다.
- **종료 조건:** [ ] 실제 API와 설명 일치 [ ] 중복 구현 0 [ ] 오해 가능한 지원 표현 0
  [ ] firmware update가 실행 가능하다고 표시하지 않음.
- **의존:** I3 complete.

### I5. 통합 검증과 수용 — `pending`

- **목표:** Windows와 HMI에서 page가 빌드되고 다른 sample과 격리됨을 확인한다.
- **범위:** sample-suite profile check, 영향 테스트, HMI build, isolation, README.
- **산출물:** 자동 검증 기록, 화면 캡처와 사용자 수용 기록.
- **자동 검증:** affected Windows check/test, HMI build, isolation, `git diff --check`를 통과한다.
- **수동 검증:** 실제 LCD/touch에서 목록 스크롤, 상세 전환, 뒤로/HOME, 문구 가독성을 확인한다.
- **종료 조건:** [ ] 자동 검사 통과 [ ] RAM/flash 변화 기록 [ ] 사용자 수용.
- **의존:** I4 complete.

## Automated Verification Checklist

- [ ] 이식 표가 public contract, 기존 구현 참조와 제외할 internal/product source를 구분한다.
- [ ] RS485/GPIO/PWM/USB command 공개 header가 Board internal과 플랫폼 SDK를 노출하지 않는다.
- [ ] unsupported adapter가 결정적인 `not_implemented`를 반환한다.
- [ ] update core에 제품 ID, factory/operator policy, endpoint, credential과 실제 flash 호출이 없다.
- [ ] update core의 signature/binding/journal/power-cut/boot accept·reject tests가 통과한다.
- [ ] 목록 여덟 항목과 상세 여덟 화면의 navigation/re-entry/destruction test가 통과한다.
- [ ] page에 실행 가능한 가짜 hardware control이 없다.
- [ ] storage/MSC/asset updater 구현이 중복되지 않는다.
- [ ] 두 locale의 key와 placeholder 계약이 같다.
- [ ] `python tools/project.py check --host windows --profile sample-suite`가 통과한다.
- [ ] `python tools/project.py check --scope isolation`이 통과한다.
- [ ] 영향받은 HMI profile build가 통과하고 upload/flash는 실행하지 않는다.

## Manual Verification Checklist

- [ ] 여덟 목록 제목과 상태가 한눈에 구분된다.
- [ ] 상세 화면의 조립 방법만 보고 후속 adapter와 제품 구현 위치를 찾을 수 있다.
- [ ] RS485/GPIO/PWM/USB command가 실제 지원된다고 오해할 표현이 없다.
- [ ] USB command, MSC와 자산 업데이트의 목적·소유권 차이가 명확하다.
- [ ] 펌웨어 업데이트 상세가 이식된 core와 후속 adapter 조립 방법을 정확히 설명한다.
- [ ] 펌웨어 update button이나 실제 설치 성공으로 오인할 표현이 없다.
- [ ] 실제 LCD/touch에서 목록 스크롤, 상세 진입, 목록 복귀와 HOME이 자연스럽다.
- [ ] 사용자가 S09 범위와 RTC 독립 page 분리를 승인한다.
