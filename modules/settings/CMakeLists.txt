add_library(hmi_settings_core STATIC
  src/level_settings.cpp)

target_compile_features(hmi_settings_core PUBLIC cxx_std_17)
target_include_directories(hmi_settings_core PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_SOURCE_DIR}/include")

if(MSVC)
  target_compile_options(hmi_settings_core PRIVATE /W4)
else()
  target_compile_options(hmi_settings_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
