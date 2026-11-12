# S09 계획: 인터페이스 기반 목록·상세 안내 Page

작성: 2026-11-08. 갱신: 2026-11-12. 상태: `complete` (I0~I6 `complete`).
의존: S01 sample-suite 기반. S08 `camera-gallery`, S10 `date-time`의 남은 실기기
수용과 독립적으로 병행 가능.

## 0. 착수 판정

지금 S09를 시작해도 무리가 없다.

- S08은 camera sensor·frame·SD image 흐름에 한정되어 있고 현재 C0 실기기 판정만
  `blocked`다. S09가 추가하는 module, page ID와 registry order는 겹치지 않는다.
- S10은 `modules/time/`, time platform adapter와 `date-time` page의 소프트웨어 구현이 끝났고
  D4 RTC·SNTP·LCD/touch 수용만 남았다. S09는 time runtime이나 RTC adapter를 사용하지 않는다.
- S09는 `AppContext`, `Board` public facade와 기존 page의 runtime 조립을 늘리지 않는다.
  정적인 capability descriptor를 표시하고 새 공통 module의 계약·결정적 fixture만 검증한다.
- S08은 registry order 80, S09는 90, S10은 100으로 유지한다. 따라서 등록 순서 충돌도 없다.

착수 시 roadmap의 S09만 `in_progress`로 바꾸고 S08·S10은 각자의 `blocked` hardware gate를
그대로 유지한다. S09의 실제 LCD/touch 확인도 마지막 I6으로 분리해, 장치가 없는 동안 I0~I5
소프트웨어 단계가 멈추지 않게 한다.

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

## 5. 목표 파일과 공개 계약

### 5.1 `interfaces` module

다음 최소 파일만 새로 둔다.

- `modules/interfaces/include/hmi/interfaces/result.hpp`: `ok`, `not_implemented`, `busy`,
  `invalid_argument`, `io_error` 결과와 오류 원인 전달 규칙
- `modules/interfaces/include/hmi/interfaces/capability.hpp`: 기능 ID, 제공 수준과 bounded
  capability snapshot
- `modules/interfaces/include/hmi/interfaces/rs485_port.hpp`: 설정, bounded frame read/write와
  취소 계약
- `modules/interfaces/include/hmi/interfaces/gpio_port.hpp`: mode/pull/read/write 계약
- `modules/interfaces/include/hmi/interfaces/pwm_port.hpp`: attach/configure/duty/stop/detach 계약
- `modules/interfaces/include/hmi/interfaces/usb_command_port.hpp`: 단일 reader가 넘기는 bounded
  command frame과 dispatcher 경계
- `modules/interfaces/src/capability.cpp`: descriptor 검증과 상태 문자열에 독립적인 변환
- `modules/interfaces/tests/interfaces_contract_test.cpp`: public header 단독 compile, 결과 전달,
  unsupported fixture와 구조체 크기 상한 검증

port는 `context + 함수 포인터` 형태의 고정 크기 비소유 view로 정의한다. adapter가 수명과
동시성을 소유하며, 호출자는 port보다 오래 사는 adapter만 연결할 수 있다. span은 pointer와
length로 전달하고 module 내부에서 frame 전체를 복사하거나 heap에 보관하지 않는다. 실제 pin,
UART number, USB endpoint와 platform handle은 adapter 생성자 뒤에 숨긴다.

### 5.2 `update` module

202609 원본을 이름만 바꿔 복사하지 않고 다음 도메인 중립 경계로 분해한다.

- `modules/update/include/hmi/update/types.hpp`: artifact ID, version, SHA-256, 크기, 상태와
  generic target/intent
- `modules/update/include/hmi/update/ports.hpp`: control, download transport, staging store,
  signature verifier, firmware writer, boot control과 health 확인 port
- `modules/update/include/hmi/update/manifest.hpp`: bounded manifest/plan parse와 서명·device
  binding·만료 검증
