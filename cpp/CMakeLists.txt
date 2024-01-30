cmake_minimum_required(VERSION 3.5.0)
project(main VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 23)

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/rpi-rgb-led-matrix/include
  ${CMAKE_SOURCE_DIR}/rpi-rgb-led-matrix/lib
)

file(GLOB SOURCES
    src/*.h
    src/*.cpp
)


file(GLOB_RECURSE RGB_SRC_FILES ${PROJECT_SOURCE_DIR}/rpi-rgb-led-matrix/lib/*.c*)
file(GLOB_RECURSE RGB_HEADER_FILES ${PROJECT_SOURCE_DIR}/rpi-rgb-led-matrix/include/*.h)

# Add an executable with the above sources
add_executable(main ${SOURCES} ${RGB_SRC_FILES} ${RGB_HEADER_FILES})

# ImageMagick

# Find where Magick++-config lives
# On linux, "libgraphicsmagick++1-dev" must be installed
#message(STATUS "Finding GraphicsMagick++")
#find_program(MAGICK_CONFIG "GraphicsMagick++-config")
# Ask about CXX and lib flags/locations
#execute_process(COMMAND "${MAGICK_CONFIG}" "--cxxflags" OUTPUT_VARIABLE MAGICK_CXX_FLAGS)
#execute_process(COMMAND "${MAGICK_CONFIG}" "--libs" OUTPUT_VARIABLE MAGICK_LD_FLAGS)
# Remove trailing whitespace (CMAKE warns about this)
#string(STRIP "${MAGICK_CXX_FLAGS}" MAGICK_CXX_FLAGS)
#string(STRIP "${MAGICK_LD_FLAGS}" MAGICK_LD_FLAGS)

#message(STATUS "Appending CMAKE_CXX_FLAGS ${MAGICK_CXX_FLAGS} ${MAGICK_LD_FLAGS}")
# Append all flags to CMAKE_CXX_FLAGS
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 ${MAGICK_CXX_FLAGS} ${MAGICK_LD_FLAGS}")

#add_definitions( -DMAGICKCORE_QUANTUM_DEPTH=16 )
#add_definitions( -DMAGICKCORE_HDRI_ENABLE=0 )
find_package(unofficial-graphicsmagick CONFIG REQUIRED)

target_link_libraries(main PRIVATE unofficial::graphicsmagick::graphicsmagick)

#include_directories(${graphicsmagick_INCLUDE_DIRS})
#target_link_libraries(main PRIVATE ${graphicsmagick_LIBRARIES})

# Curl
find_package(CURL REQUIRED) 
include_directories(${CURL_INCLUDE_DIR})
target_link_libraries(main PRIVATE ${CURL_LIBRARIES})

# spdlog
find_package(spdlog CONFIG REQUIRED)
target_link_libraries(main PRIVATE spdlog::spdlog)

# Main Target Compile

target_compile_features(main PRIVATE cxx_std_23)

# HTPT Requests

find_package(cpr CONFIG REQUIRED)
target_link_libraries(main PRIVATE cpr::cpr)

# HTTP Parsing

find_package(LibXml2 REQUIRED)
target_link_libraries(main PRIVATE LibXml2::LibXml2)