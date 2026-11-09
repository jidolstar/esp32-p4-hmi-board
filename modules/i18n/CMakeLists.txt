add_library(hmi_i18n_core STATIC
  src/locale.cpp
  src/format.cpp)

target_compile_features(hmi_i18n_core PUBLIC cxx_std_17)
target_include_directories(hmi_i18n_core PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include")

if(MSVC)
  target_compile_options(hmi_i18n_core PRIVATE /W4)
else()
  target_compile_options(hmi_i18n_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
