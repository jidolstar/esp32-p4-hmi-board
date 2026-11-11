idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES human_face_detect
    EMBED_FILES "human_face.jpg")

# PlatformIO의 ESP-IDF runner는 build_flags를 component compile command에 전달하지 않는다.
# 별도 environment의 sdkconfig 경로로 feature-off 기준 build를 재현한다.
if(SDKCONFIG MATCHES "face-probe-off")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE FACE_PROBE_ENABLED=0)
else()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE FACE_PROBE_ENABLED=1)
endif()
