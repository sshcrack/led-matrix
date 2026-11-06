# Extra compiler warnings for this project's own targets only.
#
# led_matrix_enable_project_warnings() is called at the end of the root
# CMakeLists.txt. It walks every directory of the build tree and adds the
# warnings to targets that compile sources, skipping anything under a
# thirdparty/ directory (git submodules such as portaudio). vcpkg ports are
# not part of this build tree, so they never see these flags.
#
# Warnings only, no -Werror (yet). Disable with -DLED_MATRIX_WARNINGS=OFF.

option(LED_MATRIX_WARNINGS "Enable extra compiler warnings for project targets" ON)

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
    if(NOT LED_MATRIX_WARNINGS OR MSVC)
        return()
    endif()

    set(_cxx_warnings -Wall -Wextra -Wshadow -Wnon-virtual-dtor -Wnull-dereference)
    set(_c_warnings -Wall -Wextra -Wshadow -Wnull-dereference)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 12)
        list(APPEND _cxx_warnings -Wuse-after-free=3)
        list(APPEND _c_warnings -Wuse-after-free=3)
    endif()

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
    endforeach()
endfunction()
