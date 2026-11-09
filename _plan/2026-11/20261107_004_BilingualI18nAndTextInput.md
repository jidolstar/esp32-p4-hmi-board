# 범용 HMI 시작 프로젝트 4차 계획: 한·영 i18n과 텍스트 입력

작성: 2026-11-07. 상태: `complete` (L1~L5 `complete`; 2026-11-08 사용자 승인).

선행 조건은 `_plan/2026-11/20261107_003_ProfileRegistryAndSampleSuiteDashboard.md`의 P1~P6가 모두 `complete`인 것이다. 이 문서는 누락돼 있던 한국어/영어 화면 전환 계약과 기존 S02 키보드 입력 요구를 하나의 실행 가능한 G2 첫 계획으로 고정한다. 현재 G1의 P5/P6 범위를 넓히지는 않는다.

## 1. 목적과 결정 요약

두 언어의 고정 UI 문구와 사용자가 입력하는 동적 문자열을 서로 다른 경로로 처리하면서, Windows와 ESP32-P4 HMI가 같은 locale·편집·한글 조합 로직을 사용하게 한다.

핵심 결정은 다음과 같다.

1. 지원 locale은 첫 단계에서 `ko-KR`, `en-US` 두 개로 제한한다. locale별 화면을 복제하지 않고 같은 widget tree에 locale catalog를 주입한다.
2. `samples/suite`의 Dashboard와 `ui-basics`는 자산 없이 부팅 가능한 영어 진단 shell로 유지한다. `text-input` page가 i18n·고정/동적 font·화상 키보드 capability를 독립적으로 소유한다.
3. page 고정 문구는 한국어/영어 catalog와 typography role에서 생성한다. 키보드 문자 키, 실제 입력값, 사용자 이름은 번역 catalog에 넣지 않는다.
4. 두 catalog의 key와 format placeholder 집합은 생성 시 완전히 일치해야 한다. 누락 key를 다른 언어로 조용히 대체하지 않고 생성/검사 단계에서 실패시킨다.
5. locale 전환은 현재 page의 모든 고정 문구와 형식을 즉시 갱신한다. 선택값은 `SampleSuiteApp` 수명 동안 page 재진입 후에도 유지한다. 재부팅 영구 저장은 필수 범위가 아니며 실제 요구가 생기면 별도 page 없이 app 조립에서 추가한다.
6. 키보드는 두벌식 한글, 영문 대/소문자, 숫자, 기호 mode를 제공한다. `한/영`, `SHIFT`, `글자삭제`, `입력완료` 같은 기능 키만 i18n 고정 문구다.
7. 조합·편집 engine은 LVGL에 독립적인 `modules/text/` 순수 C++ 코드다. LVGL page는 표시, cursor와 키 event 전달만 담당한다.
8. 편집 buffer와 조합 상태는 상한이 있는 고정 용량으로 두고, firmware startup/task stack에 큰 자동 객체나 전체 catalog 복사본을 만들지 않는다.

## 2. 현재 상태와 누락 근거

- `modules/text/`에는 안전한 UTF-8 단일 codepoint decoder와 test만 있고, cursor/edit buffer·한글 조합·키보드 layout은 없다.
- `modules/ui/`에는 공통 theme/component가 있지만 locale catalog나 동적 사용자 글꼴 renderer는 없다.
- `samples/suite/page_registry.json`에는 `ui-basics`만 등록돼 있고 `text-input` page가 없다.
- `samples/suite/profile.json`의 module은 `ui`뿐이며 locale/font asset 선언이 없다.
- 현재 G1 Dashboard와 `ui-basics`는 영어 ASCII/내장 Montserrat 진단 UI로 의도적으로 제한돼 있다.
- 상위 목록의 S02에는 한글·영문 입력이 있으나, `ko-KR`/`en-US` 화면 문구 전환·catalog 동등성·locale lifetime·검증 기준이 없었다.

## 3. 목표 구조와 책임 경계

```text
modules/
  i18n/
    include/hmi/i18n/locale.hpp
    include/hmi/i18n/catalog.hpp
    src/
    tests/
  text/
    include/hmi/text/utf8.hpp
    include/hmi/text/editor.hpp
    include/hmi/text/hangul_composer.hpp
    include/hmi/text/keyboard_layout.hpp
    src/
    tests/
samples/suite/pages/text_input/
  i18n/
    ko.yaml
    en.yaml
    typography.yaml
  generated/                  # 생성기가 소유하며 직접 편집하지 않음
  text_input_page.*
  tests/
assets/fonts/                 # 필요한 최소 고정/동적 생성 자산
tools/assets/typography/      # catalog/font 생성·검사
```

