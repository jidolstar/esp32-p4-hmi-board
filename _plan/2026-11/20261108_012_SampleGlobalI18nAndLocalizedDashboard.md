# Sample 전역 i18n·자산과 다국어 Dashboard 계획

작성: 2026-11-08. 상태: `complete` (P1~P5 `complete`; 2026-11-08 사용자 승인).

## 1. 목적과 결정

`sample-suite`의 i18n과 글꼴 자산 소유권을 `Text Input` page에서
`samples/suite/` 전역으로 올리고, Dashboard에서도 한국어/영어를 선택할 수 있게 한다.
화면 고정 문구는 C++ 문자열이 아니라 catalog key로 관리하며 Windows와 HMI가 같은
locale 상태, catalog, font와 화면 코드를 사용한다.

도구는 sample이나 `main`에 종속되지 않는다. `tools/assets/typography/`는 명시적인 설정
파일 또는 입력·출력 인자를 받아 어느 app/sample에서도 사용할 수 있어야 한다. profile별
선택과 편의 명령은 profile 설정 또는 profile 접두어를 가진 얇은 wrapper가 맡는다.

핵심 결정은 다음과 같다.

1. sample 전역 catalog·typography·font 설정은 `samples/suite/i18n/`이 소유한다.
2. sample 전역 runtime font 자산은 `samples/suite/assets/`, 생성 C++/manifest/font 산출물은
   `samples/suite/generated/`가 소유한다.
3. `modules/i18n/`은 `Locale`, catalog view, format 같은 도메인 중립 계약만 유지한다.
4. Dashboard, page descriptor, theme/locale control과 기존 sample page 고정 문구는 하나의
   suite catalog를 사용한다. page 구현은 다른 page를 참조하지 않는다.
5. `SampleSuiteApp`이 현재처럼 작은 `Locale` 값 하나를 소유하고 Dashboard와 page에
   전달한다. catalog 전체를 app 객체에 복사하지 않는다.
6. page registry에는 표시 문구 대신 `title_key`와 `description_key`를 저장한다.
   registry 생성 도구는 특정 catalog namespace나 page 구현을 알지 않는다.
7. 고정 한국어/영어 글꼴은 firmware에 포함해 Dashboard 진입이 SD 동적 글꼴에
   의존하지 않게 한다. 사용자 입력용 분할 font만 runtime asset으로 유지한다.
8. locale 영구 저장은 이번 범위에서 제외한다. 기본값은 `ko-KR`이고 선택은 app 수명 동안
   Dashboard↔page 이동과 theme 재구성 뒤에도 유지한다.
9. asset update/recovery UI는 외부 자산 없이 동작해야 하므로 내장 영어 ASCII 경로를
   유지한다.

## 2. 선행 정리

다음 profile wrapper 이름은 이 계획 작성과 함께 이미 정리했다.

| Profile | Windows 실행 | HMI build | 자산 갱신 |
| --- | --- | --- | --- |
| `main` | `main_run_windows.bat` | `main_build_hmi.bat` | 없음 |
| `sample-suite` | `samples_run_windows.bat` | `samples_build_hmi.bat` | `samples_update_hmi_assets.bat` |

각 batch는 `tools/project.py`에 명시적인 `--profile`을 전달하는 얇은 wrapper다.
중립 도구 내부에는 batch 이름이나 특정 profile 기본값을 추가하지 않는다.

관련 저장소 skill도 현재 profile/app/sample 구조를 먼저 탐색하도록 갱신했다.

- `.agents/skills/ui-text-policy/SKILL.md`
- `.agents/skills/font-management-guide/SKILL.md`
- `.agents/skills/cross-platform-ui-development/SKILL.md`
- `.agents/skills/change-scoped-verification/SKILL.md`

## 3. 현재 상태와 누락 근거

- 실제 locale catalog와 font 설정은
  `samples/suite/pages/text_input/i18n/`에만 있다.
- catalog·고정 font·user font 생성기도
  `samples/suite/pages/text_input/` 경로와 page 전용 C++ namespace/symbol을 전제로 한다.
- `samples/suite/profile.json`은 `pages/text_input/assets/fonts`와 page-local config/manifest를
  직접 선언한다.
