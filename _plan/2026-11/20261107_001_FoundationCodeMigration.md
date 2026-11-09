# 범용 HMI 시작 프로젝트 1차 계획: 기반 코드 선택 이관

작성: 2026-11-07. 상태: `blocked` (현재 단계: F6 외부 배포 라이선스 수동 게이트).

상위 설계는 `_plan/2026-11/20261106_002_DomainNeutralHmiStarterInventory.md`를 따른다. 이 문서는 상위 설계의 G1을 한 번에 구현하지 않고, 가장 작은 실행 기반부터 확인하기 위한 첫 번째 실행 계획이다.

## 1. 목적과 결정 요약

기존 HMI 저장소의 기반 코드를 현재 작업공간으로 선택 이관해, 제품 기능과 외부 자산 없이 Windows에서 중립적인 `main` 화면 하나를 구성·빌드·실행할 수 있게 한다.

이번 계획의 핵심 결정은 다음과 같다.

1. 현재 작업공간을 새 스타터 저장소의 루트로 사용하고, 원본은 형제 폴더 `../dev_202609_hmi/`에서 읽는다.
2. 디렉터리 전체를 복사하지 않는다. 의존성을 확인한 허용 목록의 파일만 복사한 뒤 경로·namespace·문구를 중립화한다.
3. 첫 실행 대상은 Windows 하나로 제한한다. HMI 보드 초기화·핀·드라이버·PlatformIO 환경은 이번 범위에 넣지 않는다.
4. 최초 화면은 상품 화면이 아니라 스타터의 진단용 시작 화면으로 분류한다. 영어 ASCII와 LVGL 내장 Montserrat만 사용한다.
5. 복사한 코드의 원본 위치와 수정 이유를 기록한다. 이후 원본 변경을 자동 동기화하는 구조는 만들지 않는다.
6. 첫 계획이 완료된 뒤에만 프로필 registry와 `sample-ui`를 추가하는 다음 계획을 작성·실행한다.

## 2. 완료 시 얻는 결과

- 새 저장소에서 CMake와 Ninja로 Windows 실행기를 구성·빌드할 수 있다.
- `hmi_main_windows`를 실행하면 1024×600 SDL 창에 다음 네 정보가 표시된다.
  - `HMI Starter`
  - `Profile: main`
  - `Host: windows`
  - `Status: ready`
- 화면은 SD 카드, 생성 폰트, 이미지, 제품 config, 네트워크, serial 장치에 의존하지 않는다.
- UTF-8 순회 코어가 독립 라이브러리와 테스트로 존재한다.
- 원본 파일과 새 파일의 대응, 그대로 유지한 부분, 수정한 부분을 문서에서 추적할 수 있다.
- 다음 계획이 프로필 선택과 샘플 UI만 다루도록 빌드·호스트 경계가 준비된다.

## 3. 현재 상태와 확인 근거

### 3.1 대상 작업공간

현재 작업공간에는 `_plan/`과 저장소 스킬만 있으며 CMake 프로젝트와 Git 저장소는 아직 없다. 따라서 기존 빌드 구조와의 호환성을 유지해야 하는 제약 없이 최소 구조부터 만들 수 있다.

### 3.2 원본 저장소

원본 `../dev_202609_hmi/`에는 다음 기반이 있다.

- LVGL 8.4.0과 SDL2 2.30.12를 사용하는 Windows 빌드
- `include/lv_conf.h`와 사용자 지정 LVGL allocator 계약
- `simulator/windows/src/platform/sdl_lvgl_port.*`
- `simulator/windows/src/platform/lvgl_mem.cpp`
- `src/ui/lvgl/lvgl_mem.cpp`
- `src/utils/utf8_utils.*`

원본 작업 트리에는 서버 관련 미커밋 변경이 있으나 이번 허용 목록의 기반 파일에는 현재 diff가 없다. 실행 시점에도 다시 확인하며, 허용 목록 파일에 새 diff가 생겼다면 원본 commit과 작업 트리 중 어느 내용을 이관할지 먼저 기록한다.

### 3.3 그대로 옮기면 안 되는 부분

