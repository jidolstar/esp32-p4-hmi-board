# 범용 HMI 시작 프로젝트 2차 계획: 최소 PlatformIO·HMI 부트스트랩

작성: 2026-11-07. 상태: `complete` (H1~H6 자동·수동 게이트 완료).

상위 설계는 `_plan/2026-11/20261106_002_DomainNeutralHmiStarterInventory.md`를, 1차 기반은 `_plan/2026-11/20261107_001_FoundationCodeMigration.md`를 따른다. 사용자가 가장 기본적인 기반부터 순서대로 이관하기를 요청했고, 현재 VS Code에서 PlatformIO가 프로젝트를 인식하지 못하므로 기존에 예정했던 프로필·샘플 계획보다 이 계획을 먼저 실행한다.

## 1. 목적과 결정 요약

저장소 루트에 PlatformIO 프로젝트 진입점을 만들고, ESP32-P4 HMI에서 Windows와 동일한 `apps/main/` 진단 화면을 실행할 수 있는 가장 작은 펌웨어 기반을 선택 이관한다.

이번 계획의 핵심 결정은 다음과 같다.

1. 루트 `platformio.ini`와 기본 환경 `hmi-main`을 먼저 만든다. 이 파일이 VS Code PlatformIO 프로젝트 인식의 기준이다.
2. 원본 `../dev_202609_hmi/platformio.ini`에서 검증된 pioarduino ESP32-P4 플랫폼 URL, `esp32-p4-evboard` 보드 ID, Arduino framework, LVGL 8.4.0과 16MB partition 설정을 기준으로 가져온다.
3. PlatformIO의 `board = esp32-p4-evboard`는 MCU·toolchain·기본 variant 선택이다. 실제 JC1060P470C-I-W-Y의 패널·터치·핀 초기화는 `boards/jc1060p470c/`가 별도로 소유한다. 두 개념을 같은 보드 정의로 혼동하지 않는다.
4. 첫 펌웨어는 `hosts/hmi/main.cpp`만 빌드하는 최소 Arduino `setup()/loop()`부터 시작한다. 이 단계가 통과한 뒤에만 보드 드라이버와 LVGL을 연결한다.
5. 원본 `Board` 전체를 복사하지 않는다. 오디오, SD, RTC, USB MSC, GPIO/PWM/RS485 서비스는 제외하고 LCD, 터치, LVGL runtime에 필요한 파일만 선택 이관한다.
6. `apps/main/`은 Windows와 HMI가 함께 사용하는 공통 앱이다. host 이름과 입력 진단 상태는 host가 값만 전달하고, 앱 안에는 Arduino·SDL·보드별 분기를 넣지 않는다.
7. `Board`는 전원·LCD·터치·LVGL 초기화와 runtime tick만 소유한다. 화면 객체와 문구는 `apps/main/`이 소유한다.
8. Codex는 `pio run -e hmi-main` 빌드까지만 수행한다. 업로드·플래시·장치 변경 명령은 실행하지 않는다.

## 2. 완료 시 얻는 결과

- VS Code에서 현재 폴더가 PlatformIO 프로젝트로 인식되고 `hmi-main` 환경이 표시된다.
- 로컬 PlatformIO 실행 파일로 `hmi-main` 펌웨어를 구성하고 빌드할 수 있다.
- ESP32-P4 toolchain 설정, LVGL 버전, flash/partition 설정이 원본의 검증값에 고정된다.
- JC1060P470C-I-W-Y의 MIPI-DSI LCD, GT911 터치, LVGL display/input port와 PSRAM 우선 allocator가 최소 보드 패키지로 존재한다.
- HMI host가 `Board`와 공통 `MainApp`을 조립하고, `Board::tick()`을 통해 LVGL runtime을 구동한다.
- Windows와 HMI가 같은 `apps/main/app.cpp`를 빌드하며, 플랫폼별 차이는 host와 adapter에만 남는다.
- 실기기에서 `HMI Starter`, `Profile: main`, `Host: hmi`, `Status: ready`, `Input: waiting`을 보고 화면을 누르면 `Input: detected`로 바뀌는지 사용자가 확인할 수 있다.
- 다음 계획은 보드 기반을 다시 손대지 않고 프로필 registry와 `sample-suite` Dashboard/page 격리에 집중할 수 있다.

## 3. 현재 상태와 확인 근거

### 3.1 현재 저장소

- 루트에 `platformio.ini`가 없고 `.pio/`도 아직 없다. 따라서 현재 VS Code에서 PlatformIO 프로젝트로 인식되지 않는 것은 정상이다.
- Windows 기반은 다음 책임으로 이미 분리되어 있다.
  - `modules/text/`: 플랫폼·LVGL 비종속 UTF-8 코어
  - `apps/main/`: 공통 LVGL 시작 화면
  - `platform/windows/`: SDL·Windows allocator·LVGL port
  - `hosts/windows/`: Windows 조립과 event loop
- 현재 `apps/main/app.cpp`는 `Host: windows`를 고정 문자열로 갖고 있다. HMI 공유 전에 host 표시값을 주입받는 좁은 계약으로 바꿔야 한다.
- `.gitignore`에는 CMake `build/`가 있지만 PlatformIO 생성물 `.pio/`는 아직 없다.

### 3.2 원본 저장소

