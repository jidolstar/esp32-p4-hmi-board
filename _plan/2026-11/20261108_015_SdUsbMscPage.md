# S05 계획: SD Storage & PC USB MSC Page

작성: 2026-11-08. 상세화: 2026-11-09. 상태: `in_progress`.
현재 단계: M6 `in_progress`. 선행 조건: M5·M4·M3·M2·M1·M0 `complete`, S04 `wifi` `complete`
(2026-11-09 사용자 완료 처리 승인).

## 1. 목적과 결정 요약

이 단계는 SD 카드의 장착·용량·사용 상태를 보여주고, 사용자가 HMI에서
`PC에 연결`을 선택하면 SD의 단독 소유권을 USB Mass Storage Class(MSC)로 넘겨
PC가 이동식 디스크로 사용하게 한다. PC 사용이 끝나면 SD를 HMI에 다시 넘기고
재마운트하여 앱을 계속 사용할 수 있게 한다.

핵심 결정은 다음과 같다.

1. SD owner는 항상 `HMI` 또는 `USB_MSC` 하나뿐이다. 동시 접근 상태는 만들지 않는다.
2. 전체 화면 잠금은 PC가 첫 파일을 읽은 뒤가 아니라 HMI가 소유권 전환을 수락한 순간부터
   시작한다. raw sector 전환 중에는 로컬 파일 API가 이미 안전하지 않기 때문이다.
3. 잠금 중에는 MSC 안내·연결 상태·복귀·재시도 control만 허용한다. Dashboard 이동,
   다른 page 진입, Back, theme/locale 변경, 키보드 입력은 차단한다.
4. HMI 앱 복귀 경로는 세 가지다.
   - 사용자가 잠금 화면의 `HMI로 돌아가기`를 명시적으로 선택한다.
   - PC가 안전 제거/eject 명령을 보낸다.
   - MSC가 한 번 연결된 뒤 USB 케이블 분리 이벤트가 발생한다.
5. PC의 USB suspend는 분리로 간주하지 않는다. 절전 진입만으로 SD owner를 되찾지 않는다.
6. 복귀는 MSC media 비노출 → 진행 중 callback 종료 대기 → MSC 종료 → raw mode 해제 →
   FAT 재마운트 → generation 증가 → 소비자 재스캔 알림 순서로 수행한다.
7. 복귀 재마운트가 성공하면 전체 화면 잠금을 즉시 해제하고 `sd-storage` page로 돌아온다.
   실패하면 다른 앱 기능을 열지 않고 복구 화면을 유지하며 `다시 시도`만 제공한다.
8. 202609 구현의 저장소 facade, exclusive lease, raw sector driver, 상태 흐름과 native test를
   최대한 이식한다. 제품·audio-lab 결합, page-local 잠금, 불충분한 USB 연결 판정은 가져오지 않는다.
9. 일반 파일 CRUD 예제, SD 포맷, 파티션 변경, background 파일 관리자, firmware upload는 범위 밖이다.

## 2. 현재 저장소 근거

### 2.1 현재 프로젝트에 이미 있는 기반

- `boards/jc1060p470c/internal/sd_storage.*`는 40/20/10/4 MHz 순서의 SDMMC mount,
  파일 읽기·쓰기·rename·삭제와 free byte 조회를 제공한다.
- `boards/jc1060p470c/board.*`는 `begin_storage()`와 `storage_ready()`만 공개한다.
  현재는 owner, generation, exclusive lease, raw mode, MSC 상태를 공개하지 않는다.
- `boards/jc1060p470c/internal/asset_usb_updater.*`는 `SdStorage`를 직접 빌려 USB CDC로
  profile asset을 갱신한다. 현재 전송 전체를 보호하는 저장소 lease가 없다.
- `boards/jc1060p470c/internal/audio_service.*`는 SD의 WAV stream을 연다. MSC 진입 전
  stream 종료와 lease 해제를 보장하는 공통 owner 계약은 아직 없다.
- `hosts/hmi/main.cpp`는 SD mount, CDC asset updater, 앱 조립을 수행한다.
- `samples/suite/app.*`는 Dashboard와 활성 page 하나를 소유하지만, 모든 page 위에서
  지속되는 system lock 상태나 timer는 아직 없다.
- `modules/ui/asset_update_overlay.*`는 LVGL top layer를 덮는 전체 화면 입력 차단의 선례다.
  다만 built-in 영문 자산 갱신 UI이므로 MSC용 한국어/영어 UI를 이 객체에 섞지 않는다.
- `platformio.ini`는 현재 framework와 USB 설정을 고정한다. MSC 추가는 이 설정과 CDC updater의
  endpoint·물리 USB port·descriptor 공존 가능성을 먼저 확인해야 한다.

### 2.2 202609에서 선별 이식할 근거

| 202609 원본 | 가져올 핵심 | 그대로 가져오지 않을 부분 |
| --- | --- | --- |
| `../dev_202609_hmi/src/hardware/storage.*` | 비소유 facade, `ExclusiveLease`, status/generation | 제품 전체 파일 API와 불필요한 동적 문자열 |
| `../dev_202609_hmi/src/hardware/internal/sd_card.*` | raw mode에서 파일 API 차단, lease와 polling 배타 | IO expander·제품 background poll 결합 |
| `../dev_202609_hmi/src/hardware/internal/msc_sector_transfer.hpp` | 임의 offset/길이 요청의 512-byte sector 변환과 경계 검사 | 없음. 현재 보드 namespace에 맞게 이식 |
| `../dev_202609_hmi/src/hardware/internal/usb_msc_driver.*` | USBMSC callback, media present, raw read/write, 재진입 등록 | 고정 제품 ID, 단순 `USB` bool 연결 판정 |
| `../dev_202609_hmi/src/hardware/board.*` | lease 취득 → raw mode → MSC 시작, 종료 → 재마운트 순서 | Board에 UI 정책을 넣는 방식 |
| `../dev_202609_hmi/src/diagnostics/audio_lab_msc_flow.hpp` | 중복 요청 거부, enter/exit/remount 실패 상태 전이 | audio stop과 page-local overlay 결합 |
| `../dev_202609_hmi/src/profile/audio_lab/audio_lab_profile.cpp` | worker에서 blocking I/O, UI loop에서 결과 반영 | 전역 profile state와 영문 진단 UI |
| `../dev_202609_hmi/test/native_hardware/main.cpp` | partial/full sector read/write와 overflow test | 단일 거대 test 실행 파일 |
| `../dev_202609_hmi/test/native_audio/main.cpp` | 중복 enter/exit, 실패·재시도 상태 test | WAV catalog test와의 결합 |

### 2.3 202609 대비 보완할 결함

- 기존 `UsbMscDriver::connected()`는 USB stack bool에 의존해 “PC가 SD를 인식함”과
  “USB 기능이 활성화됨”을 충분히 구분하지 못한다.
