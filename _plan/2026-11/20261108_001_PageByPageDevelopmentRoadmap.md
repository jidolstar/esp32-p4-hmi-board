# 범용 HMI 시작 프로젝트: 페이지 단위 개발 로드맵

작성: 2026-11-08. 갱신: 2026-11-11. 상태: `in_progress` (S01~S07 `complete`,
S08~S09 `pending`).

## 1. 재편 결론

향후 구현 순서를 다음과 같이 고정한다.

`ui-basics` → `text-input` → `display-audio` → `wifi` → `sd-storage` →
`wav-sounds` → `wifi-ap-web` → `camera-gallery` → `interfaces-info`

- 밝기와 음량은 서로 다른 page로 나누지 않고 S03 한 화면에서 설정한다.
- 독립 `assets` page는 제거한다. 자산 manifest·bundle·검사는 폰트와 WAV가 사용하는
  내부 배포 기반으로만 유지한다.
- SD page의 중심은 일반 CRUD 예제가 아니라 HMI와 PC 사이의 안전한 USB Mass Storage
  전환이다.
- WAV page는 시험 도구가 아니라 등록된 여러 실사용 가능 음원을 선택해 재생하는 단순
  화면으로 축소한다.
- 카메라와 AP 웹 제어는 202609에 완성된 기능이 아니므로 새 구현으로 분류한다.
- 2026-11-10 사용자 결정에 따라 카메라 제어 경험과 보드 하드웨어 근거가 부족한
  `camera-gallery`보다 기존 Wi-Fi 기반을 확장하는 `wifi-ap-web`을 먼저 진행한다.
- RS485, I/O, USB command는 실제 통신 기능을 구현하지 않는다. 202609의 공개 경계를
  참고한 최소 기반과 설명만 S09 한 page에 모은다.

## 2. 페이지 상태와 상세 계획

| 순서 | Page ID | 상태 | 상세 계획 | 핵심 결과 |
| --- | --- | --- | --- | --- |
| S01 | `ui-basics` | `complete` | `_plan/2026-11/20261107_003_ProfileRegistryAndSampleSuiteDashboard.md` | Dashboard와 기본 UI |
| S02 | `text-input` | `complete` | `_plan/2026-11/20261108_010_TextInputUxCompletion.md` | 한·영 입력과 i18n |
| S03 | `display-audio` | `complete` | `_plan/2026-11/20261108_013_DisplayAudioPage.md` | 밝기·음량·preview·저장 복원 수용 완료 |
| S04 | `wifi` | `complete` | `_plan/2026-11/20261108_014_WifiSettingsPage.md` | STA 검색·연결·복구와 목록→입력 UI 완료, 2026-11-09 사용자 완료 승인 |
| S05 | `sd-storage` | `complete` | `_plan/2026-11/20261108_015_SdUsbMscPage.md` | SD 상태, 전역 앱 잠금과 PC USB MSC 접근 |
| S06 | `wav-sounds` | `complete` | `_plan/2026-11/20261108_016_RegisteredWavPage.md` | 목록·progress·pause/resume 실기기 수용과 사용자 승인 완료 |
| S07 | `wifi-ap-web` | `complete` | `_plan/2026-11/20261108_018_WifiApWebControlPage.md` | AP+STA·두 QR·웹/HMI 4스위치 동기화와 사용자 승인 완료 |
| S08 | `camera-gallery` | `pending` | `_plan/2026-11/20261108_017_CameraGalleryPage.md` | 촬영·SD 저장·보기·삭제 |
| S09 | `interfaces-info` | `pending` | `_plan/2026-11/20261108_019_InterfaceOverviewPage.md` | RS485·I/O·USB command 기반 안내 |

S04는 공통 STA 상태·Windows fixture·HMI adapter와 Wi-Fi 설정 UI, 지연 scan, 목록→선택→입력,
검색 spinner, 터치 보정, 인증 실패 재입력과 연결 AP 강조까지 구현했다. 2026-11-09 사용자가
Wi-Fi를 완료 처리하고 다음 개발 시작을 승인했으므로 S04를 `complete`로 닫았다.

S05는 M0~M5에서 202609의 Storage facade·exclusive lease·raw-sector MSC driver를 선별
이식하고 SD page, sample-suite 전체 잠금, 명시적 `HMI로 돌아가기`, PC 안전 제거, USB
cable unplug 자동 복구와 remount retry를 구현했다. Windows/HMI compile·test 근거와
2026-11-10 사용자의 M6 완료 승인을 기록해 S05를 `complete`로 닫았다.

S06은 202609에서 가져온 48 kHz mono PCM 16-bit 다섯 파일의 catalog·hash·bundle,
단일 재생 owner, 진행률·경과/전체/남은 시간,
재생·일시정지·이어 재생·정지 control과 지원 WAV 형식 안내를 1024×600의 목록+현재 재생
2열 UI로 구현했다. Windows/HMI 자동 build·test·memory gate와 실기기 touch·재생·pause/resume
재수용을 통과했고, 2026-11-10 사용자의 최종 완료 승인을 기록해 S06을 `complete`로 닫았다.

S07은 기존 STA를 유지하는 AP+STA, Wi-Fi/URL QR, bounded HTTP/WebSocket과 HMI↔mobile
4-switch canonical state를 구현했다. Windows profile·browser·isolation과 HMI main/sample-suite
build, A0/A0.5 실기기 data path 근거를 통과했고 2026-11-11 사용자 완료 승인을 기록해
`complete`로 닫았다. 출시 전 장시간 휴대폰 회귀는 전역 G3와 README 수동 체크리스트에서
별도로 수행한다.