- `modules/update/include/hmi/update/artifact_download.hpp`: 4 KiB 이하 chunk 기반 resume,
  pause/cancel, hash 확인
- `modules/update/include/hmi/update/durable_record.hpp`와 `install_journal.hpp`: sequence·CRC를
  가진 A/B record, power-cut-safe stage/event 전이
- `modules/update/include/hmi/update/asset_store.hpp`와 `asset_lifecycle.hpp`: generation 선택,
  activate/rollback과 stale generation 정리 정책
- `modules/update/include/hmi/update/maintenance_gate.hpp`: 설치 중 상호 배제와 안전한 상태 전이
- `modules/update/include/hmi/update/boot_coordinator.hpp`: pending boot, health 확인,
  accept/reject/rollback 결정
- `modules/update/include/hmi/update/engine.hpp`: 위 port를 호출하는 작은 상태 조정기. network,
  flash와 reboot 구현은 소유하지 않는다.
- `modules/update/src/json_view.*`: 최대 depth·key 수가 제한된 private borrowed-slice parser
- 대응 `src/*.cpp`와 `modules/update/tests/`의 manifest, download, journal, lifecycle, boot,
  memory tests

`Purpose::ProductUpdate`, factory install/update/maintenance, CT-A1 ID, operator status,
`ControlClient` server schema와 product policy는 제거한다. `FirmwarePort`는 공통 계약으로만 남기고
HMI partition writer, 실제 reboot·rollback adapter는 만들거나 조립하지 않는다. manifest와 wire
입력은 최대 64 KiB, I/O chunk는 최대 4 KiB로 유지하며 큰 입력을 task stack에 올리지 않는다.
`InstallPlan`, durable record, coordinator에는 compile-time size guard를 둔다.

| 202609 source | S09 target | 판정 |
| --- | --- | --- |
| `update_model.*`, `platform/shared/update/json_view.*` | `types`, `manifest`, private `json_view` | 제품 enum 제거 후 수정 이식 |
| `artifact_download.*` | `artifact_download.*` | bounded chunk·resume·digest 계약 이식 |
| `durable_record.*`, `install_journal.*`, `install_transaction.*` | durable record와 journal | A/B·CRC·전이 정책 이식 |
| `asset_set.*`, `asset_store.*`, `asset_lifecycle_policy.*` | asset store/lifecycle | generation 계약만 이식 |
| `firmware_port.hpp`, `maintenance_gate.hpp`, `boot_coordinator.*` | ports/gate/boot coordinator | 장치 adapter 없이 계약·정책 이식 |
| `control_client.*`, `product_update_policy.*`, `operator_status.*` | 없음 | server schema·제품/공장 UI 정책이므로 제외 |

### 5.3 `interfaces-info` page와 build wiring

- `samples/suite/pages/interfaces_info/interfaces_info_model.*`: 여덟 topic ID, 상태, i18n key와
  `list/detail` 전환 상태만 소유
- `samples/suite/pages/interfaces_info/interfaces_info_page.*`: 스크롤 목록, 공통 상세 renderer,
  목록 복귀와 HOME 처리
- `samples/suite/pages/interfaces_info/tests/interfaces_info_state_test.cpp`: 순서, 전환,
  재진입과 locale/theme rebuild 상태
- `samples/suite/pages/interfaces_info/tests/interfaces_info_interaction_test.cpp`: 여덟 row tap,
  상세 scroll, back/HOME와 destroy 후 callback 부재
- `samples/suite/page_registry.json`: order 90의 `interfaces-info` 등록
- `samples/suite/i18n/{catalog.json,ko.yaml,en.yaml}`: 제목, badge, summary, 상세 section과 제한 문구
- `samples/suite/profile.json`: `interfaces`, `update` modules와 S09 tests 등록
- `tools/hmi_project/{manifest.py,generate.py}`: 두 module의 허용 목록, source/include 입력
- `tools/tests/{test_manifest.py,test_generation.py}`: 허용 module과 deterministic generated input
- `samples/suite/CMakeLists.txt`: module·page의 native test targets
- `README.md`: 공개 계약 사용 위치, 실제 미지원 범위와 S09 실행 방법

