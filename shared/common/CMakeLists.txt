cmake_minimum_required(VERSION 3.21)
project(SharedToolsCommon)

set(CMAKE_CXX_STANDARD 23)

add_library(${PROJECT_NAME} SHARED
        src/shared/common/plugin_loader/lib_name.cpp
        src/shared/common/utils/utils.cpp
        src/shared/common/udp/packet.cpp
        src/shared/common/Version.cpp
        src/shared/common/Releases.cpp
        src/shared/common/audio_protocol.cpp
        src/shared/common/remote_render_protocol.cpp
        src/shared/common/frame_stream.cpp
        src/shared/common/crash_reporter.cpp
)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

# Version.cpp reads the configured release/build revision header.
target_include_directories(${PROJECT_NAME} PRIVATE ${LED_MATRIX_GENERATED_DIR})

find_package(spdlog CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)

# Releases.cpp: GitHub release discovery (HTTP) and the one SHA-256 check. nlohmann_json is in its public API.
find_package(cpr CONFIG REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE cpr::cpr)
target_link_libraries(${PROJECT_NAME} PUBLIC nlohmann_json::nlohmann_json)
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