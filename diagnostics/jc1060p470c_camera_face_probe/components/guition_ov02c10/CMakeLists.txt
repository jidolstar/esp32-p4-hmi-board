set(vendor_commit "c2dfeca968a0dc30fbb50c05a740cc91c1d3c43c")
set(vendor_root
    "https://raw.githubusercontent.com/guitionofficial/P4-series/${vendor_commit}/JC1060P470C_I_W_Y/JC1060P470C_I_W_Y/1-Demo/Demo_IDF/ESP-IDF_5.5.4/JC1060P470C_I_W_Y_New_Panel/esp_brookesia_phone/components/esp_cam_sensor/sensors/ov02c10")
set(vendor_dir "${CMAKE_CURRENT_BINARY_DIR}/vendor_ov02c10")

function(fetch_vendor_file relative_path sha256)
    set(output "${vendor_dir}/${relative_path}")
    get_filename_component(output_dir "${output}" DIRECTORY)
    file(MAKE_DIRECTORY "${output_dir}")
    if(EXISTS "${output}")
        file(SHA256 "${output}" existing_sha256)
        if(NOT existing_sha256 STREQUAL sha256)
            message(STATUS "Discarding checksum-mismatched Guition file: ${relative_path}")
            file(REMOVE "${output}")
        endif()
    endif()
    if(NOT EXISTS "${output}")
        message(STATUS "Fetching pinned Guition OV02C10 file: ${relative_path}")
        file(DOWNLOAD
            "${vendor_root}/${relative_path}"
            "${output}"
            EXPECTED_HASH "SHA256=${sha256}"
            TLS_VERIFY ON
            STATUS download_status)
        list(GET download_status 0 status_code)
        list(GET download_status 1 status_message)
        if(NOT status_code EQUAL 0)
            file(REMOVE "${output}")
            message(FATAL_ERROR
                "Failed to fetch ${relative_path}: ${status_message}")
        endif()
    endif()
endfunction()

fetch_vendor_file(
    "ov02c10.c"
    "42e7514700daeffdb35205f5e2c24fe86f70bfc303a2ae635bed7f87319ab993")
fetch_vendor_file(
    "include/ov02c10.h"
    "8daaff3ac36565426556fc74528d300f3d6542580550a1984e62fbc61862e843")
fetch_vendor_file(
    "include/ov02c10_types.h"
    "d5a4e237f6e0d5e92a01ca211e8c73aa363425b3f5cb1516f3f362a2ac5815f4")
fetch_vendor_file(
    "private_include/ov02c10_mipi_1lane_24Minput_1288x728_raw10_30fps.h"
    "205e442701083fe22b4e1679e921fc36a070d90b629f10645ecb1e2314f17e22")
fetch_vendor_file(
    "private_include/ov02c10_mipi_1lane_24Minput_1920x1080_raw10_30fps.h"
    "e6fb92283a56e0ba10e5df596e22a7adc40901a5587f772e4e222fb120bfb83e")
fetch_vendor_file(
    "private_include/ov02c10_mipi_2lane_24Minput_1920x1080_raw10_30fps.h"
    "565209fcad9bc8a4c842add6171500997f02087ccc99390c6444c34e650b04fd")
fetch_vendor_file(
    "private_include/ov02c10_regs.h"
    "c680cd39547ef00d9f2756f2670d61437b998ac58453466f3b26d270c306dfbb")
fetch_vendor_file(
    "private_include/ov02c10_settings.h"
    "a97c777e316cb0ed449f0fffdfab43c8cb928d7a3d9b2e26a6feb48948d45979")

idf_component_register(
    SRCS "${vendor_dir}/ov02c10.c"
    INCLUDE_DIRS "${vendor_dir}/include"
    PRIV_INCLUDE_DIRS "${vendor_dir}/private_include"
    REQUIRES esp_cam_sensor)

# Sensor detector는 linker section 등록 함수가 dead-strip되지 않아야 한다.
target_link_libraries(${COMPONENT_LIB} INTERFACE "-u ov02c10_detect")
