add_library(hmi_audio_core STATIC
  src/playback.cpp)

target_compile_features(hmi_audio_core PUBLIC cxx_std_17)
target_include_directories(hmi_audio_core PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include")

if(MSVC)
  target_compile_options(hmi_audio_core PRIVATE /W4)
else()
  target_compile_options(hmi_audio_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

