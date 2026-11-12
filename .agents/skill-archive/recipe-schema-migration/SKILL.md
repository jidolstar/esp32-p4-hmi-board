---
name: recipe-schema-migration
description: Maintain recipe storage schema versions and sequential migration functions in this HMI repository. Use when changing persisted recipe fields, units, IDs, relationships, serialization or compatibility rules, or reviewing recipe data migrations. UI-only edits and ordinary recipe content changes do not require a schema bump.
---

# 요리 저장 구조 변경과 순차 마이그레이션

요리 저장 구조를 변경할 때 기존 저장본을 새 앱에서 안전하게 읽을 수 있도록 버전·변환 함수·테스트를 함께 변경한다. 현재 계약은 [저장 형식](../../../_doc/recipe_persistence_format.md)을 먼저 확인한다. 첫 출시 기준은 v1이며, 제품에 존재하지 않는 구버전이나 가짜 변환을 만들지 않는다.

## 버전 변경 판단

- 파일의 필드 추가·삭제·이름 변경, 타입·단위·의미 변경, ID/재료/인분 관계 변경은 저장 호환성 영향부터 확인한다. 현재 codec은 알 수 없는 필드도 거절하므로 필드 추가를 자동으로 하위 호환이라고 판단하지 않는다.
- 이전 파일에 구조적 변환이 필요한 변경은 `schema_version`을 올리고 직전 버전에서 새 버전으로 가는 변환을 추가한다. 버전만 올리거나 변환 없이 기존 파일을 덮어쓰지 않는다.
- UI 문구·정렬, 사용자 요리 내용 변경, 저장 형식이 같은 내부 리팩터링에는 schema를 올리지 않는다. config 제한 변경만으로 데이터를 잘라내거나 schema 변환을 만들지 않는다.
- `schema_version`은 구조 버전, `revision`은 저장 변경 번호, `release_version`은 메뉴 배포 버전이다. 서로 대체하지 않는다.

## 변경할 책임과 위치

| 위치 | 변경 책임 |
| --- | --- |
| `src/platform/shared/recipes/recipe_codec.hpp`의 `kRecipeSchemaVersion` 및 codec 구현 | 현재 구조의 읽기·쓰기·검증 |
| `src/platform/shared/recipes/recipe_migrator.*`의 `recipe_migrator()` | 수명이 유지되는 `RecipeMigrationStep` 배열과 연속 변환 경로 등록 |
| 같은 디렉터리의 버전별 변환 구현 | 특정 `N → N+1` 입력 검증·값 매핑·출력 |
| `RecipeRepository` | 원본 은행 보존, 최종 검증, A/B 확정과 revision 증가 |
| `RecipeCatalogService` / 편집 화면 | worker 상태 전달, 진입 시 실행, 진행률·실패·완료 확인 |

일반적인 schema 추가는 codec·해당 변환·registry·테스트·문서에 한정한다. 저장 프로토콜이나 화면 계약이 그대로면 repository/service/UI를 버전별로 분기하지 않는다. `metadata.json` 또는 `commit` 포맷 자체를 바꾸는 경우에는 catalog 변환만으로 해결된 것으로 간주하지 말고 별도 읽기 호환성·원자적 확정 방식을 설계한다.

## 변환 구현

1. **현재 codec을 바꾸기 전에** 이전 형식의 fixture와 기대 데이터를 확보한다. 배포된 버전의 필드·단위·기본값·제한을 문서화하고 역사적 parser/변환 의미를 보존한다.
2. 변경 필드마다 이전 값 → 새 값 규칙을 정한다. 새 필수 값은 근거 있는 기본값을 정의한다. 이름·ID·슬롯의 빈칸·재료 순서·인분별 설정·배포 메타정보를 의도 없이 바꾸지 않는다. 표현할 수 없는 값이나 손실 없는 매핑이 불가능한 데이터는 실패로 반환한다.
3. `CodecResult convert(RecipeInput, RecipeOutput)` 계약으로 **한 단계** 변환을 작성한다. 입력 버전과 필수 값·중복·범위·UTF-8을 검증하고 다음 버전 문서를 출력한다. 현재 편집 모델로 옮기는 과정에서 과거 필드가 사라지지 않도록 주의한다.
4. 과거 변환은 그 버전의 구조에 고정한다. 현재 `decode_catalog(..., schema)`의 숫자 인자는 버전 검사값일 뿐 과거 parser 선택기가 아니다. 현재 codec을 변경한 뒤 숫자만 예전 버전으로 전달하여 과거 구조를 읽게 하지 않는다.
5. registry에 `{N, convert_N_to_N_plus_1}`을 등록하고 현재 버전을 `N+1`로 맞춘다. 지원하는 가장 오래된 버전부터 현재까지 누락·중복 없는 경로를 유지한다. 이미 출시된 변환을 현재 codec에 맞춰 다시 정의하거나 중간 버전을 건너뛰지 않는다.
6. 변환 함수는 stream과 값 변환만 담당한다. SD/Windows 파일 API, LVGL, revision 증가, A/B 확정을 직접 호출하지 않는다. 기존 migrator의 임시 파일 교대·버전/길이/CRC 검사와 repository의 최종 검증·한 번의 확정을 재사용한다.

전체 JSON DOM·카탈로그 복제·큰 스택 버퍼를 추가하기 전에 [embedded-memory-safety](../embedded-memory-safety/SKILL.md)를 적용한다. 범위와 할당 실패를 검사하고 기존 저장본을 보존한다. 실패를 빈 목록 생성이나 자동 초기화로 복구하지 않는다.

## 검증과 기록

실제 이전 구조 fixture로 다음을 검증한다. `simulator/windows/tests/recipe_migration_fixture.hpp`의 가상 버전 예시는 실행 경로 검사이며 실제 구조 변환 검증을 대신하지 않는다.

- 직전 버전 → 현재 및 여러 버전 건너 업그레이드의 전체 순차 경로. 기본값·단위 변환·ID·빈 슬롯·재료와 인분 설정의 기대 결과를 비교한다.
- 현재 버전은 재변환/재저장하지 않고, 미래 버전·누락 경로·잘못된 변환 결과는 원본을 보존하며 거부한다.
- 변환/쓰기/검증 실패와 중단 후 원본 또는 확정된 새 세대로만 복원된다. 원본 byte 보존, revision 한 번 증가, 재시작 후 중복 변환 없음, 메타정보 보존을 확인한다.
- 화면 계약에 영향이 있으면 요리 화면 진입 시 실행, 실제 작업 기준 진행률, 실패 재시도, 저장 확정 후 100%·완료 확인, 확인 전 편집 차단을 Windows에서 검사한다.

기존 `recipe_codec`, `recipe_repository`, `recipe_persistence`, 필요한 `recipe_persistence_ui`에 관련 사례를 추가한다. 정확한 빌드 범위는 [change-scoped-verification](../change-scoped-verification/SKILL.md)을 따른다. Windows 검증을 먼저 하고 HMI 통합 빌드가 필요하면 마지막에 실행한다. 문서/스킬만 바꾼 작업에는 제품 빌드를 추가하지 않는다.

[저장 형식](../../../_doc/recipe_persistence_format.md)과 [수용 절차](../../../_doc/recipe_persistence_acceptance.md)에 버전·필드 매핑·지원 시작 버전·검증 결과를 반영한다. 새 개발 계획이 필요하면 저장소 계획 규칙을 따른다. 결과 보고에서는 실제 구현된 버전 변환과 테스트 전용 가상 버전을 구분하고, 남은 실물 SD/전원 중단 확인을 명시한다.