- `src/ui/theme.*`는 상품 폰트 역할과 생성 문구에 의존한다.
- `src/ui/components/`의 다수 파일은 상품 모델, 생성 이미지, 생성 문구, 사용자 폰트에 의존한다.
- `src/ui/navigation/product_screen_flow.*`는 요리·설정·Wi-Fi 등 제품 화면 전체를 참조한다.
- `src/profile/active_profile.*`는 기존 PlatformIO 매크로와 기존 제품 프로필을 선택한다.
- 기존 SDL 포트의 기본 창 제목은 제품/진단 화면 이름을 포함한다.
- 기존 `lv_conf.h`에는 제품·공장 화면용 글꼴 조건이 있으므로 그대로 복사하면 스타터가 불필요한 자산 계약을 상속한다.
- 기존 UTF-8 함수는 null-terminated 입력을 가정하고 잘린 multibyte 입력의 경계를 확인하지 않는다. 새 기반 API는 길이를 받는 계약으로 좁히고 오류 결과를 테스트한다.

## 4. 목표 구조

```text
CMakeLists.txt
CMakePresets.json
.gitignore
README.md
include/
  lv_conf.h
  hmi_lvgl_mem.h
cmake/
  Dependencies.cmake
apps/
  main/
    app.hpp
    app.cpp
hosts/
  windows/
    main.cpp
modules/
  text/
    CMakeLists.txt
    include/hmi/text/utf8.hpp
    src/utf8.cpp
    tests/utf8_test.cpp
platform/
  windows/
    CMakeLists.txt
    sdl_lvgl_port.hpp
    sdl_lvgl_port.cpp
    lvgl_mem.cpp
    lvgl_memory_stats.hpp
_doc/
  migration/
    foundation_source_map.md
build/                  # Git 제외
```

책임은 다음과 같이 나눈다.

- `modules/text/`: 플랫폼과 LVGL에 의존하지 않는 UTF-8 계약
- `platform/windows/`: SDL 표시·마우스 입력·Windows allocator·LVGL tick
- `apps/main/`: 중립적인 화면 객체 생성과 화면 소유 자원 정리
- `hosts/windows/`: LVGL 초기화, Windows port 수명, `main` 앱 생성, 이벤트 루프
- `cmake/`: 외부 의존 버전과 공통 빌드 연결

`apps/main/`은 SDL 헤더를 포함하지 않고, `platform/windows/`는 앱이나 화면 내용을 참조하지 않는다.

## 5. 선택 이관 목록

| 원본 | 목적지 | 처리 방식 |
| --- | --- | --- |
| `../dev_202609_hmi/src/utils/utf8_utils.hpp` | `modules/text/include/hmi/text/utf8.hpp` | 알고리즘을 참고해 길이 기반 입력과 명시적 오류 결과로 재구성 |
| `../dev_202609_hmi/src/utils/utf8_utils.cpp` | `modules/text/src/utf8.cpp` | 정상 1~4바이트 처리 유지, 잘린 입력·잘못된 continuation·범위 오류 검증 추가 |
| `../dev_202609_hmi/include/lv_conf.h` | `include/lv_conf.h` | RGB565·LVGL 8 설정을 기준으로 최소화하고 제품/공장 글꼴 조건 제거 |
| `../dev_202609_hmi/include/hmi_lvgl_mem.h` | `include/hmi_lvgl_mem.h` | C allocator 계약 유지, 설명을 플랫폼 중립적으로 정리 |
| `../dev_202609_hmi/simulator/windows/src/platform/sdl_lvgl_port.hpp` | `platform/windows/sdl_lvgl_port.hpp` | 창·입력·tick 책임 유지, 기본 제목 제거, namespace와 include 경로 정리 |
| `../dev_202609_hmi/simulator/windows/src/platform/sdl_lvgl_port.cpp` | `platform/windows/sdl_lvgl_port.cpp` | 기존 SDL/LVGL 연결 로직을 선택 이관하고 실패 정리와 중립 로그 확인 |
| `../dev_202609_hmi/simulator/windows/src/platform/lvgl_mem.cpp` | `platform/windows/lvgl_mem.cpp` | CRT allocator와 통계 기능 유지, Windows 전용 구현으로 제한 |
| `../dev_202609_hmi/simulator/windows/src/platform/lvgl_memory_stats.hpp` | `platform/windows/lvgl_memory_stats.hpp` | Windows 진단 계약으로 유지 |

