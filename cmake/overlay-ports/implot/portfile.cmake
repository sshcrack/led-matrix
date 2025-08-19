vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO epezent/implot
    REF "3da8bd34299965d3b0ab124df743fe3e076fa222"#"v${VERSION}"
    SHA512 8a95f76ae4a14adf6f3bcd798d2334d8282ff7b50fc7def6308c0556adcc1dbd151c0a36aa676a82e10e1ae80d1ecc1ac54e705180650eed46d0a609611c73c5
    HEAD_REF master
    PATCHES
        add-cmake.diff
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
)

vcpkg_cmake_install()

vcpkg_copy_pdbs()
vcpkg_cmake_config_fixup(PACKAGE_NAME "implot")

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")