책임 경계:

- `modules/i18n`은 locale 값, typed text ID 조회와 format 계약만 소유한다. LVGL, SD, Windows, Board를 참조하지 않는다.
- `modules/text`는 UTF-8 경계, cursor, 삽입/삭제, 조합 중 상태, 두벌식 조합과 layout model을 소유한다. 번역 문구·폰트·저장 정책을 소유하지 않는다.
- `text-input` page는 widget, 현재 입력 field, keyboard mode, locale toggle과 생성 font handle 사용을 소유한다. 다른 page 구현을 참조하지 않는다.
- `SampleSuiteApp`은 작은 `Locale` 값만 소유해 page 재생성·재진입에 전달한다. catalog 문자열이나 입력 buffer를 복사해 보관하지 않는다.
- Windows/HMI host는 같은 page와 engine을 조립한다. 플랫폼별 키보드 로직이나 번역 분기를 만들지 않는다.
- Board/LvglPort는 font, locale, keyboard, widget을 알지 않는다.

## 4. 표시 문구와 입력 데이터 계약

### 4.1 고정 UI 문구

다음 항목은 `ko.yaml`/`en.yaml`의 같은 text ID와 typography role을 갖는다.

- page 제목·설명, 입력 field 제목과 placeholder
- locale 이름과 전환 control
- 키보드 기능 키, 완료/초기화 action
- 글자 수·최대 길이·검증 오류와 조합 상태 안내
- 날짜·수치가 추가될 경우 locale별 format string과 단위

생성기는 key parity, placeholder 이름·개수·타입, 빈 문구, 중복 key, 미사용 key, typography role 누락을 거부한다. 생성된 C++/font/manifest는 직접 수정하지 않는다.

### 4.2 동적 사용자 문자열

키보드 문자 key, 실제 입력값과 조합 중 preview는 catalog에 넣지 않는다. 동적 font 경로가 UTF-8 codepoint를 기준으로 필요한 font block을 선택하며, glyph가 없을 때는 명시적인 replacement/error 상태를 page에 반환한다.

길이 제한은 다음 두 값을 별도로 가진다.

- 사용자 계약: Unicode codepoint 수
- 저장/전송 안전 계약: UTF-8 byte 수

조합 중인 자모는 확정 text와 분리해 계산하고, 완료·mode 전환·cursor 이동·page 이탈 때 명시적으로 확정하거나 취소한다.

## 5. 단계 계획

| 단계 | 상태 | 결과 |
| --- | --- | --- |
| L1 | `complete` | 두 locale 25개 문구의 key·placeholder·role 동등성과 typed catalog 검증 완료 |
| L2 | `complete` | 고정 용량 UTF-8 editor·두벌식 조합·네 mode layout과 전환 규칙 검증 완료 |
| L3 | `complete` | 고정/동적 font 자산과 bounded 사용자 text renderer 검증 완료 |
| L4 | `complete` | `text-input` page와 런타임 한·영 전환 및 Windows 입력 검증 완료 |
| L5 | `complete` | HMI/SD·USB asset updater 자동 범위와 사용자 실기기 승인 완료 |

한 번에 한 단계만 `in_progress`로 둔다. 각 단계의 종료 조건과 근거를 기록한 뒤 다음 단계로 이동한다.

### L1. Locale/catalog 계약과 생성 검사

**목표:** `ko-KR`/`en-US`가 동일한 typed text ID와 format 계약을 제공한다.

**범위:** `modules/i18n/`, page별 YAML/typography 입력, generator의 catalog 검증. 키보드와 LVGL page는 제외한다.

**산출물:** `Locale`, catalog view, 생성 text ID, 두 언어 catalog, key/placeholder/role parity 검사, 결정적 manifest.

**자동 검증:** i18n 단위 test, generator `--dry-run`/생성/`--check`, 동일 입력 재생성 diff 0건, `modules/i18n`의 LVGL·플랫폼·Board include 0건.