- `samples/suite/page_registry.json`의 `title`과 `description`은 영어 표시 문자열이다.
  `tools/hmi_project/generate.py`가 이를 그대로 `PageDescriptor`에 넣는다.
- `samples/suite/dashboard/dashboard_page.cpp`는 제목, 설명, section, `OPEN`과
  theme caption을 직접 영어로 만든다. Dashboard create 계약에는 locale과 locale 변경
  callback이 없다.
- `modules/ui/src/theme.cpp`의 `UiTheme::toggle_label()`도 영어 문구를 반환하므로 재사용
  theme 모듈이 표시 문구를 소유하고 있다.
- `SampleSuiteApp::locale_`, `PageContext::locale`, `set_locale` callback은 이미 있어
  locale 수명과 page 전달 기반은 존재한다.
- `Text Input`은 생성된 `TextId`를 사용하지만 header의 asset 누락 경로에는 내장 영어
  문구가 있다. 이 경로는 recovery 성격이므로 외부 fixed font를 요구하지 않는다.

## 4. 목표 구조와 책임 경계

```text
samples/suite/
  i18n/
    catalog.json          # locale 입력, namespace/symbol, output을 선언하는 owner 설정
    ko.yaml
    en.yaml
    typography.yaml
    fonts.json
  assets/
    fonts/                # runtime 사용자 입력 font
  generated/
    text_catalog.hpp
    fixed_fonts.hpp
    catalog_manifest.json
    fixed_font_manifest.json
    user_font_manifest.*
    fonts/fixed/*.c
  dashboard/
  pages/
    text_input/           # page 코드와 page 전용 test
    ui_basics/
  app.*
  page_contract.hpp
  page_registry.json
  profile.json
```

`catalog.json`의 정확한 schema는 P1에서 test와 함께 고정한다. 최소한 다음 정보를
표현해야 한다.

- catalog owner ID와 C++ namespace/symbol prefix
- locale tag별 YAML 입력
- typography와 font 설정 입력
- fixed/user output directory
- manifest 이름과 runtime asset 상대 경로

도구가 설정 파일 위치에서 상대 경로를 해석하게 하며 저장소의 `samples` 또는 `apps`
폴더 형태를 알지 않게 한다. `--config <path>`를 공통 진입점으로 사용하고
`--dry-run`, generate, `--check` 동작은 유지한다.

### 4.1 Catalog key

기존 page-local key는 의미가 드러나도록 namespace를 정리한다.

```text
common.back
common.open
theme.light
theme.dark
locale.label
locale.ko
locale.en
dashboard.eyebrow
dashboard.title
dashboard.description
dashboard.section_title
page.ui_basics.title
page.ui_basics.description
page.text_input.title
page.text_input.description
text_input.field.name
text_input.keyboard.done
text_input.review.title
ui_basics.list.title
```

동일한 행동과 의미만 `common.*`으로 공유한다. keyboard 문자 key와 실제 입력값은
catalog에 넣지 않는다.

### 4.2 Page registry

`page_registry.json`의 표시 문자열은 다음처럼 key가 된다.

```json
{
  "id": "text-input",
  "title_key": "page.text_input.title",
  "description_key": "page.text_input.description"
}
```

`PageDescriptor`는 key를 보존하고 Dashboard가 현재 suite catalog로 조회한다. registry
schema/parser/generator는 key 문법만 검증한다. 실제 key 존재 여부는 suite catalog 검증과
profile check에서 교차 검사해 누락을 build 전에 실패시킨다.

### 4.3 Runtime locale 흐름

```text
Dashboard locale button
  -> SampleSuiteApp::set_locale(locale)
  -> app-owned locale_ 갱신
  -> 현재 view의 fixed text 갱신 또는 상태 보존 재구성
  -> Dashboard와 모든 page가 같은 locale을 관찰
```

theme 전환도 동일한 `locale_`를 보존한다. 화면 언어와 `Text Input`의 keyboard mode는
독립 상태로 유지한다.

## 5. 단계 계획

| 단계 | 상태 | 결과 |
| --- | --- | --- |
| P1 | `complete` | 경로·namespace 하드코딩이 없는 config 기반 typography 도구 |
| P2 | `complete` | suite 전역 i18n/assets/generated 소유권과 기존 Text Input 연결 |
| P3 | `complete` | key 기반 page registry와 다국어 Dashboard/locale control |
| P4 | `complete` | 기존 sample page의 suite catalog 통합과 직접 문구 정리 |
| P5 | `complete` | Windows/HMI 통합·bundle·문서와 실제 HMI 수용 완료 |

