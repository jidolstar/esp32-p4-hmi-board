# 최소 HMI 부트스트랩 이관 원본 기록

기록일: 2026-11-07.

이 문서는 `_plan/2026-11/20261107_002_MinimalPlatformioHmiBootstrap.md`의 H1 근거다. 원본 `Board` 전체를 복사하지 않고 LCD, touch, LVGL runtime에 필요한 허용 목록만 선택 이관한다.

## 원본 기준

- 원본 루트: `../dev_202609_hmi/`
- branch: `dev_202609_hmi`
- commit: `0161bc8b2dd6dbaeabb42c8086e8a2b07a4708bc`
- commit 요약: `docs: record firmware migration, factory workflow and handoff plans`
- commit 시각: `2026-11-06T23:19:50+09:00`

원본 작업 트리에는 `README.md`, `_plan/`, `server/firmware/`, `src/app/update/`, `src/profile/factory/`, `tools/factory_flasher/`, `tools/releases/`의 별도 미커밋 작업이 있다. 아래 허용 목록 파일에 대한 `git diff --name-only` 결과는 기록 시점에 0건이다. 따라서 HMI 기반은 기록한 commit과 동일한 허용 목록 파일을 입력으로 사용한다.

## 허용 목록과 SHA-256

| 원본 파일 | 바이트 | SHA-256 | 이관 목적지 |
| --- | ---: | --- | --- |
| `platformio.ini` | 5,223 | `61684c8f7d6b1a8bb92545e6dba63053b734d20b4f2b13b4fe1874cd1d4794e5` | `platformio.ini`의 `hmi-main` 환경 |
| `src/hardware/config.hpp` | 1,610 | `d547dbc77c86dd066caec1c3bb77aeb552bcb099479b49e4c582c32d6db6d6ca` | `boards/jc1060p470c/config.hpp` |
| `src/hardware/touch_point.hpp` | 454 | `7753b0fe1a8b10fa02354a2255ee31be4e7eba1ad1d3502e97f0177f7a48982b` | `boards/jc1060p470c/touch_point.hpp` |
| `src/hardware/internal/i2c_transaction.hpp` | 540 | `75b57ca0a3fdf4f1a0b9c373e6ed19c4c1af384735c2adddf6ac348ba66e2e5e` | `boards/jc1060p470c/internal/i2c_transaction.hpp` |
| `src/hardware/internal/i2c_transaction.cpp` | 919 | `ac191813caa913bbd1028838f12a24ae10961873976a5e22ae75e565161ec72d` | `boards/jc1060p470c/internal/i2c_transaction.cpp` |
| `src/hardware/internal/mipi_dsi_display.hpp` | 1,131 | `02d4c9415a86b0e0257cd2c5ffed3f4000e4603bdfb3de731783894bcddb64e4` | `boards/jc1060p470c/internal/mipi_dsi_display.hpp` |
| `src/hardware/internal/mipi_dsi_display.cpp` | 9,032 | `ecd1d38fe9e03f59ba0e91e4e1a91b4e7fc1e48294d0bbcf8c350d695ef7f56c` | `boards/jc1060p470c/internal/mipi_dsi_display.cpp` |
| `src/hardware/internal/touch_controller.hpp` | 981 | `3810e11761fb45fee0f1acc01caebf3cbc835a80d747a066612c10f6c6f12aee` | `boards/jc1060p470c/internal/touch_controller.hpp` |
| `src/hardware/internal/touch_controller.cpp` | 3,334 | `29c34ab0e5c27c6dd4dbf657a4e6d0c21a27bdb76e92ad7ceda93bd7ef6972cf` | `boards/jc1060p470c/internal/touch_controller.cpp` |
| `src/hardware/internal/lvgl_port.hpp` | 1,253 | `cfde82dc18e6a83eaaeb49b27b745dc76d5166e6ec8aaaaa71b7aa848f6e5477` | `boards/jc1060p470c/internal/lvgl_port.hpp` |
| `src/hardware/internal/lvgl_port.cpp` | 5,274 | `3afd9ae686b87866e1aab85f2fd4915c46faf037221a7557f8f5dbb85b5c8f5b` | `boards/jc1060p470c/internal/lvgl_port.cpp` |
| `src/ui/lvgl/lvgl_mem.cpp` | 983 | `60103554a250d13475b21a179d510c0fac5b2501ccf39a35ee635ac55b142a19` | `platform/hmi/lvgl_mem.cpp` |
| `src/diagnostics/log.hpp` | 3,141 | `8e100125c33896c799ee72e800192fe36e62d3ebd8cfc5e60608dc905c1bbfee` | `platform/hmi/log.hpp` |
| `src/hardware/board.hpp` | 7,870 | `ff5ac5cd581dd7324a985dc6fbc1f6159fa706c6de651eeb93973337fccf1aac` | `boards/jc1060p470c/board.hpp` 설계 참고 |
| `src/hardware/board.cpp` | 9,154 | `5d9958899005ab5434fdfac15e917d7bb0b51b45119fd30bceee27484ab7f563` | `boards/jc1060p470c/board.cpp` 설계 참고 |

