# Raspberry Pi Cross-Compilation Toolchain File

# Check if CROSS_COMPILE_ROOT is defined
if(NOT DEFINED ENV{CROSS_COMPILE_ROOT})
  message(FATAL_ERROR "CROSS_COMPILE_ROOT environment variable is not defined. Please set it to your cross-compiler root directory. A guide can be found here: https://github.com/abhiTronix/raspberry-pi-cross-compilers/discussions/123")
endif()

# Include the PI toolchain file directly from the environment variable path
include("$ENV{CROSS_COMPILE_ROOT}/PI.cmake")
