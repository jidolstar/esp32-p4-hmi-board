# JC1060P470C-I-W-Y camera provenance

상태: C0 완료, C0.5 얼굴 5점 검출 feasibility 진행 중.

이 문서는 S08 camera/gallery 구현에 사용할 수 있는 값과 아직 확정하면 안 되는 값을
구분한다. 제조사 데모의 설정은 강한 근거지만, 사용자가 가진 보드와 camera module이 같은
revision인지 확인하기 전에는 `config.hpp`의 제품 상수로 승격하지 않는다.

## 확인한 출처

| 출처 | 고정점 | 용도 |
| --- | --- | --- |
| [Guition P4-series](https://github.com/guitionofficial/P4-series) | commit `c2dfeca968a0dc30fbb50c05a740cc91c1d3c43c` | `JC1060P470C_I_W_Y_New_Panel` 제조사 ESP-IDF 5.5.4 demo |
| 제조사 `esp_brookesia_phone/sdkconfig` | 위 commit의 저장 파일 | sensor, SCCB, format, ISP 선택 |
| 제조사 `components/apps/camera/app_video.c` | 위 commit의 저장 파일 | `esp_video_init`, V4L2, 2-buffer 반환 흐름 |
| 제조사 vendored `esp_cam_sensor` | version `2.1.0`, commit `70d2ce41c5ae6a5a9f4603ddd1ab93f8c8dcaa71` | OV02C10 detect와 mode table |
| [Espressif esp-video-components](https://github.com/espressif/esp-video-components) | 공식 upstream | V4L2 capture API와 component compatibility 대조 |
| 현재 저장소 toolchain | pioarduino `55.03.39`, Arduino core `3.3.9`, IDF libs `5.5.4` | 제품 build와 제조사 IDF major/minor/patch 일치 확인 |

제조사 경로는 다음 두 revision을 따로 제공한다.

- `JC1060P470C_I_W_Y_New_Panel`
- `JC1060P470C_I_W_Y_Old_Panel`

camera 설정은 두 경로가 존재한다는 사실만으로 실제 보드 revision을 판정할 수 없다. LCD
panel 이름과 camera module label을 혼동하지 않는다.

## 제조사 설정에서 확인한 결정표

| 항목 | 확인 값 | 근거 수준 | C0 처리 |
| --- | --- | --- | --- |
| SoC revision | ESP32-P4 v1.3 | 실기기 esptool 응답 | 진단 image를 v1.0~v1.99로 제한 |
| SoC camera interface | ESP32-P4 MIPI CSI | 제조사 demo + ESP-IDF capability | 진단에 사용 |
| sensor | OmniVision OV02C10 | `CONFIG_CAMERA_OV02C10=y` | 실물 label 확인 전 후보 |
| SCCB address | `0x36` | vendored OV02C10 header | 진단 log에 기대값 기록 |
| SCCB controller | I2C port 0 | 제조사 sdkconfig | 진단에 사용 |
| SCCB SDA/SCL | GPIO 7 / GPIO 8 | 제조사 sdkconfig | 진단에 사용 |
| SCCB frequency | 100 kHz | 제조사 sdkconfig | 진단에 사용 |
| reset / power-down | 둘 다 `-1` | 제조사 sdkconfig | 별도 GPIO 제어 없음 |
| input clock | 24 MHz | OV02C10 mode table | sensor driver가 mode에서 적용 |
| MIPI mode | 1 lane, RAW10 | mode name과 `.lane_num = 1` | 진단 format |
| sensor size/rate | 1288×728 @ 30 fps | sdkconfig + mode table | 진단 목표 |
| MIPI clock | 400 MHz | mode table | sensor driver 소유 |
| capture buffer | 2개 | 제조사 `EXAMPLE_CAM_BUF_NUM` | 최소 pool 상한 |
| preview output | ISP를 거친 RGB565 | 제조사 app format/ISP config | C1 preview 후보 |
| capture input | Bayer RAW10 | sensor mode | ISP 없이 UI에 직접 표시 금지 |

OV02C10 Kconfig help 한 줄은 1288×728 mode를 “2-lane”이라고 설명하지만, 같은 component의
mode 이름, 파일 이름과 실제 `.lane_num`은 모두 1-lane이다. C0 진단은 실행되는 driver 값인
1-lane을 따르고 이 불일치는 기록으로 남긴다.

GPIO 7/8은 현재 touch bus의 SDA/SCL과 같다. 제조사 camera app도 기존 I2C bus handle을 받을
수 있게 되어 있으므로 최종 제품 adapter에서는 Board가 버스를 소유하고 공유해야 한다.
camera code가 별도 I2C owner를 생성하는 방식은 진단에만 허용한다.

## component/build 결정

- 제품 profile은 계속 Arduino/PlatformIO를 사용한다. C0 때문에 framework를 migration하지
  않는다.
- 설치된 Arduino framework에는 low-level CSI driver가 있지만 `esp_video`, sensor와 IPA
  component source가 없다.
- 따라서 `diagnostics/jc1060p470c_camera_probe/`만 ESP-IDF 5.5.4 standalone build로 격리한다.
  제품 source와 `Board` public facade에는 아직 연결하지 않는다.
- 실기기 esptool 응답은 chip revision v1.3이다. ESP32-P4의 v1.x와 v3.x build는 상호
  호환되지 않으므로 진단 `sdkconfig.defaults`는 v1.0~v1.99를 선택한다. bootloader와 app
  header도 같은 revision 범위, 유효한 checksum/hash로 확인했으며 revision 불일치 시
  force flash를 금지한다.
- 제조사 BSP manifest는 `esp_video ~2.0`을 요구하면서 OV02C10이 추가된
  `esp_cam_sensor 2.1.0` 사본을 local component로 override한다. 같은 version 문자열의
  registry `esp_cam_sensor 2.1.0`에는 OV02C10이 없으므로 제조사 선언을 registry만으로
  그대로 재현할 수 없다.
- 진단은 registry의 `esp_video 2.1.0`과 그 resolver가 고른 공식 sensor/IPA component를
  사용하고, OV02C10 detector만 별도 `guition_ov02c10` component로 연결한다. 이 component는
  Guition commit `c2dfeca968a0dc30fbb50c05a740cc91c1d3c43c`의 관련 source 8개를 각각
  SHA-256으로 고정해 configure 시 가져온다. platform package나 managed component는 수정하지
  않는다.
- 실제 제품 통합 방식은 C0의 30-frame 성공 후 C1 시작 전에 결정한다. platform package를
  직접 수정하는 방식은 허용하지 않는다.

## buffer와 반환 계약

- 진단 pool은 정확히 2개다.
- `VIDIOC_DQBUF` 성공 시 해당 index 하나만 lease 상태가 된다.
- frame metadata를 기록한 뒤 모든 경로에서 `VIDIOC_QBUF`로 즉시 반환한다.
- timeout, duplicate return과 stream stop 중 outstanding token은 세대 번호로 무효화한다.
- 이 정책의 플랫폼 독립 상태는
  `boards/jc1060p470c/internal/camera_frame_lease_tracker.hpp`와 native test로 검증한다.
- 최종 preview adapter는 raw pointer를 UI snapshot이나 common controller에 저장하지 않는다.

## 실기기에서 채워야 할 기록

아래 항목이 채워지기 전 C0는 완료가 아니다.

| 항목 | 결과 |
| --- | --- |
| 보드 전면/후면 label과 panel revision | 시각 확인 없음; SoC revision은 esptool로 대체 확인 |
| SoC chip revision | ESP32-P4 v1.3 (esptool 확인) |
| camera module label 또는 선명한 사진 | 시각 확인 없음; OV02C10 PID register 판독으로 대체 확인 |
| probe가 보고한 sensor/driver/card/bus | OV02C10 PID 판독과 driver 동작 확인, startup의 card/bus log는 미기록 |
| 30/30 frame, queue return 30/30 | 통과, outstanding 0 |
| 실제 width × height / fourcc / bytesused 범위 | 1288×728 RAW10, 모든 확인 frame 1,172,080 bytes |
| 방향 / 좌우 반전 / 색상 / tearing | 미실행 |
| 실패 또는 timeout log | 30-frame 구간에서 없음 |

## 2026-11-12 실기기 capture 결과

- chip revision: ESP32-P4 v1.3
- driver log: `ov02c10`, stream 정상 종료
- buffer index: 0과 1이 교대로 반환됨
- frame 25~30 `bytesused`: 모두 1,172,080 bytes
- 종료: `PASS captured=30 returned=30 outstanding=0`
- 판정 marker: `C0_CAMERA_PROBE_PASS`

1,172,080 bytes는 1288 × 728 × 10 bit ÷ 8과 정확히 일치하므로 설정한 RAW10 frame
크기와 실측 buffer 크기가 일치한다.

## C0 판정

정적 근거, build 호환성과 실제 30-frame capture를 확인했다. OV02C10 detector는 SCCB의
`0x300A/0x300B` PID register를 읽고 기대 PID와 다르면 초기화를 실패시키므로 정상 stream과
30/30 PASS는 sensor의 직접 확인 근거다. SoC revision도 esptool로 v1.3을 확인했다. 따라서
C0 판정은 `complete`다. 보드/panel과 module label 사진은 선택적 provenance 보강 자료로
남기며, RAW10 probe만으로 판단할 수 없는 방향·색상·tearing은 C1 preview에서 확인한다.