계획 작성 시점의 원본 기준은 branch `dev_202609_hmi`, commit `0161bc8b2dd6dbaeabb42c8086e8a2b07a4708bc`다. 후보 기반 파일에는 미커밋 diff가 없지만, 실행 직전에 다시 확인하고 hash를 기록한다.

원본에서 확인한 기반은 다음과 같다.

- `platformio.ini`: pioarduino ESP32-P4 플랫폼 `55.03.39`, `esp32-p4-evboard`, Arduino, LVGL 8.4.0, 16MB partition
- `src/hardware/internal/mipi_dsi_display.*`: JD9165 MIPI-DSI 1024×600 panel, double framebuffer, backlight
- `src/hardware/internal/touch_controller.*`: GT911 reset, address probe, 좌표 읽기
- `src/hardware/internal/i2c_transaction.*`: 공유 I2C transaction mutex
- `src/hardware/internal/lvgl_port.*`: LVGL display/input 등록과 tick
- `src/ui/lvgl/lvgl_mem.cpp`: PSRAM 우선 LVGL allocator
- `src/hardware/board.*`: 보드 facade와 초기화 순서

원본 `Board`는 현재 오디오, SD, RTC, USB MSC, GPIO, PWM, RS485, 온도 서비스까지 소유한다. 이를 그대로 복사하면 최소 HMI 부트스트랩이 제품 장치 서비스에 결합되므로 이번에는 display/touch/LVGL 책임만 가진 새 최소 facade로 재구성한다.

### 3.3 로컬 도구 확인

- `pio`는 현재 PowerShell `PATH`에서 바로 검색되지 않는다.
- PlatformIO Core 6.2.0은 사용자 PlatformIO 가상환경에 설치되어 있다.
- 계획의 자동 명령은 PATH를 가정하지 않고 `& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" ...` 형식을 사용한다.
- 설치된 pioarduino 플랫폼 안에 `esp32-p4-evboard` 보드 정의가 존재한다. 새 저장소에 그 JSON을 복제하지 않고 고정 platform package가 제공하는 정의를 사용한다.

## 4. 목표 구조와 책임

```text
platformio.ini
.gitignore
START_HERE.md
include/
  lv_conf.h
  hmi_lvgl_mem.h
apps/
  main/
    app.hpp
    app.cpp
boards/
  jc1060p470c/
    board.hpp
    board.cpp
    config.hpp
    touch_point.hpp
    internal/
      i2c_transaction.hpp
      i2c_transaction.cpp
      mipi_dsi_display.hpp
      mipi_dsi_display.cpp
      touch_controller.hpp
      touch_controller.cpp
      lvgl_port.hpp
      lvgl_port.cpp
hosts/
  hmi/
    main.cpp
platform/
  hmi/
    log.hpp
    lvgl_mem.cpp
_doc/
  migration/
    hmi_bootstrap_source_map.md
.pio/                         # Git 제외
```

| 위치                    | 책임                                                                    | 포함하지 않을 책임                                   |
| ----------------------- | ----------------------------------------------------------------------- | ---------------------------------------------------- |
| `apps/main/`          | 공통 시작 화면, 화면 객체 lifetime, 중립 입력 진단 표시                 | Arduino, SDL, 핀, panel handle, LVGL tick            |
| `hosts/hmi/`          | Arduino 진입점, `Board`와 `MainApp`의 생성·시작 순서               | 저수준 display/touch callback, 화면 내부 widget 조작 |
| `boards/jc1060p470c/` | JC1060P470C 전원·LCD·터치 자원과 LVGL port, `Board::begin()/tick()` | 제품 UI, 저장소, 오디오, 통신, 업무 정책             |
| `platform/hmi/`       | Arduino/ESP heap 기반 LVGL allocator와 HMI 진단 로그                    | 특정 화면, 제품 상태, board pin                      |
| `platformio.ini`      | ESP32-P4 toolchain, 명시적 소스 목록, compile flag, dependency pin      | 제품 profile 분기, upload 자동화                     |

PlatformIO는 이 단계에서 registry generator를 도입하지 않는다. 루트 기준 `src_dir = .`와 기본 제외 `-<*>` 뒤의 명시적 `build_src_filter` 허용 목록으로 필요한 `.cpp`만 컴파일한다. 이 선택은 첫 환경 하나를 가장 단순하게 검증하기 위한 임시 조립 방식이다. 다음 프로필 계획에서 registry가 안정되면 생성된 source filter로 교체한다.

## 5. 선택 이관 목록

