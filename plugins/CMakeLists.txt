cmake_minimum_required(VERSION 3.5.0)

set(CMAKE_CXX_STANDARD 23)

MACRO(SUBDIRLIST result curdir)
    FILE(GLOB children RELATIVE ${curdir} ${curdir}/*)
    SET(dirlist "")
    FOREACH(child ${children})
        IF(IS_DIRECTORY ${curdir}/${child})
            LIST(APPEND dirlist ${child})
        ENDIF()
    ENDFOREACH()
    SET(${result} ${dirlist})
ENDMACRO()

SUBDIRLIST(SUBDIRS ${CMAKE_CURRENT_SOURCE_DIR})

# Define a function to register plugins
function(register_plugin PLUGIN_NAME SOURCES)
    # Parse arguments to separate regular sources from desktop-specific sources
    set(MATRIX_SOURCES "")
    set(DESKTOP_SOURCES "")
    set(IN_DESKTOP_SECTION FALSE)

    foreach(ARG ${ARGN})
        if(ARG STREQUAL "DESKTOP")
            set(IN_DESKTOP_SECTION TRUE)
        else()
            if(IN_DESKTOP_SECTION)
                list(APPEND DESKTOP_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/${ARG})
            else()
                list(APPEND MATRIX_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/${ARG})
            endif()
        endif()
    endforeach()

    # Determine which sources to use based on build target
    message(STATUS "Adding plugin: ${PLUGIN_NAME}")
    if(ENABLE_DESKTOP)
        if(DESKTOP_SOURCES STREQUAL "")
            message(STATUS "No Sources for ${PLUGIN_NAME}. Skipping desktop build.")
            return()
        endif()

        add_library(${PLUGIN_NAME} SHARED ${DESKTOP_SOURCES})
    else()
        add_library(${PLUGIN_NAME} SHARED ${MATRIX_SOURCES})
    endif()


    target_compile_features(${PLUGIN_NAME} PRIVATE cxx_std_23)

    find_package(spdlog CONFIG REQUIRED)
    target_link_libraries(${PLUGIN_NAME} PRIVATE SharedToolsCommon)
    target_link_libraries(${PLUGIN_NAME} PRIVATE spdlog::spdlog)
    if(NOT ENABLE_DESKTOP)
        find_package(restinio REQUIRED)
        find_package(unofficial-graphicsmagick CONFIG REQUIRED)
        target_link_libraries(${PLUGIN_NAME} PRIVATE
            SharedToolsMatrix
            restinio::restinio
            unofficial::graphicsmagick::graphicsmagick
        )
        target_include_directories(${PLUGIN_NAME} PRIVATE ${RGB_HEADER_DIR})
        target_link_libraries(${PLUGIN_NAME} PRIVATE rpi-rgb-led-matrix)
    else()
        target_link_libraries(${PLUGIN_NAME} PRIVATE SharedToolsDesktop)
    endif()

    install(TARGETS ${PLUGIN_NAME}
        LIBRARY DESTINATION "plugins"
    )
endfunction()

# Export the register_plugin function to be available in subdirectories
export(PACKAGE register_plugin)

foreach(P_DIR ${SUBDIRS})
    get_filename_component(P_DIR_NAME ${P_DIR} NAME)
    add_subdirectory(${P_DIR_NAME})

endforeach()
