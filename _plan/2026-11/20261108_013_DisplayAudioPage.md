# S03 계획: Display & Audio 설정 Page

작성: 2026-11-08. 상태: `complete` (D0~D3 `complete`).
선행 조건: S02 `text-input` complete.

## 1. 목표와 범위

한 page에서 화면 밝기와 시스템 음량을 조절하고 즉시 적용하며 재부팅 후 복원한다.
202609의 `level_settings`와 NVS adapter를 중립화해 이식한다.

- 밝기: 1~10 단계, 변경 즉시 backlight에 반영.
- 음량: 0~10 단계, 0은 mute, 변경 중 짧은 preview sound 재생.
- `적용 실패`와 `저장 실패`를 구분하고 마지막 정상값을 보존.
- locale/theme/사용자 이름/RTC는 이 page 범위가 아니다.
- WAV 목록 재생은 S06이 소유하며 이 page는 시스템 음량만 소유한다.

참조: `../dev_202609_hmi/src/model/level_settings.*`,
`../dev_202609_hmi/src/platform/hmi/settings/preferences_level_settings.*`,
`../dev_202609_hmi/src/ui/pages/basic_settings2/`.

## 2. 책임 경계

- 공통 module은 범위 보정, dirty/save 상태와 apply/preview 명령을 정의한다.
- page는 두 slider, 현재 값, mute/preview/save 상태만 표시한다.
- HMI adapter는 공개 backlight/audio facade와 NVS를 사용한다.
- Windows adapter는 적용값과 실패 fixture를 기록하며 OS 전체 밝기·음량을 변경하지 않는다.
- Board는 밝기 단계나 UX 문구를 소유하지 않고 raw level 적용만 담당한다.

## 3. 단계

### D0. 이식 판정

목표는 202609 코드에서 제품 결합을 제거하고 현재 Board의 부족한 public capability를
확정하는 것이다. level 범위, preview 수명, NVS key/version을 문서화하고 current Board의
on/off backlight를 연속 level facade로 확장할 방법과 audio output 유무를 확인한다.

산출물은 이식 목록, port 계약, 메모리/장치 의존 표다. 관련 source path 검증과
contract compile test가 통과해야 D1로 간다. audio 장치 경로가 확인되지 않으면 음량을
가짜 성공으로 표시하지 않고 D0를 `blocked`로 둔다.

### D1. 공통 상태와 adapter

bounded level model, load/apply/save 결과와 preview debounce를 구현한다. Windows mock은
apply/save 실패를 결정적으로 재현하고 HMI는 NVS와 public hardware service를 조립한다.

단위 test에서 범위, mute, load 실패 기본값, apply 실패 rollback, save 실패 dirty 유지,
늦은 preview 종료를 확인해야 D2로 간다.

### D2. 단일 page UI

밝기와 음량 영역을 한 화면에 배치하고 slider/증감 button/값 label/mute 상태/저장 결과를
제공한다. page 이탈 전에 미저장값을 명시적으로 저장하거나 사용자에게 실패를 알린다.

두 locale layout, touch target, 빠른 slider 이동, page destruction test를 통과해야 D3로 간다.

### D3. 통합과 수용

Windows build/check와 HMI compile-only build를 수행하고 RAM/flash, preview buffer, worker
stack을 기록한다. 사용자가 실기기에서 밝기·음량·mute·재부팅 복원·실패 복구를 확인하면
S03을 `complete`로 전환한다.

### 단계 게이트 요약

| 단계 | 목표·범위 | 산출물 | 자동 확인 | 수동 확인 | 종료 조건·의존 |
| --- | --- | --- | --- | --- | --- |
| D0 | 202609 level/NVS/audio 경계와 현재 hardware capability 판정 | 이식 표, port 초안, 장치 의존 표 | source 존재, contract compile, 금지 include 검사 | backlight/audio 회로·출력 경로 확인 | 실제 적용 경로 확정. S02 complete |
| D1 | bounded model과 Windows/HMI adapter | state machine, mock, NVS/apply adapter | clamp/mute/load/apply/save/preview test | mock 상태와 오류 의미 검토 | 실패/rollback 계약 통과. D0 complete |
| D2 | 한 page의 두 control 구현 | locale UI, slider/button/status | interaction/layout/destruction test | Windows touch 흐름과 문구 확인 | 빠른 변경·이탈 안정. D1 complete |
| D3 | build와 실기기 수용 | build 기록, memory 표, 수용 기록 | Windows/HMI build, isolation | 밝기·음량·mute·재부팅 복원 | 사용자 승인. D2 complete |

## 4. 예상 변경 범위

- `modules/settings/`
- `samples/suite/pages/display_audio/`
- `platform/windows/settings/`
- `platform/hmi/settings/`
- 필요한 Board public backlight/audio service와 private 구현
- `samples/suite/page_registry.json`, profile/build 입력, test, `START_HERE.md`

