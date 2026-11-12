---
name: text-processing-guide
description: Use the project's UTF-8, text editing, and multi-font LVGL components when implementing or reviewing dynamic user text, cursor behavior, and text input.
---

# UTF-8과 사용자 텍스트

고정 문구와 형식은 [ui-text-policy](../ui-text-policy/SKILL.md)에 따라 카탈로그에서 가져온다. 사용자가 입력한 문자열과 장치명 같은 동적 문자열에는 아래 공통 모듈을 사용한다.

## UTF-8 순회와 편집

`modules/text/`는 LVGL에 의존하지 않는다. [UTF-8 API](../../../modules/text/include/hmi/text/utf8.hpp)의 `validate()`로 전체 문자열의 유효성과 길이를 확인하고, 코드 포인트 순회에는 `decode_next()`를 사용한다.

```cpp
#include "hmi/text/utf8.hpp"

namespace utf8 = hmi::text::utf8;
for (std::size_t offset = 0; offset < text.size();) {
    const auto decoded = utf8::decode_next(text, offset);
    if (decoded.status != utf8::DecodeStatus::ok) {
        // 호출자의 입력 정책에 따라 잘못된 UTF-8을 처리한다.
        break;
    }
    // decoded.codepoint를 처리한다.
    offset = decoded.next_offset;
}
```

`next_offset`은 바이트 위치다. 편집과 커서 이동에는 [Editor](../../../modules/text/include/hmi/text/editor.hpp), 키 입력 및 한글 조합과 연동할 때는 [InputModel](../../../modules/text/include/hmi/text/input_model.hpp)을 먼저 확인한다.

## 표시와 입력 위젯

- 동적 문자열 표시는 [UserTextLabel](../../../modules/ui/include/hmi/ui/user_text_label.hpp)을 재사용한다. 위젯은 인접한 동일 폰트 구간을 묶어 표시하고, `set_text()` 결과로 잘못된 UTF-8, 폰트 문제와 용량 제한을 알려 준다.
- 편집 문자열과 터치 커서를 표시할 때는 [UserTextInputLine](../../../modules/ui/include/hmi/ui/user_text_input_line.hpp)을 사용한다. 위젯의 터치 인덱스는 코드 포인트 경계이며 편집 상태는 호출자가 소유한다.
- 동적 글꼴 선택과 캐시는 [FontRepository](../../../modules/ui/include/hmi/ui/font_repository.hpp)가 관리한다. 저장소 객체는 사용하는 위젯보다 오래 유지한다. 자산 범위와 누락 처리 변경은 [font-management-guide](../font-management-guide/SKILL.md)를 따른다.

UTF-8, editor, 조합과 렌더링 변경은 관련 Windows 테스트와 `sample-suite`의 Text Input 화면에서 확인한다. 다중 바이트 문자와 혼합 폰트, 커서, 폰트 자산 누락 중 변경된 경로를 재현한다.
