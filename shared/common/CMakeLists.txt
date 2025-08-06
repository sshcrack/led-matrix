cmake_minimum_required(VERSION 3.5.0)
project(SharedToolsCommon)

set(CMAKE_CXX_STANDARD 23)

add_library(${PROJECT_NAME} SHARED
        src/shared/common/plugin_loader/lib_name.cpp
        src/shared/common/utils/utils.cpp
        src/shared/common/udp/packet.cpp
        src/shared/common/marketplace/client.cpp
        src/shared/common/marketplace/client_impl.cpp
)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_path(PICOSHA2_INCLUDE_DIRS "picosha2.h")
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog nlohmann_json::nlohmann_json cpr::cpr)
target_include_directories(${PROJECT_NAME} PRIVATE ${PICOSHA2_INCLUDE_DIRS})
set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(${PROJECT_NAME} PRIVATE SHARED_COMMON_EXPORTS)

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