# S08 상세 계획: Camera & Gallery Page

작성: 2026-11-08. 전면 구체화: 2026-11-11. 구현 상태: `blocked` (C5 최종 실기기 검증 대기).
선행 조건: S07 `wifi-ap-web` complete.

## 1. 목적과 결정 요약

JC1060P470C의 카메라에서 preview를 보고 사진을 촬영해 SD의 sample-suite 전용 폴더에
저장한다. 같은 page에서 제한된 사진 목록, 선택 보기, 삭제와 새로고침을 제공한다.

C0에서 OV02C10의 실기기 연속 프레임 획득을 확인했다. C0.5 얼굴 검출 실기기 진단은
RGB888 입력에서도 결과를 반환하지 못했다. 사용자 결정에 따라 S08은 기본 preview·촬영·gallery부터
완성하고, 얼굴 특징점의 포함 검증과 구현은 C5 수용 뒤 F1에서 다룬다.

공식 근거:

- [ESP32-P4 Camera Controller Driver](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/api-reference/peripherals/camera_driver.html)
- [ESP-Video Components 시작 문서](https://docs.espressif.com/projects/esp-video-components/en/latest/esp32p4/Get_Started/index.html)
- [ESP-DL Human Face Detect 모델](https://github.com/espressif/esp-dl/blob/master/models/human_face_detect/README.md)
- [ESP-DL Human Face Detect 예제](https://github.com/espressif/esp-dl/blob/master/examples/human_face_detect/README.md)

## 2. 현재 상태와 착수 전 관찰

- `boards/jc1060p470c/config.hpp`에는 LCD·touch·audio·SD 설정만 있고 camera sensor, I2C,
  reset/power, MIPI-CSI lane와 clock 설정이 없다.
- `boards/jc1060p470c/`에는 camera public facade나 private driver가 없다.
- `platformio.ini`는 pioarduino ESP32-P4와 16 MB flash, `default_16MB.csv`, PSRAM을 사용한다.
  현재 sample-suite firmware는 약 1.65 MB이고 OTA app slot은 6.4 MB지만, ESP-DL model과
  camera component를 넣은 뒤의 실제 flash/heap은 아직 측정되지 않았다.
- 설치된 ESP32-P4 framework에는 `CONFIG_SOC_MIPI_CSI_SUPPORTED`와 SoC register가 있지만,
  sensor driver와 `esp-video-components`/ESP-DL 통합은 현재 build에 없다. SoC capability만으로
  카메라 module이 동작한다고 판단하지 않는다.
- S05의 SD/USB MSC owner 전환과 `rescan_revision`이 이미 존재한다. camera writer와 gallery
  scanner는 이 owner 계약을 우회하지 않는다.
- `samples/suite/page_registry.json`에는 order 10~70 page만 등록돼 있다. S08은 order 80의
  독립 page로 추가하며 Dashboard에 page ID별 분기를 만들지 않는다.
- 공통 UI는 Windows/HMI가 같은 source를 사용한다. Windows에서는 결정적 camera/gallery
  fixture를 주입하고 HMI에서만 실제 camera adapter를 조립한다.
- 공식 two-stage 얼굴 detector는 ESP32-P4에서 160×120 1차 입력과 48×48 후보 입력을
  사용하며 5개 특징점을 반환한다. 이 결과를 사용하는 것은 비교적 작지만, 카메라/ISP와
  ESP-DL을 현재 Arduino build에 연결하는 일은 별도의 integration risk다.

## 3. 범위와 제외 범위

### 3.1 필수 범위

- live preview start/stop과 명확한 camera unavailable/error 상태
- 한 번 누르면 한 장만 촬영하는 command와 처리 중 중복 입력 차단
- SD의 `hmi/sample-suite/camera-gallery/` 아래에만 저장
- temporary file → flush/size 검증 → atomic rename 저장
- 최대 60개 metadata를 한 번에 올리지 않는 page 단위 목록
- thumbnail 또는 화면 맞춤 decode의 lazy load
- 선택 사진 보기, 삭제 확인, 삭제 뒤 목록 갱신
- SD 없음, 저장 공간 부족, 잘린 파일, decode 실패, 파일명 충돌 처리
- USB MSC가 PC owner일 때 preview 외 file 작업 또는 전체 camera session을 정책대로 중지
- page 이탈·app destroy에서 callback, worker, frame, decoder, file handle과 lease 정리
- 한국어/영어, light/dark, Windows/HMI 공통 page behavior

### 3.2 기본 S08 제외

- 얼굴 특징점 검출과 overlay. C5 이후 F1에서 검증한다.

- 얼굴 identity 등록·인식, 출석, 사용자별 이름 저장
- 표정·감정·나이·성별·시선·눈 깜박임·활성체 판정
- 눈·코·입의 윤곽 segmentation 또는 68/468-point landmark
- 여러 얼굴 tracking과 ID 유지
- 얼굴 사진 또는 embedding DB
- 동영상·오디오 녹화, cloud upload, web streaming
- filter, crop, rotate, edit, slideshow와 album 관리
- Windows에서 실제 ML inference
- S08 때문에 전체 저장소를 ESP-IDF project로 migration하는 작업

## 4. 기능 포함/제외 판정 기준

### 4.1 기본 camera/gallery gate

다음을 모두 만족해야 C1로 진행한다.

- 실제 board revision과 연결할 camera module의 정확한 sensor part number가 있다.
- MIPI-CSI lane, clock, sensor I2C address, reset/power sequence 근거가 제조사 회로 또는
  검증된 예제에서 확인된다.
- 현재 pioarduino profile에서 sensor probe와 연속 30-frame capture가 compile되고 실제
  장치에서 성공한다.
- preview용 pixel format과 저장용 format의 변환 경로가 문서화된다.
- capture buffer의 owner, 최대 개수, 할당 capability와 반환 시점이 고정된다.

하나라도 없으면 C0를 `blocked`로 두고 추측 구현을 하지 않는다.

### 4.2 얼굴 검출 진행 순서

C0.5 실기기 결과를 기록하고 기본 camera/gallery를 먼저 구현한다. 얼굴 검출의 미통과 기준은
C5 이후 F1에서 다시 검증한다.

## 5. 목표 UX와 1024×600 layout

### 5.1 화면 구조

- shell header 44 px 계약은 유지한다.
- 본문은 왼쪽 672×504 preview 영역과 오른쪽 304×504 control/gallery 영역의 2열 구조를
  기본으로 한다. 실제 gap/padding은 Windows capture에서 확정한다.
- preview는 원본 비율을 유지한 letterbox 방식으로 표시한다. crop으로 사람 얼굴이나
  저장 사진의 일부를 숨기지 않는다.
- 오른쪽 상단에는 camera 상태, 촬영 button과 마지막 저장 결과를 둔다.
- 오른쪽 하단은 세로 scroll 가능한 thumbnail 목록으로 두되, 한 번에 최대 8개 metadata와
  최대 4개 decode image만 유지한다.
- 선택 사진은 preview 영역을 재사용하는 viewer mode로 표시하고 `카메라로 돌아가기`,
  `삭제` action을 제공한다. 별도 full-screen tree를 중복 생성하지 않는다.

### 5.2 핵심 사용자 흐름

1. page 진입 시 storage owner와 camera capability를 확인한다.
2. 사용 가능하면 camera session을 시작하고 preview를 표시한다.
3. 사용자가 `촬영`을 누르면 중복 touch를 막고 capture progress를 표시한다.
4. atomic save 성공 뒤 새 사진을 목록 맨 앞에 추가하고 짧은 성공 상태를 표시한다.
5. thumbnail을 누르면 viewer mode로 전환한다. camera preview는 정지하거나 buffer를
   반환해 viewer와 동시에 큰 image buffer를 유지하지 않는다.
6. `삭제`는 filename과 함께 confirm dialog를 표시하고 성공 뒤 다음 항목 또는 빈 상태로 간다.
7. Back은 viewer에서는 preview로, preview에서는 camera session 정리 후 Dashboard로 간다.

### 5.3 text와 multiline 규칙

- 고정 label, status, error와 confirm 문구는 `samples/suite/i18n/ko.yaml`과 `en.yaml`이
  소유한다. filename, 해상도, 파일 크기와 날짜는 runtime data다.
- `page.camera_gallery.*` namespace를 사용하고 두 locale의 key/placeholder contract를 맞춘다.
- error/empty 도움말은 title과 body label을 분리한다.
- 두 줄 이상 wrap될 수 있는 status, error와 dialog body는 line spacing을 5 px 이상 명시하고
  widget 높이와 padding을 함께 검증한다.

## 6. 상태와 오류 계약

### 6.1 bounded state

`CameraGalleryController`는 UI 독립 snapshot만 노출한다.

- session: `idle`, `starting`, `previewing`, `capturing`, `viewing`, `stopping`, `failed`
- camera error: `none`, `unsupported`, `probe_failed`, `stream_failed`, `frame_timeout`,
  `out_of_memory`, `format_unsupported`
- storage error: `none`, `not_ready`, `pc_owner`, `lease_busy`, `full`, `write_failed`,
  `invalid_file`, `delete_failed`
- gallery revision, selected stable ID, visible page index와 item count
- capture의 request token과 completion revision

snapshot에는 raw frame pointer, file handle, sensor handle과 platform error
object를 넣지 않는다.

### 6.2 오류 UX

| 조건 | UI 동작 | 복구 |
| --- | --- | --- |
| camera 미지원/probe 실패 | preview 대신 원인과 Dashboard 복귀 | module/연결 확인 후 재진입 |
| frame timeout | 마지막 frame을 즉시 폐기하고 retry 제공 | session restart 1회 |
| SD 없음 | preview는 정책상 유지 가능, 촬영·gallery 비활성 | SD 삽입 후 새로고침 |
| PC가 MSC owner | camera/file action을 중지하고 S05 이동 안내 | `HMI로 돌아가기` 완료 뒤 재진입 |
| 저장 공간 부족 | 기존 gallery 유지, 촬영 실패 표시 | 파일 삭제 후 재시도 |
| 잘린/깨진 사진 | placeholder와 파일명, 삭제 허용 | 다른 사진 탐색 가능 |

camera/display 오류와 storage 오류를 한 `실패` 문자열로 합치지 않는다. log에는 driver 원인을
남길 수 있지만 UI는 사용자가 취할 수 있는 행동을 기준으로 분류한다.

## 7. 모듈과 책임 경계

### 7.1 새 공통 module

`modules/camera/`는 제품과 board에 독립적인 고정 크기 type, controller와 port를 소유한다.

예상 공개 계약:

- `CameraPort`: session start/stop, frame availability, capture request와 bounded status
- `GalleryStoragePort`: profile camera root의 scan/read/write/delete operation
- `CameraGalleryController`: command token, lifecycle, storage owner, gallery pagination과 error mapping
- `CameraFrameLease`: 명시적 move-only frame borrow/release. driver handle을 공개하지 않음

`modules/camera/`는 LVGL, Arduino, FreeRTOS, ESP-IDF, Board, SDL과 실제 filesystem header를
include하지 않는다.

### 7.2 board와 platform adapter

| 위치 | 책임 |
| --- | --- |
| `boards/jc1060p470c/camera.*` | Board-owned camera facade와 domain-neutral operation |
| `boards/jc1060p470c/internal/mipi_csi_camera.*` | sensor, CSI/ISP, DMA/frame pool과 driver lifetime |
| `boards/jc1060p470c/internal/camera_gallery_storage.*` | 기존 `Storage` lease 아래 atomic write/scan/delete |
| `boards/jc1060p470c/config.hpp` | 확인된 sensor/interface/clock/power 상수만 소유 |
| `platform/hmi/camera/board_camera_port.*` | Board facade를 common port와 worker/tick에 연결 |
| `platform/windows/camera/virtual_camera_port.*` | 결정적 frame/capture/error fixture |
| `platform/windows/camera/virtual_gallery_storage.*` | 실제 사용자 폴더를 건드리지 않는 in-memory fixture |

UI와 `hosts/*`는 `boards/jc1060p470c/internal/`을 include하지 않는다. `Board`는 camera UI,
gallery policy에 독립적이며, CSI와 sensor handle은 Board 내부에서 소유한다.

### 7.3 page와 composition

| 위치 | 책임 |
| --- | --- |
| `samples/suite/pages/camera_gallery/camera_gallery_page.*` | LVGL tree, layout과 input mapping |
| `samples/suite/pages/camera_gallery/tests/*` | state, interaction, coordinate/lifecycle test |
| `samples/suite/page_registry.json` | order 80 descriptor, capability와 verification |
| `samples/suite/profile.json` | `camera` module 선택과 test 등록 |
| `samples/suite/i18n/*` | 고정 UI text와 typography role |
| `samples/suite/app.*` | page-neutral context에 camera/gallery capability 주입 |
| `hosts/windows/main.cpp` | Windows fixture 생성·주입·tick |
| `hosts/hmi/main.cpp` | HMI adapter 생성·주입·tick |
| `tools/hmi_project/generate.py` | module/platform/board source를 capability에 따라 결정적으로 선택 |
| `tools/hmi_project/manifest.py` | `camera` module ID를 허용 목록에 등록 |
| `tools/tests/test_generation.py`, `tools/tests/test_manifest.py` | 양 host source 선택과 미선택 profile 격리 회귀 |
| `platformio.ini` 또는 저장소 소유 dependency manifest | 검증된 camera dependency version pin |

Dashboard와 host에 `camera-gallery` 문자열 분기를 추가하지 않는다. 새 camera sensor variant는
board private driver/config와 host composition만 바뀌고 page/controller는 그대로여야 한다.
`modules/camera/CMakeLists.txt`와 `modules/camera/tests/`는 common controller direct test를
등록한다. 외부 camera component는 version을 저장소가 추적하는 manifest에만 선언하고
사용자별 `.platformio/packages/` 파일을 수정하지 않는다.

## 8. frame, 파일과 메모리 계약

### 8.1 frame ownership

- driver가 PSRAM/DMA-capable 영역에 고정 개수 frame pool을 만든다.
- task stack, LVGL object user data와 controller snapshot에 full frame을 복사하지 않는다.
- `CameraFrameLease`는 move-only이고 scope 종료 또는 명시적 release에서 driver pool로 돌아간다.
- UI callback에는 pointer 대신 `frame_id`/generation을 전달하고, 파괴된 page callback은
  generation mismatch로 무시한다.
- viewer 진입 전 preview lease를 반환한다. full photo decode와 live preview frame의 동시
  생존을 기본 금지한다.

### 8.2 초기 자원 상한

정확한 값은 C0 측정 뒤 고정하지만 다음을 넘기는 변경은 별도 계획 수정이 필요하다.

- preview/capture driver frame: 최대 2개
- 동시에 decode된 gallery image: 최대 4개 thumbnail 또는 full viewer 1개
- metadata: page당 8개, 전체 scan index 최대 60개
- capture/storage queue: 각 최대 2 command
- camera/storage worker stack: 각 8 KiB 이하를 목표로 high-water mark 기록

UI/LVGL thread에서 JPEG encode와 SD write를 실행하지 않는다.

### 8.3 성능/여유 gate

- preview 10 FPS 이상, touch-to-visual feedback 100 ms 이내
- 30회 연속 촬영과 10분 preview에서 allocation 실패, watchdog, frame pool 고갈 0건
- 최종 app slot 사용률 70% 이하
- boot 이후 camera off/on 각각의 free PSRAM, internal heap, largest DMA block을 기록하고
  내부/DMA allocation failure가 없다.
- worker stack high-water mark는 각 stack의 25% 이상 남는다.

### 8.4 파일 계약

- root는 `hmi/sample-suite/camera-gallery/`로 고정하고 외부 path 입력을 받지 않는다.
- 이름은 `IMG_<monotonic-id>.<ext>` 형식으로 만들고 허용 문자와 최대 길이를 고정한다.
- RTC timestamp가 신뢰 가능할 때만 표시 metadata로 사용하며 uniqueness는 monotonic ID가
  책임진다.
- 저장 format은 C0에서 sensor/encoder 경로를 확인해 JPEG를 우선한다. 검증되지 않은 software
  encoder나 raw RGB dump를 fallback으로 자동 선택하지 않는다.
- `.tmp`는 boot/rescan에서 완성 파일로 노출하지 않고 안전하게 제거하거나 recovery 대상으로
  분류한다.
- delete 대상은 scan에서 발급한 stable ID를 다시 root 안의 canonical filename으로 resolve한
  단일 regular file이어야 한다. `..`, separator와 symlink-like escape를 거부한다.
- PC MSC owner 동안 scan/write/delete/file decode를 시작하지 않는다. owner 변경 직전 진행 중
  operation을 drain하고 handle을 닫는다.

## 9. 단계별 개발 계획

모든 단계는 순서대로 진행한다. 동시에 한 단계만 `in_progress`다. manual hardware gate가
남으면 다음 단계로 가지 않고 현재 단계를 `blocked`로 둔다.

### C0. 보드·sensor·capture feasibility — `complete`

**목표**

실제 JC1060P470C revision과 camera module의 근거를 확정하고, 현재 build 체계에서 안정적으로
한 frame을 얻을 수 있는지 판정한다.

**범위**

- board label/revision, camera connector와 module part number
- schematic/vendor example의 lane/I2C/clock/reset/power 근거
- 현재 pioarduino와 ESP-IDF camera/esp-video component 호환성
- probe, 단일 frame과 연속 30-frame diagnostic
- LVGL page, SD 저장과 얼굴 detector 제외

**산출물**

- `boards/jc1060p470c/CAMERA_PROVENANCE.md`
- sensor/interface/config 결정표
- production UI와 분리된 최소 diagnostic 또는 test harness
- frame format, size, buffer count와 release log

**자동 검증**

- [x] 확인되지 않은 camera 상수는 production `config.hpp`에 넣지 않았다.
- [x] Guition OV02C10 근거, registry `esp_video 2.1.0`, IDF 5.5.4와 source SHA-256을
  `CAMERA_PROVENANCE.md`에 기록했다.
- [x] 독립 진단 firmware가 RAW10 1288×728, buffer 2개와 30-frame 반환 경로로 빌드됐다.
  실기기 chip revision v1.3에 맞춰 image 범위를 v1.0~v1.99로 제한했고, bootloader와 app
  header의 revision 범위와 checksum/hash를 확인했다. app 산출물은 406,496 B이고
  1,500 KiB app partition의 74%가 남는다.
- [x] `python tools/project.py build --host hmi --profile sample-suite`가 통과했다.
- [x] Windows sample-suite 44/44가 통과했고 frame lease test가 duplicate return, timeout,
  uint32 wrap과 stop 중 outstanding frame을 검증했다.
- [x] app/UI의 Board private include 검색이 0건이다.

현재 Windows PlatformIO runner는 ESP-IDF project graph 생성 뒤 SCons
`FortranCommon` import 오류를 냈다. 같은 graph를 IDF 5.5.4 Ninja와 PlatformIO의 Python
package 경로로 빌드해 source compile, link, image 생성과 크기 검사를 완료했다. camera code
오류와 구분해 host tooling 이슈로 기록한다.

**수동 검증**

- [x] SoC revision은 esptool로 v1.3을 확인했고, camera sensor는 OV02C10 detector가 PID
  register를 판독한 뒤 정상 stream을 시작한 것으로 확인했다. 시각적 label 사진은 선택적
  provenance 보강 자료로 남긴다.
- [x] 사용자가 직접 올린 diagnostic firmware에서 연속 30-frame 획득과 전량 반환을 확인했다.
  각 확인 frame은 1,172,080 B였고 `PASS captured=30 returned=30 outstanding=0` 및
  `C0_CAMERA_PROBE_PASS`로 종료됐다.
- [x] 실제 frame 크기가 1288×728 RAW10의 1,172,080 B와 일치한다. RAW10 probe 범위에서
  판단할 수 없는 방향, 색상과 tearing은 C1 preview 수동 검증으로 넘긴다.

**종료 조건**

- sensor 근거와 30-frame 실기기 성공이 모두 기록된다. 하나라도 없으면 C0 `blocked`.

**의존**

- S07 complete, 사용자가 실제 camera module을 연결한 board 제공.

### C0.5. 5-point 얼굴 특징점 feasibility — `complete` (사용자 결정으로 F1에 연기)

**목표**

기본 gallery와 분리된 spike에서 공식 ESP-DL detector의 5개 특징점과 자원 비용을 확인하고
조건부 기능의 포함/제외를 결정한다.

**범위**

- pinned ESP-DL P4 `HumanFaceDetect` two-stage model
- static fixture 한 장과 C0 camera frame 한 장
- 160×120 RGB888 downsample/coordinate transform
- flash/heap/stack/latency 측정
- live overlay와 최종 UI는 제외

**산출물**

- dependency/version/build integration 결정 기록
- fixture result와 5-point 좌표 transform test
- feature off/on memory·flash·latency 비교표
- `포함` 또는 `제외`의 단일 결정과 이유

**자동 검증**

- [x] 제조사 demo와 공식 registry example에 맞춰 `human_face_detect 0.2.0`, ESP-DL 3.1.0,
  P4 MSR+MNP model과 fixture hash를 고정했다.
- [x] 공식 static fixture에서 face 1개, score 0.731059, box `[100,66,191,188]`와
  왼눈·왼쪽 입꼬리·코·오른눈·오른쪽 입꼬리 순서가 실기기에서 검증됐다.
  build된 진단의 result shape·기하 관계 자동 판정도 통과했다.
- [x] feature off build에는 detector 호출과 runtime allocation이 없고 app은 207,632 B다.
  feature on은 1,306,112 B로 증분은 1,098,480 B다.
- 기존 Arduino/PlatformIO HMI profile의 detector 통합 검증은 F1에서 수행한다. 결합 진단은 ESP-IDF로 빌드했다.
- [x] feature-on 진단 전체는 제품의 6,553,600 B OTA slot 중 19.93%로 flash 70% gate를
  통과한다.
- [x] isolated fixture의 최저 main task stack 여유는 1,608 B/3,584 B(44.9%)였고 allocation
  failure는 0건이었다. model load까지 internal heap 33,244 B와 PSRAM 623,688 B를 사용했고,
  release 뒤 PSRAM은 시작값으로 복구됐다. 총 추론 시간은 35,318 us였다.
- [x] OV02C10 RAW10→hardware ISP RGB565→ESP-DL 결합 진단이 compile/link/image 생성됐다.
  app은 1,485,056 B로 제품 OTA slot의 22.66%이고 bootloader는 22,912 B다. 두 image의
  v1.0~v1.99 header와 checksum/hash가 유효하다.
  재빌드에서 PlatformIO는 생성된 `esp_video_ipa_config.c`의 `NULL` 미정의로 중단됐고,
  같은 ESP-IDF CMake graph의 Ninja 빌드와 `esptool image-info` 검증은 통과했다.
- [x] 160×120 detector 좌표의 rotation, horizontal mirror, letterbox와 edge clamp native
  test가 g++ 16.1.0에서 build·실행돼 `C0_5_COORDINATE_TRANSFORM_PASS`로 끝났다.
- [x] `git diff --check`와 dependency license/provenance 검사가 통과한다.

**수동 검증**

- [x] [결합 진단 실기기 절차](../../diagnostics/jc1060p470c_camera_face_probe/README.md#실기기-절차)에
  따라 사용자가 직접 진단 firmware를 올리고 115200 baud 로그를 수집한다. RGB565 1288×728
  frame(1,875,328 B), 추론 시간, 메모리·스택 측정값 및 최종 판정을 기록했다.
  2026-11-12 실기기 로그에서 OV02C10 PID `0x5602`와 RGB565(`RGBP`) frame 35개까지
  확인했다. 두 종료 기록은 `C0_5_CAMERA_FACE_FAIL error=ESP_ERR_NOT_FOUND`였고, 수집된
  추론 60건 모두 `result_count=0`이었다(24,586~25,103 us).
  추론 직후 free internal heap은 409,603 B, free PSRAM은 29,408,436 B, main task stack
  high-water mark는 5,800 B였다. 종료 시 free PSRAM은 camera 초기화 직후와 같은
  33,550,300 B로 돌아왔다. OV02C10 ISP 제어 설정 누락 경고가 있었고, 약 2초의 프레임
  수신 간격은 프레임 준비 통지가 없는 VFS에서 사용한 probe의 `select()` 대기와 일치했다.
  수정본은 Guition 예제의 v3 미만 OV02C10 IPA 설정을 연결하고 `VIDIOC_DQBUF` 자체에
  2초 대기 상한을 적용한다. Windows 창에서 실기기 80×45 미리보기 수신을 확인했다.
  후속 수정본은 115200 baud에서 160×90 흑백 영상을 약 2초마다 보내고, 20초 뒤 최대
  30회 검출한다. Ninja 빌드(app 1,484,048 B)와 RGB565 샘플의 Base64 인코딩·분할 수신·
  Windows 표시 검증을 통과했다. 개선 영상에서 얼굴이 보였지만 실기기 로그는
  `ESP_ERR_NOT_FOUND`로 종료했다. 로그의 마지막 추론은 약 24.6 ms였고 main task stack
  high-water mark는 5,160 B였다.
  설치된 esp-video는 `RGBP`를 little-endian으로 정의하고, `HumanFaceDetect`의 P4 RGB565
  전처리는 big-endian을 지정한다. 진단은 little-endian RGB565를 160×120 RGB888로 변환하고
  PSRAM scratch 57,600 B를 사용한다. 색상 채널·샘플 좌표·버퍼 경계 native 검증과 Ninja 빌드
  (app 1,485,008 B)를 통과했다. RGB888 실기기 재시험도 `result_count=0`,
  `C0_5_CAMERA_FACE_FAIL error=ESP_ERR_NOT_FOUND`로 종료했다. 마지막 추론은 20.8~22.4 ms,
  free internal heap은 382,647 B, free PSRAM은 29,349,520 B, stack high-water mark는
  5,160 B였다. 카메라 해제 뒤 free PSRAM은 33,550,300 B였다.

**종료 조건**

- [x] 실기기 결과와 사용자의 연기 결정을 기록했다. 얼굴 검출 통과 기준을 C5 이후 F1로 옮겼다.
- [x] 결합 진단은 `diagnostics/`에 있고 제품 profile, host 및 module에 ESP-DL 등록이 없다.

**의존**

- C0 complete.

### C1. common camera/gallery model과 Windows fixture — `complete`

**목표**

실제 장치 없이 lifecycle, atomic save, pagination과 오류를 결정적으로 검증한다.

**범위**

- `modules/camera/` type, port와 `CameraGalleryController`
- Windows virtual camera/gallery
- stable ID, pagination, `.tmp` recovery와 path/delete validation
- LVGL page와 HMI driver 제외

**산출물**

- common headers/source와 static size guard
- controller/storage/frame ownership direct tests
- success/timeout/no-SD/PC-owner/full/corrupt/delete-failure fixture

**자동 검증**

- [x] `hmi_camera_contract`에서 move-only frame lease의 이동·명시 해제·범위 종료 시 1회 반환,
  JPEG 파일명의 경로·임시 파일·중복·정수 범위 검증, 최신 ID 60개 및 8개 단위 페이지를 확인했다.
- [x] `hmi_camera_gallery_controller`에서 시작·중복 입력·촬영·저장·보기·확인 삭제·재진입과
  이전 세션의 완료 무시를 검증했다. 저장한 fixture 픽셀과 다시 연 이미지의 일치도 확인했다.
- [x] Windows 메모리 fixture에서 부분 쓰기가 최종 이름으로 노출되지 않고, 재스캔이 허용된
  `.tmp`만 정리하며, 최종 이름 충돌 시 기존 파일이 유지됨을 확인했다.
- [x] SD 없음·용량 부족·lease busy·카메라 timeout·손상 파일·삭제 오류를 분리하고, 선택 ID 및
  canonical 이름이 맞는 regular file만 삭제함을 확인했다.
- [x] S05의 quiescing 상태에서 PC owner 전환 전에 저장 작업과 프레임 lease를 정리했다.
  HMI 복귀 뒤 새 세션, SD 제거 후 미리보기 복구, controller 파괴 시 뷰어 lease 해제도 확인했다.
- [x] controller/snapshot/metadata의 `static_assert` size guard가 통과했다. 공통 소스의
  플랫폼 헤더와 프레임 크기 자동 변수는 없고 snapshot은 참조로 조회한다.
- [x] 생성기 테스트에서 공통 소스는 두 sample-suite 호스트에, 가상 포트는 Windows에만
  포함되며 두 main 프로필에서 camera 모듈이 제외되는 것을 확인했다. `check --scope isolation`은
  Windows/HMI의 두 프로필에서 통과했다.
- [x] `python tools/project.py build --host windows --profile sample-suite`와 같은 프로필의
  `check`가 통과했다(61/61). Python 도구 테스트 59개도 통과했다.
- [x] `python tools/project.py build --host hmi --profile sample-suite`가 통과했다. 컴파일 결과의
  정적 RAM은 88,088 B, flash는 1,655,802 B였다. 공통 `poll()`의 RISC-V `-Os` 정적 스택은
  336 B로 확인했다.

**수동 검증**

- [x] 카메라 오류와 저장 오류를 별도 상태로 검토했다. 파일 삭제는 뷰어의 확인 ID를 다시 비교하고,
  저장 포트가 canonical 이름을 해석한다. Windows fixture는 메모리에서 이 정책을 검증하며
  실기기 저장과 파일시스템 확인은 C2에 둔다.

**종료 조건**

- platform header 없는 common module과 모든 direct test가 통과한다.

**의존**

- C0.5 결정 완료.

### C2. HMI camera·storage adapter — `complete`

**목표**

Board private driver, frame pool과 기존 SD ownership을 common port에 연결해 실제 촬영 파일을
안전하게 만든다.

**범위**

- Board-owned camera facade/private driver
- HMI camera port와 worker
- atomic capture writer, scanner, decoder와 S05 lease 연동
- final page 제외

**산출물**

- board/platform camera source와 generator build input
- 실제 저장 format/provenance 기록
- capture/stop/timeout/MSC transition integration tests

**현재 구현**

- `platform/hmi/camera/gallery_sd_files.*`에서 profile root 아래 `camera-gallery`의 JPEG 목록,
  읽기, 삭제와 임시 파일 복구를 구현했다. 압축 파일 크기는 1 MiB 이하로 제한하며, 읽기
  버퍼는 호출자가 공급한다. 이미지 치수는 이후 JPEG 헤더/decoder에서 확인한다.
- 기존 SD file lease를 유지한 채 새 `.tmp`를 배타적으로 생성하고, `fsync`·close·크기 확인 뒤
  FAT rename으로 확정한다. 이미 있는 최종 파일은 보존한다. 재스캔은 디렉터리를 닫은 뒤
  canonical 이름을 가진 임시 regular file을 한 번에 최대 8개 정리한다.
- Board의 `Camera` facade는 기존 touch/audio SCCB와 LCD PHY 전원을 공유한다. 고정한 Guition
  설정의 OV02C10 RAW10을 CSI/ISP에서 1288×728 RGB565로 받고 2개 DMA 슬롯으로 대여한다.
  원본 1개는 1,875,328 B이고, 제품용 camera pool 상한은 3,750,656 B다.
- 단일 8 KiB worker가 CSI 제어, hardware JPEG 인코딩·디코딩과 SD 연산을 직렬화한다.
  압축 PSRAM 버퍼는 최대 1 MiB이고 JPEG 인코딩 뒤 SD 쓰기 전에 프레임을 반환한다.
  C2 진단 화면은 표시용 644×364 RGB565 복사 버퍼 1개를 사용한다.
- Windows의 임시 Storage 백엔드에서 새 파일의 저장·읽기·삭제, 부분 파일 복구, 최종 이름
  충돌 보존, 디렉터리 미생성과 I/O 실패 구분, lease 해제를 확인했다.
- [C2 LCD 검증 환경](../../diagnostics/jc1060p470c_camera_gallery_check/README.md)에서
  공통 controller와 실제 worker를 조립한다. 제품 화면에서의 조립은 C3에서 수행한다.

**자동 검증**

- `python tools/project.py build --host hmi --profile sample-suite`와 `--profile main`이 통과했다.
- Windows `sample-suite` build/check 61개와 Python tools 테스트 59개가 통과했다.
  `python tools/project.py check --scope isolation`은 Windows/HMI의 두 프로필에서 통과했다.
- `hmi-sample-suite-camera-check` 빌드가 통과했다. PlatformIO 기준 Flash 979,938 B와
  정적 RAM 84,316 B를 사용한다. `esptool image-info`에서 앱과 부트로더 이미지의 checksum과
  hash가 유효했고, ELF에서 CSI와 JPEG 인코더·디코더 심볼을 확인했다.
- C0 센서 설정 227개와 일치함을 확인했다. Windows 검사에서 DMA 콜백 순서와 프레임 대여를,
  별도 JPEG 헤더 검사에서 정상 치수와 초과 크기·잘못된 샘플링·잘린 헤더의 거부를 확인했다.
- HMI camera 계층과 공통 모듈에서 Board private include가 없고, Board의 LVGL widget
  생성 검색은 0건이다. 실기기 worker stack 여유는 진단 로그에서 확인한다.

**수동 검증**

- [진단 절차](../../diagnostics/jc1060p470c_camera_gallery_check/README.md)에 따라 실제
  LCD에서 미리보기 방향·색상과 촬영·다시 열기를 확인한다. PC에서도 촬영 JPEG를 연다.
- USB MSC에서 촬영 파일이 열리고 HMI 복귀 뒤 SD 재마운트가 되는지 확인한다.

**실기기 중간 결과 (2026-11-12)**

- 사용자가 LCD 미리보기 방향이 맞고 푸른 색조가 있다고 보고했다. `IMG_2.jpg` 저장과
  LCD에서 두 차례 다시 열기를 확인했다. 카메라 worker의 stack 여유는 4,016 B였다.
- SD raw preflight와 MSC 준비가 성공했다(62,488,576 sectors). 당시 Windows에서 SD 드라이브는
  나타나지 않았다. HMI 복귀 뒤 SD 재마운트, 사진 2장 재스캔과 미리보기 재개를 확인했다.
- 별도 High-Speed OTG 포트(`USB2`)에서 Windows SD 드라이브 인식과 `IMG_2.jpg` 열기를 사용자가
  확인했다. 첨부 화면에 1288×728 JPEG가 표시됐고 LCD와 JPEG 모두 녹청색 색조가 관찰됐다.
- 카메라 스트림 시작 시 ISP의 중앙 영역 통계로 화이트밸런스를 한 번 계산하고 CCM으로 적용한다.
  보정 전과 같은 조명에서 흰 종이를 중앙에 둔 채 미리보기를 시작하고, LCD와 새 JPEG에서 색조를
  재확인한다. 표본 부족 또는 통계 시간 초과 시 기본 색을 유지한다.
- 보정을 포함한 진단 이미지(1,019,744 B) 빌드와 이미지 checksum/hash가 통과했다. ELF에
  AWB 통계와 CCM API가 연결됐으며 `balance_white()` 자체 stack frame은 576 B다.
  일반 HMI `sample-suite` 재빌드도 통과했다(Flash 1,661,646 B, 정적 RAM 88,096 B).
- 보정 펌웨어에서 사용자가 `[camera] white balance applied samples=111338 R=205 G=100 B=119`를
  확인했다. 표본 111,338개를 바탕으로 R 2.05배, G 1.00배, B 1.19배가 적용됐다.
  재업로드 후 LCD와 새 JPEG 모두 색감이 좋아졌음을 사용자가 확인했다.
- 사용자 요청으로 C3를 시작한다. SD 미장착·공간 부족의 실기기 실패 경로는 C5에서 확인한다.

**종료 조건**

- 실기기 preview/capture 1회와 atomic file/MSC gate가 통과한다.

**의존**

- C1 complete, 사용자 직접 upload.

### C3. Camera & Gallery page UX/UI — `complete`

**목표**

Windows와 HMI가 공유하는 1024×600 preview·촬영·목록·보기·삭제 흐름을 완성한다.

**범위**

- page tree, 2열 layout, preview/viewer 전환과 action
- gallery list/thumbnail lazy load와 delete dialog
- 한국어/영어, light/dark, error/empty/MSC state
- registry와 page-neutral context

**산출물**

- `CameraGalleryPage`, state/interaction/layout tests
- order 80 registry entry와 catalog/typography output
- 대표 Windows capture

**자동 검증**

- 한 touch가 capture/delete command를 한 번만 발생시킨다.
- viewer→preview→Dashboard Back과 destroy/re-entry test가 통과한다.
- locale/theme rebuild 뒤 controller state와 selected stable ID가 유지된다.
- thumbnail decode 상한과 corrupt placeholder test가 통과한다.
- `python tools/assets/typography/generate.py --config samples/suite/i18n/catalog.json --check`
  가 통과한다.
- `python tools/project.py check --host windows --profile sample-suite`가 통과한다.

**수동 검증**

- 한국어 light와 영어 dark에서 preview, 빈 목록, viewer, delete confirm, error를 확인한다.
- scroll 없는 핵심 action, thumbnail scroll, line spacing, touch target과 clipping을 확인한다.

**종료 조건**

- 두 locale·두 theme의 전체 기본 흐름과 lifecycle test가 통과한다.

**완료 근거**

- Windows와 HMI에서 order 80의 공통 LVGL 화면을 사용한다. 호스트가 공통 controller에 Windows
  가상 장치 또는 HMI camera worker를 주입한다.
- 촬영 프레임 축소본을 저장 성공 후 목록에 표시한다. 뷰어에서 연 사진도 같은 캐시를 사용한다.
  캐시는 최대 4개를 보관하고, 캐시에 없는 파일은 아이콘을 표시한다. 저장소 세대가 바뀌면 캐시를 지운다.
- Windows `sample-suite` 62개 테스트에서 촬영 중복 방지, 저장·열기·삭제, 취소, 언어·테마 전환 후
  선택 유지, 재진입, 썸네일 4개 상한, 손상 JPEG·SD 없음·공간 부족을 확인했다.
- Windows 1024×600의 한국어 밝은 테마와 영어 어두운 테마, 미리보기·빈 목록·뷰어·삭제 확인 화면을
  캡처해 잘림을 확인했다. HMI `sample-suite` 빌드와 네 프로필 격리가 통과했다.

**의존**

- C2 complete.

### C5. 최종 통합·성능·실기기 수용 — `blocked`

**목표**

기본 gallery의 안정성, storage ownership, 자원과 사용 문서를 검증한다.

**범위**

- final Windows/HMI/isolation regression
- 10분 preview, 반복 촬영/삭제/page 왕복과 reboot recovery
- SD/MSC 경합, corrupt file와 no-camera recovery
- README와 최종 사용자 승인

**산출물**

- RAM/PSRAM/DMA/stack/flash/FPS/write latency 기록
- 실기기 수용 기록과 대표 capture
- `README.md` 사용법, 지원 camera와 제한

**자동 검증**

- `python tools/project.py check --host windows --profile sample-suite`가 통과한다.
- `python tools/project.py check --scope isolation`이 통과한다.
- `python tools/project.py build --host hmi --profile sample-suite`가 통과한다.
- `python tools/project.py build --host hmi --profile main`이 통과한다.
- camera/storage direct tests와 `git diff --check`가 통과한다.
- HMI build의 RAM/flash와 runtime memory/stack 측정이 문서화된다.

**자동 검증 결과**

- Windows build/check에서 `sample-suite` 62개와 `main` 2개가 통과했다. 도구 테스트 59개,
  typography 생성 결과 검사, Windows/HMI 네 프로필 격리와 `git diff --check`가 통과했다.
- 사진 삭제·Wi-Fi AP 종료·UI 기본 확인창을 공통 확인창으로 연결하고, UI 기본 토스트의
  표시와 자동 종료를 공통으로 옮겼다. Windows 1024×600에서 한국어 밝은 화면과 영어 어두운
  화면의 삭제 확인창, 제목과 버튼 정렬, 헤더 위치의 포인터 입력 차단을 확인했다.
- HMI `sample-suite`와 `main`이 빌드됐다. 확인창 수정 후 `sample-suite`의 Flash 사용량은
  1,733,234 B, 정적 RAM은 90,624 B다. 일반 펌웨어 앱 이미지(1,803,456 B)의 checksum/hash를
  확인했다. C5 통합 시 Camera & Gallery, CSI, JPEG 인코더·디코더 심볼과 C2 진단 빌드도 확인했다.
- 60 ms UI timer가 받는 미리보기 프레임률을 10초마다 출력한다. 저장마다 JPEG 인코딩 시간과
  SD 쓰기 시간을 출력하고, 카메라 시작·종료 때 PSRAM과 내부 DMA 여유, worker stack을 출력한다.
  최종 이미지의 실측값은 사용자 확인 후 기록한다.

**수동 검증**

- 일반 `hmi-sample-suite`를 올리고 Dashboard의 **카메라와 사진**을 연다. 카메라 미리보기,
  촬영, 오른쪽 목록 열기, 삭제 취소·확인, 미리보기 복귀와 Dashboard 복귀를 확인한다.
- preview 10분, 촬영 30회, viewer/삭제 10회와 page 왕복 10회를 수행한다. 115200 serial에서
  `[camera] preview fps=`, `preview_started`·`preview_stopped` 및 `save bytes=`를 확인해
  FPS, PSRAM·DMA·stack 여유, JPEG 인코딩·SD 쓰기 시간을 기록한다.
- 재부팅 뒤 기존 사진 목록과 잘린 `.tmp` recovery를 확인한다.
- PC MSC 진입/복귀 뒤 gallery rescan과 새 촬영을 확인한다.
- 전원을 끈 뒤 SD를 빼고 부팅한다. 미리보기는 유지되고 촬영 버튼은 비활성화되며 SD 안내가
  표시된다. 여유 공간이 부족한 시험용 SD에서 촬영하면 저장 오류를 표시하고 기존 JPEG는 유지된다.
- 사용자가 S08 완료를 명시적으로 승인한다.

**종료 조건**

- 모든 자동 gate, 필수 실기기 확인과 사용자 승인이 기록된다.

**의존**

- C3 complete, 사용자 직접 upload.

### F1. 얼굴 특징점 재검증·구현 — `pending`

**목표**

실제 OV02C10 영상에서 얼굴 박스와 눈·코·입꼬리 5개 좌표를 검출하고, 포함 기준을 만족하면
기본값이 꺼진 선택형 overlay를 연결한다.

**범위와 산출물**

- pinned P4 model 진단, 실기기 얼굴 좌표 및 자원 비교표, 포함/제외 결정 기록.
- 포함 시 HMI detector adapter와 Windows fixture, toggle·overlay·lifecycle 및 좌표 테스트.

**자동 검증**

- 제품 Arduino/PlatformIO profile에서 framework migration이나 platform package patch 없이 pinned model이 빌드된다.
- 별도 모델 학습 없이 정적 fixture에서 얼굴 박스 및 5개 좌표가 반환된다.
- 회전·반전·letterbox, stale frame·no-face·실패·toggle off·destroy 정리가 통과한다.
- 2~5 Hz 검출, stale overlay 1초 이내 제거, preview 8 FPS 이상 및 off 대비 하락률 20% 이하를 확인한다.
- off/on PSRAM·internal heap·largest DMA block·worker stack·flash를 기록한다. 모델은 flash rodata에 두고 OTA slot 30% 이상을 남긴다.
- off 상태에서 detector/model runtime·scratch를 생성하지 않고 기본 gallery 테스트를 통과한다.

**수동 검증**

- OV02C10 영상에서 얼굴 박스와 5개 좌표를 검출하고 실제 두 눈·코·두 입꼬리 정합성을 확인한다.
- 안경·측면·얼굴 없음·페이지 왕복에서 오래 남는 overlay가 없다.

**종료 조건과 의존**

- C5 complete 이후 시작한다. 포함 기준을 모두 통과해 사용자 실기기 확인을 받거나, 실패 근거와 제외 결정을 기록한다.

## 10. 단계 게이트 요약

| 단계 | 상태 | 핵심 종료 조건 | 다음 단계 |
| --- | --- | --- | --- |
| C0 | `complete` | sensor 근거와 실제 30-frame capture | C0.5 |
| C0.5 | `complete` | 실기기 결과와 F1 연기 결정 기록 | C1 |
| C1 | `complete` | common lifecycle/storage contract와 Windows direct tests | C2 |
| C2 | `complete` | 실기기 preview·capture·PC MSC와 보정 후 색상 확인 | C3 |
| C3 | `complete` | 공통 화면과 Windows 상호작용·레이아웃, HMI 빌드 확인 | C5 |
| C5 | `blocked` | 최종 로컬 빌드·테스트 통과, 일반 펌웨어 실기기 확인 대기 | F1 |
| F1 | `pending` | 얼굴 특징점 재검증 및 포함 판정 | — |

## 11. 위험과 대응

| 위험 | 영향 | 대응 |
| --- | --- | --- |
| board revision/sensor 불명 | probe 실패 또는 hardware 손상 가능 | C0에서 esptool revision과 sensor PID 판독을 확인 |
| Arduino build와 esp-video 부조화 | integration 범위 폭증 | C2에서 pinned camera dependency와 실제 profile 빌드 검증 |
| preview/display/camera가 PSRAM·DMA 경합 | frame drop, allocation 실패 | 고정 frame pool과 largest DMA block 측정 |
| JPEG encode가 검증되지 않음 | 느린 저장 또는 큰 파일 | C0에서 sensor/codec 경로 확정, raw fallback 금지 |
| SD와 USB MSC 동시 접근 | filesystem 손상 | S05 owner/lease 재사용, transition 전 drain, generation rescan |
| callback이 page보다 오래 생존 | crash/use-after-free | generation token, destructor stop/drain, re-entry test |
| corrupt/대량 gallery | boot/화면 지연 | metadata 60, page 8, lazy decode와 placeholder |

## 12. 후속 변경 격리 확인

- 다른 camera sensor 추가: board private config/driver와 provenance만 변경한다.
- gallery page size 변경: common pagination policy와 page list만 변경한다.
- 저장 format 추가: storage codec/metadata 구현을 추가하고 page는 format-neutral stable ID를 유지한다.
- 다른 profile에서 camera 사용: profile registry와 host composition을 추가하고 sample-suite 문구를
  shared module로 옮기지 않는다.

## Automated Verification Checklist

- [x] camera sensor/interface/pin/clock/reset/power 근거와 component version이 기록돼 있다.
- [x] 실제 sensor probe와 연속 30-frame capture가 통과한다.
- [x] C0.5 결과를 기록하고 얼굴 검출 기준을 C5 이후 F1로 옮겼다.
- [ ] frame pool, gallery decode, metadata와 queue 상한이 고정돼 있다.
- [ ] task stack이나 snapshot에 full frame/image가 없다.
- [ ] atomic write, `.tmp` recovery, path escape, filename collision과 delete 범위 test가 통과한다.
- [ ] SD 없음, PC MSC owner, lease busy, full과 corrupt file test가 통과한다.
- [ ] Windows fixture의 start/capture/view/delete/re-entry/destruction test가 통과한다.
- [ ] `python tools/assets/typography/generate.py --config samples/suite/i18n/catalog.json --check`가 통과한다.
- [ ] `python tools/project.py check --host windows --profile sample-suite`가 통과한다.
- [ ] `python tools/project.py check --scope isolation`이 통과한다.
- [ ] `python tools/project.py build --host hmi --profile sample-suite`가 통과한다.
- [ ] `python tools/project.py build --host hmi --profile main`이 통과한다.
- [ ] UI/module의 Board internal include와 Board의 LVGL widget 생성 검색이 0건이다.
- [ ] RAM/PSRAM/internal heap/largest DMA block, worker stack, preview FPS와 SD write
  latency, firmware flash가 기록되고 gate를 만족한다.
- [ ] `git diff --check`가 통과한다.

## Manual Verification Checklist

- [x] 사용자가 SoC revision v1.3과 OV02C10 sensor PID 기반 실제 동작을 확인한다.
- [ ] 사용자가 직접 upload한 장치에서 preview 방향·색상·비율과 touch 응답을 확인한다.
- [ ] 촬영 30회에서 사진이 atomic하게 저장되고 목록/선택 보기에서 열린다.
- [ ] 선택 삭제 뒤 파일과 목록이 함께 갱신된다.
- [ ] 재부팅 뒤 기존 사진 목록이 복원되고 `.tmp`가 정상 사진으로 노출되지 않는다.
- [ ] SD 제거·공간 부족·깨진 사진에서 기존 gallery와 Dashboard 복귀가 가능하다.
- [ ] PC MSC 전환과 HMI 복귀 뒤 rescan/촬영/삭제가 정상이고 filesystem 손상이 없다.
- [ ] page 왕복 10회와 10분 preview 뒤 reset·멈춤·ghost callback·frame 고갈이 없다.
- [ ] 한국어/영어와 light/dark에서 clipping, multiline line spacing과 touch target이 적절하다.
- [ ] 사용자가 기본 Camera & Gallery의 동작을 확인하고 S08 완료를 승인한다.
