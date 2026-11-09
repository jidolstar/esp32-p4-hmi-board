# 범용 HMI 시작 프로젝트: 기능 목록과 프로필 격리·배포 기준

작성: 2026-11-06 / 개정: 5차.

이 문서는 현재 starter의 구현된 기반과 후속 기능 목록을 함께 관리한다. S01~S02와 현재
실행 명령은 구현돼 있고, S03~S09의 경로·page ID·기능은 계획 상태다. 정확한 상태는
`_plan/2026-11/20261108_001_PageByPageDevelopmentRoadmap.md`를 기준으로 한다.

> 2026-11-07 4차 구성 결정: `main`은 독립 기본 앱으로 유지하고, 기능 샘플은 하나의 `sample-suite` 앱 첫 Dashboard에서 독립 page로 진입한다. 구체적인 profile/page 계약과 G1 구현 단계는 `_plan/2026-11/20261107_003_ProfileRegistryAndSampleSuiteDashboard.md`를 따른다.

> 2026-11-08 5차 범위 결정: 실제 가치가 있는 밝기·음량, Wi-Fi, SD/MSC, WAV,
> camera와 AP web control 중심으로 재편했다. 독립 assets page와 실제 RS485/I/O/USB command
> 샘플은 제거하거나 안내 범위로 축소했다.

## 1. 목적과 제공 수준

어떤 응용 프로젝트든 자신의 기능과 데이터를 추가할 수 있는 HMI 개발 기반을 만든다. 공통 UI·입력·저장·통신·오디오 기능, 플랫폼 어댑터, 독립 샘플, Python 도구를 제공한다. 이름·설명·문구·데이터·자산·설정 키는 모두 중립적으로 작성한다.

최초 지원 환경은 Windows 시뮬레이터와 현재 ESP32-P4 HMI 보드다. 다른 보드에 필요한 드라이버·핀·초기화는 보드 패키지로 교체할 수 있게 경계를 둔다. 지원 환경과 검증한 보드·해상도는 문서에 정확히 기록한다.

제공할 결과물:

1. 독립 Git 저장소에서 구성·빌드되는 공통 기반.
2. 새 응용 기능을 넣는 빈 기본 앱 `main`.
3. 첫 Dashboard에서 기능별 page로 이동하는 단일 `sample-suite` 앱과 정식 샘플 page 9개.
4. 필요할 때 추가하는 업데이트 page와 PC 관리 도구.
5. 원본·최소 생성 자산·fixture·의존성 명세·라이선스·검증 설정.
6. 처음 준비하는 사람의 유일한 시작 문서 `README.md`.

**배포 목표:** 필요한 개발 도구가 준비된 PC에서 Git clone 또는 pull 후, 문서의 준비 명령과 실행 명령으로 기본 앱 및 선택 샘플을 실행한다. 최초 의존성 다운로드에 필요한 인터넷과 도구 설치 조건은 미리 명시한다. 기존 작업 폴더·개인 설정·외부 운영 서버에 의존하지 않는다.

### 1.1 새 사용자의 두 가지 시작 경로

| 사용 목적 | 시작 방법 | 결과 |
| --- | --- | --- |
| 기반을 익히고 응용 기능 개발 | 저장소를 받고 main 실행 → `sample-suite` Dashboard에서 필요한 page 실행 → `apps/main/` 개발 | 한 앱에서 샘플을 탐색하면서 독립된 기본 앱을 유지한다. |
| 최소 구성으로 별도 프로젝트 시작 | main과 필수 모듈만 새 폴더로 export → 독립 빌드 확인 → 새 Git 저장소로 관리 | 샘플 소스·자산·등록 항목 없이 시작할 수 있다. |

두 경로 모두 시작 화면·클래스명·문구·데이터 모델에 특정 응용 분야의 전제를 넣지 않는다.
등록 WAV와 web switch는 사용법을 설명하는 중립 데이터이며 새 앱에 특정 업무 모델이나
동작 순서를 강제하지 않는다.

### 1.2 최소 실행과 추가 도구

| 사용할 기능 | 최초 준비 조건 | 최초 실행에서 요구하지 않는 작업 |
| --- | --- | --- |
| Windows main·기본 UI | Git, Python 3, 지원 C++ 컴파일러, CMake, Ninja, 최초 의존 다운로드 | Node.js·폰트 재생성·serial 연결·HMI SDK 설치 |
| 한글 입력·WAV 예제 실행 | 위 환경과 저장소에 포함된 최소 생성 자산 | 원본 전체 재변환·외부 음원 준비 |
| 폰트·이미지 원본 수정 | 해당 Python 환경과 생성기 의존, 폰트는 Node.js 도구 | 관계없는 샘플 의존 설치 |
| SD의 PC 접근 | HMI USB MSC firmware와 사용자가 수행하는 Windows 안전 제거 | HMI와 PC의 SD 동시 쓰기 |
| Wi-Fi AP web 제어 | 지원 ESP-Hosted 조합과 QR을 읽는 휴대폰 | cloud server·외부 인터넷·모바일 앱 설치 |
| HMI 펌웨어 빌드 | Python 3, 고정된 PlatformIO/SDK·보드 정의 | PC GUI·운영 서버·실제 장치 연결 |

