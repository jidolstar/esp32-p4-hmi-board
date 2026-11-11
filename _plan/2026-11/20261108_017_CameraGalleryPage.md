# S08 상세 계획: Camera & Gallery Page

작성: 2026-11-08. 전면 구체화: 2026-11-11. 구현 상태: `in_progress` (C0.5 camera frame 검증).
선행 조건: S07 `wifi-ap-web` complete.

## 1. 목적과 결정 요약

JC1060P470C의 카메라에서 preview를 보고 사진을 촬영해 SD의 sample-suite 전용 폴더에
저장한다. 같은 page에서 제한된 사진 목록, 선택 보기, 삭제와 새로고침을 제공한다.

눈·코·입 기능은 **사람 식별이나 정교한 얼굴 분석이 아니라 선택형 5-point overlay**로
정의한다. Espressif의 현재 `human_face_detect` 예제가 얼굴 박스와 좌·우 눈, 코, 좌·우
입꼬리 좌표를 반환하므로, 별도 모델 학습 없이 가장 큰 얼굴 1개에 다섯 점을 표시하는
기능은 기술 후보에 넣는다.

다만 현재 저장소는 Arduino 기반 PlatformIO profile이고 설치된 framework에는 ESP32-P4의
MIPI-CSI capability header만 확인되며 `esp-video-components`와 ESP-DL이 build input으로
연결돼 있지 않다. 따라서 다음 기준으로 범위를 고정한다.

1. C0에서 camera sensor·board revision·MIPI-CSI 연결·driver와 현재 build 체계의 capture
   가능성을 먼저 확인한다. 근거 없는 pin이나 sensor 설정은 만들지 않는다.
2. C0.5에서 공식 ESP-DL two-stage face detector가 현재 profile에 framework migration,
   custom model 학습 또는 platform package patch 없이 들어오는지 작은 spike로 판정한다.
3. 위 조건과 자원 예산을 만족하면 `얼굴 특징점` toggle을 넣고, 만족하지 않으면 S08은
   preview·촬영·gallery만 구현한다. 선택 기능 실패가 기본 gallery를 막지 않는다.
4. C0/C0.5는 feasibility만 다룬다. 이 문서가 승인됐다는 이유로 camera가 지원된다고
   표시하거나 임시 driver를 제품 경로에 남기지 않는다.

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

### 3.2 조건부 범위: 5-point 얼굴 특징점

- `얼굴 특징점` toggle, 기본값 off, session을 벗어나 저장하지 않음
- 가장 큰 얼굴 1개만 선택
- 얼굴 bounding box와 좌·우 눈, 코, 좌·우 입꼬리 overlay
- detector 결과가 없는 frame은 기존 overlay를 짧은 timeout 뒤 제거
- preview보다 낮은 2~5 Hz cadence로 worker에서 inference
- detection result에는 `frame_id`, confidence와 고정 크기 좌표만 전달
- 사진 파일, 얼굴 crop, embedding, identity와 특징점 history는 저장하지 않음

### 3.3 제외

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

### 4.2 5-point overlay 포함 gate

다음을 모두 만족할 때만 조건부 범위를 유지한다.

- 공식 ESP-DL `HumanFaceDetect`의 P4 model을 version pin으로 재현할 수 있다.
- 기존 Arduino/PlatformIO host를 유지하고 component source를 저장소 밖 platform package에
  직접 patch하지 않는다.
- 별도 model 학습·quantization·dataset 작업이 없다.
- static fixture 한 장에서 face box와 5개 keypoint가 반환되고 좌표 변환 unit test가 통과한다.
- model은 우선 app flash rodata에 두며 현재 6.4 MB OTA slot에서 최종 firmware 30% 이상
  여유를 유지한다. 별도 model partition이나 SD model이 필수가 되면 기능을 제외하는 쪽을
  기본 판단으로 한다.
- detector off 대비 peak PSRAM/internal heap, largest DMA block, worker stack과 preview FPS가
  기록되고 아래 자원 gate를 만족한다.
- 기능 off일 때 detector/model runtime object와 inference buffer를 만들지 않는다.