| 원본                                                           | 목적지                                             | 처리 방식                                                                        |
| -------------------------------------------------------------- | -------------------------------------------------- | -------------------------------------------------------------------------------- |
| `../dev_202609_hmi/platformio.ini`의 `[hmi]` 기반          | `platformio.ini`                                 | `hmi-main` 하나만 남기고 제품·시험 profile과 release script 제거              |
| `../dev_202609_hmi/src/hardware/config.hpp`                  | `boards/jc1060p470c/config.hpp`                  | LCD·touch·serial에 필요한 상수만 유지하고 RS485·SD·audio 상수 제외           |
| `../dev_202609_hmi/src/hardware/touch_point.hpp`             | `boards/jc1060p470c/touch_point.hpp`             | 중립 좌표 DTO 유지, namespace를 새 board package에 맞게 정리                     |
| `../dev_202609_hmi/src/hardware/internal/i2c_transaction.*`  | `boards/jc1060p470c/internal/i2c_transaction.*`  | 정적 mutex와 bounded timeout 계약 유지                                           |
| `../dev_202609_hmi/src/hardware/internal/mipi_dsi_display.*` | `boards/jc1060p470c/internal/mipi_dsi_display.*` | panel timing·초기화 명령·double framebuffer 유지, 제품 기능 제외               |
| `../dev_202609_hmi/src/hardware/internal/touch_controller.*` | `boards/jc1060p470c/internal/touch_controller.*` | 사용하지 않는 `IoExpander` 인자를 제거하고 직접 reset/IRQ 초기화 계약으로 축소 |
| `../dev_202609_hmi/src/hardware/internal/lvgl_port.*`        | `boards/jc1060p470c/internal/lvgl_port.*`        | display/input callback과 runtime tick 유지, UI 생성 금지                         |
| `../dev_202609_hmi/src/ui/lvgl/lvgl_mem.cpp`                 | `platform/hmi/lvgl_mem.cpp`                      | PSRAM 우선, 내부 8-bit heap fallback 계약 유지                                   |
| `../dev_202609_hmi/src/diagnostics/log.hpp`                  | `platform/hmi/log.hpp`                           | HMI ASCII 진단 로그만 유지하고 프로젝트 중립 접두어로 정리                       |
| `../dev_202609_hmi/src/hardware/board.*`                     | `boards/jc1060p470c/board.*`                     | 구조와 초기화 순서만 참고해 display/touch/LVGL 전용 최소 facade로 재작성         |

이번 계획에서 복사하지 않는 항목은 다음과 같다.

- 원본 `src/profile/` 전체와 상품·communication-test·factory·audio-lab profile
- SD, Storage, audio, RTC, USB MSC, Wi-Fi, GPIO/PWM/RS485, motor, cooling
- 제품 UI, navigation, theme, 생성 폰트·이미지, i18n, recipe/config
- release metadata script, server, factory flasher, upload 자동화
- PlatformIO package가 이미 제공하는 `esp32-p4-evboard.json`

## 6. 단계 상태

| 단계 | 상태         | 결과                                                 |
| ---- | ------------ | ---------------------------------------------------- |
| H1   | `complete` | 원본 기준과 HMI 허용 목록 고정                       |
| H2   | `complete` | VS Code/PlatformIO 인식과 최소 Arduino firmware 빌드 |
| H3   | `complete` | JC1060P470C display·touch 보드 기반 이관            |
| H4   | `complete` | LVGL HMI port·allocator와 Board runtime 연결        |
| H5   | `complete` | 공통 `MainApp`을 Windows/HMI 두 host에 연결        |
| H6   | `complete` | 실기기 확인과 내부 코드 재사용 권한 확인 완료        |

실행 시 한 번에 하나만 `in_progress`로 바꾸고, 각 단계의 자동 종료 조건이 모두 통과한 뒤 다음 단계로 이동한다. 2026-11-07 사용자가 H6의 VS Code 인식, 직접 upload, LCD·touch·serial 동작과 내부 코드 재사용 권한을 확인해 전체 계획을 `complete`로 전환했다.

## 7. 단계별 실행 계획

### H1. 원본 기준과 HMI 이관 허용 목록 고정

**목표:** 이번 HMI 기반이 어떤 원본 상태와 파일에서 파생되는지 재현 가능하게 만든다.

**의존성:** 1차 Windows 기반이 존재할 것.

**범위**

- 포함: 원본 branch·commit·후보 파일 diff·SHA-256, 원본→목적지 mapping, 제외 목록
- 제외: production C++ 변경, PlatformIO dependency 다운로드

**산출물**

- `_doc/migration/hmi_bootstrap_source_map.md`
- 이 계획의 H1 완료 근거

**자동 검증**

```powershell
git -C ../dev_202609_hmi branch --show-current
git -C ../dev_202609_hmi rev-parse HEAD
git -C ../dev_202609_hmi diff --name-only -- <HMI-허용-목록>
```

- 모든 허용 목록 원본이 존재한다.
- 각 허용 파일의 SHA-256과 목적지가 source map에 기록된다.
- 후보 파일에 미커밋 diff가 있으면 commit 기준과 작업 트리 기준 중 사용할 입력을 source map에 명시한다.
- 원본 전체 코드 라이선스 미확정 상태를 외부 배포 제한으로 유지한다.

**수동 검증:** 없음. 외부 배포 라이선스 결정과 제3자 고지는 전체 로드맵 G4의 배포 게이트로 유지한다.

**종료 조건**

- [X] 원본 branch·commit·허용 목록 hash가 기록되어 있다.
- [X] 복사 대상과 명시적 제외 대상이 파일 단위로 구분되어 있다.
- [X] 허용 목록의 미커밋 상태가 기록되어 있다.

**완료 근거 (2026-11-07)**

- `_doc/migration/hmi_bootstrap_source_map.md`에 branch `dev_202609_hmi`, commit `0161bc8b2dd6dbaeabb42c8086e8a2b07a4708bc`, 허용 파일 15개의 byte 수와 SHA-256을 기록했다.
- 기록 후 재계산 결과 `allowlist_count=15`, `hashes_match=True`다.
- 허용 목록에 대한 원본 `git diff --name-only` 결과는 `allowlist_diff_count=0`이다.
- source map의 Windows 절대 경로 검색 결과는 `absolute_paths=0`이다.
- 제품 profile, 저장소, audio, RTC, USB, RS485, Wi-Fi, motor, release/upload 도구를 명시적 제외 대상으로 기록했다.

