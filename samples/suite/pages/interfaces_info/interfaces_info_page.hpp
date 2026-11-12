#pragma once

#include <array>
#include <cstddef>

#include <lvgl.h>

#include "page_contract.hpp"
#include "pages/interfaces_info/interfaces_info_model.hpp"

namespace hmi::samples::suite {

/**
 * 여덟 기반 기능을 한 page의 목록과 상세 view로 설명한다.
 *
 * page는 정적 descriptor와 LVGL widget만 사용한다. Board, platform adapter와 update engine을
 * 생성하지 않으며 locale/theme rebuild 동안 model의 선택 topic을 유지한다.
 */
class InterfacesInfoPage final : public SamplePage {
 public:
  InterfacesInfoPage() = default;
  InterfacesInfoPage(const InterfacesInfoPage&) = delete;
  InterfacesInfoPage& operator=(const InterfacesInfoPage&) = delete;
  ~InterfacesInfoPage() override;

  bool create(lv_obj_t* parent, const PageContext& context) override;
  void destroy() noexcept override;
  [[nodiscard]] PageSnapshot snapshot() const noexcept override { return {}; }
  [[nodiscard]] InterfacesInfoView info_view() const noexcept {
    return model_.view();
  }
  [[nodiscard]] std::size_t selected_index() const noexcept {
    return model_.selected_index();
  }

 private:
  static void handle_event(lv_event_t* event);

  [[nodiscard]] bool create_header(lv_obj_t* parent) noexcept;
  [[nodiscard]] bool render_body() noexcept;
  [[nodiscard]] bool render_list() noexcept;
  [[nodiscard]] bool render_detail() noexcept;
  [[nodiscard]] lv_obj_t* create_label(lv_obj_t* parent,
                                       generated::TextId id,
                                       hmi::ui::TextRole role) noexcept;
  [[nodiscard]] bool create_detail_section(lv_obj_t* parent,
                                           generated::TextId heading,
                                           generated::TextId body) noexcept;
  void reset_widgets() noexcept;

  PageContext context_{};
  InterfacesInfoModel model_{};
  lv_obj_t* root_ = nullptr;
  lv_obj_t* body_ = nullptr;
  lv_obj_t* back_button_ = nullptr;
  lv_obj_t* locale_ko_button_ = nullptr;
  lv_obj_t* locale_en_button_ = nullptr;
  lv_obj_t* theme_button_ = nullptr;
  std::array<lv_obj_t*, kInterfaceTopicCount> rows_{};
};

static_assert(sizeof(InterfacesInfoPage) <= 272U,
              "interfaces info page must keep only bounded model and handles");

}  // namespace hmi::samples::suite