다음 항목은 이번 계획에서 복사하지 않는다.

- `src/ui/theme.*`
- `src/ui/components/`
- `src/ui/navigation/`
- `src/ui/assets/`
- `src/profile/`
- `src/hardware/`
- `src/connectivity/`
- `src/app/`
- `assets/`, `i18n/`, 제품 config와 recipe 데이터
- 서버·업데이트·릴리즈·USB 업로드 도구

## 6. 단계별 실행 계획

F1~F5는 `complete`, F6은 `blocked`다. 구현과 자동 검증은 완료됐으며, 코드 소유·배포 라이선스 확인과 다음 계획 진행에 대한 사용자 승인을 기다린다.

### F1. 원본 기준과 이관 허용 목록 고정

**목표:** 어떤 원본 상태에서 어떤 파일을 옮기는지 재현할 수 있게 한다.

**작업**

1. `../dev_202609_hmi/`의 현재 branch, commit, 작업 트리 상태를 기록한다.
2. 허용 목록 파일의 diff 유무와 SHA-256을 기록한다.
3. `_doc/migration/foundation_source_map.md`에 원본→목적지 대응, 수정 이유, 제외 목록을 작성한다.
4. 새 저장소의 라이선스가 아직 결정되지 않았음을 기록한다. 내부 이관은 진행할 수 있지만 외부 배포 완료 판정은 라이선스 결정 전까지 보류한다.

**자동 검증**

- 허용 목록의 모든 원본 파일이 존재한다.
- 허용 목록 밖의 파일이 대상 소스 디렉터리에 복사되지 않았다.
- source map에 원본 commit과 각 파일의 상대 경로가 기록되어 있다.

**종료 조건**

- 원본 기준과 허용 목록이 문서에 고정되어 있다.
- 허용 목록 파일의 미커밋 상태가 명확하다.

**완료 근거 (2026-11-07)**

- `_doc/migration/foundation_source_map.md`에 branch `dev_202609_hmi`, commit `0161bc8b2dd6dbaeabb42c8086e8a2b07a4708bc`, 허용 파일 8개의 SHA-256을 기록했다.
- 기록 후 SHA-256 재검사 결과 `hashes_match=True`다.
- 허용 목록의 `git diff --name-only` 결과는 0건이다.
- F1 종료 시 대상의 `apps/`, `hosts/`, `modules/`, `platform/`, `include/` 아래 소스 파일은 0개다.

### F2. 독립 저장소와 최소 빌드 골격 생성

**목표:** 아직 앱 코드를 넣지 않아도 CMake가 독립적으로 구성되는 저장소 골격을 만든다.

**작업**

1. Git 저장소가 없다면 현재 작업공간에서 초기화한다.
2. `.gitignore`, `CMakeLists.txt`, `CMakePresets.json`, `cmake/Dependencies.cmake`를 만든다.
3. LVGL 8.4.0과 SDL2 2.30.12를 고정 버전으로 선언한다.
4. `build/`, `.generated/`, `.local/`, `dist/`를 Git 제외 대상으로 등록한다.
5. 타깃 이름만 선언하고 제품 저장소 경로나 전역 환경변수에 의존하지 않게 한다.

**자동 검증**

- `cmake --preset windows-debug`가 성공한다.
- CMake cache와 생성 파일을 제외한 추적 대상에서 원본 PC의 절대 경로가 검색되지 않는다.
- 저장소 루트 밖의 소스 파일을 빌드 입력으로 직접 참조하지 않는다.

**종료 조건**

- 깨끗한 build 폴더에 Windows 구성이 생성된다.
- 외부 의존 버전과 다운로드 조건이 명시되어 있다.

**완료 근거 (2026-11-07)**

- 현재 작업공간에 Git 저장소를 초기화하고 branch를 `main`으로 설정했다.
- CMake 4.4.1, Ninja 1.13.2, GNU C/C++ 16.1.0 환경에서 `cmake --preset windows-debug`가 성공했다.
- `cmake/Dependencies.cmake`에 LVGL `v8.4.0`, SDL `release-2.30.12`를 고정했다.
- build와 `.git`을 제외한 파일의 Windows 절대 경로 검색 결과는 0건이다.

