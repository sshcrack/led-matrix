cmake_minimum_required(VERSION 3.5.0)
project(SharedToolsDesktop VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 23)
add_library(${PROJECT_NAME} STATIC src/noop.cpp)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)