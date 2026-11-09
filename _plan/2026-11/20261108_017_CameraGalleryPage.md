# S07 계획: Camera & Gallery Page

작성: 2026-11-08. 상태: `pending`. 선행 조건: S06 `wav-sounds` complete.

## 1. 목표와 범위

카메라 화면에서 preview를 보고 사진을 촬영해 SD의 profile 전용 폴더에 저장한다. 같은
page에서 사진 목록, 선택 보기, 삭제와 목록 새로고침을 제공한다. 동영상, 필터, 편집,
cloud upload, 얼굴 인식은 제외한다.

카메라는 202609에 구현돼 있지 않으므로 새 기능이다. ESP32-P4의 camera controller가 있다는
사실만으로 현재 보드의 sensor·pin·clock·reset·I2C 설정이 확정됐다고 간주하지 않는다.
driver 후보와 controller 제약은 Espressif의
[ESP32-P4 Camera Controller Driver](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/api-reference/peripherals/camera_driver.html)와
[ESP-Video Components 시작 문서](https://docs.espressif.com/projects/esp-video-components/en/latest/esp32p4/Get_Started/index.html)를 기준으로 확인한다.

## 2. 안전/메모리 계약

- camera public port는 start preview, capture, stop, status만 제공한다.
- sensor와 DMA/frame buffer는 Board private implementation이 소유한다.
- preview는 화면에 필요한 bounded buffer 수와 해상도로 제한하며 task stack에 frame을 두지 않는다.
- capture는 temporary file → flush/검증 → rename으로 저장한다.
- 파일명은 monotonic ID 또는 안전한 timestamp를 쓰고 경로 입력을 받지 않는다.
- gallery는 page 단위로 제한된 metadata만 읽고 thumbnail/full image를 lazy load한다.
- delete는 확인 dialog 후 profile camera 폴더 안의 선택 파일 하나만 삭제한다.
- S05 MSC owner 중 preview/capture/list/delete를 중지하거나 unavailable로 표시한다.

## 3. 단계

### C0. 하드웨어 feasibility gate

보드 revision, camera sensor model, MIPI-CSI/DVP 연결, I2C 주소, clock/reset/power pin,
지원 driver와 capture format을 확인한다. 최소 camera probe와 단일 frame 획득이 compile 및
실기기에서 확인돼야 C1로 간다. 확인되지 않으면 추측 pin으로 구현하지 않고 `blocked`다.

### C1. camera/storage port

Board private driver, bounded frame ownership, capture encoder 또는 sensor JPEG path, atomic file
writer와 gallery metadata scan을 구현한다. start/stop/capture 실패, SD 없음, 파일 충돌,
삭제 범위, MSC 충돌 test를 통과해야 C2로 간다.

### C2. page UI

preview 영역, 촬영 button, 저장 결과, 제한된 gallery 목록, 선택 보기, 삭제 확인을 구현한다.
preview frame callback은 UI 수명을 넘지 않으며 page 이탈 시 camera와 frame을 해제한다.
navigation/destruction와 두 locale layout을 통과해야 C3로 간다.

### C3. 성능과 실기기 수용

frame rate보다 UI 응답성과 안정성을 우선해 해상도/buffer 수를 확정한다. RAM/PSRAM/DMA,
worker stack, SD write time과 firmware size를 기록한다. 사용자가 반복 촬영, 재부팅 후 목록,
보기, 삭제, SD/MSC 충돌과 page 왕복을 확인하면 S07을 완료한다.

### 단계 게이트 요약

| 단계 | 목표·범위 | 산출물 | 자동 확인 | 수동 확인 | 종료 조건·의존 |
| --- | --- | --- | --- | --- | --- |
| C0 | sensor/interface/pin/driver feasibility | 보드 config 근거, probe, format 결정 | config/source/compile probe 검사 | 단일 frame 실기기 확인 | sensor probe 성공. S06 complete |
| C1 | camera/frame/storage/gallery 기반 | private driver, public port, atomic writer, scanner | frame owner·capture 실패·path/delete·MSC 충돌 test | mock frame/사진 확인 | 자원·파일 안전 계약 통과. C0 complete |
| C2 | preview/capture/gallery UI | preview, capture, list/view/delete UI | locale/navigation/re-entry/destruction test | Windows mock 전체 흐름 확인 | callback/frame 정리 통과. C1 complete |
| C3 | 성능과 실제 camera 수용 | memory/latency/build/실기기 기록 | Windows/HMI build, isolation | 방향·색상·촬영·재부팅 목록·삭제·MSC | 사용자 승인. C2 complete |

## Automated Verification Checklist

- [ ] sensor/pin/driver 근거가 보드 설정에 명시돼 있다.
- [ ] frame owner와 최대 buffer 수/해상도가 고정돼 있다.
- [ ] atomic capture write와 잘린 파일 복구 test가 통과한다.
- [ ] gallery pagination, 안전한 path, delete 범위 test가 통과한다.
- [ ] MSC owner와 SD 없음에서 camera file 작업이 거부된다.
- [ ] mock camera page navigation/re-entry/destruction test가 통과한다.
- [ ] Windows/HMI build와 isolation check가 통과한다.
- [ ] RAM/PSRAM/DMA, worker stack, write latency와 firmware size가 기록된다.

## Manual Verification Checklist

- [ ] preview 방향·색상·비율이 올바르고 touch가 응답한다.
- [ ] 촬영한 사진이 SD에 저장되고 목록/선택 보기에서 열린다.
- [ ] 선택 삭제 후 파일과 목록이 함께 갱신된다.
- [ ] 재부팅 후 기존 사진 목록이 복원된다.
- [ ] 반복 촬영·page 왕복·MSC 전환 뒤 reset·누수·filesystem 손상이 없다.
