cmake_minimum_required(VERSION 3.5.0)
project(SharedTools VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 23)
add_definitions(-DPLUGIN_TEST=1)


file(GLOB_RECURSE SHARED_SOURCES
        src/*.cpp
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
        PUBLIC
        # where top-level project will look for the library's public headers
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        # where external projects will look for the library's public headers
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)


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

