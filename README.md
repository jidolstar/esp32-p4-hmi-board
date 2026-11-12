# HMI Starter 시작하기

이 저장소는 Windows 시뮬레이터와 JC1060P470C ESP32-P4 HMI에서 같은 앱 코드를 실행하는 범용 HMI 시작 프로젝트다. 현재 두 실행 profile을 제공한다.

| Profile | 목적 | 현재 화면 |
| --- | --- | --- |
| `main` | 새 응용 기능을 시작하는 최소 앱 | 외부 자산 없는 실행·입력 진단 화면 |
| `sample-suite` | 공통 기능을 page 단위로 탐색하는 진단 앱 | Dashboard, UI·입력·화면/음량·Wi-Fi·SD Storage·등록 WAV·Wi-Fi AP/Web·날짜/시간·인터페이스/업데이트 안내 page |

`sample-suite`의 Dashboard는 생성된 page descriptor만 사용한다. 현재 활성 page 하나만 만들며 page 구현끼리 참조하지 않는다. 한·영 i18n과 키보드 입력의 기반은 `_plan/2026-11/20261107_004_BilingualI18nAndTextInput.md`, 완성된 입력 UX와 검증 기준은 `_plan/2026-11/20261108_010_TextInputUxCompletion.md`에 기록돼 있다.

`wifi`(STA 설정), `sd-storage`(PC USB MSC 접근), `wav-sounds`(등록 음원 재생),
`wifi-ap-web`(QR·웹 4-switch), `date-time`(수동 설정·SNTP 상태),
`interfaces-info`(RS485·I/O·USB command·업데이트 기반 안내)까지 구현돼 있다. 남은 page는
`camera-gallery`(촬영·저장·보기·삭제)다. 독립 `assets` page나 firmware updater page는 만들지 않으며
asset pipeline은 폰트와 WAV를 배포하는 내부 기반으로만 사용한다. 상세 상태는
`_plan/2026-11/20261108_001_PageByPageDevelopmentRoadmap.md`에서 관리한다.

## 1. 지원 환경

- Windows 10 이상
- Python 3.11 이상
- CMake 3.21 이상, Ninja, C++17 compiler
- HMI build용 PlatformIO Core
- 최초 LVGL·SDL·ESP32-P4 platform 다운로드를 위한 인터넷 연결

검증 조합은 CMake 4.4.1, Ninja 1.13.2, GNU C/C++ 16.1.0, pioarduino ESP32-P4 platform 55.03.39, LVGL 8.4.0, SDL2 2.30.12다. HMI board ID는 `esp32-p4-evboard`다.

## 2. Profile 확인과 환경 진단

모든 명령은 저장소 루트의 PowerShell에서 실행한다.

```powershell
python tools/project.py list
python tools/project.py doctor --host windows --profile main
python tools/project.py doctor --host windows --profile sample-suite
python tools/project.py doctor --host hmi --profile main
python tools/project.py doctor --host hmi --profile sample-suite
```

`doctor`는 도구와 manifest를 읽기만 하며 설치·다운로드·장치 검색·설정 변경을 하지 않는다. `Result: ready`가 아니면 표시된 도구를 준비한 뒤 같은 명령을 다시 실행한다.

## 3. Windows build와 실행

`main`을 build하고 실행한다.

```powershell
python tools/project.py build --host windows --profile main
python tools/project.py run --host windows --profile main
```

`sample-suite`를 build하고 실행한다.

```powershell
python tools/project.py build --host windows --profile sample-suite
python tools/project.py run --host windows --profile sample-suite
```

같은 명령을 실행하는 profile별 편의 wrapper는 `main_run_windows.bat`와
`samples_run_windows.bat`다.

`run`은 먼저 선택 profile을 build한 뒤 해당 실행 파일을 연다. 실행 파일은 각각 다음 위치에 생성된다.

- `build/windows-main/hosts/windows/hmi_main_windows.exe`
- `build/windows-sample-suite/hosts/windows/hmi_sample_suite_windows.exe`

창은 1024×600이다. Esc 또는 창 닫기로 종료한다.

### `main` 확인

`HMI Starter`, profile·host·ready 상태와 `Input: waiting`이 표시된다. 중앙 panel을 누르면 `Input: detected`로 바뀐다.

