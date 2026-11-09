cmake_minimum_required(VERSION 3.21)
project(SharedToolsCommon)

set(CMAKE_CXX_STANDARD 23)

add_library(${PROJECT_NAME} SHARED
        src/shared/common/plugin_loader/lib_name.cpp
        src/shared/common/utils/utils.cpp
        src/shared/common/udp/packet.cpp
        src/shared/common/Version.cpp
        src/shared/common/audio_protocol.cpp
        src/shared/common/remote_render_protocol.cpp
        src/shared/common/crash_reporter.cpp
        src/shared/common/build_info.cpp
)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

# Scene-code fingerprint for the remote-render compatibility check. Regenerated
# on every build from source contents; the header only changes with the inputs.
add_custom_target(scene_code_digest ALL
        COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
                -DOUTPUT=${LED_MATRIX_GENERATED_DIR}/scene_code_digest.h
                -P ${CMAKE_SOURCE_DIR}/cmake/SceneCodeDigest.cmake
        BYPRODUCTS ${LED_MATRIX_GENERATED_DIR}/scene_code_digest.h
        COMMENT "Fingerprinting scene code"
        VERBATIM)
add_dependencies(${PROJECT_NAME} scene_code_digest)
target_include_directories(${PROJECT_NAME} PRIVATE ${LED_MATRIX_GENERATED_DIR})

find_package(spdlog CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)
if(WIN32)
    # CrashReporter uses the inbox DbgHelp API for symbolic SEH stack walks and
    # minidumps. No redistributable/runtime dependency is added.
    target_link_libraries(${PROJECT_NAME} PRIVATE dbghelp)
endif()
set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    SHARED_COMMON_EXPORTS
)

target_include_directories(${PROJECT_NAME}
        PRIVATE
        # where the library itself will look for its internal headers
        src
        PUBLIC
        # where top-level project will look for the library's public headers
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        # where external projects will look for the library's public headers
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)