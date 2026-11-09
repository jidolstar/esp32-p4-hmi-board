# S02 Text Input UX 완성 계획: 커서·표준 자판·폼 완료 흐름

작성: 2026-11-08. 상태: `complete` (UX1~UX5 `complete`; 2026-11-08 사용자 승인).

이 계획은 `_plan/2026-11/20261107_004_BilingualI18nAndTextInput.md`에서 구현한
i18n, bounded UTF-8 editor, 한글 조합, 분할 SD font와 USB asset 경로를 유지하면서,
실기기 검증에서 확인된 입력 UX 결함을 보완한다. 기존 계획의 L4/L5 수용 기준 중
cursor·touch keyboard·입력 완료 흐름은 이 문서의 완료 기준으로 대체한다.

## 1. 목적과 결정 요약

목적은 `text-input` page를 기능 시연 수준에서 일반적인 입력폼으로 사용할 수 있는
수준으로 끌어올리는 것이다. Windows와 ESP32-P4 HMI는 같은 editor, cursor, keyboard,
field 진행 상태와 확인 UI를 사용한다.

핵심 결정은 다음과 같다.

1. cursor를 문자열 `|`로 합성하지 않는다. 입력 글리프와 독립된 2~3px caret 객체를
   만들고 500ms 표시/500ms 숨김으로 점멸시킨다. 입력·cursor 이동·field 전환 시 즉시
   표시 상태로 재시작한다.
2. 활성 입력줄을 touch하면 가장 가까운 Unicode codepoint 경계로 cursor를 옮긴다.
   긴 문자열은 cursor가 보이도록 가로 scroll하고, Windows 좌우 방향키도 같은 모델을
   사용한다.
3. `../dev_202609_hmi/src/ui/components/keyboard.cpp`의 5행 배치를 기준으로 새 공통
   keyboard를 만든다. 숫자행, QWERTY/두벌식 3행, 기능행을 사용하고 넓은
   `공백`/`Space` key를 제공한다.
4. 현재의 별도 `숫자` 버튼은 page에서 제거한다. 숫자 1~0은 항상 첫 행에 표시한다.
   `KeyboardMode::number`는 향후 숫자 전용 field capability를 위해 engine에 유지하되
   이번 page의 기본 흐름에서는 노출하지 않는다.
5. field 진행은 `이름 → 메모 → 값` 순서다. 이름과 메모의 submit key는 `다음`, 값의
   submit key는 `입력 완료`로 표시한다. 물리 Enter도 동일한 흐름을 실행한다.
6. 마지막 `입력 완료`는 조합을 확정한 뒤 세 field의 값을 보여주는 확인 modal을 연다.
   `수정`은 modal을 닫고 마지막 활성 field로 돌아가며, `확인`은 내용을 유지한 채 완료
   상태를 표시한다. 이 sample에서는 저장소/NVS 기록과 Dashboard 이동을 자동 실행하지
   않는다.
7. 기존 `UserTextLabel`은 읽기 전용 표시 책임을 유지한다. cursor·touch hit test·조합
   밑줄·가로 scroll을 가진 `UserTextInputLine`을 별도 공통 component로 추가해 표시용
   widget을 비대하게 만들지 않는다.
8. page는 field 진행과 확인 modal만 소유한다. UTF-8 경계·조합·cursor index는
   `modules/text`, LVGL 배치·blink·touch는 `modules/ui`가 소유한다. Board, Windows SDL,
   HMI driver에는 입력 정책을 추가하지 않는다.

## 2. 현재 상태와 202609 비교 근거

