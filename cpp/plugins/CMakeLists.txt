cmake_minimum_required(VERSION 3.5.0)

set(CMAKE_CXX_STANDARD 23)

file(GLOB PLUGIN_SOURCES
        */*.cpp
)

foreach(PLUGIN_SOURCE ${PLUGIN_SOURCES})
    get_filename_component(LIB_NAME ${PLUGIN_SOURCE} NAME_WE)
    set(LIB_NAMES ${LIB_NAMES} ${LIB_NAME})
    add_library(${LIB_NAME} SHARED ${PLUGIN_SOURCE})


    # Compiler target
    target_compile_features(${LIB_NAME} PRIVATE cxx_std_23)

    # Image Magick
    find_package(unofficial-graphicsmagick CONFIG REQUIRED)
    target_link_libraries(${LIB_NAME} PRIVATE unofficial::graphicsmagick::graphicsmagick)

endforeach(PLUGIN_SOURCE ${PLUGIN_SOURCES})