지원 Python·컴파일러·SDK의 정확한 버전 범위는 새 저장소에서 실제 검증한 값으로 채운다. 설치 도구가 있음을 가정한 목표와 깨끗한 PC에서 필요한 준비 절차를 모두 제공한다.

## 2. 기본 앱과 샘플의 격리 규칙

### 2.1 코드와 빌드

- `apps/main/`은 새 프로젝트의 응용 코드 위치다. 기본 상태는 작은 빈 화면과 실행 상태만 제공한다.
- `samples/suite/`는 샘플 Dashboard와 page registry를 소유하고, `samples/suite/pages/<id>/`는 각 예제의 화면·상태·데이터·검사를 소유한다.
- `modules/`는 독립 기능을 제공한다. 기본 앱과 샘플은 필요한 모듈을 각각 연결한다.
- 의존 방향은 `main → modules`, `sample-suite → page registry → page → modules`, `host → 선택 프로필·어댑터`다. 모듈에서 main, suite, page를 참조하지 않는다.
- main이 sample-suite를 include·링크·호출하거나 sample을 통해 공통 기능을 얻지 않는다. page끼리도 구현을 참조하지 않는다. 공통 기능은 모듈로 옮긴다.
- Windows 타깃과 HMI 환경은 프로필별 소스·라이브러리·include 경로를 명시한다. 전체 소스를 재귀 수집해 나중에 플래그로 가리는 방식을 사용하지 않는다.
- include 경로·컴파일 정의·외부 라이브러리는 타깃 범위로 제한한다. 모듈 의존의 전이 관계까지 검사해 main에 샘플 의존성이 간접 유입되지 않게 한다.
- 프로필 하나를 선택하면 그 진입 구현 하나만 포함한다. 프로필 누락·중복·지원하지 않는 host 선택은 빌드 준비 단계에서 거부한다.
- main이 기본 프로필이다. sample-suite Dashboard는 별도 프로필에만 존재하며 기본 앱의 메뉴나 숨은 실행 분기로 넣지 않는다.
- sample-suite는 Dashboard와 활성 page 하나만 생성한다. 모든 page widget/state를 startup에 동시에 만들지 않는다.
- Dashboard는 page descriptor table만 소비하고 개별 page header나 ID별 분기를 소유하지 않는다.

### 2.2 자산·설정·실행 데이터

- page 전용 자산·설정·시나리오는 해당 page 폴더에 둔다. 모듈의 필수 공용 자산은 모듈 소유로 명시한다.
- 문구 ID·폰트 역할·생성 헤더도 main과 sample-suite, page 입력별로 분리한다. page 문구를 추가할 때 main이나 다른 page의 문구 목록·폰트·생성 소스가 바뀌지 않아야 한다.
- 빌드·캐시·로그는 host와 profile ID로 분리한다. 프로필 전환 때 다른 프로필 생성물을 재사용하지 않는다.
- 실행 데이터는 profile과 page ID로 분리한다. Windows는 개별 데이터 폴더, HMI는 충돌하지 않는 SD 경로와 NVS namespace를 사용한다. NVS 길이 제한 안의 이름을 manifest/descriptor에 명시하고 중복을 검사한다.
- sample-suite를 실행해도 main의 저장값·자산·기본값을 바꾸지 않는다. 한 page의 migration을 다른 page나 main 데이터에 적용하지 않는다.
- 파일 경로는 저장소 위치·실행 파일 위치·설정된 데이터 루트를 기준으로 해석한다. 실행 셸의 현재 폴더나 작성자 PC 경로에 기대지 않는다.

### 2.3 확인 가능한 격리 기준

- main의 실제 컴파일·include 의존·링크 입력·배포 manifest에 `samples/`가 없다.
- sample-suite build에는 등록 page가 함께 들어갈 수 있지만 page A source가 page B 구현을 include·호출하지 않는다.
- 샘플 전체를 제외한 임시 export에서도 main이 구성·빌드된다.
- main-only export의 profile registry에는 `main`만 남고 sample-suite/page 경로가 없다.
- page 하나를 추가·제거할 때 기존 main, Dashboard, 다른 page 내부를 수정하지 않는다. page registry 항목과 해당 page 폴더만 바뀐다.
- 프로필 전환 후 파일·설정·로그가 다른 프로필 경로에 생성되지 않는다.
- page 전환 후 비활성 page의 timer·callback·widget과 실행 데이터 handle이 남지 않는다.

## 3. 제안 디렉터리

아래 경로는 새 시작 프로젝트의 목표 구조다.