### `sample-suite` 확인

Dashboard 오른쪽 상단의 `한국어`/`영어`를 누르면 header, theme control,
section과 모든 page card가 즉시 같은 언어로 바뀐다. 선택한 언어는 theme
전환과 Dashboard↔page 왕복 후에도 유지된다.

Dashboard의 `UI Basics` card를 눌러 page에 들어간다. 다음 동작을 확인한다.

1. Dashboard에서 선택한 언어로 page 문구가 표시된다.
2. 짧게 누르기와 길게 누르기의 count가 각각 증가한다.
3. 목록이 scroll된다.
4. Toast가 표시 후 사라진다.
5. Confirm dialog의 cancel/confirm이 구분된다.
6. Back으로 Dashboard에 돌아간다.
7. theme toggle 후 Dashboard와 page 사이를 이동해도 dark/light 선택이 유지된다.

Dashboard의 `Text Input` card에서는 다음을 확인한다.

1. 한국어/영어 버튼이 제목, 설명, field, placeholder, 기능 키와 상태 문구를 함께 바꾼다.
2. 화면 언어와 `한/영` keyboard mode는 서로 독립적으로 전환된다.
3. 숫자행을 포함한 5행 자판에서 한글 조합, 영문 SHIFT, 기호, 넓은 Space와 글자삭제가 동작한다.
4. 활성 입력란의 caret가 점멸하며 문자열 시작·중간·끝을 누르면 가장 가까운 글자 경계로 이동한다.
5. `다음 → 다음 → 입력 완료` 순서로 field가 이동하고 입력 확인 modal에 이름·메모·값이 표시된다.
6. modal의 `수정`은 편집으로 돌아가고 `확인`은 값을 유지한 채 확인 상태만 표시한다.
7. Windows IME/physical keyboard 입력과 Backspace, Enter, 좌우 cursor가 touch 흐름과 같은 상태를 사용한다.
8. dark/light와 한국어/영어 전환 중 입력값·편집/확인 상태가 유지된다.
9. Dashboard로 나갔다 돌아오면 locale은 유지되고 입력값·cursor·미완성 조합은 초기화된다.

Dashboard의 `화면과 음량` card에서는 다음을 확인한다.

1. 밝기 1~10과 음량 0~10이 한 화면에 표시된다.
2. slider와 `낮춤`/`높임` button이 값을 즉시 적용하고 저장한다.
3. 음량 0은 mute이며 `미리 듣기`는 등록된 WAV asset을 재생한다.
4. page를 나가면 재생 중인 preview가 멈추고, 다시 들어오면 저장값을 복원한다.

Dashboard의 `등록 WAV` card에서는 다음을 확인한다.

1. `조리 시작`, `조리 완료`, `뚜껑 열림`, `뚜껑 닫힘`, `핵심 단계 시작` 다섯 행이 표시된다.
2. 목록 행을 누르면 즉시 해당 음원이 재생되고 선택 border와 상태 문구가 함께 바뀐다.
3. 진행 막대와 경과·전체·남은 시간이 갱신되며 완료 시 `다시 재생`으로 바뀐다.
4. 재생 중 primary button은 `일시정지`로 바뀌며, `계속 재생`을 누르면 멈춘 위치부터 이어진다.
5. `중지`는 즉시 재생을 끝내고 위치를 처음으로 되돌리며 idle 상태에서는 비활성화된다.
6. 지원 범위는 RIFF/WAVE 정수 PCM, mono/stereo, 8/16/24-bit,
   8/16/22.05/32/44.1/48 kHz다. 압축 WAV, IEEE float와 24 kHz는 지원하지 않는다.
7. page 이탈과 locale/theme 재구성, SD의 PC 연결 시작은 진행 중인 재생을 먼저 중지한다.

WAV는 파일 전체를 RAM에 올리지 않고 SD에서 bounded chunk로 읽는다. 등록 목록과 source
metadata는 `samples/suite/pages/wav_sounds/wav_catalog.json`, 원본은
`samples/suite/assets/sounds/`가 소유한다. firmware와 SD 자산은 별도이므로 아래 5절의
asset bundle을 SD에 설치해야 실제 HMI에서 재생된다. 구현·실기기 수용 근거는
`_plan/2026-11/20261108_016_RegisteredWavPage.md`에 기록돼 있다.