- 기존 MSC start/stop callback은 eject 정보를 상태 machine에 전달하지 않는다.
- 기존 흐름은 cable unplug 뒤 자동으로 HMI owner를 복원하지 않는다.
- 기존 lock은 audio-lab page 안에만 있어 profile 전체 navigation을 구조적으로 막지 못한다.
- 기존 Board 진입은 asset updater·audio·향후 WAV/camera가 보유한 file handle과의 충돌을
  하나의 공통 계약으로 검증하지 않는다.
- USB callback에서 mount나 LVGL을 직접 호출할 여지가 없도록 ISR/task 경계를 명시해야 한다.

### 2.4 현재 framework에서 사용할 수 있는 신호

설치된 Arduino-ESP32 framework에는 다음 신호가 있다.

- `ARDUINO_USB_STARTED_EVENT`: USB bus가 시작/열거된 신호
- `ARDUINO_USB_STOPPED_EVENT`: 물리 분리 또는 USB stack stop 신호
- `ARDUINO_USB_SUSPEND_EVENT`, `ARDUINO_USB_RESUME_EVENT`: 절전·복귀 신호
- `USBMSC::onStartStop(...)`: host start/stop 및 load/eject 의도
- `USBMSC::onRead(...)`, `USBMSC::onWrite(...)`: 실제 block I/O와 callback in-flight 근거

M0에서 현재 ESP32-P4와 `ARDUINO_USB_MODE=1`, CDC-on-boot 조합에서 이 신호들이 어느
물리 connector와 device stack에 해당하는지 compile/runtime 근거를 확정한다.

## 3. 범위와 비범위

### 3.1 포함

- SD 장착 여부, filesystem, 전체/사용/여유 용량, 현재 owner와 오류 표시
- HMI↔USB MSC 단일 owner 상태 machine
- HMI 파일 API·asset updater·audio stream과 MSC의 배타성
- Windows용 결정적 가상 storage/MSC adapter
- HMI용 Board adapter와 blocking 전환 worker
- sample-suite 전역 full-screen lock
- 명시적 복귀, PC eject 복귀, cable unplug 자동 복귀
- remount 실패 복구·재시도
- generation/revision 기반 후속 WAV/gallery rescan hook
- 두 locale, theme, 1024×600 layout, touch target 검증
- Windows test, HMI compile-only, RAM/flash/stack/buffer 기록

### 3.2 제외

- SD 자동 포맷 또는 filesystem repair
- partition table 변경
- HMI 안의 일반 파일 탐색기·CRUD page
- PC filesystem 내용을 HMI가 MSC 활성 중 동시에 표시하는 기능
- WAV 목록·camera gallery 자체 구현
- OS별 drive letter 자동 탐색이나 PC 파일 조작 자동화
- firmware upload·flash와 장치 설정 변경

## 4. 용어와 불변 조건

### 4.1 Owner

| Owner | 의미 | 허용 작업 |
| --- | --- | --- |
| `none` | 부팅 전, SD 없음, 전환/복구 실패 | raw/file I/O 모두 금지 |
| `hmi` | FAT가 HMI VFS에 mount됨 | lease를 얻은 HMI file client만 접근 |
| `usb_msc` | PC가 raw sector를 사용할 수 있음 | MSC callback만 raw sector 접근 |

모든 snapshot과 test에서 `hmi_file_access && usb_raw_access`는 항상 false여야 한다.

### 4.2 잠금 불변 조건

- `quiescing`, `exposing`, `waiting_for_host`, `pc_active`, `reclaiming`,
  `remount_failed` 상태에서는 global lock이 반드시 보인다.
- lock이 보이는 동안 `request_page`, `request_dashboard`, `request_theme_toggle`,
  locale change, UTF-8/key 입력은 false를 반환하거나 무시한다.
- 이미 예약된 `lv_async_call` 화면 전환은 lock 시작 시 취소한다.
- 잠금 UI는 SD runtime font나 SD image에 의존하지 않는다. firmware에 포함된 fixed font와
  catalog text만 사용한다.
- `hmi_ready`로 복귀하고 새 generation이 관찰된 뒤에만 잠금을 해제한다.

### 4.3 저장소 client 불변 조건

- CDC asset update transaction, audio stream, 향후 WAV/camera scan/write는 모두 같은
  exclusive lease 규칙을 사용한다.
- MSC 진입은 다른 lease가 하나라도 살아 있으면 `busy`로 거부한다.
- MSC raw mode가 true이면 모든 경로 기반 file API와 LVGL runtime font load는 실패한다.
- page destroy는 timer, callback, stream, worker, lease를 남기지 않는다.

## 5. 목표 구조와 책임

| 계층 | 제안 경로 | 책임 | 금지 |
| --- | --- | --- | --- |
| 공통 상태/정책 | `modules/storage/` | owner·전이·오류·자동 복귀 정책, bounded snapshot | LVGL, Arduino, USBMSC, Board include |
| Windows adapter | `platform/windows/storage/` | 가상 SD/PC connect/eject/unplug/remount 시나리오 | 실제 PC drive·network 변경 |
| HMI adapter | `platform/hmi/storage/` | Board 비동기 요청, worker 결과와 USB event snapshot 전달 | LVGL 호출, page ID 분기 |
| Board facade | `boards/jc1060p470c/storage.*`, `boards/jc1060p470c/board.*` | status, lease, MSC enter/exit, raw/file owner 전환 | sample text, page/widget 정책 |
| Board internal | `boards/jc1060p470c/internal/` | SD mount/raw sector/USBMSC callback 구현 | UI include와 profile 정책 |
| profile composition | `profiles/runtime/app_contract.hpp`, `hosts/*/main.cpp` | adapter 수명 생성·주입·tick | concrete page ID 분기 |
| sample shell | `samples/suite/app.*`, `samples/suite/storage_access_lock.*` | 전역 lock, input/navigation 차단, locale/theme 표현 | USB·SDMMC·FreeRTOS include |
| page | `samples/suite/pages/sd_storage/` | 상태 표시와 connect/reclaim/retry intent | Board internal, raw sector, mount 직접 호출 |
| catalog | `samples/suite/i18n/` | 두 locale 고정 문구와 typography role | 동적 용량·상태 값을 고정 문자열로 복제 |

## 6. 공통 API 초안

이름은 구현 시작 시 기존 namespace와 style에 맞춰 확정하되 의미는 다음을 유지한다.

```cpp
enum class StorageOwner : std::uint8_t {
  none,
  hmi,
  usb_msc,
};

enum class StorageAccessState : std::uint8_t {
  unavailable,
  hmi_ready,
  quiescing,
  exposing,
  waiting_for_host,
  pc_active,
  reclaiming,
  remount_failed,
};

enum class StorageAccessError : std::uint8_t {
  none,
  no_media,
  busy,
  lease_failed,
  msc_start_failed,
  callback_drain_timeout,
  remount_failed,
};

struct StorageAccessSnapshot {
  StorageAccessState state;
  StorageAccessError error;
  StorageOwner owner;
  bool media_present;
  bool filesystem_mounted;
  bool host_seen;
  bool host_active;
  bool host_ejected;
  bool usb_disconnected;
  std::uint64_t total_bytes;
  std::uint64_t free_bytes;
  std::uint32_t generation;
  std::uint32_t rescan_revision;
};
```

