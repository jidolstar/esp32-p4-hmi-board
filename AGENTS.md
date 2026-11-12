# ESP32-P4 HMI 프로젝트 작업 지침

- 이 저장소는 JC1060P470C ESP32-P4 HMI와 Windows 시뮬레이터가 앱 코드를 공유하는 범용 HMI 시작 프로젝트다. 실행 프로필은 `main`과 `sample-suite`이며, 소스·설정·자산 선택은 `profiles/registry.json`과 각 `profile.json`, 샘플의 `page_registry.json`을 따른다.
- 화면과 정책은 `apps/` 또는 `samples/`, 재사용 가능한 계약과 구현은 `modules/`에 둔다. `hosts/`는 선택 프로필을 조립하고 `platform/`은 호스트별 I/O를 구현한다. `boards/jc1060p470c/`는 물리 장치와 공개 Board API를 소유한다.
- Windows 소스나 빌드 입력을 바꿨으면 영향받은 프로필을 `python tools/project.py build --host windows --profile <id>`로 빌드한 뒤 같은 프로필에 `check`를 실행한다. HMI 빌드 범위와 실기기 확인은 아래 검증 스킬을 따른다.
- 자동 작업은 펌웨어 업로드·플래시와 실제 USB 자산 전송 등 연결된 장치에 쓰는 작업을 실행하지 않는다. 빌드와 로컬 검증은 수행하고 실기기 확인 절차는 사용자에게 안내한다.

## 작업별 스킬

공유 스킬의 원본은 `.agents/skills/`다. 아래 목록에서 현재 작업에 해당하는 `SKILL.md`를 열어 적용한다. 스킬 이름을 요청받았을 때도 같은 파일을 사용한다.

| 작업 | 스킬 |
| --- | --- |
| Board 공개 API, 내부 드라이버, LvglPort | [board-boundary-guard](.agents/skills/board-boundary-guard/SKILL.md) |
| C/C++의 책임, 수명, 소유권 주석 | [c-cpp-korean-comments](.agents/skills/c-cpp-korean-comments/SKILL.md) |
| 모듈 책임과 프로필·호스트 조립 | [change-isolated-composition](.agents/skills/change-isolated-composition/SKILL.md) |
| 변경 범위에 맞는 빌드·테스트 | [change-scoped-verification](.agents/skills/change-scoped-verification/SKILL.md) |
| Windows/HMI 공통 UI와 플랫폼 어댑터 | [cross-platform-ui-development](.agents/skills/cross-platform-ui-development/SKILL.md) |
| 저장소에 개발 계획 작성 | [development-plan-writer](.agents/skills/development-plan-writer/SKILL.md) |
| 임베디드 메모리, 버퍼, 수명 | [embedded-memory-safety](.agents/skills/embedded-memory-safety/SKILL.md) |
| 폰트 생성과 런타임 폰트 | [font-management-guide](.agents/skills/font-management-guide/SKILL.md) |
| LVGL 줄바꿈·행간·잘림 | [lvgl-multiline-text-layout](.agents/skills/lvgl-multiline-text-layout/SKILL.md) |
| 컴파일과 실기기 작업의 경계 | [no-direct-upload](.agents/skills/no-direct-upload/SKILL.md) |
| 여러 단계의 계획과 검증 기준 | [phase-gated-development](.agents/skills/phase-gated-development/SKILL.md) |
| UTF-8, 사용자 입력과 다중 폰트 렌더링 | [text-processing-guide](.agents/skills/text-processing-guide/SKILL.md) |
| UI 문구와 locale 카탈로그 | [ui-text-policy](.agents/skills/ui-text-policy/SKILL.md) |
| 저장소 문서와 계획의 경로 표기 | [use-relative-paths](.agents/skills/use-relative-paths/SKILL.md) |
| Windows의 PlatformIO native 진단 | [windows-native-gpp-test](.agents/skills/windows-native-gpp-test/SKILL.md) |