| 항목 | 현재 구현 | 202609에서 확인한 기반 | 이번 목표 |
| --- | --- | --- | --- |
| cursor 표시 | `refresh_field()`가 문자열 안에 `|`를 삽입한다. 항상 고정 표시이고 font glyph처럼 보인다. | `UserTextLine`이 실제 2px LVGL 객체를 글리프 경계에 둔다. | 실제 caret + 점멸 + 입력 시 blink reset |
| 중간 위치 선택 | field 전체 선택만 가능하다. touch 위치는 cursor에 반영되지 않는다. | 글리프 advance 경계를 저장하고 touch x와 가장 가까운 index를 전달한다. | touch codepoint 위치 지정 + 좌우 key parity |
| 긴 입력 | active field의 cursor 보장 scroll이 없다. | cursor 위치까지 자동 가로 scroll한다. | cursor가 항상 field viewport 안에 보임 |
| 자판 | 문자 26개를 10/9/7의 3행으로 표시하고 모든 기능 key를 마지막 한 행에 몰아둔다. | 숫자행을 포함한 5행 고정 자판과 staggered QWERTY/두벌식 배치를 사용한다. | 5행 자판을 현재 theme/font 구조로 재구성 |
| 공백 | UI key와 model action이 없다. | 넓은 Space key와 `KeyboardEventType::Space`가 있다. | 모든 일반 text field에서 cursor 위치에 공백 삽입 |
| cursor engine | `Editor::move_cursor(delta)`와 UTF-8 중간 삽입은 이미 있다. | display index 기반 절대 이동이 있다. | 기존 bounded editor에 절대 codepoint 이동만 보강 |
| field 진행 | `입력완료`가 현재 field의 조합만 확정하고 focus를 바꾸지 않는다. | 화면별 Enter label과 handler 정책을 주입할 수 있다. | `다음 → 다음 → 입력 완료` context action |
| 완료 확인 | 입력 결과를 한곳에서 검토할 수 없다. | 공통 dialog/scrim 패턴이 있다. | 세 값 확인 modal + 수정/확인 |

현재 순수 model은 중간 삽입과 좌우 이동을 이미 지원하므로 한글 composer를 다시 만들지
않는다. 202609 코드는 동작 기준으로 사용하되, `std::string` 소유·기존 font manager와
제품 전용 i18n 결합은 그대로 복사하지 않는다. 현재의 192 UTF-8 byte/64 codepoint 상한,
lazy binary font, locale catalog와 theme를 유지한다.

## 3. 목표 UX 계약

### 3.1 Field와 cursor

- field를 누르면 해당 field가 활성화되고 focus border와 점멸 caret가 표시된다.
- 문자열 안을 누르면 가장 가까운 글자 앞/뒤 경계로 cursor가 이동한다.
- 한글 조합 중 touch 이동, field 변경, mode 변경, submit은 먼저 조합을 확정한다.
  용량 제한으로 확정에 실패하면 기존 field와 cursor를 그대로 유지한다.
- cursor는 빈 field의 시작, 문자열 중간, 문자열 끝에서 모두 보여야 한다.
- 조합 중 글자는 cursor 앞에 preview하고 2px 밑줄로 조합 범위를 표시한다.
- 64 codepoint까지 입력해도 active cursor가 horizontal viewport 안에 유지된다.
- page destroy 전에 blink timer와 widget callback을 해제한다. Dashboard로 돌아간 뒤
  callback이 남아서는 안 된다.

### 3.2 5행 keyboard

1024x600의 오른쪽 keyboard panel 안에 다음 순서를 고정한다.

1. `1 2 3 4 5 6 7 8 9 0`
2. 한글 `ㅂㅈㄷㄱㅅㅛㅕㅑㅐㅔ` 또는 영문 `QWERTYUIOP`
3. 가운데로 들여쓴 `ㅁㄴㅇㄹㅎㅗㅓㅏㅣ` 또는 `ASDFGHJKL`
4. `SHIFT` + `ㅋㅌㅊㅍㅠㅜㅡ` 또는 `ZXCVBNM` + context submit
5. `기호` + `한/영` + `,` + 넓은 `공백`/`Space` + `.` + `글자삭제`

Symbol mode도 같은 5행 frame과 기능 key 위치를 유지한다. SHIFT는 한 글자 입력 뒤
해제하는 기존 규칙을 유지한다. 문자 key는 동적 user font, 기능 key는 생성된 고정 font를
사용한다. 각 touch target은 최소 44px 높이를 유지한다.

