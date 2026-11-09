add_library(hmi_text_core STATIC
  src/utf8.cpp
  src/editor.cpp
  src/hangul_composer.cpp
  src/input_model.cpp
  src/keyboard_layout.cpp)

target_compile_features(hmi_text_core PUBLIC cxx_std_17)
target_include_directories(hmi_text_core PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include")

if(MSVC)
  target_compile_options(hmi_text_core PRIVATE /W4)
else()
  target_compile_options(hmi_text_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
  function(hmi_add_text_test target source test_name)
    add_executable(${target} ${source})
    target_compile_features(${target} PRIVATE cxx_std_17)
    target_link_libraries(${target} PRIVATE hmi_text_core)
    add_test(NAME ${test_name} COMMAND ${target})
  endfunction()

  hmi_add_text_test(hmi_text_utf8_test tests/utf8_test.cpp hmi_text_utf8)
  hmi_add_text_test(hmi_text_editor_test tests/editor_test.cpp hmi_text_editor)
  hmi_add_text_test(hmi_text_hangul_test tests/hangul_composer_test.cpp hmi_text_hangul)
  hmi_add_text_test(hmi_text_input_model_test tests/input_model_test.cpp hmi_text_input_model)
  hmi_add_text_test(hmi_text_keyboard_layout_test tests/keyboard_layout_test.cpp hmi_text_keyboard_layout)
endif()
