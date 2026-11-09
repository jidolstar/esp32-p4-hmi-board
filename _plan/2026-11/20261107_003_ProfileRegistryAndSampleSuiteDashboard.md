# 범용 HMI 시작 프로젝트 3차 계획: 프로필 Registry와 Sample Suite Dashboard

작성: 2026-11-07. 상태: `complete` (P1~P6 `complete`, P6 실기기 확인은 사용자 승인으로 G4 수용 게이트에 이관, 2차 계획 H6 `complete`).

상위 목적과 전체 기능 범위는 `_plan/2026-11/20261106_002_DomainNeutralHmiStarterInventory.md`를 따른다. Windows 기반 선택 이관은 `_plan/2026-11/20261107_001_FoundationCodeMigration.md`, 최소 HMI 부트스트랩은 `_plan/2026-11/20261107_002_MinimalPlatformioHmiBootstrap.md`에 기록되어 있다.

2차 계획은 자동 검증과 사용자의 실제 LCD·touch·serial 확인, 내부 코드 재사용 권한 확인을 마쳐 H6까지 `complete`다. 외부 공개·납품용 라이선스 결정은 이 계획의 구현 시작 조건이 아니며 전체 로드맵 G4의 배포 게이트에서 다룬다.

## 1. 프로젝트 목적 인식과 이번 결정

이 프로젝트의 목적은 특정 제품이나 업무 흐름을 복제하는 것이 아니라, 새 응용 프로젝트가 공통 UI·입력·저장·통신·오디오 기반을 선택적으로 조립할 수 있는 **범용 HMI 시작 저장소**를 만드는 것이다. Windows 시뮬레이터와 ESP32-P4 HMI가 같은 응용 소스를 실행하고, 기본 앱은 샘플 기능에 의존하지 않아야 한다.

이번 차수는 전체 로드맵의 G1을 닫는 작업이다. 프로필 선택과 소스 격리 계약을 먼저 안정화하고, 별도 `sample-suite` 앱의 첫 Dashboard에서 각 기능 page로 이동하는 샘플 구조를 만든다. G1에서는 Dashboard와 첫 `ui-basics` page만 구현하고, 이후 샘플은 새 profile이 아니라 이 앱의 독립 page로 추가한다.

핵심 결정은 다음과 같다.

1. build profile은 기본 앱 `main`과 샘플 탐색 앱 `sample-suite` 두 종류로 유지한다. 기능 샘플마다 별도 firmware profile을 만들지 않는다.
2. 두 앱은 각각 versioned `profile.json`으로 공통 소스, host별 추가 소스, 모듈 의존, 지원 host, 데이터 namespace를 선언한다.
3. `sample-suite` 내부는 Dashboard와 page registry, 독립 page module로 나눈다. Dashboard는 descriptor만 읽고 개별 page 구현을 include하거나 분기하지 않는다.
4. 새 page는 `samples/suite/pages/<id>/`와 page registry 항목만 추가한다. 기존 page 내부, `main`, Board, platform port는 수정하지 않는다.
5. `profiles/registry.json`은 등록 manifest 경로와 기본 프로필 `main`만 소유한다. 프로필별 소스 목록을 중복해서 적지 않는다.
6. `tools/project.py`가 registry와 manifest를 검증하고 선택한 profile의 명시적 Windows CMake 입력과 HMI PlatformIO 입력을 `.generated/`에 만든다.
7. host는 `main`이나 `sample-suite`의 구체 class/header를 직접 알지 않는다. generator가 선택 profile의 concrete app type을 공통 compile-time 계약에 연결한다.
8. runtime plugin과 서비스 로케이터는 만들지 않는다. profile은 compile time에 하나만 선택하고, sample page는 compile-time descriptor table로 Dashboard에 등록한다.
9. Dashboard와 `ui-basics` page는 영어 ASCII와 LVGL 내장 Montserrat를 사용하는 진단 UI다. 제품 i18n·SD·외부 자산에 의존하지 않는다.
10. page는 진입할 때 LVGL 객체를 만들고 Dashboard로 돌아갈 때 timer·callback·widget을 정리한다. 모든 page를 startup에 동시에 생성하지 않는다.
11. Windows에서 profile/page 계약과 UI 동작을 빠르게 검증한 뒤, 최종 통합 게이트에서 `main`과 `sample-suite`의 HMI firmware를 각각 한 번 빌드한다.
12. Codex는 firmware compile까지만 수행한다. upload·flash·장치 변경은 사용자가 직접 한다.

## 2. 완료 시 얻는 결과

- `python tools/project.py list`가 `main`과 `sample-suite`를 registry 순서와 기본 여부와 함께 표시한다.
- 잘못된 schema version, 중복 ID, 누락 파일, 허용되지 않은 경로, 다른 profile 소스 참조, 중복 NVS namespace를 build 전에 거부한다.
- `main`과 `sample-suite`를 Windows와 HMI에서 각각 선택해 빌드할 수 있다.
- generic host는 선택된 concrete app을 생성·파괴하지만 `apps/main/`이나 `samples/suite/`를 직접 include하지 않는다.
- `main` build의 compile/link 입력에는 `samples/`가 없고, `sample-suite` build에는 `apps/main/app.cpp`가 없다.
- `sample-suite` 첫 화면에 page card/button 목록이 있고 `UI Basics`를 눌러 page로 이동한 뒤 Dashboard로 돌아올 수 있다.
- `ui-basics` page는 button, long press, scroll, toast, confirm/cancel을 시연한다.
- 새 sample page를 추가할 때 기존 app/page, Board, LVGL port 내부를 수정하지 않고 새 page 폴더와 page registry 항목만 추가하는 경로가 검증된다.
- `README.md`가 profile 목록 확인, 진단, Windows 실행, HMI build의 단일 시작 경로를 설명한다.

## 3. 현재 상태와 확인 근거

### 3.1 이미 존재하는 기반

- `apps/main/app.*`는 외부 자산 없는 공통 진단 화면이며 Windows와 HMI가 같은 `app.cpp`를 빌드한다.
- `hosts/windows/main.cpp`와 `hosts/hmi/main.cpp`는 각각 `MainApp`과 `MainAppOptions`를 직접 알고 있다.
- `platform/windows/`는 SDL display/input, LVGL tick, screenshot을 제공한다.
- `boards/jc1060p470c/`와 `platform/hmi/`는 LCD·touch·LVGL runtime과 allocator를 제공한다.
- `platformio.ini`는 `hmi-main` 한 환경의 production source를 `build_src_filter`에 직접 나열한다.
- 루트 `CMakeLists.txt`와 `apps/main/CMakeLists.txt`는 `main`을 고정 subdirectory와 target으로 연결한다.
- `README.md`는 현재 직접 CMake/PlatformIO 명령과 `main` 하나만 설명한다.
- profile manifest, registry, `.generated/`, `tools/project.py`, `samples/suite/`는 아직 없다.

### 3.2 현재 경계에서 바꿔야 할 점

현재 구성은 첫 부트스트랩에는 적합하지만 profile을 추가할 때 다음 파일을 직접 수정해야 한다.

- 루트 `CMakeLists.txt`
- `hosts/windows/CMakeLists.txt`
- `hosts/windows/main.cpp`
- `hosts/hmi/main.cpp`
- `platformio.ini`

이 방식으로 앱을 늘리면 host에 profile별 include·분기가 누적되고, PlatformIO source filter가 수동 목록의 단일 진실 공급원이 된다. 이번 계획은 앱 source 선택의 단일 진실 공급원을 profile manifest로 옮기고 host를 profile-neutral 조립 코드로 바꾼다. 기능 샘플 선택은 host/profile 분기가 아니라 `sample-suite` 내부 page registry가 소유한다.

### 3.3 구현 시작 선행 게이트 완료

2026-11-07 다음 항목을 사용자가 확인해 2차 계획 H6이 `complete`가 되었다.

- VS Code PlatformIO가 현재 저장소와 `hmi-main` 환경을 인식한다.
- 사용자가 직접 업로드한 firmware에서 LCD와 touch 진단 화면이 정상 동작한다.
- serial log에 display·touch·LVGL 초기화 실패가 없다.
- 원본 코드를 현재 저장소의 내부 개발에 재사용할 권한이 있다.

외부 공개·납품용 프로젝트 라이선스와 제3자 고지는 G4의 별도 종료 조건이다. 이후 display/touch/Board 결함이 발견되면 profile 계획에서 우회하지 않고 HMI 기반 회귀로 분리해 수정한다.

## 4. 목표 구조

