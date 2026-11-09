# Typography catalog and font generators

이 도구는 app 또는 sample이 소유한 locale catalog, typography role과 font 설정을
검증하고 typed C++ catalog, 고정 LVGL font, 동적 사용자 font를 생성한다. 도구는 특정
profile이나 page 경로를 알지 않으며 owner의 `catalog.json`만 입력으로 받는다.

## Owner config

owner config는 다음 정보를 선언한다.

- `owner_id`: manifest에 기록할 안정적인 owner ID
- `cpp_namespace`: 생성 header의 C++ namespace
- `symbol_prefix`: 고정 LVGL font C symbol 접두어
- `paths.owner_root`: 모든 catalog/output을 포함하는 owner 경계
- `paths.catalogs`: `ko-KR`, `en-US` YAML
- `paths.typography`, `paths.fonts`: typography와 font 설정
- `paths.generated_output`: generated header/C source/manifest 출력
- `paths.user_output`: runtime user font binary 출력
- `paths.user_asset_prefix`: profile asset root 기준 runtime 상대 경로

config 안의 filesystem 경로는 config 파일 기준 상대 경로이며 `owner_root` 밖으로 나갈 수
없다. 입력과 출력, fixed와 user output이 겹치면 validation이 실패한다. source font 경로는
`fonts.json`에서 저장소 루트 기준으로 선언하고 SHA-256을 함께 고정한다.

현재 sample owner config는 `samples/suite/i18n/catalog.json`이다. 향후 app도
`apps/<app>/i18n/catalog.json`을 만들고 같은 명령을 사용한다.

## Fixed catalog and fonts

저장소 루트에서 owner config를 명시해 실행한다.

```powershell
python tools/assets/typography/generate.py --config samples/suite/i18n/catalog.json --dry-run
python tools/assets/typography/generate.py --config samples/suite/i18n/catalog.json
python tools/assets/typography/generate.py --config samples/suite/i18n/catalog.json --check
```

생성기는 locale key와 `{name:type}` placeholder 집합, 빈 문구, typography role 동등성,
원본 font SHA-256, role/style 배정과 stale output을 검사한다. 현재 placeholder type은
`u32`와 `string`이다. locale 누락을 다른 언어 fallback으로 숨기지 않는다.

## Dynamic user fonts

키보드 문자와 사용자가 입력한 동적 문자열은 fixed catalog에 넣지 않는다. owner가 선언한
glyph range를 분할 binary font로 생성한다.

```powershell
python tools/assets/typography/generate_user.py --config samples/suite/i18n/catalog.json --dry-run
python tools/assets/typography/generate_user.py --config samples/suite/i18n/catalog.json
python tools/assets/typography/generate_user.py --config samples/suite/i18n/catalog.json --ensure
python tools/assets/typography/generate_user.py --config samples/suite/i18n/catalog.json --check
```

runtime은 manifest가 선언한 자산 중 실제 문자열에 필요한 block만 owner가 정한 cache
범위에서 로드한다. 지원하지 않는 codepoint는 설정된 replacement glyph로 표시한다.
`--ensure`는 binary와 manifest가 모두 현재 입력과 일치하면 파일을 다시 쓰지 않고,
누락되거나 stale한 출력이 있으면 전체 runtime font set을 다시 생성한다.

## Ownership rule

profile별 입력 선택은 `catalog.json`과 profile manifest가 담당한다. Python generator에
`samples/`, `apps/main/`, page ID, 제품 namespace 또는 SD destination을 하드코딩하지
않는다. profile 편의 명령이 필요하면 중립 generator를 호출하는 얇은 profile-prefixed
wrapper로 둔다.
