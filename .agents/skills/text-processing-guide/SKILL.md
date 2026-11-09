---
name: text-processing-guide
description: Guide on how to use utf8_utils and text_utils for parsing and rendering multi-font LVGL strings. Use this when developing new text components like TextBox, TextArea, etc.
---

# Text Processing Guide

이 가이드는 `UserTextLabel` 외에 새로운 텍스트 관련 UI 컴포넌트를 개발할 때, 공통으로 제공되는 텍스트 처리 유틸리티를 활용하는 방법을 안내합니다.

## 1. UTF-8 디코딩 (`hmi::utils::utf8`)

LVGL은 내부적으로 UTF-8을 사용하지만, 순회용 API를 외부에 노출하지 않으므로 직접 파싱해야 합니다. 문자열을 순회하며 글자 개수를 세거나 특정 문자를 찾을 때 사용합니다.

**파일:** `src/utils/utf8_utils.hpp`

**사용법:**
```cpp
#include "utils/utf8_utils.hpp"

std::size_t offset = 0;
std::size_t next_offset = 0;
std::uint32_t codepoint = 0;

while (hmi::utils::utf8::decode_next_codepoint(text, offset, next_offset, codepoint)) {
    // codepoint를 활용한 로직 수행
    offset = next_offset;
}
```

## 2. 폰트 세그먼트 생성 (`hmi::ui::text`)

LVGL의 `lv_label`은 중간에 폰트를 바꿀 수 없습니다. 다중 폰트(Common/Block)가 섞인 문자열을 표시하려면 문자열을 폰트 단위로 분할하여 여러 개의 `lv_label`로 구성해야 합니다.

**파일:** `src/ui/text_utils.hpp`

**사용법:**
```cpp
#include "ui/text_utils.hpp"

std::array<hmi::ui::text::FontSegment, hmi::ui::text::kMaxSegments> segments {};
std::size_t segment_count = 0;

if (hmi::ui::text::build_font_segments(text, segments, segment_count)) {
    for (std::size_t i = 0; i < segment_count; ++i) {
        lv_obj_t* label = lv_label_create(parent);
        lv_label_set_text(label, segments[i].text.c_str());
        lv_obj_set_style_text_font(label, segments[i].font, 0);
    }
}
```

## 3. 원칙 및 주의사항

- **레이어 분리 유지**: `utf8_utils`는 UI(LVGL 등)에 독립적인 순수 로직이어야 합니다. `text_utils`는 UI 테마 폰트에 의존하므로 분리되어 있습니다.
- **최적화 내장**: `build_font_segments`는 이미 인접한 동일 폰트 문자를 단일 세그먼트로 병합하는 렌더링 최적화를 포함하고 있습니다. 컴포넌트 측에서 이를 중복 구현하지 마십시오.
