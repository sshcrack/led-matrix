# Include the vcpkg toolchain file
if(DEFINED ENV{VCPKG_ROOT})
  include("$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
else()
  message(FATAL_ERROR "VCPKG_ROOT environment variable is not defined. Please set it to your vcpkg installation directory. For install instructions: https://github.com/Microsoft/vcpkg")
endif()