## 파일별 변경 원칙

- `platformio.ini`: 검증된 ESP32-P4 platform, board, framework, flash, partition, LVGL 설정만 가져온다. 제품·factory·진단 profile과 release script는 제거한다.
- `config.hpp`: LCD, touch, serial에 필요한 상수만 유지한다. RS485, SD, audio 설정은 이관하지 않는다.
- `touch_point.hpp`: board port와 LVGL input 사이의 작은 좌표 DTO만 유지한다.
- `i2c_transaction.*`: 정적 mutex와 bounded timeout을 유지한다. 동적 semaphore 할당이나 전역 service locator를 추가하지 않는다.
- `mipi_dsi_display.*`: JD9165 초기화 table, panel timing, double framebuffer, backlight 제어를 유지한다. 화면/widget은 만들지 않는다.
- `touch_controller.*`: GT911 reset, address probe, bounded 8-byte point buffer를 유지한다. 현재 구현에서 실제로 사용하지 않는 `IoExpander` 인자는 제거한다.
- `lvgl_port.*`: display/input callback, refresh semaphore, LVGL tick만 소유한다. 앱 화면이나 제품 상태를 참조하지 않는다.
- `lvgl_mem.cpp`: PSRAM 우선, 내부 8-bit heap fallback allocator 계약을 유지한다.
- `log.hpp`: HMI 진단용 ASCII serial log만 유지하고 중립 접두어를 사용한다.
- `board.*`: 초기화 순서와 facade 책임만 참고해 display, touch, LVGL 전용 최소 객체로 다시 작성한다.

## 명시적 제외

- 원본 `src/profile/`의 product, communication-test, factory, audio-lab
- 오디오, SD, RTC, USB MSC, Wi-Fi, GPIO/PWM/RS485, motor, cooling, temperature
- 제품 UI, navigation, theme, 생성 폰트·이미지, i18n, recipe/config
- release metadata, server, factory flasher, upload 자동화
- PlatformIO package가 제공하는 `esp32-p4-evboard.json`

## 메모리와 수명 기준

- 1024×600 RGB565 framebuffer 한 장은 1,228,800바이트이며 두 장은 총 2,457,600바이트다.
- 두 framebuffer는 ESP LCD driver가 PSRAM에 소유하며 C++ stack이나 별도 배열에 중복하지 않는다.
- `Board`, display, touch, LVGL port는 HMI host의 persistent lifetime으로 유지한다.
- GT911 point read의 임시 buffer는 8바이트로 제한한다.
- LVGL allocator는 PSRAM을 우선하고 실패 시 내부 8-bit heap으로 후퇴한다.

## 라이선스 상태

원본 루트에는 프로젝트 전체를 대상으로 하는 `LICENSE`, `COPYING`, `NOTICE`가 확인되지 않았다. 2026-11-07 사용자가 현재 저장소의 내부 개발을 위한 원본 코드 재사용 권한을 확인했다. 외부 공개·납품은 허용으로 간주하지 않으며, 프로젝트 라이선스와 제3자 고지는 전체 로드맵 G4의 배포 게이트에서 확정한다.

## 실제 이관 결과

- `platformio.ini`에는 `hmi-main` 환경 하나와 production source의 명시적 허용 목록만 등록했다. 원본 profile, release script와 upload 자동화는 이관하지 않았다.
- `boards/jc1060p470c/`는 display, touch, LVGL runtime만 소유하는 최소 `Board` facade로 재구성했다. 원본의 제품 서비스는 포함하지 않았다.
- MIPI-DSI double framebuffer는 ESP LCD driver가 PSRAM에 소유하고 LVGL이 그대로 사용한다. 별도 full-frame draw buffer는 추가하지 않았다.
- `platform/hmi/lvgl_mem.cpp`는 PSRAM 우선 allocator를 제공하고, `Board::tick()`은 private `LvglPort`에 runtime pump를 위임한다.
- Windows와 HMI는 같은 `apps/main/app.cpp`를 컴파일한다. host 이름은 `MainAppOptions`로 주입하며 앱은 board/platform API를 참조하지 않는다.
- 공통 앱 포함 HMI 빌드 사용량은 RAM 27,892바이트/512,000바이트(5.4%), flash 645,586바이트/6,553,600바이트(9.9%)였다. clean build 결과는 계획의 H6 완료 근거에 별도로 기록한다.
- Codex는 compile과 host test만 수행하며 firmware upload나 flash는 수행하지 않았다.
