# Memory-safety instrumentation and hardening (included right after project()).
#
#   -DLED_MATRIX_SANITIZE=OFF       (default) no sanitizer
#   -DLED_MATRIX_SANITIZE=address   AddressSanitizer + UndefinedBehaviorSanitizer
#                                   (LeakSanitizer is part of ASan on Linux)
#   -DLED_MATRIX_SANITIZE=thread    ThreadSanitizer
#
# ASan and TSan cannot be combined, so the option takes exactly one value.
# The flags are added with add_compile_options()/add_link_options() before any
# add_subdirectory(), so every in-tree target picks them up: the executables,
# SharedTools* libraries, tests and every plugin DSO created by
# register_plugin() (plugins/CMakeLists.txt). vcpkg ports are built separately
# and stay uninstrumented; their noise is handled by scripts/*.supp.
#
# The presets emulator-asan / emulator-tsan / desktop-linux-asan /
# desktop-linux-tsan set this option; see AGENTS.md "Sanitizers and core dumps".

set(LED_MATRIX_SANITIZE "OFF" CACHE STRING "Sanitizer to build with: OFF, address (ASan+UBSan) or thread (TSan)")
set_property(CACHE LED_MATRIX_SANITIZE PROPERTY STRINGS OFF address thread)

# Backwards compatibility with the old boolean (used by older CI configs).
if(ENABLE_SANITIZERS)
    if(LED_MATRIX_SANITIZE STREQUAL "OFF")
        message(DEPRECATION "ENABLE_SANITIZERS=ON is deprecated; use -DLED_MATRIX_SANITIZE=address")
        set(LED_MATRIX_SANITIZE "address" CACHE STRING "" FORCE)
    elseif(NOT LED_MATRIX_SANITIZE STREQUAL "address")
        message(FATAL_ERROR "ENABLE_SANITIZERS=ON conflicts with LED_MATRIX_SANITIZE=${LED_MATRIX_SANITIZE}")
    endif()
endif()

string(TOLOWER "${LED_MATRIX_SANITIZE}" _lm_sanitize)
if(_lm_sanitize STREQUAL "off" OR _lm_sanitize STREQUAL "" OR _lm_sanitize STREQUAL "none")
    set(LED_MATRIX_SANITIZE_MODE "")
elseif(_lm_sanitize STREQUAL "address" OR _lm_sanitize STREQUAL "asan")
    set(LED_MATRIX_SANITIZE_MODE "address")
elseif(_lm_sanitize STREQUAL "thread" OR _lm_sanitize STREQUAL "tsan")
    set(LED_MATRIX_SANITIZE_MODE "thread")
else()
    # Catches "address;thread", "address,thread", typos, ...
    message(FATAL_ERROR
        "LED_MATRIX_SANITIZE='${LED_MATRIX_SANITIZE}' is invalid. Use OFF, address or thread "
        "(ASan and TSan are mutually exclusive).")
endif()

if(LED_MATRIX_SANITIZE_MODE)
    if(MSVC OR NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "LED_MATRIX_SANITIZE requires GCC or Clang (got ${CMAKE_CXX_COMPILER_ID})")
    endif()
    if(CMAKE_CROSSCOMPILING)
        message(WARNING "LED_MATRIX_SANITIZE on a cross build: the target needs the matching sanitizer runtime (libasan/libtsan) installed.")
    endif()

    if(LED_MATRIX_SANITIZE_MODE STREQUAL "address")
        set(_lm_san_flags -fsanitize=address,undefined -fno-sanitize-recover=undefined)
    else()
        set(_lm_san_flags -fsanitize=thread)
    endif()

    add_compile_options(${_lm_san_flags} -fno-omit-frame-pointer)
    add_link_options(${_lm_san_flags})
    # Lets code skip things that defeat the sanitizer (e.g. dlclose() of
    # plugins, which turns their frames into "<unknown module>").
    add_compile_definitions(LED_MATRIX_SANITIZER_BUILD=1)
    if(LED_MATRIX_SANITIZE_MODE STREQUAL "address")
        add_compile_definitions(LED_MATRIX_ASAN=1)
    else()
        add_compile_definitions(LED_MATRIX_TSAN=1)
    endif()
    message(STATUS "Sanitizer build: ${LED_MATRIX_SANITIZE_MODE} (${_lm_san_flags})")
endif()

# Cheap bounds/precondition checks in libstdc++ (operator[] on vector/array/
# string/span, front()/back() on empty containers, ...). They do not change
# the ABI, so this is safe to combine with uninstrumented vcpkg libraries, and
# it is enabled in every build including the Pi release.
if(NOT MSVC)
    add_compile_definitions(_GLIBCXX_ASSERTIONS)
endif()

# GNU build-ids let systemd-coredump / coredumpctl match each module of a core
# dump to its binary (Debian's native GCC adds them by default, the Pi
# cross-compiler does not, so `coredumpctl info` showed "without build-id").
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_link_options("LINKER:--build-id=sha1")
endif()