```text
profiles/
  registry.json
  schema/
    registry.schema.json
    profile.schema.json
    page_registry.schema.json
  runtime/
    app_contract.hpp
apps/
  main/
    profile.json
    app.hpp
    app.cpp
samples/
  suite/
    profile.json
    app.hpp
    app.cpp
    page_contract.hpp
    page_registry.json
    dashboard/
      dashboard_page.hpp
      dashboard_page.cpp
    pages/
      ui_basics/
        ui_basics_page.hpp
        ui_basics_page.cpp
    tests/
      navigation_test.cpp
      state_test.cpp
      interaction_test.cpp
tools/
  project.py
  project/
    manifest.py
    paths.py
    generate.py
    commands.py
  tests/
    fixtures/
      profiles/
    test_manifest.py
    test_generation.py
cmake/
  ProfileSelection.cmake
.generated/                    # Git 제외, host/profile별 생성
  windows/
    main/
    sample-suite/
  hmi/
    main/
    sample-suite/
build/
  windows-main/
  windows-sample-suite/
.pio/
  build/                       # 생성 config의 환경도 profile별 분리
```

실제 구현 중 helper 파일명은 책임이 더 분명한 이름으로 조정할 수 있다. 다만 manifest, 생성물, build/data 경로의 host/profile 분리와 아래 책임 경계는 유지한다.

## 5. 프로필 등록 계약

### 5.1 Registry

`profiles/registry.json`은 다음 정보만 가진다.

- `schema_version`
- `default_profile`: 항상 등록된 `main`
- `profiles`: registry 위치 기준의 manifest 상대 경로 목록

registry는 중복 ID를 허용하지 않는다. 목록 순서는 `list` 출력의 안정적인 순서에만 사용하고 build 의미를 바꾸지 않는다.

### 5.2 Profile manifest

각 `profile.json`은 최소한 다음을 선언한다.

| 항목 | 계약 |
| --- | --- |
| `schema_version` | 지원하는 정확한 manifest schema version |
| `id`, `kind`, `description` | 고유 ID, `application` 또는 `sample`, 중립 설명 |
| `entry` | concrete app header와 type, 공통 app 계약 구현 |
| `sources.common` | 두 host가 함께 빌드하는 명시적 `.cpp` 목록 |
| `sources.windows`, `sources.hmi` | 해당 profile에만 필요한 host별 추가 source 목록 |
| `include_dirs` | profile root 안의 필요한 include 경로 |
| `modules` | 등록된 공통 module ID 목록 |
| `hosts` | 지원 host와 HMI board 요구사항 |
| `assets`, `config` | 필수 파일의 명시적 목록; G1에서는 빈 목록 허용 |
| `data` | Windows root key, SD 하위 경로, NVS namespace |
| `verification` | profile이 제공하는 test/scenario ID |

모든 파일 경로는 manifest 위치 기준으로 해석한다. wildcard, 절대 경로, `..`로 profile root를 벗어나는 경로, symlink를 통한 저장소 탈출을 거부한다. 공통 module은 향후 `modules/` registry로 확장할 수 있지만 G1에서는 실제 존재하는 `text`처럼 등록된 ID만 허용한다.

### 5.3 Compile-time app 계약

`profiles/runtime/app_contract.hpp`는 host가 profile app에 전달하는 최소 context만 정의한다.

- host ID 또는 표시용 host name
- profile app의 `create(context)`와 `destroy()` 수명 계약
- copy 금지와 host/runtime이 app보다 오래 살아야 한다는 조건

generator는 선택된 manifest의 `entry.header`를 include하고 concrete type을 `SelectedProfileApp` alias로 노출하는 작은 header를 만든다. host는 이 생성 header와 공통 context만 include한다. virtual base, heap 기반 factory, `void*`, runtime registry는 도입하지 않는다.

`MainAppOptions`는 공통 context로 대체하고 `MainApp`은 같은 method 계약을 유지한다. `SampleSuiteApp`도 동일한 계약을 구현한다. concrete app 객체는 Windows scope 또는 HMI function-static storage에 존재하며 LVGL runtime보다 먼저 파괴되지 않는다.

### 5.4 생성물과 build 연결

`tools/project.py prepare --host <host> --profile <id>`는 검증을 통과한 뒤 다음을 원자적으로 생성한다.

- 선택 app alias header
- 해석된 source/include/module 목록 JSON
- Windows용 CMake include
- HMI용 독립 PlatformIO config (`.generated/hmi/<profile>/platformio.ini`)
- 입력 manifest hash와 generator version metadata

생성물은 `.generated/<host>/<profile>/`에만 쓰고 직접 편집하지 않는다. build 폴더와 실행 데이터도 host/profile별로 분리한다. 동일 hash면 결과를 재사용할 수 있지만 다른 profile 생성물을 공유하지 않는다.

Windows CMake와 HMI PlatformIO는 생성된 **명시적 목록**만 소비한다. 모든 `apps/`나 `samples/`를 재귀 수집한 뒤 compile definition으로 비활성화하는 방식은 금지한다.

HMI build는 생성 config를 `pio -c`로 선택한다. 생성 config는 루트 `platformio.ini`의 공통 toolchain/board 설정을 include하고 profile별 environment 이름, source filter, build 경로만 추가한다. 루트 설정에 profile source 목록을 다시 적지 않는다. VS Code가 사용할 default `main` config도 같은 generator 결과에서 준비하며, clean clone에서는 `tools/project.py prepare --host hmi --profile main`이 선행된다.

### 5.5 Sample page registry

`samples/suite/page_registry.json`은 `sample-suite` 안에서 Dashboard에 노출할 page의 단일 등록 지점이다. 최소 항목은 다음과 같다.

| 항목 | 계약 |
| --- | --- |
| `schema_version` | 지원하는 정확한 page registry version |
| `id`, `title`, `description`, `order` | 안정적인 page ID, Dashboard 표시 문구와 순서 |
| `entry` | `SamplePage` 구현 header/type과 factory 생성 정보 |
| `sources.common/windows/hmi` | 해당 page의 명시적 source 목록 |
| `capabilities` | page가 요구하는 좁은 기능 ID; G1의 `ui-basics`는 빈 목록 |
| `verification` | page 단위 test/scenario ID |
| `data_key` | 향후 page별 실행 데이터 namespace |

generator는 page registry를 검증해 `sample-suite` profile의 resolved source set에 page source를 합치고, firmware에는 작은 정적 descriptor/factory table만 생성한다. runtime에서 JSON을 읽거나 page 디렉터리를 탐색하지 않는다. Dashboard는 이 table만 소비한다.

page controller는 작은 `SamplePage` interface를 구현하고 checked factory로 한 번에 하나만 생성한다. 생성 실패는 Dashboard 오류 상태로 반환하며 기존 Dashboard를 유지한다. page별 실제 widget/model은 `create()`에서 준비하고 `destroy()`에서 해제한다. 이 extension boundary에서는 page 간 타입이 실제로 다르므로 작은 동적 다형성을 허용하되, 객체 크기·할당 실패·동시 생존 개수 1을 검증한다.

page source는 다른 page 구현이나 `apps/main/`을 참조하지 않는다. 공통 의미가 확인된 코드만 `modules/` 또는 `samples/suite/common/`으로 이동한다. page별 Windows/HMI adapter가 필요해지면 해당 page의 host source로 등록하고 generic host나 Dashboard에 플랫폼 분기를 추가하지 않는다.

## 6. `sample-suite` Dashboard와 page 계약

`sample-suite`는 상품 앱이 아니라 공통 기능을 한 firmware에서 탐색하는 진단 앱이다. 첫 화면은 Dashboard이고 각 기능은 독립 page다. 현재 G1의 Dashboard와 `ui-basics`는 영어 ASCII와 내장 Montserrat만 사용하고 외부 font, image, SD, i18n에 의존하지 않는다. 후속 feature page는 자신의 descriptor/source/asset 범위에서 필요한 capability를 독립적으로 추가할 수 있으며, 한·영 i18n과 키보드 입력은 `_plan/2026-11/20261107_004_BilingualI18nAndTextInput.md`를 따른다.

Dashboard와 page 계약은 다음과 같다.