### 3.3 Form 진행과 확인

```text
이름 입력 --다음/Enter--> 메모 입력 --다음/Enter--> 값 입력
   ^                                                  |
   |                                        입력 완료/Enter
   |                                                  v
   +-------------------- 수정 <--- 입력 확인 modal ---+
                                      |
                                     확인
                                      v
                            완료 상태, 입력값 유지
```

- 사용자가 field를 직접 누르면 순서를 건너뛸 수 있다. submit label과 동작은 현재 field
  index에 따라 결정된다.
- `다음`은 현재 조합을 확정하고 다음 field를 활성화하며 cursor를 그 field 끝에 둔다.
- 마지막 `입력 완료`는 모든 model의 조합을 확정한 뒤 modal을 연다.
- 빈 field도 sample 기능 확인을 막지 않는다. modal에서는 locale별 `입력 없음`/`Empty`
  문구로 명확히 표시한다. 필수값 validation은 실제 상품 form 정책이 생길 때 별도 계약으로
  추가한다.
- modal의 `수정`은 마지막 활성 field와 cursor를 복원한다. `확인`은 modal을 닫고
  `입력 내용을 확인했습니다` 상태를 표시하되 값을 지우거나 저장됐다고 표현하지 않는다.
- locale/theme 전환은 edit/review/confirmed 상태와 입력값을 보존하고 고정 문구만 갱신한다.

## 4. 목표 구조와 변경 경계

```text
modules/text/
  editor.*                     # 절대 codepoint cursor 이동
  input_model.*                # space, move_cursor_to, 조합 확정 계약
  keyboard_layout.*            # 문자/기호 row descriptor와 width unit

modules/ui/
  user_text_input_line.*       # glyph 경계, caret, 조합 밑줄, touch, scroll, blink
  virtual_keyboard.*           # 5행 LVGL key 배치와 semantic event 방출

samples/suite/pages/text_input/
  text_input_page.*            # 세 field 진행, context submit, 확인 modal
  i18n/*.yaml                  # 다음/완료/공백/확인/수정/빈 값/완료 상태
  generated/                   # generator만 갱신
  tests/*                      # model/component/page interaction과 capture
```

책임 경계:

- `modules/text`는 LVGL, locale, font와 modal을 모른다.
- `UserTextInputLine`은 문자열을 소유하거나 editor를 수정하지 않는다. page가 전달한 text,
  composing preview와 cursor를 표시하고 선택된 codepoint index만 callback으로 돌려준다.
- `VirtualKeyboard`는 field 순서를 모른다. character/action event와 page가 제공한 context
  submit caption만 표시한다.
- `TextInputPage`는 세 `InputModel`, active field, `editing/review/confirmed` flow state와
  modal 수명만 소유한다.
- Dashboard, Windows/HMI host, Board와 SD asset protocol은 수정하지 않는다. 새 source
  등록과 generated profile 갱신 외 조립 변경은 아래 theme 상태 보존에 한정한다.

구현 중 theme 전환이 active page 객체를 파기해 입력값까지 잃는 기존 조립 동작을
확인했다. 따라서 `SampleSuiteApp`은 theme widget을 재생성할 때 active `SamplePage`
객체를 유지하고 `destroy()`/`create()`로 view만 교체하도록 최소 변경했다. Dashboard,
host, Board와 asset protocol은 그대로 유지한다.

## 5. 메모리와 수명 예산

- editor 상한은 field당 192 bytes/64 codepoints를 유지한다. 세 field의 model을 추가
  snapshot으로 복사하지 않는다.
- `UserTextInputLine`의 glyph boundary는 최대 65개의 bounded scalar 배열로 둔다. 입력
  문자열과 font bitmap을 소유하지 않으며 `sizeof(UserTextInputLine) <= 512` guard를 둔다.
- `VirtualKeyboard`는 최대 48개 key pointer/event를 고정 배열로 소유하고 heap container를
  사용하지 않는다. `sizeof(VirtualKeyboard) <= 1024` guard를 둔다.
