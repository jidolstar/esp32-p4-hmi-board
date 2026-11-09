add_library(hmi_ui STATIC
  src/asset_update_overlay.cpp
  src/theme.cpp
  src/components.cpp
  src/font_repository.cpp
  src/user_text_label.cpp
  src/user_text_input_line.cpp
  src/virtual_keyboard.cpp)

target_compile_features(hmi_ui PUBLIC cxx_std_17)
target_include_directories(hmi_ui PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(hmi_ui PUBLIC lvgl::lvgl hmi_text_core)

if(MSVC)
  target_compile_options(hmi_ui PRIVATE /W4)
else()
  target_compile_options(hmi_ui PRIVATE -Wall -Wextra -Wpedantic)
endif()