`StorageAccessPort`는 요청을 시작하고 작은 snapshot만 반환한다. blocking mount/unmount와
USB driver 호출은 HMI adapter worker에서 수행한다. `StorageAccessController`는 다음 intent를
정규화한다.

- `request_pc_access(now_ms)`
- `request_hmi_access(now_ms, reason)`
- `retry_remount(now_ms)`
- `tick(now_ms)`
- `snapshot()`

controller는 password나 file name, directory 목록, sector buffer를 보관하지 않는다.
snapshot은 64 byte 이하를 목표로 `static_assert`를 둔다.

## 7. 상태 machine

| 현재 상태 | 입력/조건 | 수행 | 다음 상태 | global lock |
| --- | --- | --- | --- | --- |
| `unavailable` | SD mount 성공 | status/generation 반영 | `hmi_ready` | 해제 |
| `unavailable` | 사용자가 재시도 | async mount 요청 | `reclaiming` | 표시 |
| `hmi_ready` | `PC에 연결` | pending navigation 취소, lock 표시, client quiesce 요청 | `quiescing` | 표시 |
| `quiescing` | 모든 client idle/lease 없음 | Board MSC enter 요청 | `exposing` | 표시 |
| `quiescing` | asset update/stream busy | 요청 거부, page에 busy 오류 | `hmi_ready` | 해제 |
| `exposing` | lease+raw mode+MSC start 성공 | host 신호 대기 | `waiting_for_host` | 표시 |
| `exposing` | 시작 실패·rollback 성공 | 오류 표시 | `hmi_ready` | 해제 |
| `exposing` | rollback/remount 실패 | 복구만 허용 | `remount_failed` | 표시 |
| `waiting_for_host` | 첫 MSC start/read/write | host_seen=true | `pc_active` | 표시 |
| `waiting_for_host` | 명시적 취소 | media 비노출 후 reclaim | `reclaiming` | 표시 |
| `pc_active` | HMI 복귀 확인 | device-side detach와 callback drain | `reclaiming` | 표시 |
| `pc_active` | host eject/load_eject | 자동 reclaim | `reclaiming` | 표시 |
| `pc_active` | USB STOPPED, 이전 host_seen=true | debounce 후 자동 reclaim | `reclaiming` | 표시 |
| `pc_active` | USB SUSPEND | 상태만 표시, owner 유지 | `pc_active` | 표시 |
| `reclaiming` | MSC 종료+remount 성공 | generation/rescan revision 증가 | `hmi_ready` | 해제 |
| `reclaiming` | remount 실패 | 오류와 retry 제공 | `remount_failed` | 표시 |
| `remount_failed` | `다시 시도` | mount worker 재실행 | `reclaiming` | 표시 |

중복 enter/exit/retry, out-of-order worker completion, late USB event는 generation/token이
현재 작업과 일치할 때만 반영한다.

## 8. USB 분리와 명시적 복귀 세부 정책

### 8.1 PC 인식 판정

단순 `USB.begin()` 성공은 PC 인식으로 보지 않는다. 다음 중 하나를 관찰하면 `host_seen`을
true로 고정한다.

- MSC start/stop callback의 start 신호
- 첫 유효 read/write callback
- 현재 framework에서 MSC interface 단위 event가 확인되면 그 event

USB bus `STARTED`는 보조 상태로만 사용한다. CDC-on-boot 또는 이미 실행 중인 stack과
혼동할 수 있기 때문이다.

### 8.2 명시적 `HMI로 돌아가기`

- `waiting_for_host`에서는 즉시 취소할 수 있다.
- `pc_active`에서는 “PC에서 안전하게 제거했는지” 확인 dialog를 한 번 더 표시한다.
- 사용자가 계속을 선택하면 `mediaPresent(false)`로 새 I/O를 거부하고 in-flight callback이
  0이 될 때까지 제한 시간 안에서 기다린 뒤 MSC를 종료한다.
- callback drain timeout이면 FAT를 즉시 mount하지 않고 `remount_failed`에 준하는 안전
  오류 상태를 유지한다. 강제로 HMI file API를 열지 않는다.

### 8.3 PC 안전 제거

`onStartStop(..., start=false, load_eject=true)`를 host eject로 기록한다. callback 자체에서는
atomic event만 남기고, worker가 media 비노출과 reclaim을 실행한다. 이 경로는 별도 HMI
button 없이 자동으로 앱을 복구한다.

### 8.4 USB 케이블 분리

- `host_seen=true` 이후의 `ARDUINO_USB_STOPPED_EVENT`만 자동 reclaim 원인으로 인정한다.
- event 직후 raw I/O callback의 in-flight count가 0인지 확인하고 짧은 debounce/grace 구간을 둔다.
- cable pull은 PC write cache가 완전히 flush됐다는 보장이 없으므로 remount 실패나 filesystem
  이상을 성공으로 숨기지 않는다.
- remount 성공 시 앱을 다시 사용하게 하고, 실패 시 lock과 `다시 시도`를 유지한다.
- `SUSPEND`는 분리가 아니므로 자동 reclaim하지 않는다.

## 9. 저장소 arbitration과 기존 기능 공존

### 9.1 Exclusive lease

202609의 이동 전용 `Storage::ExclusiveLease` 의미를 현재 보드에 맞게 이식한다.

- HMI file lease는 bounded counter로 공유할 수 있지만 owner는 항상 `hmi` 하나로 표현한다.
- raw mode 진입은 lease count가 정확히 0일 때만 하나의 atomic CAS로 성공한다.
- raw mode에서는 새 lease와 모든 file API를 거부한다.
- mount polling과 remount도 lease 규칙을 따른다.
- lease move/destruction, double release, raw race를 native test한다.

### 9.2 CDC asset updater

- asset transaction 시작 시 lease를 얻고 complete/fail/cancel에서 해제한다.
- MSC owner 또는 전환 중이면 `ASSET_BUSY`와 안정적인 상태 code를 PC에 반환한다.
- MSC 활성 중 수신 byte를 SD에 쓰지 않는다.
- asset update 진행 중 `PC에 연결` 요청은 page에서 busy로 거부하고 global lock을 남기지 않는다.
- asset updater UI와 storage lock이 동시에 나타나지 않도록 shell 우선순위를 정의한다.

### 9.3 Audio와 향후 WAV/camera