### P1. Typography 도구 중립화

**목표:** typography/catalog/user-font 도구가 설정 경로만으로 임의의 app/sample owner를
처리하며 특정 profile/page 경로와 C++ namespace를 내장하지 않는다.

**범위:** `tools/assets/typography/`, `tools/tests/`, 도구 README와 새 owner 설정 schema.
UI, catalog 이동, page registry는 제외한다.

**산출물:** `--config` 계약, 중립 config loader, 입력/출력/namespace/symbol 검증,
임시 디렉터리 fixture를 사용하는 회귀 test, sample과 향후 app 사용 예시.

**자동 검증:** generator unit test와 전체 `tools/tests`를 실행한다. Python 실행 코드의
특정 sample/page 경로 검색 결과가 0건이고, 서로 다른 두 임시 owner config가 각자의
namespace/output으로 결정적 산출물을 만드는지 확인한다.

**수동 검증:** 새 README의 명령과 ownership 설명이 sample과 app 어느 한쪽을 기본
제품으로 전제하지 않는지 검토한다.

**종료 조건**

- [x] `generate.py`와 `generate_user.py`가 `--config`로 owner를 선택한다.
- [x] reusable Python 코드에 `samples/suite`, `apps/main`, `text_input` 경로가 없다.
- [x] namespace, symbol prefix, 입력, output의 누락·탈출·충돌이 validation error다.
- [x] 두 locale key·placeholder·role parity와 stale output 검사가 유지된다.
- [x] 임의의 두 owner fixture가 서로의 파일이나 symbol을 오염시키지 않는다.

**의존:** 없음.

**P1 검증 근거 (2026-11-08):** `catalog.json` owner 계약과 bounded path 검증을
`tools/assets/typography/owner_config.py`에 추가하고 두 generator가 필수 `--config`를
사용하도록 변경했다. 두 임시 owner의 namespace·symbol·output 격리, path escape,
namespace와 output 충돌 test를 포함한 typography/user-font test 12개가 통과했다.
`python -m unittest discover -s tools/tests -v`는 45개 전부 통과했고, typography Python
실행 코드에서 `samples/suite`, `apps/main`, `text_input` 검색 결과는 0건이었다. README의
owner config와 app/sample 사용 설명도 특정 제품을 전제하지 않음을 검토했다.

### P2. Sample 전역 i18n·assets 이동

**목표:** 기존 `Text Input` catalog와 font 자산이 `samples/suite/` 소유로 이동하고 기능과
생성 결과가 동일하게 유지된다.

**범위:** `samples/suite/i18n/`, `samples/suite/assets/`,
`samples/suite/generated/`, `samples/suite/profile.json`, `samples/suite/CMakeLists.txt`,
`samples/suite/page_registry.json`의 source 선언과 Text Input include/test 경로.

**산출물:** suite owner config, 이동된 원본·생성물·runtime fonts, profile-level fixed font
source 연결, page-local 생성물/asset 제거, 갱신된 manifest와 test include.

**자동 검증:** 새 config로 dry-run→generate→check, user-font check, profile manifest 검사,
Windows `sample-suite` check와 isolation check를 실행한다. 생성 후 이전 page-local
i18n/assets/generated 경로에 소유 파일이 남지 않는지 확인한다.

**수동 검증:** Text Input의 한국어/영어 fixed 문구, 혼합 사용자 입력, caret와 keyboard가
이동 전과 같은지 Windows에서 확인한다.

**종료 조건**

- [x] suite i18n/assets/generated 디렉터리가 sample 전역 원본의 단일 소유자다.
- [x] fixed font C source는 profile 공통 source이고 특정 page source 목록에 속하지 않는다.
- [x] user font runtime asset은 `samples/suite/assets/fonts/`에서 bundle된다.
- [x] Text Input catalog/font/state/interaction test가 새 경로로 통과한다.
- [x] `main` profile source·config·assets에는 sample 파일이 들어가지 않는다.

**의존:** P1 `complete`.