### H2. PlatformIO 인식과 최소 Arduino firmware

**목표:** 보드 드라이버 없이도 VS Code가 프로젝트를 인식하고 ESP32-P4 toolchain으로 최소 firmware를 빌드한다.

**의존성:** H1 `complete`.

**범위**

- 포함: `platformio.ini`, `.pio/` ignore, 최소 `hosts/hmi/main.cpp`, `START_HERE.md`의 현재 지원 상태
- 제외: LVGL 초기화, LCD·touch, 공통 앱 연결, upload

**산출물**

- 기본 환경 `hmi-main`
- Arduino `setup()/loop()`만 가진 최소 host
- 고정된 platform/framework/board/partition/LVGL dependency 선언
- Windows·HMI 소스를 섞지 않는 명시적 source filter

**자동 검증**

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" project config --json-output
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e hmi-main
```

- config 출력에 환경 `hmi-main`, board `esp32-p4-evboard`, framework `arduino`가 하나씩 존재한다.
- 최소 firmware 빌드가 성공하고 `.pio/build/hmi-main/firmware.bin`이 생성된다.
- `platformio.ini`의 HMI source filter에 `platform/windows/`, `hosts/windows/`, `samples/`, 원본 저장소 경로가 없다.
- upload target이나 장치를 자동 선택하는 script가 없다.

**수동 검증**

- VS Code에서 폴더를 다시 열거나 `Developer: Reload Window` 후 PlatformIO가 `hmi-main` 환경을 표시한다.

**종료 조건**

- [X] PlatformIO config parsing이 성공한다.
- [X] 최소 `hmi-main` firmware가 빌드된다.
- [X] 사용자가 VS Code의 `hmi-main` 환경 표시를 확인했거나, 해당 확인을 H6 수동 게이트로 명시적으로 이관했다.

**완료 근거 (2026-11-07)**

- `platformio.ini`에 기본 환경 `hmi-main`, pioarduino ESP32-P4 platform `55.03.39`, board `esp32-p4-evboard`, Arduino framework, LVGL `8.4.0`을 등록했다.
- PlatformIO Core 6.2.0의 `project config --json-output`을 재해석한 결과 `environment_found=True`, `board=esp32-p4-evboard`, `framework=arduino`다.
- `hosts/hmi/main.cpp`만 포함한 `pio run -e hmi-main`이 성공했고 `.pio/build/hmi-main/firmware.bin`과 `firmware.elf`가 생성됐다.
- 최소 firmware 사용량은 RAM 23,720바이트/512,000바이트(4.6%), flash 303,570바이트/6,553,600바이트(4.6%)다.
- HMI source filter의 Windows·sample·원본 저장소 참조는 `forbidden_source_filter_hits=0`, upload 자동화 설정은 `upload_automation_hits=0`이다.
- `.pio/build/hmi-main/firmware.bin`은 `.gitignore`에 의해 제외된다.
- VS Code 프로젝트 창에서 PlatformIO 탭과 PlatformIO Home toolbar가 활성화된 것을 확인했다. 접근성 API가 환경 상세 항목을 노출하지 않아 `hmi-main` 이름의 UI 표시는 H6 사용자 수동 게이트로 이관했다.

### H3. JC1060P470C display·touch 보드 기반 이관

**목표:** 제품 서비스 없이 LCD와 touch를 소유하는 최소 board package가 컴파일된다.

**의존성:** H2 `complete`.

**범위**

- 포함: display/touch config, MIPI-DSI panel, GT911, I2C transaction, `TouchPoint`, 최소 `Board`
- 제외: LVGL port/allocator, 앱 화면, SD·audio·RTC·USB·통신 서비스

**산출물**

- `boards/jc1060p470c/`의 public `Board`와 private `internal/` 구현
- `Board::begin_hardware()` 또는 동등한 최소 초기화 계약
- LCD 1024×600 RGB565 double framebuffer와 touch I2C 초기화 순서
- 앱이 참조하지 않는 board-internal driver API

**메모리 검토**

- 두 개의 1024×600 RGB565 framebuffer는 총 2,457,600바이트이며 ESP LCD driver가 PSRAM 쪽 수명을 소유한다.
- framebuffer를 C++ 자동 지역 배열이나 `Board` 내부 배열로 중복 생성하지 않는다.
- display, touch, `Board` 객체는 정적 또는 host보다 긴 persistent lifetime으로 두고 startup stack에 큰 객체를 만들지 않는다.
- GT911 임시 read buffer는 8바이트로 bounded 상태를 유지한다.

**자동 검증**

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e hmi-main
rg -n "lv_obj_|lv_label_|lv_btn_|screen|widget" boards/jc1060p470c
rg -n "audio|storage|sd_card|usb_msc|rs485|recipe|cooking|wifi" boards/jc1060p470c
rg -n "internal/" apps/main hosts/hmi
```

- HMI firmware가 새 board source를 포함해 빌드된다.
- `Board`와 내부 driver가 LVGL widget이나 제품 화면을 만들지 않는다.
- 제외한 제품·장치 서비스가 board package에 유입되지 않는다.
- `apps/main/`은 board header와 `internal/`을 참조하지 않는다.
- `hosts/hmi/`는 public `Board`만 사용하고 board `internal/`을 include하지 않는다.

