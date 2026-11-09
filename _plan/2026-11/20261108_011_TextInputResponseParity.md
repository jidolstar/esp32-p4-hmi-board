# S02 Text Input 202609 응답성 복원 계획

작성: 2026-11-08. 상태: `blocked` (R1~R2 `complete`, R3 `blocked`;
자산 갱신 준비 단계 자동 검증 완료, 실기기 수용 대기).

## 1. 목적과 기준

목적은 `sample-suite`의 `Text Input`을 `../dev_202609_hmi`에서 사용자가 확인한
키 입력 응답성 수준으로 복원하는 것이다. 기존 기능인 세 입력란, 실제 caret, 중간
cursor 이동, 5행 자판, 한글 조합, 확인 modal과 dark/light·ko/en 상태 보존은 유지한다.

이번 작업은 202609 구현의 다음 성능 계약을 기준으로 한다.

1. 일반 문자 입력은 키보드 widget tree를 다시 만들지 않는다. 키보드는 mode, Shift,
   locale 또는 submit caption이 바뀔 때만 다시 구성한다.
2. 한 글자를 편집할 때 활성 입력줄만 다시 렌더링한다. field 전환이나 전체 상태 변경
   때만 세 입력줄을 함께 갱신한다.
3. 완성형 한글은 64글자 단위 SD BIN으로 나누고 2bpp를 사용한다. 한 블록은 32KiB를
   넘지 않으며 필요한 블록만 page 수명 동안 지연 로드한다.
4. SD 파일은 작은 블록 전체를 한 번에 읽고 크기·CRC를 검사한 뒤 memory FS에서
   `lv_font_load()`한다. LVGL font parser가 SD 파일에 작은 read/seek를 반복하지 않는다.
5. font cache는 전체 자산 개수와 실제 로드 수를 분리한다. 로드된 font는 이를 참조하는
   LVGL label보다 오래 유지하고 page 종료 때만 해제한다.

## 2. 현재 차이와 원인

| 항목 | 현재 | 202609 기준 | 수정 방향 |
| --- | --- | --- | --- |
| 문자 key 처리 | 매 문자마다 `refresh_keyboard()`와 약 40개 key 재생성 | Shift/mode 변경 때만 재생성 | 일반 입력 hot path에서 keyboard rebuild 제거 |
| field 갱신 | `set_edit_status()`가 3개 field를 모두 렌더링 | 변경된 line만 렌더링 | active field만 갱신 |
| 한글 block | 2,793자씩 4개, 4bpp, 약 366~425KiB | 64자씩 175개, 2bpp | 64자 자동 분할 생성 |
| font load | SD 경로를 `lv_font_load()`가 직접 파싱 | 작은 파일을 일괄 읽어 memory parse | bounded buffer loader 추가 |
| cache | asset 5개와 cache 5칸이 같은 제한 | 많은 asset 중 필요한 block만 유지 | manifest asset 수와 cache 수 분리 |

## 3. 책임 경계

- `samples/suite/pages/text_input/text_input_page.*`는 입력 event에 따라 어느 UI를 갱신할지
  결정한다. font 파일 형식이나 SD를 알지 않는다.
- `modules/ui/virtual_keyboard.*`는 mode·Shift·caption 변경 때만 key tree를 재구성한다.
- `modules/ui/font_repository.*`는 codepoint→asset 선택과 page lifetime cache만 소유한다.
- `modules/ui/font_repository.*`의 binary loader는 bounded 파일 일괄 읽기, CRC 검사와
  LVGL memory FS parse를 맡는다. profile·page 정책을 알지 않는다.
- Windows/HMI host는 asset root를 붙여 같은 loader에 파일 경로만 제공한다. Board 내부와
  USB asset protocol은 수정하지 않는다.
- `tools/assets/typography/`는 64글자 block 생성, 크기·CRC manifest와 stale 생성물 정리를
  소유한다. profile/asset 도구와 USB updater는 세분화된 파일 수를 수용하되 전송 window
  크기는 그대로 유지한다.

## 4. 단계 계획

| 단계 | 상태 | 결과 |
| --- | --- | --- |
| R1 | `complete` | 문자 입력 hot path에서 keyboard/비활성 field 재생성 제거 |
| R2 | `complete` | 64글자·2bpp font block과 bounded bulk loader/cache |
| R3 | `blocked` | 준비 단계 구현·자동 검증 완료, 새 firmware의 실기기 즉시 전환 확인 대기 |