- 확인 modal은 page가 이미 소유한 model을 view로 읽는다. 이름·메모·값 전체 복사본이나
  별도 제출 model을 만들지 않는다.
- caret blink timer는 active line당 하나만 동작한다. inactive field는 timer를 멈추고
  caret를 숨긴다. page destroy 시 timer 0개가 종료 조건이다.
- 구현 후 `TextInputPage`, `InputModel`, 두 새 component의 Windows/HMI `sizeof`와 firmware
  RAM/flash 변화량을 기록한다. page size guard를 올려야 한다면 실제 field/component
  합산 근거를 먼저 기록하고 최소치로 조정한다.

## 6. 단계 계획

| 단계 | 상태 | 결과 |
| --- | --- | --- |
| UX1 | `complete` | 절대 cursor 이동·Space·5행 row descriptor의 순수 model 계약 |
| UX2 | `complete` | 점멸 caret·touch 위치 지정·조합 밑줄을 가진 입력줄 component |
| UX3 | `complete` | 202609 기반 5행 virtual keyboard component와 context submit |
| UX4 | `complete` | `다음 → 다음 → 입력 완료 → 확인` page flow와 i18n UI |
| UX5 | `complete` | 자동 통합과 사용자 실기기 수용 완료 |

한 번에 하나의 단계만 `in_progress`로 바꾸며, 각 단계의 자동 검증과 가능한 수동 확인을
통과한 뒤 다음 단계로 이동한다.

### UX1. 순수 편집·자판 계약 보강

**목표:** UI 없이 Space와 touch absolute cursor 이동을 UTF-8 경계에서 안전하게 처리하고,
5행 keyboard가 소비할 row descriptor를 제공한다.

**범위:** `modules/text/include/hmi/text/editor.hpp`, `input_model.hpp`,
`keyboard_layout.hpp`, 대응 `src/`와 `tests/`. LVGL component와 page는 제외한다.

**산출물:** `move_cursor_to(codepoint_index)`, `insert_space()`, row/width descriptor,
한글·영문·symbol·number mode의 결정적 layout test.

**자동 검증:** 문자열 시작/중간/끝과 범위 밖 cursor 요청, 조합 중 절대 이동 전 확정,
한글/ASCII 사이 Space 삽입·삭제, 용량 한계 실패 시 원본 보존, 5행 row key 순서와 width
unit를 native test로 확인한다.

**수동 검증:** 두벌식/QWERTY/기호 배열표가 202609 배치와 일반적인 키 순서에 맞는지
코드 표로 검토한다.

**종료 조건**

- [x] absolute cursor가 UTF-8 continuation byte에 놓일 수 없다.
- [x] Space가 현재 cursor에 삽입되고 한글 조합 수명 규칙을 지킨다.
- [x] 숫자행과 10/9/7 문자행의 순서·SHIFT 결과가 test로 고정된다.
- [x] `modules/text`의 LVGL·플랫폼·Board 의존이 0건이다.

**의존:** 기존 L2 bounded editor와 Hangul composer.

**UX1 검증 근거 (2026-11-08):** 관련 세 target을 `cmake --build`로 컴파일했고,
`build/windows-sample-suite/selected-profile-tests`에서 `hmi_text_editor`,
`hmi_text_input_model`, `hmi_text_keyboard_layout` 3/3이 통과했다. 경계 검색 결과
`modules/text`의 LVGL·SDL·Arduino·FreeRTOS·Board 의존은 0건이었다. 두벌식과 QWERTY의
10/9/7 행 시작·끝 key, 숫자 1~0, SHIFT 배열을 assertion으로 고정해 202609 기준 배열과
수동 대조했다.

### UX2. `UserTextInputLine`과 실제 caret

**목표:** 분할 font 문자열에 실제 점멸 caret, 조합 밑줄, touch cursor와 자동 scroll을
제공한다.

**범위:** 새 `modules/ui/include/hmi/ui/user_text_input_line.hpp`,
`modules/ui/src/user_text_input_line.cpp`, `modules/ui/tests/user_text_input_line_test.cpp`,
`samples/suite/CMakeLists.txt`. page flow와 keyboard는 제외한다.

