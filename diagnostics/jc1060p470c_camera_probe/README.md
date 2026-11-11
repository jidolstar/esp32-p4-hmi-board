# JC1060P470C-I-W-Y C0 camera probe

제품 UI와 분리된 일회성 C0 진단 firmware다. OV02C10을 probe하고 MIPI CSI RAW10
1288×728 stream에서 frame 30개를 dequeue한 뒤 각각 즉시 queue로 반환한다. SD, LVGL,
얼굴 detector와 사진 저장은 포함하지 않는다.

## Build

저장소 root에서 다음 build-only 명령을 실행한다.

```powershell
pio run -d diagnostics/jc1060p470c_camera_probe
```

이미 ESP-IDF 5.5.4 환경을 활성화한 경우 표준 CMake 진입점도 사용할 수 있다.

```powershell
idf.py -C diagnostics/jc1060p470c_camera_probe build
```

PlatformIO가 ESP-IDF 5.5.4와 pin된 Espressif component를 처음 받을 때는 시간이 걸릴 수 있다.
산출 firmware는 diagnostic 아래 `.pio/build/camera-probe/`에 생성된다. 이 저장소의 자동화는
장치를 직접 flash하지 않는다.

실기기에서 확인한 ESP32-P4 chip revision은 v1.3이다. ESP32-P4의 v1.x와 v3.x 이미지는 서로
호환되지 않으므로 이 진단은 `CONFIG_ESP32P4_REV_MIN_100=y`로 v1.0~v1.99만 대상으로 빌드한다.
`esptool image-info`로 bootloader와 application 모두 최소 v1.0, 최대 v1.99 및 유효한
checksum/hash를 확인했다. 이전 v3.1용 산출물이나 esptool의 force 옵션을 사용하지 않는다.

2026-11-11의 Windows 검증 환경에서는 PlatformIO가 CMake graph를 만든 뒤 SCons
`FortranCommon` import 오류를 냈다. 생성된 graph를 같은 IDF 5.5.4 Ninja toolchain으로
이어 빌드해 `jc1060p470c_camera_probe.bin` 생성과 app partition 크기 검사를 통과했다. 이는
진단 source compile/link 오류가 아니라 해당 host runner의 Python/SCons 경로 문제로 기록한다.

## 수동 확인

1. 보드 전면/후면 label, panel revision과 camera module label을 사진으로 남긴다.
2. 사용자가 새 v1.x build 산출물을 복구 가능한 방식으로 직접 올린다. revision 불일치가
   나타나면 중단하고 force하지 않는다.
3. 115200 baud serial log에서 `sensor=OV02C10`, driver/card/bus와 format을 기록한다.
4. frame 1~30 각각의 index/bytes/flags와 마지막
   `PASS captured=30 returned=30 outstanding=0`을 확인한다.
5. C1 preview spike에서 방향, 좌우 반전, 색상, tearing을 별도로 확인한다. RAW10 probe만으로
   색상을 판단하지 않는다.

30 frame 전에 timeout, DQBUF/QBUF 실패 또는 sensor probe 실패가 한 번이라도 있으면 C0는
실패다. 다른 sensor를 자동 fallback으로 선택하지 말고 module label과 배선을 먼저 대조한다.