1. Dashboard는 page ID, title, 짧은 설명, 지원 host/capability를 descriptor에서 읽어 card/button을 만든다.
2. Dashboard가 개별 page header를 직접 include하거나 page ID별 `if/switch`를 가지지 않는다.
3. page contract는 `create(parent, context)`, `destroy()`와 작은 상태 snapshot만 정의한다. raw display/touch/Board handle을 노출하지 않는다.
4. Dashboard에서 `UI Basics`를 선택하면 `ui-basics` page가 생성되고 Back으로 돌아오면 해당 page의 timer·callback·widget이 정리된다.
5. 동시에 활성화되는 feature page는 하나뿐이다. Dashboard와 비활성 page의 무거운 state/widget을 중복 보유하지 않는다.
6. 향후 hardware page는 필요한 실제 capability가 없으면 descriptor 상태로 disabled/unsupported를 표시한다. Dashboard가 가짜 성공 상태를 만들지 않는다.
7. 새 page 등록은 `samples/suite/pages/<id>/`와 `page_registry.json`만 변경하며 기존 page 내부를 수정하지 않는다.

첫 `ui-basics` page의 필수 동작은 다음과 같다.

1. 일반 button press 횟수를 표시한다.
2. long press를 일반 press와 구분해 상태 문구를 갱신한다.
3. 고정된 중립 항목 목록을 세로로 scroll한다.
4. action button이 bounded lifetime의 toast를 표시하고 자동 정리한다.
5. destructive 의미가 없는 중립 action에 confirm dialog를 열고 cancel/confirm 결과를 각각 표시한다.
6. Dashboard↔page 이동과 overlay 열기/닫기를 반복해도 이전 page 객체, timer, callback이 남지 않는다.

confirm 본문과 자동 줄바꿈 가능 문구는 title/body label을 분리하고 line spacing을 명시한다. 중앙 안내는 가운데 정렬하며 label width, height, parent padding을 함께 고정해 1024×600에서 잘리지 않게 한다.

UI callback은 screen state를 갱신하거나 안전한 전환 요청만 기록한다. callback 안에서 Board API, host event loop, display/touch driver를 호출하지 않는다. 화면 교체는 callback의 삭제 중첩을 피할 수 있는 안전한 시점에 수행한다.

상태 검사용 공개 API가 필요하면 raw `lv_obj_t*` 대신 `current_page`, `overlay_state`, `last_action` 같은 작은 enum/value snapshot만 제공한다. test hook 때문에 platform API나 widget ownership을 외부로 노출하지 않는다.

## 7. CLI 범위

이번 차수에서 구현하는 명령은 다음으로 제한한다.

```powershell
python tools/project.py list
python tools/project.py doctor --host windows --profile main
python tools/project.py doctor --host hmi --profile sample-suite
python tools/project.py prepare --host windows --profile sample-suite
python tools/project.py build --host windows --profile main
python tools/project.py build --host windows --profile sample-suite
python tools/project.py run --host windows --profile sample-suite
python tools/project.py build --host hmi --profile main
python tools/project.py build --host hmi --profile sample-suite
python tools/project.py check --host windows --profile sample-suite
python tools/project.py check --scope isolation
```

명령 계약은 다음과 같다.

- `list`: registry를 검증하고 ID, kind, 지원 host, 기본 여부를 출력한다.
- `doctor`: 파일·도구·manifest·host 지원을 검사하는 read-only 명령이다. 설치, 다운로드, 장치 검색, config 변경을 하지 않는다.
- `prepare`: 선택 profile의 generated build input만 만든다.
- `build`: 필요한 prepare를 수행한 뒤 선택 host/profile만 구성·빌드한다.
- `run`: Windows에서만 build 후 선택 profile을 실행한다. HMI에서는 명확한 오류와 수동 upload 안내를 반환한다.
- `check --host`: profile이 선언한 자동 test를 실행한다.
- `check --scope isolation`: 모든 등록 profile/page의 정적 경로 규칙, page 간 참조 금지와 실제 생성 source set의 교차 오염을 검사한다.

`bootstrap`, `export`, `bundle`, asset generator, dependency 설치, 실제 serial/backend 선택은 후속 계획으로 미룬다.

## 8. 단계 상태

| 단계 | 상태 | 결과 |
| --- | --- | --- |
| P1 | `complete` | versioned registry/manifest schema와 validator; 15개 fixture 및 두 host doctor 통과 |
| P2 | `complete` | compile-time app 계약, 원자적 host/profile별 build input과 격리 검사 통과 |
| P3 | `complete` | `main`의 profile-neutral 조립과 VS Code `hmi-main` 환경 회귀 수정·검증 완료 |
| P4 | `complete` | 생성 descriptor 기반 Dashboard와 `ui-basics`의 Windows pointer·상태·화면 검증 완료 |
| P4A | `complete` | 공통 UI style foundation과 인메모리 다크·라이트 theme 전환·Windows 검증 완료 |
| P5 | `complete` | 두 profile의 HMI build, 실제 source·build directory 격리와 memory/lifetime 검토 완료 |
| P6 | `complete` | 시작 문서·반복 추가·clean build 자동 게이트 통과, Windows 수용 승인과 실기기 G4 이관 완료 |

한 번에 한 단계만 `in_progress`로 바꾼다. 각 단계의 자동·가능한 수동 검증과 종료 조건에 근거를 기록한 뒤 다음 단계로 이동한다.

## 9. 단계별 실행 계획

### P1. Registry schema와 validator

**목표:** build를 시작하기 전에 profile 등록 오류와 격리 위반을 결정적으로 거부한다.

**의존성:** 2차 계획 H6 `complete`.

**범위**

- 포함: `profiles/registry.json`, JSON schema, `apps/main/profile.json`, manifest loader/path validator, negative fixture test
- 제외: CMake/PlatformIO 생성, host 수정, `sample-suite` 구현

**산출물**

- schema version 1의 registry/profile 계약
- 경로 정규화와 저장소/profile root 탈출 방지
- ID·host·module·namespace·파일 존재 검사
- 중복 ID/NVS namespace, 다른 profile 경로, wildcard, 절대 경로 거부
- `list`, `doctor`의 검증 core

G1의 registry 검증은 Python 표준 라이브러리 `json`, `pathlib`, `unittest`만 사용한다. schema 파일은 사람과 도구가 공유하는 계약 문서이며, loader와 schema의 필수/허용 필드 일치는 fixture test로 고정한다. 이 단계에서 별도 `pip install`을 요구하지 않는다.

**자동 검증**

```powershell
python -m unittest discover -s tools/tests -p "test_manifest.py"
python tools/project.py list
python tools/project.py doctor --host windows --profile main
python tools/project.py doctor --host hmi --profile main
```

정상 `main`은 통과하고, fixture의 잘못된 version·중복 ID·누락 파일·경로 탈출·교차 profile 참조·namespace 충돌은 각각 원인과 manifest 경로가 포함된 오류로 실패해야 한다.

**수동 검증:** `list`와 대표 오류 문구가 새 사용자가 수정할 파일과 원인을 이해할 수 있는지 검토한다.

**종료 조건**

- [X] schema와 loader가 같은 필수/허용 필드를 판정한다.
- [X] 모든 negative fixture가 의도한 오류 코드와 메시지로 실패한다.
- [X] validator가 파일을 쓰거나 외부 도구를 설치하지 않는다.
- [X] P1 근거가 기록되기 전 P2 구현을 시작하지 않는다.

**완료 근거 (2026-11-07)**

- `python -m unittest discover -s tools/tests -p "test_manifest.py"`: 15개 통과. 정상 registry, schema/loader 필드 일치, read-only, 잘못된 registry/profile version, 중복 ID/NVS namespace, 누락 파일, 저장소 탈출, 다른 profile 참조, wildcard, 절대 경로, 미등록 module, 미등록 필드를 검증했다.
- `python tools/project.py list`: 종료 코드 0. `main`, `application`, `windows,hmi`, default `yes`를 표시했다.
- `python tools/project.py doctor --host windows --profile main`: 종료 코드 0. Python 3.14, CMake, Ninja와 manifest가 `ready`다.
- `python tools/project.py doctor --host hmi --profile main`: 종료 코드 0. Python 3.14, PlatformIO와 manifest가 `ready`다.
- 대표 실패 문구를 `[missing-file] apps/main/profile.json.sources.common[0]: file 'missing.cpp' does not exist`로 확인했다. 저장소 상대 경로, 오류 코드, 필드 위치와 원인이 모두 포함된다.
- loader 전후 fixture 파일 byte snapshot이 같음을 test로 확인했으며 설치·다운로드·생성물 기록을 수행하지 않았다.

### P2. App 계약과 generated build input

**목표:** profile concrete type과 source 목록을 host/profile별 생성물로 변환한다.

**의존성:** P1 `complete`.

**범위**

- 포함: 공통 app context, generator, 원자적 `.generated/` 기록, CMake/PlatformIO 입력 형식, generator test
- 제외: 기존 host 전환, `sample-suite` Dashboard/page, firmware build

**산출물**

