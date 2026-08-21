# Raspberry Pi Cross-Compilation Toolchain File

# Set the cross-compile directory name (this is a bit hacky but works I know)
set(LED_MATRIX_BUILD_DIR "${CMAKE_CURRENT_LIST_DIR}/../build")
set(CROSS_COMPILE_DIR "${LED_MATRIX_BUILD_DIR}/cross-compile")

if(NOT "$ENV{CROSS_COMPILE_ROOT}" STREQUAL "")
  set(CROSS_COMPILE_DIR $ENV{CROSS_COMPILE_ROOT})
endif()

# Check if the cross-compile directory exists
if(NOT EXISTS "${CROSS_COMPILE_DIR}")
if(NOT "$ENV{CROSS_COMPILE_ROOT}" STREQUAL "")
    message(FATAL_ERROR "CROSS_COMPILE_ROOT was defined in env but does not exist.")
  endif()

  message(STATUS "Cross-compile directory not found. Downloading and extracting...")
  file(DOWNLOAD
    "https://github.com/sshcrack/led-matrix/releases/download/v0.0.1-beta/cross-compile.tar.xz"
    "${LED_MATRIX_BUILD_DIR}/cross-compile.tar.xz"
    SHOW_PROGRESS
    INACTIVITY_TIMEOUT 60
  )

  file(MAKE_DIRECTORY "${CROSS_COMPILE_DIR}")
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar -xf "${LED_MATRIX_BUILD_DIR}/cross-compile.tar.xz"
    WORKING_DIRECTORY "${CROSS_COMPILE_DIR}"
  )
endif()

# Include the PI toolchain file from the extracted directory
include("${CROSS_COMPILE_DIR}/PI.cmake")
