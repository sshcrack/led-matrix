vcpkg_check_linkage(ONLY_DYNAMIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO sshcrack/rpi-rgb-led-matrix
    # v0.1.11: headless rendering without SDL video initialization.
    REF 7b7cf6b8bd42fd1c2d317658373a4fbe22e6041a
    SHA512 3dce2257bb921420c43248a10d95503a6be224cf61861277ef973ac6272fbda472b23a9b7ceae652f479160ccb45f96ab9c09652237c7f0632c3f1d7bbd19ff6
    HEAD_REF master
    PATCHES fix-win-compat.patch bulk-framebuffer.patch
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
