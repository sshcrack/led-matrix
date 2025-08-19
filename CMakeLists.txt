cmake_minimum_required(VERSION 3.5.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS True)
set(VCPKG_APPLOCAL_DEPS ON)
set(RPI_RGB_LED_MATRIX_FONTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shared/fonts)

option(ENABLE_DESKTOP "Build desktop application" OFF)
option(DESKTOP_DEBUG_CONSOLE "If the desktop should have a console window for debugging" OFF)

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

# Option to enable update mechanism testing (bypasses platform restrictions)
option(ENABLE_UPDATE_TESTING "Enable update mechanism testing on any platform" OFF)

if(ENABLE_UPDATE_TESTING)
    message(STATUS "Update testing enabled. This will bypass platform checks and always run the update script.")
endif()

include(cmake/subdirlist.cmake)
include(cmake/vcpkg_features.cmake)

subdirlist(PLUGIN_DIRS ${CMAKE_SOURCE_DIR}/plugins/)
foreach(PLUGIN_NAME ${PLUGIN_DIRS})
    # Convert plugin name to kebab-case
    to_kebab_case(${PLUGIN_NAME} PLUGIN_NAME_KABAB)
    if(ENABLE_DESKTOP)
        set(FEATURE_NAME "${PLUGIN_NAME_KABAB}-desktop")
        check_vcpkg_feature_exists("${FEATURE_NAME}" FEATURE_EXISTS)
        if(FEATURE_EXISTS)
            list(APPEND VCPKG_MANIFEST_FEATURES "${FEATURE_NAME}")
        endif()
    else()
        set(FEATURE_NAME "${PLUGIN_NAME_KABAB}-matrix")
        check_vcpkg_feature_exists("${FEATURE_NAME}" FEATURE_EXISTS)
        if(FEATURE_EXISTS)
            list(APPEND VCPKG_MANIFEST_FEATURES "${FEATURE_NAME}")
        endif()
    endif()

    set(PLUGIN_DIR "${CMAKE_SOURCE_DIR}/plugins/${PLUGIN_NAME}")
    if(EXISTS "${PLUGIN_DIR}/overlay-ports/")
        list(APPEND VCPKG_OVERLAY_PORTS "${PLUGIN_DIR}/overlay-ports/")
    endif()

endforeach()

project(main VERSION 1.11.13)
include(cmake/update_package_version.cmake)

add_package_json_version_update(CONFIGURE_TIME PACKAGE_JSON_PATH "react-native/package.json" TARGET_NAME "update_package_version")
add_package_json_version_update(CONFIGURE_TIME PACKAGE_JSON_PATH "react-native/app.json" TARGET_NAME "update_app_version")


set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")

# Define version macros for compile-time access
add_compile_definitions(
    PROJECT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    PROJECT_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    PROJECT_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    PROJECT_VERSION_STRING="${PROJECT_VERSION}"
)


# Add update testing flag if enabled
if(ENABLE_UPDATE_TESTING)
    add_compile_definitions(ENABLE_UPDATE_TESTING=1)
endif()

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

    if(WIN32)
        list(APPEND SOURCES src_desktop/app_icon.rc)
    endif()