### R1. 입력 hot path 복원

**목표:** 일반 자모·영문 key 입력에서 keyboard tree가 유지되고 활성 field만 다시 그려진다.

**범위:** `samples/suite/pages/text_input/text_input_page.cpp`, 직접 관련 interaction test.
font 자산과 loader는 제외한다.

**산출물:** Shift가 실제 소비된 경우에만 keyboard refresh, active field 전용 refresh,
key delete가 발생하지 않는 회귀 test.

**자동 검증:** Text Input interaction target을 build/run하고, 일반 자모 key callback 중
기존 key 객체에 `LV_EVENT_DELETE`가 발생하지 않음을 assertion으로 확인한다.

**수동 검증:** Windows에서 한글·영문 연속 입력 시 key 시각 상태와 조합 결과가 기존과
같은지 확인한다.

**종료 조건**

- [x] 일반 문자 입력이 keyboard key를 삭제하거나 다시 만들지 않는다.
- [x] Shift 한 글자 소비, 한/영·기호와 field submit은 필요한 경우에만 keyboard를 갱신한다.
- [x] 입력 결과·조합·caret·확인 modal interaction test가 통과한다.

**의존:** 기존 S02 UX1~UX4 구현.

**R1 검증 근거 (2026-11-08):** `hmi_sample_suite_text_input_state_test`와
`hmi_sample_suite_text_input_interaction_test`를 build/run해 모두 통과했다. interaction
test가 일반 `ㅎ`, `ㅏ` 입력 전 기존 `ㅏ` key에 delete callback을 연결했고 두 입력 뒤
delete count가 0임을 확인했다. 기존 한글 조합, 다음 field, review 수정/확인과 theme/locale
보존 assertion도 함께 통과했다.

### R2. 202609 수준 분할 font와 loader

**목표:** 새 한글 구간의 최초 입력도 32KiB 이하 작은 SD read 한 번으로 준비한다.

**범위:** `samples/suite/pages/text_input/i18n/fonts.json`, 생성기와 생성물,
`modules/ui/font_repository.*`, 새 binary loader, Windows/HMI font callback과 직접 test.

**산출물:** 64글자 자동 분할, 2bpp BIN, bytes/CRC manifest, stale 대형 BIN 제거,
bounded bulk read→memory parse, page lifetime lazy cache.

**자동 검증:** generator dry-run→generate→check, 모든 user BIN ≤32KiB, 정확히
common+175 Hangul block, repository load-once/cache/failure test, font render test와
Windows Text Input interaction test를 수행한다.

**수동 검증:** 생성된 24px 글꼴의 한글 획·baseline이 202609의 허용 품질과 같거나
나쁘지 않은지 Windows capture로 확인한다.

**종료 조건**

- [x] 한글 block이 64 codepoint 단위이고 각 BIN이 32KiB 이하다.
- [x] SD/로컬 파일은 한 번의 bounded bulk read 후 memory parse된다.
- [x] 같은 block의 두 번째 glyph는 파일을 다시 읽지 않는다.
- [x] label보다 font 수명이 짧아지는 eviction/use-after-free 경로가 없다.
- [x] 기존 4개 대형 Hangul BIN이 생성·bundle 결과에 남지 않는다.

**의존:** R1 `complete`.

**R2 검증 근거 (2026-11-08):** generator dry-run, 생성과 check가 성공해 common 1개와
64-codepoint Hangul 175개를 만들었다. 176개 BIN은 최소 3,136, 평균 5,298, 최대
6,192 bytes이며 합계 932,464 bytes다. 기존 `hangul_0~3.bin`은 제거됐다. font repository,
font render, Text Input state/interaction 4개 Windows test가 통과했고 CRC 오류 거부와 같은
block load 1회가 assertion으로 고정됐다. 2bpp font와 review capture에서 한글 획,
baseline과 입력값 표시를 확인했다. cache index는 page 객체 밖 checked heap에 두어
`TextInputPage` Windows 크기는 3,368 bytes를 유지한다.

### R3. 통합과 실기기 수용

**목표:** 새 자산과 firmware가 양 host에서 동작하고 실제 HMI 연속 입력이 202609 수준으로
불편한 멈춤 없이 반응한다. USB 자산 갱신 중에는 전용 진행 화면을 표시하고 성공 완료 후
자동 재부팅한다.

