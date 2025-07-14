cmake_minimum_required(VERSION 3.5.0)

option(ENABLE_DESKTOP "Build desktop application" OFF)

if(ENABLE_DESKTOP)
    list(APPEND VCPKG_MANIFEST_FEATURES "desktop")
else()
    list(APPEND VCPKG_MANIFEST_FEATURES "matrix")
endif()

# Option to enable/disable emulator
option(ENABLE_EMULATOR "Enable LED matrix emulator functionality" OFF)
if(ENABLE_EMULATOR)
    list(APPEND VCPKG_MANIFEST_FEATURES "emulator")
endif()

if(ENABLE_EMULATOR AND ENABLE_DESKTOP)
    message(FATAL_ERROR "Cannot enable both ENABLE_EMULATOR and ENABLE_DESKTOP at the same time. Please choose one.")
endif()

# Option to enable/disable CORS headers
option(ENABLE_CORS "Enable CORS headers for cross-origin requests" OFF)

project(main VERSION 1.1.0)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")

# Set default install prefix to build directory if not specified
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "Installation Directory" FORCE)
endif()


if(SKIP_MS_WAIT)
    add_definitions(-DSKIP_MS_WAIT)
endif()

# Set RPATH settings
set(CMAKE_INSTALL_RPATH "$ORIGIN")
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

set(CMAKE_CXX_STANDARD 23)

add_subdirectory(shared)
if(ENABLE_DESKTOP)
    file(GLOB_RECURSE SOURCES
        src_desktop/*.c*
    )
else()
    file(GLOB_RECURSE SOURCES
        src_matrix/*.c*
    )
    add_subdirectory(rpi-rgb-led-matrix)

    install(TARGETS rpi-rgb-led-matrix
        EXPORT rpi-rgb-led-matrixTargets
        LIBRARY DESTINATION "."
        ARCHIVE DESTINATION "."
        RUNTIME DESTINATION "."
    )
endif()




# Adding plugins
add_subdirectory(plugins)

# Add an executable with the above sources
add_executable(${PROJECT_NAME}
    ${SOURCES}
)
if(ENABLE_DESKTOP)
    find_package(hello-imgui CONFIG REQUIRED)

    target_link_libraries(main PRIVATE hello-imgui::hello_imgui)
    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src_desktop/assets/
        DESTINATION assets
    )
endif()

# Main Target Compile
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

# Add CORS support if enabled
if(ENABLE_CORS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_CORS)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE SharedToolsCommon)
if(ENABLE_DESKTOP)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src_desktop
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE SharedToolsDesktop)
else()
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src_matrix
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE SharedToolsMatrix)
    # HTTP Server
    find_package(restinio REQUIRED)

    target_link_libraries(${PROJECT_NAME} PRIVATE restinio::restinio)

    # ImageMagick

    find_package(unofficial-graphicsmagick CONFIG REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE unofficial::graphicsmagick::graphicsmagick)

    # HTTP Requests

    find_package(cpr CONFIG REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE cpr::cpr)

    target_link_libraries(${PROJECT_NAME} PRIVATE rpi-rgb-led-matrix)
endif()

# spdlog
find_package(spdlog CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)

# JSON
find_package(nlohmann_json 3.2.0 REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)


# Link plugins as well
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIB_NAMES} ${CMAKE_DL_LIBS})



if(NOT ENABLE_EMULATOR AND NOT ENABLE_DESKTOP)
    find_package(WiringPi)
    if(WiringPi_FOUND)
        include_directories(${WiringPi_INCLUDE_DIR})
        target_link_libraries(${PROJECT_NAME} PRIVATE ${WiringPi_LIBRARIES})
    endif()
endif()

#Cross Comp https://forum.grin.mw/t/building-grin-for-raspberry-pi4/7916

if(NOT SKIP_WEB_BUILD AND NOT ENABLE_DESKTOP)
    # Check for Node.js
    find_program(NODEJS node)
    if(NOT NODEJS)
        message(FATAL_ERROR "Node.js not found! Please install Node.js.")
    endif()

    # Check for pnpm
    find_program(PNPM pnpm)
    if(NOT PNPM)
        message(FATAL_ERROR "pnpm not found! Please install pnpm.")
    endif()


    # Install React Native dependencies and build web version
    add_custom_target(react_native_install ALL
        COMMAND ${PNPM} install
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/react-native
        COMMENT "Installing React Native dependencies"
    )

    add_custom_target(react_native_build ALL
        COMMAND ${NODEJS} node_modules/.bin/expo export --platform web
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/react-native
        COMMENT "Building React Native web version"
    )

    # Make main target depend on react_native_build
    add_dependencies(${PROJECT_NAME} react_native_install)
    add_dependencies(${PROJECT_NAME} react_native_build)
endif()

# Install target
install(TARGETS ${PROJECT_NAME}
    DESTINATION "."
)

# Create plugins and fonts directories
install(DIRECTORY DESTINATION "plugins")

if(NOT ENABLE_DESKTOP)
    # Install web files
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/react-native/dist/
        DESTINATION web
    )

    # Add sync to RPI if enabled
    option(SYNC_TO_RPI "Sync to Raspberry Pi after install" OFF)
    if(SYNC_TO_RPI)
        install(CODE "execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/sync.sh)")
    endif()
endif()

if(ENABLE_DESKTOP)

    # -------------------
    # CPack configuration
    # -------------------
    include(InstallRequiredSystemLibraries)
    set(CPACK_PACKAGE_NAME "led-matrix-desktop")
    set(CPACK_PACKAGE_VENDOR "sshcrack")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "LED Matrix Project Desktop Application (used to control led-matrix)")
    set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
    set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
    set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
    set(CPACK_PACKAGE_CONTACT "hendrik@sshcrack.me")
    set(CPACK_GENERATOR "TGZ ZIP DEB")
    include(CPack)
else()

    # -------------------
    # CPack configuration
    # -------------------
    include(InstallRequiredSystemLibraries)
    set(CPACK_PACKAGE_NAME "led-matrix")
    set(CPACK_PACKAGE_VENDOR "sshcrack")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "LED Matrix Project")
    set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
    set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
    set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
    set(CPACK_PACKAGE_CONTACT "hendrik@sshcrack.me")
    set(CPACK_GENERATOR "TGZ")
    include(CPack)
endif()

