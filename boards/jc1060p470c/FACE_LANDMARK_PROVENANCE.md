# JC1060P470C-I-W-Y face landmark provenance

상태: C0.5 fixture/build/runtime 자원과 coordinate transform gate 통과, camera frame 검증 진행 중.

이 문서는 S08의 선택 기능인 얼굴 5점 검출을 포함할지 판단하는 근거다. 이 기능은 얼굴의
존재와 눈·코·입꼬리 위치만 다루며 개인 식별, 얼굴 인식, 속성·감정 분석은 범위 밖이다.

## 고정한 dependency

| 항목 | 고정 값 | 근거 |
| --- | --- | --- |
| framework | ESP-IDF 5.5.4 | camera 진단 및 제조사 demo와 동일 |
| model component | `espressif/human_face_detect 0.2.0` | 제조사 demo와 공식 registry example |
| inference runtime | `espressif/esp-dl 3.1.0` | model component의 exact dependency |
| license | MIT | 두 component manifest와 model component LICENSE |
| model | two-stage MSR S8 v1 + MNP S8 v1 | P4 지원 공식 model |
| model storage | app flash rodata | 포함 시 app slot 비용을 직접 측정 |

공식 ESP-DL master는 이후 3.3.x까지 진행됐지만, C0.5에서는 제조사 ESP-IDF 5.5.4 demo가
실제로 고정한 3.1.0을 사용한다. runtime과 model을 동시에 바꾸는 변수를 추가하지 않는다.

## model과 fixture 무결성

| 파일 | 크기 | SHA-256 |
| --- | ---: | --- |
| P4 MSR model | 61,248 B | `2bbaaf17f39ce9ee547abf5ffdc6d8fcd1844009a0d26878d7bfb33451f16211` |
| P4 MNP model | 129,968 B | `5b7fedcff59dbe69d1935d6d206bf371fc18b5dd9176320ef69c9f05ee83af55` |
| official 320×240 fixture JPEG | 39,381 B | `65d23c18bd5d75370d3eff48130ac441b99f9ffd75bbfd6eff4c9ae0d8b4d68a` |

fixture는 ESP Component Registry의 ESP-DL 3.1.0 `human_face_detect` example에서 가져왔다.
제조사 demo에도 같은 이름과 크기의 P4 model 두 개가 포함돼 있다.

## 출력 계약

`dl::detect::result_t`는 face box와 10개 정수 keypoint 값을 반환한다. ESP-DL 3.1.0 example과
wrapper가 사용하는 순서는 다음과 같다.

1. 왼눈 x/y
2. 왼쪽 입꼬리 x/y
3. 코 x/y
4. 오른눈 x/y
5. 오른쪽 입꼬리 x/y

즉, 배열 순서를 `두 눈 → 코 → 두 입꼬리`로 가정하면 안 된다. common result로 옮길 때는
이름 있는 field로 변환하고 raw 배열 순서를 UI까지 노출하지 않는다.

## build 측정

`diagnostics/jc1060p470c_face_probe/`의 동일 config에서 detector 실행 참조만 off/on으로
바꿔 build했다.

| 환경 | app image | 제품 OTA slot 6,553,600 B 사용률 |
| --- | ---: | ---: |
| feature off | 207,632 B | 3.17% |
| feature on | 1,306,112 B | 19.93% |
| detector/model 증분 | 1,098,480 B | 16.76%p |

feature-on image는 진단의 1,536,000 B factory partition에서도 link됐고 229,888 B가 남았다.
제품의 6,553,600 B OTA slot 기준 70% gate는 통과한다. bootloader와 app 모두 ESP32-P4
v1.0~v1.99 header, 유효한 checksum/hash를 확인했다.

## 2026-11-12 실기기 fixture 결과

사용자가 직접 올린 feature-on firmware에서 공식 320×240 fixture의 face 1개와 5-point
기하 관계가 자동 판정을 통과했다.

| 항목 | 측정 |
| --- | ---: |
| score | 0.731059 |
| box | `[100,66,191,188]` |
| 왼눈 / 왼쪽 입꼬리 | `[116,113]` / `[120,159]` |
| 코 | `[131,144]` |
| 오른눈 / 오른쪽 입꼬리 | `[155,113]` / `[149,159]` |
| 총 추론 시간 | 35,318 us |
| 최저 main task stack 여유 | 1,608 B / 3,584 B (44.9%) |

memory snapshot은 startup→model load에서 internal heap 33,244 B, PSRAM 623,688 B가
사용됐음을 보였다. inference 완료 시 largest internal block은 253,952 B로 유지됐고,
detector와 fixture image를 해제한 뒤 PSRAM은 startup의 33,551,716 B로 완전히 복구됐다.
allocation failure, watchdog, reset은 없었고 `C0_5_FACE_FIXTURE_PASS`로 종료됐다.

## Camera+ISP+detector 결합 build

`diagnostics/jc1060p470c_camera_face_probe/`는 C0와 같은 OV02C10 pin·RAW10 mode·driver
checksum, `esp_video 2.1.0` hardware ISP와 C0.5의 detector/model을 한 image에 결합한다.

| 산출물 | 크기 | 판정 |
| --- | ---: | --- |
| app image | 1,485,056 B | 진단 factory slot 96.68%, 제품 OTA slot 22.66% |
| bootloader | 22,912 B | v1.0~v1.99 header, checksum/hash 유효 |

C++ compile/link, app/bootloader image 생성이 platform package source patch 없이 통과했다.
현재 PC의 PlatformIO Core 6.2 `tool-scons` 중복 설치 문제 때문에 표준 wrapper 대신 생성된
CMake/Ninja graph와 동일 toolchain으로 build-only 검증했다. 장치 upload는 하지 않았다.

160×120 detector 좌표의 rotation, horizontal mirror, letterbox와 edge clamp native test는
g++ 16.1.0에서 build·실행됐고 `C0_5_COORDINATE_TRANSFORM_PASS`로 종료됐다.

## 남은 gate

- 실제 camera RGB frame 한 장에서 눈·코·입꼬리 위치 확인
- feature off에서 detector allocation/worker가 없음을 최종 제품 composition에서 확인

위 항목이 끝날 때까지 포함/제외 결정은 `pending`이다.
