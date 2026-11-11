# JC1060P470C-I-W-Y C0.5 face fixture probe

제품 camera/gallery와 분리된 ESP-DL feasibility firmware다. 공식 `human_face_detect 0.2.0`
component가 고정하는 ESP-DL 3.1.0 two-stage MSR+MNP model을 ESP32-P4 v1.x에서 실행한다.
live camera, LVGL, SD와 얼굴 인식(identity recognition)은 포함하지 않는다.

## 입력과 provenance

- fixture: ESP Component Registry의 ESP-DL 3.1.0 `human_face_detect` 공식 example
- fixture SHA-256: `65d23c18bd5d75370d3eff48130ac441b99f9ffd75bbfd6eff4c9ae0d8b4d68a`
- fixture size: 39,381 B, JPEG 320×240
- model component: `espressif/human_face_detect ==0.2.0`, MIT
- inference dependency: `espressif/esp-dl ==3.1.0` (위 model component의 exact dependency)
- P4 model sizes: MSR 61,248 B, MNP 129,968 B

공식 출력의 keypoint 배열 순서는 왼눈, 왼쪽 입꼬리, 코, 오른눈, 오른쪽 입꼬리다. 진단은
score, box 범위와 이 다섯 점의 기본 기하 관계를 검사한다. 모델은 flash rodata에 두어 기능
포함 시 app image 비용을 측정한다.

## Build

저장소 root에서 build-only로 실행한다.

```powershell
pio run -d diagnostics/jc1060p470c_face_probe
pio run -d diagnostics/jc1060p470c_face_probe -e face-probe-off
```

두 번째 환경은 동일 dependency/config에서 detector 호출과 runtime allocation이 제거되는
feature-off 기준 image다. 두 app binary 크기 차이를 feature의 flash 증분으로 기록한다.

2026-11-12 build 측정은 다음과 같다.

| 환경 | app image | 6,553,600 B 제품 OTA slot 사용률 |
| --- | ---: | ---: |
| feature off | 207,632 B | 3.17% |
| feature on | 1,306,112 B | 19.93% |
| 증분 | 1,098,480 B | 16.76%p |

feature-on bootloader와 app header는 v1.0~v1.99, 유효한 checksum/hash로 확인했다.

2026-11-12 사용자가 직접 올린 feature-on firmware의 공식 fixture 추론도 통과했다.

- 결과: `result_count=1`, score `0.731059`, box `[100,66,191,188]`
- 특징점: 왼눈 `[116,113]`, 왼쪽 입꼬리 `[120,159]`, 코 `[131,144]`,
  오른눈 `[155,113]`, 오른쪽 입꼬리 `[149,159]`
- 총 추론 시간: 35,318 us
- 최저 main task stack 여유: 1,608 B / 3,584 B (44.9%)
- 시작→model load: internal heap 33,244 B, PSRAM 623,688 B 사용
- release 뒤 PSRAM은 시작값 33,551,716 B로 복구됐고 internal largest block은
  253,952 B로 유지됐다.
- 종료 marker: `C0_5_FACE_FIXTURE_PASS`

이 저장소의 자동화는 장치를 직접 flash하지 않는다. 실기기 ESP32-P4가 v1.3이므로 이 진단의
bootloader와 app도 v1.0~v1.99만 대상으로 한다. revision 불일치를 force하지 않는다.

## 성공 조건

serial log에 다음 항목이 모두 있어야 한다.

- reset 뒤 USB CDC monitor 재연결을 위한 10초 대기와 `fixture probe begins`
- `fixture jpeg_bytes=39381 decoder_fast=0`으로 공식 JPEG decoder 설정 확인
- `result_count=1`
- score와 box, 다섯 keypoint 좌표
- startup/decoded/model_loaded/inference_done/released memory snapshot
- `C0_5_FACE_FIXTURE_PASS`

fixture와 isolated runtime 자원 gate 통과만으로 C0.5는 완료되지 않는다. 다음으로 실제
camera RGB frame 1장과 coordinate transform을 검증하고, 제품 composition의 feature off에서
detector allocation이 없는지 확인해야 한다.