**산출물:** bounded glyph boundary component, 500ms blink timer, active/inactive API,
cursor selection callback, render 실패 시 기존 표시 보존 계약.

**자동 검증:** 빈 문자열/혼합 한글·ASCII/64 codepoint, 시작·중간·끝 cursor 좌표,
touch x→index mapping, 조합 underline, blink visible/hidden 전이, 입력 후 blink reset,
horizontal scroll, destroy 후 timer/callback 미호출을 확인한다.

**수동 검증:** Windows component capture에서 caret 두께·색·baseline·underline·field focus가
dark/light theme 모두 선명한지 확인한다.

**종료 조건**

- [x] cursor가 `|` glyph가 아닌 별도 LVGL 객체다.
- [x] active field에서만 caret가 점멸하고 edit/touch 뒤 즉시 보인다.
- [x] touch가 가장 가까운 Unicode codepoint 경계를 선택한다.
- [x] 긴 입력에서 cursor가 viewport 밖으로 사라지지 않는다.
- [x] destroy 뒤 timer와 borrowed callback이 남지 않는다.

**의존:** UX1, 기존 `FontRepository`와 user font manifest.

**UX2 검증 근거 (2026-11-08):** Windows `sample-suite` 전체 build와
`hmi_ui_font_repository`, `hmi_ui_user_text_label`, `hmi_ui_user_text_input_line` 3/3이
통과했다. 새 test는 빈 값·혼합 UTF-8·조합 preview·잘못된 요청의 기존 표시 보존,
500ms blink, pointer callback, 64자 자동 scroll, parent 삭제 후 timer 정리를 확인한다.
generator test 12/12와 플랫폼 전용 include 검색 0건도 통과했다. 격리 capture의 dark/light
양쪽에서 2px caret와 조합 밑줄의 색·baseline을 확인했다.

### UX3. 5행 `VirtualKeyboard`

**목표:** 일반적인 5행 배치와 넓은 Space를 reusable LVGL component로 제공한다.

**범위:** 새 `modules/ui/include/hmi/ui/virtual_keyboard.hpp`,
`modules/ui/src/virtual_keyboard.cpp`, `modules/ui/tests/virtual_keyboard_test.cpp`,
keyboard action i18n 입력 초안. page 진행 상태는 제외한다.

**산출물:** 최대 48 key의 fixed-capacity component, semantic key event, locale caption view,
context submit caption, active SHIFT/symbol style와 minimum touch target.

**자동 검증:** 5행 geometry, 숫자/문자 key 순서, row stagger, Space width, action event,
SHIFT 1회 해제, 한/영·기호 mode 전환, disabled/destroy lifetime을 확인한다.

**수동 검증:** 1024x600 Windows capture에서 key 간격, 44px 이상 touch target,
한글/영문/기호와 dark/light 표시를 확인한다.

**종료 조건**

- [x] 숫자행과 넓은 Space가 항상 보인다.
- [x] 기능 key가 문자 key와 시각적으로 구분되며 한 행에 과밀하게 몰리지 않는다.
- [x] submit key caption을 page가 `다음`/`입력 완료`로 교체할 수 있다.
- [x] keyboard component가 editor·field index·modal을 참조하지 않는다.

**의존:** UX1 row descriptor, 기존 고정/동적 font pipeline.

**UX3 검증 근거 (2026-11-08):** Windows 전체 build와
`hmi_text_keyboard_layout`, `hmi_ui_virtual_keyboard` 2/2, generator 12/12가 통과했다.
component test는 5행·44px 이상 key·숫자 1~0·10/9/7 문자행·넓은 Space·SHIFT/submit
semantic event·한글/대문자/기호 재구성·parent 삭제 수명을 확인했다. 600x390 dark capture에서
기능 key 분리, Space 폭과 row 간격을 확인했고 플랫폼 전용 include 검색은 0건이었다.

### UX4. Form 진행·확인 modal 통합