**P2 검증 근거 (2026-11-08):** catalog·typography·font 원본을
`samples/suite/i18n/`, runtime font를 `samples/suite/assets/fonts/`, 생성 C++/font
산출물을 `samples/suite/generated/`로 이동했다. fixed font C source는 profile
공통 source로, Text Input page source는 page 구현만 남기도록 정리했다. 새
config의 fixed/user generator dry-run·generate·check가 통과했고, Windows
`sample-suite` 20개 test가 모두 통과했다. 이전 page-local 소유 경로 검색
결과는 0건이며 Windows/HMI의 `main`·`sample-suite` 생성 입력을 다시 준비한
후 isolation check도 통과했다.

### P3. Key 기반 registry와 다국어 Dashboard

**목표:** Dashboard에서 한국어/영어를 선택하고 모든 Dashboard 고정 문구와 page card가
현재 locale에 맞게 표시된다.

**범위:** page registry schema/parser/generator, `PageDescriptor`, Dashboard, suite app locale
전환 계약, suite catalog의 dashboard/page keys와 fixed font roles.

**산출물:** `title_key`/`description_key`, key 존재 교차 검사, Dashboard locale control,
catalog 기반 theme caption과 card text, locale/theme/navigation 회귀 test.

**자동 검증:** registry schema와 generator test, Dashboard 한국어/영어 interaction test,
theme 전환 후 locale 유지, page 왕복 후 locale 유지, 알 수 없는 catalog key 거부를
검증한다. Windows `sample-suite` check와 isolation check를 실행한다.

**수동 검증:** 1024×600 Windows 화면에서 두 언어의 header, locale/theme control,
section과 page card가 겹치거나 잘리지 않고 현재 선택이 식별되는지 확인한다.

**종료 조건**

- [x] page registry에 사용자 표시용 원문 `title`/`description`이 없다.
- [x] Dashboard C++에 catalog 대상 고정 문구가 직접 입력되지 않는다.
- [x] Dashboard에서 locale 선택 즉시 header, control과 모든 card 문구가 바뀐다.
- [x] theme 전환과 Dashboard↔page 이동 뒤 locale이 유지된다.
- [x] `UiTheme`은 theme caption을 소유하지 않고 mode/style만 소유한다.

**의존:** P2 `complete`.

**P3 검증 근거 (2026-11-08):** page registry schema를 `title_key`/
`description_key`로 변경하고 등록 key가 생성된 catalog manifest에 없으면
`unknown-page-catalog-key`로 거부하도록 했다. Dashboard header·locale/theme control·
section·card·Open 문구는 모두 suite catalog와 firmware fixed font를 사용한다.
한국어→영어 즉시 전환, page 왕복, theme 전환, 영어→한국어 복귀 후에도
locale와 theme가 유지되는 interaction test가 통과했다. registry·catalog·
navigation·state·interaction 직접 test 4개와 Windows profile 20개 test, tools 45개
test, four-profile isolation check가 통과했고 1024×600 한국어/영어 두 mode를
capture로 검토했다.

### P4. 기존 sample page 전역 locale 통합

**목표:** 기존 sample page가 suite 전역 catalog와 app locale을 일관되게 사용하고 한 page만
다국어인 상태를 없앤다.

**범위:** `Text Input`, `UI Basics`, 공통 sample widget caption과 관련 test. recovery/asset
update 영어 ASCII와 동적 사용자 데이터는 제외한다.

**산출물:** page namespace key, suite generated `TextId` 사용, locale 변경 시 현재 page의
fixed text 갱신, 직접 표시 문자열 audit와 허용 예외 목록.

**자동 검증:** `lv_label_set_text`, widget helper, placeholder, formatting과 descriptor를
검색해 허용된 recovery·동적 데이터 외 fixed 문자열이 없는지 확인한다. 두 locale page
state/interaction test와 Windows `sample-suite` check를 실행한다.

**수동 검증:** 두 언어에서 `UI Basics`와 `Text Input`의 버튼, 설명, status, modal,
placeholder, keyboard action caption의 줄바꿈·잘림·행동 일치를 확인한다.

**종료 조건**