page는 `AppContext`에 새 runtime을 추가하지 않고 compile-time descriptor와 번역 catalog만 읽는다.
따라서 설명 화면 때문에 unsupported hardware object나 update engine을 앱 시작 시 생성하지 않는다.

## 6. UI와 수명 규칙

- 첫 화면은 여덟 행의 세로 스크롤 목록이다. 각 행은 제목, 상태 badge, 1~2줄 요약과 상세
  진입 affordance를 가진다.
- 항목을 누르면 같은 page의 `detail` mode로 바뀐다. split view나 별도 page 등록은 사용하지
  않는다.
- 상세 header의 뒤로 가기는 목록으로, 전역 HOME은 어느 mode에서나 Dashboard로 이동한다.
- 선택 topic ID는 LVGL widget이 아니라 page model에 보관해 locale/theme rebuild 뒤에도 같은
  상세 항목을 다시 만든다. widget pointer, event target과 scroll object는 `destroy()`에서 비운다.
- 상세는 제목·상태, 용도, 제공 API, 연결 위치, 조립 순서, 소유권/상한, 제외 범위의 순서로
  표시한다. 본문은 좌측 정렬하고 paragraph마다 별도 label을 사용하며 줄 간격은 최소 5 px로
  둔다.
- 코드 예시는 짧은 읽기 전용 예제로 제한하고 고정 폭 font 추가는 하지 않는다. 긴 경로와
  symbol은 허용 폭 안에서 wrap하며 본문 전체를 하나의 거대 label로 만들지 않는다.
- 실제 송신, GPIO/PWM 변경, update 설치를 암시하는 button/toggle은 만들지 않는다.

## 7. 단계

### I0. 공개 기반과 제목 확정 — `complete`

- **목표:** 여덟 제목마다 가져올 계약, 기존 구현을 참조할 항목, 제외할 제품 코드를 확정한다.
- **범위:** 위 참조 원본, 현재 Board/storage/asset updater와 S05. production code 변경 없음.
- **산출물:** 이식/참조/제외 표와 include dependency graph.
- **자동 검증:** public header의 include와 실제 호출자를 검색하고 internal/platform type 노출을
  분류한다.
- **수동 검증:** 제목과 `구현됨`·`기반 제공` 상태 표현을 사용자가 확인한다.
- **종료 조건:** [x] 여덟 제목의 근거가 있음 [x] 온도/제품 protocol이 제외됨
  [x] 가져올 source가 public contract 최소 집합으로 고정됨.
- **의존:** 없음. S08과 병행 가능.

**2026-11-11 감사 근거:** 202609의 `hardware/{rs485,gpio,pwm}.*`,
`app/update/*`, `platform/shared/update/json_view.*`의 공개 include, model 크기 guard와 제품 결합
지점을 검색했다. interfaces는 platform type 없는 네 port로 제한하고 update는 model/manifest,
download, durable journal, asset generation, boot recovery만 수정 이식한다. `ControlClient`,
`ProductUpdatePolicy`, `OperatorStatus`, factory/product enum과 endpoint는 제외한다. 현재
`modules/storage/`와 `boards/jc1060p470c/{storage,internal/asset_usb_updater}.*`가 storage/MSC/
자산 갱신의 단일 구현임을 확인했다. 이전 대화에서 사용자가 여덟 제목, TMP102 제외와 RTC의
S10 분리를 승인했다.

### I1. 최소 interfaces module — `complete`

- **목표:** RS485/GPIO/PWM/USB command 후속 구현이 include할 공개 계약을 제공한다.
- **범위:** `modules/interfaces/`, profile generator, manifest tests와 sample-suite test 등록.
  실제 HMI driver와 `AppContext` 조립은 제외한다.
