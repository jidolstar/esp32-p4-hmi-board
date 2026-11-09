# 기반 코드 이관 원본 기록

기록일: 2026-11-07.

이 문서는 `_plan/2026-11/20261107_001_FoundationCodeMigration.md`의 F1 근거다. 원본 저장소의 디렉터리를 통째로 복사하지 않고 아래 허용 목록만 선택 이관한다.

## 원본 기준

- 원본 루트: `../dev_202609_hmi/`
- branch: `dev_202609_hmi`
- commit: `0161bc8b2dd6dbaeabb42c8086e8a2b07a4708bc`
- commit 요약: `docs: record firmware migration, factory workflow and handoff plans`
- commit 시각: `2026-11-06T23:19:50+09:00`

원본 작업 트리에는 `_plan/`과 `server/firmware/` 아래의 별도 미커밋 작업이 있다. 아래 허용 목록 파일의 `git diff --name-only` 결과는 2026-11-07 기록 시점에 0건이다. 따라서 1차 기반 이관은 기록한 commit과 동일한 허용 목록 파일을 입력으로 사용한다.

## 허용 목록과 SHA-256

| 원본 파일 | 바이트 | SHA-256 | 이관 목적지 |
| --- | ---: | --- | --- |
| `include/lv_conf.h` | 7,945 | `2375dddc3e963adb72943222120724a7e0326be014fabecc3fb26e4cab76af5d` | `include/lv_conf.h` |
| `include/hmi_lvgl_mem.h` | 280 | `be6bd931e2c0464be93561f597824b56ae0fab6f2fcc152c11b51e74ee768020` | `include/hmi_lvgl_mem.h` |
| `src/utils/utf8_utils.hpp` | 914 | `391897c8ad4d7d802c8cd6f8398db691a3baf8d10e081e2da87f18b267ef62e7` | `modules/text/include/hmi/text/utf8.hpp` |
| `src/utils/utf8_utils.cpp` | 1,502 | `cb8266ff352f83ad73a0970a9fa29eafbbcd5dd63319070e35f1c253b1f28b8d` | `modules/text/src/utf8.cpp` |
| `simulator/windows/src/platform/sdl_lvgl_port.hpp` | 1,669 | `08cd2f33164bee14756fcf6f9c4abb559daa23abecb1077f08999acf72028e28` | `platform/windows/sdl_lvgl_port.hpp` |
| `simulator/windows/src/platform/sdl_lvgl_port.cpp` | 4,780 | `cc89c298b1103a934289f0132f98fe438aa952d5f7a7e82ef08f520dda54610a` | `platform/windows/sdl_lvgl_port.cpp` |
| `simulator/windows/src/platform/lvgl_mem.cpp` | 981 | `cc177849ce20b90b8add92d83bc5b160bd5753f8d39b9c386e93188e2aae1d57` | `platform/windows/lvgl_mem.cpp` |
| `simulator/windows/src/platform/lvgl_memory_stats.hpp` | 267 | `29d5129a645a87c3a5d6256d255e3442d45c5a562f4a4abec46e0db24b54a17f` | `platform/windows/lvgl_memory_stats.hpp` |

## 파일별 변경 원칙

- `utf8_utils.*`: 원본 알고리즘을 참고하되 길이 기반 입력과 명시적 오류 결과로 바꾼다. 잘린 multibyte 입력의 경계 밖 읽기를 허용하지 않는다.
- `lv_conf.h`: RGB565와 필요한 LVGL 8 기능만 남긴다. 상품·공장 폰트와 생성 자산 조건은 제거한다.
- `hmi_lvgl_mem.h`: C ABI allocator 계약만 유지한다.
- `sdl_lvgl_port.*`: SDL 표시·마우스·tick 소유권은 유지하되 제품 이름 기본값을 제거하고 앱을 참조하지 않게 한다.
- `lvgl_mem.cpp`, `lvgl_memory_stats.hpp`: Windows CRT allocator와 통계를 Windows 플랫폼 내부에 둔다.

## 실제 이관 결과

- `modules/text/`는 원본의 null-terminated decoder를 그대로 복사하지 않고 `std::string_view` 입력과 `ok/end/invalid` 결과를 가진 독립 API로 변경했다.
- `include/lv_conf.h`는 RGB565, custom allocator, log, Montserrat 16/24/32만 남겼다.
- `platform/windows/sdl_lvgl_port.*`는 1024×600 표시와 40-line draw buffer를 유지하고 제품명 기본 제목과 key callback을 제거했다. Esc와 창 닫기는 port 자체 종료 event로 처리한다.
- Windows draw buffer 81,920바이트는 `SdlLvglPort`가 소유하고 host가 port를 heap에 생성한다.
- `apps/main/`은 원본 상품 UI를 복사하지 않고 LVGL 내장 글꼴만 사용하는 새 진단 시작 화면으로 작성했다.
- LVGL 8은 전역 deinit API가 없으므로 앱과 input·SDL 자원은 정리하고 마지막 display 전역 heap은 process 종료 시 OS가 회수한다. port 파괴 뒤 LVGL API를 다시 호출하지 않는다.

## 명시적 제외

다음 원본 경로는 F1 허용 목록에 없으며 1차 이관에서 복사하지 않는다.

- `src/ui/theme.*`
- `src/ui/components/`
- `src/ui/navigation/`
- `src/ui/assets/`
- `src/profile/`
- `src/hardware/`
- `src/connectivity/`
- `src/app/`
- `assets/`, `i18n/`
- `server/`, `tools/releases/`, 장치 업로드 도구

## 라이선스 상태

원본 루트에는 프로젝트 전체를 대상으로 하는 `LICENSE`, `COPYING`, `NOTICE` 파일이 확인되지 않았다. 외부 폰트·아이콘 하위 라이선스만 존재한다. 내부 이관과 검증은 진행하되, 새 저장소를 외부에 배포하기 전에는 코드 소유권과 배포 라이선스를 사용자가 확정해야 한다.
