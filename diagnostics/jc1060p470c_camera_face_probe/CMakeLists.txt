cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Guition 예제가 ESP32-P4 v3 미만에서 선택하는 OV02C10 노출·색상 보정 설정을 사용한다.
set(guition_demo "${CMAKE_CURRENT_LIST_DIR}/../../_doc/JC1060P470C_I_W_Y/1-Demo/Demo_IDF/ESP-IDF_5.5.4/JC1060P470C_I_W_Y_New_Panel/esp_brookesia_phone")
idf_build_set_property(ESP_IPA_JSON_CONFIG_FILE_PATH
    "${guition_demo}/components/esp_cam_sensor/sensors/ov02c10/cfg/ov02c10_default_p4_eco4.json" APPEND)
project(jc1060p470c_camera_face_probe)
