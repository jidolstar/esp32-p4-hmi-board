# JC1060P470C-I-W-Y C0.5 camera face probe

C0에서 검증한 OV02C10 1288×728 RAW10 stream을 hardware ISP의 little-endian RGB565로 받는다.
160×120 RGB888로 변환한 뒤 ESP-DL 3.1.0 two-stage face detector에 전달한다. 검출용 PSRAM
scratch는 57,600 B이고, 검출된 좌표는 로그에 원본 1288×728 프레임 기준으로 출력한다.
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
| app image | 1,485,008 B | 진단 factory 1,536,000 B 중 96.68% |
| bootloader | 22,912 B | ESP32-P4 v1.0~v1.99 |

app image는 제품 OTA slot 6,553,600 B 기준 22.66%다. app과 bootloader 모두 checksum/hash와
v1.0~v1.99 revision header가 유효하다. 이전 PlatformIO 빌드는 ISP 설정이 비어 있을 때 생성된
`esp_video_ipa_config.c`에서 `NULL` 미정의 오류로 중단됐다. 수정본은 Guition 예제의 v3 미만용
OV02C10 IPA 설정을 사용하며, ESP-IDF CMake graph의 Ninja 빌드에서 설정 생성, compile, link,
image 생성과 partition 크기 검사를 통과했다.

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

1. 사용자가 `diagnostics/jc1060p470c_camera_face_probe/.pio/build/camera-face-probe/`의
   `flash_args`에 따라 진단 이미지를 직접 upload한다. 해당 빌드 디렉터리에서 `COMx`를
   실제 포트로 바꿔 실행한다.

   ```powershell
   esptool --chip esp32p4 --port COMx write-flash '@flash_args'
   ```

2. COM 포트를 사용 중인 시리얼 모니터를 닫는다. 저장소 루트에서 115200 baud 미리보기
   창을 연다.

   ```powershell
   powershell.exe -NoProfile -STA -ExecutionPolicy Bypass -File .\diagnostics\jc1060p470c_camera_face_probe\preview.ps1 -Port COMx
   ```

3. 보드를 reset한다. 초기 대기 10초가 지나면 160×90 흑백 영상이 약 2초마다 갱신된다.
   20초 카운트다운 동안 카메라 앞 약 0.5~1.5 m에서 얼굴 한 개를 영상 중앙에 맞춘다.
4. 카운트다운이 끝나면 최대 30회 검출한다. 미리보기 창을 실행한 PowerShell에 로그와
   최종 판정이 출력된다. 재시도하려면 창을 열어 둔 채 보드를 reset한다.

성공 log는 다음을 포함한다.

- `format width=1288 height=728 fourcc=RGBP`
- `detector input RGB888 160x120 scratch_psram=57600`
- `camera frame=... bytes=1875328`
- `result_count`가 1 이상인 frame과 box/5-point 좌표
- camera+ISP+model의 heap/PSRAM/largest block과 stack high-water snapshot
- `C0_5_CAMERA_FACE_PASS`

얼굴이 검출되지 않으면 `C0_5_CAMERA_FACE_FAIL`로 끝난다. 이것만으로 기능 제외를 결정하지
않고 조명·거리·방향을 확인해 다시 실행한다. 좌표의 실제 눈·코·입 정합성과 preview 방향은
C4 overlay 수동 gate에서 최종 확인한다.
