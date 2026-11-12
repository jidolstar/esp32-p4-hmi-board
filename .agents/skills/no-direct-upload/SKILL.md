---
name: no-direct-upload
description: Apply the repository's compile-only workflow when building HMI firmware, preparing device verification, or handling flash and USB asset-transfer commands.
---

# 장치 작업과 로컬 검증

저장소 루트에서 영향받은 프로필을 선택해 컴파일한다.

```powershell
python tools/project.py build --host hmi --profile main
python tools/project.py build --host hmi --profile sample-suite
```

- 자동 작업에서 `pio run -t upload`, `esptool write_flash`, 실제 USB CDC 자산 전송 등 연결된 장치에 쓰는 명령을 실행하지 않는다. 컴파일과 로컬 이미지 생성은 가능하다.
- SD 자산은 `python tools/project.py bundle-assets --profile sample-suite`로 로컬에서 묶을 수 있다. 전송 사전 검증은 `python tools/project.py push-assets --profile sample-suite --dry-run`으로 한다. `--port COMx`로 실행하는 실제 전송은 사용자가 수행한다.
- 실기기 확인이 필요하면 컴파일 결과와 확인 대상 프로필을 밝히고 [사용자 업로드 절차](../../../README.md#6-사용자가-수행하는-hmi-upload) 및 해당 실기기 확인 항목을 안내한다.
- 자동 작업에서 업로드를 잘못 시작했다면 자신이 시작한 업로드 프로세스만 중지하고 진행 상황을 보고한다.
