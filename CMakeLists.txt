cmake_minimum_required(VERSION 3.21)

project(hmi_starter LANGUAGES C CXX)

include(CTest)
set(HMI_TEST_ENVIRONMENT "SDL_VIDEODRIVER=dummy")
include(cmake/Dependencies.cmake)
include(cmake/ProfileSelection.cmake)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/platform/windows/CMakeLists.txt")
  add_subdirectory(platform/windows)
endif()

hmi_add_selected_profile_app()

if(BUILD_TESTING AND EXISTS "${HMI_PROFILE_ROOT}/CMakeLists.txt")
  add_subdirectory("${HMI_PROFILE_ROOT}"
    "${CMAKE_CURRENT_BINARY_DIR}/selected-profile-tests")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/hosts/windows/CMakeLists.txt")
  add_subdirectory(hosts/windows)
endif()