이 gate가 실패하면 C0.5 결과를 문서에 기록하고, C4를 삭제한 새 계획으로 갱신한 뒤 기본
camera/gallery만 진행한다. 실패한 조건부 코드는 production source에 남기지 않는다.

## 5. 목표 UX와 1024×600 layout

### 5.1 화면 구조

- shell header 44 px 계약은 유지한다.
- 본문은 왼쪽 672×504 preview 영역과 오른쪽 304×504 control/gallery 영역의 2열 구조를
  기본으로 한다. 실제 gap/padding은 Windows capture에서 확정한다.
- preview는 원본 비율을 유지한 letterbox 방식으로 표시한다. crop으로 사람 얼굴이나
  저장 사진의 일부를 숨기지 않는다.
- preview 위 overlay는 camera frame 좌표 → displayed image content rect 좌표로 변환한다.
  letterbox 영역과 회전/반전도 같은 변환을 사용한다.
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

### 5.3 얼굴 특징점 UX

- toggle label은 한국어 `얼굴 특징점`, 영어 `Face landmarks`로 한다.
- 보조 설명은 `얼굴 한 개의 눈·코·입 위치만 표시합니다.`처럼 기능 한계를 명확히 한다.
- 화면에는 신원·정확도 숫자를 기본 표시하지 않는다. low confidence는 overlay를 숨기는 데만
  사용한다.
- box는 가는 선, 특징점은 6~8 px marker로 preview를 가리지 않게 한다. 색상만으로 의미를
  구분하지 않고 toggle 상태와 `얼굴 없음` text state를 함께 제공한다.
- 조건부 기능이 build에서 제외된 경우 비활성 toggle을 보여주지 않는다. `미지원` control을
  남겨 사용자가 고장으로 오해하게 하지 않는다.

### 5.4 text와 multiline 규칙

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
- detector: `excluded`, `off`, `warming`, `running`, `no_face`, `failed`
- gallery revision, selected stable ID, visible page index와 item count
- capture/detection의 request token과 completion revision

snapshot에는 raw frame pointer, file handle, sensor handle, ESP-DL result vector와 platform error
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
| detector 실패 | preview/gallery 유지, toggle off와 설명 | session 내 1회 재시도 또는 기능 off |

camera/display 오류와 storage 오류를 한 `실패` 문자열로 합치지 않는다. log에는 driver 원인을
남길 수 있지만 UI는 사용자가 취할 수 있는 행동을 기준으로 분류한다.

## 7. 모듈과 책임 경계

### 7.1 새 공통 module

`modules/camera/`는 제품과 board에 독립적인 고정 크기 type, controller와 port를 소유한다.

예상 공개 계약:

- `CameraPort`: session start/stop, frame availability, capture request와 bounded status
- `GalleryStoragePort`: profile camera root의 scan/read/write/delete operation
- `FaceLandmarkPort`: enable/disable, frame token submit와 latest fixed result
- `CameraGalleryController`: command token, lifecycle, storage owner, gallery pagination과 error mapping
- `CameraFrameLease`: 명시적 move-only frame borrow/release. driver handle을 공개하지 않음
- `FaceLandmarkResult`: face box, 정확히 5개 point, score, `frame_id`, `valid`

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
| `platform/hmi/camera/esp_dl_face_landmarks.*` | 조건부 ESP-DL model과 inference worker |
| `platform/windows/camera/virtual_camera_port.*` | 결정적 frame/capture/error fixture |
| `platform/windows/camera/virtual_gallery_storage.*` | 실제 사용자 폴더를 건드리지 않는 in-memory fixture |
| `platform/windows/camera/fixture_face_landmarks.*` | 고정 landmark/no-face/failure result fixture |

UI와 `hosts/*`는 `boards/jc1060p470c/internal/`을 include하지 않는다. `Board`는 camera UI,
gallery policy와 얼굴 의미를 알지 않는다. public facade는 low-level CSI/sensor handle이나
ESP-DL object를 반환하지 않는다.

### 7.3 page와 composition