Dashboard의 `Wi-Fi 설정` card에서는 주변 AP 검색, 보안 AP 선택·비밀번호 입력,
연결·저장·연결 해제와 한국어/영어 상태 문구를 확인한다.

Dashboard의 `Wi-Fi AP 웹 제어` card에서는 다음 흐름으로 진단 AP와 로컬 웹을 사용한다.

1. `AP 시작`을 누르면 기존 STA 연결과 IPv4를 유지한 채 WPA2 진단 AP를 시작한다.
2. 첫 QR로 휴대폰을 AP에 연결한다. SSID와 임시 암호도 화면에서 직접 확인할 수 있다.
3. 휴대폰 연결이 감지되면 같은 자리에 로컬 웹 URL QR이 표시된다. 필요하면
   `Wi-Fi QR 다시 보기`로 이전 단계로 돌아간다.
4. URL을 열면 HMI와 휴대폰에 동일한 네 switch가 나타난다. 어느 쪽에서 바꿔도 controller의
   단일 state와 revision을 통해 다른 쪽에 반영되며, 재연결 뒤에는 전체 snapshot으로 수렴한다.
5. `AP 종료`나 Back은 종료 확인을 거친다. 종료하면 AP client, HTTP/WebSocket session과
   임시 credential을 정리하지만 기존 STA 연결은 유지한다.

이 AP는 인터넷을 공유하거나 captive portal을 보장하지 않는다. 로컬 HTTP/WebSocket은
TLS가 없는 근거리 진단 기능이며 최대 client는 2개다. session마다 새 임시 암호와 128-bit
token을 사용하고 URL token은 fragment에만 두며 일반 snapshot과 log에는 포함하지 않는다.
허용 endpoint는 `/`, `/favicon.ico`, `/healthz`, `/ws`로 제한되고 외부 CDN이나 인터넷
자산을 사용하지 않는다. 구현·검증 근거는
`_plan/2026-11/20261108_018_WifiApWebControlPage.md`에 기록돼 있다.

Dashboard의 `날짜와 시간` card에서는 현재 날짜·시각과 출처, RTC 및 네트워크 동기화
상태를 확인하고 년·월·일·시·분·초를 수동으로 조정한다. 편집 중인 값도 실제 시각처럼
초 단위로 계속 진행한다. 수동 적용은 시스템 시각을 먼저
변경하며 RTC 기록 실패를 부분 성공으로 따로 표시한다. Windows는 PC 시계를 바꾸지 않는
가상 시계를 사용한다. 현재 HMI는 보드의 RTC 칩과 register 호환성이 실물에서 확인되기
전까지 RTC 접근을 비활성화하고 `RTC 칩 확인 전`으로 표시한다. Wi-Fi IP를 얻은 HMI의
시스템 시각은 `Asia/Seoul` 정책으로 SNTP 동기화하며, 실제 RTC 유지와 전원 재인가 동작은
기기 접근이 가능할 때 별도로 확인한다.

Dashboard의 `인터페이스와 업데이트` card는 한 page 안에서 항목 목록과 상세 설명을
오간다. RS485, GPIO, PWM, USB CDC command, 저장소/MSC, asset updater, RTC와
firmware update foundation을 각각 열어 제공 범위, 제품 연결 지점, 조합 순서, 아직
구현하지 않은 범위와 검증 방법을 확인할 수 있다. 이 page는 설명만 표시하며 실제
hardware, network 또는 update 작업을 시작하지 않는다.

재사용 가능한 hardware 계약은 `modules/interfaces/include/hmi/interfaces/`에 있다.
`Rs485Port`, `GpioPort`, `PwmPort`, `UsbCommandPort`는 모두 비소유 context와 callback만
보관한다. 제품 pin, protocol, credential, task와 ISR은 이 계약에 넣지 않고 별도 adapter가
구현한다. 지원하지 않는 기능은 `CapabilityState::unsupported`와 결정적인 오류값으로
표현하므로 UI가 board 내부 header를 직접 포함할 필요가 없다.

firmware update 기반은 `modules/update/include/hmi/update/`에 있다. 권장 조합 순서는 다음과 같다.

