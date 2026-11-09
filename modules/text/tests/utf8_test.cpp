#include "hmi/text/utf8.hpp"

#include <cassert>
#include <string_view>

namespace {

using hmi::text::utf8::DecodeStatus;

void expect_ok(std::string_view text, std::uint32_t expected,
               std::size_t expected_next) {
  const auto result = hmi::text::utf8::decode_next(text, 0U);
  assert(result.status == DecodeStatus::ok);
  assert(result.codepoint == expected);
  assert(result.next_offset == expected_next);
}

void expect_invalid(std::string_view text) {
  const auto result = hmi::text::utf8::decode_next(text, 0U);
  assert(result.status == DecodeStatus::invalid);
  assert(result.next_offset == 1U);
}

}  // namespace

int main() {
  expect_ok("A", 0x41U, 1U);
  expect_ok("\xC2\xA2", 0x00A2U, 2U);
  expect_ok("\xED\x95\x9C", 0xD55CU, 3U);
  expect_ok("\xF0\x9F\x98\x80", 0x1F600U, 4U);

  const auto empty = hmi::text::utf8::decode_next("", 0U);
  assert(empty.status == DecodeStatus::end);
  assert(empty.next_offset == 0U);

  const auto past_end = hmi::text::utf8::decode_next("A", 8U);
  assert(past_end.status == DecodeStatus::end);
  assert(past_end.next_offset == 1U);

  expect_invalid("\xC2");
  expect_invalid("\xE1\x80");
  expect_invalid("\xF0\x9F\x98");
  expect_invalid("\xE1\x41\x80");
  expect_invalid("\x80");
  expect_invalid("\xC0\xAF");
  expect_invalid("\xED\xA0\x80");
  expect_invalid("\xF4\x90\x80\x80");

  const std::string_view sequence = "A\xED\x95\x9C";
  const auto first = hmi::text::utf8::decode_next(sequence, 0U);
  const auto second = hmi::text::utf8::decode_next(sequence, first.next_offset);
  const auto end = hmi::text::utf8::decode_next(sequence, second.next_offset);
  assert(first.codepoint == 0x41U);
  assert(second.codepoint == 0xD55CU);
  assert(end.status == DecodeStatus::end);

  return 0;
}