- 현재 preview audio가 재생 중이면 `sd-storage` page 진입 전 기존 page destroy가 stop을 요청한다.
- 실제 stream은 lease를 보유하고 worker가 완전히 닫은 뒤 해제한다.
- MSC enter는 lease가 남아 있으면 성공하지 않는다.
- S06 WAV와 S07 camera는 동일 lease와 `rescan_revision`을 사용하도록 후속 변경 지점을 제한한다.
- MSC 복귀 시 `rescan_revision`은 성공한 remount당 정확히 한 번 증가한다.

## 10. UI/UX 상세

### 10.1 `sd-storage` 기본 page

1024×600에서 다음 정보를 제공한다.

- title/description
- SD 상태: 준비됨, 없음, 오류, PC 사용 중
- filesystem: FAT32/기타/알 수 없음
- 전체 용량, 사용량, 여유 용량과 progress bar
- 현재 owner: HMI 또는 PC
- `PC에 연결` primary action
- SD 없음/오류일 때 `다시 확인` action
- CDC asset updater와 USB MSC가 서로 다른 기능이라는 짧은 안내

용량 값은 bytes를 통째로 catalog에 넣지 않고 locale format argument로 표시한다.

### 10.2 global lock 화면

`samples/suite/storage_access_lock.*`는 `screen_`의 최상위 child로 전체 화면을 덮고
clickable root가 모든 pointer 입력을 흡수한다. lock은 active page보다 오래 살 수 있으나
`SampleSuiteApp`보다 오래 살 수 없다.

상태별 내용:

| 상태 | 제목 | 본문/상태 | 허용 action |
| --- | --- | --- | --- |
| `quiescing` | PC 연결 준비 중 | 파일 작업을 종료하고 있음 | 없음 |
| `exposing` | SD를 PC에 연결 중 | USB 저장 장치를 시작하고 있음 | 없음 |
| `waiting_for_host` | USB 케이블을 연결하세요 | PC 인식 대기 | `HMI로 돌아가기` |
| `pc_active` | PC가 SD 카드를 사용 중 | HMI의 다른 기능은 잠겨 있음 | `HMI로 돌아가기` |
| `reclaiming` | HMI로 돌아오는 중 | USB 종료·SD 재연결 진행 | 없음 |
| `remount_failed` | SD 카드를 다시 열 수 없음 | 카드·케이블 확인 안내 | `다시 시도` |

- 본문은 title과 별도 label로 만들고 한국어/영어 wrapping을 고려한다.
- multiline label에는 최소 5 px line spacing과 명시적 width/height/long mode를 둔다.
- button touch target은 최소 48 px 높이로 둔다.
- lock 중 locale/theme button을 제공하지 않는다. 진입 시 locale/theme를 유지한다.
- fixed UI text는 `samples/suite/i18n/ko.yaml`, `en.yaml`, `typography.yaml`이 소유한다.
- SD runtime font를 사용하지 않고 `samples/suite/generated/fonts/fixed/`만 사용한다.

### 10.3 앱 복귀

- 복귀 성공 후 새 Dashboard를 만들지 않고 기존 `sd-storage` page를 유지한다.
- page는 새 snapshot을 읽어 용량과 owner를 갱신하고 성공 상태를 보여준다.
- lock 시작 전에 pending navigation이 있었다면 폐기한다. 복귀 뒤 자동 실행하지 않는다.
- theme/locale와 page-local scalar state는 유지한다.

## 11. 메모리·동시성 예산

- MSC sector 크기는 512 byte로 고정한다.
- partial sector 변환용 512-byte 정렬 buffer는 callback stack에 하나만 둔다.
- callback마다 heap allocate하지 않는다.
- USB event는 atomic bit/token/in-flight counter만 갱신한다.
- mount/unmount와 `USBMSC::begin/end`는 전용 worker에서 수행한다.
- worker stack은 측정 후 결정하며 시작값을 6~8 KiB 범위에서 검토한다. stack을 늘려
  lifetime bug를 숨기지 않는다.
- controller snapshot은 64 byte 이하, port adapter는 256 byte 이하를 목표로 size guard를 둔다.
- page와 lock 객체는 LVGL handle·scalar만 보관하고 directory 목록이나 sector data를 복사하지 않는다.
- host callback과 UI timer 사이 snapshot은 lock-free scalar 또는 짧은 critical section으로 전달한다.
- callback in-flight counter가 0이 되기 전 driver·SD lifetime을 종료하지 않는다.

## 12. 예상 파일 변경 지도

### 12.1 새 파일 후보

- `modules/storage/CMakeLists.txt`
- `modules/storage/include/hmi/storage/storage_access_types.hpp`
- `modules/storage/include/hmi/storage/storage_access_port.hpp`
- `modules/storage/include/hmi/storage/storage_access_controller.hpp`
- `modules/storage/src/storage_access_controller.cpp`
- `modules/storage/tests/storage_access_controller_test.cpp`
- `platform/windows/storage/virtual_storage_access.*`
- `platform/hmi/storage/board_storage_access.*`
- `boards/jc1060p470c/storage.*`
- `boards/jc1060p470c/internal/msc_sector_transfer.hpp`
- `boards/jc1060p470c/internal/usb_msc_driver.*`
- `samples/suite/storage_access_lock.*`
- `samples/suite/pages/sd_storage/sd_storage_page.*`
- `samples/suite/pages/sd_storage/tests/sd_storage_state_test.cpp`
- `samples/suite/pages/sd_storage/tests/sd_storage_interaction_test.cpp`
- `boards/jc1060p470c/tests/msc_sector_transfer_test.cpp` 또는 동등 native test

### 12.2 수정 파일 후보

- `boards/jc1060p470c/internal/sd_storage.*`
- `boards/jc1060p470c/internal/asset_usb_updater.*`
- `boards/jc1060p470c/internal/audio_service.*`
- `boards/jc1060p470c/board.*`
- `profiles/runtime/app_contract.hpp`
- `samples/suite/page_contract.hpp`
- `samples/suite/app.*`
- `samples/suite/page_registry.json`
- `samples/suite/profile.json`
- `samples/suite/CMakeLists.txt`
- `samples/suite/i18n/ko.yaml`
- `samples/suite/i18n/en.yaml`
- `samples/suite/i18n/typography.yaml`
- `samples/suite/generated/*` 생성 산출물
- `hosts/windows/main.cpp`
- `hosts/hmi/main.cpp`
- `tools/hmi_project/manifest.py`
- `tools/hmi_project/generate.py`
- `tools/tests/test_manifest.py`
- `tools/tests/test_generation.py`
- `README.md`

storage module과 HMI MSC source는 `sample-suite`가 해당 capability를 선언할 때만 선택한다.
`main` profile에 USB MSC code·문구·page가 섞이지 않는 것을 isolation test로 확인한다.

## 13. 단계별 실행 계획

### M0. 이식 근거와 USB 구성 확정 — `complete`

**목표**

현재 framework/board에서 CDC asset updater와 MSC가 충돌하지 않는 조립 방식을 확정하고,
202609에서 이식할 최소 단위를 고정한다.

**범위**