```text
README.md
CMakeLists.txt
CMakePresets.json
platformio.ini
profiles/
  registry.json                 # 등록 manifest 경로 목록: main + sample-suite
apps/
  main/
    profile.json
    app.*                       # 새 응용 프로젝트의 시작 코드
    hosts/                      # 해당 앱의 host별 조립
    assets/
    config/
modules/
  ui/                           # 화면 구성요소·이벤트 처리
  text/                         # UTF-8·한글 조합·편집
  assets/                       # 폰트·WAV manifest·검증·수명
  settings/                     # 밝기·음량 level과 저장 정책
  storage/                      # SD lease·USB MSC 소유권
  connectivity/                 # Wi-Fi 상태·연결 정책
  camera/                       # capture/gallery의 공통 port와 metadata
  interfaces/                   # RS485·I/O·USB command 최소 공개 계약
  audio/                        # 등록 WAV 재생 요청·상태·형식 검사
platform/
  windows/                      # SDL·로컬 파일·가상 장치
  hmi/                          # NVS·SD/MSC·Wi-Fi·camera·audio 어댑터
boards/
  jc1060p470c/                  # 보드 초기화·LCD·touch·장치 구성
hosts/
  windows/                      # 공통 진입점·이벤트 루프
  hmi/                          # 공통 setup/loop
samples/
  suite/
    profile.json
    app.*                       # Dashboard와 활성 page 수명
    page_contract.*
    page_registry.json
    hosts/
    tests/
    pages/
      ui-basics/
      text-input/
      display-audio/
      wifi/
      sd-storage/
      wav-sounds/
      camera-gallery/
      wifi-ap-web/
      interfaces-info/
tools/
  project.py                    # 준비·선택·빌드·실행·검사의 진입점
  common/                       # 도구 공통 경로·manifest 처리
  assets/                       # 공용 자산 생성·검사
  requirements/                 # 용도별 의존성과 고정 버전
cmake/                          # Windows 빌드 연결
scripts/                        # 배치·PowerShell 편의 실행기
licenses/
ci/                             # 검증할 host·profile 조합 목록
.generated/                     # 선택 프로필의 빌드 연결 결과, Git 제외
build/                          # host/profile별 생성물, Git 제외
.local/                         # 사용자 실행 데이터·로컬 설정, Git 제외
dist/                           # host/profile별 독립 실행 묶음, Git 제외
```

필요 없는 빈 page 폴더나 서비스는 선행 생성하지 않는다. 위 page 목록은 목표 구조이며 각 단계에서 구현할 폴더만 추가한다. 선택 확장인 update page와 PC GUI는 도입 시 추가한다. 보드 이름을 바꾸는 것만으로 새 하드웨어를 지원한다고 취급하지 않는다.

## 4. 프로필 등록 계약

### 4.1 등록 단위

실행 프로필은 기본 앱 `main`과 샘플 앱 `sample-suite`다. 모든 기능 샘플은 sample-suite의 공통 page 계약을 구현하며, Windows와 HMI가 같은 page 로직을 실행한다. 가상 장치와 실제 장치의 차이는 page가 등록한 host별 어댑터로 처리한다.

manifest에서 선언할 항목:

| 항목 | 내용 |
| --- | --- |
| ID·종류·설명 | `main` 또는 `sample-suite`, `application` 또는 `sample`, 중립적인 기능 설명 |
| 공통 소스 | 앱 화면·상태·이벤트 처리와 등록 page의 명시적 파일 목록 |
| host별 소스 | Windows/HMI 앱 조립과 page별 필요한 어댑터·보드 요구사항 |
| 모듈 의존 | 연결할 공통 모듈과 필수 외부 라이브러리 |
| 자산·config | 필수 파일 목록·생성 규칙·기본 설정·포맷 버전 |
| 데이터 격리 | 로컬 데이터 루트·SD 하위 경로·NVS namespace |
| 실행 backend | `mock` 기본값, 지원하는 실제 장치 backend와 필수 인자 |
| 검증 | 코어 검사·가상 시나리오·필요한 수동 확인 |

새 profile 등록 절차는 `profile.json` 작성 → profile registry에 경로 추가 → manifest 검사 → 선택 프로필 빌드·실행이다. 새 기능 page 등록 절차는 `samples/suite/pages/<id>/` 작성 → page registry에 descriptor 추가 → manifest/page 검사 → sample-suite 빌드·실행이다. 생성 코드는 registry에서 만들고 직접 편집하지 않는다. 공통 host와 Dashboard에 page별 include·분기를 추가하지 않는다.

manifest 자체에도 schema 버전을 둔다. 파일 경로는 manifest 위치를 기준으로 해석하고, 공통 모듈은 등록된 모듈 ID로 참조한다. 프로필 소스에 다른 프로필 경로를 직접 넣거나, 선언하지 않은 폴더를 wildcard로 포함하지 않는다. 누락 파일·잘못된 모듈 ID·중복 데이터 namespace는 실행 전에 구체적인 경로와 함께 오류로 보고한다.

profile 등록 방식은 `main`과 `sample-suite`의 build 입력을 선택하는 작은 도구로 제한한다. page registry는 sample-suite firmware 안의 작은 정적 descriptor/factory table을 생성하며 runtime plugin이나 디렉터리 탐색으로 확장하지 않는다. 빌드 입력은 선언한 경로와 허용한 모듈로 제한하고 page 간 구현 참조를 거부한다.

### 4.2 수명과 실행 계약

