# Raspberry Pi Cross-Compilation Toolchain File

# Set the cross-compile directory name (this is a bit hacky but works I know)
set(LED_MATRIX_BUILD_DIR "${CMAKE_CURRENT_LIST_DIR}/../build")
set(CROSS_COMPILE_DIR "${LED_MATRIX_BUILD_DIR}/cross-compile")

if(DEFINED $ENV{CROSS_COMPILE_ROOT})
  set(CROSS_COMPILE_DIR $ENV{CROSS_COMPILE_ROOT})
endif()

# Check if the cross-compile directory exists
if(NOT EXISTS "${CROSS_COMPILE_DIR}")
  if(DEFINED $ENV{CROSS_COMPILE_ROOT})
    message(FATAL_ERROR "CROSS_COMPILE_DIR was defined in env but does not exist.")
  endif()

  message(STATUS "Cross-compile directory not found. Downloading and extracting...")
  file(DOWNLOAD
    "https://github.com/sshcrack/led-matrix/releases/download/v0.0.1-beta/cross-compile.tar.xz"
    "${LED_MATRIX_BUILD_DIR}/cross-compile.tar.xz"
    SHOW_PROGRESS
  )

  file(DOWNLOAD
    "https://github.com/sshcrack/led-matrix/releases/download/v0.0.1-beta/cross-compile.tar.xz.sha512"
    "${LED_MATRIX_BUILD_DIR}/cross-compile.tar.xz.sha512"
    SHOW_PROGRESS
  )

  file(READ "${LED_MATRIX_BUILD_DIR}/cross-compile.tar.xz.sha512" EXPECTED_SHA512)
  string(STRIP "${EXPECTED_SHA512}" EXPECTED_SHA512)

  file(SHA512 "${LED_MATRIX_BUILD_DIR}/cross-compile.tar.xz" ACTUAL_SHA512)

  if(NOT ACTUAL_SHA512 STREQUAL EXPECTED_SHA512)
    file(REMOVE
      "${LED_MATRIX_BUILD_DIR}/cross-compile.tar.xz"
      "${LED_MATRIX_BUILD_DIR}/cross-compile.tar.xz.sha512"
    )
    message(FATAL_ERROR "SHA512 checksum mismatch for cross-compile.tar.xz.\n  Expected: ${EXPECTED_SHA512}\n  Actual:   ${ACTUAL_SHA512}")
  endif()

  file(MAKE_DIRECTORY "${CROSS_COMPILE_DIR}")
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar -xf "${LED_MATRIX_BUILD_DIR}/cross-compile.tar.xz"
    WORKING_DIRECTORY "${CROSS_COMPILE_DIR}"
  )
endif()

# Include the PI toolchain file from the extracted directory
include("${CROSS_COMPILE_DIR}/PI.cmake")