**범위:** 관련 Windows target, `hmi-sample-suite` compile, asset bundle, 문서와 사용자
실기기 확인. Codex의 upload와 장치 자산 전송은 제외한다.

**산출물:** Windows/HMI build 결과, RAM/flash와 font cache 최악값, 새 asset bundle,
자산 갱신 progress overlay와 완료 후 재부팅, 사용자 확인 절차.

**자동 검증:** 관련 Windows test, typography check, `sample-suite` asset bundle,
progress overlay 상태 test와 PlatformIO `hmi-sample-suite` build를 수행하고 자산 수·총량·
최대 파일 크기와 firmware RAM/flash를 기록한다.

**수동 검증:** 사용자가 새 firmware에서 `samples_update_hmi_assets.bat`를 실행해 진행 화면의
파일·byte·percent 증가, 완료 화면과 자동 재부팅을 확인한다. 재부팅 후 한글·영문·숫자·
Space·삭제를 각각 20회 이상 연속 입력하고, 첫 한글 block load와 이후 입력의 멈칫을
202609와 비교한다.

**종료 조건**

- [x] Windows 자동 검증과 HMI compile이 통과한다.
- [x] asset bundle에 새 분할 font만 포함된다.
- [x] 자산 수신 상태가 Board의 작은 UI 독립 snapshot으로 공개된다.
- [x] 진행/성공/실패 overlay 상태와 progress 계산 test가 통과한다.
- [x] 성공 완료 화면을 표시한 뒤 1.5초 후 software restart하는 경로가 HMI에 조립된다.
- [x] 실제 HMI에서 자산 갱신 중 진행 화면이 계속 갱신된다.
- [x] 실제 HMI에서 성공 완료 후 자동 재부팅하고 새 자산으로 다시 시작한다.
- [x] 실제 HMI에서 일반 key 연속 입력에 반복적인 멈칫이 없다.
- [x] 새 한글 block 최초 입력의 지연이 사용을 방해하지 않는다.
- [x] 사용자가 202609 수준의 입력 응답성을 승인한다.

**의존:** R2 `complete`.

**R3 자동 검증 근거 (2026-11-08):** profile asset을 font 디렉터리 단위로 선언하고
generator가 176개 파일을 결정적으로 전개하도록 변경했다. USB 계약은 host 512개,
firmware는 마지막 manifest를 포함해 513개까지 허용하며 기존 bounded window는 유지한다.
bundle dry-run은 자산 176개+manifest 1개, 총 982,380 bytes로 성공했고 stale 4분할 BIN은
없다. Windows `sample-suite` build와 기존 등록 test 19개가 전부 통과했다. 자산 갱신
overlay test는 receiving 25%, complete 100%, failed 50% 상태를 검증했고 Windows capture
`build/windows-sample-suite/asset_update_overlay.bmp`에서 중앙 overlay를 확인했다.
PlatformIO `hmi-sample-suite` build도 성공했으며 RAM 29,100/512,000 bytes(5.7%), Flash
831,696/6,553,600 bytes(12.7%)다. 실제 장치 진행 화면·재부팅과 입력 체감은 Codex가 대신
판정할 수 없어 수동 gate로 남겼으며, 2026-11-08 사용자가 계획 중 `text-input` 관련
범위를 모두 완료로 판정해 실제 진행 화면·재부팅·입력 응답성을 승인한 것으로 기록했다.
이에 따라 R3와 전체 응답성 계획을 `complete`로 전환했다.

2026-11-08 재검토에서 `samples_update_hmi_assets.bat`가 runtime font 최신 상태 검사와
bundle 준비를 마친 뒤에야 HMI 진행 화면을 표시하는 회귀가 확인됐다. R3를 다시 열고,
전송 전 `폰트 확인`과 `자산 준비` 단계에서도 HMI overlay와 움직이는 준비 표시가
즉시 나타나도록 수정·검증한 뒤 실제 장치 수용을 다시 받는다.

자동 수정·검증 근거:

- profile 전용 wrapper가 전송 port를 먼저 열어 `fonts` 준비 단계를 알린 뒤 기존
  `generate_user.py --ensure`를 실행하고, 다음으로 `assets` 준비 단계와 실제 전송을
  순서대로 수행한다. 실패 시 취소 명령을 보내며 HMI 자체 5분 timeout도 둔다.