**목표:** 세 field가 context submit으로 이동하고 마지막에 입력 내용을 검토·확정할 수
있게 한다.

**범위:** `samples/suite/pages/text_input/text_input_page.*`, `i18n/ko.yaml`, `en.yaml`,
`typography.yaml`, generator 산출물, text-input state/interaction test. 저장소/NVS는 제외한다.

**산출물:** `editing/review/confirmed` 상태, active field focus, `다음/입력 완료` 갱신,
확인 modal, `수정/확인`, 빈 값·완료 status 문구, page capture.

**자동 검증:** 이름 Enter→메모, 메모 Enter→값, 값 Enter→review, modal 값 일치,
수정 후 중간 삽입, 확인 후 값 보존, field 직접 선택, Space 입력, locale/theme 변경 중 상태
보존, Dashboard 이탈 후 timer/modal 정리, font recovery 화면 회귀를 확인한다.

**문구·글꼴 검증:** 고정 문구를 `ko.yaml`/`en.yaml`과 typography role에 먼저 등록한 뒤
generator dry-run→generate→check를 한 번 수행한다. 생성 C++/font는 직접 편집하지 않는다.
modal의 사용자 입력값은 catalog가 아닌 동적 user font로 표시한다. 여러 줄 설명은 최소
5px line spacing과 명시적 높이/정렬을 적용한다.

**수동 검증:** 한국어/영어에서 field focus, cursor, submit caption, modal 값, 수정/확인,
긴 값 clipping/scroll과 dark/light 레이아웃을 Windows에서 확인한다.

**종료 조건**

- [x] `다음 → 다음 → 입력 완료`가 touch와 물리 Enter에서 동일하게 동작한다.
- [x] 마지막 submit이 세 입력값을 보여주는 modal을 연다.
- [x] `수정`은 편집 위치로 복귀하고 `확인`은 저장을 가장하지 않으며 값을 유지한다.
- [x] locale/theme 전환이 edit/review/confirmed 상태와 동적 입력값을 손상하지 않는다.
- [x] page destroy 뒤 modal, timer, widget callback이 0개다.

**의존:** UX2, UX3.

**UX4 검증 근거 (2026-11-08):** catalog 31개 문구를 확정한 뒤 typography
dry-run→generate→check와 생성기 unit test 8/8을 통과했다. 관련 Windows 11개 test와
최종 UX4 범위 6/6이 통과했으며 pointer test가 조합 preview touch 확정, 5행 key 입력,
`다음 → 다음 → 입력 완료 → 수정 → 입력 완료 → 확인`, locale/theme 재생성 후 값·상태
보존을 확인한다. 1024x600 ko/dark, en/light와 review modal capture에서 caret, 5행 keyboard,
넓은 Space, 동적 입력값과 고정 문구의 clipping/overlap이 없음을 확인했다.

### UX5. Windows/HMI 통합과 실기기 수용

**목표:** 같은 source가 Windows와 HMI에서 build되고 실제 LCD/touch에서 입력 UX를
수용할 수 있음을 확인한다.

**범위:** generated source list, 관련 Windows tests/captures, HMI `sample-suite` build,
memory/경계 검사, `START_HERE.md`, 사용자 실기기 확인. Codex upload는 제외한다.

**산출물:** Windows dark/light·ko/en capture, 관련 test 근거, HMI RAM/flash와 객체 크기,
실기기 절차와 사용자 확인.

**자동 검증:** 관련 Windows target/test, typography check, source isolation, 공통
text/UI의 SDL·Arduino·Board 내부 의존 0건, HMI `sample-suite` build와 RAM/flash 기록을
수행한다.

**수동 검증:** 사용자가 firmware와 asset을 적용한 실제 장치에서 caret blink, touch
중간 이동, Space, 한글 조합, 5행 key touch, 다음 field 이동, 확인 modal, 반복 page
진입/이탈을 확인한다.

**종료 조건**

