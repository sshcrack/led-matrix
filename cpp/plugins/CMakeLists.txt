cmake_minimum_required(VERSION 3.5.0)

set(CMAKE_CXX_STANDARD 23)

file(GLOB PLUGIN_SOURCES
        */*.cpp
)

foreach(PLUGIN_SOURCE ${PLUGIN_SOURCES})
    get_filename_component(P_DIR ${PLUGIN_SOURCE} DIRECTORY)
    get_filename_component(P_DIR_NAME ${P_DIR} NAME)

    get_filename_component(LIB_NAME ${PLUGIN_SOURCE} NAME_WE)

    if(NOT P_DIR_NAME STREQUAL LIB_NAME)
        continue()
    endif ()

    message(STATUS "Adding plugin: ${LIB_NAME}")
    set(LIB_NAMES ${LIB_NAMES} ${LIB_NAME})
    file(GLOB_RECURSE PLUGIN_SOURCES_ALL ${LIB_NAME}/*.cpp)

    add_library(${LIB_NAME} SHARED ${PLUGIN_SOURCES_ALL} ${RGB_SRC_FILES})


    # Compiler target
    target_compile_features(${LIB_NAME} PRIVATE cxx_std_23)


    # Default dependencies

    # ImageMagick
    find_package(unofficial-graphicsmagick CONFIG REQUIRED)
    target_link_libraries(${LIB_NAME} PRIVATE unofficial::graphicsmagick::graphicsmagick)


    # spdlog
    find_package(spdlog CONFIG REQUIRED)
    target_link_libraries(${LIB_NAME} PRIVATE spdlog::spdlog)

    # Require SharedTools
    target_link_libraries(${LIB_NAME} PRIVATE SharedTools)

    # Custom configuration
    include("${P_DIR}/CMakeLists.txt" OPTIONAL)

endforeach(PLUGIN_SOURCE ${PLUGIN_SOURCES})