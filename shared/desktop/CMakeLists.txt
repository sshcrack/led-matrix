cmake_minimum_required(VERSION 3.5.0)
project(SharedToolsDesktop VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 23)
add_library(${PROJECT_NAME} STATIC src/shared/desktop/plugin_loader/loader.cpp)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_link_libraries(${PROJECT_NAME} PRIVATE SharedToolsCommon)

find_package(spdlog CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)

find_package(nlohmann_json 3.2.0 REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE imgui::imgui)
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