- 프로필은 초기화 성공/실패를 반환하고, 반복 처리와 종료 정리를 제공한다.
- host가 LVGL·시간·이벤트 루프를 관리한다. 프로필이 별도의 LVGL pump를 시작하지 않는다.
- 각 프로필과 page는 필요한 의존성만 명시적으로 받는다. 모든 기능을 담은 전역 context나 service locator는 만들지 않는다.
- host는 보드·서비스·어댑터를 소유하고, sample-suite와 활성 page는 화면·상태를 소유한다. 빌린 객체·자산은 이를 참조하는 page보다 오래 유지한다.
- 긴 파일 읽기·통신·오디오 작업은 worker에서 수행하고 UI는 작은 결과만 받아 표시한다.
- 종료 시 화면·타이머·콜백·worker를 정리한다. 시간은 단조 시각을 사용하고 경과 시간과 달력 시각을 구분한다.

### 4.3 목표 사용자 명령

아래는 후속 구현할 CLI의 예시다. 실행기는 이 명령에서 프로필 선택·manifest 검증·빌드 연결 생성을 처리한다.

```powershell
python tools/project.py list
python tools/project.py doctor --host windows --profile main
python tools/project.py bootstrap --host windows --profile main
python tools/project.py run --host windows --profile main
python tools/project.py run --host windows --profile sample-suite
python tools/project.py check --host windows --profile sample-suite --page rs485
python tools/project.py check --host windows --profile sample-suite --page wav
python tools/project.py build --host hmi --profile sample-suite
python tools/project.py check --scope isolation
python tools/project.py export --profile main --out build/exports/new-app
python tools/project.py bundle --host windows --profile sample-suite
```

`run`은 필요한 구성·빌드를 포함하고 HMI에는 제공하지 않는다. HMI `build`는 펌웨어 파일 생성까지 수행한다. 실제 장치 기록은 사용자가 별도 절차로 수행한다. 로컬 도구 설치·준비 명령은 선택한 프로필에 필요한 의존성만 준비한다.

`export`는 선언된 의존 관계를 따라 소스·필수 자산·도구·라이선스·일치하는 빌드 설정·단일 문서를 새 폴더에 작성한다. Git 이력·개인 데이터·캐시·선택하지 않은 샘플은 포함하지 않는다. 존재하는 출력 폴더를 덮어쓰지 않는다. 결과 폴더 안에서 같은 main 준비·실행 명령이 동작해야 한다.

`bundle`은 실행 파일·필요한 DLL·자산·기본 config·버전/해시 manifest를 `dist/<host>/<profile>/`에 모은다. 배포 묶음에 컴파일 당시의 소스 폴더가 없어도 실행되어야 한다. 사용자 쓰기 데이터는 묶음의 고정 자산과 분리된 데이터 루트에 저장한다.

## 5. 정식 샘플 목록

모든 항목은 `sample-suite` Dashboard에 등록되는 독립 page이며 기본 앱 `main`에는 포함되지
않는다. page 구현 순서와 상세 종료 기준은
`_plan/2026-11/20261108_001_PageByPageDevelopmentRoadmap.md`를 따른다.

| ID | Page ID | 제공 기능 | Windows 경로 | HMI 경로 |
| --- | --- | --- | --- | --- |
| S01 | `ui-basics` | Dashboard 왕복·기본 component | SDL 입력 | LCD·touch |
| S02 | `text-input` | 한·영 UI/키보드·조합·편집 | 공통 입력 화면 | 공통 입력 화면 |
| S03 | `display-audio` | 한 화면의 밝기·음량·mute·저장 | 결정적 mock | backlight/audio/NVS |
| S04 | `wifi` | STA 검색·연결·해제·IP·복원 | 가상 AP fixture | ESP-Hosted STA |
| S05 | `sd-storage` | SD 상태와 HMI↔PC USB MSC 소유권 전환 | MSC 상태 mock | SD·USB MSC |
| S06 | `wav-sounds` | 등록된 여러 WAV의 단순 재생 | playback mock | SD streaming·audio |
| S07 | `camera-gallery` | preview·촬영·SD 저장·목록·보기·삭제 | camera/gallery mock | camera·SD |
| S08 | `wifi-ap-web` | AP 접속정보·QR·웹/HMI 4-switch 동기화 | network/browser mock | ESP-Hosted AP·web server |
| S09 | `interfaces-info` | RS485·I/O·USB command 기반/미구현 안내 | 공통 안내 화면 | 공통 안내 화면 |

### 구현 경계

- S03은 밝기와 음량을 한 page에 둔다. locale/theme/RTC는 포함하지 않는다.
- S04는 STA만 소유하고 AP mode는 S08이 소유한다.
- S05는 일반 CRUD 예제가 아니라 PC가 SD를 removable disk로 접근하는 MSC 흐름을 소유한다.
- S06의 WAV는 asset manifest에 등록하지만 독립 `assets` page는 만들지 않는다.
- S07의 사진은 runtime data이며 build asset이 아니다.
- S08은 AP 연결 QR과 local URL QR을 구분하고, captive portal 자동 열기를 필수로 보지 않는다.
- S09는 실제 RS485 frame, GPIO/PWM 동작, USB command protocol을 구현하지 않는다.
- update, PC GUI와 실제 interface protocol은 정식 page 완료 뒤 별도 범위로 계획한다.