| 위치 | 책임 |
| --- | --- |
| `samples/suite/pages/camera_gallery/camera_gallery_page.*` | LVGL tree, layout, overlay와 input mapping |
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
| `platformio.ini` 또는 저장소 소유 dependency manifest | C0/C0.5에서 승인된 camera/ESP-DL version만 pin |

Dashboard와 host에 `camera-gallery` 문자열 분기를 추가하지 않는다. 새 camera sensor variant는
board private driver/config와 host composition만 바뀌고 page/controller는 그대로여야 한다.
얼굴 overlay 제외/포함은 `FaceLandmarkPort`의 존재 capability로 조립하며 page 내부 platform
조건부 compile로 처리하지 않는다.

`modules/camera/CMakeLists.txt`와 `modules/camera/tests/`는 common controller direct test를
등록한다. 외부 camera/ML component는 version을 저장소가 추적하는 manifest에만 선언하고
사용자별 `.platformio/packages/` 파일을 수정하지 않는다.

## 8. frame, 파일과 메모리 계약

### 8.1 frame ownership

- driver가 PSRAM/DMA-capable 영역에 고정 개수 frame pool을 만든다.
- task stack, LVGL object user data와 controller snapshot에 full frame을 복사하지 않는다.
- `CameraFrameLease`는 move-only이고 scope 종료 또는 명시적 release에서 driver pool로 돌아간다.
- preview/UI와 detector가 같은 mutable buffer를 동시에 소유하지 않는다. 필요하면 detector는
  160×120 RGB888 한 장 이하의 전용 downsample scratch를 사용한다.
- UI callback에는 pointer 대신 `frame_id`/generation을 전달하고, 파괴된 page callback은
  generation mismatch로 무시한다.
- viewer 진입 전 preview lease를 반환한다. full photo decode와 live preview frame의 동시
  생존을 기본 금지한다.

### 8.2 초기 자원 상한

정확한 값은 C0 측정 뒤 고정하지만 다음을 넘기는 변경은 별도 계획 수정이 필요하다.

- preview/capture driver frame: 최대 2개
- detector RGB888 scratch: 최대 1개, 160×120×3 B
- 동시에 decode된 gallery image: 최대 4개 thumbnail 또는 full viewer 1개
- metadata: page당 8개, 전체 scan index 최대 60개
- 얼굴 result: 최대 1개 face와 5개 point
- capture/inference/storage queue: 각 최대 2 command
- camera/detector worker stack: 각 8 KiB 이하를 목표로 high-water mark 기록

detector는 preview frame마다 실행하지 않는다. 2 Hz로 시작하고 preview FPS, thermal/CPU와
응답성을 측정한 뒤 최대 5 Hz까지만 올린다. UI/LVGL thread에서 inference, JPEG encode와 SD
write를 실행하지 않는다.

### 8.3 성능/여유 gate

- 조건부 기능 off: preview 10 FPS 이상, touch-to-visual feedback 100 ms 이내
- 조건부 기능 on: preview가 off 대비 20%보다 많이 떨어지지 않고 8 FPS 이상
- landmark update: 2 Hz 이상, stale overlay 1초 이내 제거
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

### C0.5. 5-point 얼굴 특징점 feasibility — `in_progress`

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
- HMI build가 platform package patch 없이 재현된다.
- [x] feature-on 진단 전체는 제품의 6,553,600 B OTA slot 중 19.93%로 flash 70% gate를
  통과한다.
- [x] isolated fixture의 최저 main task stack 여유는 1,608 B/3,584 B(44.9%)였고 allocation
  failure는 0건이었다. model load까지 internal heap 33,244 B와 PSRAM 623,688 B를 사용했고,
  release 뒤 PSRAM은 시작값으로 복구됐다. 총 추론 시간은 35,318 us였다.
- [x] OV02C10 RAW10→hardware ISP RGB565→ESP-DL 결합 진단이 compile/link/image 생성됐다.
  app은 1,485,056 B로 제품 OTA slot의 22.66%이고 bootloader는 22,912 B다. 두 image의
  v1.0~v1.99 header와 checksum/hash가 유효하다.
