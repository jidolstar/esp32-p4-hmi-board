add_executable(hmi_profile_windows
  main.cpp)

target_compile_features(hmi_profile_windows PRIVATE cxx_std_17)
target_link_libraries(hmi_profile_windows PRIVATE
  hmi_selected_app
  hmi_windows_lvgl)

string(REPLACE "-" "_" HMI_PROFILE_TARGET_SUFFIX "${HMI_PROFILE_ID}")
set_target_properties(hmi_profile_windows PROPERTIES
  OUTPUT_NAME "hmi_${HMI_PROFILE_TARGET_SUFFIX}_windows")

if(MSVC)
  target_compile_options(hmi_profile_windows PRIVATE /W4)
else()
  target_compile_options(hmi_profile_windows PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
  set(HMI_PROFILE_SMOKE_TEST "hmi_${HMI_PROFILE_TARGET_SUFFIX}_windows_smoke")
  add_test(NAME "${HMI_PROFILE_SMOKE_TEST}"
    COMMAND hmi_profile_windows --smoke)
  set_tests_properties("${HMI_PROFILE_SMOKE_TEST}" PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=dummy")
endif()