## 6. Python 도구 구성

공통 도구는 `tools/`, page 전용 도구는 `samples/suite/pages/<id>/tools/`에 둔다. Python 도구도 `sample-suite` profile ID, page ID, manifest와 데이터 루트를 사용한다. 기본 앱 준비에 모든 sample-suite 의존성을 설치하지 않는다.

| 도구 | 제공할 기능 | 필요 의존 | 연결 프로필 |
| --- | --- | --- | --- |
| 프로젝트 실행기 | 목록·환경 진단·준비·manifest 검사·구성/빌드·Windows 실행 | 기본 진입은 Python 표준 라이브러리, host 개발 도구 | main·전체 |
| 텍스트/고정 폰트 생성 | YAML+TTF→문구 코드·폰트·manifest·일치 검사 | PyYAML·fonttools·Node.js·고정된 `lv_font_conv` | text-input |
| 동적 폰트 생성 | 문자 범위·크기별 분할 폰트와 manifest | fonttools·Node.js 도구 | text-input |
| QR/이미지 생성 | 고정 QR·필요 image asset 생성과 검사 | 고정된 생성 도구 | wifi-ap-web |
| 자산 목록 검사 | 허용 경로·형식·크기·해시·버전 검증 | 표준 라이브러리 중심 | 자산 사용 프로필 |
| WAV 도구 | 등록 음원 header·길이·hash·권리 metadata 검사 | 기본 동작은 표준 라이브러리 | wav-sounds |
| Gallery fixture | 제한된 사진·잘린 파일·목록 metadata 생성 | Pillow 또는 고정 fixture | camera-gallery |
| Web 검사 | 정적 bundle·4-switch protocol·browser 동기화 검사 | 선택 browser test 도구 | wifi-ap-web |
| 선택 자산 전송 | 계획 조회·전송·검증·실패 복구 | pyserial·대응 장치 수신기 | 자산 사용 프로필 |
| 선택 릴리즈 도구 | 버전·manifest·해시·서명·패키지 검사 | 명시한 Python/Node.js 도구·별도 키 | update |
| 선택 PC GUI | 포트 선택·worker 수명·진행률·실패·재시도 | PySide6·pyserial, 추가 기능별 의존 | update 도구 |

도구의 로컬 검사와 mock 실행은 실제 포트에 자동 연결하지 않는다. 장치 포트·출력 대상·쓰기 범위는 인자로 받는다. 기존 도구의 호환 명령을 가져올 때 장치의 지원 버전을 먼저 확인한다.

용도별 requirements와 lockfile을 제공하고 CI에서 설치를 확인한다. 기존 가상환경과 `node_modules/`는 복사하지 않는다. Node.js·FFmpeg·서명 도구가 필요한 선택 기능은 준비 단계에서 이를 명시한다. 공통 경로 처리·결과 형식만 공유하고 서로 다른 프로토콜을 억지로 합치지 않는다.

생성기는 profile manifest 또는 명시한 입력/출력 경로를 받는다. 특정 화면의 목록이나 저장소 루트의 공용 자산 폴더를 암묵적 기본값으로 사용해 다른 프로필 결과를 덮어쓰지 않는다.

## 7. Git에서 받아 시작하는 배포 기준

### 7.1 저장소에 포함할 것

- 공통 소스·보드 패키지·프로필 registry/manifest·빌드 설정·실행기·단일 시작 문서.
- 원본 문구·이미지·폰트와 필요한 라이선스·출처·재생성 명세.
- 최초 실행에 필요한 최소 생성 폰트·이미지·WAV와 manifest. 기본 실행에서 전체 자산 재생성을 요구하지 않는다.
- 작은 정상/오류 fixture와 가상 시나리오. 큰 선택 자산은 고정 버전·해시의 다운로드 경로와 검증 절차를 제공한다.
- 도구 의존성 고정 파일·지원 컴파일러/SDK·LVGL/SDL 버전·CI 정의. 다운로드 소스는 고정 버전과 가능한 경우 commit 또는 해시로 묶는다.
- `.gitignore`, 줄바꿈 정책, 로컬 설정 예시와 데이터 경로 규칙.

제외할 것: 개인 경로, 실행 데이터, 캐시, 바이너리 빌드 폴더, 개발 가상환경, 비밀키, 인증정보, 로컬 serial 포트 고정값, 실제 서버 연결을 필수로 하는 기본값.

### 7.2 첫 실행과 pull 이후

1. `doctor`가 필요한 도구·버전·경로·프로필 자산을 검사하고 누락 항목의 해결 방법을 표시한다.
2. `bootstrap`이 선택 host/profile의 의존성을 준비한다. 컴파일러·SDK 설치가 필요한 경우 문서의 명시 절차를 안내한다. 시스템 전체 설정을 임의로 바꾸지 않는다.
3. `run`이 구성·빌드·실행을 처리한다. GUI 실행에도 현재 작업 폴더와 무관하게 자산을 찾는다.
4. 새 기능 개발은 `apps/main/`에서 시작한다. 샘플은 별도 `sample-suite` profile을 실행한 뒤 Dashboard에서 page를 선택한다.
5. pull 후 의존성·manifest·생성물 버전이 달라졌다면 필요한 준비만 갱신한다. 사용자 데이터는 유지하고 schema 변환 실패를 명시한다.
6. 경로에 공백·비ASCII 문자가 있는 새 폴더와 기존 개발 폴더 밖의 위치에서 같은 절차를 검증한다.

