#include <cassert>

#include "pages/interfaces_info/interfaces_info_model.hpp"

int main() {
  using namespace hmi::samples::suite;

  const auto& descriptors = interface_topic_descriptors();
  static_assert(kInterfaceTopicCount == 8U);
  assert(descriptors.size() == kInterfaceTopicCount);
  assert(descriptors.front().topic == InterfaceTopic::rs485);
  assert(descriptors.back().topic == InterfaceTopic::firmware_update);

  InterfacesInfoModel model;
  assert(model.view() == InterfacesInfoView::list);
  assert(!model.open(kInterfaceTopicCount));
  for (std::size_t index = 0U; index < kInterfaceTopicCount; ++index) {
    assert(model.open(index));
    assert(model.view() == InterfacesInfoView::detail);
    assert(model.selected_index() == index);
    assert(model.selected().topic == descriptors[index].topic);
    assert(model.back_to_list());
    assert(model.view() == InterfacesInfoView::list);
    assert(!model.back_to_list());
  }

  return 0;
}
