# JC1060P470C-I-W-Y C0.5 camera face probe

C0에서 검증한 OV02C10 1288×728 RAW10 stream을 hardware ISP의 RGB565 frame으로 받고,
C0.5에서 고정한 ESP-DL 3.1.0 two-stage face detector에 직접 연결하는 결합 진단이다.
제품 camera/gallery, LVGL, SD, 얼굴 identity recognition은 포함하지 않는다.

## Build

저장소 root에서 build-only로 실행한다.

```powershell
pio run -d diagnostics/jc1060p470c_camera_face_probe
```

이 진단은 C0 camera probe와 동일한 checksum 고정 OV02C10 component를 사용한다. dependency는
`esp_video 2.1.0`, `human_face_detect 0.2.0`, 그 exact dependency인 `esp-dl 3.1.0`이다.
Codex는 장치에 직접 upload하지 않는다.

2026-11-12 build 측정은 다음과 같다.

| 산출물 | 크기 | 비고 |
| --- | ---: | --- |
| app image | 1,485,056 B | 진단 factory 1,536,000 B 중 96.68% |
| bootloader | 22,912 B | ESP32-P4 v1.0~v1.99 |

app image는 제품 OTA slot 6,553,600 B 기준 22.66%다. app과 bootloader 모두 checksum/hash와
v1.0~v1.99 revision header가 유효하다. 현재 PC의 PlatformIO Core 6.2 `tool-scons` 중복 설치
문제로 표준 wrapper가 불안정해, 같은 CMake graph의 C++ compile/link와 image 생성을
Ninja/esptool build-only 단계로 각각 검증했다. platform package source patch는 하지 않았다.

## Coordinate transform test

160×120 detector 좌표의 rotation, horizontal mirror, letterbox와 edge clamp는 Windows native
test로 분리했다.

```powershell
cd diagnostics/jc1060p470c_camera_face_probe/coordinate_transform_test
powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
  ../../../.agents/skills/windows-native-gpp-test/scripts/run_native_tests.ps1 `
  -Environment coordinate-transform-test
```

g++ 16.1.0 build와 assertion 실행은 `C0_5_COORDINATE_TRANSFORM_PASS`로 통과했다.

## 실기기 절차

1. 사용자가 firmware를 직접 upload한다.
2. 카메라 정면 약 0.5~1.5 m에 얼굴 한 개를 둔다.
3. 115200 baud monitor를 연결하고 reset한다.
4. 10초 monitor grace period 뒤 최대 30 frame 동안 검출 결과를 확인한다.

성공 log는 다음을 포함한다.

- `format width=1288 height=728 fourcc=RGBP`
- `camera frame=... bytes=1875328`
- `result_count`가 1 이상인 frame과 box/5-point 좌표
- camera+ISP+model의 heap/PSRAM/largest block과 stack high-water snapshot
- `C0_5_CAMERA_FACE_PASS`

얼굴이 검출되지 않으면 `C0_5_CAMERA_FACE_FAIL`로 끝난다. 이것만으로 기능 제외를 결정하지
않고 조명·거리·방향을 확인해 다시 실행한다. 좌표의 실제 눈·코·입 정합성과 preview 방향은
C4 overlay 수동 gate에서 최종 확인한다.