**수동 검증:** 한국어/영어 용어가 같은 기능 의미를 갖고 버튼 문구가 실제 동작을 표현하는지 검토한다.

**종료 조건**

- [X] 두 catalog의 text ID와 placeholder 계약이 일치한다.
- [X] 고정 문구마다 typography role이 있고 직접 C++ 문자열이 없다.
- [X] 생성 결과가 결정적이고 catalog/runtime 객체 크기가 기록됐다.

**완료 근거 (2026-11-07)**

- `modules/i18n/`에 플랫폼 독립 `Locale`, strict tag parser와 비소유 `CatalogView`를 추가했다. boundary 검색에서 LVGL·SDL·Arduino·FreeRTOS·Board include/호출은 0건이다.
- `samples/suite/pages/text_input/i18n/`의 `ko-KR`/`en-US` catalog는 L4 control 문구를 포함한 같은 25개 key를 사용한다. 기능 키는 화면 문구로 catalog에 두고 문자 key·실제 입력값은 제외했다. 한국어와 영어의 버튼 문구가 각각 실제 전환·삭제·완료·초기화 행동을 나타내는지 검토했다.
- 생성기는 key, `{name:u32|string}` placeholder 이름·타입·개수, 빈 문구와 typography role의 누락·추가·오류를 실패시킨다. 최종 `--check`가 hash `fc9dbc53fdb8b573fe77803d30ac74c5a21ac7564c3b82d829a3a9799c81c81f`로 통과했다.
- `python -m unittest tools.tests.test_typography_generation -v`의 정상·결정성·negative 8개와 기존 manifest/generator 27개가 통과했다. 같은 입력의 생성 byte가 같고 manifest에 입력 SHA-256과 typed placeholder 계약이 기록된다.
- Windows `sample-suite` build와 profile check 5개가 통과했으며 새 `hmi_sample_suite_i18n_catalog` C++ test가 두 locale의 typed 조회를 확인했다. `Locale`은 1 B, Windows `CatalogView`는 16 B 이하의 pointer-size pair이며 catalog 문자열/배열은 immutable static storage로 복사되지 않는다. HMI의 정확한 크기·RAM/flash는 L5에서 기록한다.
- generator version 4로 네 host/profile 생성 입력을 갱신했고 최종 isolation 검사가 `windows/main`, `windows/sample-suite`, `hmi/main`, `hmi/sample-suite`를 모두 확인했다.

### L2. Bounded UTF-8 editor와 두벌식 한글 조합

**목표:** UI와 무관한 편집 engine이 한글·영문·숫자·기호 입력을 안전하게 처리한다.

**범위:** UTF-8 cursor 경계, 삽입, backspace/delete, 중간 편집, selection 없는 단일 cursor, 조합/확정/취소, mode/SHIFT layout. LVGL widget과 font는 제외한다.

**산출물:** 고정 용량 editor, 두벌식 composer, keyboard layout model, 오류/한계 결과 enum, 단위 test.

**자동 검증:** ASCII·2/3/4-byte, 초/중/종성 결합과 겹받침, 조합 중 삭제, cursor 이동 전 확정, 중간 삽입, byte/codepoint 상한, 잘린/잘못된 UTF-8, capacity 실패 시 원본 보존을 검사한다.

**수동 검증:** 실제 두벌식 입력 순서와 삭제 동작이 일반적인 한국어 입력 기대와 일치하는지 사례표를 검토한다.

**종료 조건**

- [X] 모든 편집이 UTF-8 codepoint 경계에서만 발생한다.
- [X] 조합 상태 전이와 mode 전환 규칙이 test로 고정됐다.
- [X] buffer 초과·잘못된 입력이 이전 text를 손상하지 않는다.
- [X] 큰 자동 local·가변 무제한 container·전체 값 snapshot 복사가 없다.

**완료 근거 (2026-11-07)**

