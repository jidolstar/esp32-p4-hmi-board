if(BUILD_TESTING)
  function(hmi_add_sample_suite_test target source test_name)
    add_executable(${target} ${source})
    target_compile_features(${target} PRIVATE cxx_std_17)
    target_link_libraries(${target} PRIVATE hmi_selected_app hmi_windows_lvgl)
    if(MSVC)
      target_compile_options(${target} PRIVATE /W4)
    else()
      target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME ${test_name} COMMAND ${target})
    set_tests_properties(${test_name} PROPERTIES
      ENVIRONMENT "${HMI_TEST_ENVIRONMENT}")
  endfunction()

  hmi_add_sample_suite_test(
    hmi_sample_suite_navigation_test
    tests/navigation_test.cpp
    hmi_sample_suite_navigation)
  hmi_add_sample_suite_test(
    hmi_board_storage_access_guard_test
    "${CMAKE_SOURCE_DIR}/boards/jc1060p470c/tests/storage_access_guard_test.cpp"
    hmi_board_storage_access_guard)
  hmi_add_sample_suite_test(
    hmi_board_wav_reader_test
    "${CMAKE_SOURCE_DIR}/boards/jc1060p470c/tests/wav_reader_test.cpp"
    hmi_board_wav_reader)
  target_sources(hmi_board_wav_reader_test PRIVATE
    "${CMAKE_SOURCE_DIR}/boards/jc1060p470c/internal/wav_reader.cpp")
  target_include_directories(hmi_board_wav_reader_test PRIVATE
    "${CMAKE_SOURCE_DIR}/boards/jc1060p470c")
  hmi_add_sample_suite_test(
    hmi_board_msc_sector_transfer_test
    "${CMAKE_SOURCE_DIR}/boards/jc1060p470c/tests/msc_sector_transfer_test.cpp"
    hmi_board_msc_sector_transfer)
  hmi_add_sample_suite_test(
    hmi_board_usb_msc_event_state_test
    "${CMAKE_SOURCE_DIR}/boards/jc1060p470c/tests/usb_msc_event_state_test.cpp"
    hmi_board_usb_msc_event_state)
  hmi_add_sample_suite_test(
    hmi_board_storage_msc_transition_test
    "${CMAKE_SOURCE_DIR}/boards/jc1060p470c/tests/storage_msc_transition_test.cpp"
    hmi_board_storage_msc_transition)
  hmi_add_sample_suite_test(
    hmi_storage_access_controller_test
    "${CMAKE_SOURCE_DIR}/modules/storage/tests/storage_access_controller_test.cpp"
    hmi_storage_access_controller)
  target_sources(hmi_storage_access_controller_test PRIVATE
    "${CMAKE_SOURCE_DIR}/platform/windows/storage/virtual_storage_access.cpp")
  hmi_add_sample_suite_test(
    hmi_sample_suite_state_test
    tests/state_test.cpp
    hmi_sample_suite_state)
  hmi_add_sample_suite_test(
    hmi_sample_suite_interaction_test
    tests/interaction_test.cpp
    hmi_sample_suite_interaction)
  hmi_add_sample_suite_test(
    hmi_sample_suite_i18n_catalog_test
    pages/text_input/tests/catalog_test.cpp
    hmi_sample_suite_i18n_catalog)
  hmi_add_sample_suite_test(
    hmi_text_utf8_test
    "${CMAKE_SOURCE_DIR}/modules/text/tests/utf8_test.cpp"
    hmi_text_utf8)
  hmi_add_sample_suite_test(
    hmi_text_editor_test
    "${CMAKE_SOURCE_DIR}/modules/text/tests/editor_test.cpp"
    hmi_text_editor)
  hmi_add_sample_suite_test(
    hmi_text_hangul_test
    "${CMAKE_SOURCE_DIR}/modules/text/tests/hangul_composer_test.cpp"
    hmi_text_hangul)
  hmi_add_sample_suite_test(
    hmi_text_input_model_test
    "${CMAKE_SOURCE_DIR}/modules/text/tests/input_model_test.cpp"
    hmi_text_input_model)
  hmi_add_sample_suite_test(
    hmi_text_keyboard_layout_test
    "${CMAKE_SOURCE_DIR}/modules/text/tests/keyboard_layout_test.cpp"
    hmi_text_keyboard_layout)
  hmi_add_sample_suite_test(
    hmi_ui_asset_update_overlay_test
    "${CMAKE_SOURCE_DIR}/modules/ui/tests/asset_update_overlay_test.cpp"
    hmi_ui_asset_update_overlay)
  hmi_add_sample_suite_test(
    hmi_ui_font_repository_test
    "${CMAKE_SOURCE_DIR}/modules/ui/tests/font_repository_test.cpp"
    hmi_ui_font_repository)
  hmi_add_sample_suite_test(
    hmi_ui_user_text_label_test
    "${CMAKE_SOURCE_DIR}/modules/ui/tests/user_text_label_test.cpp"
    hmi_ui_user_text_label)
  hmi_add_sample_suite_test(
    hmi_ui_user_text_input_line_test
    "${CMAKE_SOURCE_DIR}/modules/ui/tests/user_text_input_line_test.cpp"
    hmi_ui_user_text_input_line)
  target_link_libraries(hmi_ui_user_text_input_line_test PRIVATE
    hmi_windows_pointer_test_driver)
  hmi_add_sample_suite_test(
    hmi_ui_virtual_keyboard_test
    "${CMAKE_SOURCE_DIR}/modules/ui/tests/virtual_keyboard_test.cpp"
    hmi_ui_virtual_keyboard)
  target_link_libraries(hmi_ui_virtual_keyboard_test PRIVATE
    hmi_windows_pointer_test_driver)
  hmi_add_sample_suite_test(
    hmi_sample_suite_font_render_test
    pages/text_input/tests/font_render_test.cpp
    hmi_sample_suite_font_render)
  target_sources(hmi_sample_suite_font_render_test PRIVATE
    generated/fonts/fixed/title_32_bold.c
    generated/fonts/fixed/body_18_regular.c
    generated/fonts/fixed/control_18_bold.c
    generated/fonts/fixed/status_16_regular.c
    generated/fonts/fixed/error_16_bold.c)
  set_tests_properties(hmi_sample_suite_font_render PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
  hmi_add_sample_suite_test(
    hmi_i18n_format_test
    "${CMAKE_SOURCE_DIR}/modules/i18n/tests/format_test.cpp"
    hmi_i18n_format)
  hmi_add_sample_suite_test(
    hmi_settings_level_test
    "${CMAKE_SOURCE_DIR}/modules/settings/tests/level_settings_test.cpp"
    hmi_settings_level)
  hmi_add_sample_suite_test(
    hmi_audio_playback_test
    "${CMAKE_SOURCE_DIR}/modules/audio/tests/playback_test.cpp"
    hmi_audio_playback)
  hmi_add_sample_suite_test(
    hmi_windows_sdl_wav_playback_test
    "${CMAKE_SOURCE_DIR}/platform/windows/audio/tests/sdl_wav_playback_test.cpp"
    hmi_windows_sdl_wav_playback)
  set_tests_properties(hmi_windows_sdl_wav_playback PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
  hmi_add_sample_suite_test(
    hmi_connectivity_wifi_sta_test
    "${CMAKE_SOURCE_DIR}/modules/connectivity/tests/wifi_sta_registration_test.cpp"
    hmi_connectivity_wifi_sta)
  hmi_add_sample_suite_test(
    hmi_connectivity_wifi_ap_runtime_test
    "${CMAKE_SOURCE_DIR}/modules/connectivity/tests/wifi_ap_runtime_test.cpp"
    hmi_connectivity_wifi_ap_runtime)
  hmi_add_sample_suite_test(
    hmi_connectivity_wifi_ap_web_controller_test
    "${CMAKE_SOURCE_DIR}/modules/connectivity/tests/wifi_ap_web_controller_test.cpp"
    hmi_connectivity_wifi_ap_web_controller)
  hmi_add_sample_suite_test(
    hmi_sample_suite_wifi_ap_web_qr_test
    pages/wifi_ap_web/tests/wifi_ap_web_qr_test.cpp
    hmi_sample_suite_wifi_ap_web_qr)
  hmi_add_sample_suite_test(
    hmi_sample_suite_display_audio_state_test
    pages/display_audio/tests/display_audio_state_test.cpp
    hmi_sample_suite_display_audio_state)
  hmi_add_sample_suite_test(
    hmi_sample_suite_display_audio_interaction_test
    pages/display_audio/tests/display_audio_interaction_test.cpp
    hmi_sample_suite_display_audio_interaction)
  set_tests_properties(hmi_sample_suite_display_audio_interaction PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
  hmi_add_sample_suite_test(
    hmi_sample_suite_wav_sounds_state_test
    pages/wav_sounds/tests/wav_sounds_state_test.cpp
    hmi_sample_suite_wav_sounds_state)
  hmi_add_sample_suite_test(
    hmi_sample_suite_wav_sounds_interaction_test
    pages/wav_sounds/tests/wav_sounds_interaction_test.cpp
    hmi_sample_suite_wav_sounds_interaction)
  target_link_libraries(hmi_sample_suite_wav_sounds_interaction_test PRIVATE
    hmi_windows_pointer_test_driver)
  hmi_add_sample_suite_test(
    hmi_sample_suite_wav_sounds_integration_test
    pages/wav_sounds/tests/wav_sounds_integration_test.cpp
    hmi_sample_suite_wav_sounds_integration)
  set_tests_properties(
    hmi_sample_suite_wav_sounds_state
    hmi_sample_suite_wav_sounds_interaction
    hmi_sample_suite_wav_sounds_integration
    PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
  hmi_add_sample_suite_test(
    hmi_sample_suite_wifi_settings_state_test
    pages/wifi/tests/wifi_settings_state_test.cpp
    hmi_sample_suite_wifi_settings_state)
  hmi_add_sample_suite_test(
    hmi_sample_suite_wifi_settings_interaction_test
    pages/wifi/tests/wifi_settings_interaction_test.cpp
    hmi_sample_suite_wifi_settings_interaction)
  hmi_add_sample_suite_test(
    hmi_sample_suite_wifi_ap_web_interaction_test
    pages/wifi_ap_web/tests/wifi_ap_web_interaction_test.cpp
    hmi_sample_suite_wifi_ap_web_interaction)
  set_tests_properties(hmi_sample_suite_wifi_ap_web_interaction PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
  hmi_add_sample_suite_test(
    hmi_sample_suite_sd_storage_state_test
    pages/sd_storage/tests/sd_storage_state_test.cpp
    hmi_sample_suite_sd_storage_state)
  hmi_add_sample_suite_test(
    hmi_sample_suite_sd_storage_interaction_test
    pages/sd_storage/tests/sd_storage_interaction_test.cpp
    hmi_sample_suite_sd_storage_interaction)
  set_tests_properties(
    hmi_sample_suite_wifi_settings_state
    hmi_sample_suite_wifi_settings_interaction
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
  hmi_add_sample_suite_test(
    hmi_sample_suite_text_input_state_test
    pages/text_input/tests/text_input_state_test.cpp
    hmi_sample_suite_text_input_state)
  target_link_libraries(hmi_sample_suite_text_input_state_test PRIVATE
    hmi_windows_pointer_test_driver)
  hmi_add_sample_suite_test(
    hmi_sample_suite_text_input_interaction_test
    pages/text_input/tests/text_input_interaction_test.cpp
    hmi_sample_suite_text_input_interaction)
  target_link_libraries(hmi_sample_suite_text_input_interaction_test PRIVATE
    hmi_windows_pointer_test_driver)
  set_tests_properties(
    hmi_sample_suite_text_input_state
    hmi_sample_suite_text_input_interaction
    PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
  target_link_libraries(hmi_sample_suite_interaction_test PRIVATE
    hmi_windows_pointer_test_driver)
endif()