- `profiles/runtime/app_contract.hpp`
- selected app alias header
- resolved source set JSON과 manifest hash metadata
- Windows CMake include와 HMI PlatformIO source input
- profile 변경 또는 제거 후 stale 생성물 감지

**자동 검증**

```powershell
python -m unittest discover -s tools/tests -p "test_generation.py"
python tools/project.py prepare --host windows --profile main
python tools/project.py prepare --host hmi --profile main
python tools/project.py check --scope isolation
```

- 두 host 생성물은 같은 common app source를 포함한다.
- Windows 생성물에는 HMI/board source가 없고 HMI 생성물에는 SDL/Windows source가 없다.
- 같은 입력의 재생성 결과와 hash는 결정적이다.
- 생성 중 실패하면 이전의 유효한 결과가 부분 파일로 바뀌지 않는다.

**수동 검증:** 생성 파일의 source/include 목록이 manifest와 platform base 책임을 구분해 읽을 수 있는지 검토한다.

**종료 조건**

- [X] host가 소비할 선택 app alias와 명시적 source 목록이 생성된다.
- [X] `.generated/<host>/<profile>/` 밖을 수정하지 않는다.
- [X] wildcard와 전체 source 재귀 수집이 없다.
- [X] P2 근거가 기록되기 전 P3 구현을 시작하지 않는다.

**완료 근거 (2026-11-07)**

- `python -m unittest discover -s tools/tests -p "test_generation.py"`: 10개 통과. 두 host 공통 앱 source 일치, platform/board 격리, selected alias/CMake/PlatformIO 입력, 결정적 hash/byte, staging 실패 시 기존 결과 보존, stale hash/삭제 profile 감지, 출력 경계와 루트 `hmi-main` 선택을 검증했다.
- `python tools/project.py prepare --host windows --profile main`: 종료 코드 0, `.generated/windows/main/`에 alias, resolved JSON, metadata와 CMake 입력을 생성했다.
- `python tools/project.py prepare --host hmi --profile main`: 종료 코드 0, `.generated/hmi/main/`에 alias, resolved JSON, metadata와 명시적 PlatformIO source 입력을 생성했다.
- `python tools/project.py check --scope isolation`: 종료 코드 0, `windows/main`, `hmi/main`의 host source 누수를 찾지 못했다.
- 같은 입력 재생성의 모든 file byte와 64자리 SHA-256이 동일했다. 주입한 staging write 실패 뒤 이전 target byte가 유지되고 임시 staging directory가 남지 않았다.
- 생성된 JSON은 `profile`, `modules`, `host`, `board` source/include를 구분한다. Windows 목록에는 HMI/Board source가 없고 HMI 목록에는 Windows/SDL source가 없다.
- manifest와 생성 파일의 positive source 항목에는 wildcard가 없으며 generator는 directory 재귀 수집을 사용하지 않는다. PlatformIO allowlist의 첫 `-<*>`는 기본 전체 포함을 끄는 deny 규칙이고 모든 positive 항목은 개별 파일이다.

### P3. `main` 등록과 profile-neutral host 조립

**목표:** 기존 `main`을 registry 기반으로 선택하면서 화면 동작과 Board 경계를 유지한다.

**의존성:** P2 `complete`.

**범위**

- 포함: `MainAppOptions`를 공통 context로 전환, generic host include, CMake/PlatformIO 생성 입력 소비, `build/run/check` 최소 명령
- 제외: `sample-suite`, Board/internal 수정, 새 platform adapter

**산출물**

- profile-neutral Windows/HMI host
- manifest에서 선택된 `main` target/source set
- host/profile별 build directory와 HMI environment
- 기존 main input test, smoke, screenshot의 새 명령 경로 연결

**자동 검증**

```powershell
python tools/project.py build --host windows --profile main
python tools/project.py check --host windows --profile main
python tools/project.py build --host hmi --profile main
python tools/project.py check --scope isolation
rg -n "apps/main|samples/suite|MainApp|SampleSuiteApp" hosts cmake platformio.ini
rg -n "Arduino|freertos|esp_|driver/|SDL|boards/" apps/main
rg -n "apps/main|samples/suite|lv_obj_|lv_label_|lv_btn_" boards/jc1060p470c platform/hmi
```

- generic host와 build wiring에 concrete app 경로/type 참조가 0건이다.
- Windows main smoke와 input test가 통과한다.
- HMI main firmware가 선택 manifest source로 빌드된다.
- 앱의 platform/board 의존과 Board/platform의 앱/widget 참조가 0건이다.

**수동 검증:** Windows `main`에서 기존 다섯 문구, click 후 입력 상태, Esc/창 닫기 동작이 유지되는지 확인한다.

**종료 조건**

- [X] `main`의 기존 Windows 동작과 screenshot이 유지된다.
- [X] host가 selected alias와 공통 context만 사용한다.
- [X] HMI firmware RAM/flash가 기록되고 예상하지 않은 큰 증가를 검토한다.
- [X] Board/LvglPort의 공개·내부 책임이 바뀌지 않는다.
- [X] P3 근거가 기록되기 전 P4 구현을 시작하지 않는다.

**완료 근거 (2026-11-07)**

- `python tools/project.py build --host windows --profile main`: 종료 코드 0. `build/windows-main/`에서 generated CMake 입력으로 `hmi_selected_app`과 generic `hmi_profile_windows`를 빌드했다.
- 최초 Windows build가 기존 `platform/windows/sdl_lvgl_port.cpp`의 `SDL_INIT_VIDEO dnjs| SDL_INIT_TIMER` 구문 오류를 발견했다. 프로필 변경과 무관한 오타 `dnjs`만 제거한 뒤 동일 명령이 통과했다.
- `python tools/project.py check --host windows --profile main`: `hmi_main_app_input`, `hmi_main_windows_smoke` 2개 통과.
- `hmi_main_windows.exe --screenshot build/windows-main/hmi-main.bmp`: 종료 코드 0. 1024×600 image에서 `HMI Starter`, `Profile: main`, `Host: windows`, `Status: ready`, `Input: waiting`의 배치와 기존 색상을 확인했다.
- `ctest --test-dir build/windows-main -R '^hmi_windows_sdl_(escape|quit)$' --output-on-failure`: ESC와 window quit 2개 통과. input test는 panel press 뒤 `Input: detected` 전환을 확인한다.
- `python tools/project.py build --host hmi --profile main`: 종료 코드 0. upload 없이 compile만 수행했으며 RAM 27,892 B/512,000 B(5.4%), flash 645,594 B/6,553,600 B(9.9%)다.
- `python tools/project.py check --scope isolation`: `windows/main`, `hmi/main` 통과. 생성 source set에서 host 반대편 source 누수가 없다.
- `rg -n "apps/main|samples/suite|MainApp|SampleSuiteApp" hosts cmake platformio.ini`, 앱의 Arduino/FreeRTOS/ESP/driver/SDL/Board 검색, Board/HMI platform의 app/LVGL widget 검색은 모두 0건이다.
- host는 `selected_profile.hpp`, `SelectedProfileApp`, `AppContext`만 사용한다. HMI 앱은 function-static persistent storage, Windows 앱은 checked heap allocation으로 main stack과 분리했고 새 대형 model/copy/callback capture는 없다.
- Board와 `boards/jc1060p470c/internal/`, HMI `LvglPort`는 수정하지 않았고 app/host의 internal include는 0건이다. 장치 upload·flash 명령은 실행하지 않았다.

**VS Code 환경 회귀 수정 근거 (2026-11-07)**

- 사용자의 VS Code PlatformIO가 공통 설정용 `[env:hmi-base]`를 실제 upload 환경으로 선택해 `lv_conf.h` include 경로 없이 실패하는 회귀를 확인했다.
- 루트 `platformio.ini`의 공통값을 build environment가 아닌 `[env]`로 옮기고, default를 `hmi-main`, extra config를 `.generated/hmi/main/profile.ini`로 지정했다. 생성기는 profile env fragment와 독립 CLI wrapper를 분리한다.
- PlatformIO `project config --json-output`은 `default_envs=[hmi-main]`, 공통 `[env]`, 실제 `[env:hmi-main]`만 반환한다. `[env:hmi-base]`는 더 이상 없다.
- 루트 설정에서 compile-only `pio run -e hmi-main`: 종료 코드 0. `.pio/build/hmi-main/`을 사용하고 `lv_conf.h`를 포함한 LVGL compile, link, firmware image 생성까지 통과했다.
- `python tools/project.py build --host hmi --profile main`: 종료 코드 0. 독립 generated wrapper 경로도 유지되며 RAM 27,892 B(5.4%), flash 645,594 B(9.9%)가 동일하다.
- 회귀 수정 중에도 upload·flash target은 실행하지 않았다.