- `modules/text/`에 192 UTF-8 byte/64 codepoint 고정 용량 `Editor`, 최대 한 글자만 보유하는 `HangulComposer`, immutable `KeyboardLayoutView`, 조합 수명과 mode/cursor 규칙을 소유하는 `InputModel`을 추가했다. LVGL·플랫폼·번역·저장 정책 의존은 없다.
- UTF-8 validation/encode, ASCII·2/3/4-byte 경계, 중간 삽입·앞/뒤 삭제, 잘린/잘못된 sequence, byte/codepoint 상한과 실패 시 원본/cursor 보존을 test로 고정했다.
- 두벌식 사례 `한`, 복합 중성 `과`, 겹받침 `값`, 모음 입력 시 종성 분리 `갑시`, 조합 중 단계별 backspace, 중간 cursor 삽입과 용량 실패 시 조합 보존이 통과했다. 일반적인 두벌식 조합·삭제 순서와 일치하는지 이 사례표로 검토했다.
- `InputModel` test는 mode 전환과 cursor 이동 전에 조합을 확정하고, locale과 별도인 `KeyboardMode`/SHIFT가 한글·영문·숫자·기호 layout을 선택함을 확인했다.
- Windows `sample-suite` build 후 profile check 10개가 모두 통과했다. GCC 16.1.0으로 `-std=c++17 -Wall -Wextra -Wpedantic` 네이티브 빌드와 생성 assertion 실행도 통과했다.
- 크기 guard는 Windows 기준 `Editor` 최대 224 B, `HangulComposer` 최대 4 B, `InputModel` 최대 232 B다. production에는 아직 자동 local이나 snapshot 복사가 없고, L4 page의 persistent state로 소유할 계약이다. `std::vector`, heap allocation과 무제한 container는 도입하지 않았다.

### L3. 고정/동적 font 자산과 사용자 text renderer

**목표:** 두 언어 고정 문구와 임의의 허용 사용자 입력을 구분된 font 경로로 표시한다.

**범위:** typography generator, 최소 고정 font, 분할 동적 font, manifest, font segment renderer. page 전체 조립은 제외한다.

**산출물:** locale별 고정 glyph 자산, 동적 한글/영문/기호 font block, lookup/segment API, 누락·손상 결과와 test fixture.

**자동 검증:** generator `--dry-run`/생성/`--check`, manifest hash/size/role/charset 검사, 혼합 문자열 segment test, font manager의 상한·누락 glyph test.

**수동 검증:** 대표 한글/영문 혼합 문자열의 baseline, clipping, replacement 표시를 Windows capture에서 확인한다.

**종료 조건**

- [X] 고정 catalog glyph와 동적 입력 glyph가 별도 manifest/lookup을 사용한다.
- [X] font object와 backing data가 이를 사용하는 widget보다 오래 산다.
- [X] 전체 한글 font를 RAM에 eager load하지 않고 필요한 block만 bounded하게 유지한다.

**완료 근거 (2026-11-07)**

- `samples/suite/pages/text_input/i18n/fonts.json`이 Pyeojin Gothic Regular/Bold의 전체 SHA-256, 고정 5개 style의 역할·크기·굵기와 동적 24px/4bpp 5개 block을 선언한다. 고정 생성물은 `fixed_font_manifest.json`, 동적 생성물은 `user_font_manifest.json`으로 서로 분리됐다.
- 고정 catalog glyph는 role별 최소 C font 5개로 생성됐고, 동적 입력은 common 9,752 B와 한글 4개 block 366,316/378,784/424,736/402,212 B로 생성됐다. 두 생성기의 dry-run → 생성 → check가 통과했고 manifest가 각 파일의 size와 SHA-256을 기록한다.
- `FontRepository`는 전체 글꼴을 시작 시 읽지 않고 조회된 block만 page 수명 동안 최대 5개 유지한다. Windows 64-bit 객체 크기는 160 B이며 bitmap은 객체나 task stack에 복사하지 않는다. `UserTextLabel`은 최대 192 UTF-8 byte를 최대 64개 font segment로 나누고 누락 glyph를 `□`로 명시한다.
- repository lazy load/재사용/누락/load 실패/reset test와 mixed label/invalid UTF-8 test가 통과했다. `python tools/project.py check --host windows --profile sample-suite`의 관련 13개 test도 모두 통과했다.
- `build/windows-sample-suite/l3-font-render.bmp`에서 한국어/영어 고정 문구, `한글 ABC 123, 값` 혼합 문자열의 baseline과 clipping, emoji·화살표의 `□` 대체 표시를 확인했다. HMI SD driver와 실제 LCD는 L5 수동 게이트로 남겼다.

### L4. `text-input` page와 런타임 한·영 전환