### F3. 플랫폼 비종속 UTF-8 코어 이관

**목표:** 첫 공통 모듈을 제품 코드 없이 독립적으로 빌드·검사한다.

**작업**

1. 기존 UTF-8 순회 알고리즘을 `modules/text/`로 옮긴다.
2. 입력은 `std::string_view` 또는 동등한 길이 기반 계약으로 받는다.
3. 정상 코드포인트, 문자열 끝, 잘린 입력, 잘못된 continuation, overlong encoding, surrogate 범위, U+10FFFF 초과를 구분한다.
4. 오류 시 다음 offset 규칙을 문서화하고 무한 반복이 발생하지 않게 한다.
5. `hmi_text_core`와 `hmi_text_core_test` 타깃을 만든다.

**자동 검증**

- `cmake --build --preset windows-debug --target hmi_text_core_test`가 성공한다.
- `ctest --preset windows-debug -R '^hmi_text_core$' --output-on-failure`가 성공한다.
- 테스트가 ASCII, 2/3/4바이트, 한글, 빈 문자열, 잘린 입력과 잘못된 입력을 포함한다.
- `modules/text/`에서 LVGL, SDL, Arduino, ESP-IDF 헤더 참조가 0건이다.

**종료 조건**

- 공통 UTF-8 모듈이 독립 빌드되고 모든 경계 테스트가 통과한다.
- 원본과 달라진 API 계약이 source map에 기록되어 있다.

**완료 근거 (2026-11-07)**

- `hmi_text_core`와 `hmi_text_core_test`를 생성했다.
- GNU C++ 16.1.0에서 경고 옵션과 C++17로 빌드했다.
- `ctest --preset windows-debug -R '^hmi_text_core$' --output-on-failure` 결과 1/1 통과했다.
- ASCII, 2/3/4바이트, 한글, 빈 문자열, 범위 밖 offset, 잘린 입력, 잘못된 continuation, overlong, surrogate, U+10FFFF 초과를 검사한다.
- `modules/text/`의 LVGL·SDL·Arduino·ESP-IDF header 검색 결과는 0건이다.

### F4. Windows LVGL 플랫폼 기반 이관

**목표:** 화면 내용 없이 Windows에서 LVGL 표시·입력·tick을 제공하는 플랫폼 계층을 만든다.

**작업**

1. 최소 `lv_conf.h`와 custom allocator 계약을 만든다.
2. SDL port와 Windows LVGL allocator를 선택 이관한다.
3. 창 제목을 호출자가 반드시 전달하게 하고 기존 제품 이름 기본값을 제거한다.
4. `SdlLvglPort`는 창, renderer, texture, draw buffer를 소유하고 앱 객체를 소유하지 않게 한다.
5. 1024×40×RGB565 draw buffer는 약 81,920바이트다. 자동 지역 변수에 큰 port 객체를 만들지 않고 host가 정적 수명 또는 checked heap 소유로 유지한다.
6. 초기화 중간 실패에서도 이미 만든 SDL 자원이 정리되도록 한다.
7. `hmi_windows_lvgl` 라이브러리를 만든다.

**자동 검증**

- `cmake --build --preset windows-debug --target hmi_windows_lvgl`가 성공한다.
- `platform/windows/`에서 `apps/`, `samples/`, 제품 모델·자산 include가 0건이다.
- RGB565 크기와 draw buffer 계산에 대한 compile-time 검사가 존재한다.
- allocator의 0바이트, 할당, 재할당, 해제 통계 검사가 통과한다.

**종료 조건**

- Windows LVGL 플랫폼 라이브러리가 앱과 독립적으로 빌드된다.
- 메모리 크기와 소유 수명이 source map 또는 코드 주석에 기록되어 있다.

**완료 근거 (2026-11-07)**