### P4. `sample-suite` Dashboard와 첫 page의 Windows 검증

**목표:** 하나의 sample 앱에서 Dashboard를 통해 `ui-basics` page에 진입하고 돌아오며, page가 모든 S01 UI 동작을 제공한다.

**의존성:** P3 `complete`.

**범위**

- 포함: `samples/suite/profile.json`, page contract/registry, Dashboard, `pages/ui_basics/`, navigation/state/interaction test, Windows build/run
- 제외: 외부 asset, 상품 i18n, storage, 설정, 통신, HMI 전용 UI 분기

**산출물**

- `SampleSuiteApp`, Dashboard, page descriptor/contract와 중립 상태 enum/snapshot
- Dashboard↔page navigation과 active page 하나의 lifetime
- `ui-basics`의 press/long press, scroll, toast, confirm/cancel
- page의 overlay/timer/widget 정리
- deterministic navigation/state test와 Windows interaction/smoke test
- 1024×600 screenshot

**자동 검증**

```powershell
python tools/project.py doctor --host windows --profile sample-suite
python tools/project.py build --host windows --profile sample-suite
python tools/project.py check --host windows --profile sample-suite
python tools/project.py check --scope isolation
rg -n "Arduino|freertos|esp_|driver/|platform/windows|SDL|boards/" samples/suite
rg -n "apps/main|MainApp" samples/suite .generated/windows/sample-suite
rg -n "ui_basics|ui-basics" samples/suite/dashboard samples/suite/app.cpp
```

- navigation/state test가 Dashboard↔`ui-basics`, press/long press 구분, toast 만료, confirm/cancel 상태를 확인한다.
- interaction/smoke test가 실제 Dashboard/page 생성과 대표 event wiring을 확인한다.
- Dashboard와 app의 `ui-basics` 직접 참조가 0건이고 page descriptor table이 연결을 소유한다.
- sample suite code의 platform/board/main 의존이 0건이다.
- Windows compile input에 `apps/main/app.cpp`가 없다.

**수동 검증**

1. Windows Dashboard에서 `UI Basics` card/button을 눌러 page로 이동하고 Back으로 돌아오는 동작을 반복한다.
2. press와 long press 결과가 서로 다르게 표시되는지 확인한다.
3. 목록이 부드럽게 scroll되고 다른 control의 click과 충돌하지 않는지 확인한다.
4. toast가 겹치지 않고 자동으로 사라지는지 확인한다.
5. confirm dialog의 cancel/confirm 결과와 반복 open/close를 확인한다.
6. 여러 줄 문구의 간격·정렬·잘림과 1024×600 전체 균형을 확인한다.

**종료 조건**

- [X] Dashboard가 descriptor에서 page 목록을 만들고 개별 page를 직접 참조하지 않는다.
- [X] Dashboard↔page 이동과 모든 필수 UI 동작의 자동 상태 검사가 통과한다.
- [X] Windows에서 대표 pointer 동작과 screenshot을 확인했다.
- [X] app/page callback, timer, overlay ownership과 파괴 순서가 문서화되어 있다.
- [X] Dashboard로 돌아온 뒤 비활성 page widget/timer가 남지 않는다.
- [X] 새 class/public lifetime 계약에 유용한 한국어 주석이 있다.
- [X] P4 근거가 기록되기 전 P5 구현을 시작하지 않는다.

**완료 근거 (2026-11-07)**

- `samples/suite/page_registry.json`을 검증해 page source와 생성 `page_registry.cpp`의 정적 descriptor/checked factory로 변환한다. Dashboard와 `SampleSuiteApp`은 개별 page header, ID, type을 직접 참조하지 않는다.
- `python tools/project.py doctor --host windows --profile sample-suite`: 종료 코드 0. Python, CMake, Ninja와 manifest가 `ready`다.
- `python tools/project.py build --host windows --profile sample-suite`: 종료 코드 0. Windows app, navigation/state/interaction test를 모두 빌드했다.
- `python tools/project.py check --host windows --profile sample-suite`: `hmi_sample_suite_navigation`, `state`, `interaction`, `windows_smoke` 4개가 모두 통과했다. navigation은 Dashboard 왕복 3회와 미등록 page 거부, state는 press/long press·toast 만료·dialog cancel/confirm을 검증한다.
- interaction test는 Windows pointer read 경로로 Dashboard card, short press, long press와 list drag/scroll을 구동하고 실제 widget event wiring, toast/dialog, Back을 확인했다. Windows test driver는 platform test target에만 있고 sample production source에는 포함되지 않는다.
- `python -m unittest discover -s tools/tests -p "test_*.py"`: 26개 통과. 실제 registry의 두 profile과 sample page source/생성 factory 입력을 함께 고정했다.
- `python tools/project.py check --scope isolation`: `windows/main`, `windows/sample-suite`, `hmi/main`, `hmi/sample-suite` 생성 입력이 통과했다. page는 자기 `pages/<id>/` 경계 안의 source만 등록할 수 있고 sample composition의 page 직접 참조와 platform/main 결합을 거부한다.
- 계획의 세 `rg` 검사 모두 0건이다. sample-suite의 Windows resolved source에는 `samples/suite/app.cpp`, Dashboard, `ui_basics_page.cpp`만 있고 `apps/main/app.cpp`는 없다.
- `build/windows-sample-suite/sample-suite-dashboard.bmp`, `ui-basics-page.bmp`, toast/dialog capture를 1024×600으로 확인했다. Dashboard, Back, control, scroll 결과와 여러 줄 dialog가 잘리지 않으며, visual QA에서 발견한 overlay flex 참여 문제는 floating overlay로 수정했다.
- `SampleSuiteApp`은 `lv_async_call`로 callback과 실제 화면 교체를 분리하고 active page 하나만 소유한다. `UiBasicsPage::destroy()`는 toast timer와 overlay를 먼저 지운 뒤 root widget을 삭제하며 두 controller 크기는 256 bytes 이하 assertion으로 제한한다.
- Windows `main` build와 두 기존 test도 재통과했다. P4에서는 HMI firmware build, upload, flash 또는 장치 변경을 실행하지 않았다.

### P4A. 공통 UI style foundation과 인메모리 theme 전환

**목표:** Dashboard와 모든 sample page가 같은 style token/component 규칙을 사용하고, 사용자가 다크·라이트 theme를 전환하면 현재 화면과 이후 page 이동에 인메모리 선택이 유지된다.

**의존성:** P4 `complete`.

**범위**

- 포함: 공통 `modules/ui/` token/style/component 계약, sample-suite theme ownership, Dashboard/UI Basics/Toast/Dialog 리디자인, theme 전환 상태·pointer·screenshot 검증
- 제외: NVS/SD 영구 저장, 상품 i18n/font asset, Board/platform 내부, HMI firmware build와 실제 LCD 검증

**산출물**

- 두 화면 이상이 공유하는 색상·간격·radius·text role·button variant와 bounded `lv_style_t` 집합
- `SampleSuiteApp`이 소유하는 `ThemeMode`와 widget보다 긴 style lifetime
- Dashboard와 page의 theme toggle, 안전한 비동기 현재 화면 재구성, Dashboard↔page 간 mode 유지
- 다크·라이트 Dashboard/page/Toast/Dialog 1024×600 capture
- theme 전환·유지·기존 UI 동작 회귀 test

**자동 검증**

```powershell
python tools/project.py build --host windows --profile sample-suite
python tools/project.py check --host windows --profile sample-suite
python tools/project.py check --scope isolation
rg -n "lv_color_hex|lv_obj_set_style_(bg|border|shadow|text_color|radius)" samples/suite/dashboard samples/suite/pages/ui_basics
rg -n "Arduino|freertos|esp_|driver/|platform/windows|SDL|boards/" modules/ui samples/suite
```

- state/navigation test가 기본 dark, light 전환, 현재 page 재구성, Dashboard 복귀 뒤 light 유지와 다시 dark 전환을 확인한다.
- 기존 press/long press, scroll, toast, confirm/cancel, Back interaction이 두 theme 중 대표 경로에서 유지된다.
- 화면 구현의 공통 색상·surface·button/text 스타일 직접 지정은 0건이고 layout·multiline bounds만 화면이 소유한다.
- theme 객체는 동적 container 없이 bounded style/token만 보유하고 widget이 모두 파괴된 뒤에만 style을 reset한다.

**수동 검증**