**목표:** 사용자가 같은 page에서 화면 언어와 키보드 입력 mode를 독립적으로 전환하고 편집 결과를 확인한다.

**범위:** page 등록, locale toggle, 이름/메모/값 field, 화상 키보드, cursor, 완료/초기화, page 재진입 상태, Windows pointer/keyboard test.

**산출물:** `text-input` descriptor/page, generated catalog/font 연결, app locale ownership, navigation/state/interaction/capture test.

**자동 검증:** Windows `sample-suite` build/check, Dashboard→page→locale 전환→입력→Dashboard→재진입 흐름, 한국어/영어 모든 고정 label 갱신, 키보드 mode와 locale의 독립성, page 파괴 후 callback/timer/widget 0건을 확인한다.

**수동 검증:** 두 locale의 제목·placeholder·기능 키·오류 문구가 잘리지 않는지, 한글 조합과 중간 편집이 pointer 입력으로 자연스러운지 확인한다.

**종료 조건**

- [X] locale 전환이 화면 tree를 복제하지 않고 모든 고정 문구에 적용된다.
- [X] 한/영 키보드 mode가 화면 locale과 독립적으로 동작한다.
- [X] page 재진입 시 locale은 유지되고 미완성 조합/입력 lifetime 규칙이 문서와 일치한다.
- [X] Dashboard와 기존 `ui-basics`가 외부 자산 없이 계속 동작한다.

**완료 근거 (2026-11-07)**

- descriptor 기반 `TextInputPage`가 이름/메모/값 field, 한글·영문·숫자·기호 화상 키보드, cursor, 완료·삭제·초기화와 locale/theme control을 조립한다. locale은 `SampleSuiteApp`, 편집 buffer와 조합은 page가 소유한다.
- 화면 locale을 영어로 바꿔도 keyboard mode가 한글로 유지되는 상태 test와, Dashboard 이탈 후 재진입 시 locale은 영어로 유지되지만 입력·cursor·조합은 초기화되는 interaction test가 통과했다.
- SDL `TEXTINPUT`과 Backspace/Enter/좌우 cursor를 공통 app/page 계약으로 전달한다. generic suite composition은 특정 page 타입이나 ID를 참조하지 않으며 isolation test가 이를 확인한다.
- Windows `sample-suite` 17/17, `main` 2/2, Python generator/manifest 35/35가 통과했다. `build/windows-sample-suite/l4-text-input.ko.bmp`와 `l4-text-input.en.bmp`에서 두 locale의 제목·설명·field·기능 키·길이 표시와 혼합 입력 cursor가 잘리지 않음을 확인했다.
- `text-input` page를 열지 않는 Dashboard와 `ui-basics`, 자산 선언이 없는 `main`은 외부 font load를 호출하지 않는다.

### L5. HMI 통합과 후속 범위 결정

**목표:** 같은 page/engine이 HMI firmware에 포함되고, locale 수명과 선택적 영구 저장의 경계를 고정한다.

**범위:** HMI `sample-suite` build, source/asset manifest, RAM/flash/stack/lifetime 검토, 사용자 실기기 확인 절차. Codex upload와 locale 영구 저장 구현은 제외한다.

**산출물:** firmware build 근거, Windows 대비 source parity, 메모리 사용량, LCD/touch/SD font 수동 절차, profile-scoped USB asset updater, locale 수명 범위 결정.

**자동 검증:** HMI `sample-suite` build, isolation check, UI/text/i18n의 Arduino·FreeRTOS·SDL·Board 내부 의존 0건, asset bundle/hash/전송 protocol test, firmware RAM/flash와 주요 객체 `sizeof` 기록.

**수동 검증:** 사용자가 업로드한 장치에서 USB asset 전송, 두 locale, touch keyboard, 조합·cursor·삭제, 반복 page 이동, SD font 누락/복구와 serial log를 확인한다.

**종료 조건**

- [X] Windows와 HMI가 같은 i18n/text/page 공통 source를 사용한다.
- [X] RAM/flash/stack과 font cache 상한이 허용 범위로 기록됐다.
- [X] USB asset bundle·hash·bounded transfer·profile 격리의 자동 검증이 통과했다.
- [X] 사용자가 실제 LCD·touch·SD font 동작을 확인했다.
- [X] 사용자가 실행 중인 HMI에 USB로 asset을 전송하고 적용을 확인했다.
- [X] locale은 현재 app 수명 동안만 유지하며 별도 Settings page를 만들지 않는 범위가 기록됐다.

