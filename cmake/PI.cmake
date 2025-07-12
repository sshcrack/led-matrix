# Raspberry Pi Cross-Compilation Toolchain File

# Check for CROSS_COMPILE_ROOT - first environment variable, then CMake variable
set(CROSS_COMPILE_ROOT_PATH "")

if(DEFINED ENV{CROSS_COMPILE_ROOT})
  set(CROSS_COMPILE_ROOT_PATH "$ENV{CROSS_COMPILE_ROOT}")
  message(STATUS "Using CROSS_COMPILE_ROOT from environment variable: ${CROSS_COMPILE_ROOT_PATH}")
elseif(DEFINED CROSS_COMPILE_ROOT)
  set(CROSS_COMPILE_ROOT_PATH "${CROSS_COMPILE_ROOT}")
  message(STATUS "Using CROSS_COMPILE_ROOT from CMake variable: ${CROSS_COMPILE_ROOT_PATH}")
else()
  message(FATAL_ERROR "CROSS_COMPILE_ROOT is not defined. Please set it either as an environment variable or CMake variable to your cross-compiler root directory. A guide can be found here: https://github.com/abhiTronix/raspberry-pi-cross-compilers/discussions/123")
endif()

# Include the PI toolchain file directly from the resolved path
include("${CROSS_COMPILE_ROOT_PATH}/PI.cmake")
