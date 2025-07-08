cmake_minimum_required(VERSION 3.5.0)

set(CMAKE_CXX_STANDARD 23)

file(GLOB PLUGIN_SOURCES
        */*.c*
)

foreach (PLUGIN_SOURCE ${PLUGIN_SOURCES})
    get_filename_component(P_DIR ${PLUGIN_SOURCE} DIRECTORY)
    get_filename_component(P_DIR_NAME ${P_DIR} NAME)

    get_filename_component(LIB_NAME ${PLUGIN_SOURCE} NAME_WE)

    if (NOT P_DIR_NAME STREQUAL LIB_NAME)
        continue()
    endif ()

    if(LIB_NAME STREQUAL "ExampleScenes")
        continue()
    endif()

    message(STATUS "Adding plugin: ${LIB_NAME}")
    set(LIB_NAMES ${LIB_NAMES} ${LIB_NAME})
    
    # Read .build_ignore file if it exists
    set(IGNORE_PATTERNS "")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${LIB_NAME}/.build_ignore")
        file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/${LIB_NAME}/.build_ignore" IGNORE_PATTERNS)
    endif()
    
    # Collect all source and header files
    file(GLOB_RECURSE PLUGIN_SOURCES_ALL ${LIB_NAME}/*.c*)
    file(GLOB_RECURSE PLUGIN_HEADERS_ALL ${LIB_NAME}/*.h*)
    
    # Filter out ignored directories
    if(IGNORE_PATTERNS)
        foreach(IGNORE_PATTERN ${IGNORE_PATTERNS})
            # Remove leading/trailing whitespace
            string(STRIP "${IGNORE_PATTERN}" IGNORE_PATTERN)
            if(NOT "${IGNORE_PATTERN}" STREQUAL "")
                list(FILTER PLUGIN_SOURCES_ALL EXCLUDE REGEX ".*/${IGNORE_PATTERN}/.*")
                list(FILTER PLUGIN_HEADERS_ALL EXCLUDE REGEX ".*/${IGNORE_PATTERN}/.*")
            endif()
        endforeach()
    endif()

    add_library(${LIB_NAME} SHARED ${PLUGIN_SOURCES_ALL} ${PLUGIN_HEADERS_ALL} ${RGB_SRC_FILES})

    target_include_directories(${LIB_NAME} PRIVATE ${RGB_HEADER_DIR})

    # Compiler target
    target_compile_features(${LIB_NAME} PRIVATE cxx_std_23)


    # Default dependencies

    target_link_libraries(${LIB_NAME} PRIVATE rpi-rgb-led-matrix)

    # ImageMagick
    find_package(unofficial-graphicsmagick CONFIG REQUIRED)
    target_link_libraries(${LIB_NAME} PRIVATE unofficial::graphicsmagick::graphicsmagick)


    # spdlog
    find_package(spdlog CONFIG REQUIRED)
    target_link_libraries(${LIB_NAME} PRIVATE spdlog::spdlog)

    # HTTP Server
    find_package(restinio REQUIRED)

    target_link_libraries(${LIB_NAME} PRIVATE restinio::restinio)

    # Require SharedTools
    target_link_libraries(${LIB_NAME} PRIVATE SharedTools)

    # Custom configuration
    include("${P_DIR}/CMakeLists.txt" OPTIONAL)

    # Install plugin
    install(TARGETS ${LIB_NAME}
            LIBRARY DESTINATION "plugins"
    )

endforeach (PLUGIN_SOURCE ${PLUGIN_SOURCES})