**진행 근거 (2026-11-08, 자동 범위 완료)**

- JC1060P470C Board가 SDMMC 4-bit 핀(43/44/39~42)과 `/sdcard` mount 수명을 소유한다. LDO4와 SD 전원 GPIO는 선택된 Arduino esp32p4 보드 variant의 `SD_MMC`가 단독 소유한다. 공개 facade는 `begin_storage()`/`storage_ready()`만 제공하며 UI/page는 Board나 SD API를 참조하지 않는다. 자산이 없는 `main`은 생성된 `kSelectedHasAssets == false`로 storage 초기화를 호출하지 않는다.
- HMI host는 생성된 `A:/sdcard/hmi/sample-suite/` root와 공통 `BinaryFontPort`로 LVGL binary font를 읽는다. Windows는 같은 상대 asset path를 저장소 root에서 읽으므로 i18n/text/page source는 두 host에서 동일하다.
- `python tools/project.py bundle-assets --profile sample-suite`가 5개 font(총 1,581,800 B)와 SHA-256 manifest를 `dist/sd/hmi/sample-suite/`에 만든다. 전송 bundle은 manifest를 포함해 6개 파일, 1,583,307 B이며 dry-run 검증이 통과했다.
- 실행 중인 HMI가 `ASSET_V2` USB CDC protocol로 `/hmi/sample-suite.__incoming`에 수신하고 file SHA-256 검증 뒤 journal/backup으로 `/hmi/sample-suite`를 교체하도록 구현했다. profile ID 불일치, 경로 탈출, 과대/잘못된 offset과 중단 재시도를 거부·복구하며 `main` build에는 updater source가 포함되지 않는다.
- Python generator/asset 전송 검증 40건이 통과했고, upload 없이 HMI `main`과 `sample-suite` firmware build가 모두 통과했다. SD mount 실패 중에도 CDC receiver가 `storage_unavailable`을 반환한다. 실기기 오류 추적에서 Board의 수동 LDO4 취득과 esp32p4 variant의 `SD_MMC` LDO4 취득이 충돌함을 확인해 수동 취득을 제거하고 40/20/10/4MHz mount fallback을 이식했다. 최종 `sample-suite`는 RAM 28,988/512,000 B(5.7%), flash 804,754/6,553,600 B(12.3%)이며 `main`은 RAM 28,064 B(5.5%), flash 662,660 B(10.1%)다.
- HMI ELF debug type 기준 `SampleSuiteApp` 260 B, `TextInputPage` 1,264 B, `InputModel` 216 B, `FontRepository` 80 B, `Board` 308 B다. Board/App은 정적 persistent, page는 heap 소유이며 font bitmap은 LVGL allocator가 PSRAM 우선 heap에 lazy load한다. font cache는 최대 5개이고 host font path 자동 배열은 320 B로 제한된다.
- 공통 `modules/i18n`, `modules/text`, `modules/ui`, `text-input` page의 SDL·Arduino·FreeRTOS·ESP·Board 내부 의존 검색은 0건이다. Board의 widget 생성도 0건이고 LVGL pump는 계속 `Board::tick()` 아래에 있다. public Board header의 internal/Arduino include 결합은 기존 구조의 compile-time encapsulation 부채이며 이번 behavioral facade를 우회하지 않는다.

**사용자 수동 승인 근거 (2026-11-08):** 사용자가 계획 중 `text-input` 관련 범위를 모두
완료로 판정했다. 이에 따라 실제 LCD·touch·SD font·USB asset 적용과 반복 동작 수동 gate를
통과한 것으로 기록하고 L5와 전체 계획을 `complete`로 전환했다.

**Locale 영구 저장 후속 결정**

- 현재 locale/theme control과 app-owned 상태가 이미 구현·승인됐으므로 이를 복제하는
  Settings page는 만들지 않는다.
- locale은 app 수명 동안 유지하고 재부팅 후 `ko-KR`로 돌아가는 현재 계약을 유지한다.
- 영구 저장이 실제 요구가 되면 app 조립 경계에 좁은 port와 host adapter를 추가하되
  `modules/i18n`, Dashboard UI, Board가 NVS를 직접 참조하지 않게 한다.

