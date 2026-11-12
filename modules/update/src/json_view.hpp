#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace hmi::update::detail::json {

/** escape 없는 ASCII update wire를 복사하지 않고 순회하는 bounded view다. */
class View {
 public:
  View() = default;
  explicit View(std::string_view raw) : raw_(raw) {}

  [[nodiscard]] bool valid(unsigned depth = 0U) const noexcept;
  [[nodiscard]] bool object() const noexcept;
  [[nodiscard]] bool string(std::string_view& output) const noexcept;
  [[nodiscard]] bool integer(std::uint64_t& output) const noexcept;
  [[nodiscard]] bool boolean(bool& output) const noexcept;
  [[nodiscard]] View get(std::string_view key) const noexcept;
  [[nodiscard]] bool keys(const char* const* allowed,
                          std::size_t count) const noexcept;
  [[nodiscard]] bool each(bool (*callback)(void*, std::string_view, View),
                          void* context) const noexcept;
  [[nodiscard]] bool present() const noexcept { return !raw_.empty(); }

 private:
  std::string_view raw_{};
};

}  // namespace hmi::update::detail::json