**수동 검증:** 실기기 동작은 아직 주장하지 않고 H6에서 통합 확인한다.

**종료 조건**

- [X] LCD·touch 최소 board package가 firmware에 컴파일된다.
- [X] Board facade와 internal 구현 경계가 검색으로 확인된다.
- [X] framebuffer·object lifetime과 크기 근거가 코드 주석 또는 source map에 기록되어 있다.

**완료 근거 (2026-11-07)**

- `boards/jc1060p470c/`에 display/touch config, `TouchPoint`, 정적 I2C mutex, JD9165 MIPI-DSI driver, GT911 driver와 최소 `Board` facade를 선택 이관했다.
- `hosts/hmi/main.cpp`는 persistent lifetime의 public `Board`만 생성하며 board `internal/`을 include하지 않는다.
- `pio run -e hmi-main`이 새 board source 4개를 컴파일해 성공했다. 사용량은 RAM 26,324바이트(5.1%), flash 359,630바이트(5.5%)다.
- board package의 LVGL widget·화면 생성 검색은 `widget_hits=0`, audio·storage·SD·USB·RS485·recipe·cooking·Wi-Fi 검색은 `excluded_service_hits=0`이다.
- `apps/main/`과 `hosts/hmi/`의 board internal include는 `caller_internal_hits=0`, 앱의 board include는 `app_board_hits=0`이다.
- 1024×600 RGB565 double framebuffer의 총 2,457,600바이트를 source map과 class 주석에 기록했고, ESP LCD driver가 PSRAM lifetime을 소유하도록 유지했다. GT911 임시 point buffer는 8바이트다.

### H4. LVGL HMI port·allocator와 Board runtime

**목표:** 보드 display/touch를 LVGL 8.4에 연결하고, host가 저수준 tick을 직접 다루지 않게 한다.

**의존성:** H3 `complete`.

**범위**

- 포함: `platform/hmi/lvgl_mem.cpp`, board 내부 `LvglPort`, `Board::begin()/tick()`, refresh semaphore
- 제외: 앱 화면 생성, 제품 theme/assets, storage/audio worker

**산출물**

- PSRAM 우선, 내부 8-bit heap fallback을 갖는 custom allocator
- MIPI-DSI double framebuffer를 LVGL full-refresh buffer로 등록하는 display port
- GT911을 LVGL pointer input으로 등록하는 input port
- `Board::tick()` 내부의 `lv_tick_inc()`와 `lv_timer_handler()` 호출
- refresh callback용 정적 semaphore와 bounded wait

**자동 검증**

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e hmi-main
rg -n "lv_tick_inc|lv_timer_handler|lv_init" hosts/hmi apps/main
rg -n "lv_obj_|lv_label_|lv_btn_|create_.*screen" boards/jc1060p470c
```

- HMI firmware가 LVGL library와 allocator를 연결해 빌드된다.
- `hosts/hmi/`와 `apps/main/`에는 `lv_tick_inc()`, `lv_timer_handler()`, `lv_init()` 직접 호출이 없다.
- LVGL runtime pump는 `Board::tick()`에서 private `LvglPort`로 위임된다.
- Board와 LvglPort는 화면/widget을 만들지 않는다.
- 빌드 결과의 RAM·flash 사용량을 계획 완료 근거에 기록한다.

**수동 검증:** 실기기 refresh와 touch는 H6에서 공통 앱과 함께 확인한다.

**종료 조건**

- [X] LVGL allocator와 display/input port가 link된다.
- [X] runtime pump 책임이 Board facade 뒤에 있다.
- [X] firmware size 결과와 큰 메모리 lifetime 검토가 기록되어 있다.

**완료 근거 (2026-11-07)**

- `platform/hmi/lvgl_mem.cpp`에 PSRAM 우선, 내부 8-bit heap fallback allocator를 이관하고 `hmi_lvgl_malloc`, `hmi_lvgl_free`, `hmi_lvgl_realloc` 세 심볼이 최종 ELF에 존재함을 확인했다.
- `boards/jc1060p470c/internal/lvgl_port.*`가 ESP LCD double framebuffer와 GT911을 LVGL 8.4 display/input driver로 등록한다.
- refresh semaphore는 internal RAM의 정적 storage를 사용하며 새 frame 제출 전에 이전 신호를 제거하고 100ms bounded wait를 적용한다.
- `hosts/hmi/`와 `apps/main/`의 `lv_init`, `lv_tick_inc`, `lv_timer_handler` 검색은 `low_level_outside_board=0`이다.
- `Board::tick()` 정의와 `lvgl_.tick()` 위임은 각각 1건이고, `LvglPort`의 저수준 tick/handler 호출은 2건이다.
- board package의 LVGL widget 생성 검색은 `board_widget_hits=0`이다.
- `pio run -e hmi-main`이 성공했다. 사용량은 RAM 27,860바이트/512,000바이트(5.4%), flash 568,744바이트/6,553,600바이트(8.7%)다.
- framebuffer는 ESP LCD 소유의 2,457,600바이트 PSRAM 두 장을 그대로 빌려 사용하며 추가 draw buffer를 생성하지 않는다. HMI host의 `Board`는 function-static persistent lifetime이다.

### H5. 공통 `MainApp`의 Windows/HMI 연결

**목표:** 동일한 앱 소스가 Windows와 HMI에서 host 정보와 입력 상태만 다르게 표시된다.

**의존성:** H4 `complete`.

**범위**

- 포함: `apps/main/app.*`의 host 정보 주입 계약, 중립 touch 진단 상태, `hosts/windows/main.cpp`, `hosts/hmi/main.cpp`, 두 빌드 연결
- 제외: profile registry, sample-suite Dashboard/page 전환, 외부 자산·한글 글꼴

**산출물**

- `MainApp::create(...)`에 플랫폼 비종속 host 표시값을 전달하는 좁은 options 계약
- Windows의 `Host: windows`, HMI의 `Host: hmi`
- 최초 `Input: waiting`, pointer press 후 `Input: detected` 상태
- 앱보다 오래 사는 Board와 LVGL runtime, 앱 소유 screen/widget의 명시적 lifetime
- HMI의 정적/persistent Board와 MainApp 조립

진단 화면은 영어 ASCII와 내장 Montserrat만 사용한다. 상품 i18n·SD font·theme에 의존하지 않는다. 이벤트 callback은 App 소유 label만 갱신하고 board API나 host를 호출하지 않는다.

**자동 검증**

```powershell
cmake --build --preset windows-debug --target hmi_main_windows hmi_main_app_input_test
ctest --preset windows-debug -R '^hmi_main_(windows_smoke|app_input)$' --output-on-failure
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e hmi-main
rg -n "Arduino|freertos|esp_|driver/|platform/windows|SDL|boards/" apps/main
rg -n "apps/main|lv_obj_|lv_label_|lv_btn_" boards/jc1060p470c platform/hmi
```

- Windows target과 smoke test가 통과한다.
- HMI firmware가 같은 `apps/main/app.cpp`를 포함해 빌드된다.
- 앱에서 Arduino, ESP-IDF, SDL, board header와 platform-specific `#ifdef`가 0건이다.
- board와 HMI platform은 앱과 LVGL widget을 참조하지 않는다.
- Windows screenshot에서 기존 네 문구와 새 입력 상태가 잘리지 않는다.