1. 다크·라이트 Dashboard의 계층, 대비, card 상태와 toggle 의미가 분명한지 비교한다.
2. UI Basics의 버튼 variant, status chip, list, Toast, Dialog가 두 theme에서 잘리지 않고 읽히는지 확인한다.
3. Dashboard↔page 이동과 현재 page에서 theme 전환을 반복해 깜박임·잔류 widget·색 혼합이 없는지 확인한다.

**종료 조건**

- [X] 공통 token/style/component가 sample 화면의 시각 규칙을 소유한다.
- [X] theme 선택이 앱 수명 동안 유지되고 영구 저장을 사용하지 않는다.
- [X] 관련 Windows build/test와 dark/light capture 검토가 통과한다.
- [X] 객체 크기, style/widget 파괴 순서와 플랫폼 격리가 검토됐다.
- [X] P4A 근거가 기록되기 전 P5 구현을 시작하지 않는다.

**완료 근거 (2026-11-07)**

- `modules/ui/`에 bounded `UiTheme`, dark/light palette, surface/card/chip/status/toast/dialog style과 공통 label/button/multiline helper를 추가했다. Dashboard와 `ui-basics`의 색상·border·shadow·radius·text color 직접 지정 검색은 0건이다.
- `SampleSuiteApp`이 기본 dark인 theme를 소유한다. Dashboard와 page의 toggle은 `lv_async_call` 전환을 사용하고, active widget을 먼저 파괴한 뒤 style을 재설정해 같은 view를 재구성한다. `SampleSuiteSnapshot` 검증으로 Dashboard→page→theme 전환→Dashboard에서도 mode가 유지됨을 확인했다.
- theme mode는 앱 객체의 enum 값으로만 유지하며 NVS, SD, 파일 저장을 사용하지 않는다. `UiTheme`와 `SampleSuiteApp`은 각각 2048 bytes, 2304 bytes 이하 compile-time assertion으로 제한했다.
- `python tools/project.py check --host windows --profile sample-suite`: navigation/state/interaction/smoke 4개가 통과했다. interaction은 실제 pointer click·long press·scroll과 toast/dialog/Back을 확인한다.
- `python -m unittest discover -s tools/tests -v`: manifest/generator 26개가 통과했고, `python tools/project.py check --scope isolation`은 `windows/main`, `windows/sample-suite`, `hmi/main`, `hmi/sample-suite` 4개 generated input을 확인했다. `python tools/project.py check --host windows --profile main`의 기존 2개 test도 통과했다.
- `build/windows-sample-suite/theme-ui-basics-dark.bmp.dark-dashboard.bmp`, `theme-ui-basics-dark.bmp`, `.toast.bmp`, `.dialog.bmp`, `.light-dashboard.bmp`, `.light.bmp`, `.light.toast.bmp`, `.light.dialog.bmp`를 1024×600으로 비교했다. 두 theme의 Dashboard/page 계층, button variant, scroll list, toast와 multiline dialog가 잘리지 않고 색 혼합 없이 표시된다.
- P4A에서는 HMI firmware compile, upload, flash 또는 실제 장치 변경을 수행하지 않았다. P5는 이 근거 기록 뒤에도 `pending`으로 유지한다.

### P5. HMI 선택 build와 source 격리

**목표:** `main`과 `sample-suite`가 같은 board/platform 기반을 공유하면서 app source는 서로 섞이지 않음을 실제 build 입력으로 증명한다.

**의존성:** P4A `complete`.

**범위**

- 포함: 두 HMI profile build, compile database/resolved source 비교, memory/lifetime 검토
- 제외: Codex upload, board driver 수정, 새 장치 서비스

**산출물**

- `main`, `sample-suite` HMI firmware build 결과
- profile별 actual/resolved source evidence
- RAM/flash 비교와 큰 객체·callback capture·timer lifetime 검토
- 교차 profile source 0건 근거

**자동 검증**

```powershell
python tools/project.py build --host hmi --profile main
python tools/project.py build --host hmi --profile sample-suite
python tools/project.py check --scope isolation
rg -n "samples/" .generated/hmi/main
rg -n "apps/main/app.cpp" .generated/hmi/sample-suite
rg -n "lv_tick_inc|lv_timer_handler|lv_init" hosts/hmi apps/main samples/suite
```

- 두 firmware build가 성공한다.
- main의 실제 source set에 `samples/`가 0건이다.
- sample-suite의 실제 source set에 `apps/main/app.cpp`가 0건이다.
- host/app에 저수준 LVGL runtime pump 호출이 없고 Board 경계가 유지된다.
- 큰 UI/port 객체가 Arduino loop stack에 생기지 않고 app은 persistent lifetime을 가진다.
- Dashboard 진입 시 모든 feature page widget/state를 eager allocation하지 않는다.

**수동 검증:** 이 단계에서는 장치 upload를 수행하지 않는다. P6 인계 절차만 준비한다.

**종료 조건**

- [X] 두 HMI build와 source 격리 검사가 통과한다.
- [X] 두 profile의 RAM/flash 차이와 주요 persistent/heap/stack lifetime이 기록되어 있다.
- [X] Codex가 upload·flash·장치 변경 명령을 실행하지 않았다.
- [X] P5 근거가 기록되기 전 P6 구현을 시작하지 않는다.

**완료 근거 (2026-11-07)**

- 최초 두 build는 성공했지만 generated wrapper의 `build_dir`이 뒤에서 읽는 공통 `platformio.ini`에 덮여 profile 전환 시 이전 build 결과가 제거되는 결함을 발견했다. profile별 마지막 extra config가 `.pio/build/hmi/<profile>`을 강제하도록 generator를 수정하고 version을 3으로 올렸다.
- generator 회귀 test에 profile별 `build_dir`과 wrapper의 중복 설정 금지를 추가했다. `python -m unittest discover -s tools/tests -v` 결과 manifest/generator 26개가 통과했다.
- 수정된 설정으로 `python tools/project.py build --host hmi --profile main`과 `--profile sample-suite`가 upload 없이 각각 성공했다. 두 결과는 `.pio/build/hmi/main/hmi-main/`과 `.pio/build/hmi/sample-suite/hmi-sample-suite/`에 동시에 유지된다.
- `main`은 RAM 27,892 B/512,000 B(5.4%), flash 645,594 B/6,553,600 B(9.9%)다. `sample-suite`는 RAM 28,124 B(5.5%), flash 654,432 B(10.0%)로 `main`보다 RAM 232 B, flash 8,838 B가 크다.
- 실제 firmware object와 `resolved_profile.json`을 비교해 `main` 8/8, `sample-suite` 13/13 source가 일치하고 diff가 0건임을 확인했다. main generated/source/object에는 `samples/`가 없고 sample-suite에는 `apps/main/app.cpp`가 없다.
- `python tools/project.py check --scope isolation`은 generator version 3의 `windows/main`, `windows/sample-suite`, `hmi/main`, `hmi/sample-suite`를 모두 확인했다. 두 firmware binary의 SHA-256은 각각 `D39D6BB4A80380BEE052BC18728BE42836771C009897EBCABE3AB04FC6E7AD05`, `C839D0DF98D911EB4C7117924AAF0958452A4B32E7155D3C070B8D9EFCD25A12`다.
- HMI host는 `Board`와 선택 app을 function static persistent object로 한 번만 생성하고 loop stack에는 reference만 둔다. `SampleSuiteApp`은 최대 2,304 B, `UiTheme`은 최대 2,048 B, `UiBasicsPage`는 최대 256 B로 compile-time guard가 있으며 active page 하나만 `unique_ptr`로 소유한다. descriptor 등록만으로 page widget/state를 만들지 않는다.
- async callback은 app pointer만 전달하고 destroy에서 예약 callback을 취소한다. page는 toast timer를 root보다 먼저 정리한다. production host/app의 `lv_init`·`lv_tick_inc`·`lv_timer_handler`는 0건이고 `Board::tick()`만 internal `LvglPort::tick()`에 위임한다. app/module의 board internal 접근과 Board/platform의 UI widget 책임도 0건이다.
- PlatformIO/ESP framework 자체 경고 두 종류는 이전과 동일하며 profile source 오류는 아니다. P5에서 upload, flash, serial device 변경을 실행하지 않았다.

**후속 VS Code PlatformIO 환경 노출 수정 (2026-11-07)**

