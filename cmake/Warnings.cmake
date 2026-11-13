# Extra compiler warnings for this project's own targets only.
#
# led_matrix_enable_project_warnings() is called at the end of the root
# CMakeLists.txt. It walks every directory of the build tree and adds the
# warnings to targets that compile sources, skipping anything under a
# thirdparty/ directory (git submodules such as portaudio). vcpkg ports are
# not part of this build tree, so they never see these flags.
#
# Warnings only by default. Disable with -DLED_MATRIX_WARNINGS=OFF.
#
# -DLED_MATRIX_WERROR=ON turns warnings of the project's own targets into
# errors (CI's emulator, desktop-linux and cross-compile builds do this, so a
# new warning fails the PR). It is off by default so a newer local compiler
# never blocks a build. Verified clean with GCC for the emulator and
# desktop-linux presets and the cross-compile toolchain (GCC 12.2 aarch64),
# and with MSVC (/W4 /WX, see _lm_msvc_warnings) for desktop-windows. Two classes stay
# warnings: -Wnull-dereference (GCC reports false positives from inlined asio
# and libstdc++ code instantiated in our translation units) and deprecated
# declarations (third-party API churn).

option(LED_MATRIX_WARNINGS "Enable extra compiler warnings for project targets" ON)
option(LED_MATRIX_WERROR "Treat warnings in the project's own targets as errors" OFF)

function(_lm_collect_targets DIR OUT_VAR)
    get_property(_targets DIRECTORY "${DIR}" PROPERTY BUILDSYSTEM_TARGETS)
    get_property(_subdirs DIRECTORY "${DIR}" PROPERTY SUBDIRECTORIES)
    foreach(_sub IN LISTS _subdirs)
        if(_sub MATCHES "/thirdparty/")
            continue()
        endif()
        _lm_collect_targets("${_sub}" _sub_targets)
        list(APPEND _targets ${_sub_targets})
    endforeach()
    set(${OUT_VAR} ${_targets} PARENT_SCOPE)
endfunction()

function(led_matrix_enable_project_warnings)
    if(NOT LED_MATRIX_WARNINGS)
        return()
    endif()

    if(MSVC)
        _lm_msvc_warnings(_cxx_warnings)
        set(_c_warnings ${_cxx_warnings})
        _lm_apply_warnings("${_cxx_warnings}" "${_c_warnings}")
        return()
    endif()

    set(_cxx_warnings -Wall -Wextra -Wshadow -Wnon-virtual-dtor -Wnull-dereference)
    set(_c_warnings -Wall -Wextra -Wshadow -Wnull-dereference)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 12)
        list(APPEND _cxx_warnings -Wuse-after-free=3)
        list(APPEND _c_warnings -Wuse-after-free=3)
    endif()

    if(LED_MATRIX_WERROR)
        list(APPEND _cxx_warnings -Werror -Wno-error=null-dereference -Wno-error=deprecated-declarations)
        list(APPEND _c_warnings -Werror -Wno-error=null-dereference -Wno-error=deprecated-declarations)
    endif()

    _lm_apply_warnings("${_cxx_warnings}" "${_c_warnings}")
endfunction()

# MSVC counterpart of -Wall -Wextra -Wshadow: /W4, with headers included
# through angle brackets or SYSTEM include dirs (vcpkg) silenced. Disabled:
# - C4244, C4267, C4305 (narrowing conversions): the GCC builds do not enable
#   -Wconversion either.
# - C4251 (std:: members of a DLL-exported class need a dll-interface): every
#   DLL here is built by the same toolchain in one build, so the warning
#   guards against a mismatch that cannot happen.
# _CRT_SECURE_NO_WARNINGS drops the C4996 "consider getenv_s/fopen_s"
# suggestions, so C4996 only reports real [[deprecated]] uses. MSVC cannot
# exempt one warning from /WX, so LED_MATRIX_WERROR switches C4996 off, the
# closest match to GCC's -Wno-error=deprecated-declarations.
function(_lm_msvc_warnings OUT_VAR)
    set(_w /W4 /external:anglebrackets /external:W0 /wd4244 /wd4267 /wd4305 /wd4251)
    if(LED_MATRIX_WERROR)
        list(APPEND _w /WX /wd4996)
    endif()
    set(${OUT_VAR} ${_w} PARENT_SCOPE)
endfunction()

function(_lm_apply_warnings _cxx_warnings _c_warnings)
    _lm_collect_targets("${CMAKE_SOURCE_DIR}" _all_targets)
    foreach(_target IN LISTS _all_targets)
        get_target_property(_type ${_target} TYPE)
        if(NOT _type MATCHES "^(EXECUTABLE|SHARED_LIBRARY|STATIC_LIBRARY|MODULE_LIBRARY|OBJECT_LIBRARY)$")
            continue()
        endif()
        get_target_property(_imported ${_target} IMPORTED)
        if(_imported)
            continue()
        endif()
        target_compile_options(${_target} PRIVATE
            "$<$<COMPILE_LANGUAGE:CXX>:${_cxx_warnings}>"
            "$<$<COMPILE_LANGUAGE:C>:${_c_warnings}>")
        if(MSVC)
            target_compile_definitions(${_target} PRIVATE _CRT_SECURE_NO_WARNINGS)
        endif()
    endforeach()
endfunction()