- 현재/202609 source와 framework header 조사
- USB connector, CDC/JTAG와 OTG 역할, descriptor/endpoint, dynamic MSC begin/end 확인
- Board public API와 module/adapter 경계 확정
- 구현 code 변경은 하지 않는다.

**산출물**

- 이 계획의 202609 이식 표
- USB event 의미와 자동 복귀 기준
- 파일 변경 지도와 공개 계약 초안
- hardware spike 절차와 기대 log

**자동 검증**

- `python tools/project.py doctor --host hmi --profile sample-suite`
- `rg -n "ARDUINO_USB_CDC_ON_BOOT|ARDUINO_USB_MODE" platformio.ini`
- current/202609 source에서 `USBMSC`, `onStartStop`, `ARDUINO_USB_STOPPED_EVENT`,
  `ExclusiveLease`, `set_raw_mode` 근거 확인
- MSC source가 아직 current build에 들어가지 않는 baseline source map 확인

**수동 검증**

- 실기기 upload가 필요한 connector 식별과 CDC 유지 확인은 M6 수동 수용으로 이동한다.
- M0는 framework API, build flag, source map과 202609 구현 근거로 조립 방향을 확정한다.

**종료 조건**

- USB 구성과 connector 확인 절차가 문서화돼 있다.
- 전체 USB stack이 아니라 MSC interface만 동적으로 start/stop하는 조립 방식이 결정돼 있다.
- 자동 복귀 event와 suspend 제외 규칙이 확정돼 있다.

**의존**

- S04 `wifi` complete.

**현재 근거 (2026-11-09)**

- `python tools/project.py doctor --host hmi --profile sample-suite`는 `Result: ready`다.
- `python tools/project.py prepare --host hmi --profile sample-suite`가 성공했다.
- 생성된 HMI source map에서 `usb_msc`, `msc_sector`, `storage_access`가 검색되지 않았다.
  따라서 MSC source가 아직 baseline firmware에 섞이지 않은 상태다.
- `platformio.ini`에 `ARDUINO_USB_CDC_ON_BOOT=1`, `ARDUINO_USB_MODE=1`이 설정돼 있다.
- 현재 설치 framework에서 USB started/stopped/suspend/resume event와
  `USBMSC::onStartStop/onRead/onWrite` API를 확인했다.
- 202609의 facade·lease·raw mode·MSC driver·flow·native test를 직접 대조했다.
- MSC는 `USBMSC::begin/end`만 제어하고 전역 `USB.end()`를 호출하지 않아 CDC-on-boot
  stack을 유지하는 방식으로 확정했다.
- 물리 connector와 MSC start/stop 뒤 CDC 유지 여부는 Codex가 upload하지 않는 원칙에 따라
  M6 실기기 수동 수용 항목으로 이동했다. 따라서 M0 자동 근거 gate는 `complete`다.

### M1. 저장소 facade·lease·raw mode — `complete`

**목표**

HMI file access와 USB raw access가 동시에 참이 될 수 없는 Board 저장소 계약을 만든다.

**범위**

- bounded `StorageStatus`, generation, owner
- 이동 전용 exclusive lease
- raw mode file API 차단
- asset updater와 audio stream의 lease 적용
- USBMSC driver 자체는 제외

**산출물**

- Board-owned public storage facade
- lease/raw mode native test
- asset updater busy 응답과 audio stream release test
- size guard

**자동 검증**

- lease acquire/move/release/double request test
- raw mode 중 file read/write/open/asset update 거부 test
- lease 보유 중 MSC reservation 거부 contract test
- `rg -n "internal/" samples/suite modules/storage platform/windows`
  결과에 Board internal include가 없음
- Windows `sample-suite` 관련 direct test 통과

**수동 검증**

- 없음. 이 단계는 pure contract/host test로 종료한다.

**종료 조건**

- 모든 전이에서 owner 수가 1 이하임을 test가 증명한다.
- asset updater와 audio가 lease를 누락하지 않는다.
- raw mode에서 HMI file API가 모두 닫힌다.

**의존**

- M0 complete.

**완료 근거 (2026-11-09)**

- public `Storage` facade와 32-byte 이하 `StorageStatus`, generation, owner를 추가했다.
- 각 file API는 짧은 lease를 자동 취득하고 asset transaction과 WAV stream은 전체 수명 동안
  이동 전용 lease를 유지한다.
- HMI lease count와 USB raw owner는 하나의 atomic word를 사용하므로 동시에 성공할 수 없다.
- `hmi_board_storage_access_guard`가 다중 HMI lease, raw 거부, raw 중 file 거부,
  중복 release/end를 검증했다.
- board storage/generator Python test 15개가 통과했다.
- Windows sample-suite 26개 기존 test와 새 ownership direct test가 통과했다.
- HMI sample-suite build가 RAM 36,220 bytes, flash 1,391,268 bytes로 성공했다.
- 공통 Board 회귀 확인을 위한 HMI main build도 RAM 28,256 bytes, flash 661,610 bytes로
  성공했다. upload/flash는 실행하지 않았다.

### M2. USB MSC driver와 HMI 비동기 adapter — `complete`

**목표**

512-byte raw sector I/O, USB event, eject/unplug 감지와 안전한 enter/exit worker를 구현한다.

**범위**

- sector transfer helper
- USBMSC read/write/start-stop callback
- event atomics와 in-flight counter
- Board MSC enter/exit/rollback
- HMI storage adapter worker
- sample-suite에만 driver/adapter를 넣기 위한 storage module 최소 build wiring
- UI와 page는 제외

**산출물**

- MSC driver와 sector test
- host_seen/eject/stopped/suspend snapshot
- worker token으로 late result 폐기
- callback drain timeout 오류

**자동 검증**

- full/partial/multi-sector read/write, overflow, null, zero length test
- begin 실패 rollback과 remount 실패 test
- USB STOPPED는 host_seen 뒤에만 disconnect로 승격되는 test
- SUSPEND가 reclaim을 발생시키지 않는 test
- eject가 reclaim event를 한 번만 발생시키는 test
- callback in-flight 중 driver end가 실행되지 않는 test
- HMI `sample-suite` compile-only

**수동 검증**

- PC에서 이동식 디스크가 한 번만 나타난다.
- serial/asset updater가 요구된 connector에서 유지된다.

**종료 조건**

- read/write callback과 remount lifetime race가 없다.
- 명시적 exit, eject, unplug event가 서로 구분된다.
- driver 종료 뒤 SD가 HMI owner로 remount된다.

**의존**

- M1 complete.

**완료 근거 (2026-11-10)**

- 512-byte 정렬 buffer 하나를 사용하는 partial/full/multi-sector helper를 구현했고 overflow는
  어떤 sector도 변경하기 전에 거부한다.
- USB start/read/write/start-stop/eject/stopped/suspend/resume와 callback in-flight를 atomic
  event state로 분리했다. callback에서는 mount나 LVGL을 호출하지 않는다.
