vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO sshcrack/shadertoy-headless
    REF "v${VERSION}"
    SHA512 fca3faa48fd71ab1705718720f2bf50f5571f63de3dbb4796753601c5aab061b94c81ac536bb36a5f38550a5ae0318fc94a9a7d5ba8d5e13617b203253483ed1
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
)

vcpkg_cmake_install()

vcpkg_copy_pdbs()
vcpkg_cmake_config_fixup(PACKAGE_NAME "shadertoy-headless")

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")