- [x] 160×120 detector 좌표의 rotation, horizontal mirror, letterbox와 edge clamp native
  test가 g++ 16.1.0에서 build·실행돼 `C0_5_COORDINATE_TRANSFORM_PASS`로 끝났다.
- [ ] 실제 camera RGB565 frame에서 face와 5-point 결과를 확인한다.
- [x] `git diff --check`와 dependency license/provenance 검사가 통과한다.

**수동 검증**

- 실제 얼굴 1개가 있는 frame에서 다섯 점이 눈·코·입에 대략 맞는다.
- 안경, 측면 얼굴, 얼굴 없음에서 잘못된 점이 오래 남지 않는다.

**종료 조건**

- 4.2의 모든 포함 gate가 통과하면 C4를 유지한다.
- 하나라도 실패하면 계획에서 C4와 관련 UI/checklist를 제거하고 제외 사유를 기록한다.
- 포함/제외 어느 쪽이든 결정과 정리까지 끝나야 C1로 진행한다.

**의존**

- C0 complete.

### C1. common camera/gallery model과 Windows fixture — `pending`

**목표**

실제 장치 없이 lifecycle, atomic save, pagination, 오류와 조건부 detector 상태를 결정적으로
검증한다.

**범위**

- `modules/camera/` type, port와 `CameraGalleryController`
- Windows virtual camera/gallery와 선택된 landmark fixture
- stable ID, pagination, `.tmp` recovery와 path/delete validation
- LVGL page와 HMI driver 제외

**산출물**

- common headers/source와 static size guard
- controller/storage/frame ownership direct tests
- success/timeout/no-SD/PC-owner/full/corrupt/delete-failure fixture

**자동 검증**

- start/duplicate start/capture/stop/re-entry와 stale completion test가 통과한다.
- partial write가 final file로 보이지 않고 scan에서 `.tmp`를 제외한다.
- path escape, duplicate filename, page boundary와 최대 60개 상한 test가 통과한다.
- PC owner 전환 전 handle/lease drain test가 통과한다.
- controller/snapshot/metadata/result의 `static_assert` size guard가 통과한다.
- large automatic frame/image object와 pass-by-value snapshot 검색 결과가 0건이다.

**수동 검증**

- error grouping과 file deletion policy를 검토한다.

**종료 조건**

- platform header 없는 common module과 모든 direct test가 통과한다.

**의존**

- C0.5 결정 완료.

### C2. HMI camera·storage adapter — `pending`

**목표**

Board private driver, frame pool과 기존 SD ownership을 common port에 연결해 실제 촬영 파일을
안전하게 만든다.

**범위**

- Board-owned camera facade/private driver
- HMI camera port와 worker
- atomic capture writer, scanner, decoder와 S05 lease 연동
- live landmark overlay와 final page 제외

**산출물**

- board/platform camera source와 generator build input
- 실제 저장 format/provenance 기록
- capture/stop/timeout/MSC transition integration tests

**자동 검증**

- `python tools/project.py build --host hmi --profile sample-suite`가 통과한다.
- `python tools/project.py build --host hmi --profile main` 회귀가 통과한다.
- `python tools/project.py check --scope isolation`이 통과한다.
- UI/module의 Board internal include와 Board의 LVGL widget 생성 검색이 0건이다.
- frame pool max, file buffer, worker stack과 object size guard가 고정된다.

**수동 검증**

- 실제 장치에서 preview frame 방향/색상과 한 장 촬영 파일을 PC에서 연다.
- SD 제거, 저장 공간 부족과 MSC 전환 중 파일 손상이 없는지 확인한다.

**종료 조건**

- 실기기 preview/capture 1회와 atomic file/MSC gate가 통과한다.

**의존**

- C1 complete, 사용자 직접 upload.

### C3. Camera & Gallery page UX/UI — `pending`

**목표**

Windows와 HMI가 공유하는 1024×600 preview·촬영·목록·보기·삭제 흐름을 완성한다.

**범위**

- page tree, 2열 layout, preview/viewer 전환과 action
- gallery list/thumbnail lazy load와 delete dialog
- 한국어/영어, light/dark, error/empty/MSC state
- registry와 page-neutral context
- 조건부 landmark overlay는 C4로 제외

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