단일 시작 문서에는 다음 수준의 시작 흐름을 그대로 제공한다. 아래 주소와 명령은 새 저장소용 목표 예시다.

```powershell
git clone <starter-repository-url> new-app
Set-Location new-app
python tools/project.py doctor --host windows --profile main
python tools/project.py bootstrap --host windows --profile main
python tools/project.py run --host windows --profile main
```

기본 앱의 첫 화면에는 앱 이름·선택 프로필·host·실행 상태를 표시한다. 소스 수정과 재실행을 설명하는 첫 실습은 `apps/main/`의 화면 제목 변경으로 고정한다. 시작을 위해 manifest·핀·설정값·생성 코드를 먼저 손으로 고칠 필요가 없어야 한다.

기존 사용자는 자신의 변경을 보존한 정상적인 Git 갱신 뒤 같은 준비·실행 명령을 사용한다. pull 이후 동작을 확인하려고 저장 데이터를 삭제하거나 전체 환경을 매번 다시 설치하게 하지 않는다. 자동 변환이 불가능한 버전 차이는 필요한 조치와 실패 이유를 표시한다.

### 7.3 CI와 배포 수용 기준

- main과 sample-suite의 Windows build, page별 mock 검사 조합을 고정 목록으로 관리한다. 선택 확장은 별도 job으로 둔다.
- HMI는 main/sample-suite의 실제 포함 소스와 page별 실제 어댑터를 확인하는 최초 배포 게이트를 둔다. 이후 변경은 영향받는 page와 공통 계약에 맞춰 검증한다.
- 의존성 검사는 실제 빌드의 source/include/link 입력과 자산 manifest를 확인한다. 문자열 검색만으로 격리 완료를 판정하지 않는다.
- 샘플을 제외한 export에서 main을 빌드하는 검사를 둔다. 샘플 폴더에 숨어 있는 필수 코드를 발견해야 한다.
- CI 캐시가 없는 환경에서 최초 준비·빌드가 성공하고 기본 샘플 자산에 추가 수작업이 필요하지 않아야 한다.
- tag/release에 지원 환경·검증 결과·알려진 제약을 기록한다. 소스 버전과 자산/도구 버전의 대응을 보존한다.

### 7.4 전달 직전의 완료 판정

| 확인 대상 | 통과해야 하는 결과 |
| --- | --- |
| 새 clone·최초 실행 | 문서의 준비 명령 이후 main 창이 열리고 누락 파일·개인 경로 오류가 없다. |
| 첫 수정 | main의 화면 제목 한 곳을 수정해 재실행하면 반영된다. |
| 밝기·음량 | mock 상태와 실제 HMI 적용·저장·재부팅 복원을 확인한다. |
| SD/PC 접근 | USB MSC 전환 중 SD 단독 owner, Windows 안전 제거와 HMI remount를 확인한다. |
| WAV 예제 | 등록 음원의 mock 상태 전이와 실제 HMI 출력, 음량 연계를 확인한다. |
| 카메라 | sensor 근거와 실제 촬영·저장·보기·삭제를 확인한다. |
| AP 웹 제어 | 휴대폰 AP 연결, URL QR, 웹/HMI 4-switch 양방향 동기화를 확인한다. |
| page 추가 | 새 page 폴더·descriptor 항목 추가만으로 Dashboard에서 실행된다. main, Dashboard, 기존 page 내부를 고치지 않는다. |
| 최소 export | 샘플 폴더·등록 항목이 없는 결과물을 별도 위치에서 구성·빌드·실행한다. |
| 실행 묶음 | 원본 저장소가 없는 위치에서 선택 프로필의 bundle을 실행하고 자산을 읽는다. |
| pull 이후 | 준비 명령을 반복 실행해도 정상이며 기존 사용자 데이터와 로컬 장치 설정을 보존한다. |
| 근거 보존 | commit/tag·도구 버전·검사 결과·실제 확인 범위를 배포 기록에 남긴다. |

위 결과를 확보한 버전에만 바로 시작할 수 있는 배포본이라는 상태를 부여한다. 현재 문서의 설계 완료와 배포본의 실행 검증 완료는 별도로 기록한다.

## 8. 단일 시작 문서 `README.md`

문서의 목적은 새 직원이 한 파일에서 준비·실행·수정·검증 방법을 찾게 하는 것이다. 다음 순서로 작성한다.

