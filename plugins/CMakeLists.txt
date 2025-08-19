cmake_minimum_required(VERSION 3.5.0)

set(CMAKE_CXX_STANDARD 23)

# Define a function to register plugins
function(register_plugin PLUGIN_NAME)
    # Parse arguments to separate regular sources from desktop-specific sources
    set(MATRIX_SOURCES "")
    set(DESKTOP_SOURCES "")
    set(IN_DESKTOP_SECTION FALSE)

    string(FIND "${PLUGIN_NAME}" ":" COLON_INDEX)

    if(NOT COLON_INDEX EQUAL -1)
        message(FATAL_ERROR "PLUGIN_NAME must not contain a colon (:), but was: '${PLUGIN_NAME}'")
    endif()

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
    project(${PLUGIN_NAME})
    if(ENABLE_DESKTOP)
        if(DESKTOP_SOURCES STREQUAL "")
            message(STATUS "No Sources for ${PLUGIN_NAME}. Skipping desktop build.")
            return()
        endif()

        add_library(${PLUGIN_NAME} SHARED ${DESKTOP_SOURCES})
    else()
        add_library(${PLUGIN_NAME} SHARED ${MATRIX_SOURCES} ${RGB_SRC_FILES})
    endif()


    target_compile_features(${PLUGIN_NAME} PRIVATE cxx_std_23)
    target_compile_definitions(${PLUGIN_NAME} PRIVATE PLUGIN_NAME="${PLUGIN_NAME}")

    find_package(spdlog CONFIG REQUIRED)
    target_link_libraries(${PLUGIN_NAME} PRIVATE SharedToolsCommon)
    target_link_libraries(${PLUGIN_NAME} PRIVATE spdlog::spdlog)
    if(ENABLE_DESKTOP)
        target_link_libraries(${PLUGIN_NAME} PRIVATE SharedToolsDesktop)
        find_package(nlohmann_json 3.2.0 REQUIRED)
        find_package(hello-imgui CONFIG REQUIRED)

        target_link_libraries(${PROJECT_NAME} PRIVATE hello-imgui::hello_imgui nlohmann_json::nlohmann_json)
    else()
        find_package(restinio REQUIRED)
        find_package(unofficial-graphicsmagick CONFIG REQUIRED)
        find_package(magic_enum CONFIG REQUIRED)

        target_link_libraries(${PLUGIN_NAME} PRIVATE
            SharedToolsMatrix
            restinio::restinio
            unofficial::graphicsmagick::graphicsmagick
            magic_enum::magic_enum
        )

        find_package(rpi-rgb-led-matrix CONFIG REQUIRED)
        target_link_libraries(${PLUGIN_NAME} PRIVATE rpi_rgb_led_matrix::rpi-rgb-led-matrix)
    endif()

    if(${PLUGIN_NAME} STREQUAL "ExampleScenes")
        return()
    endif()

    install(TARGETS ${PLUGIN_NAME}
        RUNTIME DESTINATION plugins/${PLUGIN_NAME} # For DLLs and executables (Windows)
        LIBRARY DESTINATION plugins/${PLUGIN_NAME} # For shared libraries (.so/.dylib)
        ARCHIVE DESTINATION plugins/${PLUGIN_NAME} # For static libraries (.lib/.a)
        COMPONENT ${PLUGIN_NAME}
    )
endfunction()

subdirlist(SUBDIRS ${CMAKE_CURRENT_SOURCE_DIR})
foreach(P_DIR ${SUBDIRS})
    get_filename_component(P_DIR_NAME ${P_DIR} NAME)
    add_subdirectory(${P_DIR_NAME})
endforeach()