- MSC 종료는 먼저 media를 숨기고 in-flight가 0이 될 때까지 최대 500 ms 기다리며 timeout이면
  raw owner를 유지한다.
- Board enter/exit helper가 raw 예약, driver start rollback, drain, raw 해제, unmount/remount 순서를
  고정하고 HMI adapter worker가 blocking 작업을 6,144-byte stack task에서 수행한다.
- storage module build flag로 USBMSC/worker source를 sample-suite에만 선택하고 main에는 넣지 않았다.
- sector/event/transition native test와 전체 Windows sample-suite 30개 test가 통과했다.
- HMI sample-suite compile이 RAM 37,100 bytes, flash 1,393,588 bytes로 성공했다.
- isolation check가 windows/main, windows/sample-suite, hmi/main, hmi/sample-suite 모두 통과했다.
  upload/flash와 실제 PC 연결 검증은 수행하지 않았다.

### M3. 공통 controller와 Windows fixture — `complete`

**목표**

플랫폼 독립 상태 machine을 완성하고 실제 PC drive를 건드리지 않는 Windows 시나리오로
모든 정상·실패·late event 흐름을 재현한다.

**범위**

- `modules/storage/` 상태/정책
- Windows virtual adapter
- AppContext 주입과 build generator module 등록
- LVGL UI는 제외

**산출물**

- controller/port/types
- virtual scenario: no media, ready, busy, connect, eject, unplug, suspend,
  start fail, remount fail, late completion
- module/profile generation test

**자동 검증**

- 상태 표의 모든 edge test
- 중복 enter/exit/retry 거부 test
- explicit/eject/unplug의 성공 복귀 test
- remount failure에서 lock-required 상태 유지 test
- generation/rescan revision 정확히 한 번 증가 test
- stale token과 out-of-order completion 폐기 test
- `python -m unittest discover -s tools/tests -v`

**수동 검증**

- 없음. 실제 UI는 M4에서 확인한다.

**종료 조건**

- 상태 machine에 미정의 전이가 없다.
- Windows fixture가 실제 PC storage를 변경하지 않는다.
- `main` profile source에 storage module이 섞이지 않는다.

**의존**

- M2 complete.

**완료 근거 (2026-11-10)**

- `StorageAccessController`가 HMI 준비, quiescing, exposing, host 대기, PC active,
  reclaiming, remount 실패를 하나의 플랫폼 독립 상태 machine으로 관리한다.
- 전환 요청이 수락되는 즉시 `lock_required`가 켜지고, 성공한 remount/retry에서만 잠금이
  해제되며 `rescan_revision`이 정확히 한 번 증가한다.
- eject는 즉시 자동 복귀하고, cable unplug는 실제 host 접근 뒤 `USB_STOPPED`가 200 ms
  유지된 경우에만 복귀한다. USB suspend는 분리로 오인하지 않는다.
- operation/token 불일치와 stale completion을 폐기하고 drain timeout·remount 실패에서는
  USB owner와 잠금 상태를 유지한다.
- Windows virtual adapter는 실제 drive를 열지 않고 no-media, busy, host read, eject, unplug,
  suspend, start/remount 실패, stale completion을 재현한다.
- AppContext와 Windows/HMI host가 같은 controller를 주입하고 storage capability가 없는
  `main` profile에는 관련 adapter source가 포함되지 않는다.
- 생성/manifest/Board contract Python test 30개와 Windows sample-suite 전체 31개 test가
  통과했다.
- HMI sample-suite compile이 RAM 86,036 bytes, flash 1,453,614 bytes로 성공했다. RAM 증가는
  controller를 실제 조립하면서 링크된 TinyUSB MSC/CDC 정적 endpoint buffer가 주원인이며,
  사용자 controller snapshot은 64 bytes 이하의 bounded scalar 구조를 유지한다.
- isolation check가 windows/main, windows/sample-suite, hmi/main, hmi/sample-suite 모두 통과했다.
  upload/flash와 실제 PC storage 접근은 수행하지 않았다.

### M4. SD page와 sample-suite global lock — `complete`

**목표**

SD 상태 page와 profile 전체를 확실히 막는 full-screen lock UI를 구현한다.

**범위**

- page registry와 page UI
- sample shell timer/global overlay/input 차단
- 두 locale catalog/typography/fixed font 생성
- 명시적 복귀 confirm과 remount retry

**산출물**

- `sd-storage` Dashboard card/page
- 상태별 global lock
- page state/interaction/lock navigation test
- 1024×600 한국어/영어 capture

**자동 검증**

- lock 중 page/dashboard/theme/locale/key 입력 거부 test
- pending async transition 취소 test
- explicit/eject/unplug 후 lock 해제와 sd page 유지 test
- remount fail에서 다른 page 접근 불가와 retry test
- destroy 중 timer/callback/overlay가 남지 않는 test
- catalog locale/placeholder/typography parity와 generator check
- `python tools/project.py check --host windows --profile sample-suite`

**수동 검증**

- 1024×600에서 용량·안내·button이 잘리지 않는다.
- multiline 본문 line spacing과 중앙/좌측 정렬이 의도대로 보인다.
- lock root가 화면 가장자리와 기존 control touch까지 모두 가로챈다.

**종료 조건**

- MSC 전환 순간부터 다른 앱 기능을 사용할 수 없다.
- 세 복귀 경로가 성공 시 앱을 다시 활성화한다.
- 재마운트 실패만 잠금을 유지한다.

**의존**

- M3 complete.

**완료 근거 (2026-11-10)**

- Dashboard에 `sd-storage` card/page를 등록하고 SD 상태, owner, FAT mount, 전체/사용/여유
  용량과 사용률을 1024×600 한 화면에 표시했다.
- PC 연결 요청이 수락되는 즉시 clickable full-screen system overlay를 최상위로 올리고,
  예약된 async navigation을 취소한 뒤 page/dashboard/theme/locale/keyboard intent를 거부한다.
- 잠금 화면은 SD runtime asset을 참조하지 않고 catalog와 firmware 고정 font만 사용한다.
  한국어·영어 multiline 본문에는 6 px line spacing과 고정 wrap 영역을 적용했다.
- 명시적 복귀는 한 번 더 확인한 뒤 reclaim하고, PC eject와 host 접근 후 cable unplug는
  자동 reclaim한다. 성공 시 기존 `sd-storage` page를 유지하며 잠금을 해제한다.
- callback drain/remount 실패는 잠금을 유지하고 `다시 시도`만 허용하며, retry 성공에서만
  앱 기능을 다시 연다.
- SD page state/interaction test가 pending navigation 폐기, shell input 차단, explicit/eject/
  unplug 복귀, remount 실패·retry, one-shot rescan revision을 검증한다.
- 전체 Windows sample-suite 33개 test와 repository Python test 49개가 통과했다.
- typography generator가 두 locale 184개 문구의 key/placeholder/role parity를 통과했고 생성
  산출물 check도 통과했다.