- [x] Windows capture와 관련 자동 test가 모두 통과한다.
- [x] HMI `sample-suite`가 upload 없이 build되고 memory budget을 넘지 않는다.
- [x] 사용자가 실제 LCD에서 caret와 조합 underline의 선명도를 확인한다.
- [x] 사용자가 touch로 문자열 중간 cursor 이동과 삽입을 확인한다.
- [x] 사용자가 `다음 → 다음 → 입력 완료 → 수정/확인` 전체 흐름을 승인한다.
- [x] 반복 진입/이탈과 theme/locale 전환 뒤 멈춤·중복 callback·reset이 없다.

**의존:** UX1~UX4 `complete`.

**UX5 자동 검증 근거 (2026-11-08):** Windows `sample-suite` check 19/19와 도구
unit test 40/40, typography check, 네 profile isolation을 통과했다. 공통 text/UI와
`text-input` page의 SDL·Arduino·FreeRTOS·Board internal 의존 검색은 0건이다. upload 없이
HMI `hmi-sample-suite` build가 성공했고 RAM 28,988/512,000 bytes(5.7%), flash
815,058/6,553,600 bytes(12.4%)였다. 비교 가능한 이전 firmware snapshot이 저장소/VCS에
없어 정확한 증감치는 계산하지 못했으며 현재 절대 사용량을 기준선으로 기록한다.
객체 크기는 Windows/HMI 순서로 `InputModel` 232/216, `UserTextInputLine` 224/180,
`VirtualKeyboard` 872/632, `TextInputPage` 3,432/2,420 bytes다. 남은 gate는 아래 실제
LCD/touch 사용자 확인뿐이다. 확인 modal의 scrim은 page flex에서 분리해 전체 화면
형제로 배치했고, dialog가 화면 중앙에 놓이는지 interaction test 좌표 assertion과
1024x600 review capture로 추가 검증했다.

**UX5 사용자 승인 근거 (2026-11-08):** 사용자가 계획 중 `text-input` 관련 범위를 모두
완료로 판정했다. 실제 LCD caret/underline, touch cursor와 전체 form 흐름, 반복 전환 수동
gate를 통과한 것으로 기록하고 UX5와 S02 UX 계획을 `complete`로 전환했다.

## 7. 예상 변경 파일

| 경로 | 변경 이유 |
| --- | --- |
| `modules/text/include/hmi/text/editor.hpp`, `modules/text/src/editor.cpp` | absolute codepoint cursor |
| `modules/text/include/hmi/text/input_model.hpp`, `modules/text/src/input_model.cpp` | Space와 조합 안전 cursor 이동 |
| `modules/text/include/hmi/text/keyboard_layout.hpp`, `modules/text/src/keyboard_layout.cpp` | 5행 row/width descriptor |
| `modules/text/tests/*` | UTF-8 cursor·Space·layout 회귀 |
| `modules/ui/include/hmi/ui/user_text_input_line.hpp`, `modules/ui/src/user_text_input_line.cpp` | caret·touch·scroll·blink |
| `modules/ui/include/hmi/ui/virtual_keyboard.hpp`, `modules/ui/src/virtual_keyboard.cpp` | 5행 keyboard와 semantic event |
| `modules/ui/tests/*` | component geometry·lifetime·interaction |
| `samples/suite/pages/text_input/text_input_page.*` | field 진행과 확인 modal |
| `samples/suite/pages/text_input/i18n/*.yaml` | 다음·완료·공백·확인 문구와 font role |
| `samples/suite/pages/text_input/generated/` | generator가 갱신하는 catalog/fixed font |
| `samples/suite/pages/text_input/tests/*` | form state, pointer/keyboard, capture |
| `samples/suite/app.*` | theme 재생성 중 active page 객체와 in-memory 입력 상태 유지 |
| `samples/suite/CMakeLists.txt`, generated profile source | 새 공통 component/test 조립 |
| `START_HERE.md` | 완성된 sample 사용법과 실제 장치 확인 절차 |

`boards/`, `platform/`, `hosts/`, Dashboard와 다른 sample page 내부는 변경 대상이 아니다.

## 8. 위험과 대응

