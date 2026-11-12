#include "pages/interfaces_info/interfaces_info_model.hpp"

namespace hmi::samples::suite {
namespace {

using generated::TextId;

constexpr std::array<InterfaceTopicDescriptor, kInterfaceTopicCount> kTopics{{
    {InterfaceTopic::rs485, TextId::interfaces_topic_rs485_title,
     TextId::interfaces_topic_rs485_summary,
     TextId::interfaces_status_foundation,
     TextId::interfaces_topic_rs485_purpose,
     TextId::interfaces_topic_rs485_provided,
     TextId::interfaces_topic_rs485_connect,
     TextId::interfaces_topic_rs485_steps,
     TextId::interfaces_topic_rs485_limits,
     TextId::interfaces_topic_rs485_excluded},
    {InterfaceTopic::gpio, TextId::interfaces_topic_gpio_title,
     TextId::interfaces_topic_gpio_summary,
     TextId::interfaces_status_foundation,
     TextId::interfaces_topic_gpio_purpose,
     TextId::interfaces_topic_gpio_provided,
     TextId::interfaces_topic_gpio_connect,
     TextId::interfaces_topic_gpio_steps,
     TextId::interfaces_topic_gpio_limits,
     TextId::interfaces_topic_gpio_excluded},
    {InterfaceTopic::pwm, TextId::interfaces_topic_pwm_title,
     TextId::interfaces_topic_pwm_summary,
     TextId::interfaces_status_foundation,
     TextId::interfaces_topic_pwm_purpose,
     TextId::interfaces_topic_pwm_provided,
     TextId::interfaces_topic_pwm_connect,
     TextId::interfaces_topic_pwm_steps,
     TextId::interfaces_topic_pwm_limits,
     TextId::interfaces_topic_pwm_excluded},
    {InterfaceTopic::usb_command,
     TextId::interfaces_topic_usb_command_title,
     TextId::interfaces_topic_usb_command_summary,
     TextId::interfaces_status_foundation,
     TextId::interfaces_topic_usb_command_purpose,
     TextId::interfaces_topic_usb_command_provided,
     TextId::interfaces_topic_usb_command_connect,
     TextId::interfaces_topic_usb_command_steps,
     TextId::interfaces_topic_usb_command_limits,
     TextId::interfaces_topic_usb_command_excluded},
    {InterfaceTopic::storage_files,
     TextId::interfaces_topic_storage_files_title,
     TextId::interfaces_topic_storage_files_summary,
     TextId::interfaces_status_implemented,
     TextId::interfaces_topic_storage_files_purpose,
     TextId::interfaces_topic_storage_files_provided,
     TextId::interfaces_topic_storage_files_connect,
     TextId::interfaces_topic_storage_files_steps,
     TextId::interfaces_topic_storage_files_limits,
     TextId::interfaces_topic_storage_files_excluded},
    {InterfaceTopic::usb_msc, TextId::interfaces_topic_usb_msc_title,
     TextId::interfaces_topic_usb_msc_summary,
     TextId::interfaces_status_implemented,
     TextId::interfaces_topic_usb_msc_purpose,
     TextId::interfaces_topic_usb_msc_provided,
     TextId::interfaces_topic_usb_msc_connect,
     TextId::interfaces_topic_usb_msc_steps,
     TextId::interfaces_topic_usb_msc_limits,
     TextId::interfaces_topic_usb_msc_excluded},
    {InterfaceTopic::usb_assets, TextId::interfaces_topic_usb_assets_title,
     TextId::interfaces_topic_usb_assets_summary,
     TextId::interfaces_status_implemented,
     TextId::interfaces_topic_usb_assets_purpose,
     TextId::interfaces_topic_usb_assets_provided,
     TextId::interfaces_topic_usb_assets_connect,
     TextId::interfaces_topic_usb_assets_steps,
     TextId::interfaces_topic_usb_assets_limits,
     TextId::interfaces_topic_usb_assets_excluded},
    {InterfaceTopic::firmware_update,
     TextId::interfaces_topic_firmware_update_title,
     TextId::interfaces_topic_firmware_update_summary,
     TextId::interfaces_status_foundation,
     TextId::interfaces_topic_firmware_update_purpose,
     TextId::interfaces_topic_firmware_update_provided,
     TextId::interfaces_topic_firmware_update_connect,
     TextId::interfaces_topic_firmware_update_steps,
     TextId::interfaces_topic_firmware_update_limits,
     TextId::interfaces_topic_firmware_update_excluded},
}};

}  // namespace

const std::array<InterfaceTopicDescriptor, kInterfaceTopicCount>&
interface_topic_descriptors() noexcept {
  return kTopics;
}

bool InterfacesInfoModel::open(std::size_t index) noexcept {
  if (index >= kTopics.size()) {
    return false;
  }
  selected_index_ = static_cast<std::uint8_t>(index);
  view_ = InterfacesInfoView::detail;
  return true;
}

bool InterfacesInfoModel::back_to_list() noexcept {
  if (view_ != InterfacesInfoView::detail) {
    return false;
  }
  view_ = InterfacesInfoView::list;
  return true;
}

const InterfaceTopicDescriptor& InterfacesInfoModel::selected() const noexcept {
  return kTopics[selected_index_];
}

}  // namespace hmi::samples::suite
