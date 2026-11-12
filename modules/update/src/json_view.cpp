#include "json_view.hpp"

#include <array>
#include <limits>

namespace hmi::update::detail::json {
namespace {

std::string_view trim(std::string_view value) noexcept {
  while (!value.empty() &&
         (value.front() == ' ' || value.front() == '\n' ||
          value.front() == '\r' || value.front() == '\t')) {
    value.remove_prefix(1U);
  }
  return value;
}

bool literal(std::string_view& value, std::string_view expected) noexcept {
  if (value.substr(0U, expected.size()) != expected) {
    return false;
  }
  value.remove_prefix(expected.size());
  return true;
}

bool quoted(std::string_view& value, std::string_view& output) noexcept {
  if (value.empty() || value.front() != '"') {
    return false;
  }
  value.remove_prefix(1U);
  const auto end = value.find('"');
  if (end == std::string_view::npos) {
    return false;
  }
  output = value.substr(0U, end);
  // update wire는 우회 표현을 막기 위해 escape 없는 printable ASCII만 허용한다.
  for (const unsigned char character : output) {
    if (character < 32U || character > 126U || character == '\\') {
      return false;
    }
  }
  value.remove_prefix(end + 1U);
  return true;
}

bool token(std::string_view& value, unsigned depth) noexcept {
  value = trim(value);
  if (value.empty() || depth > 8U) {
    return false;
  }
  if (value.front() == '"') {
    std::string_view ignored;
    return quoted(value, ignored);
  }
  const char first = value.front();
  if (first == '{' || first == '[') {
    value.remove_prefix(1U);
    value = trim(value);
    const char end = first == '{' ? '}' : ']';
    if (!value.empty() && value.front() == end) {
      value.remove_prefix(1U);
      return true;
    }
    std::array<std::string_view, 24U> keys{};
    std::size_t count = 0U;
    for (;;) {
      if (first == '{') {
        std::string_view key;
        if (count == keys.size() || !quoted(value, key)) {
          return false;
        }
        for (std::size_t index = 0U; index < count; ++index) {
          if (keys[index] == key) {
            return false;
          }
        }
        keys[count++] = key;
        value = trim(value);
        if (value.empty() || value.front() != ':') {
          return false;
        }
        value.remove_prefix(1U);
      }
      if (!token(value, depth + 1U)) {
        return false;
      }
      value = trim(value);
      if (value.empty()) {
        return false;
      }
      if (value.front() == end) {
        value.remove_prefix(1U);
        return true;
      }
      if (value.front() != ',') {
        return false;
      }
      value.remove_prefix(1U);
      value = trim(value);
    }
  }
  if (first == 't') {
    return literal(value, "true");
  }
  if (first == 'f') {
    return literal(value, "false");
  }
  if (first == 'n') {
    return literal(value, "null");
  }
  const auto before = value;
  std::uint64_t number = 0U;
  while (!value.empty() && value.front() >= '0' && value.front() <= '9') {
    const auto digit = static_cast<unsigned>(value.front() - '0');
    if (number > (std::numeric_limits<std::uint64_t>::max() - digit) / 10U) {
      return false;
    }
    number = number * 10U + digit;
    value.remove_prefix(1U);
  }
  const auto used = before.size() - value.size();
  return used > 0U && (used == 1U || before.front() != '0');
}

bool walk(std::string_view value,
          bool (*member)(void*, std::string_view, View),
          void* context) noexcept {
  value = trim(value);
  if (value.empty() || value.front() != '{') {
    return false;
  }
  value.remove_prefix(1U);
  value = trim(value);
  if (!value.empty() && value.front() == '}') {
    return trim(value.substr(1U)).empty();
  }
  for (;;) {
    std::string_view key;
    if (!quoted(value, key)) {
      return false;
    }
    value = trim(value);
    if (value.empty() || value.front() != ':') {
      return false;
    }
    value.remove_prefix(1U);
    value = trim(value);
    const auto begin = value;
    if (!token(value, 1U)) {
      return false;
    }
    const View item(begin.substr(0U, begin.size() - value.size()));
    if (member != nullptr && !member(context, key, item)) {
      return false;
    }
    value = trim(value);
    if (value.empty()) {
      return false;
    }
    if (value.front() == '}') {
      return trim(value.substr(1U)).empty();
    }
    if (value.front() != ',') {
      return false;
    }
    value.remove_prefix(1U);
    value = trim(value);
  }
}

}  // namespace

bool View::valid(unsigned depth) const noexcept {
  auto value = raw_;
  return token(value, depth) && trim(value).empty();
}

bool View::object() const noexcept {
  const auto value = trim(raw_);
  return !value.empty() && value.front() == '{';
}

bool View::string(std::string_view& output) const noexcept {
  auto value = trim(raw_);
  return quoted(value, output) && trim(value).empty();
}

bool View::integer(std::uint64_t& output) const noexcept {
  auto value = trim(raw_);
  if (value.empty() || value.front() < '0' || value.front() > '9') {
    return false;
  }
  std::uint64_t parsed = 0U;
  std::size_t used = 0U;
  for (const char character : value) {
    if (character < '0' || character > '9') {
      break;
    }
    const auto digit = static_cast<unsigned>(character - '0');
    if (parsed > (UINT64_MAX - digit) / 10U) {
      return false;
    }
    parsed = parsed * 10U + digit;
    ++used;
  }
  if (used == 0U || (used > 1U && value.front() == '0') ||
      !trim(value.substr(used)).empty()) {
    return false;
  }
  output = parsed;
  return true;
}

bool View::boolean(bool& output) const noexcept {
  auto value = trim(raw_);
  if (literal(value, "true") && trim(value).empty()) {
    output = true;
    return true;
  }
  value = trim(raw_);
  if (literal(value, "false") && trim(value).empty()) {
    output = false;
    return true;
  }
  return false;
}

bool View::each(bool (*callback)(void*, std::string_view, View),
                void* context) const noexcept {
  return walk(raw_, callback, context);
}

View View::get(std::string_view key) const noexcept {
  struct Find {
    std::string_view key;
    View found;
  } find{key, {}};
  if (!each(
          [](void* context, std::string_view candidate, View value) {
            auto& state = *static_cast<Find*>(context);
            if (candidate == state.key) {
              state.found = value;
            }
            return true;
          },
          &find)) {
    return {};
  }
  return find.found;
}

bool View::keys(const char* const* allowed, std::size_t count) const noexcept {
  struct Check {
    const char* const* names;
    std::size_t count;
  } check{allowed, count};
  return each(
      [](void* context, std::string_view key, View) {
        const auto& state = *static_cast<Check*>(context);
        for (std::size_t index = 0U; index < state.count; ++index) {
          if (key == state.names[index]) {
            return true;
          }
        }
        return false;
      },
      &check);
}

}  // namespace hmi::update::detail::json