- **산출물:** result/capability, 네 개의 좁은 port, deterministic unsupported fixture와
  compile-time size guard.
- **자동 검증:** `hmi_interfaces_contract`, manifest/generation tests와 공개 header 독립 include
  검사를 통과한다.
- **수동 검증:** 계약이 device protocol이나 제품 pin 의미를 소유하지 않는지 검토한다.
- **종료 조건:** [x] internal handle 0 [x] 실제 driver 0 [x] deterministic unsupported
  [x] 고정 크기 계약 [x] `interfaces` module 입력이 host와 무관하게 deterministic함.
- **의존:** I0 complete.

**2026-11-11 검증 근거:** `modules/interfaces/`에 result/capability와 RS485·GPIO·PWM·USB
command port, unsupported fixture를 추가했다. port는 context+함수 포인터와 호출 중에만 유효한
buffer view만 보관하며 각각 32~40바이트 상한을 갖는다. Board internal·Arduino·FreeRTOS·driver·
platform include 검색은 0건이다. `python -m unittest tools.tests.test_manifest
tools.tests.test_generation -v` 27/27과 `hmi_interfaces_contract` 1/1이 통과했다.

### I2. update model·manifest·download — `complete`

- **목표:** 실제 updater의 입력을 안전하게 검증하고 artifact를 staging할 수 있는 전반부 core를
  제품 도메인 없이 제공한다.
- **범위:** types/ports/private JSON parser, manifest와 plan 검증, bounded download와 fixture.
  실제 HTTP client, endpoint, credential과 flash write는 제외한다.
- **산출물:** bounded update model, control/download/store/crypto/firmware port, strict manifest
  parser, resume 가능한 downloader와 hash 검증.
- **수정 이식:** 202609의 `Purpose::kProductUpdate`·factory 흐름과 `ControlClient` server schema를
  그대로 복사하지 않는다. generic update intent/target과 `UpdateControlPort`로 바꾸고
  CT-A1 ID, endpoint, operator command, 상품 화면 policy를 제거한다.
- **자동 검증:** `hmi_update_manifest`, `hmi_update_download`, `hmi_update_memory`에서 unknown·
  duplicate key, depth/key/size 초과, signature/device binding/expiry, short I/O, resume,
  pause/cancel과 digest mismatch를 검증한다.
- **수동 검증:** public API만으로 후속 server adapter를 별도 module에서 구현할 수 있는지
  검토한다.
- **종료 조건:** [x] 제품/공장 고정값 0 [x] 운영 endpoint/credential 고정값 0
  [x] 실제 장치 mutation 0 [x] 64 KiB 입력·4 KiB chunk 상한 [x] 전반부 tests 통과.
- **의존:** I1 complete.

**2026-11-11 검증 근거:** `modules/update/`에 bounded `Manifest`, strict borrowed-slice JSON,
서명·device/target·expiry binding과 resume downloader를 추가했다. downloader는 caller-owned 최대
4 KiB scratch만 빌리고 기존 partial을 다시 digest한 뒤 이어 쓴다. production source의 vector,
string owner, heap buffer는 0건이다. generator/manifest 27/27, `hmi_update_manifest`,
`hmi_update_download`, `hmi_update_memory` 3/3이 통과했고 unknown/duplicate/depth 초과, signature,
binding, expiry, short I/O, resume, pause/cancel, digest mismatch를 검증했다.

### I3. update journal·lifecycle·boot core — `complete`

- **목표:** 전원 차단과 실패 후에도 설치 상태와 활성 generation을 복구할 수 있는 후반부 core를
  제공한다.
- **범위:** durable A/B record, install journal/transaction, asset store/lifecycle,
  maintenance gate, boot coordinator와 작은 engine. 실제 partition/reboot adapter는 제외한다.
- **산출물:** sequence/CRC record, 유효한 stage transition 표, generation activate/rollback,
  pending boot accept/reject와 port-based coordinator.
