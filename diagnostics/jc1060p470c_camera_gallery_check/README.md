# C2 카메라·SD 실기기 확인

`hmi-sample-suite-camera-check`는 실제 Board의 OV02C10, CSI/ISP, JPEG worker와 S05 SD/USB MSC를
연결한다. 테스트 화면에서 LCD 미리보기, 사진 저장과 다시 열기, PC에서 파일 읽기를 확인한다.

## 로컬 빌드

저장소 루트에서 실행한다.

```powershell
python tools/project.py prepare --host hmi --profile sample-suite
$env:PLATFORMIO_BUILD_DIR = Join-Path $PWD '.pio/build/hmi/camera-check'
pio run -c .generated/hmi/sample-suite/platformio.ini -e hmi-sample-suite-camera-check
```

## 사용자 업로드

시리얼 모니터를 닫은 뒤 보드 포트에 맞춰 `COM3`을 바꾼다. 이 명령은 저장소 루트에서 사용자가 직접 실행한다.

```powershell
$env:PLATFORMIO_BUILD_DIR = Join-Path $PWD '.pio/build/hmi/camera-check'
& 'C:\pio\penv\Scripts\pio.exe' run -c .generated/hmi/sample-suite/platformio.ini `
  -e hmi-sample-suite-camera-check -t upload --upload-port COM3
Remove-Item Env:PLATFORMIO_BUILD_DIR
```

115200 baud 로그는 다음 명령으로 열고 보드를 리셋하면 확인할 수 있다.

```powershell
& 'C:\pio\penv\Scripts\pio.exe' device monitor --port COM3 --baud 115200
```

## 확인할 동작

1. SD 카드를 넣고 부팅한다. LCD에 `Preview | SD HMI`가 표시되고 움직이는 영상의 방향, 색상,
   비율이 자연스러운지 확인한다. 로그에는 `[C2] RGB565 1288x728 stride=2576 bytes=1875328`이
   한 번 찍힌다.
2. **Capture**를 한 번 누른다. `saved` 값과 사진 수가 증가하고 로그에 `[C2] JPEG saved IMG_….jpg`가
   한 줄 출력된다. **View latest**로 사진을 열면 LCD에 촬영 당시 이미지가 나타나고 로그에
   `[C2] JPEG viewed id=…`가 찍힌다. **Preview**로 돌아갈 수 있다.
3. `COM3` 로그용 USB-C 케이블은 그대로 두고, 보드의 다른 USB-C 포트(회로도 `USB2`, High-Speed OTG)를
   데이터 케이블로 PC에 연결한다. `COM3`가 표시되는 포트는 회로도 `USB3`의 Serial/JTAG다.
   **USB MSC**를 누르면 미리보기가 멈추고 상태가 `SD PC`로 바뀐다. 이 상태는 보드의 SD 소유권 전환을
   뜻한다. Windows에서 SD 드라이브가 나타나는지 확인한 뒤
   `hmi/sample-suite/camera-gallery/IMG_….jpg`를 열고 방향·색상과 파일이 깨지지 않았는지 확인한다.
   PC에서 드라이브를 안전하게 제거하고, 필요하면 **Return to HMI**를 누른 뒤 **Preview**를 눌러
   복귀한다. 복귀 후 다시 촬영하고 **View latest**를 확인한다.
4. SD가 없는 상태로 부팅하면 미리보기는 동작하고 촬영은 시작되지 않는다. 여유 공간이 부족한
   시험 카드로 촬영하면 저장 오류가 표시되고 기존 JPEG를 계속 열 수 있다. SD를 제거하거나
   교체할 때는 시험용 카드를 사용하고 진행 중인 작업의 결과와 USB MSC 복귀 상태를 확인한다.

결과를 공유할 때는 LCD의 상태 문구와 `[C2]`, `[camera]` 로그를 보내면 된다. `camera 2`는
센서 설정/탐색 실패, `camera 3`은 수신기 초기화 실패, `camera 4`는 촬영 프레임 timeout이다.
`storage 4`는 공간 부족, `storage 5`는 저장 실패, `storage 6`은 읽기/디코드 실패다.
`SD PC`인데 드라이브가 없으면 Windows 장치 관리자에서 USB 장치가 새로 나타나는지 확인한다.
화이트밸런스는 미리보기를 시작할 때 한 번 측정한다. 색상 확인 시 흰 종이를 프레임 중앙에 두고
리셋하거나 사진 보기에서 **Preview**로 돌아간다. `[camera] white balance` 로그와 함께 LCD
미리보기와 새로 찍어 Windows에서 연 JPEG의 색감을 비교한다.