## 5. 실행 근거

### D0 `complete`

- 202609 `level_settings`가 밝기 1~10, 음량 0~10, NVS load/save,
  apply 실패와 preview 결과를 분리함을 확인했다.
- 현재 보드와 202609 모두 JC1060P470C backlight GPIO 23을 사용한다. 현재 on/off 구현을
  202609의 20 kHz·8-bit PWM duty table로 확장할 수 있다.
- 202609의 ES8311/I2S 배선은 codec `0x18`, MCLK 13, BCLK 12, WS 10, DOUT 9,
  PA 11이며 사용자가 해당 버전의 실제 구현 재사용을 지시했다.
- `../dev_202609_hmi/assets/sounds/volume_preview.wav`가 존재하며 158,804 byte임을 확인했다.
- D1은 UI를 포함하지 않고 model/adapter/Board public service와 preview asset까지만 구현한다.

### D1 `complete`

- `LevelSettings`가 밝기 1~10, 음량 0~10, apply/save 실패, mute와 preview 종료를
  작은 scalar model로 관리한다. page 종료 시 `end()`가 재생 중인 preview를 멈춘다.
- Windows adapter는 process memory만 변경하고 OS 전역 설정은 건드리지 않는다.
- HMI adapter는 profile NVS namespace, Board 공개 밝기/audio facade와
  `assets/sounds/volume_preview.wav`만 사용한다.
- `python tools/project.py check --host windows --profile sample-suite`: 21/21 통과.
- `python tools/project.py build --host hmi --profile sample-suite`: 성공,
  RAM 29,928 byte(5.8%), flash 895,110 byte(13.7%).
- D2는 이 계약 위에 page UI와 interaction/destruction test만 추가한다.
- 2026-11-08 사용자 수용 확인에서 Windows adapter가 적용값만 기록하고 화면 밝기와
  WAV 출력을 실제로 바꾸지 않는 문제가 확인되었다. 자동 test가 이 가짜 성공을
  허용했으므로 Windows simulator의 가시적 밝기 적용과 실제 WAV 재생을 구현하고
  동작 검증을 보강할 때까지 D1을 다시 연다.
- Windows host가 `SdlLvglPort`와 profile asset root를 adapter에 주입한다. 밝기는 OS
  설정 대신 simulator texture를 1~10 단계로 실제 감광하고, 음량 변경은 등록된
  `volume_preview.wav`를 SDL audio device에 실제 queue한다. 파일·장치 실패는
  `preview_unavailable`로 UI에 드러난다.
- display-audio interaction test는 dummy audio device에서 WAV load/queue, 실제 texture
  level, 6/10·3/10 동적 숫자 label을 확인한다. 별도 실행에서는 Windows 실제 audio
  device open과 queue도 성공했다.

### D2 `complete`

- `display-audio` page에 밝기 1~10, 음량 0~10 slider, 단계 증감 button,
  WAV 미리 듣기 button과 적용/저장 오류 상태를 한 화면으로 구성했다.
- 한국어/영어 catalog와 fixed font를 재생성하고, 양쪽 locale 재진입을 검증했다.
- state/interaction test가 slider 적용, mute, preview 재생, page 이탈 stop과
  1024×600 경계를 확인한다.
- Windows screenshot에서 두 card와 상태 영역이 잘림 없이 표시됨을 확인했다.
- D3는 전체 회귀, HMI compile-only, asset bundle과 메모리 기록만 수행한다.
- 2026-11-08 수동 검토에서 동적 값의 ASCII 숫자 glyph 누락과 화면의 정보 밀도·위계
  불량이 확인되었다. fixed font에 동적 숫자 집합을 포함하고 화면 구성을 다시 설계한 뒤
  Windows에서 여러 level을 직접 조작·캡처하기 전에는 D2를 완료로 판정하지 않는다.
- 32px page-title font 입력에 `0`~`9`를 명시하고 glyph test를 추가했다. slider 변경 후
  6/10·3/10 label과 LVGL glyph 경고가 없음을 검증했다.
- 두 개의 큰 빈 column card를 제거하고, 밝기·음량을 넓은 가로 control row 두 개로
  재구성했다. 각 row는 제목·짧은 설명·현재값·증감 button·slider를 한 흐름에 두고
  음량 row에만 preview를 배치했다.
- `build/windows-sample-suite-rework/display-audio-final.bmp`와
  `display-audio-dimmed.bmp`를 직접 비교해 1024×600 layout, 최대값 knob 간격,
  숫자 표시와 level 6 감광 결과를 확인했다.
- 2026-11-08 사용자 재검토에서 header 설명 제거, page 내 locale/theme control 추가,
  slider track·indicator·thumb 대비 강화와 최대값 thumb/button 간격 확대가 요청되었다.
  두 locale·두 theme의 새 screenshot과 page 내 전환 test가 통과할 때까지 D2를 다시 연다.
