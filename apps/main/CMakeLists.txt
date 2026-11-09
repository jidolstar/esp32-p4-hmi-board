if(BUILD_TESTING)
  add_executable(hmi_selected_app_input_test
    tests/app_input_test.cpp)
  target_compile_features(hmi_selected_app_input_test PRIVATE cxx_std_17)
  target_link_libraries(hmi_selected_app_input_test PRIVATE
    hmi_selected_app
    hmi_windows_lvgl)

  if(MSVC)
    target_compile_options(hmi_selected_app_input_test PRIVATE /W4)
  else()
    target_compile_options(hmi_selected_app_input_test PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  add_test(NAME hmi_main_app_input
    COMMAND hmi_selected_app_input_test)
  set_tests_properties(hmi_main_app_input PROPERTIES
    ENVIRONMENT "${HMI_TEST_ENVIRONMENT}")
endif()
