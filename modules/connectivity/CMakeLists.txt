add_library(hmi_connectivity_core STATIC
  src/wifi_ap_runtime.cpp
  src/wifi_ap_web_controller.cpp
  src/wifi_ap_web_protocol.cpp
  src/wifi_scan_service.cpp
  src/wifi_sta_registration.cpp
  src/wifi_station_settings.cpp)

target_compile_features(hmi_connectivity_core PUBLIC cxx_std_17)
target_include_directories(hmi_connectivity_core PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include")

if(MSVC)
  target_compile_options(hmi_connectivity_core PRIVATE /W4)
else()
  target_compile_options(hmi_connectivity_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