1. `decode_manifest`로 크기 제한, strict JSON, 서명, device/target/만료 binding을 검증한다.
2. `ArtifactDownloader`로 4 KiB 이하 chunk를 이어받아 staging에 저장하고 SHA-256을 확인한다.
3. `InstallJournal`과 `AssetLifecycle`로 A/B CRC record, 설치 단계와 active/previous/pending
   generation을 durable하게 기록한다.
4. 제품 adapter가 `FirmwareWriterPort`와 `BootControlPort`를 구현해 실제 partition write,
   pending image 선택, 실행 확인, rollback과 재시작을 연결한다.
5. `BootCoordinator`의 순수 결정을 부팅 초기에 적용하고, 정상 확인 뒤에만 journal과
   generation을 확정한다.

저장소, 다운로드 transport, crypto, partition과 reboot의 실제 구현은 의도적으로 포함하지
않는다. 따라서 이 기반 코드만으로 장치를 갱신하거나 재부팅하지 않으며, 제품 정책과
credential을 가진 adapter가 명시적으로 연결돼야 한다. USB 관련 기능도 서로 다르다.

- USB CDC command 계약은 bounded frame을 command dispatcher에 전달하는 기반이다.
- USB MSC는 `SD 저장소` page가 SD ownership을 HMI와 PC 사이에서 전환하는 기존 기능이다.
- CDC asset updater는 font/WAV bundle을 SD에 전달하는 기존 개발 도구이며 firmware updater가 아니다.

구현 범위와 검증 근거는 `_plan/2026-11/20261108_019_InterfaceOverviewPage.md`에 기록돼 있다.

Dashboard의 `SD 저장소` card에서는 mount 상태, HMI/PC owner, FAT 여부와
전체·사용·여유 용량을 확인한다. `PC에 연결`을 누르는 순간 전체 화면 잠금이 표시되고
Dashboard 이동, 다른 page, theme/locale, 키 입력은 차단된다. PC가 SD를 사용하는 동안
HMI file API와 CDC asset updater는 SD에 접근하지 않는다. 다음 세 경우에 SD를 HMI로
재마운트한 뒤 같은 page에서 앱 기능을 다시 사용할 수 있다.

1. 잠금 화면의 `HMI로 돌아가기`를 누르고 연결 종료를 한 번 더 확인한다.
2. PC 운영체제에서 이동식 디스크를 안전 제거한다.
3. PC가 SD에 한 번 접근한 뒤 USB 케이블을 분리한다.

PC의 USB 절전(suspend)은 분리로 취급하지 않는다. 재마운트에 실패하면 잠금을 유지하며
`다시 시도`만 제공한다.

## 4. 자동 검사

profile별 선언 test를 실행한다.

```powershell
python tools/project.py check --host windows --profile main
python tools/project.py check --host windows --profile sample-suite
python tools/project.py check --scope isolation
```

Python manifest/generator 회귀 test는 다음과 같다.

```powershell
python -m unittest discover -s tools/tests -v
```

`check --scope isolation`은 현재 생성된 Windows/HMI profile 입력의 manifest hash와 교차 profile·platform source 누락 여부를 검사한다. 생성물이 없거나 manifest가 바뀌었다면 먼저 해당 profile을 `prepare` 또는 `build`한다.

## 5. HMI firmware build

다음 명령은 firmware를 compile만 하며 장치에 쓰지 않는다.

```powershell
python tools/project.py build --host hmi --profile main
python tools/project.py build --host hmi --profile sample-suite
```

같은 compile-only 명령을 실행하는 profile별 편의 wrapper는 `main_build_hmi.bat`와
`samples_build_hmi.bat`다. 두 wrapper 모두 firmware upload를 수행하지 않는다.

VS Code PlatformIO의 환경 선택 목록에 두 profile을 모두 표시하려면 generated
environment fragment를 한 번 준비한다.

```powershell
python tools/project.py prepare --host hmi --profile main
python tools/project.py prepare --host hmi --profile sample-suite
```

루트 `platformio.ini`는 `.generated/hmi/*/profile.ini`를 자동으로 읽으므로 준비 후
PlatformIO를 새로 고치면 `hmi-main`과 `hmi-sample-suite`가 함께 표시된다.
이후 PlatformIO CLI에서도 루트 설정을 그대로 사용해 선택 빌드를 실행할 수 있다.