**수동 검증:** Windows에서 click 후 `Input: detected`로 바뀌고 창 닫기·Esc로 정상 종료한다.

**종료 조건**

- [X] 같은 앱 구현이 두 host에서 빌드된다.
- [X] 공통 앱과 board/platform 경계 검색이 통과한다.
- [X] Windows 표시와 pointer 입력이 확인된다.

**완료 근거 (2026-11-07)**

- `apps/main/app.cpp` 하나를 Windows의 `hmi_main_app`과 PlatformIO의 `hmi-main`이 각각 object로 생성함을 확인했다.
- `MainAppOptions`로 host 이름만 주입하고, `Input: waiting`에서 pointer press 후 `Input: detected`로 바뀌는 callback은 앱 소유 label만 갱신한다.
- Windows `hmi_main_windows`와 전용 `hmi_main_app_input_test`를 빌드했으며 `hmi_main_windows_smoke`, `hmi_main_app_input` 테스트가 2/2 통과했다.
- `build/windows-debug/hmi-main-h5.bmp`의 1024×600 screenshot에서 다섯 문구가 중앙 panel 안에 잘리지 않고 표시됨을 확인했다.
- HMI firmware 빌드가 성공했다. 공통 앱 포함 사용량은 RAM 27,892바이트/512,000바이트(5.4%), flash 645,586바이트/6,553,600바이트(9.9%)다.
- 생산 `apps/main/app.*`의 Arduino·ESP-IDF·SDL·board·platform 의존 검색은 `app_platform_hits=0`, platform별 조건부 전처리 검색은 0건이다.
- `boards/jc1060p470c/`와 `platform/hmi/`의 앱 참조 및 LVGL widget 생성 검색은 `board_platform_app_widget_hits=0`이다.

### H6. 깨끗한 재빌드, 문서화, 실기기 인계

**목표:** 새 사용자가 PlatformIO를 인식·빌드할 수 있고, 사용자가 실제 보드에서 LCD·touch를 검증할 수 있게 인계한다.

**의존성:** H5 `complete`.

**범위**

- 포함: clean build, `START_HERE.md`, source map 최종화, 사용자 실기기 확인 절차
- 제외: Codex의 upload/flash, SD 자산 배포, 제품 기능 수용, 외부 공개·납품용 배포 라이선스 결정

**산출물**

- Windows와 HMI 준비·빌드 명령
- VS Code PlatformIO 인식 절차
- 구현·미구현 범위와 `hmi-main` 환경 설명
- 사용자 수동 upload 후 LCD·touch·serial 확인 절차
- 다음 계획 입력 요약

