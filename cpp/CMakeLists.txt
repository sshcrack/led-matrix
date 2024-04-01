cmake_minimum_required(VERSION 3.5.0)
project(main VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 23)


file(GLOB_RECURSE SOURCES
        src/*.c*
)


file(GLOB_RECURSE RGB_SRC_FILES ${PROJECT_SOURCE_DIR}/rpi-rgb-led-matrix/lib/*.c*)

set(RGB_SRC_FILES ${RGB_SRC_FILES} CACHE INTERNAL "RGB Source Files")
set(RGB_HEADER_DIR ${PROJECT_SOURCE_DIR}/rpi-rgb-led-matrix/include CACHE INTERNAL "RGB Header Directory")

add_subdirectory(shared)

# Adding plugins
add_subdirectory(plugins)


# Add an executable with the above sources
add_executable(${PROJECT_NAME}
        ${SOURCES}
        ${RGB_SRC_FILES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ${RGB_HEADER_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Main Target Compile

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)


# Require SharedTools
target_link_libraries(${PROJECT_NAME} PRIVATE SharedTools)

# ImageMagick

find_package(unofficial-graphicsmagick CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE unofficial::graphicsmagick::graphicsmagick)

# spdlog
find_package(spdlog CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)

# HTTP Server
find_package(restinio REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE restinio::restinio)

# JSON
find_package(nlohmann_json 3.2.0 REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)


# Link plugins as well
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIB_NAMES} ${CMAKE_DL_LIBS})

# HTTP Requests

find_package(cpr CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE cpr::cpr)
