hmi_load_windows_ui_dependencies()

target_include_directories(lvgl PRIVATE
  "${PROJECT_SOURCE_DIR}/include")

add_library(hmi_windows_lvgl OBJECT
  lvgl_mem.cpp
  sdl_lvgl_port.cpp
  settings/memory_level_settings.cpp)

target_compile_features(hmi_windows_lvgl PUBLIC cxx_std_17)
target_compile_definitions(hmi_windows_lvgl PUBLIC SDL_MAIN_HANDLED)
target_include_directories(hmi_windows_lvgl PUBLIC
  "${PROJECT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(hmi_windows_lvgl PUBLIC
  lvgl::lvgl
  SDL2::SDL2-static)

if(MSVC)
  target_compile_options(hmi_windows_lvgl PRIVATE /W4)
else()
  target_compile_options(hmi_windows_lvgl PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
  add_library(hmi_windows_pointer_test_driver STATIC
    tests/pointer_test_driver.cpp)
  target_compile_features(hmi_windows_pointer_test_driver PUBLIC cxx_std_17)
  target_include_directories(hmi_windows_pointer_test_driver PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/tests")
  target_link_libraries(hmi_windows_pointer_test_driver PUBLIC hmi_windows_lvgl)

  add_executable(hmi_windows_lvgl_memory_test
    tests/lvgl_memory_test.cpp)
  target_compile_features(hmi_windows_lvgl_memory_test PRIVATE cxx_std_17)
  target_link_libraries(hmi_windows_lvgl_memory_test PRIVATE hmi_windows_lvgl)
  add_test(NAME hmi_windows_lvgl_memory COMMAND hmi_windows_lvgl_memory_test)

  add_executable(hmi_windows_sdl_event_test
    tests/sdl_event_test.cpp)
  target_compile_features(hmi_windows_sdl_event_test PRIVATE cxx_std_17)
  target_link_libraries(hmi_windows_sdl_event_test PRIVATE hmi_windows_lvgl)
  add_test(NAME hmi_windows_sdl_escape COMMAND hmi_windows_sdl_event_test escape)
  add_test(NAME hmi_windows_sdl_quit COMMAND hmi_windows_sdl_event_test quit)
  add_test(NAME hmi_windows_sdl_keyboard COMMAND hmi_windows_sdl_event_test keyboard)
  set_tests_properties(hmi_windows_sdl_escape hmi_windows_sdl_quit
    hmi_windows_sdl_keyboard PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=dummy")
endif()
