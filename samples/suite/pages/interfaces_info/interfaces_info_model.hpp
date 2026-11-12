#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "generated/text_catalog.hpp"

namespace hmi::samples::suite {

enum class InterfacesInfoView : std::uint8_t { list, detail };
enum class InterfaceTopic : std::uint8_t {
  rs485,
  gpio,
  pwm,
  usb_command,
  storage_files,
  usb_msc,
  usb_assets,
  firmware_update,
};

struct InterfaceTopicDescriptor {
  InterfaceTopic topic = InterfaceTopic::rs485;
  generated::TextId title = generated::TextId::count;
  generated::TextId summary = generated::TextId::count;
  generated::TextId status = generated::TextId::count;
  generated::TextId purpose = generated::TextId::count;
  generated::TextId provided = generated::TextId::count;
  generated::TextId connect = generated::TextId::count;
  generated::TextId steps = generated::TextId::count;
  generated::TextId limits = generated::TextId::count;
  generated::TextId excluded = generated::TextId::count;
};

inline constexpr std::size_t kInterfaceTopicCount = 8U;

[[nodiscard]] const std::array<InterfaceTopicDescriptor, kInterfaceTopicCount>&
interface_topic_descriptors() noexcept;

/** 목록·상세 전환과 선택 topic만 보존하는 bounded page model이다. */
class InterfacesInfoModel {
 public:
  [[nodiscard]] bool open(std::size_t index) noexcept;
  [[nodiscard]] bool back_to_list() noexcept;
  [[nodiscard]] InterfacesInfoView view() const noexcept { return view_; }
  [[nodiscard]] std::size_t selected_index() const noexcept {
    return selected_index_;
  }
  [[nodiscard]] const InterfaceTopicDescriptor& selected() const noexcept;

 private:
  InterfacesInfoView view_ = InterfacesInfoView::list;
  std::uint8_t selected_index_ = 0U;
};

static_assert(sizeof(InterfacesInfoModel) <= 2U,
              "interfaces info model must keep only view and selected index");

}  // namespace hmi::samples::suite