- **자동 검증:** `hmi_update_journal`, `hmi_update_lifecycle`, `hmi_update_boot`에서 newest record
  corruption fallback, 각 persist 지점 power cut, illegal transition, active install 보호,
  asset mismatch, health timeout, accept/reject/rollback을 검증한다.
- **수동 검증:** 성공 표시가 health confirmation보다 먼저 나올 수 없고, 실패 상태가 재부팅 뒤
  복원되는지 contract review한다.
- **종료 조건:** [x] heap 소유 모델 0 [x] 큰 자동 buffer 0 [x] active generation 원자적 전환
  [x] 실제 flash/reboot 호출 0 [x] 후반부 tests 통과.
- **의존:** I2 complete.

**2026-11-11 검증 근거:** `DurableRecordStore`, `InstallJournal`, `AssetLifecycle`, bounded
stale-generation visitor, `MaintenanceGate`, 순수 `BootCoordinator`와 port를 호출하지 않는
`Engine`을 추가했다. A/B CRC·sequence·flush/readback, newest corruption fallback, write 실패 시
기존 snapshot 유지, 불법 stage, active install 보호, generation activate/rollback, asset binding,
health timeout, accept/recovery와 end-to-end engine 전이를 검증했다. `hmi_update_journal`,
`hmi_update_lifecycle`, `hmi_update_boot`, `hmi_update_engine` 4/4가 통과했다.

### I4. 목록·상세 page — `complete`

- **목표:** 한 page 안에서 여덟 항목의 목록→상세→목록 탐색을 제공한다.
- **범위:** `samples/suite/pages/interfaces_info/`, page registry, sample-suite i18n과 tests.
- **산출물:** 목록 view, 공통 상세 view, 여덟 bounded descriptor와 navigation state.
- **자동 검증:** `hmi_sample_suite_interfaces_info_state`와
  `hmi_sample_suite_interfaces_info_interaction`에서 목록 순서, 각 상세 진입, 뒤로 가기, HOME,
  재진입, locale/theme rebuild와 destruction을 검증한다.
- **수동 검증:** 1024×600에서 제목·상태·본문·예제가 읽히고 실행 기능으로 오해되지 않는다.
- **종료 조건:** [x] 여덟 항목 모두 접근 가능 [x] 가짜 실행 control 0
  [x] 목록/상세 상태 누수 0 [x] 두 locale parity.
- **의존:** I3 complete. 상세 문구가 최종 public API와 일치해야 한다.

**2026-11-11 검증 근거:** `interfaces-info`를 registry order 90에 등록하고 여덟 항목의
목록·공통 상세 view, scroll, 상세→목록→Dashboard navigation을 구현했다. locale/theme
재구성에서 선택 항목이 유지되고 page 재진입에서는 초기 목록으로 복귀한다.
`hmi_sample_suite_interfaces_info_state`와
`hmi_sample_suite_interfaces_info_interaction` 2/2 및 10회 재진입 검증이 통과했다.
1024×600 Windows의 한국어 light 목록과 영어 dark firmware 상세 capture에서 잘림·겹침이
없고 상세 본문만 scroll되는 것을 확인했다. 두 locale은 395개 key parity를 유지한다.

### I5. 설명 정확성·자동 통합 — `complete`

- **목표:** storage, MSC, asset update와 update core를 복제하지 않고 실제 소유 코드와
  사용 순서를 설명하고 Windows/HMI profile을 통합 검증한다.
- **범위:** 상세 descriptor, README, 기존 S05/Board API, profile check/build/isolation.
  기존 storage/USB 기능 동작 변경은 제외한다.
- **산출물:** storage/USB 세 기능의 소유권·배타성·복구 흐름, USB CDC command와의 차이,
  update core에 server/firmware adapter를 연결하는 순서, 자동 검증 기록과 Windows 화면 캡처.
- **자동 검증:** module/page tests, i18n/typography, Windows sample-suite check, HMI build,
  isolation, 중복 source 검색과 `git diff --check`를 통과한다.
