vcpkg_check_linkage(ONLY_DYNAMIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO sshcrack/rpi-rgb-led-matrix
    # v0.1.9: stream ownership, hardware readback, and emulator lifecycle fixes.
    REF b712197256c8023532f3c97191857c8a652072a2
    SHA512 1719429687e170f36ddefbb6ad918cc9d20725b291eabd70138dbba3f086651811dac845a3d1d6b434feae58b9fd8add5946e0b85989541e2c0ee87c56bb5d76
    HEAD_REF master
    PATCHES fix-win-compat.patch
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
