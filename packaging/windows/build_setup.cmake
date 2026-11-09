# CPack post-build script (CPACK_POST_BUILD_SCRIPTS): turns the desktop ZIP into
# the per-user setup .exe next to it (same base name, so release asset names
# stay led-matrix-desktop-<version>-win64.{zip,exe}).
#
# CPack's own NSIS generator always asks for administrator rights and installs
# to Program Files, which the app could not update by itself. See setup.nsi.

foreach(_pkg IN LISTS CPACK_PACKAGE_FILES)
    if(NOT _pkg MATCHES "\\.zip$")
        continue()
    endif()

    set(_pf86 "ProgramFiles(x86)")
    find_program(_makensis makensis
        PATHS "$ENV{${_pf86}}/NSIS" "$ENV{ProgramFiles}/NSIS" "C:/Program Files (x86)/NSIS" "C:/Program Files/NSIS")
    if(NOT _makensis)
        message(FATAL_ERROR "makensis not found: install NSIS to build the setup .exe")
    endif()

    get_filename_component(_dir "${_pkg}" DIRECTORY)
    get_filename_component(_stem "${_pkg}" NAME_WLE)
    set(_unpack "${_dir}/_setup_unpack")
    file(REMOVE_RECURSE "${_unpack}")
    file(MAKE_DIRECTORY "${_unpack}")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xf "${_pkg}"
        WORKING_DIRECTORY "${_unpack}" RESULT_VARIABLE _rc)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "Could not unpack ${_pkg}")
    endif()

    file(GLOB _top LIST_DIRECTORIES true "${_unpack}/*")
    list(LENGTH _top _count)
    if(NOT _count EQUAL 1 OR NOT EXISTS "${_top}/bin/led-matrix-desktop.exe")
        message(FATAL_ERROR "Unexpected package layout in ${_pkg}")
    endif()

    # CPACK_PACKAGE_FILES points into _CPack_Packages; the ZIP is copied to
    # CPACK_PACKAGE_DIRECTORY (the build dir) afterwards, so put the .exe there.
    if(CPACK_PACKAGE_DIRECTORY)
        set(_out "${CPACK_PACKAGE_DIRECTORY}/${_stem}.exe")
    else()
        set(_out "${_dir}/${_stem}.exe")
    endif()
    file(TO_NATIVE_PATH "${_top}" _top_native)
    file(TO_NATIVE_PATH "${_out}" _out_native)
    file(TO_NATIVE_PATH "${CPACK_LED_MATRIX_SETUP_ICON}" _icon_native)
    execute_process(
        COMMAND "${_makensis}" /V2
            "/DPKG_DIR=${_top_native}"
            "/DOUT_FILE=${_out_native}"
            "/DVERSION=${CPACK_PACKAGE_VERSION}"
            "/DICON=${_icon_native}"
            "${CPACK_LED_MATRIX_SETUP_NSI}"
        RESULT_VARIABLE _rc)
    file(REMOVE_RECURSE "${_unpack}")
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "makensis failed (${_rc})")
    endif()
    message(STATUS "Per-user setup: ${_out}")
endforeach()
