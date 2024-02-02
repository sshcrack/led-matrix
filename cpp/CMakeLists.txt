cmake_minimum_required(VERSION 3.5.0)
project(main VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 23)


# Resources
file(READ ${CMAKE_SOURCE_DIR}/resources/default.json RESOURCE_DATA HEX)
configure_file(resources/resources.h.in ${CMAKE_SOURCE_DIR}/src/resources.h @ONLY)

INCLUDE_DIRECTORIES(
        ${CMAKE_SOURCE_DIR}/rpi-rgb-led-matrix/include
        ${CMAKE_SOURCE_DIR}/rpi-rgb-led-matrix/lib
)

file(GLOB_RECURSE SOURCES
        src/*.h
        src/*.cpp
)

file(GLOB_RECURSE RGB_SRC_FILES ${PROJECT_SOURCE_DIR}/rpi-rgb-led-matrix/lib/*.c*)
file(GLOB_RECURSE RGB_HEADER_FILES ${PROJECT_SOURCE_DIR}/rpi-rgb-led-matrix/include/*.h)

# Add an executable with the above sources
add_executable(main ${SOURCES} ${RGB_SRC_FILES} ${RGB_HEADER_FILES} ${RWLOCK_HEADER_FILES}
        src/shared.cpp
        src/shared.h
        src/Config.cpp
        src/Config.h
        resources/resources.h.in)

# ImageMagick

find_package(unofficial-graphicsmagick CONFIG REQUIRED)
target_link_libraries(main PRIVATE unofficial::graphicsmagick::graphicsmagick)

#include_directories(${graphicsmagick_INCLUDE_DIRS})
#target_link_libraries(main PRIVATE ${graphicsmagick_LIBRARIES})

# Curl
find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIR})
target_link_libraries(main PRIVATE ${CURL_LIBRARIES})

# spdlog
find_package(spdlog CONFIG REQUIRED)
target_link_libraries(main PRIVATE spdlog::spdlog)

# Main Target Compile

target_compile_features(main PRIVATE cxx_std_23)

# HTPT Requests

find_package(cpr CONFIG REQUIRED)
target_link_libraries(main PRIVATE cpr::cpr)

# HTTP Parsing

find_package(LibXml2 REQUIRED)
target_link_libraries(main PRIVATE LibXml2::LibXml2)

# HTTP Server
find_package(restinio REQUIRED)

target_link_libraries(main PRIVATE restinio::restinio)

# JSON
find_package(nlohmann_json 3.2.0 REQUIRED)
target_link_libraries(main PRIVATE nlohmann_json::nlohmann_json)