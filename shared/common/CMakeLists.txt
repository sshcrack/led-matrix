cmake_minimum_required(VERSION 3.5.0)
project(SharedToolsCommon VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 23)

add_library(${PROJECT_NAME} STATIC
        src/shared/common/plugin_loader/lib_name.cpp
        src/shared/common/utils/utils.cpp
)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

find_package(spdlog CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)
set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)

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