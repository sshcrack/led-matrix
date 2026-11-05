# Build-time sharing between git worktrees (included before project()).
#
# Agents work in many short-lived worktrees of this repo. Two things make a
# fresh worktree build cheap:
#
#  1. vcpkg binary caching (on by default, ~/.cache/vcpkg/archives): a new
#     build dir restores all ports in about a second instead of compiling them.
#     We only need to find vcpkg when VCPKG_ROOT is not exported.
#  2. ccache with base_dir=<worktree> and hash_dir=false: absolute paths below
#     the worktree are rewritten to relative ones before hashing, so object
#     files compiled in one worktree are cache hits in every other worktree
#     (and in the main checkout) as long as the sources match.
#
# Disable with -DLED_MATRIX_CCACHE=OFF.

# --- vcpkg toolchain fallback ------------------------------------------------
# The presets point at $env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake, which
# degrades to "/scripts/buildsystems/vcpkg.cmake" when VCPKG_ROOT is unset.
if(DEFINED CMAKE_TOOLCHAIN_FILE AND NOT EXISTS "${CMAKE_TOOLCHAIN_FILE}"
        AND CMAKE_TOOLCHAIN_FILE MATCHES "scripts/buildsystems/vcpkg.cmake$")
    set(_lm_vcpkg_candidates "$ENV{VCPKG_ROOT}" "$ENV{VCPKG_INSTALLATION_ROOT}")
    # Main checkout of this repo (worktrees share its .git directory).
    execute_process(
        COMMAND git rev-parse --path-format=absolute --git-common-dir
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE _lm_git_common OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(_lm_git_common)
        get_filename_component(_lm_main_checkout "${_lm_git_common}" DIRECTORY)
        get_filename_component(_lm_main_parent "${_lm_main_checkout}" DIRECTORY)
        list(APPEND _lm_vcpkg_candidates "${_lm_main_checkout}/vcpkg" "${_lm_main_parent}/vcpkg")
    endif()
    list(APPEND _lm_vcpkg_candidates "$ENV{HOME}/vcpkg" "$ENV{HOME}/.local/share/vcpkg" "/opt/vcpkg")
    foreach(_lm_root IN LISTS _lm_vcpkg_candidates)
        if(_lm_root AND EXISTS "${_lm_root}/scripts/buildsystems/vcpkg.cmake")
            message(STATUS "VCPKG_ROOT not set; using vcpkg at ${_lm_root}")
            set(CMAKE_TOOLCHAIN_FILE "${_lm_root}/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "vcpkg toolchain" FORCE)
            break()
        endif()
    endforeach()
    if(NOT EXISTS "${CMAKE_TOOLCHAIN_FILE}")
        message(FATAL_ERROR "vcpkg not found. Export VCPKG_ROOT=<path to vcpkg checkout> and re-run cmake --preset.")
    endif()
endif()

# --- ccache shared across worktrees ------------------------------------------
option(LED_MATRIX_CCACHE "Use ccache (shared across git worktrees) when available" ON)
if(LED_MATRIX_CCACHE AND NOT CMAKE_CXX_COMPILER_LAUNCHER AND NOT MSVC AND NOT CMAKE_GENERATOR MATCHES "Visual Studio")
    find_program(LED_MATRIX_CCACHE_PROGRAM ccache)
    if(LED_MATRIX_CCACHE_PROGRAM)
        # ccache >= 4.8 accepts KEY=VALUE overrides before the compiler, so no
        # global ccache.conf edits are needed.
        set(_lm_launcher "${LED_MATRIX_CCACHE_PROGRAM}" "base_dir=${CMAKE_CURRENT_SOURCE_DIR}" "hash_dir=false")
        set(CMAKE_C_COMPILER_LAUNCHER "${_lm_launcher}" CACHE STRING "C compiler launcher")
        set(CMAKE_CXX_COMPILER_LAUNCHER "${_lm_launcher}" CACHE STRING "C++ compiler launcher")
        message(STATUS "ccache enabled (base_dir=${CMAKE_CURRENT_SOURCE_DIR}, shared across worktrees)")
    endif()
endif()