1. 지원 환경, 필수 도구와 설치/진단 방법.
2. clone·pull 후 최초 준비와 main 실행 명령, 기대 화면.
3. profile 목록, Dashboard page 목록과 Windows mock/실제 backend/HMI 지원표.
4. 디렉터리 책임과 기본 앱·샘플의 의존 규칙.
5. page별 진입 경로·수정 지점·오류 시나리오·관련 Python 도구.
6. 새 응용 기능과 새 sample page를 추가·등록하는 최소 절차.
7. 문구·폰트·WAV 생성과 검사, 자산 경로·버전 규칙.
8. 밝기·음량 저장, SD/MSC 소유권과 camera gallery 데이터 경로.
9. Wi-Fi STA, AP/QR/web switch와 WAV 실제 청취 확인 절차.
10. 빌드·관련 검사·HMI 파일 생성·사용자 장치 기록 절차.
11. pull 후 갱신 방법, 자주 발생하는 준비/실행 오류.
12. 라이선스·배포·선택 확장 조건.

최소 export를 만들 때에는 포함하지 않은 샘플의 실행 명령·등록 항목을 문서에서도 제거한다. 결과 프로젝트에서 실제 제공하는 기능과 문서가 일치해야 한다.

각 샘플 설명은 목적 → 명령 → 기대 결과 → 바꿀 파일 → 관련 검사 순서로 통일한다. 설명을 읽기 위해 다른 저장소나 기존 앱의 문서를 찾아야 하는 상태로 배포하지 않는다.

## 9. 구현 순서와 단계별 종료 기준

G1~G4는 배포 범위를 설명하는 상위 milestone로 유지한다. 실제 구현 순서는 2026-11-08부터 `_plan/2026-11/20261108_001_PageByPageDevelopmentRoadmap.md`를 따른다. 각 정식 page는 자신의 계획·자동 검증·Windows 수동 확인·HMI build·사용자 실기기 게이트를 모두 통과한 뒤 다음 page로 이동한다.

현재 S01 `ui-basics`와 S02 `text-input`은 `complete`, 재편된 S03~S09는 `pending`이다.
중복 `settings`와 독립 `assets` page는 제거했다. G1은 `complete`이고 G2·G3는 page 진행
상황을 요약하는 상위 묶음일 뿐 동시 구현 단위가 아니다. G4는 S09 완료 뒤 별도 계획으로
시작한다.

| 단계 | 목표·범위 | 산출물 | 자동 검증과 기대 결과 | 수동 검증 | 종료 조건·의존 |
| --- | --- | --- | --- | --- | --- |
| G1 기반·격리 | main, sample-suite Dashboard·ui-basics page, 공통 host, 프로필 등록·빌드 선택 | 별도 저장소 골격·실행기·두 프로필·Dashboard/page 계약·첫 문서 | `doctor --host windows --profile main`, `check --scope isolation`, main/sample-suite 선택 빌드. profile/page 등록 검증, main 격리, Dashboard의 page별 분기 0, main-only export 성공 | Windows의 main/sample-suite를 각각 실행해 Dashboard 왕복·버튼·팝업 확인 | 모든 검사·화면 근거 기록. 선행 없음 |
| G2 로컬 기능 | S02·S03·S05·S06·S07 page와 필요한 공통 module/asset | text input, 밝기·음량, SD/MSC, WAV, camera/gallery | 각 상세 계획의 단위 test, Windows check, HMI build, isolation | LCD·touch·brightness/audio·SD/USB·camera 확인 | 해당 S02/S03/S05/S06/S07 gate complete |
| G3 연결 기능 | S04·S08·S09 page | Wi-Fi STA, AP/QR/web control, interface 안내 | 상태/timeout/reconnect/browser mock과 HMI build | 실제 STA·AP·휴대폰 web 확인, 안내 지원 범위 검토 | 해당 S04/S08/S09 gate complete |
| G4 독립 배포 | 전체 정식 목록·라이선스·단일 문서·최초 사용·export/bundle | Git 배포본·CI·지원표·수용 기록 | 새 작업 폴더/무캐시에서 준비·main 실행·샘플 검사, 최소 export·독립 bundle | 처음 쓰는 직원의 문서 실행/첫 수정과 표시된 실제 기능 수용 | 미확인 기능을 지원 완료로 표시하지 않으며 G3 complete |

page 구현 순서는 S01→S02→S03→S04→S05→S06→S07→S08→S09로 고정한다. G2·G3 분류가
다르더라도 page를 건너뛰거나 여러 page를 동시에 구현하지 않는다. 선택 update page·PC GUI와
실제 RS485/I/O/USB command 구현은 정식 9개 page 완료 후 별도 범위와 게이트로 계획한다.
문서 변경만 있는 현재 작업에서는 코드 build·전체 검사·장치 연결을 실행하지 않는다.

## 10. 추출 검토 근거

다음은 202609 참조 프로젝트와 현재 starter에서 기능 구조를 확인한 경로다. 이 목록은 현재
starter가 참조 프로젝트를 include하거나 배포 의존성으로 가진다는 뜻이 아니다. 이식할 때
공개 계약·제품 결합·보드 결합·생성물 의존을 다시 분리한다.