```powershell
pio run -e hmi-main
pio run -e hmi-sample-suite
```

결과는 profile별로 분리된다.

- `.pio/build/hmi/main/hmi-main/firmware.bin`
- `.pio/build/hmi/sample-suite/hmi-sample-suite/firmware.bin`

`Text Input`의 동적 font, `Display & Audio`의 preview WAV와 `등록 WAV`의 다섯 효과음은
sample 전역 SD 카드 자산이다. 다음 명령으로 SHA-256 manifest와 SD tree를 만든다.
전역 catalog·fixed font와 user font manifest를 다시 생성하거나 stale 여부만
확인하려면 다음 owner config 기반 명령을 사용한다.

```powershell
python tools/assets/typography/generate.py --config samples/suite/i18n/catalog.json
python tools/assets/typography/generate.py --config samples/suite/i18n/catalog.json --check
python tools/assets/typography/generate_user.py --config samples/suite/i18n/catalog.json
python tools/assets/typography/generate_user.py --config samples/suite/i18n/catalog.json --check
```

생성 원본은 `samples/suite/i18n/`, firmware fixed font와 manifest는
`samples/suite/generated/`, SD에 복사할 runtime font는 `samples/suite/assets/fonts/`가
각각 소유한다. 다음 명령으로 SHA-256 manifest와 SD tree를 만든다.

fixed UI font는 SD asset이 아니다. `generate.py`가
`samples/suite/generated/fonts/fixed/*.c`의 LVGL C 배열로 만들고 firmware build에
포함한다. 따라서 catalog·typography·fixed font가 바뀌면 `generate.py` 실행 후 firmware를
다시 build/upload해야 한다. 사용자가 입력하는 동적 문자열용 `user_*.bin`만
`samples/suite/assets/fonts/`에 생성되어 SD asset으로 전송된다.

```powershell
python tools/project.py bundle-assets --profile sample-suite
```

`dist/sd/` 아래의 `hmi/` 폴더 전체를 FAT32 SD 카드 root에 복사한다. 최종 font 경로는
`hmi/sample-suite/assets/fonts/`, preview 음원은
`hmi/sample-suite/assets/sounds/volume_preview.wav`다. 등록 음원은 같은 sounds 폴더의
`cook_start.wav`, `cook_end.wav`, `lid_open.wav`, `lid_close.wav`,
`cooktech_start.wav`다. firmware와 SD asset은 서로 별도이며 firmware upload만으로
font나 WAV가 설치되지는 않는다.

실행 중인 HMI가 firmware upload용 USB CDC로 PC에 연결돼 있으면 SD 카드를
분리하지 않고 같은 묶음을 전송할 수 있다. 먼저 사용 가능한 COM port를 확인하고,
로컬 파일·크기·SHA-256만 검사한 뒤 실제 전송한다.

SD가 PC USB MSC owner인 동안 asset 전송은 `ASSET_BUSY`로 거부된다. 잠금 화면에서
HMI로 돌아오거나 PC 안전 제거/USB 분리 후 SD 재마운트가 끝난 다음 다시 실행한다.

```powershell
.\samples_update_hmi_assets.bat --list-ports
.\samples_update_hmi_assets.bat --dry-run
.\samples_update_hmi_assets.bat --port COM16
```

`samples_update_hmi_assets.bat`는 전송 전에 owner config를 검사한다. runtime
`user_*.bin`이나 user font manifest가 없거나 stale하면 먼저 다시 생성하고, 이어서 bundle을
만들어 전송한다. 생성된 `user_*.bin`은 Git 대상이 아니며 같은 config로 재현된다.
`--list-ports`는 font 생성 없이 port 목록만 조회한다.

실제 전송 명령은 먼저 HMI를 전용 진행 화면으로 전환한다. runtime font를 확인·생성하는
동안에는 `Checking and generating runtime fonts`, bundle 복사와 SHA-256 검증 중에는
`Bundling and verifying asset files`를 표시하며 진행 막대가 계속 움직인다. 이후 파일
전송이 시작되면 기존 file·byte·percent 진행률로 전환된다. PC 준비가 실패하거나 5분
동안 다음 단계가 오지 않으면 HMI는 실패 상태로 전환해 준비 화면에 무기한 머물지 않는다.