| 위험 | 대응 |
| --- | --- |
| blink 때마다 전체 문자열과 font를 재생성 | caret 객체의 hidden/opacity만 변경하고 text render는 edit 때만 수행한다. |
| touch x가 UTF-8 byte 중간을 선택 | font advance 기반 codepoint boundary 배열만 callback으로 반환한다. |
| 조합 중 cursor 이동으로 글자 손실 | `InputModel::move_cursor_to()`가 commit 성공 후에만 cursor를 바꾼다. |
| 5행 key가 오른쪽 panel에서 잘림 | available width에서 width unit를 계산하고 44px touch target을 test/capture한다. |
| 새 고정 문구의 glyph 누락 | catalog/typography 확정 후 generator dry-run→generate→check를 수행한다. |
| modal이 사용자 값을 고정 font로 표시 | 입력값은 `UserTextLabel`/dynamic font view를 사용한다. |
| page object가 커짐 | bounded 배열과 size guard를 유지하고 값 snapshot 복사를 금지한다. |
| timer가 page 이탈 뒤 실행 | component destroy에서 timer를 먼저 삭제하고 LVGL delete test를 둔다. |
| Windows 성공을 실기기 완료로 오인 | UX5에 별도 LCD/touch 수동 gate를 두고 사용자 확인 전 S02를 완료하지 않는다. |

## Automated Verification Checklist

- [x] `python tools/assets/typography/generate.py --dry-run`이 성공한다.
- [x] `python tools/assets/typography/generate.py` 후 `--check`가 성공한다.
- [x] `python -m unittest tools.tests.test_typography_generation -v`가 성공한다.
- [x] `python tools/project.py build --host windows --profile sample-suite`가 성공한다.
- [x] 관련 text/UI/page Windows test가 성공한다.
- [x] cursor 시작/중간/끝, Space, 조합 후 touch 이동, 긴 입력 scroll test가 성공한다.
- [x] 5행 key 순서·geometry·Space width·context submit test가 성공한다.
- [x] 이름→메모→값→review→수정/확인 state/interaction test가 성공한다.
- [x] dark/light와 ko/en Windows capture에 clipping·overlap이 없다.
- [x] `python tools/project.py check --scope isolation`이 성공한다.
- [x] 공통 `modules/text`, `modules/ui`, `text-input` page의 SDL·Arduino·FreeRTOS·Board internal 의존 검색이 0건이다.
- [x] `python tools/project.py build --host hmi --profile sample-suite`가 upload 없이 성공한다.
- [x] `InputModel`, `UserTextInputLine`, `VirtualKeyboard`, `TextInputPage` 크기와 HMI RAM/flash가 기록된다.
- [x] page destroy 뒤 caret timer, modal과 callback이 남지 않는 test가 성공한다.

## Manual Verification Checklist

- [x] Windows dark/light, ko/en 화면에서 5행 keyboard와 넓은 Space가 자연스럽다.
- [x] Windows pointer로 문자열 시작·중간·끝을 눌렀을 때 caret가 정확히 이동한다.
- [x] 실제 LCD에서 active field caret가 선명하게 점멸하고 inactive field에는 보이지 않는다.
- [x] 실제 touch로 한글 조합 중/후 cursor 이동, 중간 삽입과 글자삭제가 자연스럽다.
- [x] 실제 touch에서 숫자행, 두벌식/QWERTY, SHIFT, 기호, 한/영, Space와 글자삭제가 오입력 없이 동작한다.
- [x] 이름의 `다음`이 메모로, 메모의 `다음`이 값으로 focus를 이동한다.
- [x] 값의 `입력 완료`가 세 입력값을 정확히 보여주는 확인 modal을 연다.
- [x] `수정`과 `확인`의 동작·문구가 기대와 일치하고 저장됐다고 오해시키지 않는다.
- [x] locale/theme 전환과 Dashboard 반복 왕복 뒤 입력 중복·멈춤·reset이 없다.
- [x] 사용자가 S02 Text Input UX 완료를 승인한다.