- 준비 overlay는 현재 단계 문구와 8~76% 구간을 왕복하는 막대를 표시하고, 실제 전송이
  시작되면 file·byte·percent 진행률로 전환된다.
- `python samples/suite/update_hmi_assets.py --dry-run`은 약 37초의 font 확인을 포함해
  자산 177개와 manifest, 총 1,135,443 byte를 검증했으며 장치를 변경하지 않았다.
- Windows `sample-suite` test 23/23, 도구 test 47/47과 준비 overlay screenshot 검사가
  통과했다.
- HMI compile-only build가 RAM 29,928 byte(5.8%), flash 913,696 byte(13.9%)로
  성공했다. firmware upload와 실제 자산 전송은 수행하지 않았다.

## 5. 메모리·성능 예산

- 입력 callback stack에는 font file buffer를 두지 않는다. block buffer는 checked heap
  allocation을 사용하고 parse 직후 해제한다.
- 단일 BIN은 32KiB 이하, 일반적인 24px/2bpp 64글자 block은 이보다 훨씬 작아야 한다.
- cache index는 자산 수만큼 checked heap에 만들고 font bitmap은 실제 사용 block만
  지연 할당한다. 생성된 전체 BIN 합계는 기존 4분할 1,581,800 bytes보다 작은
  932,464 bytes이며 단일 block은 최대 6,192 bytes다.
- font는 page lifetime 동안 유지해 LVGL label의 borrowed pointer를 무효화하지 않는다.
- keyboard widget 수는 입력마다 변하지 않아야 한다.

## 6. 위험과 대응

| 위험 | 대응 |
| --- | --- |
| 작은 파일 수 증가로 asset 전송 시간이 늘어남 | bundle file count와 전송 manifest 한계를 자동 검사한다. |
| 2bpp에서 획 품질 저하 | Windows capture와 실제 LCD 수동 gate를 둔다. |
| stale 대형 BIN이 bundle에 혼입 | 생성기가 소유 패턴의 미등록 파일을 제거하고 check가 extra 파일을 실패시킨다. |
| cache 확대가 page 객체를 과대화 | `sizeof(FontRepository/TextInputPage)`와 HMI RAM을 기록한다. |
| font를 참조 중 해제 | page lifetime 유지 정책을 test와 주석으로 고정한다. |
| Windows 성공을 HMI 성능 완료로 오인 | R3는 사용자 실기기 승인 전 `blocked`로 유지한다. |

## Automated Verification Checklist

- [x] Text Input interaction test에서 일반 문자 입력 중 keyboard delete가 0회다.
- [x] `python tools/assets/typography/generate_user.py --dry-run`이 성공한다.
- [x] `python tools/assets/typography/generate_user.py` 후 `--check`가 성공한다.
- [x] user font가 common 1개와 Hangul 175개이며 모든 BIN이 32KiB 이하다.
- [x] font repository/loader와 font render 관련 Windows test가 성공한다.
- [x] Text Input state/interaction Windows test가 성공한다.
- [x] `python tools/project.py bundle-assets --profile sample-suite`가 성공하고 stale 대형 BIN이 없다.
- [x] `pio run -e hmi-sample-suite`가 upload 없이 성공한다.
- [x] font cache·`TextInputPage` 크기와 HMI RAM/flash가 기록된다.
- [x] `git diff --check`가 성공한다.
- [x] 자산 갱신 overlay Windows test와 capture가 성공한다.
- [x] 자산 갱신 snapshot/restart 조립을 포함한 HMI build가 성공한다.

## Manual Verification Checklist

- [x] Windows에서 한글/영문/숫자/Space/삭제 연속 입력과 Shift·mode 전환이 자연스럽다.
- [x] Windows에서 2bpp 24px 한글 획과 baseline이 허용 품질이다.
- [x] 사용자가 새 firmware와 asset을 실제 HMI에 적용한다.
- [ ] `samples_update_hmi_assets.bat` 시작 시 HMI가 폰트 확인 단계 진행 화면으로 즉시 전환된다.
- [x] 파일 수·전송량·percent가 수신 중 증가한다.
- [x] 성공 화면 뒤 약 1.5초 후 HMI가 자동 재부팅한다.
- [x] 실제 HMI에서 첫 한글 block load가 입력을 방해하지 않는다.
- [x] 실제 HMI에서 같은 block과 다른 block의 연속 입력에 반복적인 멈칫이 없다.
- [x] 사용자가 입력 반응이 202609 수준이라고 승인한다.
