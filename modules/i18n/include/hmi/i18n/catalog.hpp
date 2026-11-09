#pragma once

#include <cstddef>
#include <string_view>

namespace hmi::i18n {

/**
 * 생성 catalog의 문자열 배열을 소유하지 않고 조회하는 작은 view다.
 *
 * backing 배열은 static storage duration이어야 하며 CatalogView보다 오래 살아야 한다.
 * 문자열 누락은 다른 locale fallback으로 숨기지 않고 빈 view로 드러낸다.
 */
class CatalogView {
 public:
  constexpr CatalogView() noexcept = default;

  constexpr CatalogView(const std::string_view* entries, std::size_t size) noexcept
      : entries_(entries), size_(size) {}

  [[nodiscard]] constexpr std::string_view get(std::size_t index) const noexcept {
    return entries_ != nullptr && index < size_ ? entries_[index] : std::string_view{};
  }

  [[nodiscard]] constexpr std::size_t size() const noexcept { return size_; }
  [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0U; }

 private:
  const std::string_view* entries_ = nullptr;
  std::size_t size_ = 0U;
};

static_assert(sizeof(CatalogView) <= sizeof(void*) * 2U,
              "CatalogView must remain a non-owning pointer-size pair");

}  // namespace hmi::i18n
