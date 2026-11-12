#include "hmi/update/types.hpp"

namespace hmi::update {

Target target_from_name(std::string_view value) noexcept {
  if (value == "firmware") {
    return Target::firmware;
  }
  if (value == "assets") {
    return Target::assets;
  }
  if (value == "configuration") {
    return Target::configuration;
  }
  return Target::invalid;
}

std::string_view target_name(Target value) noexcept {
  switch (value) {
    case Target::firmware:
      return "firmware";
    case Target::assets:
      return "assets";
    case Target::configuration:
      return "configuration";
    case Target::invalid:
      return {};
  }
  return {};
}

bool valid_uuid(std::string_view value) noexcept {
  if (value.size() != 36U) {
    return false;
  }
  for (std::size_t index = 0U; index < value.size(); ++index) {
    if (index == 8U || index == 13U || index == 18U || index == 23U) {
      if (value[index] != '-') {
        return false;
      }
    } else if (!((value[index] >= '0' && value[index] <= '9') ||
                 (value[index] >= 'a' && value[index] <= 'f'))) {
      return false;
    }
  }
  return true;
}

bool valid_sha256(std::string_view value) noexcept {
  if (value.size() != 64U) {
    return false;
  }
  for (const char character : value) {
    if (!((character >= '0' && character <= '9') ||
          (character >= 'a' && character <= 'f'))) {
      return false;
    }
  }
  return true;
}

bool valid_semver(std::string_view value) noexcept {
  if (value.empty() || value.size() > 40U) {
    return false;
  }
  unsigned parts = 0U;
  for (;;) {
    const auto dot = value.find('.');
    const auto part = value.substr(0U, dot);
    if (part.empty() || (part.size() > 1U && part.front() == '0')) {
      return false;
    }
    for (const char character : part) {
      if (character < '0' || character > '9') {
        return false;
      }
    }
    ++parts;
    if (dot == std::string_view::npos) {
      return parts == 3U;
    }
    value.remove_prefix(dot + 1U);
  }
}

bool safe_relative_path(std::string_view value) noexcept {
  if (value.empty() || value.size() > 191U || value.front() == '/' ||
      value.back() == '/') {
    return false;
  }
  for (const char character : value) {
    if (!((character >= 'a' && character <= 'z') ||
          (character >= 'A' && character <= 'Z') ||
          (character >= '0' && character <= '9') || character == '_' ||
          character == '-' || character == '.' || character == '/')) {
      return false;
    }
  }
  for (;;) {
    const auto slash = value.find('/');
    const auto part = value.substr(0U, slash);
    if (part.empty() || part == "." || part == ".." || part.back() == '.') {
      return false;
    }
    if (slash == std::string_view::npos) {
      return true;
    }
    value.remove_prefix(slash + 1U);
  }
}

}  // namespace hmi::update
