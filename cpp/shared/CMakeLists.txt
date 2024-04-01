cmake_minimum_required(VERSION 3.5.0)
project(SharedTools VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 23)

# Resources
file(READ resources/default.json RESOURCE_DATA HEX)
configure_file(resources/resources.h.in include/shared/resources.h @ONLY)

file(GLOB_RECURSE SHARED_SOURCES
        src/*.c*
)

add_library(${PROJECT_NAME} SHARED)
target_sources(${PROJECT_NAME}
        PRIVATE
        ${SHARED_SOURCES}
)
# Main Target Compile

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_include_directories(${PROJECT_NAME}
        PRIVATE
        # where the library itself will look for its internal headers
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${RGB_HEADER_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/PicoSHA2
        PUBLIC
        # where top-level project will look for the library's public headers
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        # where external projects will look for the library's public headers
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_directories(${PROJECT_NAME} PRIVATE ${RGB_SRC_FILES})


# spdlog
find_package(spdlog CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)


# JSON
find_package(nlohmann_json 3.2.0 REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)

# Fmt
find_package(fmt CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt)


# HTTP Parsing

find_package(LibXml2 REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE LibXml2::LibXml2)


# ImageMagick
find_package(unofficial-graphicsmagick CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE unofficial::graphicsmagick::graphicsmagick)