- LVGL `v8.4.0`, SDL `release-2.30.12`를 내려받아 `hmi_windows_lvgl`을 빌드했다.
- `ctest --preset windows-debug -R '^hmi_windows_lvgl_memory$' --output-on-failure` 결과 1/1 통과했다.
- allocator의 0바이트, 할당, 재할당, 해제 후 현재 byte 복귀를 확인했다.
- `platform/windows/`의 앱·샘플·제품 model·제품 asset include 검색 결과는 0건이다.
- RGB565 `sizeof(lv_color_t) == 2`와 1024×40 draw buffer 81,920바이트를 `static_assert`로 고정했다.
- `SdlLvglPort`가 display·input·SDL 자원을 정리하고 host보다 짧은 lifetime을 갖도록 공개 계약을 기록했다.

### F5. 중립적인 main 앱과 Windows host 연결

**목표:** 최소 앱이 실제 Windows 창에서 실행된다.

**작업**

1. `apps/main/app.*`는 전달받은 LVGL root 아래에 시작 화면을 만들고 정리하는 책임만 가진다.
2. `hosts/windows/main.cpp`는 LVGL 초기화, port 생성, 앱 생성, 이벤트 루프, 종료 순서를 관리한다.
3. 첫 화면은 영어 ASCII와 내장 Montserrat를 사용한다. 생성 문구·상품 폰트·SD 자산에 의존하지 않는다.
4. `hmi_main_windows` 실행 타깃을 만든다.
5. 앱이 SDL 헤더를 포함하지 않고 host가 앱 내부 위젯을 직접 조작하지 않게 한다.

**자동 검증**

- `cmake --build --preset windows-debug --target hmi_main_windows`가 성공한다.
- 실행 파일의 `--smoke` 모드가 LVGL/SDL 초기화와 앱 생성 후 정상 종료한다.
- `apps/main/`에서 SDL, Arduino, ESP-IDF, `hardware/internal`, `../dev_202609_hmi` 참조가 0건이다.
- 추적 대상 소스에 요리·상품명·기존 브랜드·서버 주소·serial 포트 고정값이 없다.

**수동 검증**

- Windows 창이 1024×600으로 열린다.
- `HMI Starter`, `Profile: main`, `Host: windows`, `Status: ready`가 잘리지 않고 보인다.
- 마우스 이동이나 창 닫기 후 오류 없이 종료된다.

**종료 조건**

- 자동 검증이 모두 통과한다.
- 화면과 종료 동작의 수동 확인 근거가 기록되어 있다.

**완료 근거 (2026-11-07)**

- `hmi_main_app`과 `hmi_main_windows`를 생성하고 GNU C++ 16.1.0, C++17, 경고 옵션으로 빌드했다.
- `ctest --preset windows-debug -R '^hmi_main_windows_smoke$' --output-on-failure` 결과 1/1 통과했다.
- 독립 process의 `hmi_windows_sdl_escape`와 `hmi_windows_sdl_quit`에서 실제 SDL queue의 `SDLK_ESCAPE`와 `SDL_QUIT`가 `process_events()` 종료를 반환함을 확인했다.
- `build/windows-debug/hmi-main.bmp`를 생성해 1024×600, 네 문구, 중앙 정렬, 잘림 없음과 외부 자산 미사용을 확인했다.
- `apps/main/`의 SDL·Arduino·ESP-IDF·hardware internal·원본 저장소 include 검색 결과는 0건이다.
- 소스의 제품명·요리 용어·브랜드·고정 COM port 검색 결과는 0건이다.

### F6. 독립 실행 문서와 1차 인계

**목표:** 다음 작업자가 원본 저장소 문서 없이 이 기반을 구성·실행하고 다음 계획을 시작할 수 있게 한다.

**작업**

1. `README.md`에 지원 환경, 최초 구성, 빌드, 실행, 테스트 명령을 적는다.
2. 인터넷이 필요한 최초 LVGL/SDL 다운로드 조건을 적는다.
3. 구현하지 않은 HMI, 프로필 registry, sample, 자산 생성 기능을 명시한다.
4. 각 단계의 명령·결과·수동 근거를 이 계획에 기록한다.
5. 다음 계획 입력 목록을 실제 생성된 파일과 타깃 이름에 맞게 갱신한다.

**자동 검증**

