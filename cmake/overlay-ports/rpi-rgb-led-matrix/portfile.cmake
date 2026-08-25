vcpkg_check_linkage(ONLY_DYNAMIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO sshcrack/rpi-rgb-led-matrix
    # Pinned to the pushed Windows-headless emulator commit. This is equivalent
    # to the former v0.1.7 + local backport, but keeps the implementation solely
    # in the fork where it belongs.
    REF ce8200517237aa31959440f8c8cad600201f761b
    SHA512 4ddc67502ecfd6ccc1432e3fa66ec265748f9775497963156367c388fa3f4a0347dd79ed6faecdb1cb7bf4a213e352f8da255737c8ee8d5188ef7ee4f4f6e75f
    HEAD_REF master
)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
    emulator ENABLE_EMULATOR
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${FEATURE_OPTIONS}
)



vcpkg_cmake_install()

# CMake installs headers for both configurations; keep one copy. The Debug
# package config must remain until vcpkg_cmake_config_fixup() has merged it.
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_copy_pdbs()
vcpkg_cmake_config_fixup(PACKAGE_NAME "rpi-rgb-led-matrix")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/COPYING")