`COM16`은 장치 관리자에서 확인한 HMI USB CDC port로 바꾼다. 전송기는 현재
firmware의 profile ID가 `sample-suite`인지 확인하고, `dist/sd/hmi/sample-suite/`의
각 파일을 SHA-256으로 검증한다. HMI는 `/hmi/sample-suite.__incoming`에 먼저 기록한
뒤 journal과 backup을 사용해 `/hmi/sample-suite`를 교체하므로 다른 SD directory는
수정하지 않는다. 성공 후 Dashboard로 돌아갔다가 `Text Input`을 다시 열면 새 font를
로드한다. 전송 중 USB나 전원이 끊기면 다시 같은 명령을 실행한다.

`HMI asset protocol did not answer`는 선택한 COM port에 updater receiver가 없다는
뜻이다. Serial Monitor를 닫고, USB updater가 포함된 최신 `hmi-sample-suite`를 다시
build/upload한 뒤 port를 재확인한다. `storage_unavailable`이 표시되면 CDC 연결과
profile은 정상이며 HMI의 SD mount가 실패한 상태다. 최신 firmware는 Arduino esp32p4
보드 variant가 LDO4와 SD 전원 GPIO를 단독 관리하고, 40/20/10/4MHz 순서로 mount를
재시도한다. 최신 `hmi-sample-suite`를 올린 뒤에도 같은 오류라면 SD 카드 장착·FAT
filesystem과 부팅 시 `[storage]` log를 확인한다.

`main` build에는 `samples/` source가 없고, `sample-suite` build에는 `apps/main/app.cpp`가 없다. 두 profile은 같은 Board·HMI platform 기반을 공유하지만 앱 source와 생성 결과를 섞지 않는다.

## 6. 사용자가 수행하는 HMI upload

Codex 자동 작업은 upload·flash·장치 변경을 수행하지 않는다. 실제 장치 upload는 사용자가 직접 한다. `sample-suite`를 CLI로 upload하려면 먼저 생성 입력을 준비한 뒤 다음 명령을 **사용자 PowerShell에서만** 실행한다.

```powershell
python tools/project.py prepare --host hmi --profile sample-suite
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -d . -c .generated/hmi/sample-suite/platformio.ini -e hmi-sample-suite -t upload
```

VS Code PlatformIO를 사용하는 경우에도 `hmi-sample-suite` 환경을 선택한 뒤 사용자가 Upload를 실행한다. 연결 port와 보드 상태는 사용자가 확인한다.

## 7. 실기기 수동 확인

`sample-suite` upload 후 다음을 순서대로 확인한다.