## 3. 202609 재사용 판정

| 기능 | 판정 | 가져올 핵심 | 그대로 가져오지 않는 것 |
| --- | --- | --- | --- |
| 밝기·음량 | 이식 | level model, NVS 저장, apply/preview port | 기존 제품 화면과 profile 조립 |
| Wi-Fi STA | 이식 | scan/connect 상태, credential store, ESP-Hosted adapter | 제품 profile과 화면 결합 |
| SD/PC 접근 | 이식 | storage facade, USB MSC 전환, exclusive lease | audio-lab 전용 흐름 |
| WAV | 축소 이식 | audio facade, WAV reader, asset catalog | 진단용 fade/format lab UI |
| 카메라 | 신규 | 공개 camera port만 새로 정의 | 존재하지 않는 202609 구현을 가정하지 않음 |
| Wi-Fi AP 웹 | 신규 확장 | 기존 STA/Hosted 초기화 지식 | STA adapter를 AP 구현으로 간주하지 않음 |
| RS485·I/O·USB command | 계약 참고 | 공개 facade와 capability 설명 | worker, protocol, 실제 장치 동작 |

참조 원본은 `../dev_202609_hmi/`이며 복사 전에 제품명·profile·화면·보드 내부 의존을
제거한다. 현재 저장소의 page와 module은 원본 파일을 직접 include하지 않는다.

## 4. 공통 실행 규칙

1. 동시에 하나의 page 계획만 `in_progress`로 둔다.
2. 각 단계는 상세 계획의 자동 검사와 수동 확인을 통과해야 다음 단계로 진행한다.
3. Windows와 HMI가 공유할 상태·정책은 공통 module에 두고 실제 장치 접근만 adapter로 둔다.
4. UI/page는 Board internal, ESP-IDF handle, LVGL port 내부 객체를 직접 사용하지 않는다.
5. Dashboard는 descriptor/factory만 소비하며 page ID별 include나 분기를 추가하지 않는다.
6. 목록·문자열·frame·이미지 metadata·I/O buffer는 모두 상한을 둔다.
7. SD 소유권을 HMI와 USB MSC가 동시에 갖지 않도록 한다.
8. 카메라 frame과 WAV 전체 파일을 task stack이나 단일 RAM buffer에 올리지 않는다.
9. page 이탈 시 callback, timer, worker, stream, file handle과 장치 lease를 정리한다.
10. Codex는 firmware upload·flash를 실행하지 않는다. HMI 실기기 검증은 사용자가 수행한다.

## 5. 단계 전환 규칙

- `pending`: 아직 구현을 시작하지 않음.
- `in_progress`: 현재 page의 현재 단계만 작업 중.
- `blocked`: build는 끝났지만 사용자 실기기 확인 또는 필수 하드웨어 판정이 남음.
- `complete`: 자동 검사, Windows 확인, HMI build, 사용자 실기기 확인을 모두 통과함.
- S07은 현재 ESP-Hosted 버전에서 AP data path가 확인되지 않으면 A0에서 `blocked`로 둔다.
- S08은 camera sensor·pin·clock·driver가 확인되지 않으면 C0에서 `blocked`로 둔다.
- S09는 안내 page이므로 실제 RS485/I/O/USB command 동작을 완료 조건으로 요구하지 않는다.

## 6. 전체 완료 정의

- 각 page는 `samples/suite/pages/<page-id>/`에서 화면·상태·fixture를 소유한다.
- page 등록만으로 Dashboard에 나타나며 기존 page 내부 수정이 없다.
- Windows mock 또는 명시적 unsupported adapter로 모든 page가 열리고 종료된다.
- HMI 대상은 upload 없이 build되고 RAM/flash 및 큰 buffer/stack 크기가 기록된다.
- 지원한다고 표시한 실제 기능만 실기기에서 확인한다.
- 설명 전용 기능은 화면에 `기반 제공`, `구현 안 됨`을 명확히 구분한다.
- `python tools/project.py check --scope isolation`이 통과한다.
- 관련 사용법과 제한이 `README.md`에 반영된다.

## Automated Verification Checklist

- [x] 활성 page 계획이 하나만 `in_progress` 또는 `blocked`다.
- [x] 활성 단계의 단위 test와 Windows profile check가 통과한다.
- [x] Windows와 HMI `sample-suite` build가 통과한다.
- [x] `python tools/project.py check --scope isolation`이 통과한다.
- [x] Dashboard에 page ID별 include·분기가 없다.
- [x] UI/page에 Board internal·ESP-IDF·SDL 직접 의존이 없다.
- [x] page 종료 후 남는 callback·timer·worker·handle·lease가 없다.
- [x] bounded model과 firmware RAM/flash 변화가 기록된다.

## Manual Verification Checklist

- [ ] 사용자가 현재 page의 LCD 배치와 touch 흐름을 확인한다.
- [ ] 사용자가 정상·실패·재시도·Dashboard 복귀를 확인한다.
- [ ] 사용자가 실제로 지원하는 hardware capability를 확인한다.
- [ ] page 반복 진입·이탈 후 reset·멈춤·중복 callback이 없다.
- [ ] 사용자가 현재 page 완료를 승인한 뒤에만 다음 page를 시작한다.