- 새 build 폴더에서 `cmake --preset windows-debug`가 성공한다.
- `cmake --build --preset windows-debug --target hmi_main_windows hmi_text_core_test`가 성공한다.
- `ctest --preset windows-debug -R '^hmi_text_core$' --output-on-failure`가 성공한다.
- 문서의 명령과 실제 preset·target 이름이 일치한다.
- 추적 대상 파일에 원본 저장소 절대 경로, 개인 경로, build cache가 없다.

**종료 조건**

- F1~F6의 종료 조건과 증거가 모두 기록되어 있다.
- `main` Windows 실행과 UTF-8 코어 검사가 독립적으로 재현된다.
- 다음 계획 작성이 허용된다.

**현재 근거와 차단 항목 (2026-11-07)**

- `README.md`에 지원 환경, 최초 구성, build, 실행, test, 미구현 범위를 기록했다.
- 기존 cache와 분리한 `build/clean-check`에서 configure, 지정 대상 전체 build, CTest 5/5가 통과했다.
- `CMakePresets.json` JSON parsing이 통과하고 `windows-debug`에서 동일한 CTest 5개가 등록된다.
- build와 `.git`을 제외한 파일의 Windows 절대 경로 검색 결과는 0건이다.
- production build 정의의 `../dev_202609_hmi`, 개인 경로, 원본 저장소 직접 참조는 0건이다.
- 제품명·요리 용어·브랜드·고정 COM port 검색 결과는 0건이다.
- `build/` 생성물은 `.gitignore`에 의해 제외된다.
- 사용자가 다음 단계 계획 작성을 승인해 `_plan/2026-11/20261107_002_MinimalPlatformioHmiBootstrap.md`를 작성했다.
- 미완료 수동 게이트: 사용자의 원본 코드 소유·배포 라이선스 상태 확인.

## 7. 변경 격리와 후속 변형 점검

첫 계획 완료 후 예상되는 변경과 수정 위치는 다음과 같다.

| 후속 변경 | 바뀌어야 하는 위치 | 바뀌지 않아야 하는 위치 |
| --- | --- | --- |
| 새 프로필 추가 | `profiles/`, 새 `apps/` 또는 `samples/`, 생성된 빌드 연결 | `modules/text/`, `platform/windows/sdl_lvgl_port.*` 내부 |
| HMI host 추가 | `hosts/hmi/`, `platform/hmi/`, `boards/`, PlatformIO 연결 | `apps/main/` 화면 정책, Windows port |
| 새 공통 UI 컴포넌트 | `modules/ui/`와 사용하는 프로필 | UTF-8 decoder, host 이벤트 루프 |
| 해상도 변경 | host/board 표시 설정과 해당 레이아웃 | 텍스트 코어, profile registry schema |
| 외부 폰트 도입 | 자산 모듈, font manifest, 사용하는 sample | 무자산 진단 시작 화면 |

새 기능을 추가할 때 위 표의 `바뀌지 않아야 하는 위치`가 반복해서 수정된다면 경계가 잘못된 것으로 보고 다음 단계 진행 전에 조립 계약을 재검토한다.

## 8. 위험과 대응

| 위험 | 대응 |
| --- | --- |
| 원본 제품 코드가 연쇄적으로 유입됨 | 파일 허용 목록과 include 검색을 단계 종료 조건으로 둔다. |
| 원본 작업 트리의 미커밋 변경이 섞임 | F1에서 원본 commit, diff, hash를 기록하고 허용 목록별 상태를 확인한다. |
| 기존 `lv_conf.h`의 제품 글꼴 계약이 남음 | RGB565·allocator·필수 LVGL 기능만 남긴 최소 설정으로 다시 검토한다. |
| SDL port의 큰 draw buffer가 stack을 압박함 | 크기를 계산하고 port를 정적 수명 또는 checked heap으로 소유한다. |
| UTF-8 원본의 경계 밖 읽기를 그대로 가져옴 | 길이 기반 API와 malformed 입력 테스트를 첫 이관에서 추가한다. |
| Windows 빌드 성공을 HMI 지원으로 오인함 | 문서와 화면에 Windows host만 지원한다고 명시한다. |
| 라이선스가 없는 상태로 외부 배포함 | source map에 미결정을 기록하고 외부 배포 게이트에서 라이선스 결정을 필수로 둔다. |
| 계획 초반부터 범용 계층이 과도해짐 | 실제 필요한 text core와 Windows display port만 만들고 service locator나 범용 context를 만들지 않는다. |