**자동 검증**

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e hmi-main -t clean
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e hmi-main
cmake --build --preset windows-debug --target hmi_main_windows
ctest --preset windows-debug -R '^hmi_main_windows_smoke$' --output-on-failure
git check-ignore .pio/build/hmi-main/firmware.bin
```

- clean HMI firmware build와 관련 Windows build/test가 통과한다.
- `.pio/`가 Git에서 제외된다.
- 문서의 환경·명령·파일명이 실제 설정과 일치한다.
- 생산 소스와 문서에 개인 절대 경로가 없다.
- source map의 hash와 실제 이관 기준이 일치한다.
- upload·flash·장치 변경 command를 Codex가 실행하지 않았다.

**수동 검증**

1. 사용자가 VS Code에서 `hmi-main` 환경을 확인한다.
2. 사용자가 직접 firmware를 보드에 업로드한다.
3. 전원 인가 후 1024×600 화면에 `HMI Starter`, `Profile: main`, `Host: hmi`, `Status: ready`, `Input: waiting`이 표시되는지 확인한다.
4. 화면을 눌러 `Input: detected`로 바뀌는지 확인한다.
5. 화면 떨림·반복 reset·백라이트 미점등이 없는지 확인한다.
6. serial log에서 display, touch, LVGL 등록 성공과 치명적 초기화 오류 부재를 확인한다.
7. 사용자가 원본 코드의 내부 재사용 권한을 확인한다. 외부 공개·납품용 배포 라이선스와 제3자 고지는 전체 로드맵 G4의 배포 게이트에서 확정한다.

**종료 조건**

- [X] 모든 자동 검증 근거가 기록되어 있다.
- [X] 사용자가 VS Code 인식, LCD 표시, touch 입력, serial log를 확인했다.
- [X] 사용자가 원본 코드의 내부 재사용 권한을 확인했다.
- [X] 외부 배포 라이선스 결정과 제3자 고지는 G4 배포 게이트로 분리되어 있다.
- [X] H1~H6이 모두 `complete`이며 각 단계의 근거가 기록되어 있다.

**자동 검증 완료 근거 (2026-11-07)**

- PlatformIO `clean` 후 `hmi-main`을 처음부터 다시 빌드했다. 결과는 RAM 27,892바이트/512,000바이트(5.4%), flash 645,586바이트/6,553,600바이트(9.9%)이며 `firmware.bin`과 공통 앱 object가 생성됐다.
- Windows `hmi_main_windows`, `hmi_main_app_input_test`를 재빌드했고 관련 테스트 2/2와 Windows 전체 CTest 6/6이 통과했다.
- `pio project config`에서 기본 환경 `hmi-main`, board `esp32-p4-evboard`, framework `arduino`, 명시적 production source 허용 목록을 확인했다.
- HMI source filter의 Windows·sample·원본 저장소 참조는 `hmi_filter_forbidden_hits=0`이다.
- 생산 앱의 platform/board 의존은 `app_platform_hits=0`, board/HMI platform의 앱·widget 참조는 `board_platform_app_widget_hits=0`이다.
- source map의 허용 목록은 15개이고 원본 SHA-256 재검사는 `hashes_match=True`, 허용 목록의 원본 작업 트리 diff는 `allowlist_diff_count=0`이다.
- 생산 소스·문서의 개인 절대 경로는 `personal_absolute_path_hits=0`이며 `.pio/build/hmi-main/firmware.bin`은 `/.pio/` 규칙으로 Git에서 제외된다.
- firmware upload, flash 또는 기타 장치 변경 명령은 실행하지 않았다.

**사용자 확인 완료 근거 (2026-11-07)**

- 사용자가 VS Code PlatformIO의 `hmi-main` 인식과 직접 firmware upload를 확인했다.
- 실제 1024×600 LCD의 다섯 진단 문구, touch 후 `Input: detected`, 화면 안정성, serial 초기화 log를 확인했다.
- 원본 코드를 이 저장소의 내부 개발에 재사용할 권한을 확인했다.
- 외부 공개·납품은 아직 허용으로 간주하지 않으며, 프로젝트 라이선스와 제3자 고지는 G4 배포 계획의 별도 종료 조건으로 유지한다.

## 8. 변경 격리와 예상 후속 변경

| 후속 변경                       | 바뀌어야 하는 위치                                                | 바뀌지 않아야 하는 위치                               |
| ------------------------------- | ----------------------------------------------------------------- | ----------------------------------------------------- |
| 프로필 registry 추가            | `profiles/`, `tools/project.py`, 생성된 CMake/PlatformIO 입력 | display/touch driver 내부, UTF-8 decoder              |
| `sample-suite`와 첫 page 추가 | `samples/suite/`, registry/manifest                             | `apps/main/`, `Board`, Windows/HMI port 내부      |
| 다른 ESP32-P4 HMI board 추가    | 새 `boards/<id>/`, 새 environment 조립                          | `apps/main/`, `modules/text/`, 기존 board 내부    |
| SD·설정 기능 추가              | `modules/storage/`, `platform/hmi/` adapter, 사용하는 profile | LCD driver, `MainApp` 기본 화면                     |
| Wi-Fi·RS485·audio sample 추가 | 해당 module·platform adapter·sample                             | profile registry schema가 안정적이면 기존 sample 내부 |

새 profile이나 sample 때문에 `mipi_dsi_display.cpp`, `touch_controller.cpp`, `lvgl_port.cpp`가 수정된다면 책임 누수를 먼저 의심한다. 새 board를 추가할 때 기존 board 내부를 조건문으로 확장하지 않고 별도 board package와 host 조립을 우선한다.

## 9. 위험과 대응

| 위험                                                        | 대응                                                                                           |
| ----------------------------------------------------------- | ---------------------------------------------------------------------------------------------- |
| `platformio.ini`만 추가하고 실제 firmware가 빌드되지 않음 | H2에서 driver 없는 최소 `setup()/loop()`를 먼저 빌드한다.                                    |
| 원본 `Board` 전체가 들어와 제품 서비스까지 결합됨         | 파일 허용 목록과 제외 용어 검색으로 display/touch/LVGL만 이관한다.                             |
| PlatformIO board ID를 실제 패널 board package로 오해함      | toolchain board ID와 `boards/jc1060p470c/` 책임을 문서·경로로 분리한다.                     |
| `src_dir = .`가 관계없는 C++를 포함함                     | `-<*>` 후 정확한 허용 경로만 더하고 config 출력과 source 검색으로 검증한다.                  |
| HMI와 Windows가 서로 다른 앱을 복제함                       | 두 host가 같은 `apps/main/app.cpp`를 빌드하고 플랫폼 값만 options로 전달한다.                |
| UI가 board internal에 접근함                                | public `Board` facade만 host에 노출하고 앱의 board/internal include를 0건으로 유지한다.      |
| full framebuffer가 stack이나 heap에 중복됨                  | ESP LCD가 소유하는 PSRAM double framebuffer만 LVGL에 연결하고 2,457,600바이트 근거를 기록한다. |
| refresh 중인 framebuffer를 다시 그림                        | 원본의 refresh callback과 정적 semaphore 동기화를 유지하고 bounded timeout을 둔다.             |
| compile 성공을 실기기 성공으로 오인함                       | H6에서 사용자 업로드, LCD, touch, serial을 별도 수동 게이트로 둔다.                            |
| Codex가 장치를 직접 갱신함                                  | `pio run` compile만 실행하고 upload/flash command는 사용자 절차로만 제공한다.                |
| 라이선스 미확정 코드가 외부 배포됨                          | 내부 이관은 계속하되 G4 외부 배포 게이트가 끝나기 전에는 공개·납품하지 않는다.                  |

## 10. 바로 이어질 다음 계획 요약

> 2026-11-07 사용자 결정: 아래 최초 요약의 `sample-ui` 단독 profile은 3차 계획에서 `sample-suite` Dashboard의 첫 `ui-basics` page로 변경되었다. `main`과 sample 앱의 build 격리는 유지하되, 이후 기능 샘플은 sample 앱 안의 독립 page로 추가한다.

H6의 자동 검증이 끝나고 실기기 상태가 명확해지면 `_plan/2026-11/`에 **프로필 registry·선택 빌드와 `sample-suite` Dashboard/page 격리 계획**을 새 문서로 작성한다.

다음 계획의 범위는 다음과 같다.

1. versioned `profiles/registry.json`과 `profile.json` schema를 정의한다.
2. `apps/main/profile.json`을 등록하고 `main`을 기본 profile로 유지한다.
3. `tools/project.py list`, `doctor`, `check --scope isolation`을 구현한다.
4. registry에서 Windows CMake와 HMI PlatformIO의 명시적 source input을 생성한다.
5. `samples/suite/`를 독립 profile로 추가하고 Dashboard, 첫 `ui-basics` page, button, scroll, toast, confirm dialog를 검증한다.
6. `main` 빌드에 `samples/`가 없고 `sample-suite` 빌드에 `apps/main/app.cpp`가 없는지 두 host에서 확인한다.
7. 새 page 추가가 기존 app, Dashboard, board, platform port 내부 수정 없이 page registry 등록과 새 page 파일만으로 끝나는지 확인한다.

다음 계획에서도 Wi-Fi, RS485, WAV, USB, 저장소, 설정, update/server 기능은 제외한다. 먼저 profile 선택과 source isolation 계약을 안정화한 뒤 기능 sample을 한 개씩 추가한다.

## Automated Verification Checklist

- [X] HMI 이관 원본 branch·commit·허용 목록 hash가 기록되어 있다.
- [X] `platformio.ini` config parsing에서 `hmi-main` 환경이 확인된다.
- [X] driver 없는 최소 `hmi-main` firmware가 먼저 빌드된다.
- [X] JC1060P470C display·touch board package가 제품 서비스 없이 빌드된다.
- [X] LVGL allocator와 display/input port가 link된다.
- [X] `Board::tick()`이 LVGL runtime pump를 소유하고 host가 저수준 tick을 호출하지 않는다.
- [X] HMI와 Windows가 같은 `apps/main/app.cpp`를 빌드한다.
- [X] Windows `hmi_main_windows` build와 smoke test가 통과한다.
- [X] HMI `pio run -e hmi-main` clean build가 통과한다.
- [X] HMI source filter에 Windows·sample·원본 저장소 소스가 없다.
- [X] 앱에 Arduino·ESP-IDF·SDL·board internal include가 없다.
- [X] board/platform에 앱 참조와 LVGL widget 생성이 없다.
- [X] RAM·flash 사용량과 framebuffer/lifetime 검토 결과가 기록되어 있다.
- [X] `.pio/` 생성물이 Git에서 제외된다.
- [X] Codex가 upload·flash·장치 변경 command를 실행하지 않았다.

## Manual Verification Checklist

- [X] VS Code PlatformIO가 현재 폴더와 `hmi-main` 환경을 인식한다.
- [X] 사용자가 직접 firmware를 업로드한다.
- [X] 실제 LCD가 1024×600으로 켜지고 다섯 진단 문구가 잘리지 않는다.
- [X] 실제 touch 입력 후 `Input: waiting`이 `Input: detected`로 바뀐다.
- [X] 화면 떨림·반복 reset·백라이트 오류가 없다.
- [X] serial log에 display·touch·LVGL 등록 성공이 보이고 치명적 초기화 오류가 없다.
- [X] 사용자가 원본 코드의 내부 재사용 권한을 확인했다. 외부 배포 조건은 G4에서 확정한다.