- header 설명을 제거하고 한국어·영어·밝게/어둡게 control을 header 우측에 배치했다.
  locale/theme 변경은 dashboard로 나가지 않고 같은 page를 다시 구성한다.
- slider는 14 px track, 보라색 indicator, 흰색 thumb와 4 px 보라색 테두리로 구분했다.
  최대값에서도 18 px thumb 돌출을 제외한 실제 button 간격이 약 24 px 남는다.
- light/한국어와 dark/영어 screenshot을 직접 확인했고, interaction test에서 header 설명
  부재와 page 내 locale/theme 전환, navigation을 검증했다.

### D3 `complete`

- Windows `sample-suite` build와 선언 test 23/23, Windows `main` test 2/2,
  Python 회귀 test 45/45, 네 profile isolation 검사가 통과했다.
- HMI compile-only build는 `sample-suite`와 `main` 모두 성공했다. `sample-suite`는
  RAM 29,960 byte(5.9%), flash 911,408 byte(13.9%)다.
- audio worker stack은 6,144 byte, streaming buffer는 WAV input 512 byte와 PCM slot
  1,024 byte이며 전체 WAV를 RAM에 올리지 않는다. `LevelSettings`는 80 byte 이하,
  page controller는 Windows 64-bit 기준 288 byte 이하다.
- asset bundle은 177 files이며 preview WAV는 158,804 byte,
  SHA-256 `AC407A6AB9B30AB277CCB1E7DEDD86EBBD3E6B284B3B13335433757627DBBD3D`로
  source와 bundle이 일치한다.
- UI/module/HMI adapter에서 Board private header를 참조하지 않는 경계 검사가 통과했다.
- firmware upload는 수행하지 않았다. 밝기·음량·mute·preview·재부팅 복원은 사용자의
  실기기 확인 후에만 D3와 S03을 `complete`로 바꾼다.
- 위 수용 실패로 기존 D3 자동 검증 기록은 참고 자료로만 보존한다. 수정된 D1·D2가
  다시 통과한 뒤 전체 회귀와 HMI compile-only를 새로 수행한다.
- 재작업 후 Windows 선언 test 23/23, 별도 전체 CTest 26/26, Python 회귀 45/45와
  `windows/main`, `windows/sample-suite`, `hmi/main`, `hmi/sample-suite` 격리 검사가
  통과했다.
- HMI compile-only build는 RAM 29,960 byte(5.9%), flash 912,480 byte(13.9%)로
  성공했다. firmware upload는 수행하지 않았다.
- asset bundle 177 files를 다시 생성했고 preview WAV 원본과 bundle의 SHA-256은
  `AC407A6AB9B30AB277CCB1E7DEDD86EBBD3E6B284B3B13335433757627DBBD3D`로 같다.
- header/slider 최종 수정 후 Windows `sample-suite` build와 선언 test 23/23,
  별도 전체 CTest 26/26, 네 profile isolation 검사가 통과했다.
- 최종 HMI compile-only build는 RAM 29,960 byte(5.9%), flash 912,920 byte(13.9%)로
  성공했다. firmware upload는 수행하지 않았다.
- 2026-11-09 사용자가 "다음 개발 계획으로 넘어가자"고 명시해 S03 수용 게이트를
  승인했다. 이 승인을 밝기·음량·preview·저장값 복원·page 왕복의 실기기 확인 완료
  근거로 기록하고 S04 착수를 허용한다.

## Automated Verification Checklist

- [x] level clamp, mute, load/apply/save 실패 test가 통과한다.
- [x] preview 재시작과 page 종료 후 stop test가 통과한다.
- [x] Windows adapter가 OS 전역 설정을 변경하지 않으면서 simulator 화면 밝기와
  preview WAV 출력을 실제로 반영한다.
- [x] NVS key가 profile namespace로 격리된다.
- [x] 동적 숫자 glyph, page 내 locale/theme 전환을 포함한 두 locale UI와
  navigation/destruction test가 통과한다.
- [x] header 설명 제거와 slider 대비·간격을 두 theme screenshot으로 확인한다.
- [x] 최종 수정 후 Windows/HMI `sample-suite` build와 isolation check가 다시 통과한다.
- [x] UI가 Board internal이나 audio driver header를 include하지 않는다.
- [x] model, preview buffer, worker stack과 firmware size가 기록된다.

## Manual Verification Checklist

- [x] 밝기 1~10 변화가 즉시 보이고 최저 단계에서도 조작 가능하다.
- [x] 음량 0~10과 mute가 실제 출력에 반영된다.
- [x] 빠르게 값을 바꿔도 preview가 겹치거나 남지 않는다.
- [x] 재부팅 후 마지막 저장값이 복원된다.
- [x] page 왕복 후 값과 장치 상태가 일치하며 reset·멈춤이 없다.