- **수동 검증:** 사용자가 MSC, 자산 갱신, 일반 command를 서로 구분할 수 있다.
- **종료 조건:** [x] 실제 API와 설명 일치 [x] 중복 구현 0 [x] 오해 가능한 지원 표현 0
  [x] firmware update가 실행 가능하다고 표시하지 않음 [x] 자동 검사 통과
  [x] RAM/flash 변화 기록.
- **의존:** I4 complete.

**2026-11-11 검증 근거:** README와 page 상세에 interface port 사용법, update 조합 순서,
USB CDC command·MSC·CDC asset updater의 서로 다른 책임과 실제 adapter 미구현 범위를
기록했다. 설명에 필요한 `FirmwareWriterPort`와 `BootControlPort`도 bounded 비소유 계약으로
추가했으며 실제 partition write/reboot 구현이나 실행 UI는 조립하지 않았다. Windows
`sample-suite`를 최종 생성물로 다시 build한 뒤 profile test 59/59, Python 도구 test 58/58,
interface/update test 8/8, page test 2/2, typography 395개와 user font asset 176개 stale 검사,
네 profile isolation과 `git diff --check`가 통과했다. 공용 모듈의 Board 내부·Arduino·FreeRTOS·
driver 직접 include는 0건이다. 최종 HMI compile-only build는 RAM 88,080/512,000 bytes(17.2%),
flash 1,635,650/6,553,600 bytes(25.0%)다. S10 직전 기준 대비 RAM 변화는 0 bytes,
flash 증가는 30,268 bytes이며 새 core, page와 고정 font를 포함한다. upload/flash는 실행하지
않았다. 최종 capture는 `build/windows-sample-suite/interfaces-info-detail.bmp.list.bmp`와
`build/windows-sample-suite/interfaces-info-detail.bmp`에서 확인했다.

### I6. 실기기 수용 — `complete`

- **목표:** 실제 1024×600 LCD/touch에서 안내 page의 조작성과 가독성을 수용한다.
- **범위:** 목록 scroll, 여덟 상세 진입, 긴 본문 scroll, 뒤로/HOME, locale/theme와 반복 진입.
  RS485/GPIO/PWM/update의 실제 장치 동작은 수용 범위가 아니다.
- **산출물:** LCD/touch 확인 기록과 사용자 완료 승인.
- **자동 검증:** I5 결과를 재사용하며 Codex는 upload/flash를 실행하지 않는다.
- **수동 검증:** 잘림·겹침·오동작·지원 수준 오해가 없고 10회 반복 진입/이탈 뒤 callback이나
  widget 누수가 없는지 확인한다.
- **종료 조건:** [x] LCD layout 승인 [x] touch/navigation 승인 [x] 상태 표현 승인
  [x] 사용자 완료 승인.
- **의존:** I5 complete. 장치 접근이 불가능하면 S09는 I0~I5 complete, I6 `blocked`로 둔다.

**2026-11-11 blocker:** 현재 사용자가 기기 테스트를 위해 S08·S10과 함께 실기기 수용을
미뤄 둔 상태다. firmware는 compile까지만 검증했으며 실제 LCD/touch의 목록 scroll, 여덟
상세 진입, locale/theme와 10회 반복 진입·이탈 승인이 필요하다.

**2026-11-12 재개:** 사용자 요청으로 I6 실기기 수용을 시작했다. Codex는 장치를 변경하지
않고 연결 상태와 build 산출물을 읽기 전용으로 확인하며, firmware upload는 사용자가 직접
수행한다.

**2026-11-12 읽기 전용 확인:** HMI build doctor는 `ready`이고 ESP32-P4 USB serial
`COM19`(VID:PID `303A:1001`)가 연결돼 있다. 수동 upload에 사용할 산출물은
`.pio/build/hmi/hmi-sample-suite/firmware.bin`, 1,699,824 bytes, SHA-256
`2327B1B757815E752827C021509BF5A6DF11ECE72ED6915BAF19F5EAEE979B8C`다. 저장소 규칙에 따라
Codex는 upload를 실행하지 않았다. 사용자의 수동 upload와 LCD/touch 판정 전까지 I6는
`blocked`다.

