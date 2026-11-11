include_guard(GLOBAL)

if(NOT DEFINED HMI_GENERATED_DIR OR HMI_GENERATED_DIR STREQUAL "")
  set(HMI_GENERATED_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/.generated/windows/main"
    CACHE PATH "Generated Windows profile input directory")
endif()

get_filename_component(HMI_GENERATED_DIR "${HMI_GENERATED_DIR}" ABSOLUTE)
set(HMI_PROFILE_CMAKE_INPUT "${HMI_GENERATED_DIR}/profile.cmake")

if(NOT EXISTS "${HMI_PROFILE_CMAKE_INPUT}")
  message(FATAL_ERROR
    "Generated profile input is missing: ${HMI_PROFILE_CMAKE_INPUT}\n"
    "Run: python tools/project.py prepare --host windows --profile <id>")
endif()

include("${HMI_PROFILE_CMAKE_INPUT}")

foreach(required_variable
    HMI_PROFILE_ID
    HMI_PROFILE_ROOT
    HMI_PROFILE_SOURCES
    HMI_INCLUDE_DIRS
    HMI_PROFILE_HAS_AUDIO
    HMI_PROFILE_HAS_TIME
    HMI_SELECTED_PROFILE_INCLUDE_DIR)
  if(NOT DEFINED ${required_variable})
    message(FATAL_ERROR
      "Generated profile input does not define ${required_variable}")
  endif()
endforeach()

function(hmi_add_selected_profile_app)
  add_library(hmi_selected_app STATIC
    ${HMI_PROFILE_SOURCES}
    ${HMI_MODULE_SOURCES})

  target_compile_features(hmi_selected_app PUBLIC cxx_std_17)
  target_include_directories(hmi_selected_app PUBLIC
    ${HMI_INCLUDE_DIRS}
    "${HMI_SELECTED_PROFILE_INCLUDE_DIR}")
  target_compile_definitions(hmi_selected_app PUBLIC
    HMI_PROFILE_HAS_AUDIO=${HMI_PROFILE_HAS_AUDIO}
    HMI_PROFILE_HAS_TIME=${HMI_PROFILE_HAS_TIME})
  target_link_libraries(hmi_selected_app PUBLIC lvgl::lvgl)

  if(MSVC)
    target_compile_options(hmi_selected_app PRIVATE /W4)
  else()
    target_compile_options(hmi_selected_app PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()