1. 1024×600 LCD에 Dashboard card와 locale/theme control이 잘리지 않고 표시된다.
2. Dashboard에서 한국어/영어 전환 시 header·control·card 문구가 함께 바뀐다.
3. `UI Basics` 진입과 Back이 동작한다.
4. 짧게 누르기와 길게 누르기가 서로 구분된다.
5. 목록 scroll, Toast, confirm dialog의 cancel/confirm이 동작한다.
6. dark/light 전환 후 현재 page가 재구성되고 Dashboard로 돌아와도 선택이 유지된다.
7. Dashboard↔page 이동과 overlay open/close를 10회 이상 반복해 화면 떨림·멈춤·reset이 없다.
8. serial log에 display/touch/LVGL 초기화 실패, watchdog reset, 반복 allocation 실패가 없다.
9. `Text Input`에서 점멸 caret, touch 중간 이동, 한글·영문·숫자·기호·Space 입력과 글자삭제를 확인한다.
10. 이름의 `다음`, 메모의 `다음`, 값의 `입력 완료`가 차례로 동작하고 확인 modal의 세 값이 입력과 일치하는지 확인한다.
11. modal의 `수정` 후 문자열 중간에 다시 삽입할 수 있고, `확인` 후 값이 유지되는지 확인한다.
12. dark/light와 한국어/영어를 바꿔도 현재 입력값과 편집/확인 상태가 유지되는지 확인한다.
13. serial log의 `[storage] SD ready`를 확인하고 혼합 사용자 문자열에 네모 대체문자나 의도하지 않은 공백이 없는지 확인한다.
14. `화면과 음량`에서 밝기 1·10, 음량 0·10, `미리 듣기`와 page 이탈 중지를 확인한다.
15. 재부팅 후 마지막 밝기·음량이 복원되는지 확인한다.
16. `Wi-Fi 설정`에서 AP 검색, 연결, 저장, 연결 해제와 재부팅 복원을 확인한다.
17. 기존 STA가 연결된 상태에서 `Wi-Fi AP 웹 제어`의 AP를 시작해 STA IPv4가 유지되는지 확인한다.
18. 휴대폰으로 Wi-Fi QR을 읽고 진단 AP의 DHCP 주소를 받는지 확인한다.
19. client 감지 뒤 URL QR로 전환되고, 다시 Wi-Fi QR을 볼 수 있는지 확인한다.
20. URL QR이 로컬 웹을 열고 HMI↔휴대폰의 네 switch가 양방향으로 동기화되는지 확인한다.
21. 휴대폰 Wi-Fi off/on과 browser background/foreground 뒤 최신 snapshot으로 복구되는지 확인한다.
22. AP 실행 중 원래 공유기 LAN의 다른 기기에서도 기존 STA 주소로 접근되는지 확인한다.
23. locale/theme 전환과 AP 시작·종료를 반복해도 중복 callback, reset과 STA 재연결 대기가 없는지 확인한다.
24. AP 종료와 Back 확인 뒤 AP client는 끊기고 기존 STA 연결은 유지되는지 확인한다.
25. `SD 저장소`에서 실제 카드 용량과 HMI owner가 표시되는지 확인한다.
26. `PC에 연결` 직후 전체 화면 잠금이 나타나고 Back·Dashboard·다른 page·theme·locale이 동작하지 않는지 확인한다.
27. PC에서 이동식 디스크를 열고 작은 파일을 생성·수정·삭제한 뒤 안전 제거하면 잠금이 풀리고 SD page 용량이 다시 표시되는지 확인한다.
28. 다시 연결해 PC가 디스크를 읽은 뒤 USB 케이블을 분리하면 자동으로 HMI owner를 복원하는지 확인한다.
29. 다시 연결해 `HMI로 돌아가기`를 두 단계로 확인하면 PC 접근이 끝나고 앱 기능이 다시 열리는지 확인한다.
30. USB suspend만 발생한 경우 잠금과 PC owner가 유지되는지 확인한다.
31. asset updater CDC와 MSC connector를 실제 보드에서 식별하고 MSC 전환 전후 CDC log/asset protocol이 유지되는지 확인한다.
32. `등록 WAV`의 다섯 행을 각각 눌러 실제 소리와 화면 이름이 일치하는지 확인한다.
33. 재생 중 진행 막대와 경과·전체·남은 시간이 자연스럽게 변하고 완료 시 100%·`재생 완료`·`다시 재생`이 일치하는지 확인한다.
34. `일시정지`에서 소리와 시간이 함께 멈추고 `계속 재생`이 같은 위치에서 자연스럽게 이어지는지 확인한다.
35. `중지`, 빠른 행 교체, 음량 0·10과 page 이탈에서 소리가 겹치거나 남지 않는지 확인한다.
36. WAV 재생 중 `SD 저장소`의 PC 연결을 시작하면 소리가 먼저 멈추고, HMI 복귀 후 다시 재생되는지 확인한다.
37. `날짜와 시간`에서 현재 시각·출처·RTC·네트워크 상태와 년·월·일·시·분·초 조작 영역이 잘리지 않고 하단 안내가 상하 중앙에 표시되는지 확인한다.
38. 수동 시각 적용 뒤 시스템 시각은 즉시 바뀌고 RTC 미지원은 부분 성공으로 정확히 안내되는지 확인한다.
39. 보드 revision과 RTC 칩/register 호환성, CR1220 충전 금지 bit를 확인한 뒤에만 RTC adapter를 활성화한다.
40. RTC adapter 활성화 뒤 전원 재인가 유지, Wi-Fi IP 획득 후 KST 보정과 RTC 기록 조건을 확인한다.

확인 결과를 P6 근거로 기록하기 전에는 firmware compile 성공을 실기기 완료로 간주하지 않는다.

## 8. 생성물과 실행 데이터

