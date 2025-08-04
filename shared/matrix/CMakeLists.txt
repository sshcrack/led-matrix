cmake_minimum_required(VERSION 3.5.0)
project(SharedToolsMatrix)

set(CMAKE_CXX_STANDARD 23)


add_library(${PROJECT_NAME} SHARED
        src/shared/matrix/config/data.cpp
        src/shared/matrix/Scene.cpp
        src/shared/matrix/post.cpp
        src/shared/matrix/post_processor.cpp
        src/shared/matrix/utils/utils.cpp
        src/shared/matrix/utils/shared.cpp
        src/shared/matrix/utils/image_fetch.cpp
        src/shared/matrix/utils/FrameTimer.cpp
        src/shared/matrix/utils/canvas_image.cpp
        src/shared/matrix/plugin_loader/loader.cpp
        src/shared/matrix/config/MainConfig.cpp
        src/shared/matrix/config/image_providers/general.cpp
        src/shared/matrix/interrupt.cpp
        src/shared/matrix/server/server_utils.cpp
        src/shared/matrix/server/MimeTypes.cpp
        src/shared/matrix/server/common.cpp
        src/shared/matrix/canvas_consts.cpp
)

# Main Target Compile

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

# Add CORS support if enabled
if(ENABLE_CORS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_CORS)
endif()
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

target_link_libraries(${PROJECT_NAME} PRIVATE rpi-rgb-led-matrix)
target_link_libraries(${PROJECT_NAME} PRIVATE SharedToolsCommon)


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