else()
    file(GLOB_RECURSE SOURCES
        src_matrix/*.c*
    )
endif()




# Adding plugins
add_subdirectory(plugins)

# Add an executable with the above sources
add_executable(${PROJECT_NAME}
    ${SOURCES}
)
if(ENABLE_DESKTOP)
    if(NOT WIN32)
        find_package(PkgConfig REQUIRED) # Include functions provided by PkgConfig module.

        pkg_check_modules(DBUS REQUIRED dbus-1) # This calls pkgconfig with appropriate arguments
        # Use results of pkg_check_modules() call.
        include_directories(${DBUS_INCLUDE_DIRS})
        link_directories(${DBUS_LIBRARY_DIRS})

        target_link_libraries(${PROJECT_NAME} PRIVATE ${DBUS_LIBRARIES})
    endif()

    find_package(tray CONFIG REQUIRED)
    find_package(hello-imgui CONFIG REQUIRED)
    find_package(implot CONFIG REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE hello-imgui::hello_imgui tray::tray)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/traypp/include
    )

    install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src_desktop/assets/
        DESTINATION assets
    )

    # Install required DLLs on Windows automatically
    if(WIN32)
        # Install all DLLs from the build output directory
        install(DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/"
            DESTINATION "bin"
            FILES_MATCHING PATTERN "*.dll"
            PATTERN "*.pdb" EXCLUDE
        )
    endif()
endif()

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
    DESTINATION .
)

# Install update script for matrix builds (not for desktop)
if(NOT ENABLE_DESKTOP)
    install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/update_service.sh
        DESTINATION .
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    )
endif()

# Main Target Compile
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

# Add emulator flag if enabled
if(ENABLE_EMULATOR)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_EMULATOR)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE SharedToolsCommon)
if(ENABLE_DESKTOP)
    target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src_desktop)
    target_link_libraries(${PROJECT_NAME} PRIVATE SharedToolsCommon)
    target_link_libraries(${PROJECT_NAME} PRIVATE SharedToolsDesktop)

    if(DESKTOP_DEBUG_CONSOLE)
        add_definitions(-DUSE_DESKTOP_CONSOLE)
    else()
        set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
    endif()
else()
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src_matrix
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE SharedToolsMatrix)
    # HTTP Server
    find_package(restinio REQUIRED)
    find_package(magic_enum CONFIG REQUIRED)

    target_link_libraries(${PROJECT_NAME} PRIVATE restinio::restinio magic_enum::magic_enum)

    # ImageMagick

    find_package(unofficial-graphicsmagick CONFIG REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE unofficial::graphicsmagick::graphicsmagick)

    # HTTP Requests

    find_package(cpr CONFIG REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE cpr::cpr)

    find_package(rpi-rgb-led-matrix CONFIG REQUIRED)

    target_link_libraries(${PROJECT_NAME} PRIVATE rpi_rgb_led_matrix::rpi-rgb-led-matrix)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/rpi-rgb-led-matrix/include
    )

    install(FILES
        "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/lib/librpi-rgb-led-matrix.so"
        DESTINATION "."
    )
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

    # Install React Native dependencies
    add_custom_target(react_native_install ALL
        COMMAND /bin/bash -c ${CMAKE_CURRENT_SOURCE_DIR}/scripts/install_deps.sh
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Installing React Native dependencies"
    )

    # Build web version, depends on install
    add_custom_target(react_native_build ALL
        COMMAND ${NODEJS} node_modules/.bin/expo export --platform web
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/react-native
        COMMENT "Building React Native web version"
        DEPENDS react_native_install
    )

    # Make main target depend on react_native_build
    add_dependencies(${PROJECT_NAME} react_native_build)
endif()

set(INSTALL_DESTINATION ".")
if(ENABLE_DESKTOP)
    set(INSTALL_DESTINATION "bin")
endif()

# Install target
install(TARGETS ${PROJECT_NAME}
    DESTINATION ${INSTALL_DESTINATION}
)

# Install vcpkg package licenses
# Find all copyright files during configure step
file(GLOB_RECURSE COPYRIGHT_FILES
    "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share/*/copyright")

# Only set up install rules for directories that actually contain copyright files
foreach(COPYRIGHT_FILE ${COPYRIGHT_FILES})
    get_filename_component(PACKAGE_DIR "${COPYRIGHT_FILE}" DIRECTORY)
    get_filename_component(PACKAGE_NAME "${PACKAGE_DIR}" NAME)

    install(FILES "${COPYRIGHT_FILE}"
        DESTINATION "licenses/${PACKAGE_NAME}")
endforeach()

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

    install(TARGETS SharedToolsCommon SharedToolsMatrix
        RUNTIME DESTINATION . # .dll (Windows)
        ARCHIVE DESTINATION . # .lib (Windows static or import lib)
        LIBRARY DESTINATION . # .so/.dylib (Unix)
    )


    # Install fonts to fonts directory
    install(FILES
        "${RPI_RGB_LED_MATRIX_FONTS_DIR}/7x13.bdf"
        DESTINATION "."
    )
else()
    install(TARGETS SharedToolsCommon SharedToolsDesktop
        RUNTIME DESTINATION bin # .dll (Windows)
        ARCHIVE DESTINATION bin # .lib (Windows static or import lib)
        LIBRARY DESTINATION bin # .so/.dylib (Unix)
    )

endif()



if(ENABLE_DESKTOP)

    # -------------------
    # CPack configuration
    # -------------------
    #include(InstallRequiredSystemLibraries)
    set(CPACK_PACKAGE_NAME "led-matrix-desktop")
    set(CPACK_PACKAGE_VENDOR "sshcrack")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "LED Matrix Project Desktop Application (used to control led-matrix)")
    set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
    set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
    set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
    set(CPACK_PACKAGE_CONTACT "hendrik@sshcrack.me")

    if(WIN32)
        set(CPACK_GENERATOR "ZIP;NSIS64")
        set(CPACK_NSIS_DISPLAY_NAME "LED Matrix Controller")
        set(CPACK_NSIS_PACKAGE_NAME "LED Matrix Controller")
        set(CPACK_PACKAGE_INSTALL_DIRECTORY "${CPACK_PACKAGE_NAME}")
        set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
        set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/src_desktop/assets/app_settings/icon.ico")
        set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/src_desktop/assets/app_settings/icon.ico")
        set(CPACK_NSIS_INSTALLED_ICON_NAME "bin/main.exe")
        set(CPACK_NSIS_HELP_LINK "https://github.com/sshcrack/led-matrix")
        set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/sshcrack/led-matrix")
        set(CPACK_NSIS_CONTACT "hendrik@sshcrack.me")
        set(CPACK_NSIS_ALLOW_ROOT_INSTALL ON)
        set(CPACK_NSIS_ENABLE_MODERN_UI ON)
        set(CPACK_NSIS_ENABLE_CUSTOM_INSTALL_DIRECTORY ON)

        # Visual C++ Redistributable configuration
        set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
            ; Check if Visual C++ Redistributable is installed
            ReadRegStr $0 HKLM \\\"SOFTWARE\\\\Microsoft\\\\VisualStudio\\\\14.0\\\\VC\\\\Runtimes\\\\x64\\\" \\\"Version\\\"
            StrCmp $0 \\\"\\\" 0 vcredist_installed

            ; If not installed, download and install VC++ Redistributable
            DetailPrint \\\"Downloading Visual C++ Redistributable...\\\"
            NSISdl::download \\\"https://aka.ms/vs/17/release/vc_redist.x64.exe\\\" \\\"$TEMP\\\\vc_redist.x64.exe\\\"
            Pop $R0
            StrCmp $R0 \\\"success\\\" download_ok
            MessageBox MB_OK \\\"Failed to download Visual C++ Redistributable. Please install it manually.\\\"
            Goto vcredist_end

            download_ok:
            DetailPrint \\\"Installing Visual C++ Redistributable...\\\"
            ExecWait \\\"$TEMP\\\\vc_redist.x64.exe /quiet /norestart\\\" $1
            Delete \\\"$TEMP\\\\vc_redist.x64.exe\\\"
            IntCmp $1 0 vcredist_installed
            MessageBox MB_OK \\\"Visual C++ Redistributable installation may have failed. Exit code: $1\\\"

            vcredist_installed:
            DetailPrint \\\"Visual C++ Redistributable is available.\\\"

            vcredist_end:
        ")

        # Remove the old shortcut settings that don't work properly
        # set(CPACK_NSIS_CREATE_DESKTOP_SHORTCUTS ON)
        # set(CPACK_NSIS_CREATE_START_MENU_SHORTCUTS ON)
        # set(CPACK_NSIS_SHORTCUT_NAME "LED Matrix Controller")

        # Properly create shortcuts with working NSIS commands
        set(CPACK_NSIS_CREATE_ICONS_EXTRA "
            CreateShortCut \\\"$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\LED Matrix Controller.lnk\\\" \\\"$INSTDIR\\\\bin\\\\main.exe\\\" \\\"\\\" \\\"$INSTDIR\\\\bin\\\\main.exe\\\" 0
            CreateShortCut \\\"$DESKTOP\\\\LED Matrix Controller.lnk\\\" \\\"$INSTDIR\\\\bin\\\\main.exe\\\" \\\"\\\" \\\"$INSTDIR\\\\bin\\\\main.exe\\\" 0
        ")

        # Remove shortcuts during uninstall
        set(CPACK_NSIS_DELETE_ICONS_EXTRA "
            Delete \\\"$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\LED Matrix Controller.lnk\\\"
            Delete \\\"$DESKTOP\\\\LED Matrix Controller.lnk\\\"
        ")

        # Add option to launch application after installation
        set(CPACK_NSIS_MUI_FINISHPAGE_RUN "$INSTDIR\\\\bin\\\\main.exe")
        set(CPACK_NSIS_MUI_FINISHPAGE_RUN_TEXT "Launch LED Matrix Controller")
        set(CPACK_NSIS_MUI_FINISHPAGE_RUN_CHECKED "1")
    else()
        set(CPACK_GENERATOR "DEB;TGZ")
    endif()
    include(CPack)
else()
    set(CPACK_GENERATOR "TGZ")

    # -------------------
    # CPack configuration
    # -------------------
    include(InstallRequiredSystemLibraries)
    set(CPACK_PACKAGE_NAME "led-matrix")
    set(CPACK_PACKAGE_VENDOR "sshcrack")
    set(CPACK_SYSTEM_NAME "arm64")
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