- 한국어/영어 page·lock 및 Dashboard 1024×600 capture를 확인했고 잘림이나 stale redraw가 없다.
- HMI sample-suite compile이 RAM 86,100 bytes, flash 1,480,052 bytes로 성공했으며 M3 대비
  RAM +64 bytes, flash +26,438 bytes다.
- isolation check가 네 generated output 모두 통과했다. upload/flash는 수행하지 않았다.

### M5. HMI 조립·기존 기능 회귀·자원 검증 — `complete`

**목표**

실제 Board adapter를 sample-suite에 조립하고 CDC asset update, audio preview, font asset,
Wi-Fi, navigation 회귀가 없음을 compile/test로 확인한다.

**범위**

- HMI host 조립
- generated profile source/build flag
- asset updater/audio lease integration
- memory/stack/firmware size 기록
- 실제 upload는 제외

**산출물**

- HMI/Windows profile build evidence
- isolation evidence
- RAM/flash, worker stack, sector buffer, controller/page size 기록
- `README.md` 사용법과 제한

**자동 검증**

- `python tools/project.py check --host windows --profile sample-suite`
- `python tools/project.py check --scope isolation`
- `python tools/project.py build --host hmi --profile sample-suite`
- Board 공통 source를 수정했다면 `python tools/project.py build --host hmi --profile main`
- `python -m unittest discover -s tools/tests -v`
- `rg -n "hardware/internal|USBMSC|SD_MMC|freertos/" samples/suite modules/storage`
  결과가 없음
- firmware RAM/flash와 worker stack을 이전 sample-suite 기준과 비교

**수동 검증**

- 없음. 실기기 동작은 M6에서 사용자가 확인한다.

**종료 조건**

- 모든 자동 검사와 compile-only build가 통과한다.
- main/sample-suite source isolation이 유지된다.
- memory 증가 근거와 상한이 기록돼 있다.

**의존**

- M4 complete.

**완료 근거 (2026-11-10)**

- HMI host가 BoardStorageAccess worker와 공통 controller를 장기 수명으로 조립하고
  AppContext에는 public controller만 주입한다. sample/page/module에는 Board internal,
  USBMSC, SD_MMC, FreeRTOS include가 없다.
- asset updater transaction과 audio stream은 같은 Storage facade lease를 사용하므로 MSC
  raw owner와 동시에 파일 handle을 열 수 없다.
- blocking expose/reclaim/remount는 6,144-byte worker stack에서 수행하고 sector 변환 buffer는
  512 bytes 하나다. port/controller snapshot은 각각 64 bytes 이하, adapter/controller는
  각각 128 bytes 이하 size guard를 유지한다.
- Windows sample-suite 33개 test, repository Python 49개 test, catalog generator check와
  네 profile isolation check가 모두 통과했다.
- HMI sample-suite compile은 RAM 86,100 / flash 1,480,454 bytes, storage capability가 없는
  HMI main compile은 RAM 28,256 / flash 661,610 bytes로 성공했다.
- `README.md`에 SD page, 전체 화면 잠금, explicit/eject/unplug 복귀, suspend 제외,
  remount retry, CDC asset updater busy 조건과 실기기 checklist를 추가했다.
- `git diff --check`가 whitespace 오류 없이 통과했다. upload/flash는 수행하지 않았다.

### M6. 실제 PC·SD 수용 — `in_progress`

**목표**

실제 HMI와 PC에서 연결·파일 변경·세 복귀 경로·실패 복구를 확인한다.

**범위**

- 사용자가 firmware를 직접 upload한 장치
- Windows 이동식 디스크
- 정상/오류/반복 시나리오

**산출물**

- 장치 log와 수동 확인 기록
- connector/OS/SD 카드 정보
- 발견된 결함의 재현 절차와 수정 후 재검증

**자동 검증**

- M5의 build/test를 최종 변경 후 다시 실행한다.

**수동 검증**

- 아래 Manual Verification Checklist 전체를 수행한다.
- asset updater CDC connector와 MSC OTG connector를 실제 보드에서 식별한다.
- MSC interface의 추가/제거 뒤 CDC updater 또는 serial log가 유지되는지 확인한다.

**종료 조건**

- 모든 manual checklist에 사용자 확인 근거가 있다.
- 반복 전환 후 filesystem 손상, reset, UI unlock 누락이 없다.
- S05 상태를 `complete`로 갱신할 수 있다.

**의존**

- M5 complete, 사용자 직접 upload.

## 14. 단계 게이트 요약

| 단계 | 상태 | 핵심 종료 조건 | 다음 단계 허용 |
| --- | --- | --- | --- |
| M0 | `complete` | USB/CDC/MSC 구성과 이식 경계 확정 | M1 |
| M1 | `complete` | 단일 owner lease/raw contract 통과 | M2 |
| M2 | `complete` | MSC driver/event/worker lifetime 통과 | M3 |
| M3 | `complete` | 공통 상태 machine과 Windows fixture 통과 | M4 |
| M4 | `complete` | global lock과 세 복귀 경로 UI 통과 | M5 |
| M5 | `complete` | Windows/HMI/isolation/memory gate 통과 | M6 |
| M6 | `in_progress` | 실제 PC/SD/USB 수용과 사용자 승인 | S05 complete |

동시에 하나의 단계만 `in_progress`로 둔다. 현재는 M6만 진행하며 사용자가 직접 upload한
장치의 실제 PC/SD/USB 확인 근거 없이는 M6와 S05를 완료 처리하지 않는다.

## 15. 위험과 완화

| 위험 | 영향 | 완화 |
| --- | --- | --- |
| CDC-on-boot와 MSC descriptor/endpoint 충돌 | PC 인식 실패, asset updater 중단 | M0에서 connector와 stack을 확정하고 MSC source 선택을 profile capability로 제한 |
| PC와 HMI 동시 FAT 접근 | filesystem 손상 | lease+raw mode 불변 조건, lock을 전환 시작 시점부터 적용 |
| cable pull 중 write cache 미반영 | 다음 mount 실패 또는 데이터 손상 | callback drain, debounce, 실패 시 잠금 유지, 성공으로 숨기지 않음 |
| suspend를 unplug로 오인 | PC 사용 중 HMI remount | SUSPEND 제외 test와 STOPPED+host_seen 조건 |
| asset updater가 MSC 중 SD 기록 | sector/file API 충돌 | updater transaction lease, MSC owner에서 stable busy 응답 |
| audio/file worker handle 잔존 | enter 실패·use-after-close | page destroy stop, stream lease, worker idle 확인 |
| lock overlay 아래 control 작동 | 설정 변경·page 이동 | clickable full-screen root, shell-level API guard, interaction test |
| SD runtime font가 unmount 후 참조됨 | glyph 오류·crash | lock은 fixed firmware font만 사용, sd page 진입 전 page widget/font 정리 |
| late USB/worker event | 잘못된 unlock·중복 remount | generation/token과 one-shot revision |
| blocking mount로 UI 정지 | spinner 멈춤, watchdog | HMI worker, UI timer는 snapshot만 poll |
| module 추상화가 firmware를 과도하게 키움 | RAM/flash 증가 | 작은 port/snapshot, no virtual ownership graph, size guard와 footprint 비교 |