## 6. 예상 변경 격리

| 후속 변경 | 바뀌어야 하는 위치 | 바뀌지 않아야 하는 위치 |
| --- | --- | --- |
| 세 번째 locale 추가 | 새 catalog/typography 입력, locale registry, 생성 manifest | editor/composer, Board/LvglPort, 기존 page layout 내부 |
| 새 키보드 layout 추가 | `modules/text` layout data와 해당 test | i18n catalog lookup, host, Board |
| locale 영구 저장 요구 추가 | app 조립의 좁은 storage port와 host adapter | 새 Settings page, composer, font renderer, Dashboard control 복제 |
| password field 추가 | 재사용 입력 widget의 masking 정책과 Wi-Fi page 조립 | 저장 암호화 정책, locale engine, Board |
| 동적 glyph block 변경 | font generator/config/manifest와 font lookup test | 고정 catalog key, editor state machine |

## 7. 위험과 대응

| 위험 | 대응 |
| --- | --- |
| 화면 locale과 키보드 한/영 mode를 같은 상태로 취급 | `Locale`과 `KeyboardMode`를 별도 enum/owner로 두고 교차 조합 test를 둔다. |
| 번역 누락을 runtime fallback이 숨김 | key/placeholder/role parity를 생성 실패로 처리한다. |
| 고정 문구를 동적 glyph 목록으로만 해결 | catalog와 typography role 등록을 필수로 검사한다. |
| 전체 한글 font 또는 catalog 복사로 RAM 증가 | 분할 font, immutable catalog view, bounded cache와 객체 크기 guard를 사용한다. |
| 조합 중 cursor/delete에서 UTF-8 손상 | 순수 editor/composer state test와 capacity 실패 원본 보존 test를 둔다. |
| page 추가 때문에 Dashboard/Board에 분기 추가 | descriptor 등록과 page-owned source/asset만 변경하고 isolation 검색을 유지한다. |
| Windows 성공을 LCD/touch/font 성공으로 오인 | HMI build와 사용자 실기기 manual gate를 분리한다. |

## Automated Verification Checklist

- [X] 두 locale catalog key·placeholder·typography role parity 검사가 통과한다.
- [X] typography generator의 `--dry-run`, 생성, `--check`가 순서대로 통과한다.
- [X] UTF-8 editor·두벌식 composer·keyboard layout 단위 test가 정상/경계/거부 사례를 통과한다.
- [X] 동적 font segment·누락 glyph·bounded cache test가 통과한다.
- [X] `python tools/project.py build --host windows --profile sample-suite`가 통과한다.
- [X] `python tools/project.py check --host windows --profile sample-suite`가 locale/input/navigation 회귀를 포함해 통과한다.
- [X] `python tools/project.py build --host hmi --profile sample-suite`가 upload 없이 통과한다.
- [X] `python tools/project.py check --scope isolation`이 두 profile과 두 host에서 통과한다.
- [X] 공통 i18n/text/page source의 SDL·Arduino·FreeRTOS·ESP·Board 내부 의존이 0건이다.
- [X] firmware RAM/flash, editor/page/font cache 크기와 persistent/heap/stack lifetime이 기록됐다.

## Manual Verification Checklist

- [X] 한국어/영어 고정 문구의 의미·용어·버튼 행동이 검토됐다.
- [X] Windows에서 두 locale의 제목·placeholder·기능 키·오류·길이 표시가 잘리지 않는다.
- [X] Windows pointer 입력으로 한글 조합·삭제·cursor 이동·중간 삽입·완료가 기대대로 동작한다.
- [X] 사용자가 firmware를 직접 업로드한 뒤 실제 LCD에서 두 locale과 혼합 사용자 text를 확인했다.
- [X] 실제 touch에서 작은 문자 키와 기능 키가 구분되고 반복 입력 시 누락·중복이 없다.
- [X] SD font 정상/누락/손상 조건과 복구 문구가 확인됐다.
- [X] USB asset updater로 font bundle을 전송하고 Dashboard 복귀 후 새 font가 로드된다.
- [X] 반복 page 진입·locale 전환·keyboard mode 전환 뒤 reset, 멈춤, allocation 오류가 없다.
- [X] locale은 현재 인메모리이며 별도 Settings page를 정식 범위에서 제거했음을 문서와 UI lifetime test로 확인했다.