- [x] 기존 두 page의 고정 문구가 suite catalog key로 관리된다.
- [x] locale 변경이 현재 page의 입력/state를 잃지 않고 fixed text만 갱신한다.
- [x] keyboard mode와 화면 locale의 독립성이 유지된다.
- [x] recovery/asset update와 동적 데이터 외 직접 표시 문자열이 audit에 남지 않는다.

**의존:** P3 `complete`.

**P4 검증 근거 (2026-11-08):** `UI Basics`의 header·control·status format·
scroll list·toast·dialog를 suite catalog에 포함했고 `Text Input`의 page 전용
key를 `text_input.*`로 namespace화했다. catalog은 두 locale/role/placeholder 동일성을
가진 72개 key로 생성·check되며 user font 176개 asset check도 통과했다.
UI Basics의 press/long-press state가 theme widget 재생성 후 유지되고, 영어/한국어
버튼·list·toast·dialog 행동이 같은 interaction test에서 통과했다. recovery
ASCII 문구와 동적 데이터를 제외한 widget helper/라벨 직접 문자열 검색은
0건이고, Windows profile 20개 test와 tools 45개 test가 모두 통과했다.

### P5. 통합·bundle·인계

**목표:** suite 전역 i18n/assets가 Windows와 HMI build, SD bundle과 문서에서 같은 profile
입력으로 재현되고 실제 장치 확인 절차가 준비된다.

**범위:** sample profile 전체 검사, `hmi-sample-suite` compile, asset bundle, 시작 문서,
batch wrapper 문서와 최종 manifest/크기 확인. upload와 실제 장치 변경은 제외한다.

**산출물:** Windows/HMI build 근거, suite asset manifest/bundle, 문서화된 생성·실행·build·
asset-update 명령, Dashboard ko/en 캡처와 실기기 checklist.

**자동 검증:** `sample-suite` Windows check, isolation, typography check, asset bundle,
`python tools/project.py build --host hmi --profile sample-suite`, old batch/path 검색과
`git diff --check`를 실행한다. `main`은 isolation과 도구 fixture로 sample 비의존성을
확인하고, 공통 API가 바뀐 경우에만 `main` 관련 build/check를 추가한다.

**수동 검증:** 사용자가 firmware를 업로드하고 sample asset bundle을 전송한 뒤 실제 LCD의
Dashboard locale control, page card, page 이동, theme 전환, 동적 입력 font와 recovery 화면을
확인한다.

**종료 조건**

- [x] Windows sample profile의 등록 test가 모두 통과한다.
- [x] HMI sample profile이 upload 없이 compile된다.
- [x] bundle의 destination과 파일이 `samples/suite/profile.json` 선언과 일치한다.
- [x] 시작 문서가 새 batch 이름과 config 기반 generator 명령만 사용한다.
- [x] 실제 HMI의 locale/theme/page/asset 동작을 사용자가 승인한다.

**의존:** P4 `complete`.

**P5 자동 검증 근거 (2026-11-08):** Windows `sample-suite` 등록 test 20개와
`main` 등록 test 2개가 통과했다. HMI는 upload 없이 `main`과 `sample-suite`를 모두
compile했고, 각각 RAM/Flash 5.5%/10.1%, 5.7%/13.0%를 사용했다. 자산이 없는
`main` HMI가 sample 자산 UI에 의존하던 host 경계를 `HMI_PROFILE_HAS_ASSETS` 조건부
compile로 정리한 뒤 두 profile build를 다시 통과시켰다. asset bundle은
`hmi/sample-suite` 아래 `assets/fonts/*` 176개로 생성됐고 선언 밖 path는 0건이었다.
catalog 72개와 user font 176개 check, tools test 45개, 네 profile isolation, 네 skill
validation이 모두 통과했다. 정확한 이전 batch 이름 검색은 0건이며 `git diff --check`도
성공했다. 현재 저장소 파일은 Git에서 전부 untracked 상태여서 diff 기반 검사는 제한적이다.
실제 장치 upload·asset 전송·LCD 확인은 수동 게이트로 남겼으며, 2026-11-08 사용자가 계획
중 `text-input` 관련 범위를 모두 완료로 판정해 suite 전역 i18n/assets와 Dashboard UX를
승인한 것으로 기록했다. 이에 따라 P5와 전체 계획을 `complete`로 전환했다.

## 6. 변경 격리

