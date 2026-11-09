#include <cassert>

#include <lvgl.h>

#include "hmi/ui/asset_update_overlay.hpp"
#include "sdl_lvgl_port.hpp"

int main(int argc, char** argv) {
  using namespace hmi::ui;

  lv_init();
  hmi::platform::windows::SdlLvglPort port;
  assert(port.begin("asset update overlay", false));

  AssetUpdateOverlay overlay;
  assert(overlay.show());
  overlay.update({AssetUpdateViewState::preparing,
                  AssetUpdatePreparationStep::fonts});
  assert(overlay.visible());
  assert(overlay.state() == AssetUpdateViewState::preparing);
  assert(overlay.progress_percent() == 0U);
  port.tick();
  if (argc == 2) {
    assert(port.save_screenshot(argv[1]));
  }

  overlay.update({AssetUpdateViewState::preparing,
                  AssetUpdatePreparationStep::assets});
  assert(overlay.state() == AssetUpdateViewState::preparing);
  port.tick();

  overlay.update({AssetUpdateViewState::receiving,
                  AssetUpdatePreparationStep::none, 4U, 177U, 250U, 1000U});
  assert(overlay.visible());
  assert(overlay.state() == AssetUpdateViewState::receiving);
  assert(overlay.progress_percent() == 25U);
  port.tick();

  overlay.update({AssetUpdateViewState::completed,
                  AssetUpdatePreparationStep::none, 177U, 177U, 1000U,
                  1000U});
  assert(overlay.state() == AssetUpdateViewState::completed);
  assert(overlay.progress_percent() == 100U);
  port.tick();

  overlay.update({AssetUpdateViewState::failed,
                  AssetUpdatePreparationStep::none, 9U, 177U, 500U, 1000U});
  assert(overlay.state() == AssetUpdateViewState::failed);
  assert(overlay.progress_percent() == 50U);

  overlay.destroy();
  assert(!overlay.visible());
  return 0;
}