**2026-11-12 실기기 수용 근거:** 사용자가 S09를 실제 기기에서 확인한 뒤 완료 처리를
명시적으로 요청했다. 이를 목록·상세 layout, touch/navigation, 상태 표현과 반복 사용에 대한
최종 승인으로 기록하고 I6 및 S09를 `complete`로 닫았다.

## 8. 실행 순서와 검증 명령

각 단계는 종료 조건을 충족한 뒤 roadmap과 이 문서의 상태를 갱신한다. 실패한 gate가 있으면
다음 단계로 넘어가지 않고 해당 단계에 원인과 재현 명령을 기록한다.

```powershell
# I1: build-input 계약
python -m unittest tools.tests.test_manifest tools.tests.test_generation -v

# I1~I4: 새 native tests만 빠르게 반복
ctest --test-dir build/windows-sample-suite -R "hmi_(interfaces|update|sample_suite_interfaces_info)" --output-on-failure

# I4~I5: i18n·font asset 일치
python tools/assets/typography/generate.py --config samples/suite/i18n/catalog.json --check
python tools/assets/typography/generate_user.py --config samples/suite/i18n/catalog.json --check

# I5: profile 전체·격리·firmware compile
python tools/project.py check --host windows --profile sample-suite
python tools/project.py check --scope isolation
python tools/project.py build --host hmi --profile sample-suite
git diff --check
```

`ctest`의 첫 실행 전에는 Windows profile check가 build tree와 test 목록을 생성하게 한다.
HMI 검증은 compile까지만 수행하며 upload, flash, serial deployment 명령은 계획에 포함하지 않는다.

## Automated Verification Checklist

- [x] 이식 표가 public contract, 기존 구현 참조와 제외할 internal/product source를 구분한다.
- [x] RS485/GPIO/PWM/USB command 공개 header가 Board internal과 플랫폼 SDK를 노출하지 않는다.
- [x] unsupported adapter가 결정적인 `not_implemented`를 반환한다.
- [x] update core에 제품 ID, factory/operator policy, endpoint, credential과 실제 flash 호출이 없다.
- [x] update core의 signature/binding/journal/power-cut/boot accept·reject tests가 통과한다.
- [x] 목록 여덟 항목과 상세 여덟 화면의 navigation/re-entry/destruction test가 통과한다.
- [x] page에 실행 가능한 가짜 hardware control이 없다.
- [x] storage/MSC/asset updater 구현이 중복되지 않는다.
- [x] 두 locale의 key와 placeholder 계약이 같다.
- [x] `python tools/project.py check --host windows --profile sample-suite`가 통과한다.
- [x] `python tools/project.py check --scope isolation`이 통과한다.
- [x] 영향받은 HMI profile build가 통과하고 upload/flash는 실행하지 않는다.

## Manual Verification Checklist

- [x] 여덟 목록 제목과 상태가 한눈에 구분된다.
- [x] 상세 화면의 조립 방법만 보고 후속 adapter와 제품 구현 위치를 찾을 수 있다.
- [x] RS485/GPIO/PWM/USB command가 실제 지원된다고 오해할 표현이 없다.
- [x] USB command, MSC와 자산 업데이트의 목적·소유권 차이가 명확하다.
- [x] 펌웨어 업데이트 상세가 이식된 core와 후속 adapter 조립 방법을 정확히 설명한다.
- [x] 펌웨어 update button이나 실제 설치 성공으로 오인할 표현이 없다.
- [x] 실제 LCD/touch에서 목록 스크롤, 상세 진입, 목록 복귀와 HOME이 자연스럽다.
- [x] 사용자가 S09 범위와 RTC 독립 page 분리를 승인한다.
