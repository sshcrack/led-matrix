vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO sshcrack/shadertoy-headless
    REF "v${VERSION}"
    SHA512 6cde6e3ee61b9e62d4773e9b9050137e35cb7c4a37d1fdaf0b0a434457de745029b49cb6874b7e31dff6ef5aeb694ac6ebf0b037901d7b7c8e9a83b8dd5c7bd1
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(PACKAGE_NAME "shadertoy-headless")

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")