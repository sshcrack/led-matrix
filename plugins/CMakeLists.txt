cmake_minimum_required(VERSION 3.21)

set(CMAKE_CXX_STANDARD 23)

function(configure_matrix_plugin_target TARGET_NAME PLUGIN_NAME)
    target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_23)
    target_compile_definitions(${TARGET_NAME} PRIVATE PLUGIN_NAME="${PLUGIN_NAME}")

    find_package(spdlog CONFIG REQUIRED)
    find_package(restinio REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GraphicsMagick REQUIRED IMPORTED_TARGET GraphicsMagick++)
    find_package(magic_enum CONFIG REQUIRED)
    find_package(rpi-rgb-led-matrix CONFIG REQUIRED)

    target_link_libraries(${TARGET_NAME} PRIVATE
        SharedToolsCommon
        SharedToolsMatrix
        spdlog::spdlog
        restinio::restinio
        PkgConfig::GraphicsMagick
        magic_enum::magic_enum
        rpi_rgb_led_matrix::rpi-rgb-led-matrix
    )
endfunction()

function(configure_desktop_plugin_target TARGET_NAME PLUGIN_NAME)
    target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_23)
    target_compile_definitions(${TARGET_NAME} PRIVATE PLUGIN_NAME="${PLUGIN_NAME}")

    find_package(spdlog CONFIG REQUIRED)
    find_package(nlohmann_json 3.2.0 REQUIRED)
    find_package(hello-imgui CONFIG REQUIRED)
    target_link_libraries(${TARGET_NAME} PRIVATE
        SharedToolsCommon
        SharedToolsDesktop
        spdlog::spdlog
        hello-imgui::hello_imgui
        nlohmann_json::nlohmann_json
    )
endfunction()

# A plugin can contain a normal matrix half, an optional desktop half, or both.
# Desktop builds additionally compile the matrix half into a *worker-only* DSO.
# Those DSOs are loaded by led-matrix-scene-worker in a separate process, so a
# new portable scene automatically becomes remotely executable without writing
# a second desktop renderer.
function(register_plugin PLUGIN_NAME)
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
        elseif(IN_DESKTOP_SECTION)
            list(APPEND DESKTOP_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/${ARG})
        else()
            list(APPEND MATRIX_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/${ARG})
        endif()
    endforeach()

    project(${PLUGIN_NAME})

    if(ENABLE_DESKTOP)
        if(NOT DESKTOP_SOURCES STREQUAL "")
            add_library(${PLUGIN_NAME} SHARED ${DESKTOP_SOURCES})
            configure_desktop_plugin_target(${PLUGIN_NAME} ${PLUGIN_NAME})

            if(NOT ${PLUGIN_NAME} STREQUAL "ExampleScenes")
                install(TARGETS ${PLUGIN_NAME}
                    RUNTIME DESTINATION lib/led-matrix-desktop/plugins/${PLUGIN_NAME}
                    LIBRARY DESTINATION lib/led-matrix-desktop/plugins/${PLUGIN_NAME}
                    ARCHIVE DESTINATION lib/led-matrix-desktop/plugins/${PLUGIN_NAME}
                    COMPONENT ${PLUGIN_NAME}
                )
            endif()
        else()
            message(STATUS "No desktop UI/source plugin for ${PLUGIN_NAME}; scene worker copy only.")
        endif()

        if(ENABLE_SCENE_WORKER AND NOT MATRIX_SOURCES STREQUAL "")
            set(WORKER_TARGET "${PLUGIN_NAME}SceneWorker")
            add_library(${WORKER_TARGET} SHARED ${MATRIX_SOURCES} ${RGB_SRC_FILES})
            configure_matrix_plugin_target(${WORKER_TARGET} ${PLUGIN_NAME})
            # PluginLoader derives factory names from the file name, so preserve
            # lib<PluginName>.so / <PluginName>.dll inside a separate directory.
            set_target_properties(${WORKER_TARGET} PROPERTIES
                OUTPUT_NAME "${PLUGIN_NAME}"
                LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/scene_plugins/${PLUGIN_NAME}"
                RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/scene_plugins/${PLUGIN_NAME}"
                ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/scene_plugins/${PLUGIN_NAME}"
                INSTALL_RPATH "$ORIGIN/../.."
            )
            if(NOT WIN32)
                # Build-tree worker DSOs live under scene_plugins/<Plugin>, while
                # SharedToolsCommon/Matrix live in shared/* build directories.
                # Installed DSOs keep the compact $ORIGIN/../.. RPATH above.
                set_target_properties(${WORKER_TARGET} PROPERTIES
                    BUILD_WITH_INSTALL_RPATH FALSE
                    BUILD_RPATH "$<TARGET_FILE_DIR:SharedToolsCommon>;$<TARGET_FILE_DIR:SharedToolsMatrix>"
                )
            endif()
            set_property(GLOBAL APPEND PROPERTY SCENE_WORKER_PLUGIN_TARGETS ${WORKER_TARGET})
            install(TARGETS ${WORKER_TARGET}
                RUNTIME DESTINATION lib/led-matrix-desktop/scene-plugins/${PLUGIN_NAME}
                LIBRARY DESTINATION lib/led-matrix-desktop/scene-plugins/${PLUGIN_NAME}
                ARCHIVE DESTINATION lib/led-matrix-desktop/scene-plugins/${PLUGIN_NAME}
                COMPONENT ${PLUGIN_NAME}
            )
        endif()
        return()
    endif()

    add_library(${PLUGIN_NAME} SHARED ${MATRIX_SOURCES} ${RGB_SRC_FILES})
    configure_matrix_plugin_target(${PLUGIN_NAME} ${PLUGIN_NAME})
    set_property(GLOBAL APPEND PROPERTY MATRIX_PLUGIN_TARGETS ${PLUGIN_NAME})

    if(${PLUGIN_NAME} STREQUAL "ExampleScenes")
        return()
    endif()

    set_target_properties(${PLUGIN_NAME} PROPERTIES
        INSTALL_RPATH "$ORIGIN/../.."
        BUILD_WITH_INSTALL_RPATH TRUE
    )
    install(TARGETS ${PLUGIN_NAME}
        RUNTIME DESTINATION lib/led-matrix/plugins/${PLUGIN_NAME}
        LIBRARY DESTINATION lib/led-matrix/plugins/${PLUGIN_NAME}
        ARCHIVE DESTINATION lib/led-matrix/plugins/${PLUGIN_NAME}
        COMPONENT ${PLUGIN_NAME}
    )
endfunction()

subdirlist(SUBDIRS ${CMAKE_CURRENT_SOURCE_DIR})
foreach(P_DIR ${SUBDIRS})
    get_filename_component(P_DIR_NAME ${P_DIR} NAME)
    add_subdirectory(${P_DIR_NAME})
endforeach()
