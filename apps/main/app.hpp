#pragma once

#include <lvgl.h>

#include "profiles/runtime/app_contract.hpp"

namespace hmi::apps::main {

/**
 * 외부 자산 없이 스타터 실행 상태를 표시하는 최소 앱이다.
 *
 * 이 객체가 생성한 screen과 자식 widget을 소유한다. host는 LVGL display를 먼저
 * 준비하고 이 객체보다 오래 유지해야 하며, 앱은 host 구현이나 장치 API를 참조하지 않는다.
 */
class MainApp {
 public:
  MainApp() = default;
  MainApp(const MainApp &) = delete;
  MainApp &operator=(const MainApp &) = delete;

  bool create(const hmi::profiles::AppContext &context);
  bool handle_utf8_input(const char *) noexcept { return false; }
  bool handle_key(hmi::profiles::AppKey) noexcept { return false; }
  void destroy() noexcept;
  ~MainApp();

 private:
  static void handle_input_event(lv_event_t *event);

  lv_obj_t *screen_ = nullptr;
  lv_obj_t *input_status_ = nullptr;
};

}  // namespace hmi::apps::main
