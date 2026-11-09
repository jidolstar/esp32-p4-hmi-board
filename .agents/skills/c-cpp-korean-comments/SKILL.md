---
name: c-cpp-korean-comments
description: Add and maintain useful Korean comments in C/C++ code. Use when creating or changing C/C++ headers or implementations, especially for class responsibilities, ownership, lifetime, module boundaries, public APIs, LVGL/UI layers, hardware facades, non-obvious algorithms, or performance-sensitive code.
---

# C/C++ Korean Comments

## Overview

Use this skill to keep C/C++ comments helpful, accurate, and lightweight. Prefer Korean comments, while allowing established technical terms such as `class`, `ownership`, `lifetime`, `callback`, `buffer`, `LVGL`, and `PSRAM` when they are clearer than forced translation.

## Comment Policy

- Add comments when code introduces a class, module boundary, ownership rule, lifetime rule, public API contract, hardware/UI responsibility split, or non-obvious algorithm.
- Avoid comments that repeat the code mechanically. Do not write comments like "값을 대입한다" for a simple assignment.
- Keep comments close to the declaration or complex block they explain.
- Update nearby comments whenever behavior, ownership, or responsibilities change.
- Prefer Doxygen-style block comments for public classes and public methods in headers.
- Prefer short `//` comments inside `.cpp` files for complex blocks, performance tradeoffs, or surprising constraints.
- Keep wording concrete: name who owns what, who may call what, and what must not leak across boundaries.

## Header Comments

For each new or substantially changed C++ class, add a short Korean class comment that explains:

- The class responsibility.
- The class relationship to neighboring classes.
- Ownership and lifetime expectations.
- What lower-level details are intentionally hidden.

Example:

```cpp
/**
 * 제품 UI의 공통 레이어를 만드는 screen-level shell이다.
 *
 * AppShell은 AppLayers와 AppBackground를 소유한다. 화면별 UI는 content layer만 받아
 * 구성하고, raw lv_obj_t는 AppShell 내부 구현 밖으로 노출하지 않는다.
 */
class AppShell { ... };
```

## Implementation Comments

Use implementation comments sparingly. Add them when:

- An optimization changes the straightforward implementation.
- A buffer must stay alive because another API stores a pointer to it.
- A callback is called by a framework or interrupt-like runtime.
- A boundary exists for safety, such as product UI not reaching into hardware internals.

## Review Checklist

Before finishing C/C++ work:

- Check new classes have useful Korean responsibility comments.
- Check public methods with lifetime or ownership constraints are documented.
- Check comments still match code after refactors.
- Remove stale or misleading comments.
- Keep simple code uncluttered.