| 기능 | 검토 경로 | 확인할 경계 |
| --- | --- | --- |
| 프로필 선택·빌드 | `../dev_202609_hmi/src/profile/`, `platformio.ini`, `profiles/` | 기존 제품 profile과 현재 명시적 등록/host source 분리 |
| 화면·입력 | `../dev_202609_hmi/src/ui/components/`, `modules/ui/`, `modules/text/` | 생성 문구·치수·외부 상태와 컴포넌트 수명 |
| 폰트·이미지 | `../dev_202609_hmi/src/ui/assets/`, `tools/assets/`, `samples/suite/generated/` | 원본·생성물·loader 버전과 host 파일 접근 |
| 밝기·음량 | `../dev_202609_hmi/src/model/level_settings.*`, `../dev_202609_hmi/src/platform/hmi/settings/` | level 범위·NVS·apply/preview port |
| Wi-Fi | `../dev_202609_hmi/src/connectivity/`, `../dev_202609_hmi/src/platform/hmi/connectivity/` | STA 상태/credential과 Hosted adapter, AP 신규 확장 경계 |
| SD·USB MSC | `../dev_202609_hmi/src/hardware/storage.*`, `../dev_202609_hmi/src/hardware/internal/usb_msc_driver.*` | exclusive owner·unmount/MSC/remount |
| WAV | `../dev_202609_hmi/src/hardware/audio.*`, `../dev_202609_hmi/src/hardware/internal/wav_reader.*`, `../dev_202609_hmi/src/diagnostics/audio_lab_catalog.*` | parser·streaming·asset catalog·보드 출력 |
| Camera | `boards/jc1060p470c/`와 선택 camera driver | sensor/pin/clock 근거·frame ownership·SD 저장 |
| RS485·I/O·USB command | `../dev_202609_hmi/src/hardware/rs485.*`, `../dev_202609_hmi/src/hardware/gpio.*`, `../dev_202609_hmi/src/platform/hmi/assets/` | public contract만 검토하고 실제 구현은 제외 |
| WAV fixture | `../dev_202609_hmi/test/audio_lab/`, `../dev_202609_hmi/tools/assets/sounds/` | 중립 음원·재현성·표준 라이브러리 |
| 선택 업데이트 | `../dev_202609_hmi/src/app/update/`, `../dev_202609_hmi/src/platform/hmi/update/` | 키·서버·설치 계약과 공통 엔진의 경계 |

## 11. 검증 체크리스트

### 이번 문서 수정의 자동 확인

- [x] 특정 응용 분야·브랜드를 전제하는 설명과 예시가 없다. 문서 검토와 관련 용어 검색 결과 0건.
- [x] 재편된 정식 샘플 9개가 sample-suite의 독립 page로 정의되어 있다. S01~S09 고유 page ID 확인.
- [x] main 소스·링크·자산·실행 데이터의 격리 기준이 명시되어 있다. 2절·4절·7절 검토.
- [x] 신규 경로·명령이 목표 설계이며 아직 구현되지 않았음을 표시했다. 문서 첫머리·4.3절·G1~G4 pending 상태 확인.
- [x] 추출 근거의 기존 파일·폴더가 존재하고 참조 경로가 상대 경로다. 기존 경로 40개 존재, 절대 경로 0건, 코드 fence 짝 확인.
- [x] Python 도구와 Git 최초 준비·pull 이후·무캐시 검증 기준이 포함되어 있다. 6절·7절 검토.
- [x] 2026-11-08 재검토에서 밝기·음량, Wi-Fi, SD/MSC, WAV, camera, AP web, interface 안내 순서로 재편하고 독립 assets page를 제거했다.

### 후속 구현의 자동 확인

- [ ] profile/page 누락·중복·타 host 소스·page 간 구현 의존을 준비 단계에서 거부한다.
- [ ] main과 sample-suite의 각 page가 자신에게 필요한 소스·모듈·자산만 사용한다.
- [ ] 샘플 제외 export에서 main이 빌드된다.
- [ ] 정식 page의 Windows mock 검사와 sample-suite HMI 빌드가 통과한다.
- [ ] 최초 실행 자산이 존재하고 선택한 생성기의 일치 검사가 통과한다.
- [ ] 새 폴더·무캐시·공백/비ASCII 경로에서 준비·실행한다.
- [ ] pull 후 기존 사용자 데이터 보존·필요 migration을 확인한다.
- [ ] main-only export에 남은 샘플 등록이 없고 결과물의 문서와 실행 기능이 일치한다.
- [ ] 실행 bundle이 원본 저장소 없이 동작하며 DLL·자산·config가 누락되지 않는다.
- [ ] mock에서 MSC owner 전환, WAV 교체/EOF, camera 저장/삭제, web switch reconnect를 자동 재현한다.

### 후속 구현의 수동 확인

- [ ] 새 사용자가 단일 시작 문서로 main과 sample-suite를 실행하고 Dashboard에서 page 하나를 수정한다.
- [ ] page를 추가·제거해도 main, Dashboard, 다른 page의 수정이 필요 없다.
- [ ] 실제 LCD·touch·brightness/audio·SD/MSC·Wi-Fi·camera 동작을 지원표와 대조한다.
- [ ] WAV의 실제 청취·음량·교체·오류 복구를 확인한다.
- [ ] 휴대폰으로 AP 연결, URL QR, web/HMI 4-switch 동기화를 확인한다.
- [ ] RS485·I/O·USB command가 안내 전용이며 실제 지원으로 오인되지 않는지 확인한다.