- 루트 `platformio.ini`가 `.generated/hmi/*/profile.ini`를 glob으로 읽도록 변경해 생성된 모든 HMI profile을 VS Code PlatformIO 환경 목록에 노출한다. 현재 PlatformIO project config에서 `env:hmi-main`, `env:hmi-sample-suite` 두 환경을 확인했다.
- generated `profile.ini`는 environment별 source/include만 소유하고, CLI wrapper 전용 `selection.ini`가 선택 profile의 `default_envs`와 `.pio/build/hmi/<profile>` 경로를 마지막에 적용한다. 따라서 IDE의 다중 환경 노출과 기존 profile별 build directory 격리를 함께 유지한다.
- generator version을 9로 올리고 관련 회귀 검사를 갱신했다. Python 35개 test, 두 HMI firmware build와 generated isolation 검사가 통과했으며 upload·flash는 수행하지 않았다.

### P6. 문서, 반복 추가 검증과 실기기 인계

**목표:** 새 사용자가 단일 문서로 앱을 선택하고 Dashboard에서 샘플 page를 찾으며, 새 page 추가가 기존 완료 모듈을 수정하지 않는지 확인한다.

**의존성:** P5 `complete`.

**범위**

- 포함: `README.md`, CLI 도움말, clean generation/build, 임시 fixture page 추가/제거 검사, 사용자 HMI 확인 절차
- 제외: export/bundle/CI, 추가 정식 sample, upload 자동화

**산출물**

- profile 목록·doctor·Windows run·HMI build 절차
- generated/build/data 경로 설명과 cleanup 방법
- 임시 no-op page 등록 후 기존 app/page 파일 무변경 검사
- 사용자의 `sample-suite` Dashboard·`ui-basics` LCD·touch 수동 확인 절차

**자동 검증**

```powershell
python tools/project.py list
python tools/project.py doctor --host windows --profile main
python tools/project.py doctor --host windows --profile sample-suite
python tools/project.py check --scope isolation
python tools/project.py build --host windows --profile main
python tools/project.py build --host windows --profile sample-suite
python tools/project.py check --host windows --profile main
python tools/project.py check --host windows --profile sample-suite
python tools/project.py build --host hmi --profile main
python tools/project.py build --host hmi --profile sample-suite
git check-ignore .generated .pio
```

별도 test fixture page registry에서 임시 page를 추가·제거하고 descriptor 생성 결과만 바뀌는지 검사한다. production `apps/main/`, `samples/suite/app.*`, 기존 `samples/suite/pages/`, `boards/`, `platform/`, `hosts/`를 임시 검증 때문에 수정하지 않는다.

**수동 검증**

1. 사용자가 `README.md`만 보고 `main`과 `sample-suite`를 Windows에서 각각 실행한다.
2. Dashboard만 보고 각 page의 목적과 지원 상태를 이해할 수 있는지 확인한다.
3. 사용자가 직접 `sample-suite` firmware를 업로드한다.
4. 실제 LCD에서 Dashboard↔`UI Basics`, list, toast, confirm dialog가 잘리지 않는지 확인한다.
5. 실제 touch에서 Dashboard card, Back, press, long press, scroll, dialog button이 의도대로 구분되는지 확인한다.
6. 반복 page 이동과 overlay open/close 후 화면 떨림·reset·멈춤이 없는지 확인한다.
7. serial log에 초기화 실패, watchdog reset, 반복 allocation 실패가 없는지 확인한다.
8. 외부 공개·납품용 라이선스와 제3자 고지가 G4의 미완료 배포 게이트임을 인계 문서에 유지한다.

**종료 조건**

- [X] clean 상태에서 두 Windows profile과 두 HMI profile의 관련 검사가 통과한다.
- [X] 문서의 명령·profile ID·경로가 실제 구현과 일치한다.
- [X] 임시 page 추가/제거가 기존 app/page/production module 내부 수정 없이 통과한다.
- [X] 사용자가 Windows UI 검증 결과를 P6 수용 근거로 승인하고 실제 LCD·touch·serial 확인을 G4 수용 게이트로 이관했다.
- [X] 모든 P1~P6 단계가 근거와 함께 `complete`다.

실기기 항목은 원래 P6 필수 게이트였으나, 2026-11-07 사용자가 “대부분 UI이므로 Windows 검증을 충분한 P6 근거로 수용하고 다음 개발을 시작”하도록 범위를 변경했다. 이 결정은 실제 보드에서 관찰한 것으로 기록하지 않으며, LCD·touch·serial 검사는 아래 G4 실기기 수용 게이트로 이관한다.

**자동 게이트 완료 근거 (2026-11-07)**

- `README.md`를 현재 profile registry 기준으로 다시 작성했다. `main`/`sample-suite` 목록·doctor·Windows build/run·check·HMI build, 실제 executable/firmware 경로, 생성물 정리, sample page 추가 경계, G2 i18n/text-input 계획과 G4 외부 배포 게이트를 한 문서에 맞췄다. upload는 사용자 전용 절차로만 남겼다.
- `python tools/project.py --help`, `list`, Windows/HMI의 `main`·`sample-suite` doctor가 모두 종료 코드 0이었다. 문서의 오래된 `sample-ui`, 단일 `.pio/build/hmi-main`, 미구현 registry 설명은 0건이다.
- `tools/tests/test_generation.py`에 완전한 임시 저장소에서 no-op page를 등록·생성·제거하는 회귀를 추가했다. 생성 registry에만 임시 descriptor/source가 반영되고, 제거 후 생성물 byte가 baseline과 같으며 production app/page/Board/platform/host snapshot은 바뀌지 않는다. 전체 `python -m unittest discover -s tools/tests -v`는 27개가 통과했다.
- 기존 configure/object/executable과 HMI profile build directory를 치운 뒤, 다운로드된 Windows 제3자 dependency source cache만 재사용해 두 Windows profile을 다시 configure/build했다. `main` input/smoke 2개와 `sample-suite` navigation/state/interaction/smoke 4개가 모두 통과했고 isolation은 `windows/main`, `windows/sample-suite`를 확인했다.
- 새 실행 파일로 `main`, sample Dashboard, `UI Basics` screenshot을 다시 생성해 1024×600에서 제목·카드·버튼·상태·목록·scrollbar가 잘리거나 겹치지 않음을 확인했다. 이는 실기기 LCD 확인을 대신하지 않는다.
- 빈 profile build directory에서 HMI `main`과 `sample-suite`를 upload 없이 다시 빌드했다. `main`은 RAM 27,892 B(5.4%), flash 645,594 B(9.9%), `sample-suite`는 RAM 28,124 B(5.5%), flash 654,432 B(10.0%)였고 두 `firmware.bin`이 각각 profile별 경로에 생성됐다. 최종 isolation 재검사는 Windows/HMI의 두 profile, 총 네 조합을 모두 확인했다.
- `git check-ignore .generated .pio`는 두 경로를 모두 ignored로 판정했다. 이 P6 실행에서도 upload·flash·serial device 변경은 수행하지 않았다.
- 2026-11-07 사용자가 Windows 검증 결과를 P6 수용 근거로 승인했다. 따라서 P6은 `complete`로 닫고, 미수행 실기기 항목은 G4 배포 전 수용 게이트로 이관한다.

**G4로 이관한 실기기 수용 게이트**

- [ ] 사용자가 직접 `sample-suite` firmware를 업로드한다.
- [ ] 실제 LCD에서 Dashboard, `UI Basics`, list, toast, dialog가 1024×600 안에 정상 표시된다.
- [ ] 실제 touch에서 Dashboard card, Back, press, long press, scroll, dialog button이 구분된다.
- [ ] 반복 전환 후 화면 떨림·reset·멈춤·allocation 오류가 없다.
- [ ] serial log에 display·touch·LVGL 초기화 실패나 watchdog reset이 없다.

이 항목은 P6 완료 조건에서 제거됐지만 실제 보드 통과로 간주하지 않는다. 외부 공개·납품 또는 G4 완료 전에는 별도 확인해야 한다.

## 10. 변경 격리와 예상 후속 변경

| 후속 변경 | 바뀌어야 하는 위치 | 바뀌지 않아야 하는 위치 |
| --- | --- | --- |
| `text-input` page 추가 | `samples/suite/pages/text_input/`, `page_registry.json`, 필요한 text/font module | `apps/main/`, Dashboard 내부, `ui-basics`, Board/LvglPort 내부 |
| 새 page 전용 host adapter 추가 | 해당 page module과 `samples/suite/hosts/<host>/`의 조립 | generic host의 profile 분기, Dashboard, 다른 page |
| 새 ESP32-P4 HMI board 추가 | 새 `boards/<id>/`, HMI platform base/board 선택 조립 | 공통 app, 기존 board 내부, Windows port |
| manifest schema v2 도입 | schema, loader, sequential migration 또는 명확한 version 거부, fixture | 기존 manifest 의미를 조용히 변경하는 코드 |
| storage page 추가 | 해당 module, platform adapter, page module/descriptor | `ui-basics`, Dashboard, display/touch driver |
| 외부 asset page 추가 | 해당 page assets, asset module/generator, page descriptor | profile registry core와 Dashboard에 asset별 분기 |