| 경로 | 내용 | Git |
| --- | --- | --- |
| `.generated/<host>/<profile>/` | 선택 profile의 source·include·descriptor build 입력 | 제외 |
| `build/windows-<profile>/` | Windows configure/build/test 결과 | 제외 |
| `.pio/build/hmi/<profile>/` | HMI profile별 PlatformIO 결과 | 제외 |
| `.local/<profile>/` | 향후 Windows profile별 실행 데이터 | 제외 |
| `dist/sd/<hmi_sd_root>/` | profile별 SD asset tree와 SHA-256 manifest | 제외 |

생성·build 결과만 다시 만들려면 아래의 정확한 디렉터리를 제거한 뒤 `build`를 다시 실행한다. `.local/`은 사용자 실행 데이터를 위한 위치이므로 초기화를 의도한 경우에만 별도로 제거한다.

```powershell
Remove-Item -Recurse -Force .generated
Remove-Item -Recurse -Force build/windows-main,build/windows-sample-suite
Remove-Item -Recurse -Force .pio/build/hmi/main,.pio/build/hmi/sample-suite
```

## 9. 디렉터리 책임

| 경로 | 책임 |
| --- | --- |
| `profiles/` | profile registry, schema와 공통 app 계약 |
| `apps/main/` | 외부 자산 없는 최소 기본 앱 |
| `samples/suite/` | Dashboard, page 계약·registry와 현재 활성 page 수명 |
| `samples/suite/i18n/` | sample 전역 locale catalog·typography·font owner 설정 |
| `samples/suite/generated/` | sample 전역 생성 catalog·fixed font·manifest |
| `samples/suite/assets/` | sample 전역 runtime SD 자산 |
| `samples/suite/pages/<id>/` | page별 화면·상태·검사와 향후 전용 자산 |
| `modules/ui/` | 공통 theme token·style·component |
| `modules/text/` | LVGL과 플랫폼에 독립적인 UTF-8 처리 |
| `hosts/windows/` | SDL port와 선택 app의 Windows 조립·event loop |
| `hosts/hmi/` | persistent Board와 선택 app의 Arduino 조립 |
| `boards/jc1060p470c/` | LCD·touch·LVGL runtime과 Board facade |
| `platform/windows/` | SDL display/input, LVGL tick과 Windows allocator |
| `platform/hmi/` | ESP heap 기반 LVGL allocator |
| `tools/hmi_project/` | manifest 검증과 결정적 build 입력 생성 |

앱과 page는 Board 내부나 SDL을 직접 사용하지 않는다. Dashboard에 page별 include나 ID 분기를 추가하지 않고 `samples/suite/page_registry.json`에 descriptor와 page 소스만 등록한다.

## 10. 새 page 추가 원칙

새 page는 다음 위치만 변경하는 것을 기본으로 한다.

1. `samples/suite/pages/<id>/`에 page header/source/test를 추가한다.
2. `samples/suite/page_registry.json`에 descriptor를 등록한다.
3. 공통 의미가 실제로 필요한 경우에만 `modules/` 계약을 추가한다.
4. Windows `sample-suite` build/check와 isolation 검사를 실행한다.

새 page 때문에 `apps/main/`, Dashboard 내부, 기존 page, Board/LvglPort 내부를 수정해야 한다면 먼저 책임 경계 누수를 확인한다.

## 11. 현재 미구현 범위

- camera preview·촬영·SD gallery·보기·삭제
- RS485·GPIO/PWM·USB command의 최소 public contract와 안내 page
- 실물 확인을 거친 RTC register adapter와 CR1220 유지 시각 수용
- main-only export, Windows bundle, CI와 외부 배포 자동화

현재 locale과 theme는 앱 수명 동안만 유지되며 재부팅 후 기본값으로 돌아간다. 범용
Settings page는 만들지 않고 실제 장치 기능인 밝기와 음량만 `display-audio` 한 page에 둔다.
RS485·I/O·USB command는 이번 로드맵에서 실제 장치 동작을 구현하지 않으며 지원 범위를
설명하는 page만 제공한다.

## 12. 배포 제한

현재 결과물은 내부 개발·검증용이다. 외부 공개·납품 전에는 제3자 라이선스·고지, clean PC 재현, export/bundle과 전체 실제 장치 수용 검사를 G4에서 별도로 완료해야 한다. 현재 지원하지 않거나 실기기에서 확인하지 않은 기능을 지원 완료로 표시하지 않는다.
