cmake_minimum_required(VERSION 3.5.0)
project(main VERSION 0.1.0)

# Set default install prefix to build directory if not specified
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "Installation Directory" FORCE)
endif()


if(SKIP_MS_WAIT)
    add_definitions(-DSKIP_MS_WAIT)
endif()

# Add installation paths
include(GNUInstallDirs)

# Set RPATH settings
set(CMAKE_INSTALL_RPATH "$ORIGIN")
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

set(CMAKE_CXX_STANDARD 23)


file(GLOB_RECURSE SOURCES
        src/*.c*
)


add_subdirectory(rpi-rgb-led-matrix)
add_subdirectory(shared)

# Adding plugins
add_subdirectory(plugins)


# Add an executable with the above sources
add_executable(${PROJECT_NAME}
        ${SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Main Target Compile

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)


# Require SharedTools
target_link_libraries(${PROJECT_NAME} PRIVATE SharedTools)
target_link_libraries(${PROJECT_NAME} PRIVATE rpi-rgb-led-matrix)

# ImageMagick

find_package(unofficial-graphicsmagick CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE unofficial::graphicsmagick::graphicsmagick)

# spdlog
find_package(spdlog CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)

# HTTP Server
find_package(restinio REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE restinio::restinio)

# JSON
find_package(nlohmann_json 3.2.0 REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)


# Link plugins as well
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIB_NAMES} ${CMAKE_DL_LIBS})

# HTTP Requests

find_package(cpr CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE cpr::cpr)


#Cross Comp https://forum.grin.mw/t/building-grin-for-raspberry-pi4/7916

# Check for Node.js
find_program(NODEJS node)
if(NOT NODEJS)
    message(FATAL_ERROR "Node.js not found! Please install Node.js.")
endif()

# Check for Yarn
find_program(YARN yarn)
if(NOT YARN)
    message(FATAL_ERROR "Yarn not found! Please install Yarn.")
endif()

# Install React Native dependencies and build web version
add_custom_target(react_native_build ALL
    COMMAND ${YARN} install
    COMMAND ${NODEJS} node_modules/.bin/expo export --platform web
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/react-native
)

# Make main target depend on react_native_build
add_dependencies(${PROJECT_NAME} react_native_build)

# Install target
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION "."
)

# Create plugins and fonts directories
install(DIRECTORY DESTINATION "plugins")

# Install web files
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/react-native/dist/
        DESTINATION web
)

# Add sync to RPI if enabled
option(SYNC_TO_RPI "Sync to Raspberry Pi after install" OFF)
if(SYNC_TO_RPI)
    install(CODE "execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/sync.sh)")
endif()