**의존**

- C2 complete.

### C4. 조건부 5-point 얼굴 overlay — `pending`

이 단계는 C0.5가 `포함`으로 판정된 경우에만 계획에 남긴다.

**목표**

기본 camera/gallery 안정성을 훼손하지 않고 가장 큰 얼굴 1개의 5개 특징점을 preview에
선택적으로 표시한다.

**범위**

- feature toggle, detector worker/cadence와 result generation
- frame→content rect 좌표 변환과 stale result 제거
- face/no-face/failure UI
- identity, tracking, 감정/시선 판단 제외

**산출물**

- selected HMI detector adapter와 Windows result fixture
- overlay renderer와 coordinate/lifecycle tests
- off/on performance/memory 표

**자동 검증**

- rotation, mirror, letterbox, edge clipping의 5-point coordinate test가 통과한다.
- stale `frame_id`, low confidence와 no-face result가 overlay를 제거한다.
- toggle off/destroy 뒤 worker, model runtime, queue와 scratch가 남지 않는다.
- preview FPS, inference cadence, flash/heap/stack이 8.3 gate를 만족한다.
- detector failure에도 capture/gallery test가 계속 통과한다.

**수동 검증**

- 정면 얼굴에서 두 눈·코·두 입꼬리가 대략 맞고 marker가 preview를 가리지 않는다.
- 얼굴 없음, 측면, 안경과 page 왕복에서 ghost overlay가 없다.
- 기능 설명이 identity/정밀 분석으로 오해되지 않는다.

**종료 조건**

- 정확도 데모, lifecycle과 자원 gate가 모두 통과한다. 실패하면 계획을 갱신해 기능을 제거하고
  C3의 기본 page 상태로 복귀한다.

**의존**

- C3 complete, C0.5 포함 결정.

### C5. 최종 통합·성능·실기기 수용 — `pending`

**목표**

기본 gallery와 최종 선택 기능의 안정성, storage ownership, 자원과 사용 문서를 검증한다.

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
- camera/face/storage direct tests와 `git diff --check`가 통과한다.
- HMI build의 RAM/flash와 runtime memory/stack 측정이 문서화된다.

**수동 검증**

- preview 10분, 촬영 30회, viewer/삭제 10회와 page 왕복 10회를 수행한다.
- 재부팅 뒤 기존 사진 목록과 잘린 `.tmp` recovery를 확인한다.
- PC MSC 진입/복귀 뒤 gallery rescan과 새 촬영을 확인한다.
- 조건부 기능이 있으면 on/off 각각의 preview와 no-face cleanup을 확인한다.
- 사용자가 S08 완료를 명시적으로 승인한다.

**종료 조건**

- 모든 자동 gate, 필수 실기기 확인과 사용자 승인이 기록된다.

**의존**

- C3 complete, C4가 계획에 남아 있으면 C4 complete, 사용자 직접 upload.

## 10. 단계 게이트 요약

| 단계 | 상태 | 핵심 종료 조건 | 다음 단계 |
| --- | --- | --- | --- |
| C0 | `complete` | sensor 근거와 실제 30-frame capture | C0.5 |
| C0.5 | `in_progress` | 5-point 포함/제외 결정과 코드 정리 | C1 |
| C1 | `pending` | common lifecycle/storage contract와 Windows direct tests | C2 |
| C2 | `pending` | 실제 preview/capture와 SD/MSC 안전 계약 | C3 |
| C3 | `pending` | 기본 camera/gallery UX와 lifecycle | C4 또는 C5 |
| C4 | `pending` 조건부 | 5-point 정확도·자원·정리 gate | C5 |
| C5 | `pending` | 최종 자동/실기기/사용자 수용 | S09 |

## 11. 위험과 대응