| 후속 변경 | 바뀌어야 하는 위치 | 바뀌지 않아야 하는 위치 |
| --- | --- | --- |
| 새 sample locale 추가 | suite locale YAML/config, 생성물, layout test | Board, host, text editor |
| 새 sample page 추가 | page 폴더, registry key, suite catalog | 기존 page 내부, `apps/main/` |
| `main` 다국어화 | `apps/main/i18n/` owner config와 app UI | sample catalog/asset |
| 새 font owner 추가 | owner config와 profile source/config/assets | generator Python의 owner 경로 로직 |
| locale 영구 저장 요구 추가 | app composition의 좁은 read/write port와 host adapter | 새 Settings page, catalog, keyboard composer, Board UI |
| 새 board 추가 | board/host/profile 조립 | suite catalog와 Dashboard |

## 7. 위험과 대응

| 위험 | 대응 |
| --- | --- |
| catalog 이동 중 generated include/symbol 충돌 | P1에서 namespace/symbol을 owner config로 검증하고 P2를 이동만 하는 독립 단계로 둔다. |
| Dashboard가 SD font에 의존해 자산 누락 시 부팅 불가 | Dashboard fixed font는 firmware C source로 유지하고 runtime user font와 분리한다. |
| page registry key 오타가 빈 문구로 표시 | catalog와 registry 교차 검사를 profile check에 넣어 build 전에 실패시킨다. |
| locale 전환이 page 입력/state를 초기화 | app-owned locale과 기존 page 객체/state 보존 계약을 회귀 test로 고정한다. |
| 번역 길이로 header/card/button이 잘림 | Windows 두 locale capture와 실제 LCD manual gate를 둔다. |
| 도구가 다시 sample 경로를 내장 | arbitrary owner fixture와 Python source boundary 검색을 P1 종료 조건으로 둔다. |
| `main` build에 sample source/asset이 혼입 | profile isolation check와 resolved input 검사를 유지한다. |
| 과거 batch 이름이 문서에 잔존 | 저장소 전역 old-name 검색을 P5 자동 게이트에 둔다. |

## Automated Verification Checklist

- [x] `python -m unittest discover -s tools/tests -v`가 성공한다.
- [x] typography Python 실행 코드에 특정 app/sample/page 경로 하드코딩이 없다.
- [x] 새 suite config의 catalog/fixed/user generator dry-run→generate→check가 성공한다.
- [x] page-local i18n/assets/generated 소유 파일이 suite 전역 경로로 완전히 이동한다.
- [x] registry의 모든 title/description key가 두 locale catalog에 존재한다.
- [x] `python tools/project.py check --host windows --profile sample-suite`가 성공한다.
- [x] `python tools/project.py check --scope isolation`이 성공한다.
- [x] locale/theme/navigation과 두 page의 직접 관련 test가 성공한다.
- [x] `python tools/project.py bundle-assets --profile sample-suite`가 성공한다.
- [x] `python tools/project.py build --host hmi --profile sample-suite`가 upload 없이 성공한다.
- [x] `rg` old batch 이름 검색 결과가 0건이다.
- [x] 네 개 변경 skill이 `quick_validate.py`를 통과한다.
- [x] `git diff --check`가 성공한다.

## Manual Verification Checklist

- [x] Windows Dashboard에서 한국어/영어 선택이 즉시 전체 fixed 문구에 적용된다.
- [x] locale 선택 상태가 theme 전환과 Dashboard↔page 왕복 뒤에도 유지된다.
- [x] 두 언어의 Dashboard header, control과 page card가 1024×600에서 잘리지 않는다.
- [x] `UI Basics`와 `Text Input`의 두 언어 문구·버튼 행동·modal·placeholder가 일치한다.
- [x] 화면 locale과 keyboard mode를 독립적으로 조합할 수 있다.
- [x] 실제 HMI에서 fixed 한국어/영어가 SD user font 없이 Dashboard에 표시된다.
- [x] 실제 HMI에서 sample asset 전송 후 동적 사용자 입력 font가 정상 표시된다.
- [x] asset 누락/갱신 recovery UI가 외부 catalog 없이 계속 표시된다.
- [x] 사용자가 sample 전역 i18n/assets 소유 구조와 Dashboard UX를 승인한다.