## 16. 후속 기능 변경 격리 확인

이 설계 이후 예상되는 변경은 다음 경계에 한정한다.

- S06 `wav-sounds`: audio page와 WAV catalog가 storage lease와 `rescan_revision`을 소비한다.
  MSC controller나 USB driver 내부는 수정하지 않는다.
- S07 `camera-gallery`: camera file writer/gallery scanner가 같은 lease와 generation을 소비한다.
  SD owner 상태 machine은 수정하지 않는다.
- 다른 board 추가: 새 board/platform adapter가 `StorageAccessPort`를 구현한다.
  sample page와 controller는 수정하지 않는다.
- locale 추가: sample catalog와 generated font만 변경한다. Board/MSC code는 수정하지 않는다.

후속 기능마다 현재 controller 내부를 다시 고쳐야 한다면 storage client/generation 경계가
잘못 설계된 것으로 보고 다음 단계로 진행하기 전에 경계를 조정한다.

## 17. 완료 정의

S05는 다음 조건을 모두 만족해야 `complete`다.

- SD owner가 동시에 둘이 되는 경로가 자동 test로 존재하지 않는다.
- PC 인식 전환 순간부터 profile 전체가 잠긴다.
- 명시적 HMI 복귀, PC eject, cable unplug 세 경로가 remount 성공 후 앱을 다시 활성화한다.
- remount 실패 시 다른 기능을 열지 않고 retry 가능한 recovery 화면을 유지한다.
- asset updater/audio/향후 storage client가 lease 계약으로 MSC와 충돌하지 않는다.
- Windows fixture와 HMI adapter가 같은 공통 상태 machine을 사용한다.
- 두 locale UI와 1024×600 touch/layout이 수용된다.
- Windows/HMI build, isolation, generator, direct test가 통과한다.
- RAM/flash/stack/buffer 변화가 기록된다.
- 실제 PC 파일 생성·복사·삭제·안전 제거와 반복 전환을 사용자가 확인한다.
- Codex는 firmware upload나 flash를 수행하지 않는다.

## Automated Verification Checklist

- [ ] `python tools/project.py doctor --host windows --profile sample-suite`가 ready다.
- [ ] `python tools/project.py doctor --host hmi --profile sample-suite`가 ready다.
- [ ] lease acquire/move/release, raw mode, owner≤1 test가 통과한다.
- [ ] full/partial/multi-sector MSC transfer와 overflow test가 통과한다.
- [ ] enter 실패 rollback, remount 실패, duplicate/late event test가 통과한다.
- [ ] USB suspend는 자동 reclaim하지 않고 eject/unplug는 한 번만 reclaim한다.
- [ ] callback in-flight가 0이 되기 전에 driver/storage lifetime을 종료하지 않는다.
- [ ] CDC asset update와 audio stream이 lease를 보유하고 MSC 중 접근이 거부된다.
- [ ] remount 성공당 generation/rescan revision이 정확히 한 번 증가한다.
- [ ] lock 중 navigation/theme/locale/key 입력이 모두 차단된다.
- [ ] explicit/eject/unplug 복귀 성공 시 lock이 해제되고 `sd-storage` page가 유지된다.
- [ ] remount 실패 중 lock이 유지되고 retry 외 app 기능이 차단된다.
- [ ] page/app destroy 뒤 timer, callback, worker, stream, lease, overlay가 남지 않는다.
- [ ] catalog locale·placeholder·typography parity와 fixed font generator check가 통과한다.
- [ ] `python tools/project.py check --host windows --profile sample-suite`가 통과한다.
- [ ] `python -m unittest discover -s tools/tests -v`가 통과한다.
- [ ] `python tools/project.py check --scope isolation`이 통과한다.
- [ ] `python tools/project.py build --host hmi --profile sample-suite`가 통과한다.
- [ ] Board 공통 source 변경 시 `python tools/project.py build --host hmi --profile main`이 통과한다.
- [ ] `rg` 검사에서 sample/module UI에 Board internal, SD_MMC, USBMSC, FreeRTOS include가 없다.
- [ ] controller snapshot, adapter/page/lock 객체, worker stack, sector buffer, RAM/flash 크기가 기록된다.
- [ ] `README.md`에 PC 연결·안전 제거·자동 복귀·실패 복구·USB connector 제한이 기록된다.

## Manual Verification Checklist

- [ ] 사용자가 firmware를 직접 upload하고 boot log에서 SD mount 성공을 확인한다.
- [ ] SD 미장착 상태에서 `PC에 연결`이 거부되고 앱은 잠기지 않는다.
- [ ] SD 장착 상태에서 전체/사용/여유 용량이 실제 카드와 대략 일치한다.
- [ ] `PC에 연결` 직후 전체 화면 lock이 나타나고 Dashboard·Back·theme·locale·다른 page를 누를 수 없다.
- [ ] PC에서 장치가 이동식 디스크로 한 번만 나타난다.
- [ ] PC에서 파일 생성·복사·이름 변경·삭제 후 안전 제거가 된다.
- [ ] PC 안전 제거만으로 HMI가 자동 remount하고 앱 잠금이 해제된다.
- [ ] 다시 연결한 뒤 HMI의 `HMI로 돌아가기`를 명시적으로 선택하면 remount 후 앱이 열린다.
- [ ] 다시 연결한 뒤 USB 케이블을 뽑으면 HMI가 자동 remount하고 앱이 열린다.
- [ ] USB suspend/PC 절전만으로는 HMI가 SD owner를 되찾지 않는다.
- [ ] PC 연결 중 asset updater·audio/WAV·camera/file 접근이 실행되지 않는다.
- [ ] 복귀 후 PC에서 만든 파일과 삭제 결과가 후속 rescan에 반영된다.
- [ ] remount 실패를 유도하면 전체 화면 lock과 `다시 시도`가 유지되고 다른 기능은 열리지 않는다.
- [ ] 카드 복구 후 `다시 시도`가 성공하면 같은 `sd-storage` page에서 앱 사용이 재개된다.
- [ ] 연결→파일 변경→eject/명시적 복귀/unplug를 각 10회 반복해 reset·멈춤·중복 callback이 없다.
- [ ] 강제 cable pull 뒤 filesystem 손상 경고나 mount 실패가 성공으로 표시되지 않는다.
- [ ] 한국어/영어 lock 문구가 잘리지 않고 multiline line spacing과 button touch가 안정적이다.
- [ ] serial log에 simultaneous owner, raw/file 동시 접근, watchdog, stack overflow, allocation 실패가 없다.
- [ ] 사용자가 S05 완료를 명시적으로 승인한다.
