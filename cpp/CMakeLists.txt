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

add_definitions( -DMAGICKCORE_QUANTUM_DEPTH=16 )
add_definitions( -DMAGICKCORE_HDRI_ENABLE=0 )
find_package(ImageMagick COMPONENTS Magick++)
include_directories(${ImageMagick_INCLUDE_DIRS})
target_link_libraries(main PRIVATE ${ImageMagick_LIBRARIES})

# Main Target Compile

target_compile_features(main PRIVATE cxx_std_23)

# HTPT Requests

find_package(cpr CONFIG REQUIRED)
target_link_libraries(main PRIVATE cpr::cpr)

# HTTP Parsing

find_package(LibXml2 REQUIRED)
target_link_libraries(main PRIVATE LibXml2::LibXml2)