## 9. 바로 이어질 다음 계획 요약

첫 계획이 완료되면 `_plan/2026-11/`에 **프로필 등록·선택과 `sample-ui` 계획**을 새 문서로 작성한다.

### 다음 계획의 목표

- `profiles/registry.json`과 versioned `profile.json` schema 정의
- `apps/main/profile.json` 등록
- `tools/project.py list`, `doctor`, `check --scope isolation` 구현
- registry/manifest에서 선택 프로필의 CMake 입력 생성
- `samples/ui/`에 화면 A/B, 버튼, 스크롤, toast, 확인창을 가진 독립 `sample-ui` 추가
- main과 sample-ui를 각각 선택 빌드하고 서로의 소스·자산이 섞이지 않음을 확인
- 샘플을 제외한 main-only export의 최소 골격 정의

### 다음 계획의 입력

- `hmi_text_core`
- `hmi_windows_lvgl`
- `apps/main/`
- `hosts/windows/`
- F1의 source map과 F6의 실제 검증 결과

### 다음 계획에서 계속 제외할 것

- HMI 보드 초기화와 장치 드라이버
- Wi-Fi, RS485, WAV, USB, 저장소, 설정 샘플
- 외부 상품 폰트와 제품 이미지
- 업데이트·서버·PC GUI

### 다음 계획의 완료 기준 초안

1. `python tools/project.py list`가 `main`과 `sample-ui`만 표시한다.
2. `main` 선택 빌드에는 `samples/` 입력이 없다.
3. `sample-ui` 선택 빌드에는 main 앱 구현이 없다.
4. 두 프로필을 각각 Windows에서 실행하고 예상 화면을 확인한다.
5. 잘못된 profile ID, 누락 manifest, 중복 ID를 구성 전에 거부한다.
6. 새 sample 프로필 추가가 기존 main이나 Windows port 내부 수정 없이 가능하다.

그 다음 계획은 위 프로필 계약이 안정된 후에 **HMI host·ESP32-P4 board package 이관**으로 잡는다. HMI 계획에서는 `Board` 공개 facade, PSRAM allocator, LCD·터치 초기화, PlatformIO 환경과 펌웨어 빌드를 다루되 장치 업로드는 수행하지 않는다.

## Automated Verification Checklist

- [x] 원본 branch·commit·작업 트리 상태와 허용 목록 hash가 기록되어 있다.
- [x] 허용 목록 밖의 원본 코드가 대상 소스 디렉터리에 복사되지 않았다.
- [x] `cmake --preset windows-debug`가 새 build 폴더에서 성공한다.
- [x] `cmake --build --preset windows-debug --target hmi_text_core_test`가 성공한다.
- [x] `ctest --preset windows-debug -R '^hmi_text_core$' --output-on-failure`가 성공한다.
- [x] `cmake --build --preset windows-debug --target hmi_windows_lvgl`가 성공한다.
- [x] `cmake --build --preset windows-debug --target hmi_main_windows`가 성공한다.
- [x] `hmi_main_windows --smoke`가 정상 종료한다.
- [x] 공통 모듈에서 LVGL·SDL·Arduino·ESP-IDF include가 0건이다.
- [x] 앱에서 SDL·하드웨어 내부 API·원본 저장소 경로 참조가 0건이다.
- [x] 추적 대상 파일에 개인 절대 경로·제품 데이터·고정 serial 포트·운영 주소가 없다.
- [x] `README.md`의 preset과 target 이름이 실제 빌드 정의와 일치한다.

## Manual Verification Checklist

- [x] 사용자가 첫 이관 범위와 제외 범위를 확인한다.
- [x] Windows 창이 1024×600으로 열리고 네 개의 시작 정보가 잘리지 않는다.
- [x] 독립 SDL event 검사에서 창 닫기와 Esc 입력으로 오류 없이 종료된다.
- [ ] 사용자가 원본 코드의 소유·라이선스 상태와 향후 외부 배포 조건을 확인한다.
- [x] 사용자가 F1~F6의 근거를 확인하고 다음 단계 계획 작성을 승인한다.