| 위험 | 영향 | 대응 |
| --- | --- | --- |
| board revision/sensor 불명 | probe 실패 또는 hardware 손상 가능 | C0에서 esptool revision과 sensor PID 판독을 확인 |
| Arduino build와 esp-video/ESP-DL 부조화 | integration 범위 폭증 | framework migration/package patch가 필요하면 5-point 제외, camera도 근거 없이 우회하지 않음 |
| preview/display/camera가 PSRAM·DMA 경합 | frame drop, allocation 실패 | 고정 frame pool, detector scratch 1개, largest DMA block 측정 |
| JPEG encode가 검증되지 않음 | 느린 저장 또는 큰 파일 | C0에서 sensor/codec 경로 확정, raw fallback 금지 |
| detector가 UI를 block | touch 지연과 watchdog | 전용 bounded worker, 2~5 Hz, UI thread에는 fixed result만 전달 |
| 얼굴 기능 오해 | 개인 식별/정확도 기대 | `얼굴 특징점` 명칭, 5-point 설명, identity/분석 명시적 제외 |
| SD와 USB MSC 동시 접근 | filesystem 손상 | S05 owner/lease 재사용, transition 전 drain, generation rescan |
| callback이 page보다 오래 생존 | crash/use-after-free | generation token, destructor stop/drain, re-entry test |
| corrupt/대량 gallery | boot/화면 지연 | metadata 60, page 8, lazy decode와 placeholder |
| model이 app slot을 과도하게 사용 | OTA/build 실패 | flash rodata 우선, slot 70% gate, 별도 partition 필요 시 제외 기본값 |

## 12. 후속 변경 격리 확인

- 다른 camera sensor 추가: board private config/driver와 provenance만 변경한다.
- 실제 ML model 교체: HMI `FaceLandmarkPort` 구현과 dependency pin만 변경한다.
- 얼굴 기능 완전 제거: port 미주입과 page capability만 변경하고 camera/gallery controller는 유지한다.
- gallery page size 변경: common pagination policy와 page list만 변경한다.
- 저장 format 추가: storage codec/metadata 구현을 추가하고 page는 format-neutral stable ID를 유지한다.
- 다른 profile에서 camera 사용: profile registry와 host composition을 추가하고 sample-suite 문구를
  shared module로 옮기지 않는다.

## Automated Verification Checklist

- [x] camera sensor/interface/pin/clock/reset/power 근거와 component version이 기록돼 있다.
- [x] 실제 sensor probe와 연속 30-frame capture가 통과한다.
- [ ] 5-point 기능의 포함/제외 결정이 C0.5 근거와 일치하고 dead spike code가 없다.
- [ ] frame pool, detector scratch, gallery decode, metadata와 queue 상한이 고정돼 있다.
- [ ] task stack이나 snapshot에 full frame/image/model vector가 없다.
- [ ] atomic write, `.tmp` recovery, path escape, filename collision과 delete 범위 test가 통과한다.
- [ ] SD 없음, PC MSC owner, lease busy, full과 corrupt file test가 통과한다.
- [ ] Windows fixture의 start/capture/view/delete/re-entry/destruction test가 통과한다.
- [ ] 조건부 기능이 있으면 5-point 좌표, stale frame, no-face와 toggle cleanup test가 통과한다.
- [ ] `python tools/assets/typography/generate.py --config samples/suite/i18n/catalog.json --check`가 통과한다.
- [ ] `python tools/project.py check --host windows --profile sample-suite`가 통과한다.
- [ ] `python tools/project.py check --scope isolation`이 통과한다.
- [ ] `python tools/project.py build --host hmi --profile sample-suite`가 통과한다.
- [ ] `python tools/project.py build --host hmi --profile main`이 통과한다.
- [ ] UI/module의 Board internal include와 Board의 LVGL widget 생성 검색이 0건이다.
- [ ] RAM/PSRAM/internal heap/largest DMA block, worker stack, preview FPS, inference와 SD write
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
- [ ] 조건부 5-point 기능이 있으면 눈·코·입꼬리 위치, 얼굴 없음과 toggle off 정리를 확인한다.
- [ ] 한국어/영어와 light/dark에서 clipping, multiline line spacing과 touch target이 적절하다.
- [ ] 사용자가 기본 gallery와 조건부 기능의 제한을 이해하고 S08 완료를 승인한다.
