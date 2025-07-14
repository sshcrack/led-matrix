cmake_minimum_required(VERSION 3.5.0)
project(SharedToolsCommon VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 23)

add_library(${PROJECT_NAME} SHARED)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

find_package(picosha2 CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE picosha2})