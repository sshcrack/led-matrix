vcpkg_check_linkage(ONLY_DYNAMIC_LIBRARY)

# Temporary backport of sshcrack/rpi-rgb-led-matrix commit b3c8fe1. Once a
# published fork tag contains that commit, bump VERSION/SHA512 and remove the
# patch entry/file instead of maintaining a second implementation here.
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO sshcrack/rpi-rgb-led-matrix
    REF "v${VERSION}"
    SHA512 fd99b97c02861224ac65e2cfafa6c48fd67f2b7e8c4bf65bba66ac7e9f6bcf8e433f70bc2dfa89ffcd515d4a4ac57841f90db64dcd82bf6ff7fb541525dd256c
    HEAD_REF master
    PATCHES
        windows-headless-backport.patch
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