새 page 때문에 `boards/jc1060p470c/internal/`, `platform/windows/sdl_lvgl_port.*`, Dashboard 내부가 수정된다면 먼저 책임 누수를 의심한다. 공통 의미와 규칙이 두 page 또는 두 host에서 실제로 같을 때만 `modules/`로 올리고, 코드 모양이 비슷하다는 이유로 추측성 범용 계층을 만들지 않는다.

## 11. 메모리·수명 기준

- Windows SDL port의 약 80 KiB draw buffer와 HMI double framebuffer ownership은 기존 platform/Board에 그대로 둔다.
- selected app concrete object는 host scope 또는 정적 persistent storage에 한 번만 존재한다.
- `SampleSuiteApp`은 Dashboard와 현재 활성 page 하나만 소유한다. page 전환 시 callback/timer를 먼저 해제하고 page widget/state를 정리한다.
- page descriptor table은 ID, title, 설명, capability, 작은 factory callback만 가진 bounded 정적 데이터다. descriptor 등록만으로 page widget이나 대형 state를 생성하지 않는다.
- `std::vector<std::string>` 같은 가변 대형 모델을 Dashboard나 `ui-basics`에 도입하지 않는다. scroll 항목은 작은 고정 중립 데이터로 제한한다.
- callback은 큰 object를 값으로 capture하지 않고 app pointer 또는 작은 ID만 전달한다.
- 생성된 profile source metadata는 Python/build-time 데이터이며 firmware에 runtime profile registry로 적재하지 않는다. page descriptor table만 sample-suite 내부 runtime navigation 입력으로 포함한다.
- app contract 변경 후 object 크기와 firmware RAM/flash를 기록한다. compile-time alias를 위해 virtual table이나 heap factory를 추가하지 않는다.

## 12. 위험과 대응

| 위험 | 대응 |
| --- | --- |
| manifest와 실제 build source가 달라짐 | 생성 source set metadata와 compile input을 최종 격리 검사에서 대조한다. |
| generator가 stale profile을 재사용함 | manifest hash, generator version, host/profile 경로를 metadata에 기록하고 불일치 시 재생성한다. |
| host에 profile별 분기가 다시 생김 | selected alias 하나만 include하고 host의 profile ID/type/path 검색을 종료 조건으로 둔다. |
| 모든 profile source를 컴파일한 뒤 flag로 선택함 | 명시적 source 목록만 build system에 전달하고 compile input에서 교차 source 0건을 확인한다. |
| Python 도구가 current working directory에 의존함 | script 위치에서 repository root를 결정하고 manifest 기준 상대 경로만 해석한다. |
| sample suite가 모든 기능을 eager 초기화해 firmware/heap이 비대해짐 | Dashboard에는 descriptor만 두고 활성 page 하나만 생성한다. 각 후속 page 계획에서 RAM/flash와 startup allocation 증가를 기록한다. |
| Dashboard가 page별 include와 `if/switch`로 커짐 | generated compile-time descriptor table만 소비하고 Dashboard의 page ID 직접 참조를 0건으로 검사한다. |
| 한 page의 실패가 전체 sample app을 중단함 | page `create()` 실패를 Dashboard의 해당 page 오류 상태로 돌리고 기존 Dashboard와 다른 page를 유지한다. |
| page 의존성을 위해 거대 service locator가 생김 | 첫 hardware page가 필요해질 때 page별 좁은 port와 host 조립을 추가하고 G1에서는 추측성 범용 service context를 만들지 않는다. |
| sample suite가 main UI를 재사용함 | 공통 의미가 입증된 component만 module로 추출하고 `samples/suite`의 `apps/main` 참조를 거부한다. |
| UI callback 중 page 삭제로 use-after-free 발생 | 전환 요청과 실제 교체 시점을 분리하고 Dashboard 왕복 test를 둔다. |
| toast/dialog timer가 page 파괴 후 실행됨 | active page 소유 timer 목록과 명시적 cancel/destroy 순서를 적용한다. |
| 여러 줄 dialog 문구가 잘림 | line spacing, 정렬, label bounds를 명시하고 Windows screenshot과 실제 LCD를 분리 검증한다. |
| profile별 build/data가 섞임 | `.generated/`, `build/`, `.pio/`, 향후 `.local/`을 host/profile ID로 분리한다. |
| compile 성공을 실기기 성공으로 오인함 | P6의 사용자 LCD·touch·serial 확인을 별도 필수 게이트로 유지한다. |
| Codex가 장치를 직접 갱신함 | compile만 실행하고 upload/flash는 사용자 절차로만 안내한다. |
| 2차 HMI 결함을 3차 구조 변경으로 우회함 | H6을 선행 게이트로 두고 발견된 기반 결함은 2차 계획에서 먼저 수정·검증한다. |

## 13. 계획 완료 후 다음 순서

이 계획이 완전히 `complete`가 되면 `_plan/2026-11/20261107_004_BilingualI18nAndTextInput.md`를 G2 첫 계획으로 실행한다. 그 계획은 `ko-KR`/`en-US` catalog 동등성, 런타임 화면 locale 전환, 화면 locale과 키보드 mode의 독립성, 두벌식 한글 조합·중간 편집·글자 수/UTF-8 byte 수·고정/동적 font 자산을 함께 다룬다.

설정·저장·자산·WAV를 한 번에 구현하지 않는다. profile registry와 Dashboard/page 계약이 첫 page에서 안정적임을 확인한 뒤 기능을 page 단위로 하나씩 추가하고, 각 page는 이번 계획의 격리·수명 기준을 그대로 통과해야 한다.

## Automated Verification Checklist

- [X] 2차 계획 H6이 사용자 확인 근거와 함께 `complete`다.
- [X] registry/profile schema version과 loader 판정이 일치한다.
- [X] 잘못된 version·중복 ID·누락 파일·경로 탈출·교차 profile 참조·namespace 충돌 fixture가 거부된다.
- [X] `list`와 Windows/HMI `doctor`가 `main`, `sample-suite`에 대해 통과한다.
- [X] generator 결과가 결정적이고 실패 시 부분 생성물을 남기지 않는다.
- [X] Windows와 HMI의 generated source set이 platform 경계를 지킨다.
- [X] generic host에 concrete profile 경로·type·분기가 없다.
- [X] Windows `main` build, smoke, input test가 통과한다.
- [X] Windows `sample-suite` build와 navigation/state/interaction test가 통과한다.
- [X] Dashboard가 descriptor table만 사용하고 page ID별 include/분기가 없다.
- [X] `sample-suite` screenshot에서 Dashboard, Back, 모든 필수 control과 여러 줄 문구가 잘리지 않는다.
- [X] HMI `main`과 `sample-suite` firmware build가 통과한다.
- [X] main source set에 `samples/`가 없고 sample-suite source set에 `apps/main/app.cpp`가 없다.
- [X] 앱에 platform/board 내부 의존이 없고 Board/platform에 app/widget 책임이 없다.
- [X] 큰 자동 local, 큰 callback capture, 중복 framebuffer/runtime registry가 없다.
- [X] 두 HMI profile의 RAM/flash와 주요 lifetime 검토 결과가 기록되어 있다.
- [X] `.generated/`, `.pio/`, profile별 build 생성물이 Git에서 제외된다.
- [X] 임시 page 추가/제거 검사가 기존 app/page/production module 수정 없이 통과한다.
- [X] 문서의 명령·ID·경로가 구현과 일치한다.
- [X] Codex가 upload·flash·장치 변경 명령을 실행하지 않았다.

## Manual Verification Checklist

- [X] P1 오류 문구가 수정 대상과 원인을 명확히 안내한다.
- [X] Windows `main`의 기존 다섯 문구와 pointer 상태 전환이 유지된다.
- [X] Windows `sample-suite`의 Dashboard↔`UI Basics`, Back, press, long press, scroll, toast, confirm/cancel이 동작한다.
- [X] Windows의 dialog/toast 문구가 균형 있게 정렬되고 잘리지 않는다.
- [X] 사용자가 Windows 검증과 갱신된 `README.md`를 P6 수용 근거로 승인했다.
- [X] 미수행 실기기 항목을 실제 통과로 오인하지 않고 G4 실기기 수용 게이트로 이관했다.
- [X] 사용자가 내부 재사용 권한을 확인했고 외부 배포 조건은 G4의 별도 게이트로 분리했다.
