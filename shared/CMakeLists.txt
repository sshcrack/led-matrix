cmake_minimum_required(VERSION 3.5.0)
project(SharedTools VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 23)

# Resources
file(READ resources/default.json RESOURCE_DATA HEX)
configure_file(resources/resources.h.in include/shared/resources.h @ONLY)

option(USE_EMULATOR "Enable emulator support" OFF)
if(USE_EMULATOR)
    add_definitions(-DUSE_EMULATOR)
endif()

add_library(${PROJECT_NAME} SHARED
        src/shared/config/data.cpp
        src/Scene.cpp
        src/shared/post.cpp
        src/shared/utils/utils.cpp
        src/shared/utils/shared.cpp
        src/shared/utils/image_fetch.cpp
        src/shared/utils/FrameTimer.cpp
        src/shared/utils/canvas_image.cpp
        src/shared/plugin_loader/lib_glob.cpp
        src/shared/plugin_loader/lib_glob.h
        src/shared/plugin_loader/lib_name.cpp
        src/shared/plugin_loader/lib_name.h
        src/shared/plugin_loader/loader.cpp
        src/shared/config/MainConfig.cpp
        src/shared/config/image_providers/general.cpp
        src/shared/interrupt.cpp
        src/shared/server/server_utils.cpp
)

# Main Target Compile

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_include_directories(${PROJECT_NAME}
        PRIVATE
        # where the library itself will look for its internal headers
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/PicoSHA2
        PUBLIC
        # where top-level project will look for the library's public headers
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        # where external projects will look for the library's public headers
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(${PROJECT_NAME} PRIVATE rpi-rgb-led-matrix)


# spdlog
find_package(spdlog CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)


# JSON
find_package(nlohmann_json 3.2.0 REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)

# Fmt
find_package(fmt CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt)

# HTTP Server
find_package(restinio REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE restinio::restinio)


# HTTP Parsing

find_package(LibXml2 REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE LibXml2::LibXml2)

# ImageMagick
find_package(unofficial-graphicsmagick CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE unofficial::graphicsmagick::graphicsmagick)

include(GNUInstallDirs)

# Installation
install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION "."
)
