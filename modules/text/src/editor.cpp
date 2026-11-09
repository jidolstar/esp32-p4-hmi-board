#include "hmi/text/editor.hpp"

#include <cstring>

#include "hmi/text/utf8.hpp"

namespace hmi::text {

std::size_t Editor::cursor_codepoint_index() const noexcept {
  std::size_t offset = 0U;
  std::size_t index = 0U;
  while (offset < cursor_byte_offset_) {
    const auto decoded = utf8::decode_next(text(), offset);
    if (decoded.status != utf8::DecodeStatus::ok) {
      break;
    }
    offset = decoded.next_offset;
    ++index;
  }
  return index;
}

EditStatus Editor::set_text(std::string_view value) noexcept {
  const auto validation = utf8::validate(value);
  if (validation.status != utf8::DecodeStatus::ok) {
    return EditStatus::invalid_utf8;
  }
  if (validation.bytes > kMaxBytes) {
    return EditStatus::byte_limit;
  }
  if (validation.codepoints > kMaxCodepoints) {
    return EditStatus::codepoint_limit;
  }

  if (!value.empty()) {
    std::memcpy(buffer_.data(), value.data(), value.size());
  }
  byte_size_ = value.size();
  codepoint_count_ = validation.codepoints;
  cursor_byte_offset_ = byte_size_;
  buffer_[byte_size_] = '\0';
  return EditStatus::ok;
}

EditStatus Editor::insert(std::string_view value) noexcept {
  const auto validation = utf8::validate(value);
  if (validation.status != utf8::DecodeStatus::ok) {
    return EditStatus::invalid_utf8;
  }
  if (byte_size_ + validation.bytes > kMaxBytes) {
    return EditStatus::byte_limit;
  }
  if (codepoint_count_ + validation.codepoints > kMaxCodepoints) {
    return EditStatus::codepoint_limit;
  }
  if (value.empty()) {
    return EditStatus::ok;
  }

  std::memmove(buffer_.data() + cursor_byte_offset_ + value.size(),
               buffer_.data() + cursor_byte_offset_,
               byte_size_ - cursor_byte_offset_ + 1U);
  std::memcpy(buffer_.data() + cursor_byte_offset_, value.data(), value.size());
  cursor_byte_offset_ += value.size();
  byte_size_ += value.size();
  codepoint_count_ += validation.codepoints;
  return EditStatus::ok;
}

EditStatus Editor::insert_codepoint(std::uint32_t codepoint) noexcept {
  const auto encoded = utf8::encode(codepoint);
  if (encoded.size == 0U) {
    return EditStatus::invalid_codepoint;
  }
  return insert({encoded.bytes.data(), encoded.size});
}

std::size_t Editor::previous_offset(std::size_t offset) const noexcept {
  std::size_t current = 0U;
  std::size_t previous = 0U;
  while (current < offset) {
    const auto decoded = utf8::decode_next(text(), current);
    if (decoded.status != utf8::DecodeStatus::ok || decoded.next_offset > offset) {
      return previous;
    }
    previous = current;
    current = decoded.next_offset;
  }
  return previous;
}

void Editor::erase_range(std::size_t begin, std::size_t end,
                         std::size_t removed_codepoints) noexcept {
  std::memmove(buffer_.data() + begin, buffer_.data() + end,
               byte_size_ - end + 1U);
  const auto removed_bytes = end - begin;
  byte_size_ -= removed_bytes;
  codepoint_count_ -= removed_codepoints;
  cursor_byte_offset_ = begin;
}

EditStatus Editor::backspace() noexcept {
  if (cursor_byte_offset_ == 0U) {
    return EditStatus::ok;
  }
  const auto begin = previous_offset(cursor_byte_offset_);
  erase_range(begin, cursor_byte_offset_, 1U);
  return EditStatus::ok;
}

EditStatus Editor::delete_forward() noexcept {
  if (cursor_byte_offset_ >= byte_size_) {
    return EditStatus::ok;
  }
  const auto decoded = utf8::decode_next(text(), cursor_byte_offset_);
  if (decoded.status != utf8::DecodeStatus::ok) {
    return EditStatus::invalid_utf8;
  }
  erase_range(cursor_byte_offset_, decoded.next_offset, 1U);
  return EditStatus::ok;
}

EditStatus Editor::move_cursor(int codepoint_delta) noexcept {
  while (codepoint_delta < 0 && cursor_byte_offset_ > 0U) {
    cursor_byte_offset_ = previous_offset(cursor_byte_offset_);
    ++codepoint_delta;
  }
  while (codepoint_delta > 0 && cursor_byte_offset_ < byte_size_) {
    const auto decoded = utf8::decode_next(text(), cursor_byte_offset_);
    if (decoded.status != utf8::DecodeStatus::ok) {
      return EditStatus::invalid_utf8;
    }
    cursor_byte_offset_ = decoded.next_offset;
    --codepoint_delta;
  }
  return EditStatus::ok;
}

EditStatus Editor::move_cursor_to(std::size_t codepoint_index) noexcept {
  if (codepoint_index > codepoint_count_) {
    return EditStatus::cursor_out_of_range;
  }

  std::size_t next_offset = 0U;
  for (std::size_t index = 0U; index < codepoint_index; ++index) {
    const auto decoded = utf8::decode_next(text(), next_offset);
    if (decoded.status != utf8::DecodeStatus::ok) {
      return EditStatus::invalid_utf8;
    }
    next_offset = decoded.next_offset;
  }
  cursor_byte_offset_ = next_offset;
  return EditStatus::ok;
}

void Editor::clear() noexcept {
  byte_size_ = 0U;
  codepoint_count_ = 0U;
  cursor_byte_offset_ = 0U;
  buffer_[0] = '\0';
}

}  // namespace hmi::text
