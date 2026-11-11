cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(jc1060p470c_camera_probe)

# esp_ipa 2.0.0의 empty-config stub은 NULL용 header를 누락한다. RAW10 probe는 IPA를
# 사용하지 않지만 dependency library 자체는 build되므로 compile에만 표준 header를 보탠다.
idf_component_get_property(esp_ipa_lib espressif__esp_ipa COMPONENT_LIB)
target_compile_options(${esp_ipa_lib} PRIVATE -include stddef.h)
